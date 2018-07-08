#ifndef _INTELX_H
#define _INTELX_H

/** @file
 *
 * Intel 40 Gigabit Ethernet network card driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/if_ether.h>

struct intelxl_nic;

/** BAR size */
#define INTELXL_BAR_SIZE 0x200000

/** Alignment
 *
 * No data structure requires greater than 128 byte alignment.
 */
#define INTELXL_ALIGN 128

/******************************************************************************
 *
 * Admin queue
 *
 ******************************************************************************
 */

/** PF Admin Command Queue register block */
#define INTELXL_ADMIN_CMD 0x080000

/** PF Admin Event Queue register block */
#define INTELXL_ADMIN_EVT 0x080080

/** Admin Queue Base Address Low Register (offset) */
#define INTELXL_ADMIN_BAL 0x000

/** Admin Queue Base Address High Register (offset) */
#define INTELXL_ADMIN_BAH 0x100

/** Admin Queue Length Register (offset) */
#define INTELXL_ADMIN_LEN 0x200
#define INTELXL_ADMIN_LEN_LEN(x)	( (x) << 0 )	/**< Queue length */
#define INTELXL_ADMIN_LEN_ENABLE	0x80000000UL	/**< Queue enable */

/** Admin Queue Head Register (offset) */
#define INTELXL_ADMIN_HEAD 0x300

/** Admin Queue Tail Register (offset) */
#define INTELXL_ADMIN_TAIL 0x400

/** Admin queue data buffer command parameters */
struct intelxl_admin_buffer_params {
	/** Reserved */
	uint8_t reserved[8];
	/** Buffer address high */
	uint32_t high;
	/** Buffer address low */
	uint32_t low;
} __attribute__ (( packed ));

/** Admin queue Get Version command */
#define INTELXL_ADMIN_VERSION 0x0001

/** Admin queue version number */
struct intelxl_admin_version {
	/** Major version number */
	uint16_t major;
	/** Minor version number */
	uint16_t minor;
} __attribute__ (( packed ));

/** Admin queue Get Version command parameters */
struct intelxl_admin_version_params {
	/** ROM version */
	uint32_t rom;
	/** Firmware build ID */
	uint32_t build;
	/** Firmware version */
	struct intelxl_admin_version firmware;
	/** API version */
	struct intelxl_admin_version api;
} __attribute__ (( packed ));

/** Admin queue Driver Version command */
#define INTELXL_ADMIN_DRIVER 0x0002

/** Admin queue Driver Version command parameters */
struct intelxl_admin_driver_params {
	/** Driver version */
	uint8_t major;
	/** Minor version */
	uint8_t minor;
	/** Build version */
	uint8_t build;
	/** Sub-build version */
	uint8_t sub;
	/** Reserved */
	uint8_t reserved[4];
	/** Data buffer address */
	uint64_t address;
} __attribute__ (( packed ));

/** Admin queue Driver Version data buffer */
struct intelxl_admin_driver_buffer {
	/** Driver name */
	char name[32];
} __attribute__ (( packed ));

/** Admin queue Shutdown command */
#define INTELXL_ADMIN_SHUTDOWN 0x0003

/** Admin queue Shutdown command parameters */
struct intelxl_admin_shutdown_params {
	/** Driver unloading */
	uint8_t unloading;
	/** Reserved */
	uint8_t reserved[15];
} __attribute__ (( packed ));

/** Driver is unloading */
#define INTELXL_ADMIN_SHUTDOWN_UNLOADING 0x01

/** Admin queue Get Switch Configuration command */
#define INTELXL_ADMIN_SWITCH 0x0200

/** Switching element configuration */
struct intelxl_admin_switch_config {
	/** Switching element type */
	uint8_t type;
	/** Revision */
	uint8_t revision;
	/** Switching element ID */
	uint16_t seid;
	/** Uplink switching element ID */
	uint16_t uplink;
	/** Downlink switching element ID */
	uint16_t downlink;
	/** Reserved */
	uint8_t reserved_b[3];
	/** Connection type */
	uint8_t connection;
	/** Reserved */
	uint8_t reserved_c[2];
	/** Element specific information */
	uint16_t info;
} __attribute__ (( packed ));

/** Virtual Station Inferface element type */
#define INTELXL_ADMIN_SWITCH_TYPE_VSI 19

/** Admin queue Get Switch Configuration command parameters */
struct intelxl_admin_switch_params {
	/** Starting switching element identifier */
	uint16_t next;
	/** Reserved */
	uint8_t reserved[6];
	/** Data buffer address */
	uint64_t address;
} __attribute__ (( packed ));

/** Admin queue Get Switch Configuration data buffer */
struct intelxl_admin_switch_buffer {
	/** Number of switching elements reported */
	uint16_t count;
	/** Total number of switching elements */
	uint16_t total;
	/** Reserved */
	uint8_t reserved_a[12];
	/** Switch configuration */
	struct intelxl_admin_switch_config cfg;
} __attribute__ (( packed ));

/** Admin queue Get VSI Parameters command */
#define INTELXL_ADMIN_VSI 0x0212

/** Admin queue Get VSI Parameters command parameters */
struct intelxl_admin_vsi_params {
	/** VSI switching element ID */
	uint16_t vsi;
	/** Reserved */
	uint8_t reserved[6];
	/** Data buffer address */
	uint64_t address;
} __attribute__ (( packed ));

/** Admin queue Get VSI Parameters data buffer */
struct intelxl_admin_vsi_buffer {
	/** Reserved */
	uint8_t reserved_a[30];
	/** Queue numbers */
	uint16_t queue[16];
	/** Reserved */
	uint8_t reserved_b[34];
	/** Queue set handles for each traffic class */
	uint16_t qset[8];
	/** Reserved */
	uint8_t reserved_c[16];
} __attribute__ (( packed ));

/** Admin queue Set VSI Promiscuous Modes command */
#define INTELXL_ADMIN_PROMISC 0x0254

/** Admin queue Set VSI Promiscuous Modes command parameters */
struct intelxl_admin_promisc_params {
	/** Flags */
	uint16_t flags;
	/** Valid flags */
	uint16_t valid;
	/** VSI switching element ID */
	uint16_t vsi;
	/** Reserved */
	uint8_t reserved[10];
} __attribute__ (( packed ));

/** Promiscuous unicast mode */
#define INTELXL_ADMIN_PROMISC_FL_UNICAST 0x0001

/** Promiscuous multicast mode */
#define INTELXL_ADMIN_PROMISC_FL_MULTICAST 0x0002

/** Promiscuous broadcast mode */
#define INTELXL_ADMIN_PROMISC_FL_BROADCAST 0x0004

/** Promiscuous VLAN mode */
#define INTELXL_ADMIN_PROMISC_FL_VLAN 0x0010

/** Admin queue Restart Autonegotiation command */
#define INTELXL_ADMIN_AUTONEG 0x0605

/** Admin queue Restart Autonegotiation command parameters */
struct intelxl_admin_autoneg_params {
	/** Flags */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved[15];
} __attribute__ (( packed ));

/** Restart autonegotiation */
#define INTELXL_ADMIN_AUTONEG_FL_RESTART 0x02

/** Enable link */
#define INTELXL_ADMIN_AUTONEG_FL_ENABLE 0x04

/** Admin queue Get Link Status command */
#define INTELXL_ADMIN_LINK 0x0607

/** Admin queue Get Link Status command parameters */
struct intelxl_admin_link_params {
	/** Link status notification */
	uint8_t notify;
	/** Reserved */
	uint8_t reserved_a;
	/** PHY type */
	uint8_t phy;
	/** Link speed */
	uint8_t speed;
	/** Link status */
	uint8_t status;
	/** Reserved */
	uint8_t reserved_b[11];
} __attribute__ (( packed ));

/** Notify driver of link status changes */
#define INTELXL_ADMIN_LINK_NOTIFY 0x03

/** Link is up */
#define INTELXL_ADMIN_LINK_UP 0x01

/** Admin queue command parameters */
union intelxl_admin_params {
	/** Additional data buffer command parameters */
	struct intelxl_admin_buffer_params buffer;
	/** Get Version command parameters */
	struct intelxl_admin_version_params version;
	/** Driver Version command parameters */
	struct intelxl_admin_driver_params driver;
	/** Shutdown command parameters */
	struct intelxl_admin_shutdown_params shutdown;
	/** Get Switch Configuration command parameters */
	struct intelxl_admin_switch_params sw;
	/** Get VSI Parameters command parameters */
	struct intelxl_admin_vsi_params vsi;
	/** Set VSI Promiscuous Modes command parameters */
	struct intelxl_admin_promisc_params promisc;
	/** Restart Autonegotiation command parameters */
	struct intelxl_admin_autoneg_params autoneg;
	/** Get Link Status command parameters */
	struct intelxl_admin_link_params link;
} __attribute__ (( packed ));

/** Admin queue data buffer */
union intelxl_admin_buffer {
	/** Driver Version data buffer */
	struct intelxl_admin_driver_buffer driver;
	/** Get Switch Configuration data buffer */
	struct intelxl_admin_switch_buffer sw;
	/** Get VSI Parameters data buffer */
	struct intelxl_admin_vsi_buffer vsi;
} __attribute__ (( packed ));

/** Admin queue descriptor */
struct intelxl_admin_descriptor {
	/** Flags */
	uint16_t flags;
	/** Opcode */
	uint16_t opcode;
	/** Data length */
	uint16_t len;
	/** Return value */
	uint16_t ret;
	/** Cookie */
	uint32_t cookie;
	/** Reserved */
	uint32_t reserved;
	/** Parameters */
	union intelxl_admin_params params;
} __attribute__ (( packed ));

/** Admin descriptor done */
#define INTELXL_ADMIN_FL_DD 0x0001

/** Admin descriptor contains a completion */
#define INTELXL_ADMIN_FL_CMP 0x0002

/** Admin descriptor completed in error */
#define INTELXL_ADMIN_FL_ERR 0x0004

/** Admin descriptor uses data buffer for command parameters */
#define INTELXL_ADMIN_FL_RD 0x0400

/** Admin descriptor uses data buffer */
#define INTELXL_ADMIN_FL_BUF 0x1000

/** Admin queue */
struct intelxl_admin {
	/** Descriptors */
	struct intelxl_admin_descriptor *desc;
	/** Queue index */
	unsigned int index;

	/** Register block */
	unsigned int reg;
	/** Data buffer */
	union intelxl_admin_buffer *buffer;
};

/**
 * Initialise admin queue
 *
 * @v admin		Admin queue
 * @v reg		Register block
 */
static inline __attribute__ (( always_inline )) void
intelxl_init_admin ( struct intelxl_admin *admin, unsigned int reg ) {

	admin->reg = reg;
}

/** Number of admin queue descriptors */
#define INTELXL_ADMIN_NUM_DESC 4

/** Maximum time to wait for an admin request to complete */
#define INTELXL_ADMIN_MAX_WAIT_MS 100

/** Admin queue API major version */
#define INTELXL_ADMIN_API_MAJOR 1

/******************************************************************************
 *
 * Transmit and receive queue context
 *
 ******************************************************************************
 */

/** CMLAN Context Data Register */
#define INTELXL_PFCM_LANCTXDATA(x) ( 0x10c100 + ( 0x80 * (x) ) )

/** CMLAN Context Control Register */
#define INTELXL_PFCM_LANCTXCTL 0x10c300
#define INTELXL_PFCM_LANCTXCTL_QUEUE_NUM(x) \
	( (x) << 0 )					/**< Queue number */
#define INTELXL_PFCM_LANCTXCTL_SUB_LINE(x) \
	( (x) << 12 )					/**< Sub-line */
#define INTELXL_PFCM_LANCTXCTL_TYPE(x) \
	( (x) << 15 )					/**< Queue type */
#define INTELXL_PFCM_LANCTXCTL_TYPE_RX \
	INTELXL_PFCM_LANCTXCTL_TYPE ( 0x0 )		/**< RX queue type */
#define INTELXL_PFCM_LANCTXCTL_TYPE_TX \
	INTELXL_PFCM_LANCTXCTL_TYPE ( 0x1 )		/**< TX queue type */
#define INTELXL_PFCM_LANCTXCTL_OP_CODE(x) \
	( (x) << 17 )					/**< Op code */
#define INTELXL_PFCM_LANCTXCTL_OP_CODE_READ \
	INTELXL_PFCM_LANCTXCTL_OP_CODE ( 0x0 )		/**< Read context */
#define INTELXL_PFCM_LANCTXCTL_OP_CODE_WRITE \
	INTELXL_PFCM_LANCTXCTL_OP_CODE ( 0x1 )		/**< Write context */

/** CMLAN Context Status Register */
#define INTELXL_PFCM_LANCTXSTAT 0x10c380
#define INTELXL_PFCM_LANCTXSTAT_DONE   0x00000001UL	/**< Complete */

/** Queue context line */
struct intelxl_context_line {
	/** Raw data */
	uint32_t raw[4];
} __attribute__ (( packed ));

/** Transmit queue context */
struct intelxl_context_tx {
	/** Head pointer */
	uint16_t head;
	/** Flags */
	uint16_t flags;
	/** Base address */
	uint64_t base;
	/** Reserved */
	uint8_t reserved_a[8];
	/** Queue count */
	uint16_t count;
	/** Reserved */
	uint8_t reserved_b[100];
	/** Queue set */
	uint16_t qset;
	/** Reserved */
	uint8_t reserved_c[4];
} __attribute__ (( packed ));

/** New transmit queue context */
#define INTELXL_CTX_TX_FL_NEW 0x4000

/** Transmit queue base address */
#define INTELXL_CTX_TX_BASE( base ) ( (base) >> 7 )

/** Transmit queue count */
#define INTELXL_CTX_TX_COUNT( count ) ( (count) << 1 )

/** Transmit queue set */
#define INTELXL_CTX_TX_QSET( qset) ( (qset) << 4 )

/** Receive queue context */
struct intelxl_context_rx {
	/** Head pointer */
	uint16_t head;
	/** Reserved */
	uint8_t reserved_a[2];
	/** Base address and queue count */
	uint64_t base_count;
	/** Data buffer length */
	uint16_t len;
	/** Flags */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved_b[7];
	/** Maximum frame size */
	uint16_t mfs;
} __attribute__ (( packed ));

/** Receive queue base address and queue count */
#define INTELXL_CTX_RX_BASE_COUNT( base, count ) \
	( ( (base) >> 7 ) | ( ( ( uint64_t ) (count) ) << 57 ) )

/** Receive queue data buffer length */
#define INTELXL_CTX_RX_LEN( len ) ( (len) >> 1 )

/** Strip CRC from received packets */
#define INTELXL_CTX_RX_FL_CRCSTRIP 0x20

/** Receive queue maximum frame size */
#define INTELXL_CTX_RX_MFS( mfs ) ( (mfs) >> 2 )

/** Maximum time to wait for a context operation to complete */
#define INTELXL_CTX_MAX_WAIT_MS 100

/** Time to wait for a queue to become enabled */
#define INTELXL_QUEUE_ENABLE_DELAY_US 20

/** Time to wait for a transmit queue to become pre-disabled */
#define INTELXL_QUEUE_PRE_DISABLE_DELAY_US 400

/** Maximum time to wait for a queue to become disabled */
#define INTELXL_QUEUE_DISABLE_MAX_WAIT_MS 1000

/******************************************************************************
 *
 * Transmit and receive descriptors
 *
 ******************************************************************************
 */

/** Global Transmit Queue Head register */
#define INTELXL_QTX_HEAD(x) ( 0x0e4000 + ( 0x4 * (x) ) )

/** Global Transmit Pre Queue Disable register */
#define INTELXL_GLLAN_TXPRE_QDIS(x) ( 0x0e6500 + ( 0x4 * ( (x) / 0x80 ) ) )
#define INTELXL_GLLAN_TXPRE_QDIS_QINDX(x) \
	( (x) << 0 )					/**< Queue index */
#define INTELXL_GLLAN_TXPRE_QDIS_SET_QDIS \
	0x40000000UL					/**< Set disable */
#define INTELXL_GLLAN_TXPRE_QDIS_CLEAR_QDIS				\
	0x80000000UL					/**< Clear disable */

/** Global Transmit Queue register block */
#define INTELXL_QTX(x) ( 0x100000 + ( 0x4 * (x) ) )

/** Global Receive Queue register block */
#define INTELXL_QRX(x) ( 0x120000 + ( 0x4 * (x) ) )

/** Queue Enable Register (offset) */
#define INTELXL_QXX_ENA 0x0000
#define INTELXL_QXX_ENA_REQ		0x00000001UL	/**< Enable request */
#define INTELXL_QXX_ENA_STAT		0x00000004UL	/**< Enabled status */

/** Queue Control Register (offset) */
#define INTELXL_QXX_CTL 0x4000
#define INTELXL_QXX_CTL_PFVF_Q(x)	( (x) << 0 )	/**< PF/VF queue */
#define INTELXL_QXX_CTL_PFVF_Q_PF \
	INTELXL_QXX_CTL_PFVF_Q ( 0x2 )			/**< PF queue */
#define INTELXL_QXX_CTL_PFVF_PF_INDX(x)	( (x) << 2 )	/**< PF index */

/** Queue Tail Pointer Register (offset) */
#define INTELXL_QXX_TAIL 0x8000

/** Transmit data descriptor */
struct intelxl_tx_data_descriptor {
	/** Buffer address */
	uint64_t address;
	/** Flags */
	uint32_t flags;
	/** Length */
	uint32_t len;
} __attribute__ (( packed ));

/** Transmit data descriptor type */
#define INTELXL_TX_DATA_DTYP 0x0

/** Transmit data descriptor end of packet */
#define INTELXL_TX_DATA_EOP 0x10

/** Transmit data descriptor report status */
#define INTELXL_TX_DATA_RS 0x20

/** Transmit data descriptor pretty please
 *
 * This bit is completely missing from older versions of the XL710
 * datasheet.  Later versions describe it innocuously as "reserved,
 * must be 1".  Without this bit, everything will appear to work (up
 * to and including the port "transmit good octets" counter), but no
 * packet will actually be sent.
 */
#define INTELXL_TX_DATA_JFDI 0x40

/** Transmit data descriptor length */
#define INTELXL_TX_DATA_LEN( len ) ( (len) << 2 )

/** Transmit writeback descriptor */
struct intelxl_tx_writeback_descriptor {
	/** Reserved */
	uint8_t reserved_a[8];
	/** Flags */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved_b[7];
} __attribute__ (( packed ));

/** Transmit writeback descriptor complete */
#define INTELXL_TX_WB_FL_DD 0x01

/** Receive data descriptor */
struct intelxl_rx_data_descriptor {
	/** Buffer address */
	uint64_t address;
	/** Flags */
	uint32_t flags;
	/** Reserved */
	uint8_t reserved[4];
} __attribute__ (( packed ));

/** Receive writeback descriptor */
struct intelxl_rx_writeback_descriptor {
	/** Reserved */
	uint8_t reserved[8];
	/** Flags */
	uint32_t flags;
	/** Length */
	uint32_t len;
} __attribute__ (( packed ));

/** Receive writeback descriptor complete */
#define INTELXL_RX_WB_FL_DD 0x00000001UL

/** Receive writeback descriptor error */
#define INTELXL_RX_WB_FL_RXE 0x00080000UL

/** Receive writeback descriptor length */
#define INTELXL_RX_WB_LEN(len) ( ( (len) >> 6 ) & 0x3fff )

/** Packet descriptor */
union intelxl_descriptor {
	/** Transmit data descriptor */
	struct intelxl_tx_data_descriptor tx;
	/** Transmit writeback descriptor */
	struct intelxl_tx_writeback_descriptor tx_wb;
	/** Receive data descriptor */
	struct intelxl_rx_data_descriptor rx;
	/** Receive writeback descriptor */
	struct intelxl_rx_writeback_descriptor rx_wb;
};

/** Descriptor ring */
struct intelxl_ring {
	/** Descriptors */
	union intelxl_descriptor *desc;
	/** Producer index */
	unsigned int prod;
	/** Consumer index */
	unsigned int cons;

	/** Register block */
	unsigned int reg;
	/** Length (in bytes) */
	size_t len;
	/** Program queue context
	 *
	 * @v intelxl		Intel device
	 * @v address		Descriptor ring base address
	 */
	int ( * context ) ( struct intelxl_nic *intelxl, physaddr_t address );
};

/**
 * Initialise descriptor ring
 *
 * @v ring		Descriptor ring
 * @v count		Number of descriptors
 * @v context		Method to program queue context
 */
static inline __attribute__ (( always_inline)) void
intelxl_init_ring ( struct intelxl_ring *ring, unsigned int count,
		    int ( * context ) ( struct intelxl_nic *intelxl,
					physaddr_t address ) ) {

	ring->len = ( count * sizeof ( ring->desc[0] ) );
	ring->context = context;
}

/** Number of transmit descriptors */
#define INTELXL_TX_NUM_DESC 16

/** Transmit descriptor ring maximum fill level */
#define INTELXL_TX_FILL ( INTELXL_TX_NUM_DESC - 1 )

/** Number of receive descriptors
 *
 * In PXE mode (i.e. able to post single receive descriptors), 8
 * descriptors is the only permitted value covering all possible
 * numbers of PFs.
 */
#define INTELXL_RX_NUM_DESC 8

/** Receive descriptor ring fill level */
#define INTELXL_RX_FILL ( INTELXL_RX_NUM_DESC - 1 )

/******************************************************************************
 *
 * Top level
 *
 ******************************************************************************
 */

/** PF Interrupt Zero Dynamic Control Register */
#define INTELXL_PFINT_DYN_CTL0 0x038480
#define INTELXL_PFINT_DYN_CTL0_INTENA	0x00000001UL	/**< Enable */
#define INTELXL_PFINT_DYN_CTL0_CLEARPBA	0x00000002UL	/**< Acknowledge */
#define INTELXL_PFINT_DYN_CTL0_INTENA_MASK 0x80000000UL	/**< Ignore enable */

/** PF Interrupt Zero Linked List Register */
#define INTELXL_PFINT_LNKLST0 0x038500
#define INTELXL_PFINT_LNKLST0_FIRSTQ_INDX(x) \
	( (x) << 0 )					/**< Queue index */
#define INTELXL_PFINT_LNKLST0_FIRSTQ_INDX_NONE \
	INTELXL_PFINT_LNKLST0_FIRSTQ_INDX ( 0x7ff )	/**< End of list */
#define INTELXL_PFINT_LNKLST0_FIRSTQ_TYPE(x) \
	( (x) << 11 )					/**< Queue type */
#define INTELXL_PFINT_LNKLST0_FIRSTQ_TYPE_RX \
	INTELXL_PFINT_LNKLST0_FIRSTQ_TYPE ( 0x0 )	/**< Receive queue */
#define INTELXL_PFINT_LNKLST0_FIRSTQ_TYPE_TX \
	INTELXL_PFINT_LNKLST0_FIRSTQ_TYPE ( 0x1 )	/**< Transmit queue */

/** PF Interrupt Zero Cause Enablement Register */
#define INTELXL_PFINT_ICR0_ENA 0x038800
#define INTELXL_PFINT_ICR0_ENA_ADMINQ	0x40000000UL	/**< Admin event */

/** Receive Queue Interrupt Cause Control Register */
#define INTELXL_QINT_RQCTL(x) ( 0x03a000 + ( 0x4 * (x) ) )
#define INTELXL_QINT_RQCTL_NEXTQ_INDX(x) ( (x) << 16 )	/**< Queue index */
#define INTELXL_QINT_RQCTL_NEXTQ_INDX_NONE \
	INTELXL_QINT_RQCTL_NEXTQ_INDX ( 0x7ff )		/**< End of list */
#define INTELXL_QINT_RQCTL_NEXTQ_TYPE(x) ( (x) << 27 )	/**< Queue type */
#define INTELXL_QINT_RQCTL_NEXTQ_TYPE_RX \
	INTELXL_QINT_RQCTL_NEXTQ_TYPE ( 0x0 )		/**< Receive queue */
#define INTELXL_QINT_RQCTL_NEXTQ_TYPE_TX \
	INTELXL_QINT_RQCTL_NEXTQ_TYPE ( 0x1 )		/**< Transmit queue */
#define INTELXL_QINT_RQCTL_CAUSE_ENA	0x40000000UL	/**< Enable */

/** Transmit Queue Interrupt Cause Control Register */
#define INTELXL_QINT_TQCTL(x) ( 0x03c000 + ( 0x4 * (x) ) )
#define INTELXL_QINT_TQCTL_NEXTQ_INDX(x) ( (x) << 16 )	/**< Queue index */
#define INTELXL_QINT_TQCTL_NEXTQ_INDX_NONE \
	INTELXL_QINT_TQCTL_NEXTQ_INDX ( 0x7ff )		/**< End of list */
#define INTELXL_QINT_TQCTL_NEXTQ_TYPE(x) ( (x) << 27 )	/**< Queue type */
#define INTELXL_QINT_TQCTL_NEXTQ_TYPE_RX \
	INTELXL_QINT_TQCTL_NEXTQ_TYPE ( 0x0 )		/**< Receive queue */
#define INTELXL_QINT_TQCTL_NEXTQ_TYPE_TX \
	INTELXL_QINT_TQCTL_NEXTQ_TYPE ( 0x1 )		/**< Transmit queue */
#define INTELXL_QINT_TQCTL_CAUSE_ENA	0x40000000UL	/**< Enable */

/** PF Control Register */
#define INTELXL_PFGEN_CTRL 0x092400
#define INTELXL_PFGEN_CTRL_PFSWR	0x00000001UL	/**< Software Reset */

/** Time to delay for device reset, in milliseconds */
#define INTELXL_RESET_DELAY_MS 100

/** PF Queue Allocation Register */
#define INTELXL_PFLAN_QALLOC 0x1c0400
#define INTELXL_PFLAN_QALLOC_FIRSTQ(x) \
	( ( (x) >> 0 ) & 0x7ff )			/**< First queue */
#define INTELXL_PFLAN_QALLOC_LASTQ(x) \
	( ( (x) >> 16 ) & 0x7ff )			/**< Last queue */

/** PF LAN Port Number Register */
#define INTELXL_PFGEN_PORTNUM 0x1c0480
#define INTELXL_PFGEN_PORTNUM_PORT_NUM(x) \
	( ( (x) >> 0 ) & 0x3 )				/**< Port number */

/** Port MAC Address Low Register */
#define INTELXL_PRTGL_SAL 0x1e2120

/** Port MAC Address High Register */
#define INTELXL_PRTGL_SAH 0x1e2140
#define INTELXL_PRTGL_SAH_MFS_GET(x)	( (x) >> 16 )	/**< Max frame size */
#define INTELXL_PRTGL_SAH_MFS_SET(x)	( (x) << 16 )	/**< Max frame size */

/** Receive address */
union intelxl_receive_address {
	struct {
		uint32_t low;
		uint32_t high;
	} __attribute__ (( packed )) reg;
	uint8_t raw[ETH_ALEN];
};

/** An Intel 40Gigabit network card */
struct intelxl_nic {
	/** Registers */
	void *regs;
	/** Maximum frame size */
	size_t mfs;

	/** Physical function number */
	unsigned int pf;
	/** Absolute queue number base */
	unsigned int base;
	/** Port number */
	unsigned int port;
	/** Queue number */
	unsigned int queue;
	/** Virtual Station Interface switching element ID */
	unsigned int vsi;
	/** Queue set handle */
	unsigned int qset;

	/** Admin command queue */
	struct intelxl_admin command;
	/** Admin event queue */
	struct intelxl_admin event;

	/** Transmit descriptor ring */
	struct intelxl_ring tx;
	/** Receive descriptor ring */
	struct intelxl_ring rx;
	/** Receive I/O buffers */
	struct io_buffer *rx_iobuf[INTELXL_RX_NUM_DESC];
};

#endif /* _INTELXL_H */
