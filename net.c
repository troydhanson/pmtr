#include <string.h>
#include "net.h"

void set_listen(parse_t *ps, char *listen_spec) { 
  /* listen_spec is like "udp://127.0.0.1:3333" */
  enum {none, syntaxerr, protoerr, iperr, porterr} rc = none;
  //if (strncmp(proto,"udp://",6)) {rc = protoerr; goto done;}
  char *ip_str = &listen_spec[6];
  char *colon;
  //if ( (colon = strchr(ip_str,':')) == NULL) {rc = syntaxerr; goto done;}
  char *port_str = colon+1;
}

void set_report(parse_t *ps, char *report_spec) { 
}
