#ifndef _GPXE_POSIX_IO_H
#define _GPXE_POSIX_IO_H

/** @file
 *
 * POSIX-like I/O
 *
 */

#include <stdint.h>
#include <gpxe/uaccess.h>

extern int open ( const char *uri_string );
extern ssize_t read_user ( int fd, userptr_t buffer,
			   off_t offset, size_t len );
extern ssize_t fsize ( int fd );
extern int close ( int fd );

/**
 * Read data from file
 *
 * @v fd		File descriptor
 * @v buf		Data buffer
 * @v len		Maximum length to read
 * @ret len		Actual length read, or negative error number
 */
static inline ssize_t read ( int fd, void *buf, size_t len ) {
	return read_user ( fd, virt_to_user ( buf ), 0, len );
}

#endif /* _GPXE_POSIX_IO_H */
