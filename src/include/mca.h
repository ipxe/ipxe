/*
 * MCA bus driver code
 *
 * Abstracted from 3c509.c.
 *
 */

#ifndef MCA_H
#define MCA_H

#include "isa_ids.h"
#include "nic.h"

#define MCA_BUS_TYPE	3

/*
 * MCA constants
 *
 */

#define MCA_MOTHERBOARD_SETUP_REG	0x94
#define MCA_ADAPTER_SETUP_REG		0x96
#define MCA_MAX_SLOT_NR			0x07	/* Must be 2^n - 1 */
#define MCA_POS_REG(n)			(0x100+(n))

/* Is there a standard that would define this? */
#define GENERIC_MCA_VENDOR ISA_VENDOR ( 'M', 'C', 'A' )

/*
 * A location on an MCA bus
 *
 */
struct mca_loc {
	unsigned int slot;
};

/*
 * A physical MCA device
 *
 */
struct mca_device {
	const char *name;
	unsigned int slot;
	unsigned char pos[8];
};
#define MCA_ID(mca) ( ( (mca)->pos[1] << 8 ) + (mca)->pos[0] )

/*
 * An individual MCA device identified by ID
 *
 */
struct mca_id {
        const char *name;
        int id;
};

/*
 * An MCA driver, with a device ID (struct mca_id) table.
 *
 */
struct mca_driver {
	struct mca_id *ids;
	unsigned int id_count;
};

/*
 * Define an MCA driver
 *
 */
#define MCA_DRIVER( _name, _ids )					\
	static struct mca_driver _name = {				\
		.ids = _ids,						\
		.id_count = sizeof ( _ids ) / sizeof ( _ids[0] ),	\
	}

/*
 * Functions in mca.c
 *
 */
extern void mca_fill_nic ( struct nic *nic, struct mca_device *mca );

/*
 * MCA bus global definition
 *
 */
extern struct bus_driver mca_driver;

#endif
