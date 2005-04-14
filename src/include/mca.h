/*
 * MCA bus driver code
 *
 * Abstracted from 3c509.c.
 *
 */

#ifndef MCA_H
#define MCA_H

#include "isa_ids.h"
#include "dev.h"

/*
 * MCA constants
 *
 */

#define MCA_MOTHERBOARD_SETUP_REG	0x94
#define MCA_ADAPTER_SETUP_REG		0x96
#define MCA_MAX_SLOT_NR			8
#define MCA_POS_REG(n)			(0x100+(n))

/* Is there a standard that would define this? */
#define GENERIC_MCA_VENDOR ISA_VENDOR ( 'M', 'C', 'A' )

/*
 * A physical MCA device
 *
 */
struct mca_device {
	char *magic; /* must be first */
	const char *name;
	unsigned int slot;
	unsigned char pos[8];
	int already_tried;
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
	const char *name;
	struct mca_id *ids;
	unsigned int id_count;
};

/*
 * Define an MCA driver
 *
 */
#define MCA_DRIVER( driver_name, mca_ids ) {			\
	.name = driver_name,					\
	.ids = mca_ids,						\
	.id_count = sizeof ( mca_ids ) / sizeof ( mca_ids[0] ),	\
}

/*
 * Functions in mca.c
 *
 */
extern int find_mca_device ( struct mca_device *mca,
			     struct mca_driver *driver );
extern int find_mca_boot_device ( struct dev *dev, struct mca_driver *driver );

#endif
