#ifndef _UNDILOAD_H
#define _UNDILOAD_H

/** @file
 *
 * UNDI load/unload
 *
 */

struct undi_device;
struct undi_rom;

extern int undi_load ( struct undi_device *undi, struct undi_rom *undirom );
extern int undi_unload ( struct undi_device *undi );

/**
 * Call UNDI loader to create a pixie
 *
 * @v undi		UNDI device
 * @v undirom		UNDI ROM
 * @v pci_busdevfn	PCI bus:dev.fn
 * @ret rc		Return status code
 */
static inline int undi_load_pci ( struct undi_device *undi,
				  struct undi_rom *undirom,
				  unsigned int pci_busdevfn ) {
	undi->pci_busdevfn = pci_busdevfn;
	undi->isapnp_csn = 0xffff;
	undi->isapnp_read_port = 0xffff;
	return undi_load ( undi, undirom );
}

#endif /* _UNDILOAD_H */
