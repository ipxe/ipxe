#ifndef _WYCHEPROOF_TEST_H
#define _WYCHEPROOF_TEST_H

/** @file
 *
 * Cryptographic test vectors imported from Project Wycheproof
 *
 */

FILE_LICENCE ( BSD2 );

#include <ipxe/crypto.h>
#include <ipxe/p256.h>
#include <ipxe/p384.h>
#include <ipxe/x25519.h>
#include "exchange_test.h"

extern void wycheproof_p256_exec ( void );
extern void wycheproof_p384_exec ( void );
extern void wycheproof_x25519_exec ( void );

#endif /* _WYCHEPROOF_TEST_H */
