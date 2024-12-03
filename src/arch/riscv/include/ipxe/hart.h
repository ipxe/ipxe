#ifndef _IPXE_HART_H
#define _IPXE_HART_H

/** @file
 *
 * Hardware threads (harts)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

extern unsigned long boot_hart;

extern int hart_supported ( const char *ext );

#endif /* _IPXE_HART_H */
