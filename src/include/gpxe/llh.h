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
	/** Broadcast flag
	 *
	 * Filled in only on outgoing packets.
	 */
	int broadcast : 1;
	/** Multicast flag
	 *
	 * Filled in only on outgoing packets.
	 */
	int multicast : 1;
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

#endif /* _LLH_H */
