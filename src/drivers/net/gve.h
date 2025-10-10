#ifndef _GVE_H
#define _GVE_H

/** @file
 *
 * Google Virtual Ethernet network driver
 *
 * The Google Virtual Ethernet NIC (GVE or gVNIC) is found only in
 * Google Cloud instances.  There is essentially zero documentation
 * available beyond the mostly uncommented source code in the Linux
 * kernel.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/dma.h>
#include <ipxe/pci.h>
#include <ipxe/pcimsix.h>
#include <ipxe/in.h>
#include <ipxe/process.h>
#include <ipxe/retry.h>

struct gve_nic;

/**
 * A Google Cloud MAC address
 *
 * Google Cloud locally assigned MAC addresses encode the local IPv4
 * address in the trailing 32 bits, presumably as a performance
 * optimisation to allow ARP resolution to be skipped by a suitably
 * aware network stack.
 */
struct google_mac {
	/** Reserved */
	uint8_t reserved[2];
	/** Local IPv4 address */
	struct in_addr in;
} __attribute__ (( packed ));

/** Page size */
#define GVE_PAGE_SIZE 0x1000

/**
 * Address alignment
 *
 * All DMA data structure base addresses seem to need to be aligned to
 * a page boundary.  (This is not documented anywhere, but is inferred
 * from existing source code and experimentation.)
 */
#define GVE_ALIGN GVE_PAGE_SIZE

/** Configuration BAR */
#define GVE_CFG_BAR PCI_BASE_ADDRESS_0

/**
 * Configuration BAR size
 *
 * All registers within the configuration BAR are big-endian.
 */
#define GVE_CFG_SIZE 0x1000

/** Device status */
#define GVE_CFG_DEVSTAT 0x0000
#define GVE_CFG_DEVSTAT_RESET 0x00000010UL	/**< Device is reset */

/** Driver status */
#define GVE_CFG_DRVSTAT 0x0004
#define GVE_CFG_DRVSTAT_RUN 0x00000001UL	/**< Run admin queue */

/** Maximum time to wait for reset */
#define GVE_RESET_MAX_WAIT_MS 500

/** Admin queue page frame number (for older devices) */
#define GVE_CFG_ADMIN_PFN 0x0010

/** Admin queue doorbell */
#define GVE_CFG_ADMIN_DB 0x0014

/** Admin queue event counter */
#define GVE_CFG_ADMIN_EVT 0x0018

/** Driver version (8-bit register) */
#define GVE_CFG_VERSION 0x001f

/** Admin queue base address high 32 bits */
#define GVE_CFG_ADMIN_BASE_HI 0x0020

/** Admin queue base address low 32 bits */
#define GVE_CFG_ADMIN_BASE_LO 0x0024

/** Admin queue base address length (16-bit register) */
#define GVE_CFG_ADMIN_LEN 0x0028

/** Doorbell BAR */
#define GVE_DB_BAR PCI_BASE_ADDRESS_2

/**
 * Admin queue entry header
 *
 * All values within admin queue entries are big-endian.
 */
struct gve_admin_header {
	/** Reserved */
	uint8_t reserved[3];
	/** Operation code */
	uint8_t opcode;
	/** Status */
	uint32_t status;
} __attribute__ (( packed ));

/** Command succeeded */
#define GVE_ADMIN_STATUS_OK 0x00000001

/** Simple admin command */
struct gve_admin_simple {
	/** Header */
	struct gve_admin_header hdr;
	/** ID */
	uint32_t id;
} __attribute__ (( packed ));

/** Describe device command */
#define GVE_ADMIN_DESCRIBE 0x0001

/** Describe device command */
struct gve_admin_describe {
	/** Header */
	struct gve_admin_header hdr;
	/** Descriptor buffer address */
	uint64_t addr;
	/** Descriptor version */
	uint32_t ver;
	/** Descriptor maximum length */
	uint32_t len;
} __attribute__ (( packed ));

/** Device descriptor version */
#define GVE_ADMIN_DESCRIBE_VER 1

/** Device descriptor */
struct gve_device_descriptor {
	/** Reserved */
	uint8_t reserved_a[10];
	/** Number of transmit queue entries */
	uint16_t tx_count;
	/** Number of receive queue entries */
	uint16_t rx_count;
	/** Reserved */
	uint8_t reserved_b[2];
	/** Maximum transmit unit */
	uint16_t mtu;
	/** Number of event counters */
	uint16_t counters;
	/** Reserved */
	uint8_t reserved_c[4];
	/** MAC address */
	struct google_mac mac;
	/** Number of device options */
	uint16_t opt_count;
	/** Total length (including this header) */
	uint16_t len;
	/** Reserved */
	uint8_t reserved_d[6];
	/** Space for options
	 *
	 * There is no specified upper limit, and no negotiation
	 * mechanism for the amount of space required.  We allow space
	 * for seems like a reasonable number of options.
	 */
	uint8_t opts[216];
} __attribute__ (( packed ));

/** Device option header */
struct gve_option {
	/** Option ID */
	uint16_t id;
	/** Length (excluding this header) */
	uint16_t len;
	/** Required feature mask
	 *
	 * The purpose of this field is remarkably unclear.  The Linux
	 * kernel driver does define enum gve_dev_opt_req_feat_mask,
	 * but every member of this enum has a zero value.
	 */
	uint32_t required;
} __attribute__ (( packed ));

/** In-order descriptor queues with raw DMA addressing */
#define GVE_OPT_GQI_RDA 0x02

/** In-order descriptor queues with queue page list addressing */
#define GVE_OPT_GQI_QPL 0x03

/** Out-of-order descriptor queues with raw DMA addressing */
#define GVE_OPT_DQO_RDA 0x04

/** Out-of-order descriptor queues with queue page list addressing */
#define GVE_OPT_DQO_QPL 0x07

/** Configure device resources command */
#define GVE_ADMIN_CONFIGURE 0x0002

/** Configure device resources command */
struct gve_admin_configure {
	/** Header */
	struct gve_admin_header hdr;
	/** Event counter array */
	uint64_t events;
	/** IRQ doorbell address */
	uint64_t irqs;
	/** Number of event counters */
	uint32_t num_events;
	/** Number of IRQ doorbells */
	uint32_t num_irqs;
	/** IRQ doorbell stride */
	uint32_t irq_stride;
	/** MSI-X base index */
	uint32_t msix_base;
	/** Descriptor queue format */
	uint8_t format;
	/** Reserved */
	uint8_t reserved[7];
} __attribute__ (( packed ));

/** Descriptor queue format */
#define GVE_FORMAT( mode ) ( (mode) + 1 )

/** Register page list command */
#define GVE_ADMIN_REGISTER 0x0003

/** Register page list command */
struct gve_admin_register {
	/** Header */
	struct gve_admin_header hdr;
	/** Page list ID */
	uint32_t id;
	/** Number of pages */
	uint32_t count;
	/** Address list address */
	uint64_t addr;
	/** Page size */
	uint64_t size;
} __attribute__ (( packed ));

/**
 * Maximum number of pages per queue
 *
 * This is a policy decision.  Must be sufficient to allow for both
 * the transmit and receive queue fill levels.
 */
#define GVE_QPL_MAX 32

/** Page list */
struct gve_pages {
	/** Page address */
	uint64_t addr[GVE_QPL_MAX];
} __attribute__ (( packed ));

/** Unregister page list command */
#define GVE_ADMIN_UNREGISTER 0x0004

/** Create transmit queue command */
#define GVE_ADMIN_CREATE_TX 0x0005

/** Create transmit queue command */
struct gve_admin_create_tx {
	/** Header */
	struct gve_admin_header hdr;
	/** Queue ID */
	uint32_t id;
	/** Reserved */
	uint8_t reserved_a[4];
	/** Queue resources address */
	uint64_t res;
	/** Descriptor ring address */
	uint64_t desc;
	/** Queue page list ID */
	uint32_t qpl_id;
	/** Notification channel ID */
	uint32_t notify_id;
	/** Completion ring address */
	uint64_t cmplt;
	/** Number of descriptor ring entries */
	uint16_t desc_count;
	/** Number of completion ring entries */
	uint16_t cmplt_count;
	/** Reserved */
	uint8_t reserved_b[4];
} __attribute__ (( packed ));

/** Create receive queue command */
#define GVE_ADMIN_CREATE_RX 0x0006

/** Create receive queue command */
struct gve_admin_create_rx {
	/** Header */
	struct gve_admin_header hdr;
	/** Queue ID */
	uint32_t id;
	/** Index */
	uint32_t index;
	/** Reserved */
	uint8_t reserved_a[4];
	/** Notification channel ID */
	uint32_t notify_id;
	/** Queue resources address */
	uint64_t res;
	/** Completion ring address */
	uint64_t cmplt;
	/** Descriptor ring address */
	uint64_t desc;
	/** Queue page list ID */
	uint32_t qpl_id;
	/** Number of descriptor ring entries */
	uint16_t desc_count;
	/** Packet buffer size */
	uint16_t bufsz;
	/** Number of completion ring entries */
	uint16_t cmplt_count;
	/** Reserved */
	uint8_t reserved[6];
} __attribute__ (( packed ));

/** Destroy transmit queue command */
#define GVE_ADMIN_DESTROY_TX 0x0007

/** Destroy receive queue command */
#define GVE_ADMIN_DESTROY_RX 0x0008

/** Deconfigure device resources command */
#define GVE_ADMIN_DECONFIGURE 0x0009

/** An admin queue command */
union gve_admin_command {
	/** Header */
	struct gve_admin_header hdr;
	/** Simple command */
	struct gve_admin_simple simple;
	/** Describe device */
	struct gve_admin_describe desc;
	/** Configure device resources */
	struct gve_admin_configure conf;
	/** Register page list */
	struct gve_admin_register reg;
	/** Create transmit queue */
	struct gve_admin_create_tx create_tx;
	/** Create receive queue */
	struct gve_admin_create_rx create_rx;
	/** Padding */
	uint8_t pad[64];
};

/**
 * Number of admin queue commands
 *
 * This is theoretically a policy decision.  However, older revisions
 * of the hardware seem to have only the "admin queue page frame
 * number" register and no "admin queue length" register, with the
 * implication that the admin queue must be exactly one page in
 * length.
 *
 * Choose to use a one page (4kB) admin queue for both older and newer
 * versions of the hardware, to minimise variability.
 */
#define GVE_ADMIN_COUNT ( GVE_PAGE_SIZE / sizeof ( union gve_admin_command ) )

/** Admin queue */
struct gve_admin {
	/** Commands */
	union gve_admin_command *cmd;
	/** Producer counter */
	uint32_t prod;
	/** DMA mapping */
	struct dma_mapping map;
};

/** Scratch buffer for admin queue commands */
struct gve_scratch {
	/** Buffer contents */
	union {
		/** Device descriptor */
		struct gve_device_descriptor desc;
		/** Page address list */
		struct gve_pages pages;
	} *buf;
	/** DMA mapping */
	struct dma_mapping map;
};

/**
 * An event counter
 *
 * Written by the device to indicate completions.  The device chooses
 * which counter to use for each transmit queue, and stores the index
 * of the chosen counter in the queue resources.
 */
struct gve_event {
	/** Number of events that have occurred */
	volatile uint32_t count;
} __attribute__ (( packed ));

/** Event counter array */
struct gve_events {
	/** Event counters */
	struct gve_event *event;
	/** DMA mapping */
	struct dma_mapping map;
	/** Actual number of event counters */
	unsigned int count;
};

/** An interrupt channel */
struct gve_irq {
	/** Interrupt doorbell index (within doorbell BAR) */
	uint32_t db_idx;
	/** Reserved */
	uint8_t reserved[60];
} __attribute__ (( packed ));

/**
 * Number of interrupt channels
 *
 * We tell the device how many interrupt channels we have provided via
 * the "configure device resources" admin queue command.  The device
 * will accept being given zero interrupt channels, but will
 * subsequently fail to create more than a single queue (either
 * transmit or receive).
 *
 * There is, of course, no documentation indicating how may interrupt
 * channels actually need to be provided.  In the absence of evidence
 * to the contrary, assume that two channels (one for transmit, one
 * for receive) will be sufficient.
 */
#define GVE_IRQ_COUNT 2

/** Interrupt channel array */
struct gve_irqs {
	/** Interrupt channels */
	struct gve_irq *irq;
	/** DMA mapping */
	struct dma_mapping map;
	/** Interrupt doorbells */
	volatile uint32_t *db[GVE_IRQ_COUNT];
};

/** Disable in-order queue interrupt */
#define GVE_GQI_IRQ_DISABLE 0x40000000UL

/** Rearm out-of-order queue interrupt */
#define GVE_DQO_IRQ_REARM 0x00000019UL

/**
 * Queue resources
 *
 * Written by the device to indicate the indices of the chosen event
 * counter and descriptor doorbell register.
 *
 * This appears to be a largely pointless data structure: the relevant
 * information is static for the lifetime of the queue and could
 * trivially have been returned in the response for the "create
 * transmit/receive queue" command, instead of requiring yet another
 * page-aligned coherent DMA buffer allocation.
 */
struct gve_resources {
	/** Descriptor doorbell index (within doorbell BAR) */
	uint32_t db_idx;
	/** Event counter index (within event counter array) */
	uint32_t evt_idx;
	/** Reserved */
	uint8_t reserved[56];
} __attribute__ (( packed ));

/**
 * Queue data buffer size
 *
 * In theory, we may specify the size of receive buffers.  However,
 * the original version of the device seems not to have a parameter
 * for this, and assumes the use of half-page (2kB) buffers.  Choose
 * to use this as the buffer size, on the assumption that older
 * devices will not support any other buffer size.
 */
#define GVE_BUF_SIZE ( GVE_PAGE_SIZE / 2 )

/** Number of data buffers per page */
#define GVE_BUF_PER_PAGE ( GVE_PAGE_SIZE / GVE_BUF_SIZE )

/**
 * Queue page list
 *
 * The device uses preregistered pages for fast-path DMA operations
 * (i.e. transmit and receive buffers).  A list of device addresses
 * for each page must be registered before the transmit or receive
 * queue is created, and cannot subsequently be modified.
 *
 * The Linux driver allocates pages as DMA_TO_DEVICE or
 * DMA_FROM_DEVICE as appropriate, and uses dma_sync_single_for_cpu()
 * etc to ensure that data is copied to/from bounce buffers as needed.
 *
 * Unfortunately there is no such sync operation available within our
 * DMA API, since we are constrained by the limitations imposed by
 * EFI_PCI_IO_PROTOCOL.  There is no way to synchronise a buffer
 * without also [un]mapping it, and no way to force the reuse of the
 * same device address for a subsequent remapping.  We are therefore
 * constrained to use only DMA-coherent buffers, since this is the
 * only way we can repeatedly reuse the same device address.
 *
 * Newer versions of the gVNIC device support "raw DMA addressing
 * (RDA)", which is essentially a prebuilt queue page list covering
 * the whole of the guest address space.  Unfortunately we cannot rely
 * on this, since older versions will not support it.
 *
 * Experimentation suggests that the device will accept a request to
 * create a queue page list covering the whole of the guest address
 * space via two giant "pages" of 2^63 bytes each.  However,
 * experimentation also suggests that the device will accept any old
 * garbage value as the "page size".  In the total absence of any
 * documentation, it is probably unsafe to conclude that the device is
 * bothering to look at or respect the "page size" parameter: it is
 * most likely just presuming the use of 4kB pages.
 */
struct gve_qpl {
	/** Page addresses */
	void *data;
	/** Page mapping */
	struct dma_mapping map;
	/** Number of pages */
	unsigned int count;
	/** Queue page list ID */
	unsigned int id;
	/** Queue page list base device address
	 *
	 * This will be zero in the GQI-QPL operating mode, or the DMA
	 * address of the first page in any other operating mode.
	 * (Despite its name, DQO-QPL still requires the use of raw
	 * DMA addresses in transmit and receive descriptors.)
	 */
	physaddr_t base;
};

/** Raw DMA addressing queue page list ID */
#define GVE_RAW_QPL 0xffffffff

/**
 * Maximum number of transmit buffers
 *
 * This is a policy decision.
 */
#define GVE_TX_FILL 8

/** Transmit queue page list ID */
#define GVE_TX_QPL 0x18ae5458

/** Tranmsit queue interrupt channel */
#define GVE_TX_IRQ 0

/** A transmit or receive buffer descriptor */
struct gve_buffer {
	/** Address (within queue page list address space) */
	uint64_t addr;
} __attribute__ (( packed ));

/** An in-order transmit descriptor */
struct gve_gqi_tx_descriptor {
	/** Type */
	uint8_t type;
	/** Reserved */
	uint8_t reserved_a[2];
	/** Number of descriptors in this packet */
	uint8_t count;
	/** Total length of this packet */
	uint16_t total;
	/** Length of this descriptor */
	uint16_t len;
	/** Buffer descriptor */
	struct gve_buffer buf;
} __attribute__ (( packed ));

/** Start of packet transmit descriptor type */
#define GVE_GQI_TX_TYPE_START 0x00

/** Continuation of packet transmit descriptor type */
#define GVE_GQI_TX_TYPE_CONT 0x20

/** An out-of-order transmit tag
 *
 * From the hardware perspective, this is an opaque 15-bit (sic) value
 * that is simply copied from the descriptor to the corresponding
 * completion.
 */
struct gve_dqo_tx_tag {
	/** Buffer index within queue page list */
	uint8_t id;
	/** Number of descriptors covered by this completion
	 *
	 * Note that this is a 7-bit quantity: the high bit may be
	 * (ab)used by the hardware to indicate that a completion is a
	 * terminologically undefined "miss" completion.
	 */
	int8_t count;
} __attribute__ (( packed ));

/** An out-of-order transmit descriptor */
struct gve_dqo_tx_descriptor {
	/** Buffer descriptor */
	struct gve_buffer buf;
	/** Descriptor type and flags */
	uint8_t type;
	/** Reserved */
	uint8_t reserved_a[3];
	/** Tag */
	struct gve_dqo_tx_tag tag;
	/** Length of this descriptor */
	uint16_t len;
} __attribute__ (( packed ));

/** Normal packet transmit descriptor type */
#define GVE_DQO_TX_TYPE_PACKET 0x0c

/** Last transmit descriptor in a packet */
#define GVE_DQO_TX_TYPE_LAST 0x20

/** An out-of-order transmit completion */
struct gve_dqo_tx_completion {
	/** Reserved */
	uint8_t reserved_a[1];
	/** Completion flags */
	uint8_t flags;
	/** Tag */
	struct gve_dqo_tx_tag tag;
	/** Reserved */
	uint8_t reserved_b[4];
} __attribute__ (( packed ));

/** Transmit completion packet flag */
#define GVE_DQO_TXF_PKT 0x10

/** Transmit completion generation flag */
#define GVE_DQO_TXF_GEN 0x80

/**
 * Maximum number of receive buffers
 *
 * This is a policy decision.  Experiments suggest that using fewer
 * than 64 receive buffers leads to excessive packet drop rates on
 * some instance types.
 */
#define GVE_RX_FILL 64

/** Receive queue page list ID */
#define GVE_RX_QPL 0x18ae5258

/** Receive queue interrupt channel */
#define GVE_RX_IRQ 1

/** An in-order receive descriptor */
struct gve_gqi_rx_descriptor {
	/** Buffer descriptor */
	struct gve_buffer buf;
} __attribute__ (( packed ));

/** Receive error */
#define GVE_GQI_RXF_ERROR 0x08

/** Receive packet continues into next descriptor */
#define GVE_GQI_RXF_MORE 0x20

/** Receive sequence number mask */
#define GVE_GQI_RX_SEQ_MASK 0x07

/** An in-order receive completion descriptor */
struct gve_gqi_rx_completion {
	/** Reserved */
	uint8_t reserved[60];
	/** Length */
	uint16_t len;
	/** Flags */
	uint8_t flags;
	/** Sequence number */
	uint8_t seq;
} __attribute__ (( packed ));

/** Padding at the start of all received packets */
#define GVE_GQI_RX_PAD 2

/** An out-of-order receive descriptor */
struct gve_dqo_rx_descriptor {
	/** Tag */
	uint8_t tag;
	/** Reserved */
	uint8_t reserved_a[7];
	/** Buffer descriptor */
	struct gve_buffer buf;
	/** Reserved */
	uint8_t reserved_b[16];
} __attribute__ (( packed ));

/** An out-of-order receive completion */
struct gve_dqo_rx_completion {
	/** Reserved */
	uint8_t reserved_a[1];
	/** Status */
	uint8_t status;
	/** Reserved */
	uint8_t reserved_b[2];
	/** Length and generation bit */
	uint16_t len;
	/** Reserved */
	uint8_t reserved_c[2];
	/** Flags */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved_d[3];
	/** Tag */
	uint8_t tag;
	/** Reserved */
	uint8_t reserved_e[19];
} __attribute__ (( packed ));

/** Receive error */
#define GVE_DQO_RXS_ERROR 0x04

/** Receive completion generation flag */
#define GVE_DQO_RXL_GEN 0x4000

/** Last receive descriptor in a packet */
#define GVE_DQO_RXF_LAST 0x02

/** Queue strides */
struct gve_queue_stride {
	/** Descriptor ring stride */
	uint8_t desc;
	/** Completion ring stride */
	uint8_t cmplt;
};

/** A descriptor queue */
struct gve_queue {
	/** Descriptor ring */
	union {
		/** Transmit descriptors */
		union {
			/** In-order transmit descriptors */
			struct gve_gqi_tx_descriptor *gqi;
			/** Out-of-order transmit descriptors */
			struct gve_dqo_tx_descriptor *dqo;
		} tx;
		/** Receive descriptors */
		union {
			/** In-order receive descriptors */
			struct gve_gqi_rx_descriptor *gqi;
			/** Out-of-order receive descriptors */
			struct gve_dqo_rx_descriptor *dqo;
		} rx;
		/** Raw data */
		void *raw;
	} desc;
	/** Completion ring */
	union {
		/** Transmit completions */
		union {
			/** Out-of-order transmit completions */
			struct gve_dqo_tx_completion *dqo;
		} tx;
		/** Receive completions */
		union {
			/** In-order receive completions */
			struct gve_gqi_rx_completion *gqi;
			/** Out-of-order receive completions */
			struct gve_dqo_rx_completion *dqo;
		} rx;
		/** Raw data */
		void *raw;
	} cmplt;
	/** Queue resources */
	struct gve_resources *res;

	/** Queue type */
	const struct gve_queue_type *type;
	/** Queue strides */
	struct gve_queue_stride stride;
	/** Number of descriptors (must be a power of two) */
	unsigned int count;
	/** Maximum fill level (must be a power of two) */
	unsigned int fill;

	/** Descriptor mapping */
	struct dma_mapping desc_map;
	/** Completion mapping */
	struct dma_mapping cmplt_map;
	/** Queue resources mapping */
	struct dma_mapping res_map;

	/** Doorbell register */
	volatile uint32_t *db;
	/** Event counter */
	struct gve_event *event;

	/** Producer counter */
	uint32_t prod;
	/** Consumer counter */
	uint32_t cons;
	/** Completion counter */
	uint32_t done;
	/** Tag ring */
	uint8_t *tag;

	/** Queue page list */
	struct gve_qpl qpl;
};

/** A descriptor queue type */
struct gve_queue_type {
	/** Name */
	const char *name;
	/**
	 * Populate command parameters to create queue
	 *
	 * @v queue		Descriptor queue
	 * @v qpl		Queue page list ID
	 * @v cmd		Admin queue command
	 */
	void ( * param ) ( struct gve_queue *queue, uint32_t qpl,
			   union gve_admin_command *cmd );
	/** Queue page list ID */
	uint32_t qpl;
	/** Interrupt channel */
	uint8_t irq;
	/** Maximum fill level */
	uint8_t fill;
	/** Queue strides */
	struct {
		/** In-order queue strides */
		struct gve_queue_stride gqi;
		/** Out-of-order queue strides */
		struct gve_queue_stride dqo;
	} stride;
	/** Command to create queue */
	uint8_t create;
	/** Command to destroy queue */
	uint8_t destroy;
};

/** A Google Virtual Ethernet NIC */
struct gve_nic {
	/** Configuration registers */
	void *cfg;
	/** Doorbell registers */
	void *db;
	/** PCI revision */
	uint8_t revision;
	/** Network device */
	struct net_device *netdev;
	/** DMA device */
	struct dma_device *dma;
	/** Dummy MSI-X interrupt */
	struct pci_msix msix;

	/** Admin queue */
	struct gve_admin admin;
	/** Interrupt channels */
	struct gve_irqs irqs;
	/** Event counters */
	struct gve_events events;
	/** Scratch buffer */
	struct gve_scratch scratch;
	/** Supported options */
	uint32_t options;
	/** Operating mode */
	unsigned int mode;

	/** Transmit queue */
	struct gve_queue tx;
	/** Receive queue */
	struct gve_queue rx;
	/** Transmit I/O buffers (indexed by tag) */
	struct io_buffer *tx_iobuf[GVE_TX_FILL];
	/** Transmit tag chain */
	uint8_t tx_chain[GVE_TX_FILL];
	/** Transmit tag ring */
	uint8_t tx_tag[GVE_TX_FILL];
	/** Receive tag ring */
	uint8_t rx_tag[GVE_RX_FILL];
	/** Receive sequence number */
	unsigned int seq;

	/** Startup process */
	struct process startup;
	/** Startup process retry counter */
	unsigned int retries;
	/** Reset recovery watchdog timer */
	struct retry_timer watchdog;
	/** Reset recovery recorded activity counter */
	uint32_t activity;
};

/** Operating mode
 *
 * These values are chosen to allow for easy transformation to a queue
 * format identifier as used for the "Configure device resources"
 * command.
 */
#define GVE_MODE_QPL 0x01		/**< Use registered queue pages */
#define GVE_MODE_DQO 0x02		/**< Use out-of-order queues */

/** Maximum time to wait for admin queue commands */
#define GVE_ADMIN_MAX_WAIT_MS 500

/** Maximum number of times to reattempt device reset */
#define GVE_RESET_MAX_RETRY 5

/** Time between reset recovery checks */
#define GVE_WATCHDOG_TIMEOUT ( 1 * TICKS_PER_SEC )

#endif /* _GVE_H */
