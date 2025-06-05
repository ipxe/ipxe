#ifndef _IPXE_IP_H
#define _IPXE_IP_H

/** @file
 *
 * IP protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/in.h>
#include <ipxe/list.h>
#include <ipxe/retry.h>
#include <ipxe/netdevice.h>

struct io_buffer;

/* IP constants */

#define IP_VER			0x40U
#define IP_MASK_VER		0xf0U
#define IP_MASK_HLEN 		0x0fU
#define IP_MASK_OFFSET		0x1fffU
#define IP_MASK_DONOTFRAG	0x4000U
#define IP_MASK_MOREFRAGS	0x2000U
#define IP_PSHLEN 	12

/* IP header defaults */
#define IP_TOS		0
#define IP_TTL		64

/** An IPv4 packet header */
struct iphdr {
	uint8_t  verhdrlen;
	uint8_t  service;
	uint16_t len;
	uint16_t ident;
	uint16_t frags;
	uint8_t  ttl;
	uint8_t  protocol;
	uint16_t chksum;
	struct in_addr src;
	struct in_addr dest;
} __attribute__ (( packed ));

/** An IPv4 pseudo header */
struct ipv4_pseudo_header {
	struct in_addr src;
	struct in_addr dest;
	uint8_t zero_padding;
	uint8_t protocol;
	uint16_t len;
};

/** An IPv4 address/routing table entry
 *
 * Routing table entries are maintained in order of specificity.  For
 * a given destination address, the first matching table entry will be
 * used as the egress route.
 */
struct ipv4_miniroute {
	/** List of miniroutes */
	struct list_head list;

	/** Network device
	 *
	 * When this routing table entry is matched, this is the
	 * egress network device to be used.
	 */
	struct net_device *netdev;

	/** IPv4 address
	 *
	 * When this routing table entry is matched, this is the
	 * source address to be used.
	 *
	 * The presence of this routing table entry also indicates
	 * that this address is a valid local destination address for
	 * the matching network device.
	 */
	struct in_addr address;
	/** Subnet network address
	 *
	 * A subnet is a range of addresses defined by a network
	 * address and subnet mask.  A destination address with all of
	 * the subnet mask bits in common with the network address is
	 * within the subnet and therefore matches this routing table
	 * entry.
	 */
	struct in_addr network;
	/** Subnet mask
	 *
	 * An address with all of these bits in common with the
	 * network address matches this routing table entry.
	 */
	struct in_addr netmask;
	/** Gateway address, or zero
	 *
	 * When this routing table entry is matched and this address
	 * is non-zero, it will be used as the next-hop address.
	 *
	 * When this routing table entry is matched and this address
	 * is zero, the subnet is local (on-link) and the next-hop
	 * address will be the original destination address.
	 */
	struct in_addr gateway;
	/** Host mask
	 *
	 * An address in a local subnet with all of these bits set to
	 * zero represents the network address, and an address in a
	 * local subnet with all of these bits set to one represents
	 * the local directed broadcast address.  All other addresses
	 * in a local subnet are valid host addresses.
	 *
	 * For most local subnets, this is the inverse of the subnet
	 * mask.  In a small subnet (/31 or /32) there is no network
	 * address or directed broadcast address, and all addresses in
	 * the subnet are valid host addresses.
	 *
	 * When this routing table entry is matched and the subnet is
	 * local, a next-hop address with all of these bits set to one
	 * will be treated as a local broadcast address.  All other
	 * next-hop addresses will be treated as unicast addresses.
	 *
	 * When this routing table entry is matched and the subnet is
	 * non-local, the next-hop address is always a unicast
	 * address.  The host mask for non-local subnets is therefore
	 * set to @c INADDR_NONE to allow the same logic to be used as
	 * for local subnets.
	 */
	struct in_addr hostmask;
};

extern struct list_head ipv4_miniroutes;

extern struct net_protocol ipv4_protocol __net_protocol;

extern struct ipv4_miniroute * ipv4_route ( unsigned int scope_id,
					    struct in_addr *dest );
extern int ipv4_has_any_addr ( struct net_device *netdev );
extern int parse_ipv4_setting ( const struct setting_type *type,
				const char *value, void *buf, size_t len );
extern int format_ipv4_setting ( const struct setting_type *type,
				 const void *raw, size_t raw_len, char *buf,
				 size_t len );

#endif /* _IPXE_IP_H */
