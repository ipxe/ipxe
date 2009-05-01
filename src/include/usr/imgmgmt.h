#ifndef _USR_IMGMGMT_H
#define _USR_IMGMGMT_H

/** @file
 *
 * Image management
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

struct image;

extern int imgfetch ( struct image *image, const char *uri_string,
		      int ( * image_register ) ( struct image *image ) );
extern int imgload ( struct image *image );
extern int imgexec ( struct image *image );
extern struct image * imgautoselect ( void );
extern void imgstat ( struct image *image );
extern void imgfree ( struct image *image );

#endif /* _USR_IMGMGMT_H */
