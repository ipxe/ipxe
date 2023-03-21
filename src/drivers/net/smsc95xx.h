#ifndef _SMSC95XX_H
#define _SMSC95XX_H

/** @file
 *
 * SMSC LAN95xx USB Ethernet driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include "smscusb.h"

/** Interrupt status register */
#define SMSC95XX_INT_STS 0x008
#define SMSC95XX_INT_STS_RXDF_INT	0x00000800UL	/**< RX FIFO overflow */
#define SMSC95XX_INT_STS_PHY_INT	0x00008000UL	/**< PHY interrupt */

/** Transmit configuration register */
#define SMSC95XX_TX_CFG 0x010
#define SMSC95XX_TX_CFG_ON		0x00000004UL	/**< TX enable */

/** Hardware configuration register */
#define SMSC95XX_HW_CFG 0x014
#define SMSC95XX_HW_CFG_BIR		0x00001000UL	/**< Bulk IN use NAK */
#define SMSC95XX_HW_CFG_LRST		0x00000008UL	/**< Soft lite reset */

/** LED GPIO configuration register */
#define SMSC95XX_LED_GPIO_CFG 0x024
#define SMSC95XX_LED_GPIO_CFG_GPCTL2(x)	( (x) << 24 )	/**< GPIO 2 control */
#define SMSC95XX_LED_GPIO_CFG_GPCTL2_NSPD_LED \
	SMSC95XX_LED_GPIO_CFG_GPCTL2 ( 1 )		/**< Link speed LED */
#define SMSC95XX_LED_GPIO_CFG_GPCTL1(x)	( (x) << 20 )	/**< GPIO 1 control */
#define SMSC95XX_LED_GPIO_CFG_GPCTL1_NLNKA_LED \
	SMSC95XX_LED_GPIO_CFG_GPCTL1 ( 1 )		/**< Activity LED */
#define SMSC95XX_LED_GPIO_CFG_GPCTL0(x)	( (x) << 16 )	/**< GPIO 0 control */
#define SMSC95XX_LED_GPIO_CFG_GPCTL0_NFDX_LED \
	SMSC95XX_LED_GPIO_CFG_GPCTL0 ( 1 )		/**< Full-duplex LED */

/** EEPROM register base */
#define SMSC95XX_E2P_BASE 0x030

/** Interrupt endpoint control register */
#define SMSC95XX_INT_EP_CTL 0x068
#define SMSC95XX_INT_EP_CTL_RXDF_EN	0x00000800UL	/**< RX FIFO overflow */
#define SMSC95XX_INT_EP_CTL_PHY_EN	0x00008000UL	/**< PHY interrupt */

/** Bulk IN delay register */
#define SMSC95XX_BULK_IN_DLY 0x06c
#define SMSC95XX_BULK_IN_DLY_SET(ticks)	( (ticks) << 0 ) /**< Delay / 16.7ns */

/** MAC control register */
#define SMSC95XX_MAC_CR 0x100
#define SMSC95XX_MAC_CR_RXALL		0x80000000UL	/**< Receive all */
#define SMSC95XX_MAC_CR_FDPX		0x00100000UL	/**< Full duplex */
#define SMSC95XX_MAC_CR_MCPAS		0x00080000UL	/**< All multicast */
#define SMSC95XX_MAC_CR_PRMS		0x00040000UL	/**< Promiscuous */
#define SMSC95XX_MAC_CR_PASSBAD		0x00010000UL	/**< Pass bad frames */
#define SMSC95XX_MAC_CR_TXEN		0x00000008UL	/**< TX enabled */
#define SMSC95XX_MAC_CR_RXEN		0x00000004UL	/**< RX enabled */

/** MAC address register base */
#define SMSC95XX_ADDR_BASE 0x104

/** MII register base */
#define SMSC95XX_MII_BASE 0x0114

/** PHY interrupt source MII register */
#define SMSC95XX_MII_PHY_INTR_SOURCE 29

/** PHY interrupt mask MII register */
#define SMSC95XX_MII_PHY_INTR_MASK 30

/** PHY interrupt: auto-negotiation complete */
#define SMSC95XX_PHY_INTR_ANEG_DONE 0x0040

/** PHY interrupt: link down */
#define SMSC95XX_PHY_INTR_LINK_DOWN 0x0010

/** Receive packet header */
struct smsc95xx_rx_header {
	/** Command word */
	uint32_t command;
} __attribute__ (( packed ));

/** Runt frame */
#define SMSC95XX_RX_RUNT 0x00004000UL

/** Late collision */
#define SMSC95XX_RX_LATE 0x00000040UL

/** CRC error */
#define SMSC95XX_RX_CRC 0x00000002UL

/** Transmit packet header */
struct smsc95xx_tx_header {
	/** Command word */
	uint32_t command;
	/** Frame length */
	uint32_t len;
} __attribute__ (( packed ));

/** First segment */
#define SMSC95XX_TX_FIRST 0x00002000UL

/** Last segment */
#define SMSC95XX_TX_LAST 0x00001000UL

/** Buffer size */
#define SMSC95XX_TX_LEN(len) ( (len) << 0 )

/** Receive statistics */
struct smsc95xx_rx_statistics {
	/** Good frames */
	uint32_t good;
	/** CRC errors */
	uint32_t crc;
	/** Runt frame errors */
	uint32_t undersize;
	/** Alignment errors */
	uint32_t alignment;
	/** Frame too long errors */
	uint32_t oversize;
	/** Later collision errors */
	uint32_t late;
	/** Bad frames */
	uint32_t bad;
	/** Dropped frames */
	uint32_t dropped;
} __attribute__ (( packed ));

/** Receive statistics */
#define SMSC95XX_RX_STATISTICS 0

/** Transmit statistics */
struct smsc95xx_tx_statistics {
	/** Good frames */
	uint32_t good;
	/** Pause frames */
	uint32_t pause;
	/** Single collisions */
	uint32_t single;
	/** Multiple collisions */
	uint32_t multiple;
	/** Excessive collisions */
	uint32_t excessive;
	/** Late collisions */
	uint32_t late;
	/** Buffer underruns */
	uint32_t underrun;
	/** Excessive deferrals */
	uint32_t deferred;
	/** Carrier errors */
	uint32_t carrier;
	/** Bad frames */
	uint32_t bad;
} __attribute__ (( packed ));

/** Transmit statistics */
#define SMSC95XX_TX_STATISTICS 1

/** Reset delay (in microseconds) */
#define SMSC95XX_RESET_DELAY_US 2

/** Bulk IN maximum fill level
 *
 * This is a policy decision.
 */
#define SMSC95XX_IN_MAX_FILL 8

/** Bulk IN buffer size */
#define SMSC95XX_IN_MTU						\
	( sizeof ( struct smsc95xx_rx_header ) +		\
	  ETH_FRAME_LEN + 4 /* possible VLAN header */		\
	  + 4 /* CRC */ )

/** Honeywell VM3 MAC address OEM string index */
#define SMSC95XX_VM3_OEM_STRING_MAC 2

#endif /* _SMSC95XX_H */
