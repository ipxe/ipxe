#ifndef	ISA_H
#define ISA_H

#include "isa_ids.h"
#include "dev.h"

/*
 * A physical ISA device
 *
 */
struct isa_device {
	char *magic; /* must be first */
	unsigned int probe_idx;
	uint16_t ioaddr;
	int already_tried;
};

/*
 * An individual ISA device, identified by probe address
 *
 */
struct isa_probe_addr {
	uint16_t addr;
} __attribute__ (( packed ));

/*
 * An ISA driver, with a probe address list and a probe_addr method.
 * probe_addr() should return 1 if a card is physically present,
 * leaving the other operations (read MAC address etc.) down to the
 * main probe() routine.
 *
 */
struct isa_driver {
	const char *name;
	struct isa_probe_addr *probe_addrs;
	unsigned int addr_count;
	int ( * probe_addr ) ( uint16_t addr );
	uint16_t mfg_id;
	uint16_t prod_id;
};

/*
 * Define an ISA driver
 *
 */
#define ISA_DRIVER( _name, _probe_addrs, _probe_addr, _mfg_id, _prod_id ) { \
	.name = _name,							    \
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
extern int find_isa_device ( struct isa_device *eisa,
			     struct isa_driver *driver );
extern int find_isa_boot_device ( struct dev *dev,
				  struct isa_driver *driver );

/*
 * config.c defines isa_extra_probe_addrs and isa_extra_probe_addr_count.
 *
 */
extern struct isa_probe_addr isa_extra_probe_addrs[];
extern unsigned int isa_extra_probe_addr_count;

#endif /* ISA_H */

