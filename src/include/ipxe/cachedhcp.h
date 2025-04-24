#ifndef _IPXE_CACHEDHCP_H
#define _IPXE_CACHEDHCP_H

/** @file
 *
 * Cached DHCP packet
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stddef.h>

struct net_device;
struct cached_dhcp_packet;

extern struct cached_dhcp_packet cached_dhcpack;
extern struct cached_dhcp_packet cached_proxydhcp;
extern struct cached_dhcp_packet cached_pxebs;

extern int cachedhcp_record ( struct cached_dhcp_packet *cache,
			      unsigned int vlan, const void *data,
			      size_t max_len );
extern void cachedhcp_recycle ( struct net_device *netdev );

#endif /* _IPXE_CACHEDHCP_H */
