#ifndef _INTELXLVF_H
#define _INTELXLVF_H

/** @file
 *
 * Intel 40 Gigabit Ethernet virtual function network card driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include "intelxl.h"

/** BAR size */
#define INTELXLVF_BAR_SIZE 0x10000

/** MSI-X vector
 *
 * The 100 Gigabit physical function driver requires a virtual
 * function driver to request that transmit and receive queues are
 * mapped to MSI-X vector 1 or higher.
 */
#define INTELXLVF_MSIX_VECTOR 1

/** Transmit Queue Tail Register */
#define INTELXLVF_QTX_TAIL 0x00000

/** Receive Queue Tail Register */
#define INTELXLVF_QRX_TAIL 0x02000

/** VF Interrupt N Dynamic Control Register */
#define INTELXLVF_VFINT_DYN_CTLN( x ) ( 0x3800 + ( 0x4 * ( (x) - 1  ) ) )

/** VF Interrupt Zero Dynamic Control Register */
#define INTELXLVF_VFINT_DYN_CTL0 0x5c00

/** VF Admin Queue register block */
#define INTELXLVF_ADMIN 0x6000

/** Admin Command Queue Base Address Low Register (offset) */
#define INTELXLVF_ADMIN_CMD_BAL 0x1c00

/** Admin Command Queue Base Address High Register (offset) */
#define INTELXLVF_ADMIN_CMD_BAH 0x1800

/** Admin Command Queue Length Register (offset) */
#define INTELXLVF_ADMIN_CMD_LEN 0x0800

/** Admin Command Queue Head Register (offset) */
#define INTELXLVF_ADMIN_CMD_HEAD 0x0400

/** Admin Command Queue Tail Register (offset) */
#define INTELXLVF_ADMIN_CMD_TAIL 0x2400

/** Admin Event Queue Base Address Low Register (offset) */
#define INTELXLVF_ADMIN_EVT_BAL 0x0c00

/** Admin Event Queue Base Address High Register (offset) */
#define INTELXLVF_ADMIN_EVT_BAH 0x0000

/** Admin Event Queue Length Register (offset) */
#define INTELXLVF_ADMIN_EVT_LEN 0x2000

/** Admin Event Queue Head Register (offset) */
#define INTELXLVF_ADMIN_EVT_HEAD 0x1400

/** Admin Event Queue Tail Register (offset) */
#define INTELXLVF_ADMIN_EVT_TAIL 0x1000

/** Maximum time to wait for a VF admin request to complete */
#define INTELXLVF_ADMIN_MAX_WAIT_MS 2000

/** Admin queue Send Message to PF command */
#define INTELXLVF_ADMIN_SEND_TO_PF 0x0801

/** Admin queue Send Message to VF command */
#define INTELXLVF_ADMIN_SEND_TO_VF 0x0802

/** Admin Queue VF Version opcode */
#define INTELXLVF_ADMIN_VERSION 0x00000001

/** Admin Queue VF Version data buffer */
struct intelxlvf_admin_version_buffer {
	/** Major version */
	uint32_t major;
	/** Minor version */
	uint32_t minor;
} __attribute__ (( packed ));

/** Admin queue VF API major version */
#define INTELXLVF_ADMIN_API_MAJOR 1

/** Admin queue VF API minor version */
#define INTELXLVF_ADMIN_API_MINOR 1

/** Admin Queue VF Reset opcode */
#define INTELXLVF_ADMIN_RESET 0x00000002

/** Admin Queue VF Get Resources opcode */
#define INTELXLVF_ADMIN_GET_RESOURCES 0x00000003

/** Admin Queue VF Capabilities data buffer */
struct intelxlvf_admin_capabilities_buffer {
	/** Capabilities */
	uint32_t caps;
} __attribute__ (( packed ));

/** Admin Queue VF Get Resources data buffer */
struct intelxlvf_admin_get_resources_buffer {
	/** Number of VSIs */
	uint16_t vsis;
	/** Number of queue pairs */
	uint16_t qps;
	/** Number of MSI-X vectors */
	uint16_t vectors;
	/** Maximum MTU */
	uint16_t mtu;
	/** Capabilities */
	uint32_t caps;
	/** Reserved */
	uint8_t reserved_a[8];
	/** VSI switching element ID */
	uint16_t vsi;
	/** Reserved */
	uint8_t reserved_b[8];
	/** MAC address */
	uint8_t mac[ETH_ALEN];
} __attribute__ (( packed ));

/** Layer 2 capabilities (add/remove MAC, configure promiscuous mode) */
#define INTELXLVF_ADMIN_CAP_L2 0x00000001

/** Request Queues capabilities */
#define INTELXLVF_ADMIN_CAP_RQPS 0x00000040

/** Admin Queue VF Status Change Event opcode */
#define INTELXLVF_ADMIN_STATUS 0x00000011

/** Link status change event type */
#define INTELXLVF_ADMIN_STATUS_LINK 0x00000001

/** Link status change event data */
struct intelxlvf_admin_status_link {
	/** Link speed */
	uint32_t speed;
	/** Link status */
	uint8_t status;
	/** Reserved */
	uint8_t reserved[3];
} __attribute__ (( packed ));

/** Admin Queue VF Status Change Event data buffer */
struct intelxlvf_admin_status_buffer {
	/** Event type */
	uint32_t event;
	/** Event data */
	union {
		/** Link change event data */
		struct intelxlvf_admin_status_link link;
	} data;
	/** Reserved */
	uint8_t reserved[4];
} __attribute__ (( packed ));

/** Admin Queue VF Configure Queues opcode */
#define INTELXLVF_ADMIN_CONFIGURE 0x00000006

/** Admin Queue VF Configure Queues data buffer */
struct intelxlvf_admin_configure_buffer {
	/** VSI switching element ID */
	uint16_t vsi;
	/** Number of queue pairs */
	uint16_t count;
	/** Reserved */
	uint8_t reserved_a[4];
	/** Transmit queue */
	struct {
		/** VSI switching element ID */
		uint16_t vsi;
		/** Queue ID */
		uint16_t id;
		/** Queue count */
		uint16_t count;
		/** Reserved */
		uint8_t reserved_a[2];
		/** Base address */
		uint64_t base;
		/** Reserved */
		uint8_t reserved_b[8];
	} __attribute__ (( packed )) tx;
	/** Receive queue */
	struct {
		/** VSI switching element ID */
		uint16_t vsi;
		/** Queue ID */
		uint16_t id;
		/** Queue count */
		uint32_t count;
		/** Reserved */
		uint8_t reserved_a[4];
		/** Data buffer length */
		uint32_t len;
		/** Maximum frame size */
		uint32_t mfs;
		/** Reserved */
		uint8_t reserved_b[4];
		/** Base address */
		uint64_t base;
		/** Reserved */
		uint8_t reserved_c[8];
	} __attribute__ (( packed )) rx;
	/** Reserved
	 *
	 * This field exists only due to a bug in the PF driver's
	 * message validation logic, which causes it to miscalculate
	 * the expected message length.
	 */
	uint8_t reserved_b[64];
} __attribute__ (( packed ));

/** Admin Queue VF IRQ Map opcode */
#define INTELXLVF_ADMIN_IRQ_MAP 0x00000007

/** Admin Queue VF IRQ Map data buffer */
struct intelxlvf_admin_irq_map_buffer {
	/** Number of interrupt vectors */
	uint16_t count;
	/** VSI switching element ID */
	uint16_t vsi;
	/** Interrupt vector ID */
	uint16_t vec;
	/** Receive queue bitmap */
	uint16_t rxmap;
	/** Transmit queue bitmap */
	uint16_t txmap;
	/** Receive interrupt throttling index */
	uint16_t rxitr;
	/** Transmit interrupt throttling index */
	uint16_t txitr;
	/** Reserved
	 *
	 * This field exists only due to a bug in the PF driver's
	 * message validation logic, which causes it to miscalculate
	 * the expected message length.
	 */
	uint8_t reserved[12];
} __attribute__ (( packed ));

/** Admin Queue VF Enable Queues opcode */
#define INTELXLVF_ADMIN_ENABLE 0x00000008

/** Admin Queue VF Disable Queues opcode */
#define INTELXLVF_ADMIN_DISABLE 0x00000009

/** Admin Queue VF Enable/Disable Queues data buffer */
struct intelxlvf_admin_queues_buffer {
	/** VSI switching element ID */
	uint16_t vsi;
	/** Reserved */
	uint8_t reserved[2];
	/** Receive queue bitmask */
	uint32_t rx;
	/** Transmit queue bitmask */
	uint32_t tx;
} __attribute__ (( packed ));

/** Admin Queue VF Configure Promiscuous Mode opcode */
#define INTELXLVF_ADMIN_PROMISC 0x0000000e

/** Admin Queue VF Configure Promiscuous Mode data buffer */
struct intelxlvf_admin_promisc_buffer {
	/** VSI switching element ID */
	uint16_t vsi;
	/** Flags */
	uint16_t flags;
} __attribute__ (( packed ));

/** Admin Queue VF Get Statistics opcode */
#define INTELXLVF_ADMIN_GET_STATS 0x0000000f

/** VF statistics */
struct intelxlvf_admin_stats {
	/** Bytes */
	uint64_t bytes;
	/** Unicast packets */
	uint64_t unicasts;
	/** Multicast packets */
	uint64_t multicasts;
	/** Broadcast packets */
	uint64_t broadcasts;
	/** Discarded packets */
	uint64_t discards;
	/** Errors */
	uint64_t errors;
} __attribute__ (( packed ));

/** Admin Queue VF Get Statistics data buffer */
struct intelxlvf_admin_stats_buffer {
	/** Receive statistics */
	struct intelxlvf_admin_stats rx;
	/** Transmit statistics */
	struct intelxlvf_admin_stats tx;
} __attribute__ (( packed ));

/** Admin Queue VF Request Queues opcode */
#define INTELXLVF_ADMIN_REQUEST_QPS 0x0000001d

/** Admin Queue VF Request Queues data buffer */
struct intelxlvf_admin_request_qps_buffer {
	/** Number of queue pairs */
	uint16_t count;
} __attribute__ (( packed ));

/** Admin queue data buffer */
union intelxlvf_admin_buffer {
	/** Original 40 Gigabit Ethernet data buffer */
	union intelxl_admin_buffer xl;
	/** VF Version data buffer */
	struct intelxlvf_admin_version_buffer ver;
	/** VF Capabilities data buffer */
	struct intelxlvf_admin_capabilities_buffer caps;
	/** VF Get Resources data buffer */
	struct intelxlvf_admin_get_resources_buffer res;
	/** VF Status Change Event data buffer */
	struct intelxlvf_admin_status_buffer stat;
	/** VF Configure Queues data buffer */
	struct intelxlvf_admin_configure_buffer cfg;
	/** VF Enable/Disable Queues data buffer */
	struct intelxlvf_admin_queues_buffer queues;
	/** VF Configure Promiscuous Mode data buffer */
	struct intelxlvf_admin_promisc_buffer promisc;
	/** VF IRQ Map data buffer */
	struct intelxlvf_admin_irq_map_buffer irq;
	/** VF Get Statistics data buffer */
	struct intelxlvf_admin_stats_buffer stats;
	/** VF Request Queues data buffer */
	struct intelxlvf_admin_request_qps_buffer rqps;
} __attribute__ (( packed ));

/** Admin queue descriptor */
struct intelxlvf_admin_descriptor {
	/** Transparent union */
	union {
		/** Original 40 Gigabit Ethernet descriptor */
		struct intelxl_admin_descriptor xl;
		/** Transparent struct */
		struct {
			/** Flags */
			uint16_t flags;
			/** Opcode */
			uint16_t opcode;
			/** Data length */
			uint16_t len;
			/** Return value */
			uint16_t ret;
			/** VF opcode */
			uint32_t vopcode;
			/** VF return value */
			int32_t vret;
			/** Parameters */
			union intelxl_admin_params params;
		} __attribute__ (( packed ));
	} __attribute__ (( packed ));
} __attribute__ (( packed ));

/**
 * Get next admin command queue descriptor
 *
 * @v intelxl		Intel device
 * @ret cmd		Command descriptor
 */
struct intelxlvf_admin_descriptor *
intelxlvf_admin_command_descriptor ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin_descriptor *xlcmd =
		intelxl_admin_command_descriptor ( intelxl );

	return container_of ( xlcmd, struct intelxlvf_admin_descriptor, xl );
}

/**
 * Get next admin command queue data buffer
 *
 * @v intelxl		Intel device
 * @ret buf		Data buffer
 */
static inline __attribute__ (( always_inline )) union intelxlvf_admin_buffer *
intelxlvf_admin_command_buffer ( struct intelxl_nic *intelxl ) {
	union intelxl_admin_buffer *xlbuf =
		intelxl_admin_command_buffer ( intelxl );

	return container_of ( xlbuf, union intelxlvf_admin_buffer, xl );
}

/** VF Reset Status Register */
#define INTELXLVF_VFGEN_RSTAT 0x8800
#define INTELXLVF_VFGEN_RSTAT_VFR_STATE(x) ( (x) & 0x3 )
#define INTELXLVF_VFGEN_RSTAT_VFR_STATE_ACTIVE 0x2

/** Minimum time to wait for reset to complete */
#define INTELXLVF_RESET_DELAY_MS 100

/** Maximum time to wait for reset to complete */
#define INTELXLVF_RESET_MAX_WAIT_MS 1000

/**
 * Initialise descriptor ring
 *
 * @v ring		Descriptor ring
 * @v count		Number of descriptors
 * @v len		Length of a single descriptor
 * @v tail		Tail register offset
 */
static inline __attribute__ (( always_inline)) void
intelxlvf_init_ring ( struct intelxl_ring *ring, unsigned int count,
		      size_t len, unsigned int tail ) {

	ring->len = ( count * len );
	ring->tail = tail;
}

#endif /* _INTELXLVF_H */
