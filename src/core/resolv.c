#include "resolv.h"
#include "stdio.h"

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
	if ( inet_aton ( name, address ) ) {
		DBG ( "RESOLV saw valid IP address %s\n", name );
		return 1;
	}

	/* Try any compiled-in name resolution modules */
	for ( resolver = resolvers ; resolver < resolvers_end ; resolver++ ) {
		if ( resolver->resolv ( address, name ) ) {
			DBG ( "RESOLV resolved \"%s\" to %@ using %s\n",
			      name, address->s_addr, resolver->name );
			return 1;
		}
	}

	DBG ( "RESOLV failed to resolve %s\n", name );
	return 0;
}
