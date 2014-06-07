#ifndef _JOB_H_
#define _JOB_H_

#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#include "pmtr.h"
#include "utstring.h"
#include "utarray.h"

/* exit status that a job can use to indicate it does not want to be respawned */
#define PM_NO_RESTART 33

/* signals that we'll allow (unblock) during sigsuspend */
static const int sigs[] = {SIGHUP,SIGCHLD,SIGTERM,SIGINT,SIGQUIT,
                           SIGALRM,SIGUSR1,SIGIO};

typedef struct {
  char *name;
  UT_array cmdv;
  UT_array envv;
  char *dir;
  char *out;
  char *err;
  char *in;
  pid_t pid;
  time_t start_ts; /* last start time */
  time_t start_at; /* desired next start - used to slow restarts if cycling */
  time_t terminate;/* non-zero if termination requested due to disabling */
  uid_t uid;
  int respawn;
  int delete_when_collected;
  int order;
  int disabled;
  int wait;
  int once;
  int bounce_interval;
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
void set_name(parse_t *ps, char *name);
char *unquote(char *str);
void alarm_within(pmtr_t *cfg, int sec);
int get_tok(char *c_orig, char **c, size_t *bsz, size_t *toksz, int *line);
void set_dir(parse_t *ps, char *s);
void set_out(parse_t *ps, char *s);
void set_in(parse_t *ps, char *s);
void set_err(parse_t *ps, char *s);
void set_user(parse_t *ps, char *s);
void set_ord(parse_t *ps, char *s);
void set_env(parse_t *ps, char *s);
void set_dis(parse_t *ps);
void set_wait(parse_t *ps);
void set_once(parse_t *ps);
void set_cmd(parse_t *ps, char *s);



#endif /* _JOB_H_ */
