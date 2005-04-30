#ifndef RESOLV_H
#define RESOLV_H

#include "in.h"
#include "tables.h"

struct resolver {
	const char *name;
	int ( * resolv ) ( struct in_addr *address, const char *name );
};

#define __resolver __table(resolver,01)

extern int resolv ( struct in_addr *address, const char *name );

#endif /* RESOLV_H */
