#include "pci.h"
#include "registers.h"

/*
 * Register a device as the default PCI boot device.  This code is
 * called by the PCI ROM prefix.
 *
 * Do not move this code to drivers/bus/pci.c, because it is
 * i386-specific, and don't merge it with select_isapnp.c, because
 * that would cause linker symbol pollution.
 *
 */
void i386_select_pci_device ( struct i386_all_regs *regs ) {
	/*
	 * PCI BIOS passes busdevfn in %ax
	 *
	 */
	select_pci_device ( regs->ax );
}
