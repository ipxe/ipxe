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
	select_isapnp_device ( regs->dx, regs->bx );
}
