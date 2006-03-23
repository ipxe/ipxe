#ifndef PROTO_H
#define PROTO_H

#include "tables.h"
#include "buffer.h"
#include <gpxe/in.h>

struct protocol {
	char *name;
	in_port_t default_port;
	int ( * load ) ( char *url, struct sockaddr_in *server, char *file,
			 struct buffer *buffer );
};

/*
 * Protocols that should be used if no explicit protocol is specified
 * (i.e. tftp) should use __default_protocol; all other protocols
 * should use __protocol.
 *
 */
#define __protocol_start		__table_start(protocol)
#define __protocol			__table(protocol,01)
#define __default_protocol_start	__table(protocol,02)
#define __default_protocol		__table(protocol,03)
#define __protocol_end			__table_end(protocol)

/*
 * Functions in proto.c
 *
 */
extern struct protocol * identify_protocol ( const char *name );

#endif /* PROTO_H */
