#ifndef _GPXE_IP_H
#define _GPXE_IP_H

/** @file
 *
 * IP protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <gpxe/in.h>
#include <gpxe/list.h>
#include <gpxe/retry.h>

struct io_buffer;
struct net_device;
struct net_protocol;

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

#define IP_FRAG_IOB_SIZE	1500
#define IP_FRAG_TIMEOUT		50

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

/* Fragment reassembly buffer */
struct frag_buffer {
	/* Identification number */
	uint16_t ident;
	/* Source network address */
	struct in_addr src;
	/* Destination network address */
	struct in_addr dest;
	/* Reassembled I/O buffer */
	struct io_buffer *frag_iob;
	/* Reassembly timer */
	struct retry_timer frag_timer;
	/* List of fragment reassembly buffers */
	struct list_head list;
};

extern struct list_head ipv4_miniroutes;

extern struct net_protocol ipv4_protocol;

#endif /* _GPXE_IP_H */
