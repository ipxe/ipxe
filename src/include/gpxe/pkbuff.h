#ifndef _PKBUFF_H
#define _PKBUFF_H

/** @file
 *
 * Packet buffers
 *
 * Packet buffers are used to contain network packets.  Methods are
 * provided for appending, prepending, etc. data.
 *
 */

#include <stdint.h>
#include <assert.h>

/** A packet buffer
 *
 * This structure is used to represent a network packet within gPXE.
 */
struct pk_buff {
	/** Head of the buffer */
	void *head;
	/** Start of data */
	void *data;
	/** End of data */
	void *tail;
	/** End of the buffer */
        void *end;

	/** The network-layer protocol
	 *
	 * This is the network-layer protocol expressed as an
	 * ETH_P_XXX constant, in network-byte order.
	 */
	uint16_t net_proto;
	/** Flags
	 *
	 * Filled in only on outgoing packets.  Value is the
	 * bitwise-OR of zero or more PKB_FL_XXX constants.
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
	void *net_addr;
};

/** Packet is a broadcast packet */
#define PKB_FL_BROADCAST 0x01

/** Packet is a multicast packet */
#define PKB_FL_MULTICAST 0x02

/** Network-layer address is a raw hardware address */
#define PKB_FL_RAW_NET_ADDR 0x04

/**
 * Add data to start of packet buffer
 *
 * @v pkb	Packet buffer
 * @v len	Length to add
 * @ret data	Pointer to new start of buffer
 */
static inline void * pkb_push ( struct pk_buff *pkb, size_t len ) {
	pkb->data -= len;
	assert ( pkb->data >= pkb->head );
	return pkb->data;
}

/**
 * Remove data from start of packet buffer
 *
 * @v pkb	Packet buffer
 * @v len	Length to remove
 * @ret data	Pointer to new start of buffer
 */
static inline void * pkb_pull ( struct pk_buff *pkb, size_t len ) {
	pkb->data += len;
	assert ( pkb->data >= pkb->tail );
	return pkb->data;
}

/**
 * Add data to end of packet buffer
 *
 * @v pkb	Packet buffer
 * @v len	Length to add
 * @ret data	Pointer to newly added space
 */
static inline void * pkb_put ( struct pk_buff *pkb, size_t len ) {
	void *old_tail = pkb->tail;
	pkb->tail += len;
	assert ( pkb->tail <= pkb->end );
	return old_tail;
}

/**
 * Remove data from end of packet buffer
 *
 * @v pkb	Packet buffer
 * @v len	Length to remove
 */
static inline void pkb_unput ( struct pk_buff *pkb, size_t len ) {
	pkb->tail -= len;
	assert ( pkb->tail >= pkb->data );
}

/**
 * Empty a packet buffer
 *
 * @v pkb	Packet buffer
 */
static inline void pkb_empty ( struct pk_buff *pkb ) {
	pkb->tail = pkb->data;
}

/**
 * Calculate length of data in a packet buffer
 *
 * @v pkb	Packet buffer
 * @ret len	Length of data in buffer
 */
static inline size_t pkb_len ( struct pk_buff *pkb ) {
	return ( pkb->tail - pkb->data );
}

#endif /* _PKBUFF_H */
