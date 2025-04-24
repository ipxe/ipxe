#ifndef _IPXE_LINUX_SYSFS_H
#define _IPXE_LINUX_SYSFS_H

/** @file
 *
 * Linux sysfs files
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

extern int linux_sysfs_read ( const char *filename, void **data );

#endif /* _IPXE_LINUX_SYSFS_H */
