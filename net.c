#include <unistd.h>
#include <assert.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <errno.h>

#include <ifaddrs.h>
#include <net/if.h>

#include <string.h>
#include "utarray.h"
#include "net.h"

static int parse_spec(pmtr_t *cfg, UT_string *em, char *spec, 
                      in_addr_t *addr, int *port) {
  char *proto = spec, *colon, *host;
  struct hostent *h;
  int hlen, rc=-1;

  if (strncmp(proto, "udp://", 6)) goto done;
  host = &spec[6];

  if ( !(colon = strrchr(spec, ':'))) goto done;
  *port = atoi(colon+1);
  if ((*port < 0) || (*port > 65535)) goto done;

  /* stop here if syntax checking */
  if (cfg->test_only) return 0; 

  /* dns lookup. */
  *colon = '\0'; 
  h = gethostbyname(host); 
  hlen = strlen(host);
  *colon = ':';
  if (!h) {
    utstring_printf(em, "lookup [%.*s]: %s", hlen, host, hstrerror(h_errno));
    rc = -2;
    goto done;
  }

  *addr = ((struct in_addr*)h->h_addr)->s_addr;
  rc = 0; /* success */

 done:
  if (rc == -1) utstring_printf(em,"expected format is udp://1.2.3.4:5678");
  return rc;
}

void set_ident(pmtr_t *cfg, in_addr_t ip, int port) {
     utstring_clear(cfg->ident);
     struct in_addr addr = {.s_addr=ip};
     char host[100];
     if (gethostname(host,sizeof(host))<0) strncpy(host,"unknown",sizeof(host));
     else host[sizeof(host)-1] = '\0'; /* ensure termination on truncate */
     utstring_printf(cfg->ident, "report %s %u %s\n", inet_ntoa(addr), port, host);
}

/* addr is like "udp://127.0.0.1:3333".
 * only one listener can be set up currently.
 * we set up a UDP socket file descriptor bound to the port,
 * during the main loop we get SIGIO on incoming datagram 
*/
void set_listen(parse_t *ps, char *addr) { 
  in_addr_t local_ip;
  int rc = -1, port, flags;

  if (parse_spec(ps->cfg, ps->em, addr, &local_ip, &port)) goto done;
  if (ps->cfg->test_only) return;  /* syntax looked ok */

  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == -1) {rc = -2; goto done;}

  /* set close-on-exec flag for the descriptor so our jobs don't inherit it */
  flags = fcntl(fd, F_GETFD);
  flags |= FD_CLOEXEC;
  if (fcntl(fd, F_SETFD, flags) == -1) {rc = -3; goto done;}

  /* specify the local address and port  */
  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = local_ip;
  sin.sin_port = htons(port);

  /* bind our socket to it */
  if (bind(fd, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
    close(fd);
    rc = -4;
    utstring_printf(ps->em,"can't bind to %s: %s", addr, strerror(errno));
    goto done;
  }

  /* enable SIGIO to our pid whenever it becomes readable */
  int fl = fcntl(fd, F_GETFL);
  fl |= O_ASYNC | O_NONBLOCK;
  fcntl(fd, F_SETFL, fl);
  fcntl(fd, F_SETOWN, getpid()); 

  /* setup identity to include in report */
  set_ident(ps->cfg, local_ip, port);

  /* success */
  utarray_push_back(ps->cfg->listen, &fd);
  rc = 0;

 done:
  if (rc == -1) utstring_printf(ps->em, " (at line %d)", ps->line); 
  if (rc == -2) utstring_printf(ps->em,"can't open file descriptor");
  if (rc == -3) utstring_printf(ps->em,"can't set close-on-exec");
  if (rc == -4) { /* ps->em already set */ }
  if (rc < 0) ps->rc = -1;
}

/* report destination is like "udp://machine.org:3333". 
   there can be multiple instances of report destinations.
   we set up a UDP socket file descriptor 'connected' to the
   destination so that events can be sent to it at runtime
*/
void set_report(parse_t *ps, char *dest) { 
  int rc = -1, port, flags;
  in_addr_t dest_ip;

  if (parse_spec(ps->cfg, ps->em, dest, &dest_ip, &port)) goto done;
  if (ps->cfg->test_only) return;  /* syntax looked ok */

  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == -1) {rc = -2; goto done;}

  /* set close-on-exec flag for the descriptor so our jobs don't inherit it */
  flags = fcntl(fd, F_GETFD);
  flags |= FD_CLOEXEC;
  if (fcntl(fd, F_SETFD, flags) == -1) {rc = -3; goto done;}

  /* specify the local address and port  */
  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = dest_ip;
  sin.sin_port = htons(port);

  if (connect(fd, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
    utstring_printf(ps->em, "can't connect to %s: %s", dest, strerror(errno));
    rc = -3;
    goto done;
  }

  /* success */
  utarray_push_back(ps->cfg->report, &fd);
  rc = 0;

 done:
  if (rc == -1) { /* ps->em already set */ }
  if (rc == -2) utstring_printf(ps->em,"can't open file descriptor");
  if (rc == -3) { /* ps->em already set */ }
  if (rc < 0) {
    utstring_printf(ps->em, " at line %d", ps->line);
    ps->rc = -1;
  }
}

/* decode datagram that we received.
 *
 * enable job1 [job2 ...] 
 * disable job1 [job2 ...]
*/
static void decode_msg(pmtr_t *cfg, char *buf, size_t n) {
  char *pos=buf, *job, *eob = &buf[n];
  enum {err, enable, disable} mode=err;
  job_t *j;

  if (cfg->verbose) syslog(LOG_DEBUG, "received [%.*s]", (int)n, buf);

  /* iterate over jobs named on this line. */
  while (eob > pos) {
    while((eob > pos) && (*pos == ' ')) pos++;
    char *sp = pos+1;
    while ((eob > sp) && (*sp != ' ')) sp++;
    *sp = '\0'; 
    if      (!strcmp(pos,"enable"))  { mode=enable;  pos = sp+1; continue; }
    else if (!strcmp(pos,"disable")) { mode=disable; pos = sp+1; continue; }
    if (mode == err) { syslog(LOG_ERR, "invalid control msg"); goto done; }
    job = pos;
    j = get_job_by_name(cfg->jobs, job);
    pos = sp+1;
    if (j == NULL) {
      syslog(LOG_INFO,"control msg for unknown job %s", job); /* ignore */
      continue;
    }
    switch(mode) {
      case enable:
        if (!j->disabled) break; /* no-op */
        syslog(LOG_INFO,"enabling %s", job);
        j->disabled=0;
        alarm_within(cfg,1); /* let SIGALRM handler start it soon */
        break;
      case disable:
        if (j->disabled) break; /* no-op */
        syslog(LOG_INFO,"disabling %s", job);
        j->disabled=1;
        if (j->pid) term_job(j);
        alarm_within(cfg,1); /* report soon if needed */
        break;
      default: assert(0); break;
    }
  }

 done:
  return;
  
}

#define BUF_SZ 2000
static char buf[BUF_SZ+1];
/* called when we have datagrams to read */
void service_socket(pmtr_t *cfg) {
  ssize_t rc;
  int *fd = (int*)utarray_front(cfg->listen); assert(fd);
  do {
    rc = read(*fd, buf, sizeof(buf));   /* fd is non-blocking, thus */
    if (rc > 0) decode_msg(cfg,buf,rc); /* we get rc==-1 after last */
  } while (rc >= 0);
}

void close_sockets(pmtr_t *cfg) {
  int *fd;
  fd=NULL; while( (fd=(int*)utarray_next(cfg->listen,fd))) close(*fd);
  fd=NULL; while( (fd=(int*)utarray_next(cfg->report,fd))) close(*fd);
  utarray_clear(cfg->listen);
  utarray_clear(cfg->report);
}

/* report to all configured destinations */
void report_status(pmtr_t *cfg) {
  int rc;

  /* construct msg */
  utstring_clear(cfg->s);
  if (utstring_len(cfg->ident)) utstring_concat(cfg->s, cfg->ident);
  else utstring_printf(cfg->ident, "report\n");
  job_t *j = NULL;
  while ( (j=(job_t*)utarray_next(cfg->jobs,j))) {
    utstring_printf(cfg->s, "%s %c %u\n", j->name, j->disabled?'d':'e', 
                    (unsigned)j->start_ts);
  }

  /* send to all dests */
  int *fd=NULL;
  while ( (fd=(int*)utarray_next(cfg->report,fd))) {
    rc = write(*fd,utstring_body(cfg->s),utstring_len(cfg->s));
    if (rc < 0) syslog(LOG_INFO,"write error: %s", strerror(errno));
    if (rc >= 0 && rc < utstring_len(cfg->s)) {
      syslog(LOG_INFO,"incomplete write %d/%d", rc, utstring_len(cfg->s));
    }
  }
}
