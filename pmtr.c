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
#include <sys/prctl.h>
#include <sys/inotify.h>
#include <assert.h>
#include <time.h>
#include "utstring.h"
#include "utarray.h"
#include "pmtr.h"
#include "job.h"
#include "net.h"

#define DEFAULT_PM_CONFIG "/etc/pmtr.conf" 
#define SHORT_DELAY 10

sigjmp_buf jmp;

pmtr_t cfg = {
  .verbose = 0,
  .foreground = 0,
};

void usage(char *prog) {
  fprintf(stderr, "usage: %s [-v] [-F] [-c cfg] [-t]\n", prog);
  fprintf(stderr, "  -v verbose\n");
  fprintf(stderr, "  -F foreground\n");
  fprintf(stderr, "  -c <cfg> specify configuration file\n");
  fprintf(stderr, "  -t test only; parse config file only (implies -F)\n");
  exit(-1);
}

void sighandler(int signo) {
  siglongjmp(jmp,signo);
}

/* fork a process that signals us if the config file changes */
pid_t dep_monitor(char *file) {
  size_t eblen = sizeof(struct inotify_event)+PATH_MAX;
  char *eb;
  int rc,fd,wd;
  pid_t dm_pid = fork();
  if (dm_pid > 0) return dm_pid;
  if ((dm_pid == (pid_t)-1)                              ||
      ( (fd=inotify_init()) == -1)                       ||
      ( (wd=inotify_add_watch(fd,file,IN_MODIFY)) == -1)) {
    syslog(LOG_ERR,"error: %s\n", strerror(errno)); 
    exit(-1);
  }

  /* request HUP if parent exits, unblock, action terminate */
  signal(SIGHUP, SIG_DFL);
  prctl(PR_SET_PDEATHSIG, SIGHUP);
  sigset_t hup; sigemptyset(&hup); sigaddset(&hup,SIGHUP);
  sigprocmask(SIG_UNBLOCK,&hup,NULL);

  eb = malloc(eblen);
  rc = read(fd,eb,eblen);
  kill(getppid(), SIGHUP);
  free(eb);
  exit(0);
}

void rescan_config(void) {
  syslog(LOG_INFO,"rescanning job configuration");

  /* udp sockets get re-opened during config parsing */
  close_sockets(&cfg); 

  UT_array *jobs; utarray_new(jobs, &job_mm);
  UT_string *em; utstring_new(em);

  UT_array *jobs_save = cfg.jobs;
  cfg.jobs = jobs;

  if (parse_jobs(&cfg, em) == -1) {
    /* on a parse failure we stop running any jobs! */
    syslog(LOG_CRIT,"FAILED to parse %s", cfg.file);
    syslog(LOG_CRIT,"ERROR: %s\n", utstring_body(em));
    syslog(LOG_CRIT,"NOTE: using EMPTY job config!");
    cfg.jobs = jobs_save;   /* restore saved jobs list */
    term_jobs(cfg.jobs);    /* terminate all jobs- treat as emtpy config file */
    utarray_clear(cfg.jobs);
    goto done;
  }

  /* parse succeeded. diff the new jobs vs. existing jobs.cfg */
  cfg.jobs = jobs_save;
  job_t *job=NULL, *pjob;
  while( (job = (job_t*)utarray_next(jobs,job))) {
    pjob = get_job_by_name(cfg.jobs, job->name);
    if (!pjob) continue; // new job definition; startup forthcoming
    if (job_cmp(job,pjob) == 0) { // new job identical to old
      job->pid = pjob->pid;
      job->start_ts = pjob->start_ts;
      job->respawn = pjob->respawn;
    } else {
      term_job(pjob); // job definition changed; job restart required 
    }
    utarray_erase(cfg.jobs, utarray_eltidx(cfg.jobs,pjob), 1);
  }
  /* any jobs left in cfg.jobs are no longer in the new configuration */
  pjob=NULL;
  while ( (pjob=(job_t*)utarray_next(cfg.jobs,pjob))) term_job(pjob);
  utarray_clear(cfg.jobs);
  utarray_concat(cfg.jobs,jobs);  /* make the new jobs official */

 done:
  utarray_free(jobs);
  utstring_free(em);
}

int main (int argc, char *argv[]) {
  int n, opt, es, elapsed, defer_restart;
  job_t *job;
  pid_t pid, dm_pid=0;

  UT_string *em, *sm;
  utstring_new(em);
  utstring_new(sm);

  while ( (opt = getopt(argc, argv, "v+Fc:s:t")) != -1) {
    switch (opt) {
      case 'v': cfg.verbose++; break;
      case 'F': cfg.foreground=1; break;
      case 'c': cfg.file=strdup(optarg); break;
      case 't': cfg.test_only=1; cfg.foreground=1; break;
      default: usage(argv[0]); break;
    }
  }
  openlog("pmtr", LOG_PID | (cfg.foreground ? LOG_PERROR : 0), LOG_LOCAL0);
  if (!cfg.file) cfg.file = strdup(DEFAULT_PM_CONFIG);

  if (!cfg.foreground) {       /* daemon init */
    if (fork() != 0) exit(0);  /* parent exit */
    setsid();                  /* new session - no controlling terminal */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  }
  umask(0);

  utarray_new(cfg.jobs, &job_mm);
  utarray_new(cfg.listen, &ut_int_icd);
  utarray_new(cfg.report, &ut_int_icd);
  utstring_new(cfg.s);
  utstring_new(cfg.ident);

  /* block all signals. we remain fully blocked except in sigsuspend */
  sigset_t all;
  sigfillset(&all);
  sigprocmask(SIG_SETMASK,&all,NULL);

  /* parse config file. we blocked signals above because we can get SIGIO 
   * during parsing if we open UDP listeners and get any datagrams */
  if (parse_jobs(&cfg, em) == -1) {
    syslog(LOG_ERR,"parse failed: %s\n", utstring_body(em));
    goto done;
  }

  if (cfg.test_only) goto done;
  syslog(LOG_INFO,"pmtr: starting\n");

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
      do_jobs(cfg.jobs);
      dm_pid = dep_monitor(cfg.file);
      report_status(&cfg);
      alarm_within(&cfg,SHORT_DELAY);
      break;
    case SIGHUP:
      rescan_config();
      do_jobs(cfg.jobs);
      break;
    case SIGCHLD:
      /* loop over children that have exited */
      defer_restart=0;
      while ( (pid = waitpid(-1, &es, WNOHANG)) > 0) {
        if (pid == dm_pid) { dm_pid = dep_monitor(cfg.file); continue; }
        job = get_job_by_pid(cfg.jobs, pid);
        if (!job) {
           syslog(LOG_ERR,"sigchld for unknown pid %d", (int)pid);
           goto done; /* restart for sanity purposes */
           break;
        }
        job->pid = 0;
        elapsed = time(NULL) - job->start_ts;
        if (elapsed < SHORT_DELAY) defer_restart=1;
        utstring_clear(sm);
        utstring_printf(sm,"job %s %d exited after %d sec: ",job->name, (int)pid, elapsed);
        if (WIFSIGNALED(es)) utstring_printf(sm, "signal %d", (int)WTERMSIG(es));
        else if (WIFEXITED(es)) { 
          int ex = WEXITSTATUS(es);
          utstring_printf(sm,"exit status %d", ex);
          if (job->once || (ex == PM_NO_RESTART)) {
            job->respawn=0;
            utstring_printf(sm," (will not be restarted)");
          }
        }
        syslog(LOG_INFO,"%s",utstring_body(sm));
      }
      if (!defer_restart) do_jobs(cfg.jobs);
      else syslog(LOG_INFO,"job restarting too fast, delaying restart\n");
      break;
    case SIGALRM:
      do_jobs(cfg.jobs);
      report_status(&cfg);
      alarm_within(&cfg,SHORT_DELAY);
      break;
    case SIGIO:  /* our UDP listener (if enabled) got a datagram */
      service_socket(&cfg);
      break;
    default:
      syslog(LOG_INFO,"pmtr: exiting on signal %d\n", signo);
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
  term_jobs(cfg.jobs);
  close_sockets(&cfg);
  free(cfg.file);
  utarray_free(cfg.jobs);
  utarray_free(cfg.listen);
  utarray_free(cfg.report);
  utstring_free(cfg.s);
  utstring_free(cfg.ident);
  utstring_free(em);
  utstring_free(sm);
  return 0;
}
