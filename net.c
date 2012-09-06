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

/* addr is like "udp://127.0.0.1:3333".
 * only one listener can be set up currently.
 * we set up a UDP socket file descriptor bound to the port,
 * during the main loop we get SIGIO on incoming datagram 
*/
void set_listen(parse_t *ps, char *addr) { 
  in_addr_t local_ip;
  int rc = -1, port;

  if (parse_spec(ps->cfg, ps->em, addr, &local_ip, &port)) goto done;
  if (ps->cfg->test_only) return;  /* syntax looked ok */

  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == -1) {rc = -2; goto done;}

  /* specify the local address and port  */
  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = local_ip;
  sin.sin_port = htons(port);

  /* bind our socket to it */
  if (bind(fd, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
    close(fd);
    rc = -3; 
    goto done;
  }

  /* enable SIGIO to our pid whenever it becomes readable */
  int fl = fcntl(fd, F_GETFL);
  fl |= O_ASYNC | O_NONBLOCK;
  fcntl(fd, F_SETFL, fl);
  fcntl(fd, F_SETOWN, getpid()); 

  /* success */
  utarray_push_back(ps->cfg->listen, &fd);
  rc = 0;

 done:
  if (rc == -1) { /* ps->em already set */ }
  if (rc == -2) utstring_printf(ps->em,"can't open file descriptor");
  if (rc == -3) utstring_printf(ps->em,"can't bind to %s", addr);
  if (rc < 0) {
    utstring_printf(ps->em, " at line %d", ps->line);
    ps->rc = -1;
  }
}

/* report destination is like "udp://machine.org:3333". 
   there can be multiple instances of report destinations.
   we set up a UDP socket file descriptor 'connected' to the
   destination so that events can be sent to it at runtime
*/
void set_report(parse_t *ps, char *dest) { 
  in_addr_t dest_ip;
  int rc = -1, port;

  if (parse_spec(ps->cfg, ps->em, dest, &dest_ip, &port)) goto done;
  if (ps->cfg->test_only) return;  /* syntax looked ok */

  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == -1) {rc = -2; goto done;}

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
  char *job, *sp, *eob = &buf[n];
  enum {err, enable, disable} mode;

  if (cfg->verbose) syslog(LOG_DEBUG, "received [%.*s]", (int)n, buf);

 next:
  mode=err;
  if (n > 7 && !memcmp(buf,"enable ",7)) {mode=enable; job=&buf[7];}
  if (n > 8 && !memcmp(buf,"disable ",8)) {mode=disable; job=&buf[8];}
  if (mode == err) goto done;

  /* iterate over jobs named on this line. TODO multi-line */
  while (eob > job) {
    /* find space or end-of-buffer (EOB) that delimits the job */
    while((eob > job) && (*job == ' ')) job++;
    char *sp = job+1;
    while ((eob > sp) && (*sp != ' ')) sp++;
    *sp = '\0'; /*switch space or EOB to null (EOB write ok, we overallocated */
    job_t *j = get_job_by_name(cfg->jobs, job);
    job = sp+1;
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
        break;
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
  utarray_clear(cfg->listen);
  fd=NULL; while( (fd=(int*)utarray_next(cfg->report,fd))) close(*fd);
  utarray_clear(cfg->report);
}

/* report to all configured destinations */
void report_status(pmtr_t *cfg) {
  /* construct msg */
  utstring_clear(cfg->s);
  job_t *j = NULL;
  while ( (j=(job_t*)utarray_next(cfg->jobs,j))) {
    utstring_printf(cfg->s, "%s %c %u\n", j->name, j->disabled?'d':'e', 
                    (unsigned)j->start_ts);
  }

  /* send to all dests */
  int *fd=NULL;
  while ( (fd=(int*)utarray_next(cfg->report,fd))) {
    write(*fd,utstring_body(cfg->s),utstring_len(cfg->s));
  }
}
