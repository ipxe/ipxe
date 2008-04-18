#ifndef _GPXE_IPOIB_H
#define _GPXE_IPOIB_H

/** @file
 *
 * IP over Infiniband
 */

#include <gpxe/infiniband.h>

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
#define IPOIB_HLEN 24

/**
 * IPoIB link-layer header pseudo portion
 *
 * This part doesn't actually exist on the wire, but it provides a
 * convenient way to fit into the typical network device model.
 */
struct ipoib_pseudo_hdr {
	/** Peer address */
	struct ipoib_mac peer;
} __attribute__ (( packed ));

/** IPoIB link-layer header real portion */
struct ipoib_real_hdr {
	/** Network-layer protocol */
	uint16_t proto;
	/** Reserved, must be zero */
	uint16_t reserved;
} __attribute__ (( packed ));

/** An IPoIB link-layer header */
struct ipoib_hdr {
	/** Pseudo portion */
	struct ipoib_pseudo_hdr pseudo;
	/** Real portion */
	struct ipoib_real_hdr real;
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
	}
	return netdev;
}

extern void ipoib_link_state_changed ( struct ib_device *ibdev );
extern int ipoib_probe ( struct ib_device *ibdev );
extern void ipoib_remove ( struct ib_device *ibdev );

#endif /* _GPXE_IPOIB_H */
