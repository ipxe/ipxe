#ifndef _SMSC75XX_H
#define _SMSC75XX_H

/** @file
 *
 * SMSC LAN75xx USB Ethernet driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include "smscusb.h"

/** Interrupt status register */
#define SMSC75XX_INT_STS 0x00c
#define SMSC75XX_INT_STS_RDFO_INT	0x00400000UL	/**< RX FIFO overflow */
#define SMSC75XX_INT_STS_PHY_INT	0x00020000UL	/**< PHY interrupt */

/** Hardware configuration register */
#define SMSC75XX_HW_CFG 0x010
#define SMSC75XX_HW_CFG_BIR		0x00000080UL	/**< Bulk IN use NAK */
#define SMSC75XX_HW_CFG_LRST		0x00000002UL	/**< Soft lite reset */

/** Interrupt endpoint control register */
#define SMSC75XX_INT_EP_CTL 0x038
#define SMSC75XX_INT_EP_CTL_RDFO_EN	0x00400000UL	/**< RX FIFO overflow */
#define SMSC75XX_INT_EP_CTL_PHY_EN	0x00020000UL	/**< PHY interrupt */

/** Bulk IN delay register */
#define SMSC75XX_BULK_IN_DLY 0x03c
#define SMSC75XX_BULK_IN_DLY_SET(ticks)	( (ticks) << 0 ) /**< Delay / 16.7ns */

/** EEPROM register base */
#define SMSC75XX_E2P_BASE 0x040

/** Receive filtering engine control register */
#define SMSC75XX_RFE_CTL 0x060
#define SMSC75XX_RFE_CTL_AB		0x00000400UL	/**< Accept broadcast */
#define SMSC75XX_RFE_CTL_AM		0x00000200UL	/**< Accept multicast */
#define SMSC75XX_RFE_CTL_AU		0x00000100UL	/**< Accept unicast */

/** FIFO controller RX FIFO control register */
#define SMSC75XX_FCT_RX_CTL 0x090
#define SMSC75XX_FCT_RX_CTL_EN		0x80000000UL	/**< FCT RX enable */
#define SMSC75XX_FCT_RX_CTL_BAD		0x02000000UL	/**< Store bad frames */

/** FIFO controller TX FIFO control register */
#define SMSC75XX_FCT_TX_CTL 0x094
#define SMSC75XX_FCT_TX_CTL_EN		0x80000000UL	/**< FCT TX enable */

/** MAC receive register */
#define SMSC75XX_MAC_RX 0x104
#define SMSC75XX_MAC_RX_MAX_SIZE(mtu)	( (mtu) << 16 )	/**< Max frame size */
#define SMSC75XX_MAC_RX_MAX_SIZE_DEFAULT \
	SMSC75XX_MAC_RX_MAX_SIZE ( ETH_FRAME_LEN + 4 /* VLAN */ + 4 /* CRC */ )
#define SMSC75XX_MAC_RX_FCS		0x00000010UL	/**< FCS stripping */
#define SMSC75XX_MAC_RX_EN		0x00000001UL	/**< RX enable */

/** MAC transmit register */
#define SMSC75XX_MAC_TX 0x108
#define SMSC75XX_MAC_TX_EN		0x00000001UL	/**< TX enable */

/** MAC receive address register base */
#define SMSC75XX_RX_ADDR_BASE 0x118

/** MII register base */
#define SMSC75XX_MII_BASE 0x120

/** PHY interrupt source MII register */
#define SMSC75XX_MII_PHY_INTR_SOURCE 29

/** PHY interrupt mask MII register */
#define SMSC75XX_MII_PHY_INTR_MASK 30

/** PHY interrupt: auto-negotiation complete */
#define SMSC75XX_PHY_INTR_ANEG_DONE 0x0040

/** PHY interrupt: link down */
#define SMSC75XX_PHY_INTR_LINK_DOWN 0x0010

/** MAC address perfect filter register base */
#define SMSC75XX_ADDR_FILT_BASE 0x300

/** Receive packet header */
struct smsc75xx_rx_header {
	/** RX command word */
	uint32_t command;
	/** VLAN tag */
	uint16_t vtag;
	/** Checksum */
	uint16_t csum;
	/** Two-byte padding used to align Ethernet payload */
	uint16_t pad;
} __attribute__ (( packed ));

/** Receive error detected */
#define SMSC75XX_RX_RED 0x00400000UL

/** Transmit packet header */
struct smsc75xx_tx_header {
	/** TX command word */
	uint32_t command;
	/** VLAN tag */
	uint16_t tag;
	/** Maximum segment size */
	uint16_t mss;
} __attribute__ (( packed ));

/** Insert frame checksum and pad */
#define SMSC75XX_TX_FCS 0x00400000UL

/** Byte count statistics */
struct smsc75xx_byte_statistics {
	/** Unicast byte count */
	uint32_t unicast;
	/** Broadcast byte count */
	uint32_t broadcast;
	/** Multicast byte count */
	uint32_t multicast;
} __attribute__ (( packed ));

/** Frame count statistics */
struct smsc75xx_frame_statistics {
	/** Unicast frames */
	uint32_t unicast;
	/** Broadcast frames */
	uint32_t broadcast;
	/** Multicast frames */
	uint32_t multicast;
	/** Pause frames */
	uint32_t pause;
	/** Frames by length category */
	uint32_t len[7];
} __attribute__ (( packed ));

/** Receive error statistics */
struct smsc75xx_rx_error_statistics {
	/** FCS errors */
	uint32_t fcs;
	/** Alignment errors */
	uint32_t alignment;
	/** Fragment errors */
	uint32_t fragment;
	/** Jabber errors */
	uint32_t jabber;
	/** Undersize frame errors */
	uint32_t undersize;
	/** Oversize frame errors */
	uint32_t oversize;
	/** Dropped frame errors */
	uint32_t dropped;
} __attribute__ (( packed ));

/** Receive statistics */
struct smsc75xx_rx_statistics {
	/** Error statistics */
	struct smsc75xx_rx_error_statistics err;
	/** Byte count statistics */
	struct smsc75xx_byte_statistics byte;
	/** Frame count statistics */
	struct smsc75xx_frame_statistics frame;
} __attribute__ (( packed ));

/** Transmit error statistics */
struct smsc75xx_tx_error_statistics {
	/** FCS errors */
	uint32_t fcs;
	/** Excess deferral errors */
	uint32_t deferral;
	/** Carrier errors */
	uint32_t carrier;
	/** Bad byte count */
	uint32_t count;
	/** Single collisions */
	uint32_t single;
	/** Multiple collisions */
	uint32_t multiple;
	/** Excession collisions */
	uint32_t excessive;
	/** Late collisions */
	uint32_t late;
} __attribute__ (( packed ));

/** Transmit statistics */
struct smsc75xx_tx_statistics {
	/** Error statistics */
	struct smsc75xx_tx_error_statistics err;
	/** Byte count statistics */
	struct smsc75xx_byte_statistics byte;
	/** Frame count statistics */
	struct smsc75xx_frame_statistics frame;
} __attribute__ (( packed ));

/** Statistics */
struct smsc75xx_statistics {
	/** Receive statistics */
	struct smsc75xx_rx_statistics rx;
	/** Transmit statistics */
	struct smsc75xx_tx_statistics tx;
} __attribute__ (( packed ));

/** Maximum time to wait for reset (in milliseconds) */
#define SMSC75XX_RESET_MAX_WAIT_MS 100

/** Bulk IN maximum fill level
 *
 * This is a policy decision.
 */
#define SMSC75XX_IN_MAX_FILL 8

/** Bulk IN buffer size */
#define SMSC75XX_IN_MTU						\
	( sizeof ( struct smsc75xx_rx_header ) +		\
	  ETH_FRAME_LEN + 4 /* possible VLAN header */ )

extern struct usb_endpoint_driver_operations smsc75xx_in_operations;

extern int smsc75xx_dump_statistics ( struct smscusb_device *smscusb );
extern int smsc75xx_transmit ( struct net_device *netdev,
			       struct io_buffer *iobuf );
extern void smsc75xx_poll ( struct net_device *netdev );

#endif /* _SMSC75XX_H */
