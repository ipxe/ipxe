#ifndef _IPXE_INITRD_H
#define _IPXE_INITRD_H

/** @file
 *
 * Initial ramdisk (initrd) reshuffling
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

extern void initrd_reshuffle ( physaddr_t bottom );
extern int initrd_reshuffle_check ( size_t len, physaddr_t bottom );

#endif /* _IPXE_INITRD_H */
