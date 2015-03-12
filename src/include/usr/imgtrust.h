#ifndef _USR_IMGTRUST_H
#define _USR_IMGTRUST_H

/** @file
 *
 * Image trust management
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/image.h>

extern int imgverify ( struct image *image, struct image *signature,
		       const char *name );

extern int imgverifydigest ( struct image *image, const char *digest_name,
			     const char *digest );

#endif /* _USR_IMGTRUST_H */
