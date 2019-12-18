#ifndef _JOB_H_
#define _JOB_H_

#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>

#include "pmtr.h"
#include "utstring.h"
#include "utarray.h"

#define adim(x) (sizeof(x)/sizeof(*x))

/* exit status that a job can use to indicate it does not want to be respawned */
#define PMTR_NO_RESTART 33
#define PMTR_MAX_USER 100

/* signals that we'll allow (unblock) during sigsuspend */
static const int sigs[] = {SIGHUP,SIGCHLD,SIGTERM,SIGINT,SIGQUIT,
                           SIGALRM,SIGUSR1,SIGIO};

#define S(x) #x, x
static struct rlimit_label { 
 char *flag;
 char *name;  
 int  id; 
} rlimit_labels[] =  {
              { "-c", S(RLIMIT_CORE)       },
              { "-d", S(RLIMIT_DATA)       },
              { "-e", S(RLIMIT_NICE)       },
              { "-f", S(RLIMIT_FSIZE)      },
              { "-i", S(RLIMIT_SIGPENDING) },
              { "-l", S(RLIMIT_MEMLOCK)    },
              { "-m", S(RLIMIT_RSS)        },
              { "-n", S(RLIMIT_NOFILE)     },
              { "-q", S(RLIMIT_MSGQUEUE)   },
              { "-r", S(RLIMIT_RTPRIO)     },
              { "-s", S(RLIMIT_STACK)      },
              { "-t", S(RLIMIT_CPU)        },
              { "-u", S(RLIMIT_NPROC)      },
              { "-v", S(RLIMIT_AS)         },
};

/* wrap struct rlimit with our own struct to track which resource it applies to. */
typedef struct {
  int id;
  struct rlimit rlim;  
} resource_rlimit_t;

static const UT_icd rlimit_icd={.sz=sizeof(resource_rlimit_t)};

typedef struct {
  char *name;
  UT_array cmdv; // cmd and args
  UT_array envv; // environment variables
  UT_array depv; // monitored dependencies
  UT_array rlim; // resource ulimits
  int deps_hash;
  char *dir;
  char *out;
  char *err;
  char *in;
  pid_t pid;
  time_t start_ts; /* last start time */
  time_t start_at; /* desired next start - used to slow restarts if cycling */
  time_t terminate;/* non-zero if termination requested due to disabling */
  char user[PMTR_MAX_USER];
  int respawn;
  int delete_when_collected;
  int order;
  int nice;
  int disabled;
  int wait;
  int once;
  int bounce_interval;
  cpu_set_t cpuset;
  /* remember to edit job_cmp in job.c if equality definition needs updating */
} job_t;

typedef struct {
  int line;
  int rc;
  UT_string *em;
  job_t *job; /* scratch space */
  pmtr_t *cfg; /* the global ptmr config */
} parse_t;

extern const UT_icd job_mm;

/* prototypes */
int parse_jobs(pmtr_t *cfg, UT_string *em);
void do_jobs(pmtr_t *cfg);
void term_jobs(UT_array *jobs);
int term_job(job_t *job);
void push_job(parse_t *ps);
job_t *get_job_by_pid(UT_array *jobs, pid_t pid);
job_t *get_job_by_name(UT_array *jobs, char *name);
int job_cmp(job_t *a, job_t *b);
void job_fin(job_t *job);
void job_cpy(job_t *dst, const job_t *src);
void collect_jobs(pmtr_t *cfg, UT_string *sm);
void set_name(parse_t *ps, char *name);
void set_ulimit(parse_t *ps, char *rname, char *value_a);
void set_bounce(parse_t *ps, char *timespec);
char *unquote(char *str);
void alarm_within(pmtr_t *cfg, int sec);
int get_tok(char *c_orig, char **c, size_t *bsz, size_t *toksz, int *line);
void set_dir(parse_t *ps, char *s);
void set_out(parse_t *ps, char *s);
void set_in(parse_t *ps, char *s);
void set_err(parse_t *ps, char *s);
void set_user(parse_t *ps, char *s);
void set_ord(parse_t *ps, char *s);
void set_nice(parse_t *ps, char *s);
void set_env(parse_t *ps, char *s);
void set_dis(parse_t *ps);
void set_wait(parse_t *ps);
void set_once(parse_t *ps);
void set_cmd(parse_t *ps, char *s);
void set_cpu(parse_t *ps, char *s);
char *fpath(job_t *job, char *file);
pid_t dep_monitor(char *file);
int instantiate_cfg_file(pmtr_t *cfg);


#endif /* _JOB_H_ */
