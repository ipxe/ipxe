#ifdef CONFIG_ISA
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include	"etherboot.h"
#include	"nic.h"
#include	"isa.h"

void isa_enumerate(void)
{
	const struct isa_driver *driver;
	for(driver = isa_drivers; driver < isa_drivers_end; driver++) {
		printf("%s ", driver->name);
	}

}

int isa_probe(struct dev *dev, const char *type_name)
{
/*
 *	NIC probing is in the order the drivers were linked togeter.
 *	If for some reason you want to change the order,
 *	just change the order you list the drivers in.
 */
	struct isa_probe_state *state = &dev->state.isa;
	printf("Probing isa %s...\n", type_name);
	if (dev->how_probe == PROBE_FIRST) {
		state->advance = 0;
		state->driver  = isa_drivers;
		dev->index     = -1;
	}
	for(;;)
	{
		if ((dev->how_probe != PROBE_AWAKE) && state->advance) {
			state->driver++;
			dev->index = -1;
		}
		state->advance = 1;
		
		if (state->driver >= isa_drivers_end)
			break;

		if (state->driver->type != dev->type)
			continue;

		if (dev->how_probe != PROBE_AWAKE) {
			dev->type_index++;
		}
		printf("[%s]", state->driver->name);
		dev->devid.bus_type = ISA_BUS_TYPE;
		/* FIXME how do I handle dev->index + PROBE_AGAIN?? */
		/* driver will fill in vendor and device IDs */
		if (state->driver->probe(dev, state->driver->ioaddrs)) {
			state->advance = (dev->index == -1);
			return PROBE_WORKED;
		}
		putchar('\n');
	}
	return PROBE_FAILED;
}

#endif
