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
#include <ipxe/pcimsix.h>
#include <ipxe/dma.h>

struct intelxl_nic;

/** BAR size */
#define INTELXL_BAR_SIZE 0x200000

/** Alignment
 *
 * No data structure requires greater than 256 byte alignment.
 */
#define INTELXL_ALIGN 256

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

/** Admin queue register offsets
 *
 * The physical and virtual function register maps have no discernible
 * relationship.
 */
struct intelxl_admin_offsets {
	/** Base Address Low Register offset */
	unsigned int bal;
	/** Base Address High Register offset */
	unsigned int bah;
	/** Length Register offset */
	unsigned int len;
	/** Head Register offset */
	unsigned int head;
	/** Tail Register offset */
	unsigned int tail;
};

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

/** Admin queue Manage MAC Address Read command */
#define INTELXL_ADMIN_MAC_READ 0x0107

/** Admin queue Manage MAC Address Read command parameters */
struct intelxl_admin_mac_read_params {
	/** Valid addresses */
	uint8_t valid;
	/** Reserved */
	uint8_t reserved[15];
} __attribute__ (( packed ));

/** LAN MAC address is valid */
#define INTELXL_ADMIN_MAC_READ_VALID_LAN 0x10

/** Admin queue Manage MAC Address Read data buffer */
struct intelxl_admin_mac_read_buffer {
	/** Physical function MAC address */
	uint8_t pf[ETH_ALEN];
	/** Reserved */
	uint8_t reserved[ETH_ALEN];
	/** Port MAC address */
	uint8_t port[ETH_ALEN];
	/** Physical function wake-on-LAN MAC address */
	uint8_t wol[ETH_ALEN];
} __attribute__ (( packed ));

/** Admin queue Manage MAC Address Write command */
#define INTELXL_ADMIN_MAC_WRITE 0x0108

/** Admin queue Manage MAC Address Write command parameters */
struct intelxl_admin_mac_write_params {
	/** Reserved */
	uint8_t reserved_a[1];
	/** Write type */
	uint8_t type;
	/** MAC address first 16 bits, byte-swapped */
	uint16_t high;
	/** MAC address last 32 bits, byte-swapped */
	uint32_t low;
	/** Reserved */
	uint8_t reserved_b[8];
} __attribute__ (( packed ));

/** Admin queue Clear PXE Mode command */
#define INTELXL_ADMIN_CLEAR_PXE 0x0110

/** Admin queue Clear PXE Mode command parameters */
struct intelxl_admin_clear_pxe_params {
	/** Magic value */
	uint8_t magic;
	/** Reserved */
	uint8_t reserved[15];
} __attribute__ (( packed ));

/** Clear PXE Mode magic value */
#define INTELXL_ADMIN_CLEAR_PXE_MAGIC 0x02

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

/** Admin queue Set MAC Configuration command */
#define INTELXL_ADMIN_MAC_CONFIG 0x0603

/** Admin queue Set MAC Configuration command parameters */
struct intelxl_admin_mac_config_params {
	/** Maximum frame size */
	uint16_t mfs;
	/** Flags */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved[13];
} __attribute__ (( packed ));

/** Append CRC on transmit */
#define INTELXL_ADMIN_MAC_CONFIG_FL_CRC 0x04

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
	/** Manage MAC Address Read command parameters */
	struct intelxl_admin_mac_read_params mac_read;
	/** Manage MAC Address Write command parameters */
	struct intelxl_admin_mac_write_params mac_write;
	/** Clear PXE Mode command parameters */
	struct intelxl_admin_clear_pxe_params pxe;
	/** Get Switch Configuration command parameters */
	struct intelxl_admin_switch_params sw;
	/** Get VSI Parameters command parameters */
	struct intelxl_admin_vsi_params vsi;
	/** Set VSI Promiscuous Modes command parameters */
	struct intelxl_admin_promisc_params promisc;
	/** Set MAC Configuration command parameters */
	struct intelxl_admin_mac_config_params mac_config;
	/** Restart Autonegotiation command parameters */
	struct intelxl_admin_autoneg_params autoneg;
	/** Get Link Status command parameters */
	struct intelxl_admin_link_params link;
} __attribute__ (( packed ));

/** Maximum size of a data buffer */
#define INTELXL_ADMIN_BUFFER_SIZE 0x1000

/** Admin queue data buffer */
union intelxl_admin_buffer {
	/** Driver Version data buffer */
	struct intelxl_admin_driver_buffer driver;
	/** Manage MAC Address Read data buffer */
	struct intelxl_admin_mac_read_buffer mac_read;
	/** Get Switch Configuration data buffer */
	struct intelxl_admin_switch_buffer sw;
	/** Get VSI Parameters data buffer */
	struct intelxl_admin_vsi_buffer vsi;
	/** Maximum buffer size */
	uint8_t pad[INTELXL_ADMIN_BUFFER_SIZE];
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
	/** Opaque cookie */
	uint32_t cookie;
	/** Reserved */
	uint8_t reserved[4];
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

/** Error: attempt to create something that already exists */
#define INTELXL_ADMIN_EEXIST 13

/** Admin queue */
struct intelxl_admin {
	/** Descriptors */
	struct intelxl_admin_descriptor *desc;
	/** Data buffers */
	union intelxl_admin_buffer *buf;
	/** DMA mapping */
	struct dma_mapping map;
	/** Queue index */
	unsigned int index;

	/** Register block base */
	unsigned int base;
	/** Register offsets */
	const struct intelxl_admin_offsets *regs;
};

/**
 * Initialise admin queue
 *
 * @v admin		Admin queue
 * @v base		Register block base
 * @v regs		Register offsets
 */
static inline __attribute__ (( always_inline )) void
intelxl_init_admin ( struct intelxl_admin *admin, unsigned int base,
		     const struct intelxl_admin_offsets *regs ) {

	admin->base = base;
	admin->regs = regs;
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
	/** Reserved */
	uint8_t reserved_c[8];
} __attribute__ (( packed ));

/** Receive queue base address and queue count */
#define INTELXL_CTX_RX_BASE_COUNT( base, count ) \
	( ( (base) >> 7 ) | ( ( ( uint64_t ) (count) ) << 57 ) )

/** Receive queue data buffer length */
#define INTELXL_CTX_RX_LEN( len ) ( (len) >> 1 )

/** Use 32-byte receive descriptors */
#define INTELXL_CTX_RX_FL_DSIZE 0x10

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

/** Transmit descriptor */
union intelxl_tx_descriptor {
	/** Transmit data descriptor */
	struct intelxl_tx_data_descriptor data;
	/** Transmit writeback descriptor */
	struct intelxl_tx_writeback_descriptor wb;
};

/** Receive data descriptor */
struct intelxl_rx_data_descriptor {
	/** Buffer address */
	uint64_t address;
	/** Flags */
	uint32_t flags;
	/** Reserved */
	uint8_t reserved[20];
} __attribute__ (( packed ));

/** Receive writeback descriptor */
struct intelxl_rx_writeback_descriptor {
	/** Reserved */
	uint8_t reserved_a[2];
	/** VLAN tag */
	uint16_t vlan;
	/** Reserved */
	uint8_t reserved_b[4];
	/** Flags */
	uint32_t flags;
	/** Length */
	uint32_t len;
	/** Reserved */
	uint8_t reserved_c[16];
} __attribute__ (( packed ));

/** Receive writeback descriptor complete */
#define INTELXL_RX_WB_FL_DD 0x00000001UL

/** Receive writeback descriptor VLAN tag present */
#define INTELXL_RX_WB_FL_VLAN 0x00000004UL

/** Receive writeback descriptor error */
#define INTELXL_RX_WB_FL_RXE 0x00080000UL

/** Receive writeback descriptor length */
#define INTELXL_RX_WB_LEN(len) ( ( (len) >> 6 ) & 0x3fff )

/** Packet descriptor */
union intelxl_rx_descriptor {
	/** Receive data descriptor */
	struct intelxl_rx_data_descriptor data;
	/** Receive writeback descriptor */
	struct intelxl_rx_writeback_descriptor wb;
};

/** Descriptor ring */
struct intelxl_ring {
	/** Descriptors */
	union {
		/** Transmit descriptors */
		union intelxl_tx_descriptor *tx;
		/** Receive descriptors */
		union intelxl_rx_descriptor *rx;
		/** Raw data */
		void *raw;
	} desc;
	/** Descriptor ring DMA mapping */
	struct dma_mapping map;
	/** Producer index */
	unsigned int prod;
	/** Consumer index */
	unsigned int cons;

	/** Register block */
	unsigned int reg;
	/** Tail register */
	unsigned int tail;
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
 * @v len		Length of a single descriptor
 * @v context		Method to program queue context
 */
static inline __attribute__ (( always_inline)) void
intelxl_init_ring ( struct intelxl_ring *ring, unsigned int count, size_t len,
		    int ( * context ) ( struct intelxl_nic *intelxl,
					physaddr_t address ) ) {

	ring->len = ( count * len );
	ring->context = context;
}

/** Number of transmit descriptors
 *
 * Chosen to exceed the receive ring fill level, in order to avoid
 * running out of transmit descriptors when sending TCP ACKs.
 */
#define INTELXL_TX_NUM_DESC 64

/** Transmit descriptor ring maximum fill level */
#define INTELXL_TX_FILL ( INTELXL_TX_NUM_DESC - 1 )

/** Number of receive descriptors
 *
 * Must be a multiple of 32 and greater than or equal to 64.
 */
#define INTELXL_RX_NUM_DESC 64

/** Receive descriptor ring fill level
 *
 * Must be a multiple of 8 and greater than 8.
 */
#define INTELXL_RX_FILL 16

/** Maximum packet length (excluding CRC) */
#define INTELXL_MAX_PKT_LEN ( 9728 - 4 /* CRC */ )

/******************************************************************************
 *
 * Top level
 *
 ******************************************************************************
 */

/** PF Interrupt Zero Dynamic Control Register */
#define INTELXL_PFINT_DYN_CTL0 0x038480
#define INTELXL_INT_DYN_CTL_INTENA	0x00000001UL	/**< Enable */
#define INTELXL_INT_DYN_CTL_CLEARPBA	0x00000002UL	/**< Acknowledge */
#define INTELXL_INT_DYN_CTL_INTENA_MASK 0x80000000UL	/**< Ignore enable */

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

/** Function Requester ID Information Register */
#define INTELXL_PFFUNC_RID 0x09c000
#define INTELXL_PFFUNC_RID_FUNC_NUM(x) \
	( ( (x) >> 0 ) & 0x7 )				/**< Function number */

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

/** MSI-X interrupt */
struct intelxl_msix {
	/** PCI capability */
	struct pci_msix cap;
	/** MSI-X dummy interrupt target */
	uint32_t msg;
	/** DMA mapping for dummy interrupt target */
	struct dma_mapping map;
};

/** MSI-X interrupt vector */
#define INTELXL_MSIX_VECTOR 0

/** An Intel 40 Gigabit network card */
struct intelxl_nic {
	/** Registers */
	void *regs;
	/** DMA device */
	struct dma_device *dma;
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
	/** Transmit element ID */
	uint32_t teid;
	/** Device capabilities */
	uint32_t caps;
	/** Interrupt control register */
	unsigned int intr;
	/** PCI Express capability offset */
	unsigned int exp;
	/** MSI-X interrupt */
	struct intelxl_msix msix;

	/** Admin command queue */
	struct intelxl_admin command;
	/** Admin event queue */
	struct intelxl_admin event;

	/** Current VF opcode */
	unsigned int vopcode;

	/** Transmit descriptor ring */
	struct intelxl_ring tx;
	/** Receive descriptor ring */
	struct intelxl_ring rx;
	/** Receive I/O buffers */
	struct io_buffer *rx_iobuf[INTELXL_RX_NUM_DESC];

	/**
	 * Handle admin event
	 *
	 * @v netdev		Network device
	 * @v evt		Event descriptor
	 * @v buf		Data buffer
	 */
	void ( * handle ) ( struct net_device *netdev,
			    struct intelxl_admin_descriptor *evt,
			    union intelxl_admin_buffer *buf );
};

extern const struct intelxl_admin_offsets intelxl_admin_offsets;

extern int intelxl_msix_enable ( struct intelxl_nic *intelxl,
				 struct pci_device *pci,
				 unsigned int vector );
extern void intelxl_msix_disable ( struct intelxl_nic *intelxl,
				   struct pci_device *pci,
				   unsigned int vector  );
extern struct intelxl_admin_descriptor *
intelxl_admin_command_descriptor ( struct intelxl_nic *intelxl );
extern union intelxl_admin_buffer *
intelxl_admin_command_buffer ( struct intelxl_nic *intelxl );
extern int intelxl_admin_command ( struct intelxl_nic *intelxl );
extern int intelxl_admin_clear_pxe ( struct intelxl_nic *intelxl );
extern int intelxl_admin_mac_config ( struct intelxl_nic *intelxl );
extern void intelxl_poll_admin ( struct net_device *netdev );
extern int intelxl_open_admin ( struct intelxl_nic *intelxl );
extern void intelxl_reopen_admin ( struct intelxl_nic *intelxl );
extern void intelxl_close_admin ( struct intelxl_nic *intelxl );
extern int intelxl_alloc_ring ( struct intelxl_nic *intelxl,
				struct intelxl_ring *ring );
extern void intelxl_free_ring ( struct intelxl_nic *intelxl,
				struct intelxl_ring *ring );
extern int intelxl_create_ring ( struct intelxl_nic *intelxl,
				 struct intelxl_ring *ring );
extern void intelxl_destroy_ring ( struct intelxl_nic *intelxl,
				   struct intelxl_ring *ring );
extern void intelxl_empty_rx ( struct intelxl_nic *intelxl );
extern int intelxl_transmit ( struct net_device *netdev,
			      struct io_buffer *iobuf );
extern void intelxl_poll ( struct net_device *netdev );

#endif /* _INTELXL_H */
