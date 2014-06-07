#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <pwd.h>

//#define DEBUG 1

#include "utarray.h"
#include "pmtr.h"
#include "job.h"


/* lemon prototypes */
void *ParseAlloc();
void Parse();
void ParseFree();

void job_ini(job_t *job) { 
  memset(job,0,sizeof(*job));
  utarray_init(&job->cmdv, &ut_str_icd); 
  utarray_init(&job->envv, &ut_str_icd); 
  job->respawn=1;
  job->uid=-1;
}
void job_fin(job_t *job) { 
  if (job->name) free(job->name);
  utarray_done(&job->cmdv); 
  utarray_done(&job->envv); 
  if (job->dir) free(job->dir);
  if (job->out) free(job->out);
  if (job->err) free(job->err);
  if (job->in) free(job->in);
}
void job_cpy(job_t *dst, const job_t *src) {
  dst->name = src->name ? strdup(src->name) : NULL;
  utarray_init(&dst->cmdv, &ut_str_icd); utarray_concat(&dst->cmdv, &src->cmdv);
  utarray_init(&dst->envv, &ut_str_icd); utarray_concat(&dst->envv, &src->envv);
  dst->dir = src->dir ? strdup(src->dir) : NULL;
  dst->out = src->out ? strdup(src->out) : NULL;
  dst->err = src->err ? strdup(src->err) : NULL;
  dst->in = src->in ? strdup(src->in) : NULL;
  dst->uid = src->uid;
  dst->pid = src->pid;
  dst->start_ts = src->start_ts;
  dst->start_at = src->start_at;
  dst->terminate = src->terminate;
  dst->delete_when_collected = src->delete_when_collected;
  dst->respawn = src->respawn;
  dst->order = src->order;
  dst->disabled = src->disabled;
  dst->wait = src->wait;
  dst->once = src->once;
  dst->bounce_interval = src->bounce_interval;
}
const UT_icd job_mm={sizeof(job_t), (init_f*)job_ini, 
                    (ctor_f*)job_cpy, (dtor_f*)job_fin };

/* these "set_" functions are called from the lemon-generated parser.  they
 * indicate an error by setting ps->rc to -1. the values they set are one of
 * the fields in ps->job. */
#define mk_setter(n)                                                \
void set_ ## n(parse_t *ps, char *v) {                              \
  if (ps->job->n) {                                                 \
    utstring_printf(ps->em, #n " respecified near line %d in %s ",  \
                    ps->line, ps->cfg->file);                       \
    ps->rc = -1;                                                    \
    return;                                                         \
  }                                                                 \
  ps->job->n = strdup(v);                                           \
}
mk_setter(name);
mk_setter(dir);
mk_setter(out);
mk_setter(err);
mk_setter(in);

void set_cmd(parse_t *ps, char *cmd) { 
  utarray_insert(&ps->job->cmdv,&cmd,0);
}

void set_env(parse_t *ps, char *env) { 
  if (strchr(env,'=') == NULL) {
    utstring_printf(ps->em, "environment string must be VAR=VALUE");
    ps->rc = -1;
    return;
  }
  utarray_push_back(&ps->job->envv,&env);
}

void set_ord(parse_t *ps, char *ord) { 
  if (sscanf(ord,"%d",&ps->job->order) != 1) {
    utstring_printf(ps->em, "non-numeric order parameter");
    ps->rc = -1;
  }
}

void set_dis(parse_t *ps) { ps->job->disabled = 1; }
void set_wait(parse_t *ps) { ps->job->wait = 1; }
void set_once(parse_t *ps) { ps->job->once = 1; }

void set_bounce(parse_t *ps, char *timespec) { 
  int l = strlen(timespec), interval;
  char *unit_ptr = &timespec[l-1];
  char unit = *unit_ptr;
  *unit_ptr = '\0';
  if (sscanf(timespec, "%u", &interval) != 1) {
    utstring_printf(ps->em, "invalid time interval in 'bounce every'");
    ps->rc = -1;
    return;
  }

  switch (unit) {
    case 's': break;
    case 'm': interval *= 60;       break;
    case 'h': interval *= 60*60;    break;
    case 'd': interval *= 60*60*24; break;
    default: 
      utstring_printf(ps->em, "invalid time unit in 'bounce every'");
      ps->rc = -1;
      return;
  }

  ps->job->bounce_interval = interval;
}

void set_user(parse_t *ps, char *user) { 

  /* if syntax check only, don't validate username */
  if (ps->cfg->test_only) return; 

  struct passwd *p;
  p = getpwnam(user);
  if (p == NULL) {
    utstring_printf(ps->em, "can't find uid for user %s", user);
    ps->rc = -1;
    return;
  }
  ps->job->uid = p->pw_uid;
}

void push_job(parse_t *ps) {

  /* final validation */
  if (!ps->job->name) {
      utstring_printf(ps->em, "job has no name");
      ps->rc = -1;
  }

  if (ps->rc == -1) return;

  /* okay. polish it off and copy it into the jobs */
  utarray_extend_back(&ps->job->cmdv); /* put NULL on end of argv */
  utarray_push_back(ps->cfg->jobs, ps->job);
  /* reset job for another parse */
  job_fin(ps->job); 
  job_ini(ps->job);
}
char *unquote(char *str) {
  assert(*str == '"');
  char *q = strchr(str+1,'"');
  assert(q);
  *q = '\0';
  return str+1;
}

/* this function reads a whole file into a malloc'd buffer */
char *slurp(char *file, size_t *len) {
  struct stat s;
  char *buf;
  int fd;
  if (stat(file, &s) == -1) {
      fprintf(stderr,"can't stat %s: %s\n", file, strerror(errno));
      exit(-1);
  }
  *len = s.st_size;
  if ( (fd = open(file, O_RDONLY)) == -1) {
      fprintf(stderr,"can't open %s: %s\n", file, strerror(errno));
      exit(-1);
  }
  buf = malloc(*len);
  if (buf) {
    if (read(fd, buf,*len) != *len) {
       fprintf(stderr,"incomplete read\n");
       exit(-1);
    }
  }
  close(fd);
  return buf;
}

static int order_sort(const void *_a, const void *_b) {
  job_t *a = (job_t*)_a, *b = (job_t*)_b;
  return a->order - b->order;
}
int parse_jobs(pmtr_t *cfg, UT_string *em) {
  char *buf, *c, *tok;
  size_t len,toklen;
  UT_array *toks;
  parse_t ps; /* our own parser state */ 
  void *p;    /* lemon parser */
  int id;

  utarray_new(toks,&ut_str_icd);
  job_t job;  /* "scratch" space used when parsing a job */
  job_ini(&job);
  ps.job=&job; 

  ps.cfg=cfg;
  ps.line=1; 
  ps.em=em;
  ps.rc=0;

  p = ParseAlloc(malloc);

  buf = slurp(ps.cfg->file, &len);
  c = buf;
  while ( (id=get_tok(buf,&c,&len,&toklen,&ps.line)) > 0) {
    tok = strndup(c,toklen); utarray_push_back(toks,&tok); free(tok); 
    tok = *(char**)utarray_back(toks);
    if (cfg->verbose >=2) printf("token [%s] id=%d line=%d\n",tok, id, ps.line);
    Parse(p, id, tok, &ps);
    if (ps.rc == -1) goto done;
    len -= toklen;
    c += toklen;
  }
  if (id == -1) {
    utstring_printf(em,"syntax error in %s line %d", ps.cfg->file, ps.line);
    ps.rc = -1;
    goto done;
  }
  Parse(p, 0, NULL, &ps);
  if (ps.rc == -1) goto done;

  /* parsing succeeded */
  utarray_sort(cfg->jobs, order_sort);

 done:
  utarray_free(toks);
  ParseFree(p, free);
  free(buf);
  job_fin(&job);
  return ps.rc;
}

void signal_job(job_t *job) {
  time_t now = time(NULL);
  assert(job->pid);
  switch(job->terminate) {
   case 0: /* should not be here */ break;
   case 1: /* initial termination request */
     syslog(LOG_INFO,"sending SIGTERM to job %s [%d]", job->name, job->pid);
     if (kill(job->pid,SIGTERM)==-1)syslog(LOG_ERR,"error: %s",strerror(errno));
     job->terminate = now+SHORT_DELAY;/* how long to wait before kill -9*/
     break;
   default: /* job didn't exit, use stronger signal if time has elapsed */
     if (job->terminate > now) break;
     syslog(LOG_INFO,"sending SIGKILL to job %s [%d]", job->name, job->pid);
     if (kill(job->pid,SIGKILL)==-1)syslog(LOG_ERR,"error: %s",strerror(errno));
     job->terminate = 0; /* don't repeatedly signal */
     break;
  }
}

/* start up the jobs that are not already running */
void do_jobs(pmtr_t *cfg) {
  pid_t pid;
  time_t now, elapsed;
  int es, n, fo, fe, fi, rc=-1, sig, kr;
  char *pathname, *o, *e, *i, **argv, **env;

  job_t *job = NULL;
  while ( (job = (job_t*)utarray_next(cfg->jobs,job))) {
    if (job->bounce_interval && job->pid) { 
      now = time(NULL);
      elapsed = now - job->start_ts;
      if (elapsed >= job->bounce_interval) job->terminate=1; 
    }
    if (job->terminate) {signal_job(job); continue;}
    if (job->disabled) continue;
    if (job->pid) continue;  /* running already */
    if (job->respawn == 0) continue;  /* don't respawn */
    if (job->start_at > time(&now)) {  /* not yet */
      alarm_within(cfg, job->start_at - now);
      continue;
    }

    if ( (pid = fork()) == -1) {
      syslog(LOG_ERR,"fork error\n");
      kill(getpid(), 15); /* induce graceful shutdown in main loop */
      return;
    }

    if (pid > 0) {  /* parent */
      job->pid = pid;
      job->start_ts = time(NULL);
      syslog(LOG_INFO,"started job %s [%d]", job->name, (int)job->pid);
      /* support the 'wait' feature which pauses (blocks) for a job to finish.
       * this is most likely useful with 'order 0' to do some pre-init work. */
      if (job->wait) {
        syslog(LOG_INFO,"pausing for job %s to finish",job->name);
        if (waitpid(job->pid, &es, 0) != job->pid) {
          syslog(LOG_ERR,"waitpid for job %s failed\n",job->name);
          continue;
        }
        syslog(LOG_INFO,"job %s finished",job->name);
        if (WIFEXITED(es) && (WEXITSTATUS(es) == PM_NO_RESTART)) job->respawn=0;
        else if (job->once) job->respawn=0;
        job->pid = 0;
      }
      continue;
    }

    /*********************************************************************
     * child here 
     ********************************************************************/
    assert(pid == 0);

    /* setup working dir */
    if (job->dir && (chdir(job->dir) == -1))                 {rc=-1; goto fail;}

    /* close inherited descriptors- just syslog. (our udp sockets close-on-exec).
     * we closelog() _before_ dups below, otherwise it can clobber wrong fd */
    closelog(); 

    /* setup child stdin/stdout/stderr */
    i = job->in  ? job->in  : "/dev/null";
    o = job->out ? job->out : "/dev/null";
    e = job->err ? job->err : "/dev/null";

    fi = open(i,O_RDONLY);                        if(fi==-1) {rc=-2; goto fail;}
    fo = open(o,O_WRONLY|O_APPEND|O_CREAT, 0644); if(fo==-1) {rc=-3; goto fail;}
    fe = open(e,O_WRONLY|O_APPEND|O_CREAT, 0644); if(fe==-1) {rc=-4; goto fail;}

    rc = -5;
    if (fi!=STDIN_FILENO)  {if(dup2(fi,STDIN_FILENO) ==-1)goto fail; close(fi);}
    if (fo!=STDOUT_FILENO) {if(dup2(fo,STDOUT_FILENO)==-1)goto fail; close(fo);}
    if (fe!=STDERR_FILENO) {if(dup2(fe,STDERR_FILENO)==-1)goto fail; close(fe);}

    /* set environment variables */
    env=NULL;
    while ( (env=(char**)utarray_next(&job->envv,env))) putenv(*env);

    /* restore/unblock default handlers so they're unblocked after exec */
    for(n=0; n < sizeof(sigs)/sizeof(*sigs); n++) signal(sigs[n], SIG_DFL);
    sigset_t none;
    sigemptyset(&none);
    sigprocmask(SIG_SETMASK,&none,NULL);

    /* change the real and effective user ids */
    if ((job->uid != -1) && (setuid(job->uid) == -1))        {rc=-6; goto fail;}

    /* at last. we're ready to run the child process */
    argv = (char**)utarray_front(&job->cmdv);
    pathname = *argv;
    if (execv(pathname, argv) == -1)                         {rc=-7; goto fail;}

    /* not reached - child has exec'd */
    assert(0); 

   fail:
    if (rc==-1) syslog(LOG_ERR,"can't chdir %s: %s", job->dir, strerror(errno));
    if (rc==-2) syslog(LOG_ERR,"can't open %s: %s", i, strerror(errno));
    if (rc==-3) syslog(LOG_ERR,"can't open %s: %s", o, strerror(errno));
    if (rc==-4) syslog(LOG_ERR,"can't open %s: %s", e, strerror(errno));
    if (rc==-5) syslog(LOG_ERR,"can't dup: %s", strerror(errno));
    if (rc==-6) syslog(LOG_ERR,"can't setuid %d: %s", job->uid,strerror(errno));
    if (rc==-7) syslog(LOG_ERR,"can't exec %s: %s", pathname, strerror(errno));
    exit(-1);  /* child exit */
  }
}

void collect_jobs(pmtr_t *cfg, UT_string *sm) {
  int es, ex, elapsed;
  time_t now;
  job_t *job;
  pid_t pid;

  while ( (pid = waitpid(-1, &es, WNOHANG)) > 0) {

    /* just respawn if it's our dependency monitor */
    if (pid==cfg->dm_pid) { 
      cfg->dm_pid = dep_monitor(cfg->file);
      continue;
    }
    /* find the job.  we should always find it by pid. */
    job = get_job_by_pid(cfg->jobs, pid);
    if (!job) {
      syslog(LOG_ERR,"sigchld for unknown pid %d",(int)pid); 
      continue;
    }
    /* decide if and when it should be restarted */
    job->pid = 0;
    job->terminate = 0; /* any termination request has succeeded */
    now = time(NULL);
    elapsed = now - job->start_ts;
    job->start_at = (elapsed < SHORT_DELAY) ? (now+SHORT_DELAY) : now;
    if (job->once) job->respawn=0;

    /* write a log message about how the job exited */
    utstring_clear(sm);
    utstring_printf(sm,"job %s [%d] exited after %d sec: ", job->name, 
      (int)pid, elapsed);
    if (WIFSIGNALED(es)) utstring_printf(sm, "signal %d", (int)WTERMSIG(es));
    if (WIFEXITED(es)) {
      if ( (ex = WEXITSTATUS(es)) == PM_NO_RESTART) job->respawn=0;
      utstring_printf(sm,"exit status %d", ex);
    }
    syslog(LOG_INFO,"%s",utstring_body(sm));

    /* is this a former job that was deleted from the config file? */
    if (job->delete_when_collected) {
      utarray_erase(cfg->jobs, utarray_eltidx(cfg->jobs,job), 1);
      job=NULL;
    }
  }
}

/* sets termination flags. run do_jobs() after to actually signal them */
void term_jobs(UT_array *jobs) {
  job_t *job = NULL;
  while ( (job = (job_t*)utarray_next(jobs,job))) {
    if (job->pid == 0) continue;
    if (job->terminate==0) job->terminate=1;
  }
}

job_t *get_job_by_pid(UT_array *jobs, pid_t pid) {
  job_t *job = NULL;
  while ( (job = (job_t*)utarray_next(jobs,job))) {
    if (job->pid == pid) return job;
  }
  return NULL;
}

job_t *get_job_by_name(UT_array *jobs, char *name) {
  job_t *job = NULL;
  while ( (job = (job_t*)utarray_next(jobs,job))) {
    if (!strcmp(job->name,name)) return job;
  }
  return NULL;
}

/* this comparison function is used to see if two job _definitions_ (not
 * running instances) are equal. it is used when rescanning the config file */
int job_cmp(job_t *a, job_t *b) {
  char **ac,**bc;
  int rc, alen, blen;
  if ( (rc=strcmp(a->name,b->name)) != 0) return rc;
  /* compare cmdv */
  alen = utarray_len(&a->cmdv); blen = utarray_len(&b->cmdv); 
  if (alen != blen) return alen-blen;
  ac=NULL; bc=NULL;
  while ( (ac=(char**)utarray_next(&a->cmdv,ac))) {
    bc = (char**)utarray_next(&b->cmdv,bc);
    if ((*ac && *bc) && ((rc=strcmp(*ac,*bc)) != 0)) return rc;
  }
  /* compare envv */
  alen = utarray_len(&a->envv); blen = utarray_len(&b->envv); 
  if (alen != blen) return alen-blen;
  ac=NULL; bc=NULL;
  while ( (ac=(char**)utarray_next(&a->envv,ac))) {
    bc = (char**)utarray_next(&b->envv,bc);
    if ((*ac && *bc) && ((rc=strcmp(*ac,*bc)) != 0)) return rc;
  }
  /* dir */
  if ((!a->dir && b->dir) || (a->dir && !b->dir) ) return a->dir-b->dir;
  if ((a->dir && b->dir) && (rc = strcmp(a->dir,b->dir))) return rc;
  /* out */
  if ((!a->out && b->out) || (a->out && !b->out) ) return a->out-b->out;
  if ((a->out && b->out) && (rc = strcmp(a->out,b->out))) return rc;
  /* err */
  if ((!a->err && b->err) || (a->err && !b->err) ) return a->err-b->err;
  if ((a->err && b->err) && (rc = strcmp(a->err,b->err))) return rc;
  /* in */
  if ((!a->in && b->in) || (a->in && !b->in) ) return a->in-b->in;
  if ((a->in && b->in) && (rc = strcmp(a->in,b->in))) return rc;

  if (a->uid != b->uid) return a->uid - b->uid;
  if (a->order != b->order) return a->order - b->order;
  if (a->disabled != b->disabled) return a->disabled - b->disabled;
  if (a->wait != b->wait) return a->wait - b->wait;
  if (a->once != b->once) return a->once - b->once;
  if (a->bounce_interval != b->bounce_interval) return a->bounce_interval - b->bounce_interval;
  return 0;
}

/* certain events require a periodic timer. these are deferred job 
 * restarts, and periodic status reporting. when we establish the
 * alarm handler, we only need to reset the timer if the next scheduled
 * alarm is non-existent or its later than we need. 
 */
void alarm_within(pmtr_t *cfg, int sec) {
  time_t now = time(NULL);
  int reset = 0;

  if (cfg->next_alarm == 0) reset=1;      /* first time setup */
  if (cfg->next_alarm <= now) reset=1;    /* alarm woke us up, rescheduling */
  if (cfg->next_alarm > now+sec) reset=1; /* move alarm earlier */
  if (!reset) {
    alarm(cfg->next_alarm - now);         /* already set but play it safe .*/
    return;
  }

  if (sec==0) sec=1;                      /* zero-second timer is ignored */
  cfg->next_alarm = now+sec;
  alarm(sec);
}

