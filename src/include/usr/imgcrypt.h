#ifndef _USR_IMGCRYPT_H
#define _USR_IMGCRYPT_H

/** @file
 *
 * Image encryption management
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/image.h>

extern int imgdecrypt ( struct image *image, struct image *envelope,
			const char *name );

#endif /* _USR_IMGCRYPT_H */
