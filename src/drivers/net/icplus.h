#ifndef _ICPLUS_H
#define _ICPLUS_H

/** @file
 *
 * IC+ network driver
 *
 */

#include <ipxe/nvs.h>
#include <ipxe/mii_bit.h>

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** BAR size */
#define ICP_BAR_SIZE 0x200

/** Alignment requirement */
#define ICP_ALIGN 0x8

/** Base address low register offset */
#define ICP_BASE_LO 0x0

/** Base address high register offset */
#define ICP_BASE_HI 0x4

/** ASIC control register (double word) */
#define ICP_ASICCTRL 0x30
#define ICP_ASICCTRL_PHYSPEED1000	0x00000040UL	/**< PHY speed 1000 */
#define ICP_ASICCTRL_GLOBALRESET	0x00010000UL	/**< Global reset */
#define ICP_ASICCTRL_DMA		0x00080000UL	/**< DMA */
#define ICP_ASICCTRL_FIFO		0x00100000UL	/**< FIFO */
#define ICP_ASICCTRL_NETWORK		0x00200000UL	/**< Network */
#define ICP_ASICCTRL_HOST		0x00400000UL	/**< Host */
#define ICP_ASICCTRL_AUTOINIT		0x00800000UL	/**< Auto init */
#define ICP_ASICCTRL_RESETBUSY		0x04000000UL	/**< Reset busy */

/** Maximum time to wait for reset */
#define ICP_RESET_MAX_WAIT_MS 1000

/** DMA control register (word/double word) */
#define ICP_DMACTRL 0x00
#define ICP_DMACTRL_RXPOLLNOW		0x0010		/**< Receive poll now */
#define ICP_DMACTRL_TXPOLLNOW 		0x1000		/**< Transmit poll now */

/** EEPROM control register (word) */
#define ICP_EEPROMCTRL 0x4a
#define ICP_EEPROMCTRL_ADDRESS( x )	( (x) << 0 )	/**< Address */
#define ICP_EEPROMCTRL_OPCODE( x )	( (x) << 8 )	/**< Opcode */
#define ICP_EEPROMCTRL_OPCODE_READ \
	ICP_EEPROMCTRL_OPCODE ( 2 )			/**< Read register */
#define ICP_EEPROMCTRL_BUSY		0x8000		/**< EEPROM busy */

/** Maximum time to wait for reading EEPROM */
#define ICP_EEPROM_MAX_WAIT_MS 1000

/** EEPROM word length */
#define ICP_EEPROM_WORD_LEN_LOG2 1

/** Minimum EEPROM size, in words */
#define ICP_EEPROM_MIN_SIZE_WORDS 0x20

/** Address of MAC address within EEPROM */
#define ICP_EEPROM_MAC 0x10

/** EEPROM data register (word) */
#define ICP_EEPROMDATA 0x48

/** Interupt status register (word) */
#define ICP_INTSTATUS 0x5e
#define ICP_INTSTATUS_TXCOMPLETE	0x0004		/**< TX complete */
#define ICP_INTSTATUS_LINKEVENT		0x0100		/**< Link event */
#define ICP_INTSTATUS_RXDMACOMPLETE	0x0400		/**< RX DMA complete */

/** MAC control register (double word) */
#define ICP_MACCTRL 0x6c
#define ICP_MACCTRL_DUPLEX		0x00000020UL	/**< Duplex select */
#define ICP_MACCTRL_TXENABLE		0x01000000UL	/**< TX enable */
#define ICP_MACCTRL_TXDISABLE		0x02000000UL	/**< TX disable */
#define ICP_MACCTRL_RXENABLE		0x08000000UL	/**< RX enable */
#define ICP_MACCTRL_RXDISABLE		0x10000000UL	/**< RX disable */

/** PHY control register (byte) */
#define ICP_PHYCTRL 0x76
#define ICP_PHYCTRL_MGMTCLK		0x01		/**< Management clock */
#define ICP_PHYCTRL_MGMTDATA		0x02		/**< Management data */
#define ICP_PHYCTRL_MGMTDIR		0x04		/**< Management direction */
#define ICP_PHYCTRL_LINKSPEED		0xc0		/**< Link speed */

/** Receive mode register (word) */
#define ICP_RXMODE 0x88
#define ICP_RXMODE_UNICAST		0x0001		/**< Receive unicast */
#define ICP_RXMODE_MULTICAST		0x0002		/**< Receice multicast */
#define ICP_RXMODE_BROADCAST		0x0004		/**< Receive broadcast */
#define ICP_RXMODE_ALLFRAMES		0x0008		/**< Receive all frames */

/** List pointer receive register */
#define ICP_RFDLISTPTR 0x1c

/** List pointer transmit register */
#define ICP_TFDLISTPTR 0x10

/** Transmit status register */
#define ICP_TXSTATUS 0x60
#define ICP_TXSTATUS_ERROR		0x00000001UL	/**< TX error */

/** Data fragment */
union icplus_fragment {
	/** Address of data */
	uint64_t address;
	/** Length */
	struct {
		/** Reserved */
		uint8_t reserved[6];
		/** Length of data */
		uint16_t len;
	};
};

/** Transmit or receive descriptor */
struct icplus_descriptor {
	/** Address of next descriptor */
	uint64_t next;
	/** Actual length */
	uint16_t len;
	/** Flags */
	uint8_t flags;
	/** Control */
	uint8_t control;
	/** VLAN */
	uint16_t vlan;
	/** Reserved */
	uint16_t reserved_a;
	/** Data buffer */
	union icplus_fragment data;
	/** Reserved */
	uint8_t reserved_b[8];
};

/** Descriptor complete */
#define ICP_DONE 0x80

/** Transmit alignment disabled */
#define ICP_TX_UNALIGN 0x01

/** Request transmit completion */
#define ICP_TX_INDICATE 0x40

/** Sole transmit fragment */
#define ICP_TX_SOLE_FRAG 0x01

/** Recieve frame overrun error */
#define ICP_RX_ERR_OVERRUN 0x01

/** Receive runt frame error */
#define ICP_RX_ERR_RUNT 0x02

/** Receive alignment error */
#define ICP_RX_ERR_ALIGN 0x04

/** Receive FCS error */
#define ICP_RX_ERR_FCS 0x08

/** Receive oversized frame error */
#define ICP_RX_ERR_OVERSIZED 0x10

/** Recieve length error */
#define ICP_RX_ERR_LEN 0x20

/** Descriptor ring */
struct icplus_ring {
	/** Producer counter */
	unsigned int prod;
	/** Consumer counter */
	unsigned int cons;
	/** Ring entries */
	struct icplus_descriptor *entry;
	/* List pointer register */
	unsigned int listptr;
};

/** Number of descriptors */
#define ICP_NUM_DESC 4

/** Maximum receive packet length */
#define ICP_RX_MAX_LEN ETH_FRAME_LEN

/** An IC+ network card */
struct icplus_nic {
	/** Registers */
	void *regs;
	/** EEPROM */
	struct nvs_device eeprom;
	/** MII bit bashing interface */
	struct mii_bit_basher miibit;
	/** MII device */
	struct mii_device mii;
	/** Transmit descriptor ring */
	struct icplus_ring tx;
	/** Receive descriptor ring */
	struct icplus_ring rx;
	/** Receive I/O buffers */
	struct io_buffer *rx_iobuf[ICP_NUM_DESC];
};

#endif /* _ICPLUS_H */
