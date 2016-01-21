#ifndef _PROCTAB_H_
#define _PROCTAB_H_

#include <time.h>
#include "utarray.h"
#include "utstring.h"

/* proctab.conf is expected in /etc by default. This expectation can be overridden
 * at build time using ./configure --sysconfdir=/dir. The end user can also tell
 * proctab to look for its config file elsewhere using command line options. */
#ifndef CFGDIR
#define CFGDIR "/etc"
#endif
#define DEFAULT_PROCTAB_CONFIG CFGDIR "/proctab.conf"
#define SHORT_DELAY 10
#define PROCTAB_VERSION "1.7"

typedef struct {
  char *file;
  char *pidfile;
  int verbose;
  int foreground;
  int test_only;
  pid_t dm_pid;       /* pid of dependency monitor sub process */
  UT_array *jobs;
  time_t next_alarm;
  /* the next two fields are for the UDP control and reporting features */
  UT_array *listen; /* file descriptors listening on */
  UT_array *report; /* file descriptors reporting to */
  char report_id[100]; /* our identity in report */
  UT_string *s;     /* scratch space */

} proctab_t;

#endif /* _PROCTAB_H_ */
