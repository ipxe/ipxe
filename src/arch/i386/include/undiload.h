#ifndef _UNDILOAD_H
#define _UNDILOAD_H

/** @file
 *
 * UNDI load/unload
 *
 */

struct undi_device;
struct undi_rom;

extern int undi_load_pci ( struct undi_device *undi, struct undi_rom *undirom,
			   unsigned int bus, unsigned int devfn );
extern int undi_unload ( struct undi_device *undi );

#endif /* _UNDILOAD_H */
