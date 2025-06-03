#ifndef _NETFRONT_H
#define _NETFRONT_H

/** @file
 *
 * Xen netfront driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/xen.h>
#include <xen/io/netif.h>

/** Number of transmit ring entries */
#define NETFRONT_NUM_TX_DESC 16

/** Number of receive ring entries */
#define NETFRONT_NUM_RX_DESC 32

/** Receive ring fill level
 *
 * The xen-netback driver from kernels 3.18 to 4.2 inclusive have a
 * bug (CA-163395) which prevents packet reception if fewer than 18
 * receive descriptors are available.  This was fixed in upstream
 * kernel commit d5d4852 ("xen-netback: require fewer guest Rx slots
 * when not using GSO").
 *
 * We provide 18 receive descriptors to avoid unpleasant silent
 * failures on these kernel versions.
 */
#define NETFRONT_RX_FILL 18

/** Grant reference indices */
enum netfront_ref_index {
	/** Transmit ring grant reference index */
	NETFRONT_REF_TX_RING = 0,
	/** Transmit descriptor grant reference base index */
	NETFRONT_REF_TX_BASE,
	/** Receive ring grant reference index */
	NETFRONT_REF_RX_RING = ( NETFRONT_REF_TX_BASE + NETFRONT_NUM_TX_DESC ),
	/** Receive descriptor grant reference base index */
	NETFRONT_REF_RX_BASE,
	/** Total number of grant references required */
	NETFRONT_REF_COUNT = ( NETFRONT_REF_RX_BASE + NETFRONT_NUM_RX_DESC )
};

/** A netfront descriptor ring */
struct netfront_ring {
	/** Shared ring */
	union {
		/** Transmit shared ring */
		netif_tx_sring_t *tx;
		/** Receive shared ring */
		netif_rx_sring_t *rx;
		/** Raw pointer */
		void *raw;
	} sring;
	/** Shared ring grant reference key */
	const char *ref_key;
	/** Shared ring grant reference */
	grant_ref_t ref;

	/** Maximum number of used descriptors */
	size_t count;
	/** I/O buffers, indexed by buffer ID */
	struct io_buffer **iobufs;
	/** Grant references, indexed by buffer ID */
	grant_ref_t *refs;

	/** Buffer ID ring */
	uint8_t *ids;
	/** Buffer ID ring producer counter */
	unsigned int id_prod;
	/** Buffer ID ring consumer counter */
	unsigned int id_cons;
};

/**
 * Initialise descriptor ring
 *
 * @v ring		Descriptor ring
 * @v ref_key		Shared ring grant reference key
 * @v ref		Shared ring grant reference
 * @v count		Maxium number of used descriptors
 * @v iobufs		I/O buffers
 * @v refs		I/O buffer grant references
 * @v ids		Buffer IDs
 */
static inline __attribute__ (( always_inline )) void
netfront_init_ring ( struct netfront_ring *ring, const char *ref_key,
		     grant_ref_t ref, unsigned int count,
		     struct io_buffer **iobufs, grant_ref_t *refs,
		     uint8_t *ids ) {

	ring->ref_key = ref_key;
	ring->ref = ref;
	ring->count = count;
	ring->iobufs = iobufs;
	ring->refs = refs;
	ring->ids = ids;
}

/**
 * Calculate descriptor ring fill level
 *
 * @v ring		Descriptor ring
 * @v fill		Fill level
 */
static inline __attribute__ (( always_inline )) unsigned int
netfront_ring_fill ( struct netfront_ring *ring ) {
	unsigned int fill_level;

	fill_level = ( ring->id_prod - ring->id_cons );
	assert ( fill_level <= ring->count );
	return fill_level;
}

/**
 * Calculate descriptor ring remaining space
 *
 * @v ring		Descriptor ring
 * @v space		Number of unused entries
 */
static inline __attribute__ (( always_inline )) unsigned int
netfront_ring_space ( struct netfront_ring *ring ) {

	return ( ring->count - netfront_ring_fill ( ring ) );
}

/**
 * Check whether or not descriptor ring is full
 *
 * @v ring		Descriptor ring
 * @v is_full		Ring is full
 */
static inline __attribute__ (( always_inline )) int
netfront_ring_is_full ( struct netfront_ring *ring ) {

	return ( netfront_ring_fill ( ring ) >= ring->count );
}

/**
 * Check whether or not descriptor ring is empty
 *
 * @v ring		Descriptor ring
 * @v is_empty		Ring is empty
 */
static inline __attribute__ (( always_inline )) int
netfront_ring_is_empty ( struct netfront_ring *ring ) {

	return ( netfront_ring_fill ( ring ) == 0 );
}

/** A netfront NIC */
struct netfront_nic {
	/** Xen device */
	struct xen_device *xendev;
	/** Grant references */
	grant_ref_t refs[NETFRONT_REF_COUNT];

	/** Network device */
	struct net_device *netdev;
	/** List of netfront NICs */
	struct list_head list;

	/** Transmit ring */
	struct netfront_ring tx;
	/** Transmit front ring */
	netif_tx_front_ring_t tx_fring;
	/** Transmit I/O buffers */
	struct io_buffer *tx_iobufs[NETFRONT_NUM_TX_DESC];
	/** Transmit I/O buffer IDs */
	uint8_t tx_ids[NETFRONT_NUM_TX_DESC];

	/** Receive ring */
	struct netfront_ring rx;
	/** Receive front ring */
	netif_rx_front_ring_t rx_fring;
	/** Receive I/O buffers */
	struct io_buffer *rx_iobufs[NETFRONT_NUM_RX_DESC];
	/** Receive I/O buffer IDs */
	uint8_t rx_ids[NETFRONT_NUM_RX_DESC];
	/** Partial receive I/O buffer list */
	struct list_head rx_partial;

	/** Event channel */
	struct evtchn_send event;
};

/** Transmit shared ring field */
#define tx_sring tx.sring.tx

/** Receive shared ring field */
#define rx_sring rx.sring.rx

#endif /* _NETFRONT_H */
