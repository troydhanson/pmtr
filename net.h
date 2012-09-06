#ifndef _NET_H_
#define _NET_H_

#include "job.h"

/* prototypes */
void set_listen(parse_t *ps, char *spec);
void set_report(parse_t *ps, char *spec);
void close_sockets(pmtr_t *cfg);
void service_socket(pmtr_t *cfg);
void report_status(pmtr_t *cfg);

#endif /* _NET_H_ */
