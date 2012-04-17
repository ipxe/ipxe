#ifndef _REALTEK_H
#define _REALTEK_H

/** @file
 *
 * Realtek 10/100/1000 network card driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <ipxe/spi.h>
#include <ipxe/spi_bit.h>
#include <ipxe/nvo.h>
#include <ipxe/if_ether.h>

/** PCI memory BAR size */
#define RTL_BAR_SIZE 0x100

/** A packet descriptor */
struct realtek_descriptor {
	/** Buffer size */
	uint16_t length;
	/** Flags */
	uint16_t flags;
	/** Reserved */
	uint32_t reserved;
	/** Buffer address */
	uint64_t address;
} __attribute__ (( packed ));

/** Descriptor buffer size mask */
#define RTL_DESC_SIZE_MASK 0x3fff

/** Packet descriptor flags */
enum realtek_descriptor_flags {
	/** Descriptor is owned by NIC */
	RTL_DESC_OWN = 0x8000,
	/** End of descriptor ring */
	RTL_DESC_EOR = 0x4000,
	/** First segment descriptor */
	RTL_DESC_FS = 0x2000,
	/** Last segment descriptor */
	RTL_DESC_LS = 0x1000,
	/** Receive error summary */
	RTL_DESC_RES = 0x0020,
};

/** Descriptor ring alignment */
#define RTL_RING_ALIGN 256

/** ID Register 0 (6 bytes) */
#define RTL_IDR0 0x00

/** Multicast Register 0 (dword) */
#define RTL_MAR0 0x08

/** Multicast Register 4 (dword) */
#define RTL_MAR4 0x0c

/** Transmit Normal Priority Descriptors (qword) */
#define RTL_TNPDS 0x20

/** Number of transmit descriptors */
#define RTL_NUM_TX_DESC 4

/** Command Register (byte) */
#define RTL_CR 0x37
#define RTL_CR_RST		0x10	/**< Reset */
#define RTL_CR_RE		0x08	/**< Receiver Enable */
#define RTL_CR_TE		0x04	/**< Transmit Enable */

/** Maximum time to wait for a reset, in milliseconds */
#define RTL_RESET_MAX_WAIT_MS 100

/** Transmit Priority Polling Register (byte) */
#define RTL_TPPOLL 0x38
#define RTL_TPPOLL_NPQ		0x40	/**< Normal Priority Queue Polling */

/** Interrupt Mask Register (word) */
#define RTL_IMR 0x3c
#define RTL_IRQ_PUN_LINKCHG	0x20	/**< Packet underrun / link change */
#define RTL_IRQ_TER		0x08	/**< Transmit error */
#define RTL_IRQ_TOK		0x04	/**< Transmit OK */
#define RTL_IRQ_RER		0x02	/**< Receive error */
#define RTL_IRQ_ROK		0x01	/**< Receive OK */

/** Interrupt Status Register (word) */
#define RTL_ISR 0x3e

/** Receive (Rx) Configuration Register (dword) */
#define RTL_RCR 0x44
#define RTL_RCR_9356SEL		0x40	/**< EEPROM is a 93C56 */
#define RTL_RCR_AB		0x08	/**< Accept broadcast packets */
#define RTL_RCR_AM		0x04	/**< Accept multicast packets */
#define RTL_RCR_APM		0x02	/**< Accept physical match packets */
#define RTL_RCR_AAP		0x01	/**< Accept all packets */

/** 93C46 (93C56) Command Register (byte) */
#define RTL_9346CR 0x50
#define RTL_9346CR_EEM1		0x80	/**< Mode select bit 1 */
#define RTL_9346CR_EEM0		0x40	/**< Mode select bit 0 */
#define RTL_9346CR_EECS		0x08	/**< Chip select */
#define RTL_9346CR_EESK		0x04	/**< Clock */
#define RTL_9346CR_EEDI		0x02	/**< Data in */
#define RTL_9346CR_EEDO		0x01	/**< Data out */

/** Word offset of MAC address within EEPROM */
#define RTL_EEPROM_MAC ( 0x0e / 2 )

/** Word offset of VPD / non-volatile options within EEPROM */
#define RTL_EEPROM_VPD ( 0x40 / 2 )

/** Length of VPD / non-volatile options within EEPROM */
#define RTL_EEPROM_VPD_LEN 0x40

/** Configuration Register 1 (byte) */
#define RTL_CONFIG1 0x52
#define RTL_CONFIG1_VPD		0x02	/**< Vital Product Data enabled */

/** PHY Access Register (dword) */
#define RTL_PHYAR 0x60
#define RTL_PHYAR_FLAG		0x80000000UL /**< Read/write flag */

/** Construct PHY Access Register value */
#define RTL_PHYAR_VALUE( flag, reg, data ) ( (flag) | ( (reg) << 16 ) | (data) )

/** Extract PHY Access Register data */
#define RTL_PHYAR_DATA( value ) ( (value) & 0xffff )

/** Maximum time to wait for PHY access, in microseconds */
#define RTL_MII_MAX_WAIT_US 500

/** PHY (GMII, MII, or TBI) Status Register (byte) */
#define RTL_PHYSTATUS 0x6c
#define RTL_PHYSTATUS_LINKSTS	0x02	/**< Link ok */

/** RX Packet Maximum Size Register (word) */
#define RTL_RMS 0xda

/** C+ Command Register (word) */
#define RTL_CPCR 0xe0
#define RTL_CPCR_DAC		0x10	/**< PCI Dual Address Cycle Enable */
#define RTL_CPCR_MULRW		0x08	/**< PCI Multiple Read/Write Enable */

/** Receive Descriptor Start Address Register (qword) */
#define RTL_RDSAR 0xe4

/** Number of receive descriptors */
#define RTL_NUM_RX_DESC 4

/** Receive buffer length */
#define RTL_RX_MAX_LEN ( ETH_FRAME_LEN + 4 /* VLAN */ + 4 /* CRC */ )

/** A Realtek descriptor ring */
struct realtek_ring {
	/** Descriptors */
	struct realtek_descriptor *desc;
	/** Producer index */
	unsigned int prod;
	/** Consumer index */
	unsigned int cons;

	/** Descriptor start address register */
	unsigned int reg;
	/** Length (in bytes) */
	size_t len;
};

/**
 * Initialise descriptor ring
 *
 * @v ring		Descriptor ring
 * @v count		Number of descriptors
 * @v reg		Descriptor start address register
 */
static inline __attribute__ (( always_inline)) void
realtek_init_ring ( struct realtek_ring *ring, unsigned int count,
		    unsigned int reg ) {
	ring->len = ( count * sizeof ( ring->desc[0] ) );
	ring->reg = reg;
}

/** A Realtek network card */
struct realtek_nic {
	/** Registers */
	void *regs;
	/** SPI bit-bashing interface */
	struct spi_bit_basher spibit;
	/** EEPROM */
	struct spi_device eeprom;
	/** Non-volatile options */
	struct nvo_block nvo;
	/** MII interface */
	struct mii_interface mii;

	/** Transmit descriptor ring */
	struct realtek_ring tx;
	/** Receive descriptor ring */
	struct realtek_ring rx;
	/** Receive I/O buffers */
	struct io_buffer *rx_iobuf[RTL_NUM_RX_DESC];
};

#endif /* _REALTEK_H */
