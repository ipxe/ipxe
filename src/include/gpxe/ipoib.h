#ifndef _GPXE_IPOIB_H
#define _GPXE_IPOIB_H

/** @file
 *
 * IP over Infiniband
 */

#include <gpxe/infiniband.h>

/** IPoIB packet length */
#define IPOIB_PKT_LEN 2048

/** IPoIB MAC address length */
#define IPOIB_ALEN 20

/** An IPoIB MAC address */
struct ipoib_mac {
	/** Queue pair number
	 *
	 * MSB must be zero; QPNs are only 24-bit.
	 */
	uint32_t qpn;
	/** Port GID */
	struct ib_gid gid;
} __attribute__ (( packed ));

/** IPoIB link-layer header length */
#define IPOIB_HLEN 4

/** IPoIB link-layer header */
struct ipoib_hdr {
	/** Network-layer protocol */
	uint16_t proto;
	/** Reserved, must be zero */
	union {
		/** Reserved, must be zero */
		uint16_t reserved;
		/** Peer addresses
		 *
		 * We use these fields internally to represent the
		 * peer addresses using a lookup key.  There simply
		 * isn't enough room in the IPoIB header to store
		 * literal source or destination MAC addresses.
		 */
		struct {
			/** Destination address key */
			uint8_t dest;
			/** Source address key */
			uint8_t src;
		} __attribute__ (( packed )) peer;
	} __attribute__ (( packed )) u;
} __attribute__ (( packed ));

extern struct ll_protocol ipoib_protocol;

extern const char * ipoib_ntoa ( const void *ll_addr );

/**
 * Allocate IPoIB device
 *
 * @v priv_size		Size of driver private data
 * @ret netdev		Network device, or NULL
 */
static inline struct net_device * alloc_ipoibdev ( size_t priv_size ) {
	struct net_device *netdev;

	netdev = alloc_netdev ( priv_size );
	if ( netdev ) {
		netdev->ll_protocol = &ipoib_protocol;
		netdev->max_pkt_len = IPOIB_PKT_LEN;
	}
	return netdev;
}

extern void ipoib_link_state_changed ( struct ib_device *ibdev );
extern int ipoib_probe ( struct ib_device *ibdev );
extern void ipoib_remove ( struct ib_device *ibdev );

#endif /* _GPXE_IPOIB_H */
