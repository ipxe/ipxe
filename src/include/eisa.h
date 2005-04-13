#ifndef EISA_H
#define EISA_H

#include "isa_ids.h"

/*
 * EISA constants
 *
 */

#define EISA_MIN_SLOT (0x1)
#define EISA_MAX_SLOT (0xf)
#define EISA_SLOT_BASE( n ) ( 0x1000 * (n) )

#define EISA_MFG_ID_HI ( 0xc80 )
#define EISA_MFG_ID_LO ( 0xc81 )
#define EISA_PROD_ID_HI ( 0xc82 )
#define EISA_PROD_ID_LO ( 0xc83 )
#define EISA_GLOBAL_CONFIG ( 0xc84 )

#define EISA_CMD_RESET ( 1 << 2 )
#define EISA_CMD_ENABLE ( 1 << 0 )

/*
 * A physical EISA device
 *
 */
struct dev;
struct eisa_device {
	struct dev *dev;
	unsigned int slot;
	uint16_t ioaddr;
	uint16_t mfg_id;
	uint16_t prod_id;
	int already_tried;
};

/*
 * An individual EISA device identified by ID
 *
 */
struct eisa_id {
        const char *name;
	uint16_t mfg_id, prod_id;
};

/*
 * An EISA driver, with a device ID (struct eisa_id) table.
 *
 */
struct eisa_driver {
	const char *name;
	struct eisa_id *ids;
	unsigned int id_count;
};

/*
 * Define an EISA driver
 *
 */
#define EISA_DRIVER( driver_name, eisa_ids ) {				\
	.name = driver_name,						\
	.ids = eisa_ids,						\
	.id_count = sizeof ( eisa_ids ) / sizeof ( eisa_ids[0] ),	\
}

/*
 * Functions in eisa.c
 *
 */
extern struct eisa_device * eisa_device ( struct dev *dev );
extern int find_eisa_device ( struct eisa_device *eisa,
			      struct eisa_driver *driver );
extern void enable_eisa_device ( struct eisa_device *eisa );

#endif /* EISA_H */
