#ifndef _JOB_H_
#define _JOB_H_

#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#include "utstring.h"
#include "utarray.h"

/* exit status that a job can use to indicate it does not want to be respawned */
#define PM_NO_RESTART 33

/* signals that we'll allow (unblock) during sigsuspend */
static const int sigs[] = {SIGHUP,SIGCHLD,SIGTERM,SIGINT,SIGQUIT,SIGALRM,SIGUSR1};

typedef struct {
  char *name;
  UT_array cmdv;
  UT_array envv;
  char *dir;
  char *out;
  char *err;
  pid_t pid;
  time_t start_ts;
  char *usr;
  uid_t uid;
  int respawn;
  int order;
  int disabled;
  int wait;
  int once;
  /* remember to edit job_cmp in job.c if equality definition needs updating */
} job_t;

typedef struct {
  int line;
  char *file;
  int rc;
  UT_string *em;
  job_t *job; /* scratch space */
  UT_array *jobs;
} parse_t;

extern const UT_icd job_mm;

/* prototypes */
int parse_jobs(UT_array *jobs, UT_string *em, char *file, int verbose);
void do_jobs(UT_array *jobs);
void term_jobs(UT_array *jobs);
int term_job(job_t *job);
void push_job(parse_t *ps);
job_t *get_job_by_pid(UT_array *jobs, pid_t pid);
job_t *get_job_by_name(UT_array *jobs, char *name);
int job_cmp(job_t *a, job_t *b);
void set_name(parse_t *ps, char *name);
char *unquote(char *str);


#endif /* _JOB_H_ */
