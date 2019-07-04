#ifndef _SMSCUSB_H
#define _SMSCUSB_H

/** @file
 *
 * SMSC USB Ethernet drivers
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <ipxe/usb.h>
#include <ipxe/usbnet.h>
#include <ipxe/netdevice.h>
#include <ipxe/mii.h>
#include <ipxe/if_ether.h>

/** Register write command */
#define SMSCUSB_REGISTER_WRITE					\
	( USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE |	\
	  USB_REQUEST_TYPE ( 0xa0 ) )

/** Register read command */
#define SMSCUSB_REGISTER_READ					\
	( USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE |	\
	  USB_REQUEST_TYPE ( 0xa1 ) )

/** Get statistics command */
#define SMSCUSB_GET_STATISTICS					\
	( USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE |	\
	  USB_REQUEST_TYPE ( 0xa2 ) )

/** EEPROM command register offset */
#define SMSCUSB_E2P_CMD 0x000
#define SMSCUSB_E2P_CMD_EPC_BSY		0x80000000UL	/**< EPC busy */
#define SMSCUSB_E2P_CMD_EPC_CMD_READ	0x00000000UL	/**< READ command */
#define SMSCUSB_E2P_CMD_EPC_ADDR(addr) ( (addr) << 0 )	/**< EPC address */

/** EEPROM data register offset */
#define SMSCUSB_E2P_DATA 0x004
#define SMSCUSB_E2P_DATA_GET(e2p_data) \
	( ( (e2p_data) >> 0 ) & 0xff )			/**< EEPROM data */

/** MAC address EEPROM address */
#define SMSCUSB_EEPROM_MAC 0x01

/** Maximum time to wait for EEPROM (in milliseconds) */
#define SMSCUSB_EEPROM_MAX_WAIT_MS 100

/** OTP power register offset */
#define SMSCUSB_OTP_POWER 0x000
#define SMSCUSB_OTP_POWER_DOWN		0x00000001UL	/**< OTP power down */

/** OTP address high byte register offset */
#define SMSCUSB_OTP_ADDRH 0x004

/** OTP address low byte register offset */
#define SMSCUSB_OTP_ADDRL 0x008

/** OTP data register offset */
#define SMSCUSB_OTP_DATA 0x018
#define SMSCUSB_OTP_DATA_GET(otp_data) \
	( ( (otp_data) >> 0 ) & 0xff )			/**< OTP data */

/** OTP command selection register offset */
#define SMSCUSB_OTP_CMD 0x020
#define SMSCUSB_OTP_CMD_READ		0x00000001UL	/**< Read command */

/** OTP command initiation register offset */
#define SMSCUSB_OTP_GO 0x028
#define SMSCUSB_OTP_GO_GO		0x00000001UL	/**< Initiate command */

/** OTP status register offset */
#define SMSCUSB_OTP_STATUS 0x030
#define SMSCUSB_OTP_STATUS_BUSY		0x00000001UL	/**< OTP busy */

/** Maximum time to wait for OTP (in milliseconds) */
#define SMSCUSB_OTP_MAX_WAIT_MS 100

/** OTP layout 1 signature */
#define SMSCUSB_OTP_1_SIG 0xf3

/** OTP layout 1 MAC address offset */
#define SMSCUSB_OTP_1_MAC 0x001

/** OTP layout 2 signature */
#define SMSCUSB_OTP_2_SIG 0xf7

/** OTP layout 2 MAC address offset */
#define SMSCUSB_OTP_2_MAC 0x101

/** MII access register offset */
#define SMSCUSB_MII_ACCESS 0x000
#define SMSCUSB_MII_ACCESS_PHY_ADDRESS	0x00000800UL	/**< PHY address */
#define SMSCUSB_MII_ACCESS_MIIRINDA(addr) ( (addr) << 6 ) /**< MII register */
#define SMSCUSB_MII_ACCESS_MIIWNR	0x00000002UL	/**< MII write */
#define SMSCUSB_MII_ACCESS_MIIBZY	0x00000001UL	/**< MII busy */

/** MII data register offset */
#define SMSCUSB_MII_DATA 0x004
#define SMSCUSB_MII_DATA_SET(data)	( (data) << 0 )	/**< Set data */
#define SMSCUSB_MII_DATA_GET(mii_data) \
	( ( (mii_data) >> 0 ) & 0xffff )		/**< Get data */

/** Maximum time to wait for MII (in milliseconds) */
#define SMSCUSB_MII_MAX_WAIT_MS 100

/** MAC address */
union smscusb_mac {
	/** MAC receive address registers */
	struct {
		/** MAC receive address low register */
		uint32_t l;
		/** MAC receive address high register */
		uint32_t h;
	} __attribute__ (( packed )) addr;
	/** Raw MAC address */
	uint8_t raw[ETH_ALEN];
};

/** MAC receive address high register offset */
#define SMSCUSB_RX_ADDRH 0x000

/** MAC receive address low register offset */
#define SMSCUSB_RX_ADDRL 0x004

/** MAC address perfect filter N high register offset */
#define SMSCUSB_ADDR_FILTH(n) ( 0x000 + ( 8 * (n) ) )
#define SMSCUSB_ADDR_FILTH_VALID	0x80000000UL	/**< Address valid */

/** MAC address perfect filter N low register offset */
#define SMSCUSB_ADDR_FILTL(n) ( 0x004 + ( 8 * (n) ) )

/** Interrupt packet format */
struct smscusb_interrupt {
	/** Current value of INT_STS register */
	uint32_t int_sts;
} __attribute__ (( packed ));

/** An SMSC USB device */
struct smscusb_device {
	/** USB device */
	struct usb_device *usb;
	/** USB bus */
	struct usb_bus *bus;
	/** Network device */
	struct net_device *netdev;
	/** USB network device */
	struct usbnet_device usbnet;
	/** MII interface */
	struct mii_interface mdio;
	/** MII device */
	struct mii_device mii;
	/** MII register base */
	uint16_t mii_base;
	/** PHY interrupt source register */
	uint16_t phy_source;
	/** Interrupt status */
	uint32_t int_sts;
};

extern int smscusb_raw_writel ( struct smscusb_device *smscusb,
				unsigned int address, uint32_t value );
extern int smscusb_raw_readl ( struct smscusb_device *smscusb,
			       unsigned int address, uint32_t *value );

/**
 * Write register
 *
 * @v smscusb		SMSC USB device
 * @v address		Register address
 * @v value		Register value
 * @ret rc		Return status code
 */
static inline __attribute__ (( always_inline )) int
smscusb_writel ( struct smscusb_device *smscusb, unsigned int address,
		 uint32_t value ) {
	int rc;

	/* Write register */
	if ( ( rc = smscusb_raw_writel ( smscusb, address,
					 cpu_to_le32 ( value ) ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Read register
 *
 * @v smscusb		SMSC USB device
 * @v address		Register address
 * @ret value		Register value
 * @ret rc		Return status code
 */
static inline __attribute__ (( always_inline )) int
smscusb_readl ( struct smscusb_device *smscusb, unsigned int address,
		uint32_t *value ) {
	int rc;

	/* Read register */
	if ( ( rc = smscusb_raw_readl ( smscusb, address, value ) ) != 0 )
		return rc;
	le32_to_cpus ( value );

	return 0;
}

/**
 * Get statistics
 *
 * @v smscusb		SMSC USB device
 * @v index		Statistics set index
 * @v data		Statistics data to fill in
 * @v len		Length of statistics data
 * @ret rc		Return status code
 */
static inline __attribute__ (( always_inline )) int
smscusb_get_statistics ( struct smscusb_device *smscusb, unsigned int index,
			 void *data, size_t len ) {
	int rc;

	/* Read statistics */
	if ( ( rc = usb_control ( smscusb->usb, SMSCUSB_GET_STATISTICS, 0,
				  index, data, len ) ) != 0 ) {
		DBGC ( smscusb, "SMSCUSB %p could not get statistics set %d: "
		       "%s\n", smscusb, index, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/** Interrupt maximum fill level
 *
 * This is a policy decision.
 */
#define SMSCUSB_INTR_MAX_FILL 2

extern struct usb_endpoint_driver_operations smscusb_intr_operations;
extern struct usb_endpoint_driver_operations smscusb_out_operations;
extern struct mii_operations smscusb_mii_operations;

/**
 * Initialise SMSC USB device
 *
 * @v smscusb		SMSC USB device
 * @v netdev		Network device
 * @v func		USB function
 * @v in		Bulk IN endpoint operations
 */
static inline __attribute__ (( always_inline )) void
smscusb_init ( struct smscusb_device *smscusb, struct net_device *netdev,
	       struct usb_function *func,
	       struct usb_endpoint_driver_operations *in ) {
	struct usb_device *usb = func->usb;

	smscusb->usb = usb;
	smscusb->bus = usb->port->hub->bus;
	smscusb->netdev = netdev;
	usbnet_init ( &smscusb->usbnet, func, &smscusb_intr_operations, in,
		      &smscusb_out_operations );
	usb_refill_init ( &smscusb->usbnet.intr, 0, 0, SMSCUSB_INTR_MAX_FILL );
}

/**
 * Initialise SMSC USB device MII interface
 *
 * @v smscusb		SMSC USB device
 * @v mii_base		MII register base
 * @v phy_source	Interrupt source PHY register
 */
static inline __attribute__ (( always_inline )) void
smscusb_mii_init ( struct smscusb_device *smscusb, unsigned int mii_base,
		   unsigned int phy_source ) {

	mdio_init ( &smscusb->mdio, &smscusb_mii_operations );
	mii_init ( &smscusb->mii, &smscusb->mdio, 0 );
	smscusb->mii_base = mii_base;
	smscusb->phy_source = phy_source;
}

extern int smscusb_eeprom_fetch_mac ( struct smscusb_device *smscusb,
				      unsigned int e2p_base );
extern int smscusb_otp_fetch_mac ( struct smscusb_device *smscusb,
				   unsigned int otp_base );
extern int smscusb_mii_check_link ( struct smscusb_device *smscusb );
extern int smscusb_mii_open ( struct smscusb_device *smscusb,
			      unsigned int phy_mask, unsigned int intrs );
extern int smscusb_set_address ( struct smscusb_device *smscusb,
				 unsigned int addr_base );
extern int smscusb_set_filter ( struct smscusb_device *smscusb,
				unsigned int filt_base );

#endif /* _SMSCUSB_H */
