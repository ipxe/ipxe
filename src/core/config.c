/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include	"etherboot.h"
#include	"nic.h"
#ifdef BUILD_SERIAL
#include	".buildserial.h"
#define xstr(s) str(s)
#define str(s) #s
#endif

void print_config(void)
{
	printf( "Etherboot " VERSION
#ifdef BUILD_SERIAL
		" [build " 
#ifdef BUILD_ID
		BUILD_ID " "
#endif
		"#" xstr(BUILD_SERIAL_NUM) "]"
#endif /* BUILD_SERIAL */
		" (GPL) http://etherboot.org\n"
		"Drivers: " );
#ifdef CONFIG_PCI
	pci_enumerate();
#endif	
#ifdef CONFIG_ISA
	isa_enumerate();
#endif	
	printf( "  Images: " 
#ifdef	TAGGED_IMAGE
		"NBI "
#endif
#ifdef	ELF64_IMAGE
		"ELF64 "
#endif
#ifdef	ELF_IMAGE
		"ELF "
#endif
#ifdef	COFF_IMAGE
		"COFF "
#endif
#ifdef	IMAGE_FREEBSD
		"FreeBSD "
#endif
#ifdef	IMAGE_MULTIBOOT
		"Multiboot "
#endif
#ifdef	AOUT_IMAGE
		"a.out "
#endif
#ifdef	WINCE_IMAGE
		"WINCE "
#endif
#ifdef	PXE_IMAGE
		"PXE "
#endif
#ifdef PXE_EXPORT /* All possible exports */
		"  Exports: "
#ifdef PXE_EXPORT
		"PXE "
#endif
#endif /* All possible exports */
		"  "
		);
#if	(BOOTP_SERVER != 67) || (BOOTP_CLIENT != 68)
	printf( "[DHCP ports %d and %d] ",
		BOOTP_SERVER, BOOTP_CLIENT);
#endif
	putchar('\n');
	printf( "Protocols: "
#ifdef RARP_NOT_BOOTP
		"RARP "
#else
# ifndef NO_DHCP_SUPPORT
		"DHCP "
# else
		"BOOTP "
# endif
#endif
#ifdef DOWNLOAD_PROTO_TFTP
		"TFTP "
#endif
#ifdef  DOWNLOAD_PROTO_NFS
		"NFS "
#endif
#ifdef  DOWNLOAD_PROTO_SLAM
		"SLAM "
#endif
#ifdef  DOWNLOAD_PROTO_TFTM
		"TFTM "
#endif
#ifdef  DOWNLOAD_PROTO_HTTP
		"HTTP "
#endif
#ifdef  PROTO_LACP
		"LACP "
#endif
#ifdef DNS_RESOLVER
		"DNS "
#endif
		"\n");
}

static const char *driver_name[] = {
	"nic", 
	"disk", 
	"floppy",
};

int probe(struct dev *dev)
{
	const char *type_name;
	type_name = "";
	if ((dev->type >= 0) && 
		((unsigned)dev->type < sizeof(driver_name)/sizeof(driver_name[0]))) {
		type_name = driver_name[dev->type];
	}
	if (dev->how_probe == PROBE_FIRST) {
		dev->to_probe = PROBE_PCI;
		memset(&dev->state, 0, sizeof(dev->state));
	}
	if (dev->to_probe == PROBE_PCI) {
#ifdef	CONFIG_PCI
		dev->how_probe = pci_probe(dev, type_name);
#else
		dev->how_probe = PROBE_FAILED;
#endif
		if (dev->how_probe == PROBE_FAILED) {
			dev->to_probe = PROBE_ISA;
		}
	}
	if (dev->to_probe == PROBE_ISA) {
#ifdef	CONFIG_ISA
		dev->how_probe = isa_probe(dev, type_name);
#else
		dev->how_probe = PROBE_FAILED;
#endif
		if (dev->how_probe == PROBE_FAILED) {
			dev->to_probe = PROBE_NONE;
		}
	}
	if ((dev->to_probe != PROBE_PCI) &&
		(dev->to_probe != PROBE_ISA)) {
		dev->how_probe = PROBE_FAILED;
		
	}
	return dev->how_probe;
}

void disable(struct dev *dev)
{
	if (dev->disable) {
		dev->disable(dev);
		dev->disable = 0;
	}
}
