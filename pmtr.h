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
  UT_array *rpt_dsts;  /* event report destinations */
  char *listen;        /* local listener eg udp://1.1.1.1:2222 */
  int listen_fd;       /* file descriptor to listening udp socket */
} pmtr_t;

#endif 
