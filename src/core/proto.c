#include "stddef.h"
#include "string.h"
#include "proto.h"

static struct protocol protocols[0] __protocol_start;
static struct protocol default_protocols[0] __default_protocol_start;
static struct protocol protocols_end[0] __protocol_end;

/*
 * Identify protocol given a name.  name may be NULL, in which case
 * the first default protocol (if any) will be used.
 *
 */
struct protocol * identify_protocol ( const char *name ) {
	struct protocol *proto = default_protocols;

	if ( name ) {
		for ( proto = protocols ; proto < protocols_end ; proto++ ) {
			if ( strcmp ( name, proto->name ) == 0 )
				break;
		}
	}

	return proto < protocols_end ? proto : NULL;
}
