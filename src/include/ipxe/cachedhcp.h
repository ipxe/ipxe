#ifndef _IPXE_CACHEDHCP_H
#define _IPXE_CACHEDHCP_H

/** @file
 *
 * Cached DHCP packet
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stddef.h>
#include <ipxe/uaccess.h>

extern int cachedhcp_record ( userptr_t data, size_t max_len );

#endif /* _IPXE_CACHEDHCP_H */
