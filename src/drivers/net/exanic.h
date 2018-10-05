#ifndef _EXANIC_H
#define _EXANIC_H

/** @file
 *
 * Exablaze ExaNIC driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/pci.h>
#include <ipxe/ethernet.h>
#include <ipxe/uaccess.h>
#include <ipxe/retry.h>
#include <ipxe/i2c.h>
#include <ipxe/bitbash.h>

/** Maximum number of ports */
#define EXANIC_MAX_PORTS 8

/** Register BAR */
#define EXANIC_REGS_BAR PCI_BASE_ADDRESS_0

/** Transmit region BAR */
#define EXANIC_TX_BAR PCI_BASE_ADDRESS_2

/** Alignment for DMA regions */
#define EXANIC_ALIGN 0x1000

/** Flag for 32-bit DMA addresses */
#define EXANIC_DMA_32_BIT 0x00000001UL

/** Register set length */
#define EXANIC_REGS_LEN 0x2000

/** Transmit feedback region length */
#define EXANIC_TXF_LEN 0x1000

/** Transmit feedback slot
 *
 * This is a policy decision.
 */
#define EXANIC_TXF_SLOT( index ) ( 0x40 * (index) )

/** Receive region length */
#define EXANIC_RX_LEN 0x200000

/** Transmit feedback base address register */
#define EXANIC_TXF_BASE 0x0014

/** Capabilities register */
#define EXANIC_CAPS 0x0038
#define EXANIC_CAPS_100M 0x01000000UL		/**< 100Mbps supported */
#define EXANIC_CAPS_1G 0x02000000UL		/**< 1Gbps supported */
#define EXANIC_CAPS_10G 0x04000000UL		/**< 10Gbps supported */
#define EXANIC_CAPS_40G 0x08000000UL		/**< 40Gbps supported */
#define EXANIC_CAPS_100G 0x10000000UL		/**< 100Gbps supported */
#define EXANIC_CAPS_SPEED_MASK 0x1f000000UL	/**< Supported speeds mask */

/** I2C GPIO register */
#define EXANIC_I2C 0x012c

/** Power control register */
#define EXANIC_POWER 0x0138
#define EXANIC_POWER_ON 0x000000f0UL		/**< Power on PHYs */

/** Port register offset */
#define EXANIC_PORT_REGS( index ) ( 0x0200 + ( 0x40 * (index) ) )

/** Port enable register */
#define EXANIC_PORT_ENABLE 0x0000
#define EXANIC_PORT_ENABLE_ENABLED 0x00000001UL	/**< Port is enabled */

/** Port speed register */
#define EXANIC_PORT_SPEED 0x0004

/** Port status register */
#define EXANIC_PORT_STATUS 0x0008
#define EXANIC_PORT_STATUS_LINK 0x00000008UL	/**< Link is up */
#define EXANIC_PORT_STATUS_ABSENT 0x80000000UL	/**< Port is not present */

/** Port MAC address (second half) register */
#define EXANIC_PORT_MAC 0x000c

/** Port flags register */
#define EXANIC_PORT_FLAGS 0x0010
#define EXANIC_PORT_FLAGS_PROMISC 0x00000001UL	/**< Promiscuous mode */

/** Port receive chunk base address register */
#define EXANIC_PORT_RX_BASE 0x0014

/** Port transmit command register */
#define EXANIC_PORT_TX_COMMAND 0x0020

/** Port transmit region offset register */
#define EXANIC_PORT_TX_OFFSET 0x0024

/** Port transmit region length register */
#define EXANIC_PORT_TX_LEN 0x0028

/** Port MAC address (first half) register */
#define EXANIC_PORT_OUI 0x0030

/** Port interrupt configuration register */
#define EXANIC_PORT_IRQ 0x0034

/** An ExaNIC transmit chunk descriptor */
struct exanic_tx_descriptor {
	/** Feedback ID */
	uint16_t txf_id;
	/** Feedback slot */
	uint16_t txf_slot;
	/** Payload length (including padding */
	uint16_t len;
	/** Payload type */
	uint8_t type;
	/** Flags */
	uint8_t flags;
} __attribute__ (( packed ));

/** An ExaNIC transmit chunk */
struct exanic_tx_chunk {
	/** Descriptor */
	struct exanic_tx_descriptor desc;
	/** Padding */
	uint8_t pad[2];
	/** Payload data */
	uint8_t data[2038];
} __attribute__ (( packed ));

/** Raw Ethernet frame type */
#define EXANIC_TYPE_RAW 0x01

/** An ExaNIC receive chunk descriptor */
struct exanic_rx_descriptor {
	/** Timestamp */
	uint32_t timestamp;
	/** Status (valid only on final chunk) */
	uint8_t status;
	/** Length (zero except on the final chunk) */
	uint8_t len;
	/** Filter number */
	uint8_t filter;
	/** Generation */
	uint8_t generation;
} __attribute__ (( packed ));

/** An ExaNIC receive chunk */
struct exanic_rx_chunk {
	/** Payload data */
	uint8_t data[120];
	/** Descriptor */
	struct exanic_rx_descriptor desc;
} __attribute__ (( packed ));

/** Receive status error mask */
#define EXANIC_STATUS_ERROR_MASK 0x0f

/** An ExaNIC I2C bus configuration */
struct exanic_i2c_config {
	/** GPIO bit for pulling SCL low */
	uint8_t setscl;
	/** GPIO bit for pulling SDA low */
	uint8_t setsda;
	/** GPIO bit for reading SDA */
	uint8_t getsda;
};

/** EEPROM address */
#define EXANIC_EEPROM_ADDRESS 0x50

/** An ExaNIC port */
struct exanic_port {
	/** Network device */
	struct net_device *netdev;
	/** Port registers */
	void *regs;

	/** Transmit region offset */
	size_t tx_offset;
	/** Transmit region */
	void *tx;
	/** Number of transmit descriptors */
	uint16_t tx_count;
	/** Transmit producer counter */
	uint16_t tx_prod;
	/** Transmit consumer counter */
	uint16_t tx_cons;
	/** Transmit feedback slot */
	uint16_t txf_slot;
	/** Transmit feedback region */
	uint16_t *txf;

	/** Receive region */
	userptr_t rx;
	/** Receive consumer counter */
	unsigned int rx_cons;
	/** Receive I/O buffer (if any) */
	struct io_buffer *rx_iobuf;
	/** Receive status */
	int rx_rc;

	/** Port status */
	uint32_t status;
	/** Default link speed (as raw register value) */
	uint32_t default_speed;
	/** Speed capability bitmask */
	uint32_t speeds;
	/** Current attempted link speed (as a capability bit index) */
	unsigned int speed;
	/** Port status check timer */
	struct retry_timer timer;
};

/** An ExaNIC */
struct exanic {
	/** Registers */
	void *regs;
	/** Transmit region */
	void *tx;
	/** Transmit feedback region */
	void *txf;

	/** I2C bus configuration */
	struct exanic_i2c_config i2cfg;
	/** I2C bit-bashing interface */
	struct i2c_bit_basher basher;
	/** I2C serial EEPROM */
	struct i2c_device eeprom;

	/** Capabilities */
	uint32_t caps;
	/** Base MAC address */
	uint8_t mac[ETH_ALEN];

	/** Ports */
	struct exanic_port *port[EXANIC_MAX_PORTS];
};

/** Maximum used length of transmit region
 *
 * This is a policy decision to avoid overflowing the 16-bit transmit
 * producer and consumer counters.
 */
#define EXANIC_MAX_TX_LEN ( 256 * sizeof ( struct exanic_tx_chunk ) )

/** Maximum length of received packet
 *
 * This is a policy decision.
 */
#define EXANIC_MAX_RX_LEN ( ETH_FRAME_LEN + 4 /* VLAN */ + 4 /* CRC */ )

/** Interval between link state checks
 *
 * This is a policy decision.
 */
#define EXANIC_LINK_INTERVAL ( 1 * TICKS_PER_SEC )

#endif /* _EXANIC_H */
