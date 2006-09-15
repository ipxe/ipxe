#ifndef _GPXE_ETHERNET_H
#define _GPXE_ETHERNET_H

/** @file
 *
 * Ethernet protocol
 *
 */

#include <stdint.h>
#include <gpxe/netdevice.h>

extern struct ll_protocol ethernet_protocol;

extern const char * eth_ntoa ( const void *ll_addr );

/**
 * Allocate Ethernet device
 *
 * @v priv_size		Size of driver private data
 * @ret netdev		Network device, or NULL
 */
static inline struct net_device * alloc_etherdev ( size_t priv_size ) {
	struct net_device *netdev;

	netdev = alloc_netdev ( priv_size );
	if ( netdev ) {
		netdev->ll_protocol = &ethernet_protocol;
	}
	return netdev;
}

#endif /* _GPXE_ETHERNET_H */
