#ifndef _IPXE_CERTSTORE_H
#define _IPXE_CERTSTORE_H

/** @file
 *
 * Certificate store
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/x509.h>

extern struct x509_chain certstore;

extern void certstore_add ( struct x509_certificate *cert );
extern void certstore_del ( struct x509_certificate *cert );

#endif /* _IPXE_CERTSTORE_H */
