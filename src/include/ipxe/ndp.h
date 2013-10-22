#ifndef _IPXE_NDP_H
#define _IPXE_NDP_H

/** @file
 *
 * Neighbour discovery protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <ipxe/in.h>
#include <ipxe/ipv6.h>
#include <ipxe/icmpv6.h>
#include <ipxe/neighbour.h>

/** An NDP option */
struct ndp_option {
	/** Type */
	uint8_t type;
	/** Length (in blocks of 8 bytes) */
	uint8_t blocks;
	/** Value */
	uint8_t value[0];
} __attribute__ (( packed ));

/** NDP option block size */
#define NDP_OPTION_BLKSZ 8

/** An NDP neighbour solicitation or advertisement header */
struct ndp_neighbour_header {
	/** ICMPv6 header */
	struct icmp_header icmp;
	/** Flags */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved[3];
	/** Target address */
	struct in6_addr target;
	/** Options */
	struct ndp_option option[0];
} __attribute__ (( packed ));

/** NDP router flag */
#define NDP_NEIGHBOUR_ROUTER 0x80

/** NDP solicited flag */
#define NDP_NEIGHBOUR_SOLICITED 0x40

/** NDP override flag */
#define NDP_NEIGHBOUR_OVERRIDE 0x20

/** An NDP router advertisement header */
struct ndp_router_advertisement_header {
	/** ICMPv6 header */
	struct icmp_header icmp;
	/** Current hop limit */
	uint8_t hop_limit;
	/** Flags */
	uint8_t flags;
	/** Router lifetime */
	uint16_t lifetime;
	/** Reachable time */
	uint32_t reachable;
	/** Retransmission timer */
	uint32_t retransmit;
	/** Options */
	struct ndp_option option[0];
} __attribute__ (( packed ));

/** NDP managed address configuration */
#define NDP_ROUTER_MANAGED 0x80

/** NDP other configuration */
#define NDP_ROUTER_OTHER 0x40

/** An NDP header */
union ndp_header {
	/** ICMPv6 header */
	struct icmp_header icmp;
	/** Neighbour solicitation or advertisement header */
	struct ndp_neighbour_header neigh;
	/** Router advertisement header */
	struct ndp_router_advertisement_header radv;
} __attribute__ (( packed ));

/** NDP source link-layer address option */
#define NDP_OPT_LL_SOURCE 1

/** NDP target link-layer address option */
#define NDP_OPT_LL_TARGET 2

extern struct neighbour_discovery ndp_discovery;

/**
 * Transmit packet, determining link-layer address via NDP
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v net_dest		Destination network-layer address
 * @v net_source	Source network-layer address
 * @v ll_source		Source link-layer address
 * @ret rc		Return status code
 */
static inline int ndp_tx ( struct io_buffer *iobuf, struct net_device *netdev,
			   const void *net_dest, const void *net_source,
			   const void *ll_source ) {

	return neighbour_tx ( iobuf, netdev, &ipv6_protocol, net_dest,
			      &ndp_discovery, net_source, ll_source );
}

#endif /* _IPXE_NDP_H */
