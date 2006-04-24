#ifndef _GPXE_PKBUFF_H
#define _GPXE_PKBUFF_H

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
#include <gpxe/list.h>

struct net_protocol;
struct ll_protocol;

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

	/** List of which this buffer is a member */
	struct list_head list;

	/** The network-layer protocol */
	struct net_protocol *net_protocol;
	/** The link-layer protocol */
	struct ll_protocol *ll_protocol;
};

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

extern struct pk_buff * alloc_pkb ( size_t len );
extern void free_pkb ( struct pk_buff *pkb );

#endif /* _GPXE_PKBUFF_H */
