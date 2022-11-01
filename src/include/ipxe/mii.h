#ifndef _IPXE_MII_H
#define _IPXE_MII_H

/** @file
 *
 * Media Independent Interface
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <mii.h>
#include <ipxe/netdevice.h>

struct mii_interface;

/** MII interface operations */
struct mii_operations {
	/**
	 * Read from MII register
	 *
	 * @v mdio		MII interface
	 * @v phy		PHY address
	 * @v reg		Register address
	 * @ret data		Data read, or negative error
	 */
	int ( * read ) ( struct mii_interface *mdio, unsigned int phy,
			 unsigned int reg );
	/**
	 * Write to MII register
	 *
	 * @v mdio		MII interface
	 * @v phy		PHY address
	 * @v reg		Register address
	 * @v data		Data to write
	 * @ret rc		Return status code
	 */
	int ( * write ) ( struct mii_interface *mdio, unsigned int phy,
			  unsigned int reg, unsigned int data );
};

/** An MII interface */
struct mii_interface {
	/** Interface operations */
	struct mii_operations *op;
};

/** An MII device */
struct mii_device {
	/** MII interface */
	struct mii_interface *mdio;
	/** PHY address */
	unsigned int address;
};

/**
 * Initialise MII interface
 *
 * @v mdio		MII interface
 * @v op		MII interface operations
 */
static inline __attribute__ (( always_inline )) void
mdio_init ( struct mii_interface *mdio, struct mii_operations *op ) {
	mdio->op = op;
}

/**
 * Initialise MII device
 *
 * @v mii		MII device
 * @v mdio		MII interface
 * @v address		PHY address
 */
static inline __attribute__ (( always_inline )) void
mii_init ( struct mii_device *mii, struct mii_interface *mdio,
	   unsigned int address ) {
	mii->mdio = mdio;
	mii->address = address;
}

/**
 * Read from MII register
 *
 * @v mii		MII device
 * @v reg		Register address
 * @ret data		Data read, or negative error
 */
static inline __attribute__ (( always_inline )) int
mii_read ( struct mii_device *mii, unsigned int reg ) {
	struct mii_interface *mdio = mii->mdio;

	return mdio->op->read ( mdio, mii->address, reg );
}

/**
 * Write to MII register
 *
 * @v mii		MII device
 * @v reg		Register address
 * @v data		Data to write
 * @ret rc		Return status code
 */
static inline __attribute__ (( always_inline )) int
mii_write ( struct mii_device *mii, unsigned int reg, unsigned int data ) {
	struct mii_interface *mdio = mii->mdio;

	return mdio->op->write ( mdio, mii->address, reg, data );
}

/**
 * Dump MII registers (for debugging)
 *
 * @v mii		MII device
 */
static inline void
mii_dump ( struct mii_device *mii ) {
	unsigned int i;
	int data;

	/* Do nothing unless debug output is enabled */
	if ( ! DBG_LOG )
		return;

	/* Dump basic MII register set */
	for ( i = 0 ; i < 16 ; i++ ) {
		if ( ( i % 8 ) == 0 ) {
			DBGC ( mii, "MII %p registers %02x-%02x:",
			       mii, i, ( i + 7 ) );
		}
		data = mii_read ( mii, i );
		if ( data >= 0 ) {
			DBGC ( mii, " %04x", data );
		} else {
			DBGC ( mii, " XXXX" );
		}
		if ( ( i % 8 ) == 7 )
			DBGC ( mii, "\n" );
	}
}

/** Maximum time to wait for a reset, in milliseconds */
#define MII_RESET_MAX_WAIT_MS 500

/** Maximum PHY address */
#define MII_MAX_PHY_ADDRESS 31

extern int mii_restart ( struct mii_device *mii );
extern int mii_reset ( struct mii_device *mii );
extern int mii_check_link ( struct mii_device *mii,
			    struct net_device *netdev );
extern int mii_find ( struct mii_device *mii );

#endif /* _IPXE_MII_H */
