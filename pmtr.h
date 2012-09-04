#ifndef _PMTR_H_
#define _PMTR_H_

#include "utarray.h"

typedef struct {
  char *file;
  int verbose;
  int foreground;
  int alarm_pending;
  int test_only;
  UT_array *jobs;
  /* the next two fields are for the UDP control and reporting features */
  UT_array *listen; /* file descriptors listening on */
  UT_array *report; /* file descriptors reporting to */

} pmtr_t;

#endif /* _PMTR_H_ */
