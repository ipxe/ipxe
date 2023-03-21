#ifndef _IPXE_MII_BIT_H
#define _IPXE_MII_BIT_H

/** @file
 *
 * MII bit-bashing interface
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/mii.h>
#include <ipxe/bitbash.h>

#define MII_BIT_START		0xffffffff	/**< Start */
#define MII_BIT_START_MASK	0x80000000	/**< Start mask */

#define MII_BIT_CMD_MASK	0x00000008	/**< Command mask */
#define MII_BIT_CMD_READ	0x00000006	/**< Command read */
#define MII_BIT_CMD_WRITE	0x00000005	/**< Command write */
#define MII_BIT_CMD_RW		0x00000001	/**< Command read or write */

#define MII_BIT_PHY_MASK	0x00000010	/**< PHY mask */

#define MII_BIT_REG_MASK	0x00000010	/**< Register mask */

#define MII_BIT_SWITCH		0x00000002	/**< Switch */
#define MII_BIT_SWITCH_MASK	0x00000002	/**< Switch mask */

#define MII_BIT_DATA_MASK	0x00008000	/**< Data mask */

/** A bit-bashing MII interface */
struct mii_bit_basher {
	/** MII interface */
	struct mii_interface mdio;
	/** Bit-bashing interface */
	struct bit_basher basher;
};

/** Bit indices used for MII bit-bashing interface */
enum {
	/** MII clock */
	MII_BIT_MDC = 0,
	/** MII data */
	MII_BIT_MDIO,
	/** MII data direction */
	MII_BIT_DRIVE,
};

/** Delay between MDC transitions */
#define MII_BIT_UDELAY 1

extern void init_mii_bit_basher ( struct mii_bit_basher *miibit );

#endif /* _IPXE_MII_BIT_H */
