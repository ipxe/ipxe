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

/**
 * Packet buffer alignment
 *
 * Packet buffers allocated via alloc_pkb() are guaranteed to be
 * physically aligned to this boundary.  Some cards cannot DMA across
 * a 4kB boundary.  With a standard Ethernet MTU, aligning to a 2kB
 * boundary is sufficient to guarantee no 4kB boundary crossings.  For
 * a jumbo Ethernet MTU, a packet may be larger than 4kB anyway.
 */
#define PKBUFF_ALIGN 2048

/**
 * Minimum packet buffer length
 *
 * alloc_pkb() will round up the allocated length to this size if
 * necessary.  This is used on behalf of hardware that is not capable
 * of auto-padding.
 */
#define PKB_ZLEN 64

/** A packet buffer
 *
 * This structure is used to represent a network packet within gPXE.
 */
struct pk_buff {
	/** List of which this buffer is a member */
	struct list_head list;

	/** Start of the buffer */
	void *head;
	/** Start of data */
	void *data;
	/** End of data */
	void *tail;
	/** End of the buffer */
        void *end;
};

/**
 * Reserve space at start of packet buffer
 *
 * @v pkb	Packet buffer
 * @v len	Length to reserve
 * @ret data	Pointer to new start of buffer
 */
static inline void * pkb_reserve ( struct pk_buff *pkb, size_t len ) {
	pkb->data += len;
	pkb->tail += len;
	assert ( pkb->tail <= pkb->end );
	return pkb->data;
}

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
	assert ( pkb->data <= pkb->tail );
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

/**
 * Calculate available space at start of a packet buffer
 *
 * @v pkb	Packet buffer
 * @ret len	Length of data available at start of buffer
 */
static inline size_t pkb_headroom ( struct pk_buff *pkb ) {
	return ( pkb->data - pkb->head );
}

/**
 * Calculate available space at end of a packet buffer
 *
 * @v pkb	Packet buffer
 * @ret len	Length of data available at end of buffer
 */
static inline size_t pkb_tailroom ( struct pk_buff *pkb ) {
	return ( pkb->end - pkb->tail );
}

extern struct pk_buff * alloc_pkb ( size_t len );
extern void free_pkb ( struct pk_buff *pkb );
extern void pkb_pad ( struct pk_buff *pkb, size_t min_len );

#endif /* _GPXE_PKBUFF_H */
