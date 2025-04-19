#ifndef _CGEM_H
#define _CGEM_H

/** @file
 *
 * Cadence Gigabit Ethernet MAC (GEM) network driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/if_ether.h>
#include <ipxe/mii.h>
#include <ipxe/dma.h>
#include <ipxe/retry.h>

/** I/O region index */
#define CGEM_REG_IDX 0

/** I/O region length */
#define CGEM_REG_LEN 0x800

/** Network control register */
#define CGEM_NWCTRL 0x000
#define CGEM_NWCTRL_STARTTX	0x00000200	/**< Start transmission */
#define CGEM_NWCTRL_STATCLR	0x00000020	/**< Clear statistics */
#define CGEM_NWCTRL_MDEN	0x00000010	/**< MII interface enable */
#define CGEM_NWCTRL_TXEN	0x00000008	/**< Transmit enable */
#define CGEM_NWCTRL_RXEN	0x00000004	/**< Receive enable */

/** Normal value for network control register while up and running */
#define CGEM_NWCTRL_NORMAL \
	( CGEM_NWCTRL_MDEN | CGEM_NWCTRL_TXEN | CGEM_NWCTRL_RXEN )

/** Network configuration register */
#define CGEM_NWCFG 0x004

/** Network status register */
#define CGEM_NWSR 0x008
#define CGEM_NWSR_MII_IDLE	0x00000004	/**< MII interface is idle */

/** DMA configuration register */
#define CGEM_DMACR 0x010
#define CGEM_DMACR_RXBUF( x )	( ( (x) / 64 ) << 16 ) /**< RX buffer size */
#define CGEM_DMACR_TXSIZE( x )	( (x) << 10 )	/**< TX memory size */
#define CGEM_DMACR_TXSIZE_MAX \
	CGEM_DMACR_TXSIZE ( 0x1 )		/**< Max TX memory size */
#define CGEM_DMACR_RXSIZE( x )	( (x) << 8 )	/**< RX memory size */
#define CGEM_DMACR_RXSIZE_MAX \
	CGEM_DMACR_RXSIZE ( 0x3 )		/**< Max RX memory size */
#define CGEM_DMACR_BLENGTH( x )	( (x) << 0 )	/**< DMA burst length */
#define CGEM_DMACR_BLENGTH_MAX \
	CGEM_DMACR_BLENGTH ( 0x10 )		/**< Max DMA burst length */

/** RX queue base address register */
#define CGEM_RXQBASE 0x018

/** TX queue base address register */
#define CGEM_TXQBASE 0x01c

/** Interrupt disable register */
#define CGEM_IDR 0x02c
#define CGEM_IDR_ALL		0xffffffff	/**< Disable all interrupts */

/** PHY maintenance register */
#define CGEM_PHYMNTNC 0x034
#define CGEM_PHYMNTNC_CLAUSE22	0x40000000	/**< Clause 22 operation */
#define CGEM_PHYMNTNC_OP_WRITE	0x10000000	/**< Write to PHY register */
#define CGEM_PHYMNTNC_OP_READ	0x20000000	/**< Read from PHY register */
#define CGEM_PHYMNTNC_ADDR( x )	( (x) << 23 )	/**< PHY address */
#define CGEM_PHYMNTNC_REG( x ) 	( (x) << 18 )	/**< Register address */
#define CGEM_PHYMNTNC_FIXED	0x00020000	/**< Fixed value to write */
#define CGEM_PHYMNTNC_DATA_MASK	0x0000ffff	/**< Data mask */

/** Maximum time to wait for PHY access, in microseconds */
#define CGEM_MII_MAX_WAIT_US 500

/** Link state check interval */
#define CGEM_LINK_INTERVAL ( 2 * TICKS_PER_SEC )

/** Local MAC address (low half) register */
#define CGEM_LADDRL 0x088

/** Local MAC address (high half) register */
#define CGEM_LADDRH 0x08c

/** A Cadence GEM descriptor */
struct cgem_descriptor {
	/** Buffer address */
	uint32_t addr;
	/** Flags */
	uint32_t flags;
} __attribute__ (( packed ));

/** Transmit flags */
#define CGEM_TX_FL_OWNED	0x80000000	/**< Owned by software */
#define CGEM_TX_FL_WRAP		0x40000000	/**< End of descriptor ring */
#define CGEM_TX_FL_LAST		0x00008000	/**< Last buffer in frame */

/** Transmit ring length */
#define CGEM_NUM_TX_DESC 8

/** Receive flags (in buffer address) */
#define CGEM_RX_ADDR_OWNED	0x00000001	/**< Owned by software */
#define CGEM_RX_ADDR_WRAP	0x00000002	/**< End of descriptor ring */

/** Receive flags */
#define CGEM_RX_FL_LEN( x )	( (x) & 0x1fff ) /**< RX packet length */

/** Receive ring length */
#define CGEM_NUM_RX_DESC 8

/** Length of receive buffers
 *
 * Must be a multiple of 64.
 */
#define CGEM_RX_LEN 1536

/** A Cadence GEM MAC address */
union cgem_mac {
	struct {
		uint32_t low;
		uint32_t high;
	} __attribute__ (( packed )) reg;
	uint8_t raw[ETH_ALEN];
};

/** A Cadence GEM descriptor ring */
struct cgem_ring {
	/** Descriptors */
	struct cgem_descriptor *desc;
	/** Descriptor ring DMA mapping */
	struct dma_mapping map;
	/** Producer index */
	unsigned int prod;
	/** Consumer index */
	unsigned int cons;

	/** Queue base address register */
	uint8_t qbase;
	/** Number of descriptors */
	uint8_t count;
	/** Length of descriptors */
	uint16_t len;
};

/**
 * Initialise descriptor ring
 *
 * @v ring		Descriptor ring
 * @v count		Number of descriptors
 * @v qbase		Queue base address register
 */
static inline __attribute__ (( always_inline )) void
cgem_init_ring ( struct cgem_ring *ring, unsigned int count,
		 unsigned int qbase ) {

	ring->qbase = qbase;
	ring->count = count;
	ring->len = ( count * sizeof ( ring->desc[0] ) );
}

/** A Cadence GEM network card */
struct cgem_nic {
	/** Registers */
	void *regs;
	/** DMA device */
	struct dma_device *dma;
	/** Network device */
	struct net_device *netdev;
	/** Device name (for debugging) */
	const char *name;

	/** PHY interface */
	struct mii_interface mdio;
	/** PHY device */
	struct mii_device mii;
	/** Link state timer */
	struct retry_timer timer;

	/** Transmit ring */
	struct cgem_ring tx;
	/** Receive ring */
	struct cgem_ring rx;
	/** Receive I/O buffers */
	struct io_buffer *rx_iobuf[CGEM_NUM_RX_DESC];
};

#endif /* _CGEM_H */
