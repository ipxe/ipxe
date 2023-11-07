#ifndef _USR_SHIMMGMT_H
#define _USR_SHIMMGMT_H

/** @file
 *
 * EFI shim management
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/image.h>

extern int shim ( struct image *image, int require_loader, int allow_pxe,
		  int allow_sbat );

#endif /* _USR_SHIMMGMT_H */
