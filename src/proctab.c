#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/inotify.h>
#include <assert.h>
#include <time.h>
#include "utstring.h"
#include "utarray.h"
#include "proctab.h"
#include "job.h"
#include "net.h"


static struct timespec half = {.tv_sec = 0, .tv_nsec=500000000}; /* half-sec */
sigjmp_buf jmp;

proctab_t cfg = {
  .verbose = 0,
  .foreground = 0,
};

void usage(char *prog) {
  fprintf(stderr, "usage: %s [options]\n\n", prog);
  fprintf(stderr, " options:\n");
  fprintf(stderr, "   -v           (verbose)\n");
  fprintf(stderr, "   -c <file>    (config file)\n");
  fprintf(stderr, "   -p <file>    (make pidfile)\n");
  fprintf(stderr, "   -F           (stay in foreground)\n");
  fprintf(stderr, "   -t           (just test config file)\n");
  fprintf(stderr, "\n");
  fprintf(stderr, " Default config: %s\n", DEFAULT_PROCTAB_CONFIG);
  fprintf(stderr, "\n");
  exit(-1);
}

void sighandler(int signo) {
  siglongjmp(jmp,signo);
}

/* fork a process that signals us if the config or deps change */
pid_t dep_monitor(char *file) {
  size_t eblen = sizeof(struct inotify_event)+PATH_MAX;
  char *eb;
  int rc,fd,wd, events = IN_CLOSE_WRITE;
  pid_t dm_pid = fork();
  if (dm_pid > 0) return dm_pid;
  if ((dm_pid == (pid_t)-1)                              ||
      ( (fd=inotify_init()) == -1)                       ||
      ( (wd=inotify_add_watch(fd,file, events )) == -1)) {
    syslog(LOG_ERR,"can't watch %s: %s", file, strerror(errno)); 
    sleep(SHORT_DELAY); // proctab.conf deleted, let proctab respawn us 
    exit(-1);
  }

  /* add all jobs' deps to inotify watch */
  job_t *job=NULL;
  while ( (job=(job_t*)utarray_next(cfg.jobs,job))) {
    if (job->disabled) continue;
    char **dep=NULL;
    while ( (dep=(char**)utarray_next(&job->depv,dep))) {
      if (inotify_add_watch(fd, fpath(job,*dep), IN_CLOSE_WRITE) == -1) {
        // proceed despite error, proctab disables job if dep missing
        syslog(LOG_ERR,"can't watch %s: %s", *dep, strerror(errno));
      }
    }
  }

  close_sockets(&cfg);

  /* request HUP if parent exits, unblock, action terminate */
  signal(SIGHUP, SIG_DFL);
  prctl(PR_SET_PDEATHSIG, SIGHUP);
  sigset_t hup; sigemptyset(&hup); sigaddset(&hup,SIGHUP);
  sigprocmask(SIG_UNBLOCK,&hup,NULL);

  eb = malloc(eblen);
  rc = read(fd,eb,eblen);/* block for file update */
  nanosleep(&half,NULL); /* a little time to let files settle */
  kill(getppid(), SIGHUP);
  free(eb);
  exit(0);
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
   fprintf(stderr,"can't open %s: %s\n",cfg.pidfile, strerror(errno));
   goto done;
  }

  pid = getpid();
  snprintf(pid_str,sizeof(pid_str),"%u\n",(unsigned)pid);
  pid_strlen = strlen(pid_str);
  if (write(fd,pid_str,pid_strlen) != pid_strlen) {
   fprintf(stderr,"can't write to %s: %s\n",cfg.pidfile, strerror(errno));
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
  int n, opt;
  job_t *job;

  UT_string *em, *sm;
  utstring_new(em);
  utstring_new(sm);

  while ( (opt = getopt(argc, argv, "v+p:Fc:s:th")) != -1) {
    switch (opt) {
      case 'v': cfg.verbose++; break;
      case 'p': cfg.pidfile=strdup(optarg); break;
      case 'F': cfg.foreground=1; break;
      case 'c': cfg.file=strdup(optarg); break;
      case 't': cfg.test_only=1; cfg.foreground=1; break;
      case 'h': default: usage(argv[0]); break;
    }
  }
  openlog("proctab", LOG_PID | (cfg.foreground ? LOG_PERROR : 0), LOG_LOCAL0);
  if (!cfg.file) cfg.file = strdup(DEFAULT_PROCTAB_CONFIG);

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
  if (parse_jobs(&cfg, em) == -1) {
    syslog(LOG_ERR,"parse failed: %s", utstring_body(em));
    goto final;
  }

  if (cfg.test_only) goto final;
  syslog(LOG_INFO,"proctab: starting");

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
      do_jobs(&cfg);
      cfg.dm_pid = dep_monitor(cfg.file);
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
      syslog(LOG_INFO,"proctab: exiting on signal %d", signo);
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
  term_jobs(cfg.jobs);   /* just sets termination flag, so */
  do_jobs(&cfg);         /* run this loop to issue signals */
  nanosleep(&half,NULL); /* a little time to let them exit */
  collect_jobs(&cfg,sm); /* collect any jobs that exited */

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
  return 0;
}
