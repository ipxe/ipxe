#ifndef _INTELX_H
#define _INTELX_H

/** @file
 *
 * Intel 10 Gigabit Ethernet network card driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/if_ether.h>
#include "intel.h"

/** Device Control Register */
#define INTELX_CTRL 0x00000UL
#define INTELX_CTRL_LRST	0x00000008UL	/**< Link reset */
#define INTELX_CTRL_RST		0x04000000UL	/**< Device reset */

/** Device Status Register */
#define INTELX_STATUS 0x00008UL
#define INTELX_STATUS_LAN_ID	0x0000000CUL	/**< LAN ID */

/** Extended Device Control Register */
#define INTELX_CTRL_EXT	0x00018UL
#define INTELX_CTRL_EXT_DRV_LOAD	0x10000000	/**< Driver loaded */

/** Time to delay for device reset, in milliseconds */
#define INTELX_RESET_DELAY_MS 20

/** Extended Interrupt Cause Read Register */
#define INTELX_EICR 0x00800UL
#define INTELX_EIRQ_RX0		0x00000001UL	/**< RX0 (via IVAR) */
#define INTELX_EIRQ_TX0		0x00000002UL	/**< RX0 (via IVAR) */
#define INTELX_EIRQ_RXO		0x00020000UL	/**< Receive overrun */
#define INTELX_EIRQ_LSC		0x00100000UL	/**< Link status change */

/** Interrupt Mask Set/Read Register */
#define INTELX_EIMS 0x00880UL

/** Interrupt Mask Clear Register */
#define INTELX_EIMC 0x00888UL

/** Interrupt Vector Allocation Register */
#define INTELX_IVAR 0x00900UL
#define INTELX_IVAR_RX0(bit)	( (bit) << 0 )	/**< RX queue 0 allocation */
#define INTELX_IVAR_RX0_DEFAULT	INTELX_IVAR_RX0 ( 0x00 )
#define INTELX_IVAR_RX0_MASK	INTELX_IVAR_RX0 ( 0x3f )
#define INTELX_IVAR_RX0_VALID	0x00000080UL	/**< RX queue 0 valid */
#define INTELX_IVAR_TX0(bit)	( (bit) << 8 )	/**< TX queue 0 allocation */
#define INTELX_IVAR_TX0_DEFAULT	INTELX_IVAR_TX0 ( 0x01 )
#define INTELX_IVAR_TX0_MASK	INTELX_IVAR_TX0 ( 0x3f )
#define INTELX_IVAR_TX0_VALID	0x00008000UL	/**< TX queue 0 valid */

/** Receive Filter Control Register */
#define INTELX_FCTRL 0x05080UL
#define INTELX_FCTRL_MPE	0x00000100UL	/**< Multicast promiscuous */
#define INTELX_FCTRL_UPE	0x00000200UL	/**< Unicast promiscuous mode */
#define INTELX_FCTRL_BAM	0x00000400UL	/**< Broadcast accept mode */

/** Receive Address Low
 *
 * The MAC address registers RAL0/RAH0 exist at address 0x05400 for
 * the 82598 and 0x0a200 for the 82599, according to the datasheet.
 * In practice, the 82599 seems to also provide a copy of these
 * registers at 0x05400.  To aim for maximum compatibility, we try
 * both addresses when reading the initial MAC address, and set both
 * addresses when setting the MAC address.
 */
#define INTELX_RAL0 0x05400UL
#define INTELX_RAL0_ALT 0x0a200UL

/** Receive Address High */
#define INTELX_RAH0 0x05404UL
#define INTELX_RAH0_ALT 0x0a204UL
#define INTELX_RAH0_AV		0x80000000UL	/**< Address valid */

/** Receive Descriptor register block */
#define INTELX_RD 0x01000UL

/** Receive Descriptor Control Register */
#define INTELX_RXDCTL_VME	0x40000000UL	/**< Strip VLAN tag */

/** Split Receive Control Register */
#define INTELX_SRRCTL 0x02100UL
#define INTELX_SRRCTL_BSIZE(kb)	( (kb) << 0 )	/**< Receive buffer size */
#define INTELX_SRRCTL_BSIZE_DEFAULT INTELX_SRRCTL_BSIZE ( 0x02 )
#define INTELX_SRRCTL_BSIZE_MASK INTELX_SRRCTL_BSIZE ( 0x1f )

/** Receive DMA Control Register */
#define INTELX_RDRXCTL 0x02f00UL
#define INTELX_RDRXCTL_SECRC	0x00000001UL	/**< Strip CRC */

/** Receive Control Register */
#define INTELX_RXCTRL 0x03000UL
#define INTELX_RXCTRL_RXEN	0x00000001UL	/**< Receive enable */

/** Transmit DMA Control Register */
#define INTELX_DMATXCTL 0x04a80UL
#define INTELX_DMATXCTL_TE	0x00000001UL	/**< Transmit enable */

/** Transmit Descriptor register block */
#define INTELX_TD 0x06000UL

/** RX DCA Control Register */
#define INTELX_DCA_RXCTRL 0x02200UL
#define INTELX_DCA_RXCTRL_MUST_BE_ZERO 0x00001000UL /**< Must be zero */

/** MAC Core Control 0 Register */
#define INTELX_HLREG0 0x04240UL
#define INTELX_HLREG0_JUMBOEN	0x00000004UL	/**< Jumbo frame enable */

/** Maximum Frame Size Register */
#define INTELX_MAXFRS 0x04268UL
#define INTELX_MAXFRS_MFS(len)	( (len) << 16 )	/**< Maximum frame size */
#define INTELX_MAXFRS_MFS_DEFAULT \
	INTELX_MAXFRS_MFS ( ETH_FRAME_LEN + 4 /* VLAN */ + 4 /* CRC */ )
#define INTELX_MAXFRS_MFS_MASK	INTELX_MAXFRS_MFS ( 0xffff )

/** Link Status Register */
#define INTELX_LINKS 0x042a4UL
#define INTELX_LINKS_UP		0x40000000UL	/**< Link up */

/** Firmware Status Register */
#define INTELX_FWSTS	0x015F0CUL
#define INTELX_FWSTS_FWRI	0x00000200UL	/**< Firmware reset indication */

/** Software Semaphore Register */
#define INTELX_SWSM(flags) ( flags & INTELX_X550EM_A ? \
	0x015F74UL : 0x010140UL )
#define INTELX_SWSM_SMBI	0x00000001UL	/**< Driver semaphore bit */

/** Software-Firmware Synchronization Register */
#define INTELX_SW_FW_SYNC(flags) ( flags & INTELX_X550EM_A ? \
	0x015F78UL : 0x010160UL )
#define INTELX_SW_FW_SYNC_SW_PHY0_SM 0x02UL	/**< Software PHY 0 access */
#define INTELX_SW_FW_SYNC_SW_PHY1_SM 0x04UL	/**< Software PHY 1 access */
#define INTELX_SW_FW_SYNC_SW_MAC_CSR_SM 0x08UL	/**< Software MAC CSR access */
#define INTELX_SW_FW_SYNC_FW_PHY0_SM ( 1 << 6 )	/**< Firmware PHY 0 access */
#define INTELX_SW_FW_SYNC_FW_PHY1_SM ( 1 << 7 ) /**< Firmware PHY 1 access */
#define INTELX_SW_FW_SYNC_FW_MAC_CSR_SM ( 1 << 8 )	/**< Firmware MAC CSR access */
#define INTELX_SW_FW_SYNC_SW_MNG_SM ( 1 << 10 )	/**< Software manageability
													 host interface access */
#define INTELX_SW_FW_SYNC_REGSMP (1 << 32)	/**< Register Semaphore */

#define INTELX_SEMAPHORE_DELAY 50	/**< How long to wait between attempts */
#define INTELX_SEMAPHORE_ATTEMPTS 2000	/**< Number of times to try 
										     acquiring a semaphore */

/** Host Interface Control Register */
#define INTELX_HICR	0x015F00UL
#define INTELX_HICR_EN 0x01UL	/**< Register enabled */
#define INTELX_HICR_C 0x02UL	/**< Set bit for command to be processed */
#define INTELX_HICR_SV 0x04UL	/**< Indicates a valid status in response */

/** Host ARC Data RAM */
#define INTELX_ARCRAM	0x015800UL

/** Host Interface Command Header */
struct intelx_hic_hdr {
	uint8_t cmd;
	uint8_t buf_len;
	union {
		uint8_t cmd_resv;
		uint8_t ret_status;
	} cmd_or_resp;
	uint8_t checksum;
};

/** Host Interface Command Type */
#define INTELX_HIC_HDR_CMD_REQ	5	/**< Request command */

/** Host Interface Command Request */
struct intelx_hic_req {
	struct intelx_hic_hdr hdr;
	uint8_t port_number;
	uint8_t pad;
	uint16_t activity_id;
	uint32_t data[4];
};

/** Host Interface Command Request Activity Ids */
#define INTELX_HIC_REQ_ACT_PHY_INIT	1
#define INTELX_HIC_REQ_ACT_PHY_SETUP_LINK 2
#define INTELX_HIC_REQ_ACT_PHY_SW_RESET	5
#define INTELX_HIC_REQ_ACT_PHY_GET_INFO 7

/** Host Interface Command Request Setup Link Data */
#define INTELX_HIC_REQ_SETUP_LINK_DATA0 0x07005b00

#endif /* _INTELX_H */
