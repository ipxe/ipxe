#ifndef _IPXE_LLDP_H
#define _IPXE_LLDP_H

/** @file
 *
 * Link Layer Discovery Protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <byteswap.h>

#define LLDP_SETTINGS_NAME "lldp"

extern struct net_protocol lldp_protocol __net_protocol;

/** LLDP setting scopes */
extern const struct settings_scope lldp_settings_scope;
extern const struct settings_scope lldp_special_scope;

extern const struct setting_type lldp_type_unknown;

struct lldp_settings {
	struct refcnt refcnt;
	struct settings settings;

	size_t size;
	char data[0];
};


#define LLDP_TYPE(type_size) ( (type_size) >> 9 )
#define LLDP_SIZE(type_size) ( (type_size) & 0x01FF )

#define LLDP_TAG_TYPE(tag) ( (tag) & 0x000000FF )
#define LLDP_TAG_COUNT(tag) ( ( (tag) & 0x0000FF00 ) >> 8 )
#define LLDP_TAG_LEN(tag) ( ( (tag) & 0x00FF0000 ) >> 16 )
#define LLDP_TAG_OFS(tag) ( (tag) >> 24 )

#define LLDP_TAG_SET_OFS(tag, ofs) ( ( (ofs) << 24 ) | (tag) )

/** Parse an LLDP header and returns a pointer to the start of the payload */
static inline void * lldp_parse ( void *data, uint8_t *type, uint16_t *size ) {
	*size  = htons ( * (uint16_t *) data );
	*type  = LLDP_TYPE ( *size );
	*size  = LLDP_SIZE ( *size );

	return (uint16_t *) data + 1;
}

#endif /* _IPXE_LLDP_H */
