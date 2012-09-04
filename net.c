#include <unistd.h>
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

void close_sockets(pmtr_t *cfg) {
  int *fd;
  fd=NULL; while( (fd=(int*)utarray_next(cfg->listen,fd))) close(*fd);
  utarray_clear(cfg->listen);
  fd=NULL; while( (fd=(int*)utarray_next(cfg->report,fd))) close(*fd);
  utarray_clear(cfg->report);
}
