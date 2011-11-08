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
#include "job.h"


/* lemon prototypes */
void *ParseAlloc();
void Parse();
void ParseFree();

static void job_ini(job_t *job) { 
  memset(job,0,sizeof(*job));
  utarray_init(&job->cmdv, &ut_str_icd); 
  utarray_init(&job->envv, &ut_str_icd); 
  job->respawn=1;
  job->uid=-1;
}
static void job_fin(job_t *job) { 
  if (job->name) free(job->name);
  utarray_done(&job->cmdv); 
  utarray_done(&job->envv); 
  if (job->dir) free(job->dir);
  if (job->out) free(job->out);
  if (job->err) free(job->err);
  if (job->usr) free(job->usr);
}
static void job_cpy(job_t *dst, const job_t *src) {
  dst->name = src->name ? strdup(src->name) : NULL;
  utarray_init(&dst->cmdv, &ut_str_icd); utarray_concat(&dst->cmdv, &src->cmdv);
  utarray_init(&dst->envv, &ut_str_icd); utarray_concat(&dst->envv, &src->envv);
  dst->dir = src->dir ? strdup(src->dir) : NULL;
  dst->out = src->out ? strdup(src->out) : NULL;
  dst->err = src->err ? strdup(src->err) : NULL;
  dst->usr = src->usr ? strdup(src->usr) : NULL;
  dst->uid = src->uid;
  dst->pid = src->pid;
  dst->start_ts = src->start_ts;
  dst->respawn = src->respawn;
  dst->order = src->order;
  dst->disabled = src->disabled;
  dst->wait = src->wait;
  dst->once = src->once;
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
                    ps->line, ps->file);                            \
    ps->rc = -1;                                                    \
    return;                                                         \
  }                                                                 \
  ps->job->n = strdup(v);                                           \
}
mk_setter(name);
mk_setter(dir);
mk_setter(out);
mk_setter(err);
mk_setter(usr);
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
void push_job(parse_t *ps) {
  struct passwd *p;
  utarray_extend_back(&ps->job->cmdv); /* put NULL on end of argv */
  utarray_push_back(ps->jobs, ps->job);
  /* convert user id to uid */
  if (ps->job->usr) {
    if ( (p = getpwnam(ps->job->usr)) == NULL) {
      utstring_printf(ps->em, "cannot find uid for user %s", ps->job->usr);
      ps->rc = -1;
    } else {
      ps->job->uid = p->pw_uid;
    }
  }
  if (!ps->job->name) {
      utstring_printf(ps->em, "job has no name");
      ps->rc = -1;
  }
  /* reset job for another parse */
  if (ps->rc != -1) {
    job_fin(ps->job); 
    job_ini(ps->job);
  }
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
int parse_jobs(UT_array *jobs, UT_string *em, char *file, int verbose) {
  char *buf, *c, *tok;
  size_t len,toklen;
  UT_array *toks;
  parse_t ps; /* our own parser state */
  void *p;    /* lemon parser */
  int id;

  utarray_new(toks,&ut_str_icd);
  job_t job;  /* "scratch" space used when parsing a job */
  job_ini(&job);

  ps.line=1; ps.rc=0; ps.file=file; ps.em=em, ps.job=&job; ps.jobs=jobs;
  p = ParseAlloc(malloc);

  buf = slurp(file, &len);
  c = buf;
  while ( (id=get_tok(buf,&c,&len,&toklen,&ps.line)) > 0) {
    tok = strndup(c,toklen); utarray_push_back(toks,&tok); free(tok); 
    tok = *(char**)utarray_back(toks);
    if (verbose >= 2) printf("got token [%s] id=%d line=%d\n",tok, id, ps.line);
    Parse(p, id, tok, &ps);
    if (ps.rc == -1) goto done;
    len -= toklen;
    c += toklen;
  }
  if (id == -1) {
    utstring_printf(em,"tokenizer error in %s line %d\n", ps.file, ps.line);
    ps.rc = -1;
    goto done;
  }
  Parse(p, 0, NULL, &ps);
  if (ps.rc == -1) goto done;

  /* parsing succeeded */
  utarray_sort(jobs, order_sort);

 done:
  utarray_free(toks);
  ParseFree(p, free);
  free(buf);
  job_fin(&job);
  return ps.rc;
}

/* start up the jobs that are not already running */
void do_jobs(UT_array *jobs) {
  pid_t pid;
  int es, n, fo, fe, fi;
  char *pathname, *o, *e, **argv, **env;

  job_t *job = NULL;
  while ( (job = (job_t*)utarray_next(jobs,job))) {
    if (job->disabled) continue;
    if (job->pid) continue;  /* running already */
    if (job->respawn == 0) continue;  /* don't respawn */

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
    if (job->dir && (chdir(job->dir) == -1)) {
      syslog(LOG_ERR,"cannot chdir: %s\n", strerror(errno)); goto fail;
    }
    /* setup child stdout/stderr */
    o = job->out ? job->out : "/dev/null";
    e = job->err ? job->err : "/dev/null";
    if ( (fo = open(o, O_WRONLY|O_APPEND|O_CREAT, 0644)) == -1) {
      syslog(LOG_ERR,"cannot open %s: %s\n", o, strerror(errno)); goto fail;
    }
    if ( (fe = open(e, O_WRONLY|O_APPEND|O_CREAT, 0644)) == -1) {
      syslog(LOG_ERR,"cannot open %s: %s\n", e, strerror(errno)); goto fail;
    }
    if (dup2(fo,STDOUT_FILENO) == -1) {
      syslog(LOG_ERR,"cannot dup %s to stdout: %s\n", o, strerror(errno)); goto fail;
    }
    if (dup2(fe,STDERR_FILENO) == -1) {
      syslog(LOG_ERR,"cannot dup %s to stderr: %s\n", e, strerror(errno)); goto fail;
    }
    if ( (fi = open("/dev/null", O_RDONLY)) == -1) {
      syslog(LOG_ERR,"cannot open /dev/null: %s\n", strerror(errno)); goto fail;
    }
    if (dup2(fi,STDIN_FILENO) == -1) {
      syslog(LOG_ERR,"cannot dup /dev/null to stdin: %s\n", strerror(errno)); goto fail;
    }

    /* set environment variables */
    env=NULL;
    while ( (env=(char**)utarray_next(&job->envv,env))) putenv(*env);

    /* restore/unblock default handlers so they're unblocked after exec */
    for(n=0; n < sizeof(sigs)/sizeof(*sigs); n++) signal(sigs[n], SIG_DFL);
    sigset_t none;
    sigemptyset(&none);
    sigprocmask(SIG_SETMASK,&none,NULL);

    /* at last. we're ready to run the child process */
    if ((job->uid != -1) && (geteuid() != job->uid)) {
      if (setuid(job->uid) == -1) {
        syslog(LOG_ERR,"setuid failed: %s\n", strerror(errno));
        goto fail;
      }
    }
    argv = (char**)utarray_front(&job->cmdv);
    pathname = *argv;
    if (execv(pathname, argv) == -1) {
      syslog(LOG_ERR,"exec failed: %s\n", strerror(errno)); goto fail;
    }
    assert(0); /* not reached - child has exec'd */

   fail:
    exit(-1);  /* child exit on failure to exec */
  }
}

int term_job(job_t *job) {
  int es,rc=-1;

  UT_string *s;
  utstring_new(s);
  utstring_printf(s,"job %s [%d]: ", job->name, (int)job->pid);

  if (job->pid == 0) return;  /* not running */
  kill(job->pid, SIGTERM);
  if (waitpid(job->pid, &es, WNOHANG) == job->pid) job->pid = 0;
  else { /* child didn't exit. give it a moment, then force quit. */
    sleep(1);
    kill(job->pid, SIGKILL);
    if (waitpid(job->pid, &es, WNOHANG) == job->pid) job->pid = 0;
    else {
      utstring_printf(s, "failed to terminate");
      goto done;
    }
  }
  rc = 0; /* success */
  if (WIFSIGNALED(es)) utstring_printf(s,"exited on signal %d", (int)WTERMSIG(es));
  else if (WIFEXITED(es)) utstring_printf(s,"exit status %d", (int)WEXITSTATUS(es));

 done:
  syslog(rc==-1?LOG_ERR:LOG_INFO, "%s", utstring_body(s));
  utstring_free(s);
  return rc;
}

void term_jobs(UT_array *jobs) {
  job_t *job = NULL;
  while ( (job = (job_t*)utarray_next(jobs,job))) term_job(job);
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
  /* usr (username) */
  if ((!a->usr && b->usr) || (a->usr && !b->usr) ) return a->usr-b->usr;
  if ((a->usr && b->usr) && (rc = strcmp(a->usr,b->usr))) return rc;

  if (a->uid != b->uid) return a->uid - b->uid;
  if (a->order != b->order) return a->order - b->order;
  if (a->disabled != b->disabled) return a->disabled - b->disabled;
  if (a->wait != b->wait) return a->wait - b->wait;
  if (a->once != b->once) return a->once - b->once;
  return 0;
}

