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

/**
 * Allocate Ethernet device
 *
 * @v priv_size		Size of driver private data
 * @ret netdev		Network device, or NULL
 */
#define alloc_etherdev( priv_size ) ( {				\
	struct net_device *netdev;				\
	netdev = alloc_netdev ( priv_size );			\
	if ( netdev )						\
		netdev->ll_protocol = &ethernet_protocol;	\
	netdev;	} )

#endif /* _GPXE_ETHERNET_H */
