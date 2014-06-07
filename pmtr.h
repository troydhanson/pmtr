#ifndef _PMTR_H_
#define _PMTR_H_

#include <time.h>
#include "utarray.h"
#include "utstring.h"

#define DEFAULT_PM_CONFIG "/etc/pmtr.conf" 
#define SHORT_DELAY 10
#define PMTR_VERSION "1.5"

typedef struct {
  char *file;
  int verbose;
  int foreground;
  int test_only;
  pid_t dm_pid;       /* pid of dependency monitor sub process */
  UT_array *jobs;
  time_t next_alarm;
  /* the next two fields are for the UDP control and reporting features */
  UT_array *listen; /* file descriptors listening on */
  UT_array *report; /* file descriptors reporting to */
  UT_string *s;     /* scratch space */

} pmtr_t;

#endif /* _PMTR_H_ */
