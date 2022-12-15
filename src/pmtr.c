#include "pmtr.h"
#include "job.h"
#include "net.h"


pmtr_t cfg = {
  .logger_fd = -1,
};

void usage(char *prog) {
  fprintf(stderr, "usage: %s [options]\n\n", prog);
  fprintf(stderr, " options:\n");
  fprintf(stderr, "   -v           (verbose)\n");
  fprintf(stderr, "   -c <file>    (config file)\n");
  fprintf(stderr, "   -p <file>    (make pidfile)\n");
  fprintf(stderr, "   -I           (echo syslog to stderr)\n");
  fprintf(stderr, "   -F           (stay in foreground)\n");
  fprintf(stderr, "   -t           (just test config file)\n");
  fprintf(stderr, "\n");
  fprintf(stderr, " Default config: %s\n", DEFAULT_PMTR_CONFIG);
  fprintf(stderr, "\n");
  exit(-1);
}

sigjmp_buf jmp;
void sighandler(int signo) {
  siglongjmp(jmp,signo);
}

/* fork a process that signals us if the config or deps change */
pid_t dep_monitor(char *file) {
  int rc, fd, wd, sc;
  pid_t pid;

  pid = fork();

  if (pid == (pid_t)-1) {
    syslog(LOG_ERR, "fork: %s\n", strerror(errno));
    return (pid_t)-1;
  }

  if (pid > 0) return pid;

  /* child here */
  prctl(PR_SET_NAME, "pmtr-dep");
  close_sockets(&cfg);

//  fd = inotify_init();
//  if (fd == -1) {
//    syslog(LOG_ERR, "inotify_init: %s", strerror(errno));
//    sleep(SHORT_DELAY);
//    exit(-1);
//  }

//  wd = inotify_add_watch(fd, file, IN_CLOSE_WRITE);
//  if (wd == -1) {
//    syslog(LOG_ERR, "can't watch %s: %s", file, strerror(errno));
//    sleep(SHORT_DELAY);  /* pmtr.conf unlinked pending rewrite? */
//    exit(-1);            /* parent will restart us to try again */
//  }

  /* loop over jobs' dependencies, adding watch on each one. if one
   * is missing, we log the error but not fatally, because parent
   * detects this situation and disables the job */
  job_t *job=NULL;
  while ( (job=(job_t*)utarray_next(cfg.jobs,job))) {
    if (job->disabled) continue;
    char **dep=NULL;
    while ( (dep=(char**)utarray_next(&job->depv,dep))) {
      sc = inotify_add_watch(fd, fpath(job,*dep), IN_CLOSE_WRITE);
      if (sc == -1) syslog(LOG_ERR,"can't watch %s: %s", *dep, strerror(errno));
    }
  }

  /* request HUP if parent exits, unblock, action terminate */
  signal(SIGHUP, SIG_DFL);
  prctl(PR_SET_PDEATHSIG, SIGHUP);
  sigset_t hup; sigemptyset(&hup); sigaddset(&hup,SIGHUP);
  sigprocmask(SIG_UNBLOCK,&hup,NULL);

  /* block for any inotify event. when one happens, notify parent */
  rc = read(fd, &cfg.eb, sizeof(cfg.eb));
  nanosleep(&halfsec,NULL);
  kill(getppid(), SIGHUP);
  exit(0);
}

/* set up the logger socket here, in the parent, so parent can
 * pass its dynamically generated name along to jobs we will run */
int setup_logger(void) {
  int rc = -1, fd = -1, sc;

  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    syslog(LOG_ERR, "socket: %s", strerror(errno)); 
    goto done;
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;

  /* with autobind, kernel chooses a unique socket name */
  socklen_t want_autobind = sizeof(sa_family_t);
  sc = bind(fd, (struct sockaddr*)&addr, want_autobind);
  if (sc < 0) {
    syslog(LOG_ERR, "bind: %s", strerror(errno)); 
    goto done;
  }

  /* get name that autobind assigned to socket */ 
  struct sockaddr_un tmp;
  memset(&tmp, 0, sizeof(tmp));
  socklen_t addrlen;
  addrlen = sizeof(struct sockaddr_un);
  sc = getsockname(fd, (struct sockaddr *)&tmp, &addrlen);
  if (sc < 0) {
    syslog(LOG_ERR,"getsockname: %s\n", strerror(errno));
    goto done;
  }
  /* addrlen includes 2 byte sa_family_t preceding name */
  cfg.logger_namelen = addrlen - sizeof(sa_family_t); 
  memcpy(cfg.logger_socket, tmp.sun_path, cfg.logger_namelen);

  sc = listen(fd, 5);
  if (sc == -1) {
    syslog(LOG_ERR,"listen: %s\n", strerror(errno));
    goto done;
  }

  cfg.logger_fd = fd;
  rc = 0;

 done:
  if (rc < 0) {
    if (fd != -1) close(fd);
  }
  return rc;
}


/* The Linux-specific, read-only SO_PEERCRED socket option returns
 * credential information about the peer, as described in socket(7).
 *
 * name is returned in volatile memory. caller must copy/use immediately
 */
static char exe[100];
pid_t id_peer(int fd, char **name) {
  struct ucred ucred;
  int rc = -1, sc, i;
  socklen_t len;

  *exe = '\0';
  *name = exe;

  len = sizeof(struct ucred);
  sc = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &ucred, &len);
  if (sc < 0) {
    syslog(LOG_ERR, "getsockopt: %s\n", strerror(errno));
    return (pid_t)0;
  }

  /* try to lookup the executable name from /proc/<pid>/exe */
  char path[30];
  snprintf(path, sizeof(path), "/proc/%u/exe", (unsigned)ucred.pid);
  sc = readlink(path, exe, sizeof(exe)-1);
  if (sc < 0) {
    /* allow failure here, /proc filesystem not present? */
    //syslog(LOG_ERR, "readlink: %s\n", strerror(errno));
  } else {
    /* readlink does not null-terminate its output. do so */
    assert(sc > 0);
    exe[sc] = '\0';
    /* lastly, take basename() of the target */
    for(i = sc-1; i >= 0; i--) {
      if (exe[i] == '/') {
        *name = &exe[i+1];
        break;
      }
    }

  }
  return ucred.pid;
}

pid_t start_logger(void) {
  int epoll_fd, fd, rc = -1, sc;
  struct epoll_event ev;
  char buf[1000];
  ssize_t nr;
  pid_t pid;

  pid = fork();

  if (pid == (pid_t)-1) {
    syslog(LOG_ERR, "fork: %s", strerror(errno)); 
    return (pid_t)-1;
  }

  /* parent closes logger socket; it's the child's */
  if (pid > 0) {
    assert(cfg.logger_fd != -1);
    close(cfg.logger_fd);
    cfg.logger_fd = -1;
    return pid;
  }

  /* child here */
  prctl(PR_SET_NAME, "pmtr-log");
  close_sockets(&cfg);

  /* request HUP if parent exits, unblock, action terminate */
  signal(SIGHUP, SIG_DFL);
  prctl(PR_SET_PDEATHSIG, SIGHUP);
  sigset_t hup; sigemptyset(&hup); sigaddset(&hup,SIGHUP);
  sigprocmask(SIG_UNBLOCK,&hup,NULL);

  /* set up our epoll instance */
  epoll_fd = epoll_create(1); 
  if (epoll_fd == -1) {
    syslog(LOG_ERR,"epoll: %s\n", strerror(errno));
    goto fatal;
  }

  /* add the listening logger socket to epoll */
  memset(&ev,0,sizeof(ev)); /* placate valgrind */
  ev.events = EPOLLIN;
  ev.data.fd= cfg.logger_fd;
  sc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cfg.logger_fd, &ev);
  if (sc < 0) {
    syslog(LOG_ERR,"epoll_ctl: %s\n", strerror(errno));
    goto fatal;
  }

  /* child loop is epoll on listener and connected sockets */
  while (epoll_wait(epoll_fd, &ev, 1, -1) > 0) {

    if (ev.data.fd == cfg.logger_fd) {
      
      /* new client connect */
      fd = accept(cfg.logger_fd, NULL, NULL);
      if (fd < 0) {
        syslog(LOG_ERR,"accept: %s\n", strerror(errno));
        goto fatal;
      }

      /* poll on client connection */
      ev.events = EPOLLIN;
      ev.data.fd= fd;
      sc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
      if (sc < 0) {
        syslog(LOG_ERR,"epoll_ctl: %s\n", strerror(errno));
        goto fatal;
      }

    } else { 
      /* handle input from connected client */
      nr = read(ev.data.fd, buf, sizeof(buf));
      if (nr < 0) {
        syslog(LOG_ERR, "read: %s\n", strerror(errno));
        goto fatal;
      } else if (nr == 0) { /* normal client close */
        close(ev.data.fd);
      } else {
        /* produce syslog from peer output */

        char *exe;
        pid_t pid = id_peer(ev.data.fd, &exe); /* peer identity */

        char *l, *eol;
        l = buf;
        do {
          while ((*l == '\n') && (l < buf+nr)) l++;
          eol = l+1;
          while((eol < buf+nr) && (*eol != '\n')) eol++;
          if (l < buf+nr) {
            syslog(LOG_DAEMON|LOG_INFO, "%s[%d]: %.*s", exe, (int)pid, 
                    (int)(eol-l), l);
          }
          l = eol+1;
        } while(l < buf+nr);

      }
    }
  }

  /* here on fatal failure */
 fatal:
  syslog(LOG_ERR, "pmtr-log: error, terminating");
  exit(-1);
}

void rescan_config(void) {
  job_t *job, *old;
  int c;

  syslog(LOG_INFO,"rescanning job configuration");
  UT_string *em; utstring_new(em);
  UT_array *previous_jobs = cfg.jobs;
  UT_array *new_jobs; utarray_new(new_jobs, &job_mm); cfg.jobs = new_jobs;

  /* udp sockets get re-opened during config parsing */
  close_sockets(&cfg); 

  if (parse_jobs(&cfg, em) == -1) {
    syslog(LOG_CRIT,"FAILED to parse %s", cfg.file);
    syslog(LOG_CRIT,"ERROR: %s", utstring_body(em));
    syslog(LOG_CRIT,"NOTE: using PREVIOUS job config");
    cfg.jobs = previous_jobs;
    goto done;
  }

  /* parse succeeded. diff the new jobs vs. existing jobs */
  job=NULL;
  while( (job = (job_t*)utarray_next(new_jobs,job))) {
    old = get_job_by_name(previous_jobs, job->name);
    if (!old) continue;          // new job definition; startup forthcoming.
    c = job_cmp(job,old);        
    if (c == 0) {                // new job with same name and identical to old:
      job_fin(job);              // free up new job,
      job_cpy(job,old);          // and copy old job into new to retain pid etc.
    } else {                     // new job with same name, but new config:
      job->start_ts = old->start_ts;
      job->pid = old->pid;
      if (job->pid) job->terminate=1;// induce reset to pick up new settings.
    }
    utarray_erase(previous_jobs, utarray_eltidx(previous_jobs,old), 1);
  }
  /* any jobs left in previous_jobs are no longer in the new configuration */
  old=NULL;
  while ( (old=(job_t*)utarray_next(previous_jobs,old))) {
    if (old->pid == 0) continue; /* not running. free it below. */
    /* terminate old job, but keep a record of it til it exits */
    old->terminate=1;
    old->respawn=0;
    old->delete_when_collected=1;
    utstring_clear(em); utstring_printf(em, "%s(deleted)", old->name);
    free(old->name); old->name = strdup(utstring_body(em));
    utarray_push_back(cfg.jobs, old); 
  }
  utarray_free(previous_jobs);

 done:
  utstring_free(em);
}


int make_pidfile() {
  size_t pid_strlen;
  char pid_str[20];
  int fd,rc=-1;
  pid_t pid;

  if (!cfg.pidfile) return 0;

  fd = open(cfg.pidfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
  if (fd == -1) {
   syslog(LOG_ERR,"can't open %s: %s\n",cfg.pidfile, strerror(errno));
   goto done;
  }

  pid = getpid();
  snprintf(pid_str,sizeof(pid_str),"%u\n",(unsigned)pid);
  pid_strlen = strlen(pid_str);
  if (write(fd,pid_str,pid_strlen) != pid_strlen) {
   syslog(LOG_ERR,"can't write to %s: %s\n",cfg.pidfile, strerror(errno));
   close(fd);
   unlink(cfg.pidfile);
   goto done;
  }

  close(fd);
  rc = 0;

 done:
  return rc;
}

int main (int argc, char *argv[]) {
  int n, opt, log_opt;
  job_t *job;

  UT_string *em, *sm;
  utstring_new(em);
  utstring_new(sm);

  while ( (opt = getopt(argc, argv, "v+p:Fc:s:tIh")) != -1) {
    switch (opt) {
      case 'v': cfg.verbose++; break;
      case 'p': cfg.pidfile=strdup(optarg); break;
      case 'F': cfg.foreground=1; break;
      case 'I': cfg.echo_syslog_to_stderr=1; break;
      case 'c': cfg.file=strdup(optarg); break;
      case 't': cfg.test_only=1; cfg.foreground=1; break;
      case 'h': default: usage(argv[0]); break;
    }
  }

  /* always on, nowadays */
  cfg.echo_syslog_to_stderr = 1;

  /* as a container main process, stay in foreground */
  if (getpid() == 1) cfg.foreground = 1;

  log_opt = LOG_PID;
  if (cfg.echo_syslog_to_stderr || isatty(STDERR_FILENO)) log_opt |= LOG_PERROR;
  openlog("pmtr", log_opt, LOG_LOCAL0);
  if (!cfg.file) cfg.file = strdup(DEFAULT_PMTR_CONFIG);

  if (!cfg.foreground) {       /* daemon init */
    if (fork() != 0) exit(0);  /* parent exit */
    setsid();                  /* new session - no controlling terminal */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  }

  /* block all signals. we remain fully blocked except in sigsuspend */
  sigset_t all;
  sigfillset(&all);
  sigprocmask(SIG_SETMASK,&all,NULL);

  utarray_new(cfg.jobs, &job_mm);
  utarray_new(cfg.listen, &ut_int_icd);
  utarray_new(cfg.report, &ut_int_icd);
  utstring_new(cfg.s);

  if (make_pidfile()) goto final;
  umask(0);

  /* parse config file. we blocked signals above because we can get SIGIO 
   * during parsing if we open UDP listeners and get any datagrams */
  if (instantiate_cfg_file(&cfg) == -1) goto final;
  if (parse_jobs(&cfg, em) == -1) {
    syslog(LOG_ERR,"parse failed: %s", utstring_body(em));
    goto final;
  }

  if (cfg.test_only) goto final;
  syslog(LOG_INFO,"pmtr: starting");

  /* define a smaller set of signals to block within sigsuspend. */
  sigset_t ss;
  sigfillset(&ss);
  for(n=0; n < sizeof(sigs)/sizeof(*sigs); n++) sigdelset(&ss, sigs[n]);

  /* establish handlers for signals that we'll unblock in sigsuspend */
  struct sigaction sa;
  sa.sa_handler = sighandler;
  sa.sa_flags = 0;
  sigfillset(&sa.sa_mask);
  for(n=0; n < sizeof(sigs)/sizeof(*sigs); n++) sigaction(sigs[n], &sa, NULL);

  /* here is a special line. we'll come back here whenever a signal happens */
  int signo = sigsetjmp(jmp,1);

  switch(signo) {
    case 0:   /* not a signal yet, first time setup */
      if (setup_logger() < 0) goto final;
      cfg.logger_pid = start_logger();
      if (cfg.logger_pid == (pid_t)-1) goto final;
      do_jobs(&cfg);
      cfg.dm_pid = dep_monitor(cfg.file);
      if (cfg.dm_pid == (pid_t)-1) goto final;
      report_status(&cfg);
      alarm_within(&cfg,SHORT_DELAY);
      break;
    case SIGHUP:
      rescan_config();
      do_jobs(&cfg);
      break;
    case SIGCHLD:
      collect_jobs(&cfg,sm);
      do_jobs(&cfg);
      break;
    case SIGALRM:
      do_jobs(&cfg);
      report_status(&cfg);
      alarm_within(&cfg,SHORT_DELAY);
      break;
    case SIGIO:  /* our UDP listener (if enabled) got a datagram */
      service_socket(&cfg);
      do_jobs(&cfg);
      break;
    default:
      syslog(LOG_INFO,"pmtr: exiting on signal %d", signo);
      goto done;
      break;
  }

  /* wait for signals */
  sigsuspend(&ss);

  /* the only way to get past this point 
   * is from the "goto done" above, because 
   * sigsuspend waits for signals, and when
   * one arrives we longjmp back to sigsetjmp! */

 done:
  term_jobs(cfg.jobs);      /* just sets termination flag, so */
  do_jobs(&cfg);            /* run this loop to issue signals */
  nanosleep(&halfsec,NULL); /* a little time to let them exit */
  collect_jobs(&cfg,sm);    /* collect any jobs that exited */

 final:
  close_sockets(&cfg);
  free(cfg.file);
  utarray_free(cfg.jobs);
  utarray_free(cfg.listen);
  utarray_free(cfg.report);
  utstring_free(cfg.s);
  utstring_free(em);
  utstring_free(sm);
  if (cfg.pidfile) {unlink(cfg.pidfile); free(cfg.pidfile);}
  if (cfg.logger_fd != -1) close(cfg.logger_fd);
  return 0;
}
