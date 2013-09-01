#ifndef _IPXE_NEIGHBOUR_H
#define _IPXE_NEIGHBOUR_H

/** @file
 *
 * Neighbour discovery
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <ipxe/netdevice.h>

/** A neighbour discovery protocol */
struct neighbour_discovery {
	/** Name */
	const char *name;
	/**
	 * Transmit neighbour discovery request
	 *
	 * @v netdev		Network device
	 * @v net_protocol	Network-layer protocol
	 * @v net_dest		Destination network-layer address
	 * @v net_source	Source network-layer address
	 * @ret rc		Return status code
	 */
	int ( * tx_request ) ( struct net_device *netdev,
			       struct net_protocol *net_protocol,
			       const void *net_dest, const void *net_source );
};

extern int neighbour_tx ( struct io_buffer *iobuf, struct net_device *netdev,
			  struct net_protocol *net_protocol,
			  const void *net_dest,
			  struct neighbour_discovery *discovery,
			  const void *net_source, const void *ll_source );
extern int neighbour_update ( struct net_device *netdev,
			      struct net_protocol *net_protocol,
			      const void *net_dest, const void *ll_dest );
extern int neighbour_define ( struct net_device *netdev,
			      struct net_protocol *net_protocol,
			      const void *net_dest, const void *ll_dest );

#endif /* _IPXE_NEIGHBOUR_H */
