#ifndef PCI_IO_H
#define PCI_IO_H

/* %ah */
#define PCIBIOS_PCI_FUNCTION_ID         ( 0xb1 )
/* %al */
#define PCIBIOS_PCI_BIOS_PRESENT        ( 0x01 )
#define PCIBIOS_FIND_PCI_DEVICE         ( 0x02 )
#define PCIBIOS_FIND_PCI_CLASS_CODE     ( 0x03 )
#define PCIBIOS_GENERATE_SPECIAL_CYCLE  ( 0x06 )
#define PCIBIOS_READ_CONFIG_BYTE        ( 0x08 )
#define PCIBIOS_READ_CONFIG_WORD        ( 0x09 )
#define PCIBIOS_READ_CONFIG_DWORD       ( 0x0a )
#define PCIBIOS_WRITE_CONFIG_BYTE       ( 0x0b )
#define PCIBIOS_WRITE_CONFIG_WORD       ( 0x0c )
#define PCIBIOS_WRITE_CONFIG_DWORD      ( 0x0d )
#define PCIBIOS_GET_IRQ_ROUTING_OPTIONS	( 0x0e )
#define PCIBIOS_SET_PCI_IRQ		( 0x0f )

#endif /* PCI_IO_H */
