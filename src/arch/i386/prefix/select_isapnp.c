#include "dev.h"
#include "isapnp.h"
#include "registers.h"

/*
 * Register a device as the default ISAPnP boot device.  This code is
 * called by the ISAPnP ROM prefix.
 *
 * Do not move this code to drivers/bus/isapnp.c, because it is
 * i386-specific, and don't merge it with select_pci.c, because that
 * would cause linker symbol pollution.
 *
 */
void i386_select_isapnp_device ( struct i386_all_regs *regs ) {
	/*
	 * PnP BIOS passes card select number in %bx and read port
	 * address in %dx.
	 *
	 */
	union {
		struct bus_loc bus_loc;
		struct isapnp_loc isapnp_loc;
	} u;

	/* Set ISAPnP read port */
	isapnp_set_read_port ( regs->dx );
	
	/* Select ISAPnP bus and specified CSN as first boot device */
	memset ( &u, 0, sizeof ( u ) );
	u.isapnp_loc.csn = regs->bx;
	select_device ( &dev, &isapnp_driver, &u.bus_loc );
}
