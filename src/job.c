#define _GNU_SOURCE /* To get CPU_SET macros*/
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
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
  utarray_init(&job->depv, &ut_str_icd); 
  utarray_init(&job->rlim, &rlimit_icd); 
  CPU_ZERO(&job->cpuset);
  job->respawn=1;
}
void job_fin(job_t *job) { 
  if (job->name) free(job->name);
  utarray_done(&job->cmdv); 
  utarray_done(&job->envv); 
  utarray_done(&job->depv); 
  utarray_done(&job->rlim); 
  if (job->dir) free(job->dir);
  if (job->out) free(job->out);
  if (job->err) free(job->err);
  if (job->in) free(job->in);
}
void job_cpy(job_t *dst, const job_t *src) {
  int i;
  dst->name = src->name ? strdup(src->name) : NULL;
  utarray_init(&dst->cmdv, &ut_str_icd); utarray_concat(&dst->cmdv, &src->cmdv);
  utarray_init(&dst->envv, &ut_str_icd); utarray_concat(&dst->envv, &src->envv);
  utarray_init(&dst->depv, &ut_str_icd); utarray_concat(&dst->depv, &src->depv);
  utarray_init(&dst->rlim, &rlimit_icd); utarray_concat(&dst->rlim, &src->rlim);
  dst->dir = src->dir ? strdup(src->dir) : NULL;
  dst->out = src->out ? strdup(src->out) : NULL;
  dst->err = src->err ? strdup(src->err) : NULL;
  dst->in = src->in ? strdup(src->in) : NULL;
  memcpy(dst->user, src->user, PMTR_MAX_USER);
  dst->pid = src->pid;
  dst->start_ts = src->start_ts;
  dst->start_at = src->start_at;
  dst->terminate = src->terminate;
  dst->delete_when_collected = src->delete_when_collected;
  dst->respawn = src->respawn;
  dst->order = src->order;
  dst->nice = src->nice;
  dst->disabled = src->disabled;
  dst->wait = src->wait;
  dst->once = src->once;
  dst->bounce_interval = src->bounce_interval;
  dst->deps_hash = src->deps_hash;
  CPU_ZERO(&dst->cpuset);
  for(i = 0; i < CPU_SETSIZE; i++) {
    if (CPU_ISSET(i, &src->cpuset)) {
      CPU_SET(i, &dst->cpuset);
    }
  }
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

/* cpuset is expressed as a hex mask in the form 0x4A
 * or as a comma-delimited list of numbers and ranges
 * e.g. 1,3-5,8
 */
void set_cpu(parse_t *ps, char *cpu_spec) { 
  unsigned cpu, i, in_range, range_start, range_end, ndig;
  unsigned char *c, d, peek;
  size_t len;

  len = strlen(cpu_spec);

  /* parse 0xABC form of cpu mask */
  if (strncmp(cpu_spec, "0x", 2) == 0) {
    cpu_spec += 2;
    len -= 2;
    if (len == 0) {
      utstring_printf(ps->em, "parse error in cpuset");
      ps->rc = -1;
      return;
    }

    for(c=cpu_spec; *c != '\0'; c++) {
      if      (*c >= '0' && *c <= '9') d = *c-'0';
      else if (*c >= 'a' && *c <= 'f') d = *c-'a'+10;
      else if (*c >= 'A' && *c <= 'F') d = *c-'A'+10;
      else {
        utstring_printf(ps->em, "invalid hex in cpuset");
        ps->rc = -1;
        return;
      }
      /* parse one number in the range 0-15 into bits */
      for(i = 0; i < 4; i++) {
        if (d & (1 << i)) {
          cpu = i + (len-1)*4;
          CPU_SET(cpu, &ps->job->cpuset);
        }
      }
      len--;
    }
    return;
  }

  /* parse numbers and ranges format e.g. "12,14-17" */
  in_range = 0;
  d = 0;
  ndig = 0;
  for(c = cpu_spec; *c != '\0'; c++) {
    if (*c >= '0' && *c <= '9') {
      d = (d*10) + *c-'0';
      ndig++;
      peek = *(c+1);
      if (peek == '\0' || peek == ',') {
          if (in_range) {
            range_end = d;
            in_range = 0;
            if (range_end <= range_start) {
              goto fail;
            }
          } else {
            range_start = d;
            range_end = d;
          }
          for(cpu = range_start; cpu <= range_end; cpu++) {
            CPU_SET(cpu, &ps->job->cpuset);
          }
          d = 0;
      }
    } else if (*c == ',') {
      peek = *(c+1);
      if ((ndig == 0) || peek < '0' || peek > '9') goto fail;
    } else if (*c == '-') {
      if ((ndig == 0) || in_range) goto fail;
      peek = *(c+1);
      if (peek < '0' || peek > '9') goto fail;
      range_start = d;
      in_range = 1;
      d = 0;
    } else goto fail;
  }

  return;

 fail:
  utstring_printf(ps->em, "syntax error in cpuset");
  ps->rc = -1;
  return;
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

#define MIN_NICE -20 /* highest priority */
#define MAX_NICE  19 /* lowest priority */
void set_nice(parse_t *ps, char *nice) { 
  if (sscanf(nice,"%d",&ps->job->nice) != 1) {
    utstring_printf(ps->em, "non-numeric nice parameter");
    ps->rc = -1;
  }

  if ((ps->job->nice < MIN_NICE) ||
      (ps->job->nice > MAX_NICE)) {
    utstring_printf(ps->em, "nice out of range %d to %d", MIN_NICE, MAX_NICE);
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
  size_t len = strlen(user);
  if (len+1 > PMTR_MAX_USER) {
    utstring_printf(ps->em, "user name too long");
    ps->rc = -1;
    return;
  }
  memcpy(ps->job->user, user, len+1);
}

#define unlimited(a) 
void set_ulimit(parse_t *ps, char *rname, char *value_a) {
  rlim_t rval;
  int v;

  /* parse the numeric value of the rlimit */
  if ((!strcmp(value_a,"infinity")) || (!strcmp(value_a,"unlimited")))
    rval = RLIM_INFINITY;
  else if (sscanf(value_a, "%u", &v) == 1) {
    rval = v;
  } else {
    utstring_printf(ps->em, "non-numeric ulimit value");
    ps->rc = -1;
    return;
  }

  int i;
  for(i=0; i<adim(rlimit_labels); i++) {
    if ( (!strcmp(rname, rlimit_labels[i].flag)) || // accept a flag like -m or 
         (!strcmp(rname, rlimit_labels[i].name))) { // full name like RLIMIT_RSS
      resource_rlimit_t rt;
      rt.id = rlimit_labels[i].id;
      rt.rlim.rlim_cur = rval;
      rt.rlim.rlim_max = rval;
      /* prevent "ulimit -n infinity", POSIX allows, but it doesn't work */
      if ((rt.id == RLIMIT_NOFILE) && (rval == RLIM_INFINITY)) {
        utstring_printf(ps->em, "ulimit -n must be finite");
        ps->rc = -1;
        return;
      }
      utarray_push_back(&ps->job->rlim, &rt);
      return;
    }
  }

  utstring_printf(ps->em, "unknown ulimit resource %s", rname);
  ps->rc = -1;

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

/* prefix a relative filename with the job directory, if set.
 * uses static storage, overwritten from call to call! */
char *fpath(job_t *job, char *file) {
  static char path[PATH_MAX];
  if (*file == '/') return file;
  if (!job->dir   ) return file;
  size_t dlen = strlen(job->dir);
  size_t flen = strlen(file);
  if (dlen + flen + 2 > sizeof(path)) return NULL;
  strcpy(path, job->dir);
  strcat(path, "/");
  strcat(path, file);
  return path;
}

/* this function reads a whole file into a malloc'd buffer */
int slurp(char *file, char **text, size_t *len) {
  struct stat s;
  char *buf;
  int fd = -1, rc=-1, nr;
  *text=NULL; *len = 0;

  if (stat(file, &s) == -1) {
    syslog(LOG_ERR,"can't stat %s: %s", file, strerror(errno));
    goto done;
  }
  *len = s.st_size;
  if (*len == 0) {rc=0; goto done;} // special case, empty file
  if ( (fd = open(file, O_RDONLY)) == -1) {
    syslog(LOG_ERR,"can't open %s: %s", file, strerror(errno));
    goto done;
  }
  if ( (*text = malloc(*len)) == NULL) goto done;
  if ( (nr=read(fd, *text, *len)) != *len) {
   if (nr == -1) {
     syslog(LOG_CRIT,"read %s failed: %s", file, strerror(errno));
   } else {
     syslog(LOG_CRIT,"read %s failed: incomplete (%u/%u)", file, 
        nr, (unsigned)*len);
   }
   goto done;
  }
  rc = 0;

 done:
  if ((rc < 0) && *text) { free(*text); *text=NULL; }
  if (fd != -1) close(fd);
  return rc;
}

// record a hash of each job's dependencies; rehash later to detect changes
int hash_deps(UT_array *jobs) {
  char *text, *t;
  size_t len, l;
  int rc = -1;

  job_t *job=NULL;
  while ( (job=(job_t*)utarray_next(jobs,job))) {
    job->deps_hash=0;
    char **dep=NULL;
    while( (dep=(char**)utarray_next(&job->depv,dep))) {
      if (slurp(fpath(job,*dep), &text, &len) < 0) {
        syslog(LOG_ERR,"job %s: can't open dependency %s", job->name, *dep);
        job->disabled = 1; /* they need to fix pmtr.conf to trigger rescan */
        if (job->pid) job->terminate=1;
        continue;
      }
      t=text; l=len;
      while (l--) job->deps_hash = (job->deps_hash * 33) + *t++;
      memset(text,0,len); // keep our address space free of random files :-)
      free(text);
    }
  }

  rc=0;

 done:
  return rc;
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

  if (slurp(ps.cfg->file, &buf, &len) < 0) {ps.rc=-1; goto done;}
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
  hash_deps(cfg->jobs);

 done:
  utarray_free(toks);
  ParseFree(p, free);
  if (buf) free(buf);
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

/* open descriptor to the logger socket on given fd */
int logger_on(pmtr_t *cfg, int dst_fd) {
  struct sockaddr_un addr;
  int sc, fd, rc = -1;

  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1) goto done;

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  assert(cfg->logger_namelen > 0);
  memcpy(addr.sun_path, cfg->logger_socket, cfg->logger_namelen);

  socklen_t len = sizeof(sa_family_t) + cfg->logger_namelen;
  sc = connect(fd, (struct sockaddr*)&addr, len);
  if (sc == -1) goto done;

  if (fd != dst_fd) {
		sc = dup2(fd, dst_fd);
		if (sc < 0) goto done;
		sc = close(fd);
		if (sc < 0) goto done;
  }

  rc = 0;

 done:
  return rc;
}

/* open filename and dup so fileno becomes attached to it */ 
int redirect(pmtr_t *cfg, int fileno, char *filename, int flags, int mode) {
  int rc = -1, fd, sc;

  if (filename == NULL) { /* nothing to do */
    rc = 0;
    goto done;
  }

  /* handle reserved word - syslog */
  if (!strcmp(filename, "syslog")) {
    rc = logger_on(cfg, fileno);
    goto done;
  }

  /* regular file */
  fd = open(filename, flags, mode);
  if (fd < 0) goto done;

  if (fd != fileno) {
    sc = dup2(fd, fileno);
    if (sc < 0) goto done;
    close(fd);
  }

  rc = 0;

 done:
  return rc;
}

/* start up the jobs that are not already running */
void do_jobs(pmtr_t *cfg) {
  pid_t pid;
  time_t now, elapsed;
  int es, n, fo, fe, fi, rc=-1, ds;
  char *pathname, *o, *e, *i, **argv, **env;

  job_t *job = NULL;
  while ( (job = (job_t*)utarray_next(cfg->jobs,job))) {
    if (job->bounce_interval && job->pid) { 
      now = time(NULL);
      elapsed = now - job->start_ts;
      if (elapsed >= job->bounce_interval) {
        if (job->terminate==0) job->terminate=1;
      }
    }
    if (job->terminate) {signal_job(job); continue;}
    if (job->disabled) continue;
    if (job->pid) continue;  /* running already */
    if (job->respawn == 0) continue;  /* don't respawn */
    if (job->start_at > time(&now)) {  /* not yet */
      alarm_within(cfg, job->start_at - now);
      continue;
    }

    pid = fork();

    if (pid == -1) {
      syslog(LOG_ERR,"fork error\n");
      kill(getpid(), 15); /* induce graceful shutdown in main loop */
      return;
    }

    if (pid > 0) {  /* parent */
      job->pid = pid;
      job->start_ts = time(NULL);
      syslog(LOG_INFO,"started job %s [%d]", job->name, (int)job->pid);
      /* support the 'wait' feature which pauses (blocks) for a job to finish.*/
      if (job->wait) {
        syslog(LOG_INFO,"pausing for job %s to finish",job->name);
        if (waitpid(job->pid, &es, 0) != job->pid) {
          syslog(LOG_ERR,"waitpid for job %s failed\n",job->name);
          continue;
        }
        syslog(LOG_INFO,"job %s finished",job->name);
        if (WIFEXITED(es) && (WEXITSTATUS(es) == PMTR_NO_RESTART)) job->respawn=0;
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

    /* close inherited descriptors */
    closelog(); 

    /* set environment variables */
    env=NULL;
    while ( (env=(char**)utarray_next(&job->envv,env))) putenv(*env);

    /* set process priority / nice */
    if (setpriority(PRIO_PROCESS, 0, job->nice) < 0)         {rc=-5; goto fail;}

    /* set cpu affinity, if any */
    if ((CPU_COUNT(&job->cpuset) > 0) &&
      sched_setaffinity(0, sizeof(cpu_set_t), &job->cpuset)) {rc=-12; goto fail;}

    /* set ulimits */
    resource_rlimit_t *rt=NULL;
    while ( (rt=(resource_rlimit_t*)utarray_next(&job->rlim,rt))) {
      struct rlimit new_limit = {.rlim_cur=rt->rlim.rlim_cur,
                                 .rlim_max=rt->rlim.rlim_max};
      if (setrlimit(rt->id, &new_limit))                     {rc=-6; goto fail;}
    }

    /* restore/unblock default handlers so they're unblocked after exec */
    for(n=0; n < sizeof(sigs)/sizeof(*sigs); n++) signal(sigs[n], SIG_DFL);
    sigset_t none;
    sigemptyset(&none);
    sigprocmask(SIG_SETMASK,&none,NULL);

    /* change the real and effective user ids, and set the gid and supp groups */
    if (*job->user) {
      struct passwd *p;
      if ( (p = getpwnam(job->user)) == NULL)                {rc=-7; goto fail;}
      if (setgid(p->pw_gid) == -1)                           {rc=-8; goto fail;}
      if (initgroups(job->user, p->pw_gid) == -1)            {rc=-9; goto fail;}
      if (setuid(p->pw_uid) == -1)                           {rc=-10; goto fail;}
    }

    /* redirect the child's stdout and stderr to syslog unless user specified */
    i = job->in  ? job->in  : "/dev/null";
    o = job->out ? job->out : "syslog";
    e = job->err ? job->err : "syslog";

    int flags_wr = O_WRONLY|O_CREAT|O_APPEND;
    if (redirect(cfg, STDIN_FILENO,  i, O_RDONLY, 0)    < 0) { rc=-2; goto fail;}
    if (redirect(cfg, STDOUT_FILENO, o, flags_wr, 0644) < 0) { rc=-3; goto fail;}
    if (redirect(cfg, STDERR_FILENO, e, flags_wr, 0644) < 0) { rc=-4; goto fail;}

    /* at last. we're ready to run the child process */
    argv = (char**)utarray_front(&job->cmdv);
    pathname = *argv;
    if (execv(pathname, argv) == -1)                         {rc=-11; goto fail;}

    /* not reached - child has exec'd */
    assert(0); 

   fail:
    if (rc==-1) syslog(LOG_ERR,"can't chdir %s: %s", job->dir, strerror(errno));
    if (rc==-2) syslog(LOG_ERR,"can't open/dup %s: %s", i, strerror(errno));
    if (rc==-3) syslog(LOG_ERR,"can't open/dup %s: %s", o, strerror(errno));
    if (rc==-4) syslog(LOG_ERR,"can't open/dup %s: %s", e, strerror(errno));
    if (rc==-5) syslog(LOG_ERR,"can't setpriority: %s", strerror(errno));
    if (rc==-6) syslog(LOG_ERR,"can't setrlimit: %s", strerror(errno));
    if (rc==-7) syslog(LOG_ERR,"unknown user: %s", job->user);
    if (rc==-8) syslog(LOG_ERR,"can't setgid %s: %s", job->user, strerror(errno));
    if (rc==-9) syslog(LOG_ERR,"can't initgroups %s: %s", job->user, strerror(errno));
    if (rc==-10) syslog(LOG_ERR,"can't setuid %s: %s", job->user, strerror(errno));
    if (rc==-11) syslog(LOG_ERR,"can't exec %s: %s", pathname, strerror(errno));
    if (rc==-12) syslog(LOG_ERR,"can't set cpu affinity: %s", strerror(errno));
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
    /* if it's our logger sub process ... */
    if (pid==cfg->logger_pid) { 
      kill(getpid(), 15); /* induce graceful shutdown in main loop */
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
      if ( (ex = WEXITSTATUS(es)) == PMTR_NO_RESTART) job->respawn=0;
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
  /* compare rlim */
  resource_rlimit_t *ar, *br;
  alen = utarray_len(&a->rlim); blen = utarray_len(&b->rlim); 
  if (alen != blen) return alen-blen;
  ar=NULL; br=NULL;
  while ( (ar=(resource_rlimit_t*)utarray_next(&a->rlim,ar))) {
    br = (resource_rlimit_t*)utarray_next(&b->rlim,br);
    if (ar->id != br->id) return -1;
    if (ar->rlim.rlim_cur != br->rlim.rlim_cur) return -1;
    if (ar->rlim.rlim_max != br->rlim.rlim_max) return -1;
  }
  /* compare depv and the hash of the dependencies */
  alen = utarray_len(&a->depv); blen = utarray_len(&b->depv); 
  if (alen != blen) return alen-blen;
  ac=NULL; bc=NULL;
  while ( (ac=(char**)utarray_next(&a->depv,ac))) {
    bc = (char**)utarray_next(&b->depv,bc);
    if ((*ac && *bc) && ((rc=strcmp(*ac,*bc)) != 0)) return rc;
  }
  if ( (rc = (a->deps_hash-b->deps_hash))) return rc;
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

  if ( (rc = memcmp(a->user, b->user, PMTR_MAX_USER))) return rc;
  if (a->order != b->order) return a->order - b->order;
  if (a->disabled != b->disabled) return a->disabled - b->disabled;
  if (a->wait != b->wait) return a->wait - b->wait;
  if (a->once != b->once) return a->once - b->once;
  if (a->bounce_interval != b->bounce_interval) return a->bounce_interval - b->bounce_interval;
  if (CPU_EQUAL(&a->cpuset, &b->cpuset) == 0) return -1;
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

/* if the config file needs to be made, create a blank one */
int instantiate_cfg_file(pmtr_t *cfg) {
  int rc = -1, sr, cs;
  struct stat s;

  /* does it exist? */
  sr = stat(cfg->file, &s);
  if (sr == 0) { rc = 0; goto done; } /* yes, done */

  /* try to create it */
  syslog(LOG_INFO,"creating empty %s", cfg->file);
  cs = open(cfg->file, O_WRONLY|O_CREAT|O_EXCL, 0600);
  if (cs < 0) {
    syslog(LOG_ERR,"can't create %s: %s", cfg->file, strerror(errno));
    goto done;
  } else {
    close(cs);
  }

  rc = 0;

 done:
  return rc;
}
