/*
** Support for NE2000 PCI clones added David Monro June 1997
** Generalised to other NICs by Ken Yap July 1997
**
** Most of this is taken from:
**
** /usr/src/linux/drivers/pci/pci.c
** /usr/src/linux/include/linux/pci.h
** /usr/src/linux/arch/i386/bios32.c
** /usr/src/linux/include/linux/bios32.h
** /usr/src/linux/drivers/net/ne.c
*/
#ifdef CONFIG_PCI
#include "etherboot.h"
#include "pci.h"

#ifdef	CONFIG_PCI_DIRECT
#define  PCIBIOS_SUCCESSFUL                0x00

#define DEBUG 0

/*
 * Functions for accessing PCI configuration space with type 1 accesses
 */

#define CONFIG_CMD(bus, device_fn, where)   (0x80000000 | (bus << 16) | (device_fn << 8) | (where & ~3))

int pcibios_read_config_byte(unsigned int bus, unsigned int device_fn,
			       unsigned int where, uint8_t *value)
{
    outl(CONFIG_CMD(bus,device_fn,where), 0xCF8);
    *value = inb(0xCFC + (where&3));
    return PCIBIOS_SUCCESSFUL;
}

int pcibios_read_config_word (unsigned int bus,
    unsigned int device_fn, unsigned int where, uint16_t *value)
{
    outl(CONFIG_CMD(bus,device_fn,where), 0xCF8);
    *value = inw(0xCFC + (where&2));
    return PCIBIOS_SUCCESSFUL;
}

int pcibios_read_config_dword (unsigned int bus, unsigned int device_fn,
				 unsigned int where, uint32_t *value)
{
    outl(CONFIG_CMD(bus,device_fn,where), 0xCF8);
    *value = inl(0xCFC);
    return PCIBIOS_SUCCESSFUL;
}

int pcibios_write_config_byte (unsigned int bus, unsigned int device_fn,
				 unsigned int where, uint8_t value)
{
    outl(CONFIG_CMD(bus,device_fn,where), 0xCF8);
    outb(value, 0xCFC + (where&3));
    return PCIBIOS_SUCCESSFUL;
}

int pcibios_write_config_word (unsigned int bus, unsigned int device_fn,
				 unsigned int where, uint16_t value)
{
    outl(CONFIG_CMD(bus,device_fn,where), 0xCF8);
    outw(value, 0xCFC + (where&2));
    return PCIBIOS_SUCCESSFUL;
}

int pcibios_write_config_dword (unsigned int bus, unsigned int device_fn, unsigned int where, uint32_t value)
{
    outl(CONFIG_CMD(bus,device_fn,where), 0xCF8);
    outl(value, 0xCFC);
    return PCIBIOS_SUCCESSFUL;
}

#undef CONFIG_CMD

#else	 /* CONFIG_PCI_DIRECT  not defined */

#if !defined(PCBIOS)
#error "The pcibios can only be used when the PCBIOS support is compiled in"
#endif

/* Macro for calling the BIOS32 service.  This replaces the old
 * bios32_call function.  Use in a statement such as
 * __asm__ ( BIOS32_CALL,
 *	     : <output registers>
 *	     : "S" ( bios32_entry ), <other input registers> );
 */
#define BIOS32_CALL "call _virt_to_phys\n\t" \
		    "pushl %%cs\n\t" \
		    "call *%%esi\n\t" \
		    "cli\n\t" \
		    "cld\n\t" \
		    "call _phys_to_virt\n\t"

static unsigned long bios32_entry;
static unsigned long pcibios_entry;

static unsigned long bios32_service(unsigned long service)
{
	unsigned char return_code;	/* %al */
	unsigned long address;		/* %ebx */
	unsigned long length;		/* %ecx */
	unsigned long entry;		/* %edx */

	__asm__(BIOS32_CALL
		: "=a" (return_code),
		  "=b" (address),
		  "=c" (length),
		  "=d" (entry)
		: "0" (service),
		  "1" (0),
		  "S" (bios32_entry));

	switch (return_code) {
		case 0:
			return address + entry;
		case 0x80:	/* Not present */
			printf("bios32_service(%d) : not present\n", service);
			return 0;
		default: /* Shouldn't happen */
			printf("bios32_service(%d) : returned %#X????\n",
				service, return_code);
			return 0;
	}
}

int pcibios_read_config_byte(unsigned int bus,
        unsigned int device_fn, unsigned int where, uint8_t *value)
{
        unsigned long ret;
        unsigned long bx = (bus << 8) | device_fn;

        __asm__(BIOS32_CALL
                "jc 1f\n\t"
                "xor %%ah, %%ah\n"
                "1:"
                : "=c" (*value),
                  "=a" (ret)
                : "1" (PCIBIOS_READ_CONFIG_BYTE),
                  "b" (bx),
                  "D" ((long) where),
                  "S" (pcibios_entry));
        return (int) (ret & 0xff00) >> 8;
}

int pcibios_read_config_word(unsigned int bus,
        unsigned int device_fn, unsigned int where, uint16_t *value)
{
        unsigned long ret;
        unsigned long bx = (bus << 8) | device_fn;

        __asm__(BIOS32_CALL
                "jc 1f\n\t"
                "xor %%ah, %%ah\n"
                "1:"
                : "=c" (*value),
                  "=a" (ret)
                : "1" (PCIBIOS_READ_CONFIG_WORD),
                  "b" (bx),
                  "D" ((long) where),
                  "S" (pcibios_entry));
        return (int) (ret & 0xff00) >> 8;
}

int pcibios_read_config_dword(unsigned int bus,
        unsigned int device_fn, unsigned int where, uint32_t *value)
{
        unsigned long ret;
        unsigned long bx = (bus << 8) | device_fn;

        __asm__(BIOS32_CALL
                "jc 1f\n\t"
                "xor %%ah, %%ah\n"
                "1:"
                : "=c" (*value),
                  "=a" (ret)
                : "1" (PCIBIOS_READ_CONFIG_DWORD),
                  "b" (bx),
                  "D" ((long) where),
                  "S" (pcibios_entry));
        return (int) (ret & 0xff00) >> 8;
}

int pcibios_write_config_byte (unsigned int bus,
	unsigned int device_fn, unsigned int where, uint8_t value)
{
	unsigned long ret;
	unsigned long bx = (bus << 8) | device_fn;

	__asm__(BIOS32_CALL
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=a" (ret)
		: "0" (PCIBIOS_WRITE_CONFIG_BYTE),
		  "c" (value),
		  "b" (bx),
		  "D" ((long) where),
		  "S" (pcibios_entry));
	return (int) (ret & 0xff00) >> 8;
}

int pcibios_write_config_word (unsigned int bus,
	unsigned int device_fn, unsigned int where, uint16_t value)
{
	unsigned long ret;
	unsigned long bx = (bus << 8) | device_fn;

	__asm__(BIOS32_CALL
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=a" (ret)
		: "0" (PCIBIOS_WRITE_CONFIG_WORD),
		  "c" (value),
		  "b" (bx),
		  "D" ((long) where),
		  "S" (pcibios_entry));
	return (int) (ret & 0xff00) >> 8;
}

int pcibios_write_config_dword (unsigned int bus,
	unsigned int device_fn, unsigned int where, uint32_t value)
{
	unsigned long ret;
	unsigned long bx = (bus << 8) | device_fn;

	__asm__(BIOS32_CALL
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=a" (ret)
		: "0" (PCIBIOS_WRITE_CONFIG_DWORD),
		  "c" (value),
		  "b" (bx),
		  "D" ((long) where),
		  "S" (pcibios_entry));
	return (int) (ret & 0xff00) >> 8;
}

static void check_pcibios(void)
{
	unsigned long signature;
	unsigned char present_status;
	unsigned char major_revision;
	unsigned char minor_revision;
	int pack;

	if ((pcibios_entry = bios32_service(PCI_SERVICE))) {
		__asm__(BIOS32_CALL
			"jc 1f\n\t"
			"xor %%ah, %%ah\n"
			"1:\tshl $8, %%eax\n\t"
			"movw %%bx, %%ax"
			: "=d" (signature),
			  "=a" (pack)
			: "1" (PCIBIOS_PCI_BIOS_PRESENT),
			  "S" (pcibios_entry)
			: "bx", "cx");

		present_status = (pack >> 16) & 0xff;
		major_revision = (pack >> 8) & 0xff;
		minor_revision = pack & 0xff;
		if (present_status || (signature != PCI_SIGNATURE)) {
			printf("ERROR: BIOS32 says PCI BIOS, but no PCI "
				"BIOS????\n");
			pcibios_entry = 0;
		}
#if	DEBUG
		if (pcibios_entry) {
			printf ("pcibios_init : PCI BIOS revision %hhX.%hhX"
				" entry at %#X\n", major_revision,
				minor_revision, pcibios_entry);
		}
#endif
	}
}

static void pcibios_init(void)
{
	union bios32 *check;
	unsigned char sum;
	int i, length;
	bios32_entry = 0;

	/*
	 * Follow the standard procedure for locating the BIOS32 Service
	 * directory by scanning the permissible address range from
	 * 0xe0000 through 0xfffff for a valid BIOS32 structure.
	 *
	 */

	for (check = phys_to_virt(0xe0000); (void *)check <= phys_to_virt(0xffff0); ++check) {
		if (check->fields.signature != BIOS32_SIGNATURE)
			continue;
		length = check->fields.length * 16;
		if (!length)
			continue;
		sum = 0;
		for (i = 0; i < length ; ++i)
			sum += check->chars[i];
		if (sum != 0)
			continue;
		if (check->fields.revision != 0) {
			printf("pcibios_init : unsupported revision %d at %#X, mail drew@colorado.edu\n",
				check->fields.revision, check);
			continue;
		}
#if	DEBUG
		printf("pcibios_init : BIOS32 Service Directory "
			"structure at %#X\n", check);
#endif
		if (!bios32_entry) {
			if (check->fields.entry >= 0x100000) {
				printf("pcibios_init: entry in high "
					"memory, giving up\n");
				return;
			} else {
				bios32_entry = check->fields.entry;
#if	DEBUG
				printf("pcibios_init : BIOS32 Service Directory"
					" entry at %#X\n", bios32_entry);
#endif
			}
		}
	}
	if (bios32_entry)
		check_pcibios();
}
#endif	/* CONFIG_PCI_DIRECT not defined*/

unsigned long pcibios_bus_base(unsigned int bus __unused)
{
	/* architecturally this must be 0 */
	return 0;
}

void find_pci(int type, struct pci_device *dev)
{
#ifndef	CONFIG_PCI_DIRECT
	if (!pcibios_entry) {
		pcibios_init();
	}
	if (!pcibios_entry) {
		printf("pci_init: no BIOS32 detected\n");
		return;
	}
#endif
	return scan_pci_bus(type, dev);
}
#endif /* CONFIG_PCI */
