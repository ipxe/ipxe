#ifndef _LAN78XX_H
#define _LAN78XX_H

/** @file
 *
 * Microchip LAN78xx USB Ethernet driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include "smscusb.h"
#include "smsc75xx.h"

/** Hardware configuration register */
#define LAN78XX_HW_CFG 0x0010
#define LAN78XX_HW_CFG_LED1_EN		0x00200000UL	/**< LED1 enable */
#define LAN78XX_HW_CFG_LED0_EN		0x00100000UL	/**< LED1 enable */
#define LAN78XX_HW_CFG_LRST		0x00000002UL	/**< Soft lite reset */

/** Interrupt endpoint control register */
#define LAN78XX_INT_EP_CTL 0x0098
#define LAN78XX_INT_EP_CTL_RDFO_EN	0x00400000UL	/**< RX FIFO overflow */
#define LAN78XX_INT_EP_CTL_PHY_EN	0x00020000UL	/**< PHY interrupt */

/** Bulk IN delay register */
#define LAN78XX_BULK_IN_DLY 0x0094
#define LAN78XX_BULK_IN_DLY_SET(ticks)	( (ticks) << 0 ) /**< Delay / 16.7ns */

/** EEPROM register base */
#define LAN78XX_E2P_BASE 0x0040

/** USB configuration register 0 */
#define LAN78XX_USB_CFG0 0x0080
#define LAN78XX_USB_CFG0_BIR		0x00000040UL	/**< Bulk IN use NAK */

/** Receive filtering engine control register */
#define LAN78XX_RFE_CTL 0x00b0
#define LAN78XX_RFE_CTL_AB		0x00000400UL	/**< Accept broadcast */
#define LAN78XX_RFE_CTL_AM		0x00000200UL	/**< Accept multicast */
#define LAN78XX_RFE_CTL_AU		0x00000100UL	/**< Accept unicast */

/** FIFO controller RX FIFO control register */
#define LAN78XX_FCT_RX_CTL 0x00c0
#define LAN78XX_FCT_RX_CTL_EN		0x80000000UL	/**< FCT RX enable */
#define LAN78XX_FCT_RX_CTL_BAD		0x02000000UL	/**< Store bad frames */

/** FIFO controller TX FIFO control register */
#define LAN78XX_FCT_TX_CTL 0x00c4
#define LAN78XX_FCT_TX_CTL_EN		0x80000000UL	/**< FCT TX enable */

/** MAC receive register */
#define LAN78XX_MAC_RX 0x0104
#define LAN78XX_MAC_RX_MAX_SIZE(mtu)	( (mtu) << 16 )	/**< Max frame size */
#define LAN78XX_MAC_RX_MAX_SIZE_DEFAULT \
	LAN78XX_MAC_RX_MAX_SIZE ( ETH_FRAME_LEN + 4 /* VLAN */ + 4 /* CRC */ )
#define LAN78XX_MAC_RX_FCS		0x00000010UL	/**< FCS stripping */
#define LAN78XX_MAC_RX_EN		0x00000001UL	/**< RX enable */

/** MAC transmit register */
#define LAN78XX_MAC_TX 0x0108
#define LAN78XX_MAC_TX_EN		0x00000001UL	/**< TX enable */

/** MAC receive address register base */
#define LAN78XX_RX_ADDR_BASE 0x0118

/** MII register base */
#define LAN78XX_MII_BASE 0x0120

/** PHY interrupt mask MII register */
#define LAN78XX_MII_PHY_INTR_MASK 25

/** PHY interrupt source MII register */
#define LAN78XX_MII_PHY_INTR_SOURCE 26

/** PHY interrupt: global enable */
#define LAN78XX_PHY_INTR_ENABLE 0x8000

/** PHY interrupt: link state change */
#define LAN78XX_PHY_INTR_LINK 0x2000

/** PHY interrupt: auto-negotiation failure */
#define LAN78XX_PHY_INTR_ANEG_ERR 0x0800

/** PHY interrupt: auto-negotiation complete */
#define LAN78XX_PHY_INTR_ANEG_DONE 0x0400

/** MAC address perfect filter register base */
#define LAN78XX_ADDR_FILT_BASE 0x0400

/** OTP register base */
#define LAN78XX_OTP_BASE 0x1000

/** Maximum time to wait for reset (in milliseconds) */
#define LAN78XX_RESET_MAX_WAIT_MS 100

#endif /* _LAN78XX_H */
