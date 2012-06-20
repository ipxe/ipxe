#ifndef _IPXE_IP_H
#define _IPXE_IP_H

/** @file
 *
 * IP protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

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

/** An IPv4 address/routing table entry */
struct ipv4_miniroute {
	/** List of miniroutes */
	struct list_head list;

	/** Network device */
	struct net_device *netdev;

	/** IPv4 address */
	struct in_addr address;
	/** Subnet mask */
	struct in_addr netmask;
	/** Gateway address */
	struct in_addr gateway;
};

/* IPv4 fragment reassembly buffer */
struct ipv4_fragment {
	/* List of fragment reassembly buffers */
	struct list_head list;
	/** Reassembled packet */
	struct io_buffer *iobuf;
	/** Current offset */
	size_t offset;
	/** Reassembly timer */
	struct retry_timer timer;
};

extern struct list_head ipv4_miniroutes;

extern struct net_protocol ipv4_protocol __net_protocol;

extern int ipv4_has_any_addr ( struct net_device *netdev );

#endif /* _IPXE_IP_H */
