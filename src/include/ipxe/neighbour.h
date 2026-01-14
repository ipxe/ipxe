#ifndef _IPXE_NEIGHBOUR_H
#define _IPXE_NEIGHBOUR_H

/** @file
 *
 * Neighbour discovery
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <ipxe/refcnt.h>
#include <ipxe/list.h>
#include <ipxe/netdevice.h>
#include <ipxe/retry.h>

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

/** A neighbour cache entry */
struct neighbour {
	/** Reference count */
	struct refcnt refcnt;
	/** List of neighbour cache entries */
	struct list_head list;

	/** Network device */
	struct net_device *netdev;
	/** Network-layer protocol */
	struct net_protocol *net_protocol;
	/** Network-layer destination address */
	uint8_t net_dest[MAX_NET_ADDR_LEN];
	/** Link-layer destination address */
	uint8_t ll_dest[MAX_LL_ADDR_LEN];

	/** Neighbour discovery protocol (if discovery is ongoing) */
	struct neighbour_discovery *discovery;
	/** Network-layer source address (for discovery requests) */
	uint8_t net_source[MAX_NET_ADDR_LEN];
	/** Retransmission timer */
	struct retry_timer timer;

	/** Pending I/O buffers */
	struct list_head tx_queue;
};

/** A neighbour transmission delay pseudo-header */
struct neighbour_delay {
	/** Original transmission time (in ticks) */
	unsigned long start;
};

extern struct list_head neighbours;

extern int neighbour_tx ( struct io_buffer *iobuf, struct net_device *netdev,
			  struct net_protocol *net_protocol,
			  const void *net_dest,
			  struct neighbour_discovery *discovery,
			  const void *net_source );
extern int neighbour_update ( struct net_device *netdev,
			      struct net_protocol *net_protocol,
			      const void *net_dest, const void *ll_dest );
extern int neighbour_define ( struct net_device *netdev,
			      struct net_protocol *net_protocol,
			      const void *net_dest, const void *ll_dest );

#endif /* _IPXE_NEIGHBOUR_H */
