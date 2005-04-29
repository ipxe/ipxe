#include "resolv.h"

static struct resolver resolvers[0] __table_start(resolver);
static struct resolver resolvers_end[0] __table_end(resolver);

/*
 * Resolve a name (which may be just a dotted quad IP address) to an
 * IP address.
 *
 */
int resolv ( struct in_addr *address, const char *name ) {
	struct resolver *resolver;

	/* Check for a dotted quad IP address first */
	if ( inet_aton ( name, address ) )
		return 1;

	/* Try any compiled-in name resolution modules */
	for ( resolver = resolvers ; resolver < resolvers_end ; resolver++ ) {
		if ( resolver->resolv ( address, name ) )
			return 1;
	}

	return 0;
}
