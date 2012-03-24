#ifndef _USR_IMGMGMT_H
#define _USR_IMGMGMT_H

/** @file
 *
 * Image management
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <ipxe/image.h>

extern int imgdownload ( struct uri *uri, struct image **image );
extern int imgdownload_string ( const char *uri_string, struct image **image );
extern int imgacquire ( const char *name, struct image **image );
extern void imgstat ( struct image *image );

#endif /* _USR_IMGMGMT_H */
