#ifndef _IPXE_DOWNLOADER_H
#define _IPXE_DOWNLOADER_H

/** @file
 *
 * Image downloader
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

struct job_interface;
struct image;

extern int create_downloader ( struct job_interface *job, struct image *image,
			       int ( * register_image ) ( struct image *image ),
			       int type, ... );

#endif /* _IPXE_DOWNLOADER_H */
