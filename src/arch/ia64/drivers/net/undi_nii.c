#include "efi/efi.h"
#include "etherboot.h"
#include "isa.h"
#include "dev.h"
#include "nic.h"
#include "timer.h"

#warning "Place the declaraction of __call someplace more appropriate\n"
extern EFI_STATUS __call(void *,...);
#warning "Place a declaration of lookup_efi_nic somewhere useful"
EFI_NETWORK_INTERFACE_IDENTIFIER_INTERFACE *lookup_efi_nic(int index);

struct sw_undi {
	uint8_t  signature[4];
	uint8_t  len;
	uint8_t  fudge;
	uint8_t  rev;
	uint8_t  ifcnt;
	uint8_t  major;
	uint8_t  minor;
	uint16_t reserved1;
	uint32_t implementation;
#define UNDI_IMP_CMD_COMPLETE_INT_SUPPORTED		0x00000001
#define UNDI_IMP_PACKET_RX_INT_SUPPORTED		0x00000002
#define UNDI_IMP_TX_COMPLETE_INT_SUPPORTED		0x00000004
#define UNDI_IMP_SOFTWARE_INT_SUPPORTED			0x00000008
#define UNDI_IMP_FILTERED_MULTICAST_RX_SUPPORTED	0x00000010
#define UNDI_IMP_BROADCAST_RX_SUPPORTED			0x00000020
#define UNDI_IMP_PROMISCUOUS_RX_SUPPORTED		0x00000040
#define UNDI_IMP_PROMISCUOUS_MULTICAST_RX_SUPPORTED	0x00000080
#define UNDI_IMP_STATION_ADDR_SETTABLE			0x00000100
#define UNDI_IMP_STATISTICS_SUPPORTED			0x00000200
#define UNDI_IMP_NVDATA_SUPPORT_MASK			0x00000C00
#define UNDI_IMP_NVDATA_NOT_AVAILABLE			0x00000000
#define UNDI_IMP_NVDATA_READ_ONLY			0x00000400
#define UNDI_IMP_NVDATA_SPARSE_WRITEABLE		0x00000800
#define UNDI_IMP_NVDATA_BULK_WRITEABLE			0x00000C00
#define UNDI_IMP_MULTI_FRAME_SUPPORTED			0x00001000
#define UNDI_IMP_CMD_QUEUE_SUPPORTED			0x00002000
#define UNDI_IMP_CMD_LINK_SUPPORTED			0x00004000
#define UNDI_IMP_FRAG_SUPPORTED				0x00008000
#define UNDI_IMP_64BIT_DEVICE				0x00010000
#define UNDI_IMP_SW_VIRT_ADDR				0x40000000
#define UNDI_IMP_HW_UNDI				0x80000000
	uint64_t entry_point;
	uint8_t  reserved2[3];
	uint8_t  bus_type_cnt;
	uint32_t bus_type[0];
};

struct cdb {
	uint16_t op_code;
#define CDB_OP_GET_STATE		0x0000
#define CDB_OP_START			0x0001
#define CDB_OP_STOP			0x0002
#define CDB_OP_GET_INIT_INFO		0x0003
#define CDB_OP_GET_CONFIG_INFO		0x0004
#define CDB_OP_INITIALIZE		0x0005
#define CDB_OP_RESET			0x0006
#define CDB_OP_SHUTDOWN			0x0007
#define CDB_OP_INTERRUPT_ENABLES	0x0008
#define CDB_OP_RECEIVE_FILTERS		0x0009
#define CDB_OP_STATION_ADDRESS		0x000a
#define CDB_OP_STATISTICS		0x000b
#define CDB_OP_MCAST_IP_TO_MAC		0x000c
#define CDB_OP_NVDATA			0x000d
#define CDB_OP_GET_STATUS		0x000e
#define CDB_OP_FILL_HEADER		0x000f
#define CDB_OP_TRANSMIT			0x0010
#define CDB_OP_RECEIVE			0x0011
	uint16_t op_flags;
#define CDB_OPFLAGS_NOT_USED			0x0000
/* Initialize */
#define CDB_OPFLAGS_INIT_CABLE_DETECT_MASK	0x0001
#define CDB_OPFLAGS_INIT_DETECT_CABLE		0x0000
#define CDB_OPFLAGS_INIT_DO_NOT_DETECT_CABLE	0x0001
/* Reset */
#define CDB_OPFLAGS_RESET_DISABLE_INTERRUPTS	0x0001
#define CDB_OPFLAGS_RESET_DISABLE_FILTERS	0x0002
/* Interrupt Enables */
#define CDB_OPFLAGS_INTERRUPT_OPMASK		0xc000
#define CDB_OPFLAGS_INTERRUPT_ENABLE		0x8000
#define CDB_OPFLAGS_INTERRUPT_DISABLE		0x4000
#define CDB_OPFLAGS_INTERRUPT_READ		0x0000
#define CDB_OPFLAGS_INTERRUPT_RECEIVE		0x0001
#define CDB_OPFLAGS_INTERRUPT_TRANSMIT		0x0002
#define CDB_OPFLAGS_INTERRUPT_COMMAND		0x0004
#define CDB_OPFLAGS_INTERRUPT_SOFTWARE		0x0008
/* Receive Filters */
#define CDB_OPFLAGS_RECEIVE_FILTER_OPMASK		0xc000
#define CDB_OPFLAGS_RECEIVE_FILTER_ENABLE		0x8000
#define CDB_OPFLAGS_RECEIVE_FILTER_DISABLE		0x4000
#define CDB_OPFLAGS_RECEIVE_FILTER_READ			0x0000
#define CDB_OPFLAGS_RECEIVE_FILTER_RESET_MCAST_LIST	0x2000
#define CDB_OPFLAGS_RECEIVE_FILTER_UNICAST		0x0001
#define CDB_OPFLAGS_RECEIVE_FILTER_BROADCAST		0x0002
#define CDB_OPFLAGS_RECEIVE_FILTER_FILTERED_MULTICAST	0x0004
#define CDB_OPFLAGS_RECEIVE_FILTER_PROMISCUOUS		0x0008
#define CDB_OPFLAGS_RECEIVE_FILTER_ALL_MULTICAST	0x0010
/* Station Address */
#define CDB_OPFLAGS_STATION_ADDRESS_READ	0x0000
#define CDB_OPFLAGS_STATION_ADDRESS_WRITE	0x0000
#define CDB_OPFLAGS_STATION_ADDRESS_RESET	0x0001
/* Statistics */
#define CDB_OPFLAGS_STATISTICS_READ		0x0000
#define CDB_OPFLAGS_STATISTICS_RESET		0x0001
/* MCast IP to MAC */
#define CDB_OPFLAGS_MCAST_IP_TO_MAC_OPMASK	0x0003
#define CDB_OPFLAGS_MCAST_IPV4_TO_MAC		0x0000
#define CDB_OPFLAGS_MCAST_IPV6_TO_MAC		0x0001
/* NvData */
#define CDB_OPFLAGS_NVDATA_OPMASK		0x0001
#define CDB_OPFLAGS_NVDATA_READ			0x0000
#define CDB_OPFLAGS_NVDATA_WRITE		0x0001
/* Get Status */
#define CDB_OPFLAGS_GET_INTERRUPT_STATUS	0x0001
#define CDB_OPFLAGS_GET_TRANSMITTED_BUFFERS	0x0002
/* Fill Header */
#define CDB_OPFLAGS_FILL_HEADER_OPMASK		0x0001
#define CDB_OPFLAGS_FILL_HEADER_FRAGMENTED	0x0001
#define CDB_OPFLAGS_FILL_HEADER_WHOLE		0x0000
/* Transmit */
#define CDB_OPFLAGS_SWUNDI_TRANSMIT_OPMASK	0x0001
#define CDB_OPFLAGS_TRANSMIT_BLOCK		0x0001
#define CDB_OPFLAGS_TRANSMIT_DONT_BLOCK		0x0000

#define CDB_OPFLAGS_TRANSMIT_OPMASK		0x0002
#define CDB_OPFLAGS_TRANSMIT_FRAGMENTED		0x0002
#define CDB_OPFLAGS_TRANSMIT_WHOLE		0x0000
/* Receive */
	uint16_t cpb_size;
	uint16_t db_size;
	uint64_t cpb_addr;
	uint64_t db_addr;
	uint16_t stat_code;
#define CDB_STATCODE_INITIALIZE			0x0000
/* Common stat_code values */
#define CDB_STATCODE_SUCCESS			0x0000
#define CDB_STATCODE_INVALID_CDB		0x0001
#define CDB_STATCODE_INVALID_CPB		0x0002
#define CDB_STATCODE_BUSY			0x0003
#define CDB_STATCODE_QUEUE_FULL			0x0004
#define CDB_STATCODE_ALREADY_STARTED		0x0005
#define CDB_STATCODE_NOT_STARTED		0x0006
#define CDB_STATCODE_NOT_SHUTDOWN		0x0007
#define CDB_STATCODE_ALREADY_INITIALIZED	0x0008
#define CDB_STATCODE_NOT_INITIALIZED		0x0009
#define CDB_STATCODE_DEVICE_FAILURE		0x000A
#define CDB_STATCODE_NVDATA_FAILURE		0x000B
#define CDB_STATCODE_UNSUPPORTED		0x000C
#define CDB_STATCODE_BUFFER_FULL		0x000D
#define CDB_STATCODE_INVALID_PARAMETER		0x000E
#define CDB_STATCODE_INVALID_UNDI		0x000F
#define CDB_STATCODE_IPV4_NOT_SUPPORTED		0x0010
#define CDB_STATCODE_IPV6_NOT_SUPPORTED		0x0011
#define CDB_STATCODE_NOT_ENOUGH_MEMORY		0x0012
#define CDB_STATCODE_NO_DATA			0x0013

	uint16_t stat_flags;
#define CDB_STATFLAGS_INITIALIZE		0x0000
/* Common stat_flags */
#define CDB_STATFLAGS_STATUS_MASK		0xc000
#define CDB_STATFLAGS_COMMAND_COMPLETE		0xc000
#define CDB_STATFLAGS_COMMAND_FAILED		0x8000
#define CDB_STATFLAGS_COMMAND_QUEUED		0x4000
/* Get State */
#define CDB_STATFLAGS_GET_STATE_MASK		0x0003
#define CDB_STATFLAGS_GET_STATE_INITIALIZED	0x0002
#define CDB_STATFLAGS_GET_STATE_STARTED		0x0001
#define CDB_STATFLAGS_GET_STATE_STOPPED		0x0000
/* Start */
/* Get Init Info */
#define CDB_STATFLAGS_CABLE_DETECT_MASK			0x0001
#define CDB_STATFLAGS_CABLE_DETECT_NOT_SUPPORTED	0x0000
#define CDB_STATFLAGS_CABLE_DETECT_SUPPORTED		0x0001
/* Initialize */
#define CDB_STATFLAGS_INITIALIZED_NO_MEDIA	0x0001
/* Reset */
#define CDB_STATFLAGS_RESET_NO_MEDIA		0x0001
/* Shutdown */
/* Interrupt Enables */
#define CDB_STATFLAGS_INTERRUPT_RECEIVE		0x0001
#define CDB_STATFLAGS_INTERRUPT_TRANSMIT	0x0002
#define CDB_STATFLAGS_INTERRUPT_COMMAND		0x0004
/* Receive Filters */
#define CDB_STATFLAGS_RECEIVE_FILTER_UNICAST		0x0001
#define CDB_STATFLAGS_RECEIVE_FILTER_BROADCAST		0x0002
#define CDB_STATFLAGS_RECEIVE_FILTER_FILTERED_MULTICAST	0x0004
#define CDB_STATFLAGS_RECEIVE_FILTER_PROMISCUOUS	0x0008
#define CDB_STATFLAGS_RECEIVE_FILTER_ALL_MULTICAST	0x0010
/* Statistics */
/* MCast IP to MAC */
/* NvData */
/* Get Status */
#define CDB_STATFLAGS_GET_STATUS_INTERRUPT_MASK		0x000F
#define CDB_STATFLAGS_GET_STATUS_NO_INTERRUPTS		0x0000
#define CDB_STATFLAGS_GET_STATUS_RECEIVE		0x0001
#define CDB_STATFLAGS_GET_STATUS_TRANSMIT		0x0002
#define CDB_STATFLAGS_GET_STATUS_COMMAND		0x0004
#define CDB_STATFLAGS_GET_STATUS_SOFTWARE		0x0008
#define CDB_STATFLAGS_GET_STATUS_TXBUF_QUEUE_EMPTY	0x0010
#define CDB_STATFLAGS_GET_STATUS_NO_TXBUFS_WRITTEN	0x0020
/* Fill Header */
/* Transmit */
/* Receive */
	uint16_t ifnum;
#define CDB_IFNUM_START				0x0000
#define CDB_IFNUM_INVALID			0x0000
	uint16_t control;
#define CDB_CONTROL_QUEUE_IF_BUSY		0x0002

#define CDB_CONTROL_LINK			0x0001
#define CDB_CONTROL_LAST_CDB_IN_LIST		0x0000
};

#define UNDI_MAC_LENGTH 32
typedef uint8_t undi_mac_addr[UNDI_MAC_LENGTH];
typedef uint16_t undi_media_protocol;
typedef uint8_t undi_frame_type;
#define UNDI_FRAME_TYPE_NONE		0x00
#define UNDI_FRAME_TYPE_UNICAST		0x01
#define UNDI_FRAME_TYPE_BROADCAST	0x02
#define UNDI_FRAME_TYPE_MULTICAST	0x03
#define UNDI_FRAME_TYPE_PROMISCUOUS	0x04

#define UNDI_MAX_XMIT_BUFFERS 32
#define UNDI_MAX_MCAST_ADDRESS_CNT 8

#define UNDI_BUS_TYPE(a,b,c,d) \
	((((d) & 0xff) << 24) | \
	 (((c) & 0xff) << 16) | \
	 (((b) & 0xff) <<  8) | \
	 (((a) & 0xff) <<  0))

#define UNDI_BUS_TYPE_PCI	UNDI_BUS_TYPE('P','C','I','R')
#define UNDI_BUS_TYPE_PCC	UNDI_BUS_TYPE('P','C','C','R')
#define UNDI_BUS_TYPE_USB	UNDI_BUS_TYPE('U','S','B','R')
#define UNDI_BUS_TYPE_1394	UNDI_BUS_TYPE('1','3','9','4')

struct cpb_start {
	void *delay;
	void *block;
	void *virt2phys;
	void *mem_io;
} PACKED;

struct db_init_info {
	uint32_t memory_required;
	uint32_t frame_data_len;
	uint32_t link_speeds[4];
	uint32_t nv_count;
	uint16_t nv_width;
	uint16_t media_header_len;
	uint16_t hw_addr_len;
	uint16_t mcast_filter_cnt;
	uint16_t tx_buf_cnt;
	uint16_t tx_buf_size;
	uint16_t rx_buf_cnt;
	uint16_t rx_buf_size;
	uint8_t  if_type;
	uint8_t  duplex;
#define UNDI_DUPLEX_ENABLE_FULL_SUPPORTED	1
#define UNDI_DUPLEX_FORCE_FULL_SUPPORTED	2
	uint8_t  loopback;
#define UNDI_LOOPBACK_INTERNAL_SUPPORTED	1
#define UNDI_LOOPBACK_EXTERNAL_SUPPORTED	2
} PACKED;


struct db_pci_config_info {
	uint32_t bus_type;
	uint16_t bus;
	uint8_t  device;
	uint8_t  function;
	uint8_t  config[256];
};
struct db_pcc_config_info {
	uint32_t bus_type;
	uint16_t bus;
	uint8_t  device;
	uint8_t  function;
	uint8_t  config[256];
};
struct db_usb_config_info {
	uint32_t bus_type;
};
struct db_iee1394_config_info {
	uint32_t bus_type;
};
struct db_config_info {
	union {
		struct db_pci_config_info pci;
		struct db_pcc_config_info pcc;
		struct db_usb_config_info usb;
		struct db_iee1394_config_info iee1394;
	};
};

struct cpb_initialize {
	uint64_t memory_addr;
	uint32_t memory_length;
	uint32_t link_speed;
	uint16_t tx_buf_cnt;
	uint16_t tx_buf_size;
	uint16_t rx_buf_cnt;
	uint16_t rx_buf_size;
	uint8_t  duplex;
	uint8_t  loopback;
} PACKED;

struct db_initialize {
	uint32_t memory_used;
	uint16_t tx_buf_cnt;
	uint16_t tx_buf_size;
	uint16_t rx_buf_cnt;
	uint16_t rx_buf_size;
} PACKED;

struct cpb_station_address {
	undi_mac_addr station_addr;
} PACKED;

struct db_station_address {
	undi_mac_addr station_address;
	undi_mac_addr broadcast_address;
	undi_mac_addr permanent_address;
} PACKED;

struct cpb_receive_filters {
	undi_mac_addr mcast_list[UNDI_MAX_MCAST_ADDRESS_CNT];
} PACKED;

struct db_receive_filters {
	undi_mac_addr mcast_list[UNDI_MAX_MCAST_ADDRESS_CNT];
} PACKED;


struct db_get_status {
	uint32_t rx_frame_len;
	uint32_t reserved;
	uint64_t tx_buffer[UNDI_MAX_XMIT_BUFFERS];
} PACKED;

struct cpb_transmit {
	uint64_t frame_addr;
	uint32_t data_len;
	uint16_t media_header_len;
	uint16_t reserved;
} PACKED;

struct cpb_receive {
	uint64_t buffer_addr;
	uint32_t buffer_len;
	uint32_t reserved;
} PACKED;
struct db_receive {
	undi_mac_addr src_addr;
	undi_mac_addr dest_addr;
	uint32_t frame_len;
	undi_media_protocol protocol;
	uint16_t media_header_len;
	undi_frame_type type;
	uint8_t reserved[7];
} PACKED;
struct fptr {
	void *func;
	void *gp;
};

extern char __gp[];

/* Variables */
static unsigned undi_ifnum;
static void *undi_entry_point;
static struct cdb cdb;
static char buffer[1024*1024];

/* SW UNDI callbacks */
static void undi_udelay(uint64_t microseconds)
{
#if 0
	printf("undi_udelay(%lx)\n", microseconds);
#endif
	if (microseconds < 10) {
		microseconds = 10;
	}
	if (microseconds > 1000) {
		mdelay(microseconds/1000);
		microseconds%=1000;
	}
	udelay(microseconds);
}
static struct fptr fptr_undi_udelay = {
	.func = &undi_udelay,
	.gp = &__gp,
};
static void undi_block(uint32_t enable __unused)
{
#if 0
	printf("undi_block(%x)\n",
		enable);
#endif
	return;
}
static struct fptr fptr_undi_block = {
	.func = &undi_block,
	.gp = &__gp,
};
static void undi_virt2phys(uint64_t virtual, uint64_t *ptr)
{
#if 0
	printf("undi_virt2phys(%lx, %lx)\n",
		virtual, ptr);
#endif
	*ptr = virt_to_phys((void *)virtual);
}
static struct fptr fptr_undi_virt2phys = {
	.func = &undi_virt2phys,
	.gp = &__gp,
};
#define UNDI_IO_READ	0
#define UNDI_IO_WRITE	1
#define UNDI_MEM_READ	2
#define UNDI_MEM_WRITE	3
static void undi_mem_io(uint8_t read_write, uint8_t len, uint64_t port, uint64_t buf_addr)
{
	printf("undi_mem_io(%hhx, %hhx, %lx, %lx)\n",
		read_write, len, port, buf_addr);
#if 0
	if ((read_write == UNDI_IO_READ) && (len == 1)) {
		uint8_t *buf = (void *)buf_addr;
		*buf = inb(port);
	}
	else if ((read_write == UNDI_IO_READ) && (len == 2)) {
		uint16_t *buf = (void *)buf_addr;
		*buf = inw(port);
	}
	else if ((read_write == UNDI_IO_READ) && (len == 4)) {
		uint32_t *buf = (void *)buf_addr;
		*buf = inl(port);
	}
	else if ((read_write == UNDI_IO_READ) && (len == 8)) {
		uint64_t *buf = (void *)buf_addr;
		*buf = inq(port);
	}
	else if ((read_write == UNDI_IO_WRITE) && (len == 1)) {
		uint8_t *buf = (void *)buf_addr;
		outb(*buf, port);
	}
	else if ((read_write == UNDI_IO_WRITE) && (len == 2)) {
		uint16_t *buf = (void *)buf_addr;
		outw(*buf, port);
	}
	else if ((read_write == UNDI_IO_WRITE) && (len == 4)) {
		uint32_t *buf = (void *)buf_addr;
		outl(*buf, port);
	}
	else if ((read_write == UNDI_IO_WRITE) && (len == 8)) {
		uint64_t *buf = (void *)buf_addr;
		outq(*buf, port);
	}
	else if ((read_write == UNDI_MEM_READ) && (len == 1)) {
		uint8_t *buf = (void *)buf_addr;
		*buf = readb(port);
	}
	else if ((read_write == UNDI_MEM_READ) && (len == 2)) {
		uint16_t *buf = (void *)buf_addr;
		*buf = readw(port);
	}
	else if ((read_write == UNDI_MEM_READ) && (len == 4)) {
		uint32_t *buf = (void *)buf_addr;
		*buf = readl(port);
	}
	else if ((read_write == UNDI_MEM_READ) && (len == 8)) {
		uint64_t *buf = (void *)buf_addr;
		*buf = readq(port);
	}
	else if ((read_write == UNDI_MEM_WRITE) && (len == 1)) {
		uint8_t *buf = (void *)buf_addr;
		writeb(*buf, port);
	}
	else if ((read_write == UNDI_MEM_WRITE) && (len == 2)) {
		uint16_t *buf = (void *)buf_addr;
		writew(*buf, port);
	}
	else if ((read_write == UNDI_MEM_WRITE) && (len == 4)) {
		uint32_t *buf = (void *)buf_addr;
		writel(*buf, port);
	}
	else if ((read_write == UNDI_MEM_WRITE) && (len == 8)) {
		uint64_t *buf = (void *)buf_addr;
		writeq(*buf, port);
	}
#endif
}
static struct fptr fptr_undi_mem_io = {
	.func = &undi_mem_io,
	.gp = &__gp,
};

/* static void undi_memio(this, width, address, count, buffer);??? */


/* Wrappers to call the undi functions */
static int undi_call(struct cdb *cdb)
{
	int result = 1;
	cdb->stat_code  = CDB_STATCODE_INITIALIZE;
	cdb->stat_flags = CDB_STATFLAGS_INITIALIZE;
	cdb->ifnum      = undi_ifnum;
	cdb->control    = CDB_CONTROL_LAST_CDB_IN_LIST;
	__call(undi_entry_point, cdb);
	/* Wait until the command executes... */
	while((cdb->stat_flags & CDB_STATFLAGS_STATUS_MASK) == 0)
		;
	if ((cdb->stat_flags & CDB_STATFLAGS_STATUS_MASK) != 
		CDB_STATFLAGS_COMMAND_COMPLETE)
		result = 0;
	if (cdb->stat_code != CDB_STATCODE_SUCCESS)
		result = 0;
	return result;
}

static int get_state(struct cdb *cdb)
{
	memset(cdb, 0, sizeof(*cdb));
	cdb->op_code  = CDB_OP_GET_STATE;
	cdb->op_flags = CDB_OPFLAGS_NOT_USED;
	return undi_call(cdb);
}

static int start(struct cdb *cdb)
{
	static struct cpb_start cpb;
	memset(&cpb, 0, sizeof(cpb));
	cpb.delay     = &fptr_undi_udelay;
	cpb.block     = &fptr_undi_block;
	cpb.virt2phys = &fptr_undi_virt2phys;
	cpb.mem_io    = &fptr_undi_mem_io;

	memset(cdb, 0, sizeof(*cdb));
	cdb->op_code  = CDB_OP_START;
	cdb->op_flags = CDB_OPFLAGS_NOT_USED;
	cdb->cpb_size = sizeof(cpb);
	cdb->cpb_addr = virt_to_phys(&cpb);
	
	return undi_call(cdb);
}
static int stop(struct cdb *cdb)
{
	memset(cdb, 0, sizeof(*cdb));
	cdb->op_code  = CDB_OP_STOP;
	cdb->op_flags = CDB_OPFLAGS_NOT_USED;
	return undi_call(cdb);
}
static int get_init_info(struct cdb *cdb, struct db_init_info *info)
{
	memset(info, 0, sizeof(*info));
	memset(cdb, 0, sizeof(*cdb));
	cdb->op_code  = CDB_OP_GET_INIT_INFO;
	cdb->op_flags = CDB_OPFLAGS_NOT_USED;
	cdb->db_size  = sizeof(*info);
	cdb->db_addr  = virt_to_phys(info);
	return undi_call(cdb);
}

#if 0
/* get_config_info crashes broadcoms pxe driver */
static int get_config_info(struct cdb *cdb, struct db_config_info *info)
{
	memset(info, 0, sizeof(*info));
	memset(cdb, 0, sizeof(*cdb));
	cdb->op_code  = CDB_OP_GET_CONFIG_INFO;
	cdb->op_flags = CDB_OPFLAGS_NOT_USED;
	cdb->db_size  = sizeof(*info);
	cdb->db_addr  = virt_to_phys(info);
	return undi_call(cdb);
}
#endif
static int initialize(struct cdb *cdb, int media_detect,
	struct cpb_initialize *cpb, struct db_initialize *db)
{
	memset(db, 0, sizeof(*db));
	memset(cdb, 0, sizeof(*cdb));
	cdb->op_code  = CDB_OP_INITIALIZE;
	cdb->op_flags = media_detect?
		CDB_OPFLAGS_INIT_DETECT_CABLE:CDB_OPFLAGS_INIT_DO_NOT_DETECT_CABLE;
	cdb->cpb_size = sizeof(*cpb);
	cdb->cpb_addr = virt_to_phys(cpb);
	cdb->db_size  = sizeof(*db);
	cdb->db_addr  = virt_to_phys(db);
	return undi_call(cdb);
}
static int shutdown(struct cdb *cdb)
{
	memset(cdb, 0, sizeof(*cdb));
	cdb->op_code  = CDB_OP_SHUTDOWN;
	cdb->op_flags = CDB_OPFLAGS_NOT_USED;
	return undi_call(cdb);
}
static int station_address_read(struct cdb *cdb, struct db_station_address *db)
{
	memset(db, 0, sizeof(*db));
	memset(cdb, 0, sizeof(*cdb));
	cdb->op_code  = CDB_OP_STATION_ADDRESS;
	cdb->op_flags = CDB_OPFLAGS_STATION_ADDRESS_READ;
	cdb->db_size  = sizeof(*db);
	cdb->db_addr  = virt_to_phys(db);
	return undi_call(cdb);
}
static int receive_filters(struct cdb *cdb, unsigned opflags)
{
	/* I currently do not support setting
	 * or returning the multicast filter list.
	 * So do not even attempt to pass them.
	 */
	memset(cdb, 0, sizeof(*cdb));
	cdb->op_code  = CDB_OP_RECEIVE_FILTERS;
	cdb->op_flags = opflags;
	return undi_call(cdb);
}
static int get_transmitted_status(struct cdb *cdb, struct db_get_status *db)
{
	memset(db, 0, sizeof(*db));
	memset(cdb, 0, sizeof(*cdb));
	cdb->op_code  = CDB_OP_GET_STATUS;
	cdb->op_flags = CDB_OPFLAGS_GET_TRANSMITTED_BUFFERS;
	cdb->db_size  = sizeof(*db);
	cdb->db_addr  = virt_to_phys(db);
	return undi_call(cdb);
}

static int transmit(struct cdb *cdb, struct cpb_transmit *cpb)
{
	memset(cdb, 0, sizeof(*cdb));
	cdb->op_code  = CDB_OP_TRANSMIT;
	cdb->op_flags = CDB_OPFLAGS_TRANSMIT_WHOLE | CDB_OPFLAGS_TRANSMIT_DONT_BLOCK;
	cdb->cpb_size = sizeof(*cpb);
	cdb->cpb_addr = virt_to_phys(cpb);
	return undi_call(cdb);
}

static int receive(struct cdb *cdb, 
	struct cpb_receive *cpb, struct db_receive *db)
{
	memset(db, 0, sizeof(*db));
	memset(cdb, 0, sizeof(*cdb));
	cdb->op_code  = CDB_OP_RECEIVE;
	cdb->op_flags = CDB_OPFLAGS_NOT_USED;
	cdb->cpb_size = sizeof(*cpb);
	cdb->cpb_addr = virt_to_phys(cpb);
	cdb->db_size  = sizeof(*db);
	cdb->db_addr  = virt_to_phys(db);
	return undi_call(cdb);
}

/* The work horse functions */
static int nic_poll(struct nic *nic )
{
	int result;
	struct cpb_receive cpb;
	struct db_receive db;

	memset(&cpb, 0, sizeof(cpb));
	cpb.buffer_addr = virt_to_phys(nic->packet);
	cpb.buffer_len = ETH_FRAME_LEN;
	result = receive(&cdb, &cpb, &db);
	if (result) {
		nic->packetlen = db.frame_len;
		return 1;
	}
	else if (cdb.stat_code != CDB_STATCODE_NO_DATA) {
		printf("Receive failed: %lx\n", cdb.stat_code);		
	}
	return 0;	/* initially as this is called to flush the input */
}

static void nic_transmit(struct nic *nic, const char *dest, unsigned int type, 
	unsigned int size, const char *data)
{
	int result;
	static struct {
		uint8_t  dst_addr[ETH_ALEN];
		uint8_t  src_addr[ETH_ALEN];
		uint16_t type;
		uint8_t  data[ETH_MAX_MTU];
	} packet;
	struct cpb_transmit cpb;
	struct db_get_status db;
	int done;

	/* Build the packet to transmit in my buffer */
	memcpy(&packet.dst_addr, dest, ETH_ALEN);
	memcpy(&packet.src_addr, nic->node_addr, ETH_ALEN);
	packet.type = htons(type);
	memcpy(&packet.data, data, size);
	
	/* send the packet to destination */
	cpb.frame_addr       = virt_to_phys(&packet);
	cpb.data_len         = ETH_HLEN + size;
	cpb.media_header_len = ETH_HLEN;
	result = transmit(&cdb, &cpb);
	if (!result) {
		printf("transmit failed: %lx\n", cdb.stat_code);
		return;
	}
	/* Wait until the packet is actually transmitted, 
	 * indicating it is safe to reuse my trasmit buffer.
	 */
	done = 0;
	while(!done) {
		int i;
		result = get_transmitted_status(&cdb, &db);
		for(i = 0; i < UNDI_MAX_XMIT_BUFFERS; i++) {
			if (db.tx_buffer[i] == virt_to_phys(&packet)) {
				done = 1;
			}
		}
	}
}

static void nic_disable(struct dev *dev)
{
	struct nic *nic = (struct nic *)dev;
	int result;
	result = shutdown(&cdb);
	if (!result) {
		printf("UNDI nic does not want to shutdown: %x\n", cdb.stat_code);
	}
	result = stop(&cdb);
	if (!result) {
		printf("UNDI nic does not want to stop: %x\n", cdb.stat_code);
	}
	undi_ifnum = 0;
	undi_entry_point = 0;
}

static uint8_t undi_checksum(struct sw_undi *undi)
{
	uint8_t undi_sum, *ptr;
	int i;
	ptr = (uint8_t *)undi;
	undi_sum = 0;
	for(i = 0; i < undi->len; i++) {
		undi_sum += ((char *)undi)[i];
	}
	return undi_sum;
}

#if 0
/* Debug functions */
void print_nii(EFI_NETWORK_INTERFACE_IDENTIFIER_INTERFACE *nii)
{
	printf("NII Revision:  %lx\n", nii->Revision);
	printf("NII ID:        %lx\n", nii->ID);
	printf("NII ImageAddr: %lx\n", nii->ImageAddr);
	printf("NII ImageSize: %x\n",  nii->ImageSize);
	printf("NII StringID:  %c%c%c%c\n",  
		nii->StringId[0], nii->StringId[1], nii->StringId[2], nii->StringId[3]);
	printf("NII Type:      %hhx\n", nii->Type);
	printf("NII Version:   %d.%d\n", nii->MajorVer, nii->MinorVer);
	printf("NII IfNum:     %hhx\n", nii->IfNum);
	printf("\n");
}
void print_sw_undi(struct sw_undi *undi)
{
	int i;
	printf("UNDI signature:      %c%c%c%c\n",
		undi->signature[0], undi->signature[1],	undi->signature[2], undi->signature[3]);
	printf("UNDI len:            %hhx\n", undi->len);
	printf("UNDI fudge:          %hhx\n", undi->fudge);
	printf("UNDI rev:            %hhx\n", undi->rev);
	printf("UNDI ifcnt:          %hhx\n", undi->ifcnt);
	printf("UNDI version:        %d.%d\n", undi->major, undi->minor);
	printf("UNDI implementation: %x\n", undi->implementation);
	printf("UNDI entry point:    %lx\n", undi->entry_point);
	printf("UNDI bus type cnt:   %d\n", undi->bus_type_cnt);
	for(i = 0; i < undi->bus_type_cnt; i++) {
		printf("UNDI bus type:       %c%c%c%c\n", 
			((undi->bus_type[i]) >>  0) & 0xff,
			((undi->bus_type[i]) >>  8) & 0xff,
			((undi->bus_type[i]) >> 16) & 0xff,
			((undi->bus_type[i]) >> 24) & 0xff);
	}
	printf("UNDI sum:            %hhx\n", undi_checksum(undi));
	printf("\n");
}
void print_init_info(struct db_init_info *info)
{
	printf("init_info.memory_required:   %d\n", info->memory_required);
	printf("init_info.frame_data_len:    %d\n", info->frame_data_len);
	printf("init_info.link_speeds:       %d %d %d %d\n",
		info->link_speeds[0], info->link_speeds[1],
		info->link_speeds[2], info->link_speeds[3]);
	printf("init_info.media_header_len:  %d\n", info->media_header_len);
	printf("init_info.hw_addr_len:       %d\n", info->hw_addr_len);
	printf("init_info.mcast_filter_cnt:  %d\n", info->mcast_filter_cnt);
	printf("init_info.tx_buf_cnt:        %d\n", info->tx_buf_cnt);
	printf("init_info.tx_buf_size:       %d\n", info->tx_buf_size);
	printf("init_info.rx_buf_cnt:        %d\n", info->rx_buf_cnt);
	printf("init_info.rx_buf_size:       %d\n", info->rx_buf_size);
	printf("init_info.if_type:           %hhx\n", info->if_type);
	printf("init_info.duplex:            %hhx\n", info->duplex);
	printf("init_info.loopback:          %hhx\n", info->loopback);
	printf("\n");
}
void print_config_info(struct db_config_info *info)
{
	int i;
	printf("config_info.bus_type: %c%c%c%c\n",
		((info->pci.bus_type) >>  0) & 0xff,
		((info->pci.bus_type) >>  8) & 0xff,
		((info->pci.bus_type) >> 16) & 0xff,
		((info->pci.bus_type) >> 24) & 0xff);
	if (info->pci.bus_type != UNDI_BUS_TYPE_PCI) {
		return;
	}
	printf("config_info.bus:      %hx\n", info->pci.bus);
	printf("config_info.device:   %hhx\n", info->pci.device);
	printf("config_info.function: %hhx\n", info->pci.function);
	printf("config_info.config:\n");
	for(i = 0; i < 256; i++) {
		if ((i & 0xf) == 0) {
			printf("[%hhx]", i);
		}
		printf(" %hhx", info->pci.config[i]);
		if ((i & 0xf) == 0xf) {
			printf("\n");
		}
	}
	printf("\n");
		
}
void print_cdb(struct cdb *cdb)
{
	printf("\n");
	printf("cdb.op_code:    %hx\n", cdb->op_code);
	printf("cdb.op_flags:   %hx\n", cdb->op_flags);
	printf("cdb.cpb_size:   %d\n",  cdb->cpb_size);
	printf("cdb.db_size:    %d\n",  cdb->db_size);
	printf("cdb.cpb_addr:   %lx\n", cdb->cpb_addr);
	printf("cdb.db_addr:    %lx\n", cdb->db_addr);
	printf("cdb.stat_code:  %lx\n", cdb->stat_code);
	printf("cdb.stat_flags: %lx\n", cdb->stat_flags);
	printf("cdb.ifnum       %d\n",  cdb->ifnum);
	printf("cdb.control:    %hx\n", cdb->control);
	printf("\n");
}
#endif 
#define ARPHRD_ETHER 1
static int nic_setup(struct dev *dev, 
	EFI_NETWORK_INTERFACE_IDENTIFIER_INTERFACE *nii)
{
	struct nic *nic = (struct nic *)dev;
	int result;
	struct sw_undi *undi;
	struct db_init_info init_info;
	struct cpb_initialize cpb_initialize;
	struct db_initialize db_initialize;
	struct db_station_address db_station_address;
	int media_detect;
	unsigned filter, no_filter;
	int i;

	/* Fail if I I'm not passed a valid nii */
	if (!nii)
		return 0;

	/* Fail if this nit a SW UNDI interface */
	if (nii->ID == 0)
		return 0;

	undi = phys_to_virt(nii->ID);
	
	/* Verify the undi structure */

	/* It must have a pxe signature */
	if (memcmp(undi->signature, "!PXE", 4) != 0)
		return 0;
	/* It must have a valid checksum */
	if (undi_checksum(undi) != 0)
		return 0;
	/* It must be software undi */
	if (undi->implementation & UNDI_IMP_HW_UNDI)
		return 0;

	/* Setup to do undi calls */
	undi_ifnum = nii->IfNum;
	undi_entry_point = (void *)undi->entry_point;

	/* Find the UNDI state... */
	result = get_state(&cdb);
	if (!result)
		return 0;

	/* See if the device is already initialized */
	if ((cdb.stat_flags & CDB_STATFLAGS_GET_STATE_MASK) != 
		CDB_STATFLAGS_GET_STATE_STOPPED) {

		/* If so attempt to stop it */
		if ((cdb.stat_flags & CDB_STATFLAGS_GET_STATE_MASK) ==
			CDB_STATFLAGS_GET_STATE_INITIALIZED) {
			result = shutdown(&cdb);
			result = stop(&cdb);
		}
		else if ((cdb.stat_flags & CDB_STATFLAGS_GET_STATE_MASK) ==
			CDB_STATFLAGS_GET_STATE_STARTED) {
			result = stop(&cdb);
		}

		/* See if it did stop */
		result = get_state(&cdb);
		if (!result)
			return 0;

		/* If it didn't stop give up */
		if ((cdb.stat_flags & CDB_STATFLAGS_GET_STATE_MASK) != 
			CDB_STATFLAGS_GET_STATE_STOPPED)
			return 0;
	
	}

	result = start(&cdb);
	if (!result) {
		printf("Device would not start: %x\n", cdb.stat_code);
		return 0;
	}
	result = get_init_info(&cdb, &init_info);
	if (!result) {
		printf("Device wount not give init info: %x\n", cdb.stat_code);
		stop(&cdb);
		return 0;
	}
	/* See if the NIC can detect the presence of a cable */
	media_detect = (cdb.stat_flags & CDB_STATFLAGS_CABLE_DETECT_MASK) == 
		CDB_STATFLAGS_CABLE_DETECT_SUPPORTED;

	if ((init_info.if_type != ARPHRD_ETHER) ||
		(init_info.hw_addr_len != ETH_ALEN)) {
		printf("Not ethernet\n");
		stop(&cdb);
		return 0;
	}
	if (init_info.memory_required > sizeof(buffer)) {
		printf("NIC wants %d bytes I only have %ld bytes\n",
			init_info.memory_required, sizeof(buffer));
		stop(&cdb);
		return 0;
	}
	/* Initialize the device */
	memset(buffer, 0, sizeof(buffer));
	memset(&cpb_initialize, 0, sizeof(cpb_initialize));
	cpb_initialize.memory_addr   = virt_to_phys(&buffer);
	cpb_initialize.memory_length = init_info.memory_required;
	cpb_initialize.link_speed    = 0; /* auto detect */
	/* UNDI nics will not take suggestions :( 
	 * So let them figure out an appropriate buffer stragety on their own.
	 */
	cpb_initialize.tx_buf_cnt    = 0;
	cpb_initialize.tx_buf_size   = 0;
	cpb_initialize.rx_buf_cnt    = 0;
	cpb_initialize.rx_buf_size   = 0;
	cpb_initialize.duplex        = 0;
	cpb_initialize.loopback      = 0;
	result = initialize(&cdb, media_detect, &cpb_initialize, &db_initialize);
	if (!result) {
		printf("Device would not initialize: %x\n", cdb.stat_code);
		stop(&cdb);
		return 0;
	}
#if 0
	/* It appears the memory_used parameter is never set correctly, ignore it */
	if (db_initialize.memory_used > sizeof(buffer)) {
		printf("NIC is using %d bytes I only have %ld bytes\n",
			db_initialize.memory_used, sizeof(buffer));
		printf("tx_buf_cnt:  %d\n", db_initialize.tx_buf_cnt);
		printf("tx_buf_size: %d\n", db_initialize.tx_buf_size);
		printf("rx_buf_cnt:  %d\n", db_initialize.rx_buf_cnt);
		printf("rx_buf_size: %d\n", db_initialize.rx_buf_size);
		nic_disable(dev);
		return 0;
	}
	printf("NIC is using %d bytes\n",
		db_initialize.memory_used);
#endif
	if (media_detect && (
		    (cdb.stat_flags & ~CDB_STATFLAGS_STATUS_MASK) ==
		    CDB_STATFLAGS_INITIALIZED_NO_MEDIA)) {
		printf("No media present\n");
		nic_disable(dev);
		return 0;
	}

	/* Get the mac address */
	result = station_address_read(&cdb, &db_station_address);
	if (!result) {
		printf("Could not read station address: %x\n",
			cdb.stat_code);
		nic_disable(dev);
		return 0;
	}
	for(i = 0; i < ETH_ALEN; i++) {
		nic->node_addr[i] = db_station_address.station_address[i];
	}
	printf("Ethernet addr: %!\n", nic->node_addr);

	filter = CDB_OPFLAGS_RECEIVE_FILTER_ENABLE |
		CDB_OPFLAGS_RECEIVE_FILTER_UNICAST |
		CDB_OPFLAGS_RECEIVE_FILTER_BROADCAST;
	no_filter = CDB_OPFLAGS_RECEIVE_FILTER_DISABLE |
		CDB_OPFLAGS_RECEIVE_FILTER_RESET_MCAST_LIST |
		CDB_OPFLAGS_RECEIVE_FILTER_FILTERED_MULTICAST; 
	
	if (undi->implementation & UNDI_IMP_PROMISCUOUS_MULTICAST_RX_SUPPORTED) {
		filter |= CDB_OPFLAGS_RECEIVE_FILTER_ALL_MULTICAST;
		no_filter |= CDB_OPFLAGS_RECEIVE_FILTER_PROMISCUOUS;
	}
	else if (undi->implementation & UNDI_IMP_PROMISCUOUS_RX_SUPPORTED) {
		filter |= CDB_OPFLAGS_RECEIVE_FILTER_PROMISCUOUS;
	}
	
	result = receive_filters(&cdb, no_filter);
	if (!result) {
		printf("Could not clear receive filters: %x\n",
			cdb.stat_code);
		nic_disable(dev);
		return 0;
	}
	result = receive_filters(&cdb, filter);
	if (!result) {
		printf("Could not set receive filters: %x\n",
			cdb.stat_code);
		nic_disable(dev);
		return 0;
	}
	
	/* It would be nice to call get_config_info so I could pass
	 * the type of nic, but that crashes some efi drivers.
	 */
	/* Everything worked!  */
	dev->disable  = nic_disable;
	nic->poll     = nic_poll;
	nic->transmit = nic_transmit;

	return 1;
}

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
***************************************************************************/
static int nic_probe(struct dev *dev, unsigned short *dummy __unused)
{
	EFI_NETWORK_INTERFACE_IDENTIFIER_INTERFACE *nii;
	int index;
	int result;

	index = dev->index+ 1;
	if (dev->how_probe == PROBE_AWAKE) {
		index--;
	}
	for(result = 0; !result && (nii = lookup_efi_nic(index)); index++) {
		result = nic_setup(dev, nii);
		if (result) {
			break;
		}
	}
	dev->index = result ? index : -1;
	return result;
}




static struct isa_driver nic_driver __isa_driver = {
	.type     = NIC_DRIVER,
	.name     = "undi_nii",
	.probe    = nic_probe,
	.ioaddrs  = 0,
};
