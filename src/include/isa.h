#ifndef	ISA_H
#define ISA_H

#include "isa_ids.h"

struct dev;

struct isa_driver
{
	int type;
	const char *name;
	int (*probe)(struct dev *, unsigned short *);
	unsigned short *ioaddrs;
};

#ifndef __HYPERSTONE__
#define __isa_driver	__attribute__ ((used,__section__(".drivers.isa")))
#else 
#define __isa_driver	__attribute__ ((used,__section__(".drivisa")))
#endif

extern const struct isa_driver isa_drivers[];
extern const struct isa_driver isa_drivers_end[];

#define ISA_ROM(IMAGE, DESCRIPTION)

#endif /* ISA_H */

