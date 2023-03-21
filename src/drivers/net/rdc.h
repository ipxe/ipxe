#ifndef _RDC_H
#define _RDC_H

/** @file
 *
 * RDC R6040 network driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/if_ether.h>
#include <ipxe/mii.h>

/** RDC BAR size */
#define RDC_BAR_SIZE 256

/** An RDC descriptor */
struct rdc_descriptor {
	/** Flags */
	uint16_t flags;
	/** Length */
	uint16_t len;
	/** Address */
	uint32_t addr;
	/** Next descriptor */
	uint32_t next;
	/** Reserved */
	uint32_t reserved;
} __attribute__ (( packed ));

/** Descriptor is owned by NIC */
#define RDC_FL_OWNED 0x8000

/** Packet OK */
#define RDC_FL_OK 0x4000

/** MAC control register 0 */
#define RDC_MCR0 0x00
#define RDC_MCR0_FD		0x8000	/**< Full duplex */
#define RDC_MCR0_TXEN		0x1000	/**< Transmit enable */
#define RDC_MCR0_PROMISC	0x0020	/**< Promiscuous mode */
#define RDC_MCR0_RXEN		0x0002	/**< Receive enable */

/** MAC control register 1 */
#define RDC_MCR1 0x04
#define RDC_MCR1_RST		0x0001	/**< MAC reset */

/** Maximum time to wait for reset */
#define RDC_RESET_MAX_WAIT_MS 10

/** MAC transmit poll command register */
#define RDC_MTPR 0x14
#define RDC_MTPR_TM2TX		0x0001	/**< Trigger MAC to transmit */

/** MAC receive buffer size register */
#define RDC_MRBSR 0x18

/** MAC MDIO control register */
#define RDC_MMDIO 0x20
#define RDC_MMDIO_MIIWR		0x4000	/**< MDIO write */
#define RDC_MMDIO_MIIRD		0x2000	/**< MDIO read */
#define RDC_MMDIO_PHYAD(x) ( (x) << 8 )	/**< PHY address */
#define RDC_MMDIO_REGAD(x) ( (x) << 0 )	/**< Register address */

/** Maximum time to wait for an MII read or write */
#define RDC_MII_MAX_WAIT_US 2048

/** MAC MDIO read data register */
#define RDC_MMRD 0x24

/** MAC MDIO write data register */
#define RDC_MMWD 0x28

/** MAC transmit descriptor start address */
#define RDC_MTDSA 0x2c

/** MAC receive descriptor start address */
#define RDC_MRDSA 0x34

/** MAC descriptor start address low half */
#define RDC_MxDSA_LO 0x0

/** MAC descriptor start address low half */
#define RDC_MxDSA_HI 0x4

/** MAC interrupt status register */
#define RDC_MISR 0x3c
#define RDC_MIRQ_LINK		0x0200	/**< Link status changed */
#define RDC_MIRQ_TX		0x0010	/**< Transmit complete */
#define RDC_MIRQ_RX_EARLY	0x0008	/**< Receive early interrupt */
#define RDC_MIRQ_RX_EMPTY	0x0002	/**< Receive descriptor unavailable */
#define RDC_MIRQ_RX		0x0001	/**< Receive complete */

/** MAC interrupt enable register */
#define RDC_MIER 0x40

/** MAC address word 0 */
#define RDC_MID0 0x68

/** MAC address word 1 */
#define RDC_MID1 0x6a

/** MAC address word 2 */
#define RDC_MID2 0x6c

/** MAC PHY status change configuration register */
#define RDC_MPSCCR 0x88
#define RDC_MPSCCR_EN		0x8000	/**< PHY status change enable */
#define RDC_MPSCCR_PHYAD(x) ( (x) << 8 ) /**< PHY address */
#define RDC_MPSCCR_SLOW		0x0007	/**< Poll slowly */

/** MAC state machine register */
#define RDC_MACSM 0xac
#define RDC_MACSM_RST		0x0002	/**< Reset state machine */

/** Time to wait after resetting MAC state machine */
#define RDC_MACSM_RESET_DELAY_MS 10

/** A MAC address */
union rdc_mac {
	/** Raw bytes */
	uint8_t raw[ETH_ALEN];
	/** MIDx registers */
	uint16_t mid[ ETH_ALEN / 2 ];
};

/** A descriptor ring */
struct rdc_ring {
	/** Descriptors */
	struct rdc_descriptor *desc;
	/** Descriptor ring DMA mapping */
	struct dma_mapping map;
	/** Producer index */
	unsigned int prod;
	/** Consumer index */
	unsigned int cons;

	/** Number of descriptors */
	unsigned int count;
	/** Start address register 0 */
	unsigned int reg;
};

/**
 * Initialise descriptor ring
 *
 * @v ring		Descriptor ring
 * @v count		Number of descriptors
 * @v reg		Start address register 0
 */
static inline __attribute__ (( always_inline )) void
rdc_init_ring ( struct rdc_ring *ring, unsigned int count, unsigned int reg ) {

	ring->count = count;
	ring->reg = reg;
}

/** Number of transmit descriptors
 *
 * This is a policy decision.
 */
#define RDC_NUM_TX_DESC 16

/** Number of receive descriptors
 *
 * This is a policy decision.
 */
#define RDC_NUM_RX_DESC 8

/** Receive buffer length */
#define RDC_RX_MAX_LEN ( ETH_FRAME_LEN + 4 /* VLAN */ + 4 /* CRC */ )

/** An RDC network card */
struct rdc_nic {
	/** Registers */
	void *regs;
	/** DMA device */
	struct dma_device *dma;
	/** MII interface */
	struct mii_interface mdio;
	/** MII device */
	struct mii_device mii;

	/** Transmit descriptor ring */
	struct rdc_ring tx;
	/** Receive descriptor ring */
	struct rdc_ring rx;
	/** Receive I/O buffers */
	struct io_buffer *rx_iobuf[RDC_NUM_RX_DESC];
};

#endif /* _RDC_H */
