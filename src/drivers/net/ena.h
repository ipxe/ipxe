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

/** Number of async event notification queue entries */
#define ENA_AENQ_COUNT 2

/** Number of transmit queue entries */
#define ENA_TX_COUNT 16

/** Number of receive queue entries */
#define ENA_RX_COUNT 128

/** Receive queue maximum fill level */
#define ENA_RX_FILL 16

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

/** Async event notification queue capabilities register */
#define ENA_AENQ_CAPS 0x34

/** Async event notification queue base address register */
#define ENA_AENQ_BASE 0x38

/** Device control register */
#define ENA_CTRL 0x54
#define ENA_CTRL_RESET 0x00000001UL	/**< Reset */

/** Maximum time to wait for reset */
#define ENA_RESET_MAX_WAIT_MS 1000

/** Device status register */
#define ENA_STAT 0x58
#define ENA_STAT_RESET 0x00000008UL	/**< Reset in progress */

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

/** Async event notification queue config */
#define ENA_AENQ_CONFIG 26

/** Async event notification queue config */
struct ena_aenq_config {
	/** Bitmask of supported AENQ groups (device -> host) */
	uint32_t supported;
	/** Bitmask of enabled AENQ groups (host -> device) */
	uint32_t enabled;
} __attribute__ (( packed ));

/** Host attributes */
#define ENA_HOST_ATTRIBUTES 28

/** Host attributes */
struct ena_host_attributes {
	/** Host info base address */
	uint64_t info;
	/** Debug area base address */
	uint64_t debug;
	/** Debug area size */
	uint32_t debug_len;
} __attribute__ (( packed ));

/** Host information */
struct ena_host_info {
	/** Operating system type */
	uint32_t type;
	/** Operating system distribution (string) */
	char dist_str[128];
	/** Operating system distribution (numeric) */
	uint32_t dist;
	/** Kernel version (string) */
	char kernel_str[32];
	/** Kernel version (numeric) */
	uint32_t kernel;
	/** Driver version */
	uint32_t version;
	/** Linux network device features */
	uint64_t linux_features;
	/** ENA specification version */
	uint16_t spec;
	/** PCI bus:dev.fn address */
	uint16_t busdevfn;
	/** Number of CPUs */
	uint16_t cpus;
	/** Reserved */
	uint8_t reserved_a[2];
	/** Supported features */
	uint32_t features;
} __attribute__ (( packed ));

/** Operating system type
 *
 * Some very broken older versions of the ENA firmware will refuse to
 * allow a completion queue to be created if "iPXE" (type 5) is used,
 * and require us to pretend that we are "Linux" (type 1) instead.
 *
 * The ENA team at AWS assures us that the entire AWS fleet has been
 * upgraded to fix this bug, and that we are now safe to use the
 * correct operating system type value.
 */
#define ENA_HOST_INFO_TYPE_IPXE 5

/** Driver version
 *
 * The driver version field is nominally used to report a version
 * number outside of the VM for consumption by humans (and potentially
 * by automated monitoring tools that could e.g. check for outdated
 * versions with known security flaws).
 *
 * However, at some point in the development of the ENA firmware, some
 * unknown person at AWS thought it would be sensible to apply a
 * machine interpretation to this field and adjust the behaviour of
 * the firmware based on its value, thereby creating a maintenance and
 * debugging nightmare for all existing and future drivers.
 *
 * Hint to engineers: if you ever find yourself writing code of the
 * form "if (version == SOME_MAGIC_NUMBER)" then something has gone
 * very, very wrong.  This *always* indicates that something is
 * broken, either in your own code or in the code with which you are
 * forced to interact.
 */
#define ENA_HOST_INFO_VERSION_WTF 0x00000002UL

/** ENA specification version */
#define ENA_HOST_INFO_SPEC_2_0 0x0200

/** Feature */
union ena_feature {
	/** Device attributes */
	struct ena_device_attributes device;
	/** Async event notification queue config */
	struct ena_aenq_config aenq;
	/** Host attributes */
	struct ena_host_attributes host;
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

/** Empty MSI-X vector
 *
 * Some versions of the ENA firmware will complain if the completion
 * queue's MSI-X vector field is left empty, even though the queue
 * configuration specifies that interrupts are not used.
 */
#define ENA_MSIX_NONE 0xffffffffUL

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

/** Set feature */
#define ENA_SET_FEATURE 9

/** Set feature request */
struct ena_set_feature_req {
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
	/** Set feature */
	struct ena_set_feature_req set_feature;
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

/** Async event notification queue event */
struct ena_aenq_event {
	/** Type of event */
	uint16_t group;
	/** ID of event */
	uint16_t syndrome;
	/** Phase */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved[3];
	/** Timestamp */
	uint64_t timestamp;
	/** Additional event data */
	uint8_t data[48];
} __attribute__ (( packed ));

/** Async event notification queue */
struct ena_aenq {
	/** Events */
	struct ena_aenq_event *evt;
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

/** Transmit completion request identifier */
#define ENA_TX_CQE_ID(id) ( (id) >> 2 )

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
	/** Buffer IDs */
	uint8_t *ids;
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
	/** Maximum fill level */
	uint8_t max;
	/** Fill level (limited to completion queue size) */
	uint8_t fill;
};

/**
 * Initialise submission queue
 *
 * @v sq		Submission queue
 * @v direction		Direction
 * @v count		Number of entries
 * @v max		Maximum fill level
 * @v size		Size of each entry
 * @v ids		Buffer IDs
 */
static inline __attribute__ (( always_inline )) void
ena_sq_init ( struct ena_sq *sq, unsigned int direction, unsigned int count,
	      unsigned int max, size_t size, uint8_t *ids ) {

	sq->len = ( count * size );
	sq->direction = direction;
	sq->count = count;
	sq->max = max;
	sq->ids = ids;
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
	/** Host info */
	struct ena_host_info *info;
	/** Admin queue */
	struct ena_aq aq;
	/** Admin completion queue */
	struct ena_acq acq;
	/** Async event notification queue */
	struct ena_aenq aenq;
	/** Transmit queue */
	struct ena_qp tx;
	/** Receive queue */
	struct ena_qp rx;
	/** Transmit buffer IDs */
	uint8_t tx_ids[ENA_TX_COUNT];
	/** Transmit I/O buffers, indexed by buffer ID */
	struct io_buffer *tx_iobuf[ENA_TX_COUNT];
	/** Receive buffer IDs */
	uint8_t rx_ids[ENA_RX_COUNT];
	/** Receive I/O buffers, indexed by buffer ID */
	struct io_buffer *rx_iobuf[ENA_RX_COUNT];
};

#endif /* _ENA_H */
