#ifndef _DWMAC_H
#define _DWMAC_H

/** @file
 *
 * Synopsys DesignWare MAC network driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/if_ether.h>

/** I/O region index */
#define DWMAC_REG_IDX 0

/** I/O region length */
#define DWMAC_REG_LEN 0x2000

/** MAC register block */
#define DWMAC_MAC 0x0000
#define DWMAC_MAC_REG( n ) ( DWMAC_MAC + ( (n) * 4 ) )

/** MAC configuration register */
#define DWMAC_CFG DWMAC_MAC_REG ( 0 )
#define DWMAC_CFG_DO		0x00002000	/**< Disable RX own frames */
#define DWMAC_CFG_FD		0x00000800	/**< Full duplex */
#define DWMAC_CFG_TXEN		0x00000008	/**< TX enabled */
#define DWMAC_CFG_RXEN		0x00000004	/**< RX enabled */

/** MAC filter register */
#define DWMAC_FILTER DWMAC_MAC_REG ( 1 )
#define DWMAC_FILTER_PR		0x00000001	/**< Promiscuous mode */

/** Flow control register */
#define DWMAC_FLOW DWMAC_MAC_REG ( 6 )

/** Version register */
#define DWMAC_VER DWMAC_MAC_REG ( 8 )
#define DWMAC_VER_USER_MAJOR( x ) \
	( ( (x) >> 12 ) & 0xf )			/**< User major version */
#define DWMAC_VER_USER_MINOR( x ) \
	( ( (x) >> 8 ) & 0xf )			/**< User minor version */
#define DWMAC_VER_CORE_MAJOR( x ) \
	( ( (x) >> 4 ) & 0xf )			/**< Core major version */
#define DWMAC_VER_CORE_MINOR( x ) \
	( ( (x) >> 0 ) & 0xf )			/**< Core minor version */

/** Debug register */
#define DWMAC_DEBUG DWMAC_MAC_REG ( 9 )

/** Interrupt status register */
#define DWMAC_ISR DWMAC_MAC_REG ( 14 )

/** MAC address high register */
#define DWMAC_ADDRH DWMAC_MAC_REG ( 16 )

/** MAC address low register */
#define DWMAC_ADDRL DWMAC_MAC_REG ( 17 )

/** A DesignWare MAC address */
union dwmac_mac {
	struct {
		uint32_t addrl;
		uint32_t addrh;
	} __attribute__ (( packed )) reg;
	uint8_t raw[ETH_ALEN];
};

/** SGMII/RGMII status register */
#define DWMAC_GMII DWMAC_MAC_REG ( 54 )
#define DWMAC_GMII_LINK		0x00000008	/**< Link up */

/** DMA register block */
#define DWMAC_DMA 0x1000
#define DWMAC_DMA_REG( n ) ( DWMAC_DMA + ( (n) * 4 ) )

/** Bus mode register */
#define DWMAC_BUS DWMAC_DMA_REG ( 0 )
#define DWMAC_BUS_PBL4		0x01000000	/**< 4x PBL mode */
#define DWMAC_BUS_USP		0x00800000	/**< Use separate PBL */
#define DWMAC_BUS_RPBL(x)	( (x) << 17 )	/**< RX DMA PBL */
#define DWMAC_BUS_FB		0x00010000	/**< Fixed burst */
#define DWMAC_BUS_PBL(x)	( (x) << 8 )	/**< (TX) DMA PBL */
#define DWMAC_BUS_SWR		0x00000001	/**< Software reset */

/** Time to wait for software reset to complete */
#define DWMAC_RESET_MAX_WAIT_MS 500

/** Transmit poll demand register */
#define DWMAC_TXPOLL DWMAC_DMA_REG ( 1 )

/** Receive poll demand register */
#define DWMAC_RXPOLL DWMAC_DMA_REG ( 2 )

/** Receive descriptor list address register */
#define DWMAC_RXBASE DWMAC_DMA_REG ( 3 )

/** Transmit descriptor list address register */
#define DWMAC_TXBASE DWMAC_DMA_REG ( 4 )

/** Status register */
#define DWMAC_STATUS DWMAC_DMA_REG ( 5 )
#define DWMAC_STATUS_LINK	0x04000000	/**< Link status change */

/** Operation mode register */
#define DWMAC_OP DWMAC_DMA_REG ( 6 )
#define DWMAC_OP_RXSF		0x02000000	/**< RX store and forward */
#define DWMAC_OP_TXSF		0x00200000	/**< TX store and forward */
#define DWMAC_OP_TXEN		0x00002000	/**< TX enabled */
#define DWMAC_OP_RXEN		0x00000002	/**< RX enabled */

/** Packet drop counter register */
#define DWMAC_DROP DWMAC_DMA_REG ( 8 )

/** AXI bus mode register */
#define DWMAC_AXI DWMAC_DMA_REG ( 10 )

/** AHB or AXI status register */
#define DWMAC_AHB DWMAC_DMA_REG ( 11 )

/** Current transmit descriptor register */
#define DWMAC_TXDESC DWMAC_DMA_REG ( 18 )

/** Current receive descriptor register */
#define DWMAC_RXDESC DWMAC_DMA_REG ( 19 )

/** Current transmit buffer address register */
#define DWMAC_TXBUF DWMAC_DMA_REG ( 20 )

/** Current receive buffer address register */
#define DWMAC_RXBUF DWMAC_DMA_REG ( 21 )

/** Hardware feature register */
#define DWMAC_FEATURE DWMAC_DMA_REG ( 22 )

/** A frame descriptor
 *
 * We populate the descriptor with values that are valid for both
 * normal and enhanced descriptor formats, to avoid needing to care
 * about which version of the hardware we have.
 */
struct dwmac_descriptor {
	/** Completion status */
	uint32_t stat;
	/** Buffer size */
	uint16_t size;
	/** Reserved */
	uint8_t reserved_a;
	/** Ring control */
	uint8_t ctrl;
	/** Buffer address */
	uint32_t addr;
	/** Next descriptor address */
	uint32_t next;
} __attribute__ (( packed ));

/* Completion status */
#define DWMAC_STAT_OWN		0x80000000	/**< Owned by hardware */
#define DWMAC_STAT_TX_LAST	0x20000000	/**< Last segment (TX) */
#define DWMAC_STAT_TX_FIRST	0x10000000	/**< First segment (TX) */
#define DWMAC_STAT_TX_CHAIN	0x00100000	/**< Chained descriptor (TX) */
#define DWMAC_STAT_ERR		0x00008000	/**< Error summary */
#define DWMAC_STAT_RX_FIRST	0x00000200	/**< First segment (RX) */
#define DWMAC_STAT_RX_LAST	0x00000100	/**< Last segment (RX) */
#define DWMAC_STAT_RX_LEN(x) \
	( ( (x) >> 16 ) & 0x3fff )		/**< Frame length (RX) */

/** Buffer size */
#define DWMAC_SIZE_RX_CHAIN	0x4000		/**< Chained descriptor (RX) */

/* Ring control */
#define DWMAC_CTRL_TX_LAST	0x40		/**< Last segment (TX) */
#define DWMAC_CTRL_TX_FIRST	0x20		/**< First segment (TX) */
#define DWMAC_CTRL_CHAIN	0x01		/**< Chained descriptor */

/** A DesignWare descriptor ring */
struct dwmac_ring {
	/** Descriptors */
	struct dwmac_descriptor *desc;
	/** Descriptor ring DMA mapping */
	struct dma_mapping map;
	/** Producer index */
	unsigned int prod;
	/** Consumer index */
	unsigned int cons;

	/** Queue base address register (within DMA block) */
	uint8_t qbase;
	/** Number of descriptors */
	uint8_t count;
	/** Default control flags */
	uint8_t ctrl;
	/** Length of descriptors */
	size_t len;
};

/** Number of transmit descriptors */
#define DWMAC_NUM_TX_DESC 16

/** Number of receive descriptors */
#define DWMAC_NUM_RX_DESC 16

/** Length of receive buffers
 *
 * Must be a multiple of 16.
 */
#define DWMAC_RX_LEN 1536

/**
 * Initialise descriptor ring
 *
 * @v ring		Descriptor ring
 * @v count		Number of descriptors
 * @v qbase		Queue base address register
 * @v ctrl		Default descriptor control flags
 */
static inline __attribute__ (( always_inline )) void
dwmac_init_ring ( struct dwmac_ring *ring, unsigned int count,
		  unsigned int qbase, unsigned int ctrl ) {

	ring->qbase = ( qbase - DWMAC_DMA );
	ring->count = count;
	ring->ctrl = ctrl;
	ring->len = ( count * sizeof ( ring->desc[0] ) );
}

/** A DesignWare MAC network card */
struct dwmac {
	/** Registers */
	void *regs;
	/** DMA device */
	struct dma_device *dma;
	/** Device name (for debugging) */
	const char *name;

	/** Transmit ring */
	struct dwmac_ring tx;
	/** Receive ring */
	struct dwmac_ring rx;
	/** Receive I/O buffers */
	struct io_buffer *rx_iobuf[DWMAC_NUM_RX_DESC];
};

#endif /* _DWMAC_H */
