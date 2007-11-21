#ifndef _I386_UUID_H
#define _I386_UUID_H

#include <smbios.h>

static inline int get_uuid ( union uuid *uuid ) {
	return smbios_get_uuid ( uuid );
}

#endif /* _I386_UUID_H */
