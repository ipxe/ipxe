#ifndef _USR_IMGMGMT_H
#define _USR_IMGMGMT_H

/** @file
 *
 * Image management
 *
 */

struct image;

extern int imgfetch ( const char *filename, const char *name,
		      struct image **new_image );
extern int imgload ( struct image *image );
extern int imgexec ( struct image *image );
extern struct image * imgautoselect ( void );
extern void imgstat ( struct image *image );
extern void imgfree ( struct image *image );

#endif /* _USR_IMGMGMT_H */
