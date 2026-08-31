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
#include <ipxe/sha1.h>
#include <ipxe/sha256.h>
#include <ipxe/sha512.h>
#include <ipxe/x25519.h>
#include "exchange_test.h"
#include "hmac_test.h"

extern void wycheproof_hmac_sha1_exec ( void );
extern void wycheproof_hmac_sha224_exec ( void );
extern void wycheproof_hmac_sha256_exec ( void );
extern void wycheproof_hmac_sha384_exec ( void );
extern void wycheproof_hmac_sha512_exec ( void );
extern void wycheproof_hmac_sha512_224_exec ( void );
extern void wycheproof_hmac_sha512_256_exec ( void );
extern void wycheproof_p256_exec ( void );
extern void wycheproof_p384_exec ( void );
extern void wycheproof_x25519_exec ( void );

#endif /* _WYCHEPROOF_TEST_H */
