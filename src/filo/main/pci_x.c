#ifdef CONFIG_PCI
                
/*                      
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */             
                
#include <etherboot.h>  
#include <pci.h>
#include <lib.h>

#define DEBUG_THIS DEBUG_PCI

#include <debug.h>

struct pci_device *dev_list;
int n_devs;

static void pci_scan_bus(void)
{

        unsigned int first_bus, first_devfn;
        unsigned int devfn, bus, buses;
        
	uint32_t class; 
        uint16_t vendor, dev_id;
	uint8_t hdr_type;


        first_bus    = 0;
        first_devfn  = 0;

        buses=256;
        for (bus = first_bus; bus < buses; ++bus) {
          for (devfn = first_devfn; devfn < 0xff; ++devfn) {

#if 1
            if (PCI_FUNC(devfn) == 0)
                      pcibios_read_config_byte(bus, devfn, PCI_HEADER_TYPE, &hdr_type);
            else if (!(hdr_type & 0x80))    /* not a multi-function device */
                      continue;
#endif

	    pcibios_read_config_word(bus,devfn, PCI_VENDOR_ID, &vendor);
	    if (vendor==0xffff || vendor==0)
		continue;
	    
	    if (dev_list) {
		dev_list[n_devs].bus = bus;
		dev_list[n_devs].devfn = devfn;
		dev_list[n_devs].vendor = vendor;

		pcibios_read_config_word(bus,devfn, PCI_DEVICE_ID, &dev_id);
		dev_list[n_devs].dev_id = dev_id;

                pcibios_read_config_dword(bus,devfn, PCI_CLASS_REVISION, &class);
                dev_list[n_devs].class = class;

	    }
	    n_devs++;
          }
          first_devfn = 0;
        }
        first_bus = 0;

}
#define DEBUG 0

void pci_init(void)
{
    /* Count devices */
    dev_list = 0;
    n_devs = 0;
    debug("Scanning PCI: ");
    pci_scan_bus();
    debug("found %d devices\n", n_devs);

    /* Make the list */
    dev_list = allot(n_devs * sizeof(struct pci_device));
    n_devs = 0;
    pci_scan_bus();
#if DEBUG
    {
	int i;
	for (i = 0; i < n_devs; i++) {
	    printf("%02x:%02x.%x %04x:%04x %04x %02x\n", 
		    dev_list[i].bus,
		    PCI_SLOT(dev_list[i].devfn),
		    PCI_FUNC(dev_list[i].devfn),
		    dev_list[i].vendor,
		    dev_list[i].dev_id,((dev_list[i].class)>>16),
		    ((dev_list[i].class)>>8 & 0xff));
	}
    }
#endif
}

struct pci_device *pci_find_device_2(int vendor, int device, int devclass, int devclass2, int prog_if, int index)
{
    int i;

    for (i=0; i<n_devs; i++) {
	if (vendor < 0 || vendor==dev_list[i].vendor)
	    if (device < 0 || device==dev_list[i].dev_id)
		if (devclass < 0 || devclass==((dev_list[i].class)>>16) || devclass2==((dev_list[i].class)>>16))
		    if (prog_if < 0 || prog_if==((dev_list[i].class)>>8 & 0xff)) {
			if (index == 0)
			    return &dev_list[i];
			index--;
		    }
    }
	
    return NULL;
}


struct pci_device *pci_find_device(int vendor, int device, int devclass, int prog_if, int index)
{
	return pci_find_device_2(vendor, device, devclass, devclass, prog_if, index);
}

#endif
