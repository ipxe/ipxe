/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
 * Copyright (C) 2008 NetXen, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <gpxe/pci.h>
#include <gpxe/malloc.h>
#include <gpxe/iobuf.h>
#include <gpxe/netdevice.h>
#include <gpxe/if_ether.h>
#include <gpxe/ethernet.h>
#include <gpxe/spi.h>
#include "phantom.h"

/**
 * @file
 *
 * NetXen Phantom NICs
 *
 */

/** Maximum time to wait for SPI lock */
#define PHN_SPI_LOCK_TIMEOUT_MS 100

/** Maximum time to wait for SPI command to be issued */
#define PHN_SPI_CMD_TIMEOUT_MS 100

/** Maximum time to wait for command PEG to initialise
 *
 * BUGxxxx
 *
 * The command PEG will currently report initialisation complete only
 * when at least one PHY has detected a link (so that the global PHY
 * clock can be set to 10G/1G as appropriate).  This can take a very,
 * very long time.
 *
 * A future firmware revision should decouple PHY initialisation from
 * firmware initialisation, at which point the command PEG will report
 * initialisation complete much earlier, and this timeout can be
 * reduced.
 */
#define PHN_CMDPEG_INIT_TIMEOUT_SEC 50

/** Maximum time to wait for receive PEG to initialise */
#define PHN_RCVPEG_INIT_TIMEOUT_SEC 2

/** Maximum time to wait for firmware to accept a command */
#define PHN_ISSUE_CMD_TIMEOUT_MS 2000

/** Maximum time to wait for test memory */
#define PHN_TEST_MEM_TIMEOUT_MS 100

/** Link state poll frequency
 *
 * The link state will be checked once in every N calls to poll().
 */
#define PHN_LINK_POLL_FREQUENCY 4096

/** Number of RX descriptors */
#define PHN_NUM_RDS 32

/** RX maximum fill level.  Must be strictly less than PHN_NUM_RDS. */
#define PHN_RDS_MAX_FILL 16

/** RX buffer size */
#define PHN_RX_BUFSIZE ( 32 /* max LL padding added by card */ + \
			 ETH_FRAME_LEN )

/** Number of RX status descriptors */
#define PHN_NUM_SDS 32

/** Number of TX descriptors */
#define PHN_NUM_CDS 8

/** A Phantom descriptor ring set */
struct phantom_descriptor_rings {
	/** RX descriptors */
	struct phantom_rds rds[PHN_NUM_RDS];
	/** RX status descriptors */
	struct phantom_sds sds[PHN_NUM_SDS];
	/** TX descriptors */
	union phantom_cds cds[PHN_NUM_CDS];
	/** TX consumer index */
	volatile uint32_t cmd_cons;
};

/** A Phantom NIC port */
struct phantom_nic_port {
	/** Phantom NIC containing this port */
	struct phantom_nic *phantom;
	/** Port number */
	unsigned int port;


	/** RX context ID */
	uint16_t rx_context_id;
	/** RX descriptor producer CRB offset */
	unsigned long rds_producer_crb;
	/** RX status descriptor consumer CRB offset */
	unsigned long sds_consumer_crb;

	/** RX producer index */
	unsigned int rds_producer_idx;
	/** RX consumer index */
	unsigned int rds_consumer_idx;
	/** RX status consumer index */
	unsigned int sds_consumer_idx;
	/** RX I/O buffers */
	struct io_buffer *rds_iobuf[PHN_RDS_MAX_FILL];


	/** TX context ID */
	uint16_t tx_context_id;
	/** TX descriptor producer CRB offset */
	unsigned long cds_producer_crb;

	/** TX producer index */
	unsigned int cds_producer_idx;
	/** TX consumer index */
	unsigned int cds_consumer_idx;
	/** TX I/O buffers */
	struct io_buffer *cds_iobuf[PHN_NUM_CDS];


	/** Link state poll timer */
	unsigned long link_poll_timer;


	/** Descriptor rings */
	struct phantom_descriptor_rings *desc;
};

/** RX context creation request and response buffers */
struct phantom_create_rx_ctx_rqrsp {
	struct {
		struct nx_hostrq_rx_ctx_s rx_ctx;
		struct nx_hostrq_rds_ring_s rds;
		struct nx_hostrq_sds_ring_s sds;
	} __unm_dma_aligned hostrq;
	struct {
		struct nx_cardrsp_rx_ctx_s rx_ctx;
		struct nx_cardrsp_rds_ring_s rds;
		struct nx_cardrsp_sds_ring_s sds;
	} __unm_dma_aligned cardrsp;
};

/** TX context creation request and response buffers */
struct phantom_create_tx_ctx_rqrsp {
	struct {
		struct nx_hostrq_tx_ctx_s tx_ctx;
	} __unm_dma_aligned hostrq;
	struct {
		struct nx_cardrsp_tx_ctx_s tx_ctx;
	} __unm_dma_aligned cardrsp;
};

/** A Phantom DMA buffer area */
union phantom_dma_buffer {
	/** Dummy area required for (read-only) self-tests */
	uint8_t dummy_dma[UNM_DUMMY_DMA_SIZE];
	/** RX context creation request and response buffers */
	struct phantom_create_rx_ctx_rqrsp create_rx_ctx;
	/** TX context creation request and response buffers */
	struct phantom_create_tx_ctx_rqrsp create_tx_ctx;
};

/** A Phantom NIC */
struct phantom_nic {
	/** BAR 0 */
	void *bar0;
	/** Current CRB window */
	unsigned long crb_window;
	/** CRB window access method */
	unsigned long ( *crb_access ) ( struct phantom_nic *phantom,
					unsigned long reg );

	/** Number of ports */
	int num_ports;
	/** Per-port network devices */
	struct net_device *netdev[UNM_FLASH_NUM_PORTS];

	/** DMA buffers */
	union phantom_dma_buffer *dma_buf;

	/** Flash memory SPI bus */
	struct spi_bus spi_bus;
	/** Flash memory SPI device */
	struct spi_device flash;

	/** Last known link state */
	uint32_t link_state;
};

/***************************************************************************
 *
 * CRB register access
 *
 */

/**
 * Prepare for access to CRB register via 128MB BAR
 *
 * @v phantom		Phantom NIC
 * @v reg		Register offset within abstract address space
 * @ret offset		Register offset within PCI BAR0
 */
static unsigned long phantom_crb_access_128m ( struct phantom_nic *phantom,
					       unsigned long reg ) {
	static const uint32_t reg_window[] = {
		[UNM_CRB_BLK_PCIE]	= 0x0000000,
		[UNM_CRB_BLK_CAM]	= 0x2000000,
		[UNM_CRB_BLK_ROMUSB]	= 0x2000000,
		[UNM_CRB_BLK_TEST]	= 0x0000000,
	};
	static const uint32_t reg_bases[] = {
		[UNM_CRB_BLK_PCIE]	= 0x6100000,
		[UNM_CRB_BLK_CAM]	= 0x6200000,
		[UNM_CRB_BLK_ROMUSB]	= 0x7300000,
		[UNM_CRB_BLK_TEST]	= 0x6200000,
	};
	unsigned int block = UNM_CRB_BLK ( reg );
	unsigned long offset = UNM_CRB_OFFSET ( reg );
	uint32_t window = reg_window[block];
	uint32_t verify_window;

	if ( phantom->crb_window != window ) {

		/* Write to the CRB window register */
		writel ( window, phantom->bar0 + UNM_128M_CRB_WINDOW );

		/* Ensure that the write has reached the card */
		verify_window = readl ( phantom->bar0 + UNM_128M_CRB_WINDOW );
		assert ( verify_window == window );

		/* Record new window */
		phantom->crb_window = window;
	}

	return ( reg_bases[block] + offset );
}

/**
 * Prepare for access to CRB register via 32MB BAR
 *
 * @v phantom		Phantom NIC
 * @v reg		Register offset within abstract address space
 * @ret offset		Register offset within PCI BAR0
 */
static unsigned long phantom_crb_access_32m ( struct phantom_nic *phantom,
					      unsigned long reg ) {
	static const uint32_t reg_window[] = {
		[UNM_CRB_BLK_PCIE]	= 0x0000000,
		[UNM_CRB_BLK_CAM]	= 0x2000000,
		[UNM_CRB_BLK_ROMUSB]	= 0x2000000,
		[UNM_CRB_BLK_TEST]	= 0x0000000,
	};
	static const uint32_t reg_bases[] = {
		[UNM_CRB_BLK_PCIE]	= 0x0100000,
		[UNM_CRB_BLK_CAM]	= 0x0200000,
		[UNM_CRB_BLK_ROMUSB]	= 0x1300000,
		[UNM_CRB_BLK_TEST]	= 0x0200000,
	};
	unsigned int block = UNM_CRB_BLK ( reg );
	unsigned long offset = UNM_CRB_OFFSET ( reg );
	uint32_t window = reg_window[block];
	uint32_t verify_window;

	if ( phantom->crb_window != window ) {

		/* Write to the CRB window register */
		writel ( window, phantom->bar0 + UNM_32M_CRB_WINDOW );

		/* Ensure that the write has reached the card */
		verify_window = readl ( phantom->bar0 + UNM_32M_CRB_WINDOW );
		assert ( verify_window == window );

		/* Record new window */
		phantom->crb_window = window;
	}

	return ( reg_bases[block] + offset );
}

/**
 * Prepare for access to CRB register via 2MB BAR
 *
 * @v phantom		Phantom NIC
 * @v reg		Register offset within abstract address space
 * @ret offset		Register offset within PCI BAR0
 */
static unsigned long phantom_crb_access_2m ( struct phantom_nic *phantom,
					     unsigned long reg ) {
	static const uint32_t reg_window_hi[] = {
		[UNM_CRB_BLK_PCIE]	= 0x77300000,
		[UNM_CRB_BLK_CAM]	= 0x41600000,
		[UNM_CRB_BLK_ROMUSB]	= 0x42100000,
		[UNM_CRB_BLK_TEST]	= 0x29500000,
	};
	unsigned int block = UNM_CRB_BLK ( reg );
	unsigned long offset = UNM_CRB_OFFSET ( reg );
	uint32_t window = ( reg_window_hi[block] | ( offset & 0x000f0000 ) );
	uint32_t verify_window;

	if ( phantom->crb_window != window ) {

		/* Write to the CRB window register */
		writel ( window, phantom->bar0 + UNM_2M_CRB_WINDOW );

		/* Ensure that the write has reached the card */
		verify_window = readl ( phantom->bar0 + UNM_2M_CRB_WINDOW );
		assert ( verify_window == window );

		/* Record new window */
		phantom->crb_window = window;
	}

	return ( 0x1e0000 + ( offset & 0xffff ) );
}

/**
 * Read from Phantom CRB register
 *
 * @v phantom		Phantom NIC
 * @v reg		Register offset within abstract address space
 * @ret	value		Register value
 */
static uint32_t phantom_readl ( struct phantom_nic *phantom,
				unsigned long reg ) {
	unsigned long offset;

	offset = phantom->crb_access ( phantom, reg );
	return readl ( phantom->bar0 + offset );
}

/**
 * Write to Phantom CRB register
 *
 * @v phantom		Phantom NIC
 * @v value		Register value
 * @v reg		Register offset within abstract address space
 */
static void phantom_writel ( struct phantom_nic *phantom, uint32_t value,
			     unsigned long reg ) {
	unsigned long offset;

	offset = phantom->crb_access ( phantom, reg );
	writel ( value, phantom->bar0 + offset );
}

/**
 * Write to Phantom CRB HI/LO register pair
 *
 * @v phantom		Phantom NIC
 * @v value		Register value
 * @v lo_offset		LO register offset within CRB
 * @v hi_offset		HI register offset within CRB
 */
static inline void phantom_write_hilo ( struct phantom_nic *phantom,
					uint64_t value,
					unsigned long lo_offset,
					unsigned long hi_offset ) {
	uint32_t lo = ( value & 0xffffffffUL );
	uint32_t hi = ( value >> 32 );

	phantom_writel ( phantom, lo, lo_offset );
	phantom_writel ( phantom, hi, hi_offset );
}

/***************************************************************************
 *
 * Firmware message buffer access (for debug)
 *
 */

/**
 * Read from Phantom test memory
 *
 * @v phantom		Phantom NIC
 * @v offset		Offset within test memory
 * @v buf		8-byte buffer to fill
 * @ret rc		Return status code
 */
static int phantom_read_test_mem ( struct phantom_nic *phantom,
				   uint64_t offset, uint32_t buf[2] ) {
	unsigned int retries;
	uint32_t test_control;

	phantom_write_hilo ( phantom, offset, UNM_TEST_ADDR_LO,
			     UNM_TEST_ADDR_HI );
	phantom_writel ( phantom, UNM_TEST_CONTROL_ENABLE, UNM_TEST_CONTROL );
	phantom_writel ( phantom,
			 ( UNM_TEST_CONTROL_ENABLE | UNM_TEST_CONTROL_START ),
			 UNM_TEST_CONTROL );
	
	for ( retries = 0 ; retries < PHN_TEST_MEM_TIMEOUT_MS ; retries++ ) {
		test_control = phantom_readl ( phantom, UNM_TEST_CONTROL );
		if ( ( test_control & UNM_TEST_CONTROL_BUSY ) == 0 ) {
			buf[0] = phantom_readl ( phantom, UNM_TEST_RDDATA_LO );
			buf[1] = phantom_readl ( phantom, UNM_TEST_RDDATA_HI );
			return 0;
		}
		mdelay ( 1 );
	}

	DBGC ( phantom, "Phantom %p timed out waiting for test memory\n",
	       phantom );
	return -ETIMEDOUT;
}

/**
 * Dump Phantom firmware dmesg log
 *
 * @v phantom		Phantom NIC
 * @v log		Log number
 */
static void phantom_dmesg ( struct phantom_nic *phantom, unsigned int log ) {
	uint32_t head;
	uint32_t tail;
	uint32_t len;
	uint32_t sig;
	uint32_t offset;
	union {
		uint8_t bytes[8];
		uint32_t dwords[2];
	} buf;
	unsigned int i;
	int rc;

	/* Optimise out for non-debug builds */
	if ( ! DBG_LOG )
		return;

	head = phantom_readl ( phantom, UNM_CAM_RAM_DMESG_HEAD ( log ) );
	len = phantom_readl ( phantom, UNM_CAM_RAM_DMESG_LEN ( log ) );
	tail = phantom_readl ( phantom, UNM_CAM_RAM_DMESG_TAIL ( log ) );
	sig = phantom_readl ( phantom, UNM_CAM_RAM_DMESG_SIG ( log ) );
	DBGC ( phantom, "Phantom %p firmware dmesg buffer %d (%08lx-%08lx)\n",
	       phantom, log, head, tail );
	assert ( ( head & 0x07 ) == 0 );
	if ( sig != UNM_CAM_RAM_DMESG_SIG_MAGIC ) {
		DBGC ( phantom, "Warning: bad signature %08lx (want %08lx)\n",
		       sig, UNM_CAM_RAM_DMESG_SIG_MAGIC );
	}

	for ( offset = head ; offset < tail ; offset += 8 ) {
		if ( ( rc = phantom_read_test_mem ( phantom, offset,
						    buf.dwords ) ) != 0 ) {
			DBGC ( phantom, "Phantom %p could not read from test "
			       "memory: %s\n", phantom, strerror ( rc ) );
			break;
		}
		for ( i = 0 ; ( ( i < sizeof ( buf ) ) &&
				( offset + i ) < tail ) ; i++ ) {
			DBG ( "%c", buf.bytes[i] );
		}
	}
	DBG ( "\n" );
}

/**
 * Dump Phantom firmware dmesg logs
 *
 * @v phantom		Phantom NIC
 */
static void __attribute__ (( unused ))
phantom_dmesg_all ( struct phantom_nic *phantom ) {
	unsigned int i;

	for ( i = 0 ; i < UNM_CAM_RAM_NUM_DMESG_BUFFERS ; i++ )
		phantom_dmesg ( phantom, i );
}

/***************************************************************************
 *
 * SPI bus access (for flash memory)
 *
 */

/**
 * Acquire Phantom SPI lock
 *
 * @v phantom		Phantom NIC
 * @ret rc		Return status code
 */
static int phantom_spi_lock ( struct phantom_nic *phantom ) {
	unsigned int retries;
	uint32_t pcie_sem2_lock;

	for ( retries = 0 ; retries < PHN_SPI_LOCK_TIMEOUT_MS ; retries++ ) {
		pcie_sem2_lock = phantom_readl ( phantom, UNM_PCIE_SEM2_LOCK );
		if ( pcie_sem2_lock != 0 )
			return 0;
		mdelay ( 1 );
	}

	DBGC ( phantom, "Phantom %p timed out waiting for SPI lock\n",
	       phantom );
	return -ETIMEDOUT;
}

/**
 * Wait for Phantom SPI command to complete
 *
 * @v phantom		Phantom NIC
 * @ret rc		Return status code
 */
static int phantom_spi_wait ( struct phantom_nic *phantom ) {
	unsigned int retries;
	uint32_t glb_status;

	for ( retries = 0 ; retries < PHN_SPI_CMD_TIMEOUT_MS ; retries++ ) {
		glb_status = phantom_readl ( phantom, UNM_ROMUSB_GLB_STATUS );
		if ( glb_status & UNM_ROMUSB_GLB_STATUS_ROM_DONE )
			return 0;
		mdelay ( 1 );
	}

	DBGC ( phantom, "Phantom %p timed out waiting for SPI command\n",
	       phantom );
	return -ETIMEDOUT;
}

/**
 * Release Phantom SPI lock
 *
 * @v phantom		Phantom NIC
 */
static void phantom_spi_unlock ( struct phantom_nic *phantom ) {
	phantom_readl ( phantom, UNM_PCIE_SEM2_UNLOCK );
}

/**
 * Read/write data via Phantom SPI bus
 *
 * @v bus		SPI bus
 * @v device		SPI device
 * @v command		Command
 * @v address		Address to read/write (<0 for no address)
 * @v data_out		TX data buffer (or NULL)
 * @v data_in		RX data buffer (or NULL)
 * @v len		Length of data buffer(s)
 * @ret rc		Return status code
 */
static int phantom_spi_rw ( struct spi_bus *bus,
			    struct spi_device *device,
			    unsigned int command, int address,
			    const void *data_out, void *data_in,
			    size_t len ) {
	struct phantom_nic *phantom =
		container_of ( bus, struct phantom_nic, spi_bus );
	uint32_t data;
	int rc;

	DBGCP ( phantom, "Phantom %p SPI command %x at %x+%zx\n",
		phantom, command, address, len );
	if ( data_out )
		DBGCP_HDA ( phantom, address, data_out, len );

	/* We support only exactly 4-byte reads */
	if ( len != UNM_SPI_BLKSIZE ) {
		DBGC ( phantom, "Phantom %p invalid SPI length %zx\n",
		       phantom, len );
		return -EINVAL;
	}

	/* Acquire SPI lock */
	if ( ( rc = phantom_spi_lock ( phantom ) ) != 0 )
		goto err_lock;

	/* Issue SPI command as per the PRM */
	if ( data_out ) {
		memcpy ( &data, data_out, sizeof ( data ) );
		phantom_writel ( phantom, data, UNM_ROMUSB_ROM_WDATA );
	}
	phantom_writel ( phantom, address, UNM_ROMUSB_ROM_ADDRESS );
	phantom_writel ( phantom, ( device->address_len / 8 ),
			 UNM_ROMUSB_ROM_ABYTE_CNT );
	udelay ( 100 ); /* according to PRM */
	phantom_writel ( phantom, 0, UNM_ROMUSB_ROM_DUMMY_BYTE_CNT );
	phantom_writel ( phantom, command, UNM_ROMUSB_ROM_INSTR_OPCODE );

	/* Wait for SPI command to complete */
	if ( ( rc = phantom_spi_wait ( phantom ) ) != 0 )
		goto err_wait;
	
	/* Reset address byte count and dummy byte count, because the
	 * PRM asks us to.
	 */
	phantom_writel ( phantom, 0, UNM_ROMUSB_ROM_ABYTE_CNT );
	udelay ( 100 ); /* according to PRM */
	phantom_writel ( phantom, 0, UNM_ROMUSB_ROM_DUMMY_BYTE_CNT );

	/* Read data, if applicable */
	if ( data_in ) {
		data = phantom_readl ( phantom, UNM_ROMUSB_ROM_RDATA );
		memcpy ( data_in, &data, sizeof ( data ) );
		DBGCP_HDA ( phantom, address, data_in, len );
	}

 err_wait:
	phantom_spi_unlock ( phantom );
 err_lock:
	return rc;
}

/***************************************************************************
 *
 * Firmware interface
 *
 */

/**
 * Wait for firmware to accept command
 *
 * @v phantom		Phantom NIC
 * @ret rc		Return status code
 */
static int phantom_wait_for_cmd ( struct phantom_nic *phantom ) {
	unsigned int retries;
	uint32_t cdrp;

	for ( retries = 0 ; retries < PHN_ISSUE_CMD_TIMEOUT_MS ; retries++ ) {
		mdelay ( 1 );
		cdrp = phantom_readl ( phantom, UNM_NIC_REG_NX_CDRP );
		if ( NX_CDRP_IS_RSP ( cdrp ) ) {
			switch ( NX_CDRP_FORM_RSP ( cdrp ) ) {
			case NX_CDRP_RSP_OK:
				return 0;
			case NX_CDRP_RSP_FAIL:
				return -EIO;
			case NX_CDRP_RSP_TIMEOUT:
				return -ETIMEDOUT;
			default:
				return -EPROTO;
			}
		}
	}

	DBGC ( phantom, "Phantom %p timed out waiting for firmware to accept "
	       "command\n", phantom );
	return -ETIMEDOUT;
}

/**
 * Issue command to firmware
 *
 * @v phantom_port	Phantom NIC port
 * @v command		Firmware command
 * @v arg1		Argument 1
 * @v arg2		Argument 2
 * @v arg3		Argument 3
 * @ret rc		Return status code
 */
static int phantom_issue_cmd ( struct phantom_nic_port *phantom_port,
			       uint32_t command, uint32_t arg1, uint32_t arg2,
			       uint32_t arg3 ) {
	struct phantom_nic *phantom = phantom_port->phantom;
	uint32_t signature;
	int rc;

	/* Issue command */
	signature = NX_CDRP_SIGNATURE_MAKE ( phantom_port->port,
					     NXHAL_VERSION );
	DBGC2 ( phantom, "Phantom %p port %d issuing command %08lx (%08lx, "
		"%08lx, %08lx)\n", phantom, phantom_port->port,
		command, arg1, arg2, arg3 );
	phantom_writel ( phantom, signature, UNM_NIC_REG_NX_SIGN );
	phantom_writel ( phantom, arg1, UNM_NIC_REG_NX_ARG1 );
	phantom_writel ( phantom, arg2, UNM_NIC_REG_NX_ARG2 );
	phantom_writel ( phantom, arg3, UNM_NIC_REG_NX_ARG3 );
	phantom_writel ( phantom, NX_CDRP_FORM_CMD ( command ),
			 UNM_NIC_REG_NX_CDRP );

	/* Wait for command to be accepted */
	if ( ( rc = phantom_wait_for_cmd ( phantom ) ) != 0 ) {
		DBGC ( phantom, "Phantom %p could not issue command: %s\n",
		       phantom, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Issue buffer-format command to firmware
 *
 * @v phantom_port	Phantom NIC port
 * @v command		Firmware command
 * @v buffer		Buffer to pass to firmware
 * @v len		Length of buffer
 * @ret rc		Return status code
 */
static int phantom_issue_buf_cmd ( struct phantom_nic_port *phantom_port,
				   uint32_t command, void *buffer,
				   size_t len ) {
	uint64_t physaddr;

	physaddr = virt_to_bus ( buffer );
	return phantom_issue_cmd ( phantom_port, command, ( physaddr >> 32 ),
				   ( physaddr & 0xffffffffUL ), len );
}

/**
 * Create Phantom RX context
 *
 * @v phantom_port	Phantom NIC port
 * @ret rc		Return status code
 */
static int phantom_create_rx_ctx ( struct phantom_nic_port *phantom_port ) {
	struct phantom_nic *phantom = phantom_port->phantom;
	struct phantom_create_rx_ctx_rqrsp *buf;
	int rc;
	
	/* Prepare request */
	buf = &phantom->dma_buf->create_rx_ctx;
	memset ( buf, 0, sizeof ( *buf ) );
	buf->hostrq.rx_ctx.host_rsp_dma_addr =
		cpu_to_le64 ( virt_to_bus ( &buf->cardrsp ) );
	buf->hostrq.rx_ctx.capabilities[0] =
		cpu_to_le32 ( NX_CAP0_LEGACY_CONTEXT | NX_CAP0_LEGACY_MN );
	buf->hostrq.rx_ctx.host_int_crb_mode =
		cpu_to_le32 ( NX_HOST_INT_CRB_MODE_SHARED );
	buf->hostrq.rx_ctx.host_rds_crb_mode =
		cpu_to_le32 ( NX_HOST_RDS_CRB_MODE_UNIQUE );
	buf->hostrq.rx_ctx.rds_ring_offset = cpu_to_le32 ( 0 );
	buf->hostrq.rx_ctx.sds_ring_offset =
		cpu_to_le32 ( sizeof ( buf->hostrq.rds ) );
	buf->hostrq.rx_ctx.num_rds_rings = cpu_to_le16 ( 1 );
	buf->hostrq.rx_ctx.num_sds_rings = cpu_to_le16 ( 1 );
	buf->hostrq.rds.host_phys_addr =
		cpu_to_le64 ( virt_to_bus ( phantom_port->desc->rds ) );
	buf->hostrq.rds.buff_size = cpu_to_le64 ( PHN_RX_BUFSIZE );
	buf->hostrq.rds.ring_size = cpu_to_le32 ( PHN_NUM_RDS );
	buf->hostrq.rds.ring_kind = cpu_to_le32 ( NX_RDS_RING_TYPE_NORMAL );
	buf->hostrq.sds.host_phys_addr =
		cpu_to_le64 ( virt_to_bus ( phantom_port->desc->sds ) );
	buf->hostrq.sds.ring_size = cpu_to_le32 ( PHN_NUM_SDS );

	DBGC ( phantom, "Phantom %p port %d creating RX context\n",
	       phantom, phantom_port->port );
	DBGC2_HDA ( phantom, virt_to_bus ( &buf->hostrq ),
		    &buf->hostrq, sizeof ( buf->hostrq ) );

	/* Issue request */
	if ( ( rc = phantom_issue_buf_cmd ( phantom_port,
					    NX_CDRP_CMD_CREATE_RX_CTX,
					    &buf->hostrq,
					    sizeof ( buf->hostrq ) ) ) != 0 ) {
		DBGC ( phantom, "Phantom %p port %d could not create RX "
		       "context: %s\n",
		       phantom, phantom_port->port, strerror ( rc ) );
		DBGC ( phantom, "Request:\n" );
		DBGC_HDA ( phantom, virt_to_bus ( &buf->hostrq ),
			   &buf->hostrq, sizeof ( buf->hostrq ) );
		DBGC ( phantom, "Response:\n" );
		DBGC_HDA ( phantom, virt_to_bus ( &buf->cardrsp ),
			   &buf->cardrsp, sizeof ( buf->cardrsp ) );
		return rc;
	}

	/* Retrieve context parameters */
	phantom_port->rx_context_id =
		le16_to_cpu ( buf->cardrsp.rx_ctx.context_id );
	phantom_port->rds_producer_crb =
		( UNM_CAM_RAM +
		  le32_to_cpu ( buf->cardrsp.rds.host_producer_crb ));
	phantom_port->sds_consumer_crb =
		( UNM_CAM_RAM +
		  le32_to_cpu ( buf->cardrsp.sds.host_consumer_crb ));

	DBGC ( phantom, "Phantom %p port %d created RX context (id %04x, "
	       "port phys %02x virt %02x)\n", phantom, phantom_port->port,
	       phantom_port->rx_context_id, buf->cardrsp.rx_ctx.phys_port,
	       buf->cardrsp.rx_ctx.virt_port );
	DBGC2_HDA ( phantom, virt_to_bus ( &buf->cardrsp ),
		    &buf->cardrsp, sizeof ( buf->cardrsp ) );
	DBGC ( phantom, "Phantom %p port %d RDS producer CRB is %08lx\n",
	       phantom, phantom_port->port, phantom_port->rds_producer_crb );
	DBGC ( phantom, "Phantom %p port %d SDS consumer CRB is %08lx\n",
	       phantom, phantom_port->port, phantom_port->sds_consumer_crb );

	return 0;
}

/**
 * Destroy Phantom RX context
 *
 * @v phantom_port	Phantom NIC port
 * @ret rc		Return status code
 */
static void phantom_destroy_rx_ctx ( struct phantom_nic_port *phantom_port ) {
	struct phantom_nic *phantom = phantom_port->phantom;
	int rc;
	
	DBGC ( phantom, "Phantom %p port %d destroying RX context (id %04x)\n",
	       phantom, phantom_port->port, phantom_port->rx_context_id );

	/* Issue request */
	if ( ( rc = phantom_issue_cmd ( phantom_port,
					NX_CDRP_CMD_DESTROY_RX_CTX,
					phantom_port->rx_context_id,
					NX_DESTROY_CTX_RESET, 0 ) ) != 0 ) {
		DBGC ( phantom, "Phantom %p port %d could not destroy RX "
		       "context: %s\n",
		       phantom, phantom_port->port, strerror ( rc ) );
		/* We're probably screwed */
		return;
	}

	/* Clear context parameters */
	phantom_port->rx_context_id = 0;
	phantom_port->rds_producer_crb = 0;
	phantom_port->sds_consumer_crb = 0;

	/* Reset software counters */
	phantom_port->rds_producer_idx = 0;
	phantom_port->rds_consumer_idx = 0;
	phantom_port->sds_consumer_idx = 0;
}

/**
 * Create Phantom TX context
 *
 * @v phantom_port	Phantom NIC port
 * @ret rc		Return status code
 */
static int phantom_create_tx_ctx ( struct phantom_nic_port *phantom_port ) {
	struct phantom_nic *phantom = phantom_port->phantom;
	struct phantom_create_tx_ctx_rqrsp *buf;
	int rc;

	/* Prepare request */
	buf = &phantom->dma_buf->create_tx_ctx;
	memset ( buf, 0, sizeof ( *buf ) );
	buf->hostrq.tx_ctx.host_rsp_dma_addr =
		cpu_to_le64 ( virt_to_bus ( &buf->cardrsp ) );
	buf->hostrq.tx_ctx.cmd_cons_dma_addr =
		cpu_to_le64 ( virt_to_bus ( &phantom_port->desc->cmd_cons ) );
	buf->hostrq.tx_ctx.dummy_dma_addr =
		cpu_to_le64 ( virt_to_bus ( phantom->dma_buf->dummy_dma ) );
	buf->hostrq.tx_ctx.capabilities[0] =
		cpu_to_le32 ( NX_CAP0_LEGACY_CONTEXT | NX_CAP0_LEGACY_MN );
	buf->hostrq.tx_ctx.host_int_crb_mode =
		cpu_to_le32 ( NX_HOST_INT_CRB_MODE_SHARED );
	buf->hostrq.tx_ctx.cds_ring.host_phys_addr =
		cpu_to_le64 ( virt_to_bus ( phantom_port->desc->cds ) );
	buf->hostrq.tx_ctx.cds_ring.ring_size = cpu_to_le32 ( PHN_NUM_CDS );

	DBGC ( phantom, "Phantom %p port %d creating TX context\n",
	       phantom, phantom_port->port );
	DBGC2_HDA ( phantom, virt_to_bus ( &buf->hostrq ),
		    &buf->hostrq, sizeof ( buf->hostrq ) );

	/* Issue request */
	if ( ( rc = phantom_issue_buf_cmd ( phantom_port,
					    NX_CDRP_CMD_CREATE_TX_CTX,
					    &buf->hostrq,
					    sizeof ( buf->hostrq ) ) ) != 0 ) {
		DBGC ( phantom, "Phantom %p port %d could not create TX "
		       "context: %s\n",
		       phantom, phantom_port->port, strerror ( rc ) );
		DBGC ( phantom, "Request:\n" );
		DBGC_HDA ( phantom, virt_to_bus ( &buf->hostrq ),
			   &buf->hostrq, sizeof ( buf->hostrq ) );
		DBGC ( phantom, "Response:\n" );
		DBGC_HDA ( phantom, virt_to_bus ( &buf->cardrsp ),
			   &buf->cardrsp, sizeof ( buf->cardrsp ) );
		return rc;
	}

	/* Retrieve context parameters */
	phantom_port->tx_context_id =
		le16_to_cpu ( buf->cardrsp.tx_ctx.context_id );
	phantom_port->cds_producer_crb =
		( UNM_CAM_RAM +
		  le32_to_cpu(buf->cardrsp.tx_ctx.cds_ring.host_producer_crb));

	DBGC ( phantom, "Phantom %p port %d created TX context (id %04x, "
	       "port phys %02x virt %02x)\n", phantom, phantom_port->port,
	       phantom_port->tx_context_id, buf->cardrsp.tx_ctx.phys_port,
	       buf->cardrsp.tx_ctx.virt_port );
	DBGC2_HDA ( phantom, virt_to_bus ( &buf->cardrsp ),
		    &buf->cardrsp, sizeof ( buf->cardrsp ) );
	DBGC ( phantom, "Phantom %p port %d CDS producer CRB is %08lx\n",
	       phantom, phantom_port->port, phantom_port->cds_producer_crb );

	return 0;
}

/**
 * Destroy Phantom TX context
 *
 * @v phantom_port	Phantom NIC port
 * @ret rc		Return status code
 */
static void phantom_destroy_tx_ctx ( struct phantom_nic_port *phantom_port ) {
	struct phantom_nic *phantom = phantom_port->phantom;
	int rc;
	
	DBGC ( phantom, "Phantom %p port %d destroying TX context (id %04x)\n",
	       phantom, phantom_port->port, phantom_port->tx_context_id );

	/* Issue request */
	if ( ( rc = phantom_issue_cmd ( phantom_port,
					NX_CDRP_CMD_DESTROY_TX_CTX,
					phantom_port->tx_context_id,
					NX_DESTROY_CTX_RESET, 0 ) ) != 0 ) {
		DBGC ( phantom, "Phantom %p port %d could not destroy TX "
		       "context: %s\n",
		       phantom, phantom_port->port, strerror ( rc ) );
		/* We're probably screwed */
		return;
	}

	/* Clear context parameters */
	phantom_port->tx_context_id = 0;
	phantom_port->cds_producer_crb = 0;

	/* Reset software counters */
	phantom_port->cds_producer_idx = 0;
	phantom_port->cds_consumer_idx = 0;
}

/***************************************************************************
 *
 * Descriptor ring management
 *
 */

/**
 * Allocate Phantom RX descriptor
 *
 * @v phantom_port	Phantom NIC port
 * @ret index		RX descriptor index, or negative error
 */
static int phantom_alloc_rds ( struct phantom_nic_port *phantom_port ) {
	struct phantom_nic *phantom = phantom_port->phantom;
	unsigned int rds_producer_idx;
	unsigned int next_rds_producer_idx;

	/* Check for space in the ring.  RX descriptors are consumed
	 * out of order, but they are *read* by the hardware in strict
	 * order.  We maintain a pessimistic consumer index, which is
	 * guaranteed never to be an overestimate of the number of
	 * descriptors read by the hardware.
	 */
	rds_producer_idx = phantom_port->rds_producer_idx;
	next_rds_producer_idx = ( ( rds_producer_idx + 1 ) % PHN_NUM_RDS );
	if ( next_rds_producer_idx == phantom_port->rds_consumer_idx ) {
		DBGC ( phantom, "Phantom %p port %d RDS ring full (index %d "
		       "not consumed)\n", phantom, phantom_port->port,
		       next_rds_producer_idx );
		return -ENOBUFS;
	}

	return rds_producer_idx;
}

/**
 * Post Phantom RX descriptor
 *
 * @v phantom_port	Phantom NIC port
 * @v rds		RX descriptor
 */
static void phantom_post_rds ( struct phantom_nic_port *phantom_port,
			       struct phantom_rds *rds ) {
	struct phantom_nic *phantom = phantom_port->phantom;
	unsigned int rds_producer_idx;
	unsigned int next_rds_producer_idx;
	struct phantom_rds *entry;

	/* Copy descriptor to ring */
	rds_producer_idx = phantom_port->rds_producer_idx;
	entry = &phantom_port->desc->rds[rds_producer_idx];
	memcpy ( entry, rds, sizeof ( *entry ) );
	DBGC2 ( phantom, "Phantom %p port %d posting RDS %ld (slot %d):\n",
		phantom, phantom_port->port, NX_GET ( rds, handle ),
		rds_producer_idx );
	DBGC2_HDA ( phantom, virt_to_bus ( entry ), entry, sizeof ( *entry ) );

	/* Update producer index */
	next_rds_producer_idx = ( ( rds_producer_idx + 1 ) % PHN_NUM_RDS );
	phantom_port->rds_producer_idx = next_rds_producer_idx;
	wmb();
	phantom_writel ( phantom, phantom_port->rds_producer_idx,
			 phantom_port->rds_producer_crb );
}

/**
 * Allocate Phantom TX descriptor
 *
 * @v phantom_port	Phantom NIC port
 * @ret index		TX descriptor index, or negative error
 */
static int phantom_alloc_cds ( struct phantom_nic_port *phantom_port ) {
	struct phantom_nic *phantom = phantom_port->phantom;
	unsigned int cds_producer_idx;
	unsigned int next_cds_producer_idx;

	/* Check for space in the ring.  TX descriptors are consumed
	 * in strict order, so we just check for a collision against
	 * the consumer index.
	 */
	cds_producer_idx = phantom_port->cds_producer_idx;
	next_cds_producer_idx = ( ( cds_producer_idx + 1 ) % PHN_NUM_CDS );
	if ( next_cds_producer_idx == phantom_port->cds_consumer_idx ) {
		DBGC ( phantom, "Phantom %p port %d CDS ring full (index %d "
		       "not consumed)\n", phantom, phantom_port->port,
		       next_cds_producer_idx );
		return -ENOBUFS;
	}

	return cds_producer_idx;
}

/**
 * Post Phantom TX descriptor
 *
 * @v phantom_port	Phantom NIC port
 * @v cds		TX descriptor
 */
static void phantom_post_cds ( struct phantom_nic_port *phantom_port,
			       union phantom_cds *cds ) {
	struct phantom_nic *phantom = phantom_port->phantom;
	unsigned int cds_producer_idx;
	unsigned int next_cds_producer_idx;
	union phantom_cds *entry;

	/* Copy descriptor to ring */
	cds_producer_idx = phantom_port->cds_producer_idx;
	entry = &phantom_port->desc->cds[cds_producer_idx];
	memcpy ( entry, cds, sizeof ( *entry ) );
	DBGC2 ( phantom, "Phantom %p port %d posting CDS %d:\n",
		phantom, phantom_port->port, cds_producer_idx );
	DBGC2_HDA ( phantom, virt_to_bus ( entry ), entry, sizeof ( *entry ) );

	/* Update producer index */
	next_cds_producer_idx = ( ( cds_producer_idx + 1 ) % PHN_NUM_CDS );
	phantom_port->cds_producer_idx = next_cds_producer_idx;
	wmb();
	phantom_writel ( phantom, phantom_port->cds_producer_idx,
			 phantom_port->cds_producer_crb );
}

/***************************************************************************
 *
 * MAC address management
 *
 */

/**
 * Add/remove MAC address
 *
 * @v phantom_port	Phantom NIC port
 * @v ll_addr		MAC address to add or remove
 * @v opcode		MAC request opcode
 * @ret rc		Return status code
 */
static int phantom_update_macaddr ( struct phantom_nic_port *phantom_port,
				    const uint8_t *ll_addr,
				    unsigned int opcode ) {
	union phantom_cds cds;
	int index;

	/* Get descriptor ring entry */
	index = phantom_alloc_cds ( phantom_port );
	if ( index < 0 )
		return index;

	/* Fill descriptor ring entry */
	memset ( &cds, 0, sizeof ( cds ) );
	NX_FILL_1 ( &cds, 0,
		    nic_request.common.opcode, UNM_NIC_REQUEST );
	NX_FILL_2 ( &cds, 1,
		    nic_request.header.opcode, UNM_MAC_EVENT,
		    nic_request.header.context_id, phantom_port->port );
	NX_FILL_7 ( &cds, 2,
		    nic_request.body.mac_request.opcode, opcode,
		    nic_request.body.mac_request.mac_addr_0, ll_addr[0],
		    nic_request.body.mac_request.mac_addr_1, ll_addr[1],
		    nic_request.body.mac_request.mac_addr_2, ll_addr[2],
		    nic_request.body.mac_request.mac_addr_3, ll_addr[3],
		    nic_request.body.mac_request.mac_addr_4, ll_addr[4],
		    nic_request.body.mac_request.mac_addr_5, ll_addr[5] );

	/* Post descriptor */
	phantom_post_cds ( phantom_port, &cds );

	return 0;
}

/**
 * Add MAC address
 *
 * @v phantom_port	Phantom NIC port
 * @v ll_addr		MAC address to add or remove
 * @ret rc		Return status code
 */
static inline int phantom_add_macaddr ( struct phantom_nic_port *phantom_port,
					const uint8_t *ll_addr ) {
	struct phantom_nic *phantom = phantom_port->phantom;

	DBGC ( phantom, "Phantom %p port %d adding MAC address %s\n",
	       phantom, phantom_port->port, eth_ntoa ( ll_addr ) );

	return phantom_update_macaddr ( phantom_port, ll_addr, UNM_MAC_ADD );
}

/**
 * Remove MAC address
 *
 * @v phantom_port	Phantom NIC port
 * @v ll_addr		MAC address to add or remove
 * @ret rc		Return status code
 */
static inline int phantom_del_macaddr ( struct phantom_nic_port *phantom_port,
					const uint8_t *ll_addr ) {
	struct phantom_nic *phantom = phantom_port->phantom;

	DBGC ( phantom, "Phantom %p port %d removing MAC address %s\n",
	       phantom, phantom_port->port, eth_ntoa ( ll_addr ) );

	return phantom_update_macaddr ( phantom_port, ll_addr, UNM_MAC_DEL );
}

/***************************************************************************
 *
 * Link state detection
 *
 */

/**
 * Poll link state
 *
 * @v phantom		Phantom NIC
 */
static void phantom_poll_link_state ( struct phantom_nic *phantom ) {
	struct net_device *netdev;
	struct phantom_nic_port *phantom_port;
	uint32_t xg_state_p3;
	unsigned int link;
	int i;

	/* Read link state */
	xg_state_p3 = phantom_readl ( phantom, UNM_NIC_REG_XG_STATE_P3 );

	/* If there is no change, do nothing */
	if ( phantom->link_state == xg_state_p3 )
		return;

	/* Record new link state */
	DBGC ( phantom, "Phantom %p new link state %08lx (was %08lx)\n",
	       phantom, xg_state_p3, phantom->link_state );
	phantom->link_state = xg_state_p3;

	/* Indicate per-port link state to gPXE */
	for ( i = 0 ; i < phantom->num_ports ; i++ ) {
		netdev = phantom->netdev[i];
		phantom_port = netdev_priv ( netdev );
		link = UNM_NIC_REG_XG_STATE_P3_LINK ( phantom_port->port,
						      phantom->link_state );
		switch ( link ) {
		case UNM_NIC_REG_XG_STATE_P3_LINK_UP:
			DBGC ( phantom, "Phantom %p port %d link is up\n",
			       phantom, phantom_port->port );
			netdev_link_up ( netdev );
			break;
		case UNM_NIC_REG_XG_STATE_P3_LINK_DOWN:
			DBGC ( phantom, "Phantom %p port %d link is down\n",
			       phantom, phantom_port->port );
			netdev_link_down ( netdev );
			break;
		default:
			DBGC ( phantom, "Phantom %p port %d bad link state "
			       "%d\n", phantom, phantom_port->port, link );
			break;
		}
	}
}

/***************************************************************************
 *
 * Main driver body
 *
 */

/**
 * Refill descriptor ring
 *
 * @v netdev		Net device
 */
static void phantom_refill_rx_ring ( struct net_device *netdev ) {
	struct phantom_nic_port *phantom_port = netdev_priv ( netdev );
	struct io_buffer *iobuf;
	struct phantom_rds rds;
	unsigned int handle;
	int index;

	for ( handle = 0 ; handle < PHN_RDS_MAX_FILL ; handle++ ) {

		/* Skip this index if the descriptor has not yet been
		 * consumed.
		 */
		if ( phantom_port->rds_iobuf[handle] != NULL )
			continue;

		/* Allocate descriptor ring entry */
		index = phantom_alloc_rds ( phantom_port );
		assert ( PHN_RDS_MAX_FILL < PHN_NUM_RDS );
		assert ( index >= 0 ); /* Guaranteed by MAX_FILL < NUM_RDS ) */

		/* Try to allocate an I/O buffer */
		iobuf = alloc_iob ( PHN_RX_BUFSIZE );
		if ( ! iobuf ) {
			/* Failure is non-fatal; we will retry later */
			netdev_rx_err ( netdev, NULL, -ENOMEM );
			break;
		}

		/* Fill descriptor ring entry */
		memset ( &rds, 0, sizeof ( rds ) );
		NX_FILL_2 ( &rds, 0,
			    handle, handle,
			    length, iob_len ( iobuf ) );
		NX_FILL_1 ( &rds, 1,
			    dma_addr, virt_to_bus ( iobuf->data ) );

		/* Record I/O buffer */
		assert ( phantom_port->rds_iobuf[handle] == NULL );
		phantom_port->rds_iobuf[handle] = iobuf;

		/* Post descriptor */
		phantom_post_rds ( phantom_port, &rds );
	}
}

/**
 * Open NIC
 *
 * @v netdev		Net device
 * @ret rc		Return status code
 */
static int phantom_open ( struct net_device *netdev ) {
	struct phantom_nic_port *phantom_port = netdev_priv ( netdev );
	int rc;

	/* Allocate and zero descriptor rings */
	phantom_port->desc = malloc_dma ( sizeof ( *(phantom_port->desc) ),
					  UNM_DMA_BUFFER_ALIGN );
	if ( ! phantom_port->desc ) {
		rc = -ENOMEM;
		goto err_alloc_desc;
	}
	memset ( phantom_port->desc, 0, sizeof ( *(phantom_port->desc) ) );

	/* Create RX context */
	if ( ( rc = phantom_create_rx_ctx ( phantom_port ) ) != 0 )
		goto err_create_rx_ctx;

	/* Create TX context */
	if ( ( rc = phantom_create_tx_ctx ( phantom_port ) ) != 0 )
		goto err_create_tx_ctx;

	/* Fill the RX descriptor ring */
	phantom_refill_rx_ring ( netdev );

	/* Add MAC addresses
	 *
	 * BUG5583
	 *
	 * We would like to be able to enable receiving all multicast
	 * packets (or, failing that, promiscuous mode), but the
	 * firmware doesn't currently support this.
	 */
	if ( ( rc = phantom_add_macaddr ( phantom_port,
				   netdev->ll_protocol->ll_broadcast ) ) != 0 )
		goto err_add_macaddr_broadcast;
	if ( ( rc = phantom_add_macaddr ( phantom_port,
					  netdev->ll_addr ) ) != 0 )
		goto err_add_macaddr_unicast;

	return 0;

	phantom_del_macaddr ( phantom_port, netdev->ll_addr );
 err_add_macaddr_unicast:
	phantom_del_macaddr ( phantom_port,
			      netdev->ll_protocol->ll_broadcast );
 err_add_macaddr_broadcast:
	phantom_destroy_tx_ctx ( phantom_port );
 err_create_tx_ctx:
	phantom_destroy_rx_ctx ( phantom_port );
 err_create_rx_ctx:
	free_dma ( phantom_port->desc, sizeof ( *(phantom_port->desc) ) );
	phantom_port->desc = NULL;
 err_alloc_desc:
	return rc;
}

/**
 * Close NIC
 *
 * @v netdev		Net device
 */
static void phantom_close ( struct net_device *netdev ) {
	struct phantom_nic_port *phantom_port = netdev_priv ( netdev );
	struct io_buffer *iobuf;
	unsigned int i;

	/* Shut down the port */
	phantom_del_macaddr ( phantom_port, netdev->ll_addr );
	phantom_del_macaddr ( phantom_port,
			      netdev->ll_protocol->ll_broadcast );
	phantom_destroy_tx_ctx ( phantom_port );
	phantom_destroy_rx_ctx ( phantom_port );
	free_dma ( phantom_port->desc, sizeof ( *(phantom_port->desc) ) );
	phantom_port->desc = NULL;

	/* Flush any uncompleted descriptors */
	for ( i = 0 ; i < PHN_RDS_MAX_FILL ; i++ ) {
		iobuf = phantom_port->rds_iobuf[i];
		if ( iobuf ) {
			free_iob ( iobuf );
			phantom_port->rds_iobuf[i] = NULL;
		}
	}
	for ( i = 0 ; i < PHN_NUM_CDS ; i++ ) {
		iobuf = phantom_port->cds_iobuf[i];
		if ( iobuf ) {
			netdev_tx_complete_err ( netdev, iobuf, -ECANCELED );
			phantom_port->cds_iobuf[i] = NULL;
		}
	}
}

/** 
 * Transmit packet
 *
 * @v netdev	Network device
 * @v iobuf	I/O buffer
 * @ret rc	Return status code
 */
static int phantom_transmit ( struct net_device *netdev,
			      struct io_buffer *iobuf ) {
	struct phantom_nic_port *phantom_port = netdev_priv ( netdev );
	union phantom_cds cds;
	int index;

	/* Get descriptor ring entry */
	index = phantom_alloc_cds ( phantom_port );
	if ( index < 0 )
		return index;

	/* Fill descriptor ring entry */
	memset ( &cds, 0, sizeof ( cds ) );
	NX_FILL_3 ( &cds, 0,
		    tx.opcode, UNM_TX_ETHER_PKT,
		    tx.num_buffers, 1,
		    tx.length, iob_len ( iobuf ) );
	NX_FILL_2 ( &cds, 2,
		    tx.port, phantom_port->port,
		    tx.context_id, phantom_port->port );
	NX_FILL_1 ( &cds, 4,
		    tx.buffer1_dma_addr, virt_to_bus ( iobuf->data ) );
	NX_FILL_1 ( &cds, 5,
		    tx.buffer1_length, iob_len ( iobuf ) );

	/* Record I/O buffer */
	assert ( phantom_port->cds_iobuf[index] == NULL );
	phantom_port->cds_iobuf[index] = iobuf;

	/* Post descriptor */
	phantom_post_cds ( phantom_port, &cds );

	return 0;
}

/**
 * Poll for received packets
 *
 * @v netdev	Network device
 */
static void phantom_poll ( struct net_device *netdev ) {
	struct phantom_nic_port *phantom_port = netdev_priv ( netdev );
	struct phantom_nic *phantom = phantom_port->phantom;
	struct io_buffer *iobuf;
	unsigned int cds_consumer_idx;
	unsigned int raw_new_cds_consumer_idx;
	unsigned int new_cds_consumer_idx;
	unsigned int rds_consumer_idx;
	unsigned int sds_consumer_idx;
	struct phantom_sds *sds;
	unsigned int sds_handle;
	unsigned int sds_opcode;

	/* Check for TX completions */
	cds_consumer_idx = phantom_port->cds_consumer_idx;
	raw_new_cds_consumer_idx = phantom_port->desc->cmd_cons;
	new_cds_consumer_idx = le32_to_cpu ( raw_new_cds_consumer_idx );
	while ( cds_consumer_idx != new_cds_consumer_idx ) {
		DBGC2 ( phantom, "Phantom %p port %d CDS %d complete\n",
			phantom, phantom_port->port, cds_consumer_idx );
		/* Completions may be for commands other than TX, so
		 * there may not always be an associated I/O buffer.
		 */
		if ( ( iobuf = phantom_port->cds_iobuf[cds_consumer_idx] ) ) {
			netdev_tx_complete ( netdev, iobuf );
			phantom_port->cds_iobuf[cds_consumer_idx] = NULL;
		}
		cds_consumer_idx = ( ( cds_consumer_idx + 1 ) % PHN_NUM_CDS );
		phantom_port->cds_consumer_idx = cds_consumer_idx;
	}

	/* Check for received packets */
	rds_consumer_idx = phantom_port->rds_consumer_idx;
	sds_consumer_idx = phantom_port->sds_consumer_idx;
	while ( 1 ) {
		sds = &phantom_port->desc->sds[sds_consumer_idx];
		if ( NX_GET ( sds, owner ) == 0 )
			break;

		DBGC2 ( phantom, "Phantom %p port %d SDS %d status:\n",
			phantom, phantom_port->port, sds_consumer_idx );
		DBGC2_HDA ( phantom, virt_to_bus ( sds ), sds, sizeof (*sds) );

		/* Check received opcode */
		sds_opcode = NX_GET ( sds, opcode );
		switch ( sds_opcode ) {
		case UNM_RXPKT_DESC:
		case UNM_SYN_OFFLOAD:
			/* Process received packet */
			sds_handle = NX_GET ( sds, handle );
			iobuf = phantom_port->rds_iobuf[sds_handle];
			assert ( iobuf != NULL );
			iob_put ( iobuf, NX_GET ( sds, total_length ) );
			iob_pull ( iobuf, NX_GET ( sds, pkt_offset ) );
			DBGC2 ( phantom, "Phantom %p port %d RDS %d "
				"complete\n",
				phantom, phantom_port->port, sds_handle );
			netdev_rx ( netdev, iobuf );
			phantom_port->rds_iobuf[sds_handle] = NULL;
			break;
		default:
			DBGC ( phantom, "Phantom %p port %d unexpected SDS "
			       "opcode %02x\n",
			       phantom, phantom_port->port, sds_opcode );
			DBGC_HDA ( phantom, virt_to_bus ( sds ),
				   sds, sizeof ( *sds ) );
			break;
		}
			
		/* Update RDS consumer counter.  This is a lower bound
		 * for the number of descriptors that have been read
		 * by the hardware, since the hardware must have read
		 * at least one descriptor for each completion that we
		 * receive.
		 */
		rds_consumer_idx = ( ( rds_consumer_idx + 1 ) % PHN_NUM_RDS );
		phantom_port->rds_consumer_idx = rds_consumer_idx;

		/* Clear status descriptor */
		memset ( sds, 0, sizeof ( *sds ) );

		/* Update SDS consumer index */
		sds_consumer_idx = ( ( sds_consumer_idx + 1 ) % PHN_NUM_SDS );
		phantom_port->sds_consumer_idx = sds_consumer_idx;
		wmb();
		phantom_writel ( phantom, phantom_port->sds_consumer_idx,
				 phantom_port->sds_consumer_crb );
	}

	/* Refill the RX descriptor ring */
	phantom_refill_rx_ring ( netdev );

	/* Occasionally poll the link state */
	if ( phantom_port->link_poll_timer-- == 0 ) {
		phantom_poll_link_state ( phantom );
		/* Reset the link poll timer */
		phantom_port->link_poll_timer = PHN_LINK_POLL_FREQUENCY;
	}
}

/**
 * Enable/disable interrupts
 *
 * @v netdev	Network device
 * @v enable	Interrupts should be enabled
 */
static void phantom_irq ( struct net_device *netdev, int enable ) {
	struct phantom_nic_port *phantom_port = netdev_priv ( netdev );
	struct phantom_nic *phantom = phantom_port->phantom;
	static const unsigned long sw_int_mask_reg[UNM_FLASH_NUM_PORTS] = {
		UNM_NIC_REG_SW_INT_MASK_0,
		UNM_NIC_REG_SW_INT_MASK_1,
		UNM_NIC_REG_SW_INT_MASK_2,
		UNM_NIC_REG_SW_INT_MASK_3
	};

	phantom_writel ( phantom,
			 ( enable ? 1 : 0 ),
			 sw_int_mask_reg[phantom_port->port] );
}

/** Phantom net device operations */
static struct net_device_operations phantom_operations = {
	.open		= phantom_open,
	.close		= phantom_close,
	.transmit	= phantom_transmit,
	.poll		= phantom_poll,
	.irq		= phantom_irq,
};

/**
 * Map Phantom CRB window
 *
 * @v phantom		Phantom NIC
 * @ret rc		Return status code
 */
static int phantom_map_crb ( struct phantom_nic *phantom,
			     struct pci_device *pci ) {
	unsigned long bar0_start;
	unsigned long bar0_size;

	/* CRB window is always in the last 32MB of BAR0 (which may be
	 * a 32MB or a 128MB BAR).
	 */
	bar0_start = pci_bar_start ( pci, PCI_BASE_ADDRESS_0 );
	bar0_size = pci_bar_size ( pci, PCI_BASE_ADDRESS_0 );
	DBGC ( phantom, "Phantom %p BAR0 is %08lx+%lx\n",
	       phantom, bar0_start, bar0_size );

	switch ( bar0_size ) {
	case ( 128 * 1024 * 1024 ) :
		DBGC ( phantom, "Phantom %p has 128MB BAR\n", phantom );
		phantom->crb_access = phantom_crb_access_128m;
		break;
	case ( 32 * 1024 * 1024 ) :
		DBGC ( phantom, "Phantom %p has 32MB BAR\n", phantom );
		phantom->crb_access = phantom_crb_access_32m;
		break;
	case ( 2 * 1024 * 1024 ) :
		DBGC ( phantom, "Phantom %p has 2MB BAR\n", phantom );
		phantom->crb_access = phantom_crb_access_2m;
		break;
	default:
		DBGC ( phantom, "Phantom %p has bad BAR size\n", phantom );
		return -EINVAL;
	}

	phantom->bar0 = ioremap ( bar0_start, bar0_size );
	if ( ! phantom->bar0 ) {
		DBGC ( phantom, "Phantom %p could not map BAR0\n", phantom );
		return -EIO;
	}

	/* Mark current CRB window as invalid, so that the first
	 * read/write will set the current window.
	 */
	phantom->crb_window = -1UL;

	return 0;
}

/**
 * Read Phantom flash contents
 *
 * @v phantom		Phantom NIC
 * @ret rc		Return status code
 */
static int phantom_read_flash ( struct phantom_nic *phantom ) {
	struct unm_board_info board_info;
	int rc;

	/* Initialise flash access */
	phantom->spi_bus.rw = phantom_spi_rw;
	phantom->flash.bus = &phantom->spi_bus;
	init_m25p32 ( &phantom->flash );
	/* Phantom doesn't support greater than 4-byte block sizes */
	phantom->flash.nvs.block_size = UNM_SPI_BLKSIZE;

	/* Read and verify board information */
	if ( ( rc = nvs_read ( &phantom->flash.nvs, UNM_BRDCFG_START,
			       &board_info, sizeof ( board_info ) ) ) != 0 ) {
		DBGC ( phantom, "Phantom %p could not read board info: %s\n",
		       phantom, strerror ( rc ) );
		return rc;
	}
	if ( board_info.magic != UNM_BDINFO_MAGIC ) {
		DBGC ( phantom, "Phantom %p has bad board info magic %lx\n",
		       phantom, board_info.magic );
		DBGC_HD ( phantom, &board_info, sizeof ( board_info ) );
		return -EINVAL;
	}
	if ( board_info.header_version != UNM_BDINFO_VERSION ) {
		DBGC ( phantom, "Phantom %p has bad board info version %lx\n",
		       phantom, board_info.header_version );
		DBGC_HD ( phantom, &board_info, sizeof ( board_info ) );
		return -EINVAL;
	}

	/* Identify board type and number of ports */
	switch ( board_info.board_type ) {
	case UNM_BRDTYPE_P3_4_GB:
		phantom->num_ports = 4;
		break;
	case UNM_BRDTYPE_P3_HMEZ:
	case UNM_BRDTYPE_P3_IMEZ:
	case UNM_BRDTYPE_P3_10G_CX4:
	case UNM_BRDTYPE_P3_10G_CX4_LP:
	case UNM_BRDTYPE_P3_10G_SFP_PLUS:
	case UNM_BRDTYPE_P3_XG_LOM:
		phantom->num_ports = 2;
		break;
	case UNM_BRDTYPE_P3_10000_BASE_T:
	case UNM_BRDTYPE_P3_10G_XFP:
		phantom->num_ports = 1;
		break;
	default:
		DBGC ( phantom, "Phantom %p unrecognised board type %#lx; "
		       "assuming single-port\n",
		       phantom, board_info.board_type );
		phantom->num_ports = 1;
		break;
	}
	DBGC ( phantom, "Phantom %p board type is %#lx (%d ports)\n",
	       phantom, board_info.board_type, phantom->num_ports );

	return 0;
}

/**
 * Initialise the Phantom command PEG
 *
 * @v phantom		Phantom NIC
 * @ret rc		Return status code
 */
static int phantom_init_cmdpeg ( struct phantom_nic *phantom ) {
	uint32_t cold_boot;
	uint32_t sw_reset;
	physaddr_t dummy_dma_phys;
	unsigned int retries;
	uint32_t cmdpeg_state;
	uint32_t last_cmdpeg_state = 0;

	/* If this was a cold boot, check that the hardware came up ok */
	cold_boot = phantom_readl ( phantom, UNM_CAM_RAM_COLD_BOOT );
	if ( cold_boot == UNM_CAM_RAM_COLD_BOOT_MAGIC ) {
		DBGC ( phantom, "Phantom %p coming up from cold boot\n",
		       phantom );
		sw_reset = phantom_readl ( phantom, UNM_ROMUSB_GLB_SW_RESET );
		if ( sw_reset != UNM_ROMUSB_GLB_SW_RESET_MAGIC ) {
			DBGC ( phantom, "Phantom %p reset failed: %08lx\n",
			       phantom, sw_reset );
			return -EIO;
		}
	} else {
		DBGC ( phantom, "Phantom %p coming up from warm boot "
		       "(%08lx)\n", phantom, cold_boot );
	}
	/* Clear cold-boot flag */
	phantom_writel ( phantom, 0, UNM_CAM_RAM_COLD_BOOT );

	/* Set port modes */
	phantom_writel ( phantom, UNM_CAM_RAM_PORT_MODE_AUTO_NEG,
			 UNM_CAM_RAM_PORT_MODE );
	phantom_writel ( phantom, UNM_CAM_RAM_PORT_MODE_AUTO_NEG_1G,
			 UNM_CAM_RAM_WOL_PORT_MODE );

	/* Pass dummy DMA area to card */
	dummy_dma_phys = virt_to_bus ( phantom->dma_buf->dummy_dma );
	DBGC ( phantom, "Phantom %p dummy DMA at %08lx\n",
	       phantom, dummy_dma_phys );
	phantom_write_hilo ( phantom, dummy_dma_phys,
			     UNM_NIC_REG_DUMMY_BUF_ADDR_LO,
			     UNM_NIC_REG_DUMMY_BUF_ADDR_HI );
	phantom_writel ( phantom, UNM_NIC_REG_DUMMY_BUF_INIT,
			 UNM_NIC_REG_DUMMY_BUF );

	/* Tell the hardware that tuning is complete */
	phantom_writel ( phantom, 1, UNM_ROMUSB_GLB_PEGTUNE_DONE );

	/* Wait for command PEG to finish initialising */
	DBGC ( phantom, "Phantom %p initialising command PEG (will take up to "
	       "%d seconds)...\n", phantom, PHN_CMDPEG_INIT_TIMEOUT_SEC );
	for ( retries = 0; retries < PHN_CMDPEG_INIT_TIMEOUT_SEC; retries++ ) {
		cmdpeg_state = phantom_readl ( phantom,
					       UNM_NIC_REG_CMDPEG_STATE );
		if ( cmdpeg_state != last_cmdpeg_state ) {
			DBGC ( phantom, "Phantom %p command PEG state is "
			       "%08lx after %d seconds...\n",
			       phantom, cmdpeg_state, retries );
			last_cmdpeg_state = cmdpeg_state;
		}
		if ( cmdpeg_state == UNM_NIC_REG_CMDPEG_STATE_INITIALIZED ) {
			/* Acknowledge the PEG initialisation */
			phantom_writel ( phantom,
				       UNM_NIC_REG_CMDPEG_STATE_INITIALIZE_ACK,
				       UNM_NIC_REG_CMDPEG_STATE );
			return 0;
		}
		mdelay ( 1000 );
	}

	DBGC ( phantom, "Phantom %p timed out waiting for command PEG to "
	       "initialise (status %08lx)\n", phantom, cmdpeg_state );
	return -ETIMEDOUT;
}

/**
 * Read Phantom MAC address
 *
 * @v phanton_port	Phantom NIC port
 * @v ll_addr		Buffer to fill with MAC address
 */
static void phantom_get_macaddr ( struct phantom_nic_port *phantom_port,
				  uint8_t *ll_addr ) {
	struct phantom_nic *phantom = phantom_port->phantom;
	union {
		uint8_t mac_addr[2][ETH_ALEN];
		uint32_t dwords[3];
	} u;
	unsigned long offset;
	int i;

	/* Read the three dwords that include this MAC address and one other */
	offset = ( UNM_CAM_RAM_MAC_ADDRS +
		   ( 12 * ( phantom_port->port / 2 ) ) );
	for ( i = 0 ; i < 3 ; i++, offset += 4 ) {
		u.dwords[i] = phantom_readl ( phantom, offset );
	}

	/* Copy out the relevant MAC address */
	for ( i = 0 ; i < ETH_ALEN ; i++ ) {
		ll_addr[ ETH_ALEN - i - 1 ] =
			u.mac_addr[ phantom_port->port & 1 ][i];
	}
	DBGC ( phantom, "Phantom %p port %d MAC address is %s\n",
	       phantom, phantom_port->port, eth_ntoa ( ll_addr ) );
}

/**
 * Initialise Phantom receive PEG
 *
 * @v phantom		Phantom NIC
 * @ret rc		Return status code
 */
static int phantom_init_rcvpeg ( struct phantom_nic *phantom ) {
	unsigned int retries;
	uint32_t rcvpeg_state;
	uint32_t last_rcvpeg_state = 0;

	DBGC ( phantom, "Phantom %p initialising receive PEG (will take up to "
	       "%d seconds)...\n", phantom, PHN_RCVPEG_INIT_TIMEOUT_SEC );
	for ( retries = 0; retries < PHN_RCVPEG_INIT_TIMEOUT_SEC; retries++ ) {
		rcvpeg_state = phantom_readl ( phantom,
					       UNM_NIC_REG_RCVPEG_STATE );
		if ( rcvpeg_state != last_rcvpeg_state ) {
			DBGC ( phantom, "Phantom %p receive PEG state is "
			       "%08lx after %d seconds...\n",
			       phantom, rcvpeg_state, retries );
			last_rcvpeg_state = rcvpeg_state;
		}
		if ( rcvpeg_state == UNM_NIC_REG_RCVPEG_STATE_INITIALIZED )
			return 0;
		mdelay ( 1000 );
	}

	DBGC ( phantom, "Phantom %p timed out waiting for receive PEG to "
	       "initialise (status %08lx)\n", phantom, rcvpeg_state );
	return -ETIMEDOUT;
}

/**
 * Probe PCI device
 *
 * @v pci		PCI device
 * @v id		PCI ID
 * @ret rc		Return status code
 */
static int phantom_probe ( struct pci_device *pci,
			   const struct pci_device_id *id __unused ) {
	struct phantom_nic *phantom;
	struct net_device *netdev;
	struct phantom_nic_port *phantom_port;
	int i;
	int rc;

	/* Phantom NICs expose multiple PCI functions, used for
	 * virtualisation.  Ignore everything except function 0.
	 */
	if ( PCI_FUNC ( pci->devfn ) != 0 )
	  return -ENODEV;

	/* Allocate Phantom device */
	phantom = zalloc ( sizeof ( *phantom ) );
	if ( ! phantom ) {
		rc = -ENOMEM;
		goto err_alloc_phantom;
	}
	pci_set_drvdata ( pci, phantom );

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Map CRB */
	if ( ( rc = phantom_map_crb ( phantom, pci ) ) != 0 )
		goto err_map_crb;

	/* Read flash information */
	if ( ( rc = phantom_read_flash ( phantom ) ) != 0 )
		goto err_read_flash;

	/* Allocate net devices for each port */
	for ( i = 0 ; i < phantom->num_ports ; i++ ) {
		netdev = alloc_etherdev ( sizeof ( *phantom_port ) );
		if ( ! netdev ) {
			rc = -ENOMEM;
			goto err_alloc_etherdev;
		}
		phantom->netdev[i] = netdev;
		netdev_init ( netdev, &phantom_operations );
		phantom_port = netdev_priv ( netdev );
		netdev->dev = &pci->dev;
		phantom_port->phantom = phantom;
		phantom_port->port = i;
	}

	/* Allocate dummy DMA buffer and perform initial hardware handshake */
	phantom->dma_buf = malloc_dma ( sizeof ( *(phantom->dma_buf) ),
					UNM_DMA_BUFFER_ALIGN );
	if ( ! phantom->dma_buf )
		goto err_dma_buf;
	if ( ( rc = phantom_init_cmdpeg ( phantom ) ) != 0 )
		goto err_init_cmdpeg;

	/* Initialise the receive firmware */
	if ( ( rc = phantom_init_rcvpeg ( phantom ) ) != 0 )
		goto err_init_rcvpeg;

	/* Read MAC addresses */
	for ( i = 0 ; i < phantom->num_ports ; i++ ) {
		phantom_port = netdev_priv ( phantom->netdev[i] );
		phantom_get_macaddr ( phantom_port,
				      phantom->netdev[i]->ll_addr );
	}

	/* Register network devices */
	for ( i = 0 ; i < phantom->num_ports ; i++ ) {
		if ( ( rc = register_netdev ( phantom->netdev[i] ) ) != 0 ) {
			DBGC ( phantom, "Phantom %p could not register port "
			       "%d: %s\n", phantom, i, strerror ( rc ) );
			goto err_register_netdev;
		}
	}

	return 0;

	i = ( phantom->num_ports - 1 );
 err_register_netdev:
	for ( ; i >= 0 ; i-- )
		unregister_netdev ( phantom->netdev[i] );
 err_init_rcvpeg:
 err_init_cmdpeg:
	free_dma ( phantom->dma_buf, sizeof ( *(phantom->dma_buf) ) );
	phantom->dma_buf = NULL;
 err_dma_buf:
	i = ( phantom->num_ports - 1 );
 err_alloc_etherdev:
	for ( ; i >= 0 ; i-- ) {
		netdev_nullify ( phantom->netdev[i] );
		netdev_put ( phantom->netdev[i] );
	}
 err_read_flash:
 err_map_crb:
	free ( phantom );
 err_alloc_phantom:
	return rc;
}

/**
 * Remove PCI device
 *
 * @v pci		PCI device
 */
static void phantom_remove ( struct pci_device *pci ) {
	struct phantom_nic *phantom = pci_get_drvdata ( pci );
	int i;

	for ( i = ( phantom->num_ports - 1 ) ; i >= 0 ; i-- )
		unregister_netdev ( phantom->netdev[i] );
	free_dma ( phantom->dma_buf, sizeof ( *(phantom->dma_buf) ) );
	phantom->dma_buf = NULL;
	for ( i = ( phantom->num_ports - 1 ) ; i >= 0 ; i-- ) {
		netdev_nullify ( phantom->netdev[i] );
		netdev_put ( phantom->netdev[i] );
	}
	free ( phantom );
}

/** Phantom PCI IDs */
static struct pci_device_id phantom_nics[] = {
	PCI_ROM ( 0x4040, 0x0100, "nx", "NX" ),
};

/** Phantom PCI driver */
struct pci_driver phantom_driver __pci_driver = {
	.ids = phantom_nics,
	.id_count = ( sizeof ( phantom_nics ) / sizeof ( phantom_nics[0] ) ),
	.probe = phantom_probe,
	.remove = phantom_remove,
};
