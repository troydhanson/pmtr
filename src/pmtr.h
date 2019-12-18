#ifndef _PMTR_H_
#define _PMTR_H_

#define _GNU_SOURCE /* To get struct ucred definition from <sys/sockets.h> */

#include <sys/inotify.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <limits.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sched.h>
#include <time.h>
#include "utstring.h"
#include "utarray.h"

/* pmtr.conf is expected in /etc by default. This expectation can be overridden
 * at build time using ./configure --sysconfdir=/dir. The end user can also tell
 * pmtr to look for its config file elsewhere using command line options. */
#ifndef CFGDIR
#define CFGDIR "/etc"
#endif
#define DEFAULT_PMTR_CONFIG CFGDIR "/pmtr.conf"
#define SHORT_DELAY 10

static struct timespec halfsec = {.tv_sec =  0, .tv_nsec = 500000000};

typedef struct {
  char *file;
  char *pidfile;
  int verbose;
  int foreground;
  int test_only;
  int echo_syslog_to_stderr;
  pid_t dm_pid;        /* pid of dependency monitor sub process */
  UT_array *jobs;
  time_t next_alarm;
  UT_array *listen;    /* UDP listening descriptors */
  UT_array *report;    /* UDP sending descriptors */
  char report_id[100]; /* our identity in report */
  UT_string *s;        /* scratch space */
  union {              /* buffer for inotify event reads */
    struct inotify_event ev;
    char buf[sizeof(struct inotify_event) + PATH_MAX];
  } eb;
  pid_t logger_pid;       /* pid of logger sub process */
  int logger_fd;          /* listening socket descriptor */
  char logger_socket[10]; /* listening socket name (abstract, not C string!) */
  int logger_namelen;     /* listening socket name length in bytes */

} pmtr_t;


#endif /* _PMTR_H_ */
