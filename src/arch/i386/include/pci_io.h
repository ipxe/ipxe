#ifndef _PCI_IO_H
#define _PCI_IO_H

#include <pcibios.h>
#include <pcidirect.h>

/** @file
 *
 * i386 PCI configuration space access
 *
 * We have two methods of PCI configuration space access: the PCI BIOS
 * and direct Type 1 accesses.  Selecting between them is via the
 * compile-time switch -DCONFIG_PCI_DIRECT.
 *
 */

#if CONFIG_PCI_DIRECT
#define pci_max_bus		pcidirect_max_bus
#define pci_read_config_byte	pcidirect_read_config_byte
#define pci_read_config_word	pcidirect_read_config_word
#define pci_read_config_dword	pcidirect_read_config_dword
#define pci_write_config_byte	pcidirect_write_config_byte
#define pci_write_config_word	pcidirect_write_config_word
#define pci_write_config_dword	pcidirect_write_config_dword
#else /* CONFIG_PCI_DIRECT */
#define pci_max_bus		pcibios_max_bus
#define pci_read_config_byte	pcibios_read_config_byte
#define pci_read_config_word	pcibios_read_config_word
#define pci_read_config_dword	pcibios_read_config_dword
#define pci_write_config_byte	pcibios_write_config_byte
#define pci_write_config_word	pcibios_write_config_word
#define pci_write_config_dword	pcibios_write_config_dword
#endif /* CONFIG_PCI_DIRECT */

#endif /* _PCI_IO_H */
