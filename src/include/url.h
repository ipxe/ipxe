#ifndef URL_H
#define URL_H

#include "proto.h"
#include <gpxe/in.h>

extern int parse_url ( char *url, struct protocol **proto,
		       struct sockaddr_in *server, char **filename );

#endif /* URL_H */
