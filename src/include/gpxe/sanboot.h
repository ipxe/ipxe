#ifndef _GPXE_SANBOOT_H
#define _GPXE_SANBOOT_H

#include <gpxe/tables.h>

struct sanboot_protocol {
	const char *prefix;
	int ( * boot ) ( const char *root_path );
};

#define SANBOOT_PROTOCOLS \
	__table ( struct sanboot_protocol, "sanboot_protocols" )

#define __sanboot_protocol __table_entry ( SANBOOT_PROTOCOLS, 01 )

#endif /* _GPXE_SANBOOT_H */
