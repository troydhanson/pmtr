#ifndef _PMTR_H_
#define _PMTR_H_

#include <time.h>
#include "utarray.h"
#include "utstring.h"

typedef struct {
  char *file;
  int verbose;
  int foreground;
  int test_only;
  UT_array *jobs;
  time_t next_alarm;
  /* the next two fields are for the UDP control and reporting features */
  UT_array *listen; /* file descriptors listening on */
  UT_array *report; /* file descriptors reporting to */
  UT_string *s;     /* scratch space */
  UT_string *ident; /* e.g. "1.2.3.4 3456 host.xyz.net" to report */

} pmtr_t;

#endif /* _PMTR_H_ */
