#include "etherboot.h"
#include "pci.h"
#include "sal.h"

int pcibios_read_config_byte(unsigned int bus, unsigned int devfn, unsigned int reg, uint8_t *rvalue)
{
	unsigned long value;
	long result;
	result = sal_pci_config_read(PCI_SAL_ADDRESS(0,bus, 0, devfn, reg), 1, &value);
	*rvalue = value;
	return result;
}
int pcibios_read_config_word(unsigned int bus, unsigned int devfn, unsigned int reg, uint16_t *rvalue)
{
	unsigned long value;
	long result;
	result = sal_pci_config_read(PCI_SAL_ADDRESS(0,bus, 0, devfn, reg), 2, &value);
	*rvalue = value;
	return result;
}
int pcibios_read_config_dword(unsigned int bus, unsigned int devfn, unsigned int reg, uint32_t *rvalue)
{
	unsigned long value;
	long result;
	result = sal_pci_config_read(PCI_SAL_ADDRESS(0,bus, 0, devfn, reg), 4, &value);
	*rvalue = value;
	return result;
}

int pcibios_write_config_byte(unsigned int bus, unsigned int devfn, unsigned int reg, uint8_t value)
{
	return  sal_pci_config_write(PCI_SAL_ADDRESS(0,bus, 0, devfn, reg), 1, value);
}

int pcibios_write_config_word(unsigned int bus, unsigned int devfn, unsigned int reg, uint16_t value)
{
	return  sal_pci_config_write(PCI_SAL_ADDRESS(0,bus, 0, devfn, reg), 2, value);
}

int pcibios_write_config_dword(unsigned int bus, unsigned int devfn, unsigned int reg, uint32_t value)
{
	return  sal_pci_config_write(PCI_SAL_ADDRESS(0,bus, 0, devfn, reg), 4, value);
}

/* So far I have not see a non-zero PCI_BUS_OFFSET
 * and an AML parser to get it much to much trouble.
 */
#ifndef PCI_BUS_OFFSET
#define PCI_BUS_OFFSET 0
#endif

unsigned long pcibios_bus_base(unsigned int bus)
{
	return PCI_BUS_OFFSET;
}

void find_pci(int type, struct pci_device *dev)
{
	/* Should I check for sal functions being present? */
	return scan_pci_bus(type, dev);
}

