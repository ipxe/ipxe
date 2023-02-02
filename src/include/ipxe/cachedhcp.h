#ifndef _IPXE_CACHEDHCP_H
#define _IPXE_CACHEDHCP_H

/** @file
 *
 * Cached DHCP packet
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stddef.h>
#include <ipxe/uaccess.h>

struct cached_dhcp_packet;

extern struct cached_dhcp_packet cached_dhcpack;
extern struct cached_dhcp_packet cached_proxydhcp;
extern struct cached_dhcp_packet cached_pxebs;

extern int cachedhcp_record ( struct cached_dhcp_packet *cache,
			      unsigned int vlan, userptr_t data,
			      size_t max_len );

#endif /* _IPXE_CACHEDHCP_H */
