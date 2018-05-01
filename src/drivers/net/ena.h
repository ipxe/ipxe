#ifndef _ENA_H
#define _ENA_H

/** @file
 *
 * Amazon ENA network driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/if_ether.h>

/** BAR size */
#define ENA_BAR_SIZE 16384

/** Queue alignment */
#define ENA_ALIGN 4096

/** Number of admin queue entries */
#define ENA_AQ_COUNT 2

/** Number of admin completion queue entries */
#define ENA_ACQ_COUNT 2

/** Number of transmit queue entries */
#define ENA_TX_COUNT 16

/** Number of receive queue entries */
#define ENA_RX_COUNT 16

/** Base address low register offset */
#define ENA_BASE_LO 0x0

/** Base address high register offset */
#define ENA_BASE_HI 0x4

/** Capability register value */
#define ENA_CAPS( count, size ) ( ( (size) << 16 ) | ( (count) << 0 ) )

/** Admin queue base address register */
#define ENA_AQ_BASE 0x10

/** Admin queue capabilities register */
#define ENA_AQ_CAPS 0x18

/** Admin completion queue base address register */
#define ENA_ACQ_BASE 0x20

/** Admin completion queue capabilities register */
#define ENA_ACQ_CAPS 0x28

/** Admin queue doorbell register */
#define ENA_AQ_DB 0x2c

/** Maximum time to wait for admin requests */
#define ENA_ADMIN_MAX_WAIT_MS 5000

/** Device control register */
#define ENA_CTRL 0x54
#define ENA_CTRL_RESET 0x00000001UL	/**< Reset */

/** Maximum time to wait for reset */
#define ENA_RESET_MAX_WAIT_MS 1000

/** Device status register */
#define ENA_STAT 0x58
#define ENA_STAT_READY 0x00000001UL	/**< Ready */

/** Admin queue entry header */
struct ena_aq_header {
	/** Request identifier */
	uint8_t id;
	/** Reserved */
	uint8_t reserved;
	/** Opcode */
	uint8_t opcode;
	/** Flags */
	uint8_t flags;
} __attribute__ (( packed ));

/** Admin queue ownership phase flag */
#define ENA_AQ_PHASE 0x01

/** Admin completion queue entry header */
struct ena_acq_header {
	/** Request identifier */
	uint8_t id;
	/** Reserved */
	uint8_t reserved;
	/** Status */
	uint8_t status;
	/** Flags */
	uint8_t flags;
	/** Extended status */
	uint16_t ext;
	/** Consumer index */
	uint16_t cons;
} __attribute__ (( packed ));

/** Admin completion queue ownership phase flag */
#define ENA_ACQ_PHASE 0x01

/** Device attributes */
#define ENA_DEVICE_ATTRIBUTES 1

/** Device attributes */
struct ena_device_attributes {
	/** Implementation */
	uint32_t implementation;
	/** Device version */
	uint32_t version;
	/** Supported features */
	uint32_t features;
	/** Reserved */
	uint8_t reserved_a[4];
	/** Physical address width */
	uint32_t physical;
	/** Virtual address width */
	uint32_t virtual;
	/** MAC address */
	uint8_t mac[ETH_ALEN];
	/** Reserved */
	uint8_t reserved_b[2];
	/** Maximum MTU */
	uint32_t mtu;
} __attribute__ (( packed ));

/** Feature */
union ena_feature {
	/** Device attributes */
	struct ena_device_attributes device;
};

/** Submission queue direction */
enum ena_sq_direction {
	/** Transmit */
	ENA_SQ_TX = 0x20,
	/** Receive */
	ENA_SQ_RX = 0x40,
};

/** Create submission queue */
#define ENA_CREATE_SQ 1

/** Create submission queue request */
struct ena_create_sq_req {
	/** Header */
	struct ena_aq_header header;
	/** Direction */
	uint8_t direction;
	/** Reserved */
	uint8_t reserved_a;
	/** Policy */
	uint16_t policy;
	/** Completion queue identifier */
	uint16_t cq_id;
	/** Number of entries */
	uint16_t count;
	/** Base address */
	uint64_t address;
	/** Writeback address */
	uint64_t writeback;
	/** Reserved */
	uint8_t reserved_b[8];
} __attribute__ (( packed ));

/** Submission queue policy */
enum ena_sq_policy {
	/** Use host memory */
	ENA_SQ_HOST_MEMORY = 0x0001,
	/** Memory is contiguous */
	ENA_SQ_CONTIGUOUS = 0x0100,
};

/** Create submission queue response */
struct ena_create_sq_rsp {
	/** Header */
	struct ena_acq_header header;
	/** Submission queue identifier */
	uint16_t id;
	/** Reserved */
	uint8_t reserved[2];
	/** Doorbell register offset */
	uint32_t doorbell;
	/** LLQ descriptor ring offset */
	uint32_t llq_desc;
	/** LLQ header offset */
	uint32_t llq_data;
} __attribute__ (( packed ));

/** Destroy submission queue */
#define ENA_DESTROY_SQ 2

/** Destroy submission queue request */
struct ena_destroy_sq_req {
	/** Header */
	struct ena_aq_header header;
	/** Submission queue identifier */
	uint16_t id;
	/** Direction */
	uint8_t direction;
	/** Reserved */
	uint8_t reserved;
} __attribute__ (( packed ));

/** Destroy submission queue response */
struct ena_destroy_sq_rsp {
	/** Header */
	struct ena_acq_header header;
} __attribute__ (( packed ));

/** Create completion queue */
#define ENA_CREATE_CQ 3

/** Create completion queue request */
struct ena_create_cq_req {
	/** Header */
	struct ena_aq_header header;
	/** Interrupts enabled */
	uint8_t intr;
	/** Entry size (in 32-bit words) */
	uint8_t size;
	/** Number of entries */
	uint16_t count;
	/** MSI-X vector */
	uint32_t vector;
	/** Base address */
	uint64_t address;
} __attribute__ (( packed ));

/** Create completion queue response */
struct ena_create_cq_rsp {
	/** Header */
	struct ena_acq_header header;
	/** Completion queue identifier */
	uint16_t id;
	/** Actual number of entries */
	uint16_t count;
	/** NUMA node register offset */
	uint32_t node;
	/** Doorbell register offset */
	uint32_t doorbell;
	/** Interrupt unmask register offset */
	uint32_t intr;
} __attribute__ (( packed ));

/** Destroy completion queue */
#define ENA_DESTROY_CQ 4

/** Destroy completion queue request */
struct ena_destroy_cq_req {
	/** Header */
	struct ena_aq_header header;
	/** Completion queue identifier */
	uint16_t id;
	/** Reserved */
	uint8_t reserved[2];
} __attribute__ (( packed ));

/** Destroy completion queue response */
struct ena_destroy_cq_rsp {
	/** Header */
	struct ena_acq_header header;
} __attribute__ (( packed ));

/** Get feature */
#define ENA_GET_FEATURE 8

/** Get feature request */
struct ena_get_feature_req {
	/** Header */
	struct ena_aq_header header;
	/** Length */
	uint32_t len;
	/** Address */
	uint64_t address;
	/** Flags */
	uint8_t flags;
	/** Feature identifier */
	uint8_t id;
	/** Reserved */
	uint8_t reserved[2];
} __attribute__ (( packed ));

/** Get feature response */
struct ena_get_feature_rsp {
	/** Header */
	struct ena_acq_header header;
	/** Feature */
	union ena_feature feature;
} __attribute__ (( packed ));

/** Get statistics */
#define ENA_GET_STATS 11

/** Get statistics request */
struct ena_get_stats_req {
	/** Header */
	struct ena_aq_header header;
	/** Reserved */
	uint8_t reserved_a[12];
	/** Type */
	uint8_t type;
	/** Scope */
	uint8_t scope;
	/** Reserved */
	uint8_t reserved_b[2];
	/** Queue ID */
	uint16_t queue;
	/** Device ID */
	uint16_t device;
} __attribute__ (( packed ));

/** Basic statistics */
#define ENA_STATS_TYPE_BASIC 0

/** Ethernet statistics */
#define ENA_STATS_SCOPE_ETH 1

/** My device */
#define ENA_DEVICE_MINE 0xffff

/** Get statistics response */
struct ena_get_stats_rsp {
	/** Header */
	struct ena_acq_header header;
	/** Transmit byte count */
	uint64_t tx_bytes;
	/** Transmit packet count */
	uint64_t tx_packets;
	/** Receive byte count */
	uint64_t rx_bytes;
	/** Receive packet count */
	uint64_t rx_packets;
	/** Receive drop count */
	uint64_t rx_drops;
} __attribute__ (( packed ));

/** Admin queue request */
union ena_aq_req {
	/** Header */
	struct ena_aq_header header;
	/** Create submission queue */
	struct ena_create_sq_req create_sq;
	/** Destroy submission queue */
	struct ena_destroy_sq_req destroy_sq;
	/** Create completion queue */
	struct ena_create_cq_req create_cq;
	/** Destroy completion queue */
	struct ena_destroy_cq_req destroy_cq;
	/** Get feature */
	struct ena_get_feature_req get_feature;
	/** Get statistics */
	struct ena_get_stats_req get_stats;
	/** Padding */
	uint8_t pad[64];
};

/** Admin completion queue response */
union ena_acq_rsp {
	/** Header */
	struct ena_acq_header header;
	/** Create submission queue */
	struct ena_create_sq_rsp create_sq;
	/** Destroy submission queue */
	struct ena_destroy_sq_rsp destroy_sq;
	/** Create completion queue */
	struct ena_create_cq_rsp create_cq;
	/** Destroy completion queue */
	struct ena_destroy_cq_rsp destroy_cq;
	/** Get feature */
	struct ena_get_feature_rsp get_feature;
	/** Get statistics */
	struct ena_get_stats_rsp get_stats;
	/** Padding */
	uint8_t pad[64];
};

/** Admin queue */
struct ena_aq {
	/** Requests */
	union ena_aq_req *req;
	/** Producer counter */
	unsigned int prod;
};

/** Admin completion queue */
struct ena_acq {
	/** Responses */
	union ena_acq_rsp *rsp;
	/** Consumer counter */
	unsigned int cons;
	/** Phase */
	unsigned int phase;
};

/** Transmit submission queue entry */
struct ena_tx_sqe {
	/** Length */
	uint16_t len;
	/** Reserved */
	uint8_t reserved_a;
	/** Flags */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved_b[3];
	/** Request identifier */
	uint8_t id;
	/** Address */
	uint64_t address;
} __attribute__ (( packed ));

/** Receive submission queue entry */
struct ena_rx_sqe {
	/** Length */
	uint16_t len;
	/** Reserved */
	uint8_t reserved_a;
	/** Flags */
	uint8_t flags;
	/** Request identifier */
	uint16_t id;
	/** Reserved */
	uint8_t reserved_b[2];
	/** Address */
	uint64_t address;
} __attribute__ (( packed ));

/** Submission queue ownership phase flag */
#define ENA_SQE_PHASE 0x01

/** This is the first descriptor */
#define ENA_SQE_FIRST 0x04

/** This is the last descriptor */
#define ENA_SQE_LAST 0x08

/** Request completion */
#define ENA_SQE_CPL 0x10

/** Transmit completion queue entry */
struct ena_tx_cqe {
	/** Request identifier */
	uint16_t id;
	/** Status */
	uint8_t status;
	/** Flags */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved[2];
	/** Consumer index */
	uint16_t cons;
} __attribute__ (( packed ));

/** Receive completion queue entry */
struct ena_rx_cqe {
	/** Reserved */
	uint8_t reserved_a[3];
	/** Flags */
	uint8_t flags;
	/** Length */
	uint16_t len;
	/** Request identifier */
	uint16_t id;
	/** Reserved */
	uint8_t reserved_b[8];
} __attribute__ (( packed ));

/** Completion queue ownership phase flag */
#define ENA_CQE_PHASE 0x01

/** Submission queue */
struct ena_sq {
	/** Entries */
	union {
		/** Transmit submission queue entries */
		struct ena_tx_sqe *tx;
		/** Receive submission queue entries */
		struct ena_rx_sqe *rx;
		/** Raw data */
		void *raw;
	} sqe;
	/** Doorbell register offset */
	unsigned int doorbell;
	/** Total length of entries */
	size_t len;
	/** Producer counter */
	unsigned int prod;
	/** Phase */
	unsigned int phase;
	/** Submission queue identifier */
	uint16_t id;
	/** Direction */
	uint8_t direction;
	/** Number of entries */
	uint8_t count;
};

/**
 * Initialise submission queue
 *
 * @v sq		Submission queue
 * @v direction		Direction
 * @v count		Number of entries
 * @v size		Size of each entry
 */
static inline __attribute__ (( always_inline )) void
ena_sq_init ( struct ena_sq *sq, unsigned int direction, unsigned int count,
	      size_t size ) {

	sq->len = ( count * size );
	sq->direction = direction;
	sq->count = count;
}

/** Completion queue */
struct ena_cq {
	/** Entries */
	union {
		/** Transmit completion queue entries */
		struct ena_tx_cqe *tx;
		/** Receive completion queue entries */
		struct ena_rx_cqe *rx;
		/** Raw data */
		void *raw;
	} cqe;
	/** Doorbell register offset */
	unsigned int doorbell;
	/** Total length of entries */
	size_t len;
	/** Consumer counter */
	unsigned int cons;
	/** Phase */
	unsigned int phase;
	/** Completion queue identifier */
	uint16_t id;
	/** Entry size (in 32-bit words) */
	uint8_t size;
	/** Requested number of entries */
	uint8_t requested;
	/** Actual number of entries */
	uint8_t actual;
	/** Actual number of entries minus one */
	uint8_t mask;
};

/**
 * Initialise completion queue
 *
 * @v cq		Completion queue
 * @v count		Number of entries
 * @v size		Size of each entry
 */
static inline __attribute__ (( always_inline )) void
ena_cq_init ( struct ena_cq *cq, unsigned int count, size_t size ) {

	cq->len = ( count * size );
	cq->size = ( size / sizeof ( uint32_t ) );
	cq->requested = count;
}

/** Queue pair */
struct ena_qp {
	/** Submission queue */
	struct ena_sq sq;
	/** Completion queue */
	struct ena_cq cq;
};

/** An ENA network card */
struct ena_nic {
	/** Registers */
	void *regs;
	/** Admin queue */
	struct ena_aq aq;
	/** Admin completion queue */
	struct ena_acq acq;
	/** Transmit queue */
	struct ena_qp tx;
	/** Receive queue */
	struct ena_qp rx;
	/** Receive I/O buffers */
	struct io_buffer *rx_iobuf[ENA_RX_COUNT];
};

#endif /* _ENA_H */
