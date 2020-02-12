#include <unistd.h>
#include <assert.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
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
                      in_addr_t *addr, int *port, char **iface) {
  char *proto = spec, *colon, *host, *at;
  struct hostent *h;
  int hlen, rc=-1;
  if (iface) *iface = NULL;

  if (strncmp(proto, "udp://", 6)) goto done;
  host = &spec[6];

  if ( !(colon = strrchr(spec, ':'))) goto done;
  *port = atoi(colon+1);
  if ((*port < 0) || (*port > 65535)) goto done;

  if ( (at = strrchr(spec, '@')) != NULL) { // trailing @eth2
    if (iface) *iface = at+1;
  }

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
  if (rc == -1) utstring_printf(em,"required format: udp://1.2.3.4:5678[@eth2]");
  return rc;
}

/* addr is like "udp://127.0.0.1:3333".
 * only one listener can be set up currently.
 * we set up a UDP socket file descriptor bound to the port,
 * during the main loop we get SIGIO on incoming datagram 
*/
void set_listen(parse_t *ps, char *addr) { 
  in_addr_t local_ip;
  int rc = -1, port, flags;

  if (parse_spec(ps->cfg, ps->em, addr, &local_ip, &port, NULL)) goto done;
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
  char *iface;

  if (parse_spec(ps->cfg, ps->em, dest, &dest_ip, &port, &iface)) goto done;
  if (ps->cfg->test_only) return;  /* syntax looked ok */

  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == -1) {rc = -2; goto done;}

  /* set close-on-exec flag for the descriptor so our jobs don't inherit it */
  flags = fcntl(fd, F_GETFD);
  flags |= FD_CLOEXEC;
  if (fcntl(fd, F_SETFD, flags) == -1) {rc = -3; goto done;}

  gethostname(ps->cfg->report_id, sizeof(ps->cfg->report_id));

  /* use a specific NIC if one was specified, supported here for multicast */
  if (iface) {
    int l = strlen(iface);
    if (l+1 >IFNAMSIZ) {utstring_printf(ps->em,"interface too long\n"); goto done;}

    struct ifreq ifr;
    ifr.ifr_addr.sa_family = AF_INET;
    memcpy(ifr.ifr_name, iface, l+1);

    /* does this interface support multicast? */
    if (ioctl(fd, SIOCGIFFLAGS, &ifr)) {utstring_printf(ps->em,"ioctl: %s\n", strerror(errno)); goto done;} 
    if (!(ifr.ifr_flags & IFF_MULTICAST)) {utstring_printf(ps->em,"%s does not multicast\n",iface); goto done;}

    /* get the interface IP address */
    struct in_addr iface_addr;
    if (ioctl(fd, SIOCGIFADDR, &ifr)) {utstring_printf(ps->em,"ioctl: %s\n", strerror(errno)); goto done;} 
    iface_addr = (((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
    // utstring_printf(ps->em,"iface %s has addr %s\n", iface, inet_ntoa(iface_addr));
    strcat(ps->cfg->report_id, " ");
    strcat(ps->cfg->report_id, inet_ntoa(iface_addr));

    /* ask kernel to use its IP address for outgoing multicast */
    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &iface_addr, sizeof(iface_addr))) {
      utstring_printf(ps->em,"setsockopt: %s\n", strerror(errno));
      goto done;
    }
  }


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
        if (j->pid) { if (j->terminate==0) j->terminate=1; }
        alarm_within(cfg,1); /* report soon if needed */
        break;
      default: assert(0); break;
    }
    /* induce dependency monitor restart */
    if (cfg->dm_pid != -1) kill(cfg->dm_pid,SIGHUP);
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
  time_t now = time(NULL);

  /* construct msg */
  utstring_clear(cfg->s);
  utstring_printf(cfg->s, "report %s\n", cfg->report_id);
  job_t *j = NULL;
  while ( (j=(job_t*)utarray_next(cfg->jobs,j))) {
    if (j->respawn == 0) continue; /* don't advertise one-time jobs */
    utstring_printf(cfg->s, "%s %c %u %d %s\n", j->name, j->disabled?'d':'e',
                    (unsigned)(now - j->start_ts), (int)j->pid,
                    *((char**)utarray_front(&j->cmdv)));
  }

  /* send to all dests */
  int *fd=NULL;
  while ( (fd=(int*)utarray_next(cfg->report,fd))) {
    rc = write(*fd,utstring_body(cfg->s),utstring_len(cfg->s));
    if (rc < 0 && errno != ECONNREFUSED) 
      syslog(LOG_INFO,"write error: %s", strerror(errno));
    if (rc >= 0 && rc < utstring_len(cfg->s)) {
      syslog(LOG_INFO,"incomplete write %d/%d", rc, utstring_len(cfg->s));
    }
  }
}
