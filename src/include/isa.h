#ifndef	ISA_H
#define ISA_H

#include "stdint.h"
#include "isa_ids.h"
#include "nic.h"

/*
 * A location on an ISA bus
 *
 */
struct isa_driver;
struct isa_loc {
	unsigned int driver;
	unsigned int probe_idx;
};

/*
 * A physical ISA device
 *
 */
struct isa_device {
	const char *name;
	struct isa_driver *driver;
	uint16_t ioaddr;
	uint16_t mfg_id;
	uint16_t prod_id;
};

/*
 * An individual ISA device, identified by probe address
 *
 */
typedef uint16_t isa_probe_addr_t;

/*
 * An ISA driver, with a probe address list and a probe_addr method.
 * probe_addr() should return 1 if a card is physically present,
 * leaving the other operations (read MAC address etc.) down to the
 * main probe() routine.
 *
 */
struct isa_driver {
	const char *name;
	isa_probe_addr_t *probe_addrs;
	unsigned int addr_count;
	int ( * probe_addr ) ( isa_probe_addr_t addr );
	uint16_t mfg_id;
	uint16_t prod_id;
};

/*
 * Define an ISA driver
 *
 */
#define ISA_DRIVER( _name, _probe_addrs, _probe_addr, _mfg_id, _prod_id )   \
struct isa_driver _name __table ( struct isa_driver, isa_driver, 01 ) = {   \
	.probe_addrs = _probe_addrs,					    \
	.addr_count = sizeof ( _probe_addrs ) / sizeof ( _probe_addrs[0] ), \
	.probe_addr = _probe_addr,					    \
	.mfg_id = _mfg_id,						    \
	.prod_id = _prod_id,						    \
}

/*
 * ISA_ROM is parsed by parserom.pl to generate Makefile rules and
 * files for rom-o-matic.
 *
 */
#define ISA_ROM( IMAGE, DESCRIPTION )

/*
 * Functions in isa.c
 *
 */
extern void isa_fill_nic ( struct nic *nic, struct isa_device *isa );

/*
 * ISA bus global definition
 *
 */
extern struct bus_driver isa_driver;

#endif /* ISA_H */

