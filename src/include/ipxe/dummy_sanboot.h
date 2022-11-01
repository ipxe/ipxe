#ifndef _IPXE_DUMMY_SANBOOT_H
#define _IPXE_DUMMY_SANBOOT_H

/** @file
 *
 * Dummy SAN device
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef SANBOOT_DUMMY
#define SANBOOT_PREFIX_dummy
#else
#define SANBOOT_PREFIX_dummy __dummy_
#endif

#endif /* _IPXE_DUMMY_SANBOOT_H */
