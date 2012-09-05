#include <unistd.h>
#include <assert.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include "utarray.h"
#include "net.h"

/* listen_spec is like "udp://127.0.0.1:3333".
 * only one listener can be set up currently.
 * we set up a UDP socket file descriptor bound to the port,
 * during the main loop we get SIGIO on incoming datagram 
*/
void set_listen(parse_t *ps, char *listen_spec) { 
  int rc = -1;
  char *proto = listen_spec, *colon, *port_str, *ip_str;
  if (strncmp(proto, "udp://", 6)) goto done;
  if ( !(colon = strrchr(listen_spec, ':'))) goto done;
  ip_str = &listen_spec[6];
  port_str = colon+1;

  /* convert the local IP address to in_addr_t */
  *colon = '\0'; /* temporary for inet_addr */
  in_addr_t local_ip = inet_addr(ip_str);
  *colon = ':';
  if (local_ip == INADDR_NONE) goto done;
  /* convert the port to number */
  int port = atoi(port_str);
  if (port < 0 || port > 65535) goto done;

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
  if (rc == -1) utstring_printf(ps->em,"expected format is udp://1.2.3.4:5678");
  if (rc == -2) utstring_printf(ps->em,"can't open file descriptor");
  if (rc == -3) utstring_printf(ps->em,"can't bind to %s", listen_spec);
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
void set_report(parse_t *ps, char *report_spec) { 
}

/* decode datagram that we received.
 *
 * enable job1 [job2 ...] 
 * disable job1 [job2 ...]
*/
static void decode_msg(pmtr_t *cfg, char *buf, size_t n) {
  char *job, *sp, *eob = &buf[n];
  enum {err, enable, disable} mode = err;

  if (cfg->verbose) syslog(LOG_DEBUG, "received [%.*s]", n, buf);

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
    if (j) {
      switch(mode) {
        case enable:
          if (j->disabled==0) break; /* already enabled? */
          syslog(LOG_INFO,"control socket: enabling %s", job);
          j->disabled=0;             /* ok, enable it */
          if (!cfg->alarm_pending) {
            cfg->alarm_pending++;  /* let our SIGALRM handler start it soon */
            alarm(1);
          }
          break;
        case disable:
          if (j->disabled) break;
          syslog(LOG_INFO,"control socket: disabling %s", job);
          j->disabled=1;
          if (j->pid) term_job(j);
          break;
      }
    } else {
      syslog(LOG_INFO,"control error, unknown job %s", job); /* ignore */
    }

    job = sp+1;
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
