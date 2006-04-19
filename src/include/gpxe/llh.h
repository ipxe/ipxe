#ifndef _LLH_H
#define _LLH_H

/** @file
 *
 * Link-layer headers
 *
 * This file defines a media-independent link-layer header, used for
 * communication between the network and link layers of the stack.
 *
 */

#include <stdint.h>

/** Maximum length of a link-layer address */
#define MAX_LLH_ADDR_LEN 6

/** Maximum length of a network-layer address */
#define MAX_NET_ADDR_LEN 4

/* Network-layer address may be required to contain a raw link-layer address */
#if MAX_NET_ADDR_LEN < MAX_LLH_ADDR_LEN
#undef MAX_NET_ADDR_LEN
#define MAX_NET_ADDR_LEN MAX_LLH_ADDR_LEN
#endif

/** A media-independent link-layer header
 *
 * This structure represents a generic link-layer header.  It never
 * appears on the wire, but is used to communicate between different
 * layers within the gPXE protocol stack.
 */
struct gpxehdr {
	/** The network-layer protocol
	 *
	 * This is the network-layer protocol expressed as an
	 * ETH_P_XXX constant, in network-byte order.
	 */
	uint16_t net_proto;
	/** Flags
	 *
	 * Filled in only on outgoing packets.  Value is the
	 * bitwise-OR of zero or more GPXE_FL_XXX constants.
	 */
	uint8_t flags;
	/** Network-layer address length 
	 *
	 * Filled in only on outgoing packets.
	 */
	uint8_t net_addr_len;
	/** Network-layer address
	 *
	 * Filled in only on outgoing packets.
	 */
	uint8_t net_addr[MAX_NET_ADDR_LEN];
} __attribute__ (( packed ));

/* Media-independent link-layer header flags */
#define GPXE_FL_BROADCAST	0x01
#define GPXE_FL_MULTICAST	0x02
#define GPXE_FL_RAW		0x04

#endif /* _LLH_H */
