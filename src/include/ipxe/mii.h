#ifndef _IPXE_MII_H
#define _IPXE_MII_H

/** @file
 *
 * Media Independent Interface
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <mii.h>
#include <ipxe/netdevice.h>

struct mii_interface;

/** MII interface operations */
struct mii_operations {
	/**
	 * Read from MII register
	 *
	 * @v mii		MII interface
	 * @v reg		Register address
	 * @ret data		Data read, or negative error
	 */
	int ( * read ) ( struct mii_interface *mii, unsigned int reg );
	/**
	 * Write to MII register
	 *
	 * @v mii		MII interface
	 * @v reg		Register address
	 * @v data		Data to write
	 * @ret rc		Return status code
	 */
	int ( * write ) ( struct mii_interface *mii, unsigned int reg,
			  unsigned int data );
};

/** An MII interface */
struct mii_interface {
	/** Interface operations */
	struct mii_operations *op;
};

/**
 * Initialise MII interface
 *
 * @v mii		MII interface
 * @v op		MII interface operations
 */
static inline __attribute__ (( always_inline )) void
mii_init ( struct mii_interface *mii, struct mii_operations *op ) {
	mii->op = op;
}

/**
 * Read from MII register
 *
 * @v mii		MII interface
 * @v reg		Register address
 * @ret data		Data read, or negative error
 */
static inline __attribute__ (( always_inline )) int
mii_read ( struct mii_interface *mii, unsigned int reg ) {
	return mii->op->read ( mii, reg );
}

/**
 * Write to MII register
 *
 * @v mii		MII interface
 * @v reg		Register address
 * @v data		Data to write
 * @ret rc		Return status code
 */
static inline __attribute__ (( always_inline )) int
mii_write ( struct mii_interface *mii, unsigned int reg, unsigned int data ) {
	return mii->op->write ( mii, reg, data );
}

/** Maximum time to wait for a reset, in milliseconds */
#define MII_RESET_MAX_WAIT_MS 500

extern int mii_reset ( struct mii_interface *mii );

#endif /* _IPXE_MII_H */
