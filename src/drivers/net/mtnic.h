/*
 * Copyright (c) 2007 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

FILE_LICENCE ( GPL2_ONLY );

#ifndef H_MTNIC_IF_DEFS_H
#define H_MTNIC_IF_DEFS_H



/*
* Device setup
*/
#define MTNIC_MAX_PORTS		2
#define MTNIC_PORT1		0
#define MTNIC_PORT2		1
#define NUM_TX_RINGS		1
#define NUM_RX_RINGS		1
#define NUM_CQS 		(NUM_RX_RINGS + NUM_TX_RINGS)
#define GO_BIT_TIMEOUT		6000
#define TBIT_RETRIES		100
#define UNITS_BUFFER_SIZE 	8 /* can be configured to 4/8/16 */
#define MAX_GAP_PROD_CONS 	( UNITS_BUFFER_SIZE / 4 )
#define ETH_DEF_LEN		1540          /* 40 bytes used by the card */
#define ETH_FCS_LEN		14
#define DEF_MTU 		ETH_DEF_LEN + ETH_FCS_LEN
#define DEF_IOBUF_SIZE 		ETH_DEF_LEN

#define MAC_ADDRESS_SIZE 	6
#define NUM_EQES 		16
#define ROUND_TO_CHECK		0x400

#define DELAY_LINK_CHECK	300
#define CHECK_LINK_TIMES	7


#define XNOR(x,y)		(!(x) == !(y))
#define dma_addr_t 		unsigned long
#define PAGE_SIZE		4096
#define PAGE_MASK		(PAGE_SIZE - 1)
#define MTNIC_MAILBOX_SIZE	PAGE_SIZE




/* BITOPS */
#define MTNIC_BC_OFF(bc) ((bc) >> 8)
#define MTNIC_BC_SZ(bc) ((bc) & 0xff)
#define MTNIC_BC_ONES(size) (~((int)0x80000000 >> (31 - size)))
#define MTNIC_BC_MASK(bc) \
	(MTNIC_BC_ONES(MTNIC_BC_SZ(bc)) << MTNIC_BC_OFF(bc))
#define MTNIC_BC_VAL(val, bc) \
	(((val) & MTNIC_BC_ONES(MTNIC_BC_SZ(bc))) << MTNIC_BC_OFF(bc))
/*
 * Sub word fields - bit code base extraction/setting etc
 */

/* Encode two values */
#define MTNIC_BC(off, size) ((off << 8) | (size & 0xff))

/* Get value of field 'bc' from 'x' */
#define MTNIC_BC_GET(x, bc) \
	(((x) >> MTNIC_BC_OFF(bc)) & MTNIC_BC_ONES(MTNIC_BC_SZ(bc)))

/* Set value of field 'bc' of 'x' to 'val' */
#define MTNIC_BC_SET(x, val, bc) \
	((x) = ((x) & ~MTNIC_BC_MASK(bc)) | MTNIC_BC_VAL(val, bc))

/* Like MTNIC_BC_SET, except the previous value is assumed to be 0 */
#define MTNIC_BC_PUT(x, val, bc) ((x) |= MTNIC_BC_VAL(val, bc))



/*
 * Device constants
 */
typedef enum mtnic_if_cmd {
	/* NIC commands: */
	MTNIC_IF_CMD_QUERY_FW  = 0x004, /* query FW (size, version, etc) */
	MTNIC_IF_CMD_MAP_FW    = 0xfff, /* map pages for FW image */
	MTNIC_IF_CMD_RUN_FW    = 0xff6, /* run the FW */
	MTNIC_IF_CMD_QUERY_CAP = 0x001, /* query MTNIC capabilities */
	MTNIC_IF_CMD_MAP_PAGES = 0x002, /* map physical pages to HW */
	MTNIC_IF_CMD_OPEN_NIC  = 0x003, /* run the firmware */
	MTNIC_IF_CMD_CONFIG_RX = 0x005, /* general receive configuration */
	MTNIC_IF_CMD_CONFIG_TX = 0x006, /* general transmit configuration */
	MTNIC_IF_CMD_CONFIG_INT_FREQ = 0x007, /* interrupt timers freq limits */
	MTNIC_IF_CMD_HEART_BEAT = 0x008, /* NOP command testing liveliness */
	MTNIC_IF_CMD_CLOSE_NIC = 0x009, /* release memory and stop the NIC */

	/* Port commands: */
	MTNIC_IF_CMD_CONFIG_PORT_RSS_STEER     = 0x10, /* set RSS mode */
	MTNIC_IF_CMD_SET_PORT_RSS_INDIRECTION  = 0x11, /* set RSS indirection tbl */
	MTNIC_IF_CMD_CONFIG_PORT_PRIO_STEERING = 0x12, /* set PRIORITY mode */
	MTNIC_IF_CMD_CONFIG_PORT_ADDR_STEER    = 0x13, /* set Address steer mode */
	MTNIC_IF_CMD_CONFIG_PORT_VLAN_FILTER   = 0x14, /* configure VLAN filter */
	MTNIC_IF_CMD_CONFIG_PORT_MCAST_FILTER  = 0x15, /* configure mcast filter */
	MTNIC_IF_CMD_ENABLE_PORT_MCAST_FILTER  = 0x16, /* enable/disable */
	MTNIC_IF_CMD_SET_PORT_MTU              = 0x17, /* set port MTU */
	MTNIC_IF_CMD_SET_PORT_PROMISCUOUS_MODE = 0x18, /* enable/disable promisc */
	MTNIC_IF_CMD_SET_PORT_DEFAULT_RING     = 0x19, /* set the default ring */
	MTNIC_IF_CMD_SET_PORT_STATE            = 0x1a, /* set link up/down */
	MTNIC_IF_CMD_DUMP_STAT                 = 0x1b, /* dump statistics */
	MTNIC_IF_CMD_ARM_PORT_STATE_EVENT      = 0x1c, /* arm the port state event */

	/* Ring / Completion queue commands: */
	MTNIC_IF_CMD_CONFIG_CQ            = 0x20,  /* set up completion queue */
	MTNIC_IF_CMD_CONFIG_RX_RING       = 0x21,  /* setup Rx ring */
	MTNIC_IF_CMD_SET_RX_RING_ADDR     = 0x22,  /* set Rx ring filter by address */
	MTNIC_IF_CMD_SET_RX_RING_MCAST    = 0x23,  /* set Rx ring mcast filter */
	MTNIC_IF_CMD_ARM_RX_RING_WM       = 0x24,  /* one-time low-watermark INT */
	MTNIC_IF_CMD_CONFIG_TX_RING       = 0x25,  /* set up Tx ring */
	MTNIC_IF_CMD_ENFORCE_TX_RING_ADDR = 0x26,  /* setup anti spoofing */
	MTNIC_IF_CMD_CONFIG_EQ            = 0x27,  /* config EQ ring */
	MTNIC_IF_CMD_RELEASE_RESOURCE     = 0x28,  /* release internal ref to resource */
}
mtnic_if_cmd_t;


/** selectors for MTNIC_IF_CMD_QUERY_CAP */
typedef enum mtnic_if_caps {
	MTNIC_IF_CAP_MAX_TX_RING_PER_PORT = 0x0,
	MTNIC_IF_CAP_MAX_RX_RING_PER_PORT = 0x1,
	MTNIC_IF_CAP_MAX_CQ_PER_PORT      = 0x2,
	MTNIC_IF_CAP_NUM_PORTS            = 0x3,
	MTNIC_IF_CAP_MAX_TX_DESC          = 0x4,
	MTNIC_IF_CAP_MAX_RX_DESC          = 0x5,
	MTNIC_IF_CAP_MAX_CQES             = 0x6,
	MTNIC_IF_CAP_MAX_TX_SG_ENTRIES    = 0x7,
	MTNIC_IF_CAP_MAX_RX_SG_ENTRIES    = 0x8,
	MTNIC_IF_CAP_MEM_KEY              = 0x9, /* key to mem (after map_pages) */
	MTNIC_IF_CAP_RSS_HASH_TYPE        = 0xa, /* one of mtnic_if_rss_types_t */
	MTNIC_IF_CAP_MAX_PORT_UCAST_ADDR  = 0xc,
	MTNIC_IF_CAP_MAX_RING_UCAST_ADDR  = 0xd, /* only for ADDR steer */
	MTNIC_IF_CAP_MAX_PORT_MCAST_ADDR  = 0xe,
	MTNIC_IF_CAP_MAX_RING_MCAST_ADDR  = 0xf, /* only for ADDR steer */
	MTNIC_IF_CAP_INTA                 = 0x10,
	MTNIC_IF_CAP_BOARD_ID_LOW         = 0x11,
	MTNIC_IF_CAP_BOARD_ID_HIGH        = 0x12,
	MTNIC_IF_CAP_TX_CQ_DB_OFFSET      = 0x13, /* offset in bytes for TX, CQ doorbell record */
	MTNIC_IF_CAP_EQ_DB_OFFSET         = 0x14, /* offset in bytes for EQ doorbell record */

	/* These are per port - using port number from cap modifier field */
	MTNIC_IF_CAP_SPEED                = 0x20,
	MTNIC_IF_CAP_DEFAULT_MAC          = 0x21,
	MTNIC_IF_CAP_EQ_OFFSET            = 0x22,
	MTNIC_IF_CAP_CQ_OFFSET            = 0x23,
	MTNIC_IF_CAP_TX_OFFSET            = 0x24,
	MTNIC_IF_CAP_RX_OFFSET            = 0x25,

} mtnic_if_caps_t;

typedef enum mtnic_if_steer_types {
	MTNIC_IF_STEER_NONE     = 0,
	MTNIC_IF_STEER_PRIORITY = 1,
	MTNIC_IF_STEER_RSS      = 2,
	MTNIC_IF_STEER_ADDRESS  = 3,
} mtnic_if_steer_types_t;

/** types of memory access modes */
typedef enum mtnic_if_memory_types {
	MTNIC_IF_MEM_TYPE_SNOOP = 1,
	MTNIC_IF_MEM_TYPE_NO_SNOOP = 2
} mtnic_if_memory_types_t;


enum {
	MTNIC_HCR_BASE          = 0x1f000,
	MTNIC_HCR_SIZE          = 0x0001c,
	MTNIC_CLR_INT_SIZE      = 0x00008,
};

#define MTNIC_RESET_OFFSET 	0xF0010



/********************************************************************
* Device private data structures
*
* This section contains structures of all device private data:
*	descriptors, rings, CQs, EQ ....
*
*
*********************************************************************/
/*
 * Descriptor format
 */
struct mtnic_ctrl_seg {
	u32 op_own;
#define MTNIC_BIT_DESC_OWN	0x80000000
#define MTNIC_OPCODE_SEND	0xa
	u32 size_vlan;
	u32 flags;
#define MTNIC_BIT_NO_ICRC	0x2
#define MTNIC_BIT_TX_COMP	0xc
	u32 reserved;
};

struct mtnic_data_seg {
	u32 count;
#define MTNIC_INLINE		0x80000000
	u32 mem_type;
#define MTNIC_MEMTYPE_PAD	0x100
	u32 addr_h;
	u32 addr_l;
};

struct mtnic_tx_desc {
	struct mtnic_ctrl_seg ctrl;
	struct mtnic_data_seg data; /* at least one data segment */
};

struct mtnic_rx_desc {
	u16 reserved1;
	u16 next;
	u32 reserved2[3];
	struct mtnic_data_seg data; /* actual number of entries depends on
				* rx ring stride */
};

/*
 * Rings
 */
struct mtnic_rx_db_record {
	u32 count;
};

struct mtnic_ring {
	u32 size; /* REMOVE ____cacheline_aligned_in_smp; *//* number of Rx descs or TXBBs */
	u32 size_mask;
	u16 stride;
	u16 cq; /* index of port CQ associated with this ring */
	u32 prod;
	u32 cons; /* holds the last consumed index */

	/* Buffers */
	u32 buf_size; /* ring buffer size in bytes */
	dma_addr_t dma;
	void *buf;
	struct io_buffer *iobuf[UNITS_BUFFER_SIZE];

	/* Tx only */
	struct mtnic_txcq_db *txcq_db;
	u32 db_offset;

	/* Rx ring only */
	dma_addr_t iobuf_dma;
	struct mtnic_rx_db_record *db;
	dma_addr_t db_dma;
};

/*
 * CQ
 */

struct mtnic_cqe {
	u8 vp; /* VLAN present */
	u8 reserved1[3];
	u32 rss_hash;
	u32 reserved2;
	u16 vlan_prio;
	u16 reserved3;
	u8 flags_h;
	u8 flags_l_rht;
	u8 ipv6_mask;
	u8 enc_bf;
#define MTNIC_BIT_BAD_FCS	0x10
#define MTNIC_OPCODE_ERROR	0x1e
	u32 byte_cnt;
	u16 index;
	u16 chksum;
	u8 reserved4[3];
	u8 op_tr_own;
#define MTNIC_BIT_CQ_OWN	0x80
};


struct mtnic_cq_db_record {
	u32 update_ci;
	u32 cmd_ci;
};

struct mtnic_cq {
	int num; /* CQ number (on attached port) */
	u32 size; /* number of CQEs in CQ */
	u32 last; /* number of CQEs consumed */
	struct mtnic_cq_db_record *db;
	struct net_device *dev;

	dma_addr_t db_dma;
	u8 is_rx;
	u16 ring; /* ring associated with this CQ */
	u32 offset_ind;

	/* CQE ring */
	u32 buf_size; /* ring size in bytes */
	struct mtnic_cqe *buf;
	dma_addr_t dma;
};

/*
 * EQ
 */

struct mtnic_eqe {
	u8 reserved1;
	u8 type;
	u8 reserved2;
	u8 subtype;
	u8 reserved3[3];
	u8 ring_cq;
	u32 reserved4;
	u8 port;
#define MTNIC_MASK_EQE_PORT    MTNIC_BC(4,2)
	u8 reserved5[2];
	u8 syndrome;
	u8 reserved6[15];
	u8 own;
#define MTNIC_BIT_EQE_OWN      0x80
};

struct mtnic_eq {
	u32 size; /* number of EQEs in ring */
	u32 buf_size; /* EQ size in bytes */
	void *buf;
	dma_addr_t dma;
};

enum mtnic_state {
	CARD_DOWN,
	CARD_INITIALIZED,
	CARD_UP,
	CARD_LINK_DOWN,
};

/* FW */
struct mtnic_pages {
	u32 num;
	u32 *buf;
};
struct mtnic_err_buf {
	u64 offset;
	u32 size;
};



struct mtnic_cmd {
	void                    *buf;
	unsigned long           mapping;
	u32                     tbit;
};


struct mtnic_txcq_db {
	u32 reserved1[5];
	u32 send_db;
	u32 reserved2[2];
	u32 cq_arm;
	u32 cq_ci;
};



/*
 * Device private data
 *
 */
struct mtnic {
	struct net_device               *netdev[MTNIC_MAX_PORTS];
	struct mtnic_if_cmd_reg         *hcr;
	struct mtnic_cmd                cmd;
	struct pci_device               *pdev;

	struct mtnic_eq                 eq;
	u32                             *eq_db;

	/* Firmware and board info */
	u64                             fw_ver;
	struct {
		struct mtnic_pages      fw_pages;
		struct mtnic_pages      extra_pages;
		struct mtnic_err_buf    err_buf;
		u16                     ifc_rev;
		u8                      num_ports;
		u64                     mac[MTNIC_MAX_PORTS];
		u16                     cq_offset;
		u16                     tx_offset[MTNIC_MAX_PORTS];
		u16                     rx_offset[MTNIC_MAX_PORTS];
		u32                     mem_type_snoop_be;
		u32                     txcq_db_offset;
		u32                     eq_db_offset;
	} fw;
};





struct mtnic_port {

	struct mtnic                    *mtnic;
	u8                              port;

	enum mtnic_state                state;

	/* TX, RX, CQs, EQ */
	struct mtnic_ring               tx_ring;
	struct mtnic_ring               rx_ring;
	struct mtnic_cq                 cq[NUM_CQS];
	u32                             poll_counter;
	struct net_device               *netdev;


};












/***************************************************************************
 * NIC COMMANDS
 *
 * The section below provides struct definition for commands parameters,
 * and arguments values enumeration.
 *
 * The format used for the struct names is:
 * mtnic_if_<cmd name>_<in|out>_<imm|mbox>
 *
 ***************************************************************************/
/**
 *  Command Register (Command interface)
 */
struct mtnic_if_cmd_reg {
	unsigned long in_param_h;
	u32 in_param_l;
	u32 input_modifier;
	u32 out_param_h;
	u32 out_param_l;
	u32 token;
#define MTNIC_MASK_CMD_REG_TOKEN	 MTNIC_BC(16,32)
	u32 status_go_opcode;
#define MTNIC_MASK_CMD_REG_OPCODE MTNIC_BC(0,16)
#define MTNIC_MASK_CMD_REG_T_BIT  MTNIC_BC(21,1)
#define MTNIC_MASK_CMD_REG_GO_BIT MTNIC_BC(23,1)
#define MTNIC_MASK_CMD_REG_STATUS MTNIC_BC(24,8)
};



/* CMD QUERY_FW */
struct mtnic_if_query_fw_out_mbox {
	u16 fw_pages;   /* Total number of memory pages the device requires */
	u16 rev_maj;
	u16 rev_smin;
	u16 rev_min;
	u16 reserved1;
	u16 ifc_rev;    /* major revision of the command interface */
	u8  ft;
	u8  reserved2[3];
	u32 reserved3[4];
	u64 clr_int_base;
	u32 reserved4[2];
	u64 err_buf_start;
	u32 err_buf_size;
};

/* CMD MTNIC_IF_CMD_QUERY_CAP */
struct mtnic_if_query_cap_in_imm {
	u16 reserved1;
	u8               cap_modifier;   /* a modifier for the particular capability */
	u8               cap_index;      /* the index of the capability queried */
	u32 reserved2;
};

/* CMD OPEN_NIC */
struct mtnic_if_open_nic_in_mbox {
	u16 reserved1;
	u16 mkey; /* number of mem keys for all chip*/
	u32 mkey_entry; /* mem key entries for each key*/
	u8 log_rx_p1; /* log2 rx rings for port1 */
	u8 log_cq_p1; /* log2 cq for port1 */
	u8 log_tx_p1; /* log2 tx rings for port1 */
	u8 steer_p1;  /* port 1 steering mode */
	u16 reserved2;
	u8 log_vlan_p1; /* log2 vlan per rx port1 */
	u8 log_mac_p1;  /* log2 mac per rx port1 */

	u8 log_rx_p2; /* log2 rx rings for port1 */
	u8 log_cq_p2; /* log2 cq for port1 */
	u8 log_tx_p2; /* log2 tx rings for port1 */
	u8 steer_p2;  /* port 1 steering mode */
	u16 reserved3;
	u8 log_vlan_p2; /* log2 vlan per rx port1 */
	u8 log_mac_p2;  /* log2 mac per rx port1 */
};


/* CMD CONFIG_RX */
struct mtnic_if_config_rx_in_imm {
	u16 spkt_size; /* size of small packets interrupts enabled on CQ */
	u16 resp_rcv_pause_frm_mcast_vlan_comp; /* Two flags see MASK below */
	/* Enable response to receive pause frames */
	/* Use VLAN in exact-match multicast checks (see SET_RX_RING_MCAST) */
};

/* CMD CONFIG_TX */
struct mtnic_if_config_send_in_imm {
	u32  enph_gpf; /* Enable PseudoHeader and GeneratePauseFrames flags */
	u32  reserved;
};

/* CMD HEART_BEAT */
struct mtnic_if_heart_beat_out_imm {
	u32 flags; /* several flags */
#define MTNIC_MASK_HEAR_BEAT_INT_ERROR  MTNIC_BC(31,1)
	u32 reserved;
};


/*
 * PORT COMMANDS
 */
/* CMD CONFIG_PORT_VLAN_FILTER */
/* in mbox is a 4K bits mask - bit per VLAN */
struct mtnic_if_config_port_vlan_filter_in_mbox {
	u64 filter[64]; /* vlans[63:0] sit in filter[0], vlans[127:64] sit in filter[1] ..  */
};


/* CMD SET_PORT_MTU */
struct mtnic_if_set_port_mtu_in_imm {
	u16 reserved1;
	u16 mtu;                        /* The MTU of the port in bytes */
	u32 reserved2;
};

/* CMD SET_PORT_DEFAULT_RING */
struct mtnic_if_set_port_default_ring_in_imm {
	u8 reserved1[3];
	u8 ring; /* Index of ring that collects promiscuous traffic */
	u32 reserved2;
};

/* CMD SET_PORT_STATE */
struct mtnic_if_set_port_state_in_imm {
	u32 state; /* if 1 the port state should be up */
#define MTNIC_MASK_CONFIG_PORT_STATE MTNIC_BC(0,1)
	u32 reserved;
};

/* CMD CONFIG_CQ */
struct mtnic_if_config_cq_in_mbox {
	u8           reserved1;
	u8           cq;
	u8           size;        /* Num CQs is 2^size (size <= 22) */
	u8           offset; /* start address of CQE in first page (11:6) */
	u16  tlast;      /* interrupt moderation timer from last completion usec */
	u8      flags;  /* flags */
	u8          int_vector; /* MSI index if MSI is enabled, otherwise reserved */
	u16 reserved2;
	u16 max_cnt;    /* interrupt moderation counter */
	u8          page_size;   /* each mapped page is 2^(12+page_size) bytes */
	u8       reserved4[3];
	u32 db_record_addr_h;  /*physical address of CQ doorbell record */
	u32 db_record_addr_l;  /*physical address of CQ doorbell record */
	u32 page_address[0]; /* 64 bit page addresses of CQ buffer */
};

/* CMD CONFIG_RX_RING */
struct mtnic_if_config_rx_ring_in_mbox {
	u8       reserved1;
	u8       ring;                          /* The ring index (with offset) */
	u8       stride_size;           /* stride and size */
	/* Entry size = 16* (2^stride) bytes */
#define MTNIC_MASK_CONFIG_RX_RING_STRIDE     MTNIC_BC(4,3)
	/* Rx ring size is 2^size entries */
#define MTNIC_MASK_CONFIG_RX_RING_SIZE	      MTNIC_BC(0,4)
	u8       flags;                         /* Bit0 - header separation */
	u8       page_size;                       /* Each mapped page is 2^(12+page_size) bytes */
	u8       reserved2[2];
	u8       cq;                                      /* CQ associated with this ring */
	u32      db_record_addr_h;
	u32      db_record_addr_l;
	u32      page_address[0];/* Array of 2^size 64b page descriptor addresses */
	/* Must hold all Rx descriptors + doorbell record. */
};

/* The modifier for SET_RX_RING_ADDR */
struct mtnic_if_set_rx_ring_modifier {
	u8 reserved;
	u8 port_num;
	u8 index;
	u8 ring;
};

/* CMD SET_RX_RING_ADDR */
struct mtnic_if_set_rx_ring_addr_in_imm {
	u16 mac_47_32;           /* UCAST MAC Address bits 47:32 */
	u16 flags_vlan_id; /* MAC/VLAN flags and vlan id */
#define MTNIC_MASK_SET_RX_RING_ADDR_VLAN_ID MTNIC_BC(0,12)
#define MTNIC_MASK_SET_RX_RING_ADDR_BY_MAC  MTNIC_BC(12,1)
#define MTNIC_MASK_SET_RX_RING_ADDR_BY_VLAN MTNIC_BC(13,1)
	u32 mac_31_0;   /* UCAST MAC Address bits 31:0 */
};

/* CMD CONFIG_TX_RING */
struct mtnic_if_config_send_ring_in_mbox {
	u16 ring;                       /* The ring index (with offset) */
#define MTNIC_MASK_CONFIG_TX_RING_INDEX  MTNIC_BC(0,8)
	u8       size;                          /* Tx ring size is 32*2^size bytes */
#define MTNIC_MASK_CONFIG_TX_RING_SIZE	  MTNIC_BC(0,4)
	u8       reserved;
	u8       page_size;                     /* Each mapped page is 2^(12+page_size) bytes */
	u8       qos_class;                     /* The COS used for this Tx */
	u16 cq;                         /* CQ associated with this ring */
#define MTNIC_MASK_CONFIG_TX_CQ_INDEX	  MTNIC_BC(0,8)
	u32 page_address[0]; /* 64 bit page addresses of descriptor buffer. */
	/* The buffer must accommodate all Tx descriptors */
};

/* CMD CONFIG_EQ */
struct mtnic_if_config_eq_in_mbox {
	u8 reserved1;
	u8 int_vector; /* MSI index if MSI enabled; otherwise reserved */
#define MTNIC_MASK_CONFIG_EQ_INT_VEC MTNIC_BC(0,6)
	u8 size;                        /* Num CQs is 2^size entries (size <= 22) */
#define MTNIC_MASK_CONFIG_EQ_SIZE	 MTNIC_BC(0,5)
	u8 offset;              /* Start address of CQE in first page (11:6) */
#define MTNIC_MASK_CONFIG_EQ_OFFSET	 MTNIC_BC(0,6)
	u8 page_size; /* Each mapped page is 2^(12+page_size) bytes*/
	u8 reserved[3];
	u32 page_address[0]; /* 64 bit page addresses of EQ buffer */
};

/* CMD RELEASE_RESOURCE */
enum mtnic_if_resource_types {
	MTNIC_IF_RESOURCE_TYPE_CQ = 0,
	MTNIC_IF_RESOURCE_TYPE_RX_RING,
	MTNIC_IF_RESOURCE_TYPE_TX_RING,
	MTNIC_IF_RESOURCE_TYPE_EQ
};

struct mtnic_if_release_resource_in_imm {
	u8 reserved1;
	u8 index;         /* must be 0 for TYPE_EQ */
	u8 reserved2;
	u8 type;          /* see enum mtnic_if_resource_types */
	u32 reserved3;
};









/*******************************************************************
*
* PCI addon structures
*
********************************************************************/

struct pcidev {
	unsigned long bar[6];
	u32 dev_config_space[64];
	struct pci_device *dev;
	u8 bus;
	u8 devfn;
};

struct dev_pci_struct {
	struct pcidev dev;
	struct pcidev br;
};

/* The only global var */
struct dev_pci_struct mtnic_pci_dev;



#endif /* H_MTNIC_IF_DEFS_H */

