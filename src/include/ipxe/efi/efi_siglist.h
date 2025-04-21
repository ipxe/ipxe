#ifndef _IPXE_EFI_SIGLIST_H
#define _IPXE_EFI_SIGLIST_H

/** @file
 *
 * PEM-encoded ASN.1 data
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/asn1.h>
#include <ipxe/image.h>

extern int efisig_asn1 ( const void *data, size_t len, size_t offset,
			 struct asn1_cursor **cursor );

extern struct image_type efisig_image_type __image_type ( PROBE_NORMAL );

#endif /* _IPXE_EFI_SIGLIST_H */
