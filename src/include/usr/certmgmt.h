#ifndef _USR_CERTMGMT_H
#define _USR_CERTMGMT_H

/** @file
 *
 * Certificate management
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/x509.h>

extern void certstat ( struct x509_certificate *cert );

#endif /* _USR_CERTMGMT_H */
