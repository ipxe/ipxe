#ifndef EISA_H
#define EISA_H

#include "stdint.h"
#include "isa_ids.h"
#include "nic.h"

/*
 * EISA constants
 *
 */

#define EISA_MIN_SLOT (0x1)
#define EISA_MAX_SLOT (0xf)	/* Must be 2^n - 1 */
#define EISA_SLOT_BASE( n ) ( 0x1000 * (n) )

#define EISA_MFG_ID_HI ( 0xc80 )
#define EISA_MFG_ID_LO ( 0xc81 )
#define EISA_PROD_ID_HI ( 0xc82 )
#define EISA_PROD_ID_LO ( 0xc83 )
#define EISA_GLOBAL_CONFIG ( 0xc84 )

#define EISA_CMD_RESET ( 1 << 2 )
#define EISA_CMD_ENABLE ( 1 << 0 )

/*
 * A location on an EISA bus
 *
 */
struct eisa_loc {
	unsigned int slot;
};

/*
 * A physical EISA device
 *
 */
struct eisa_device {
	const char *name;
	unsigned int slot;
	uint16_t ioaddr;
	uint16_t mfg_id;
	uint16_t prod_id;
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
#define EISA_DRIVER( _ids ) {					\
	.ids = _ids,						\
	.id_count = sizeof ( _ids ) / sizeof ( _ids[0] ),	\
}

/*
 * Functions in eisa.c
 *
 */
extern void eisa_device_enabled ( struct eisa_device *eisa, int enabled );
extern void fill_eisa_nic ( struct nic *nic, struct eisa_device *eisa );

static inline void enable_eisa_device ( struct eisa_device *eisa ) {
	eisa_device_enabled ( eisa, 1 );
}
static inline void disable_eisa_device ( struct eisa_device *eisa ) {
	eisa_device_enabled ( eisa, 0 );
}

/*
 * EISA bus global definition
 *
 */
extern struct bus_driver eisa_driver;

#endif /* EISA_H */
