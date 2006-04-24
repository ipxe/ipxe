#include "dev.h"
#include <gpxe/pci.h>
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
void i386_select_pci_device ( struct i386_all_regs *ix86 ) {
	/*
	 * PCI BIOS passes busdevfn in %ax
	 *
	 */
	union {
		struct bus_loc bus_loc;
		struct pci_loc pci_loc;
	} u;
	
	/* Select PCI bus and specified busdevfn as first boot device */
	memset ( &u, 0, sizeof ( u ) );
	u.pci_loc.busdevfn = ix86->regs.ax;
	select_device ( &dev, &pci_driver, &u.bus_loc );
}
