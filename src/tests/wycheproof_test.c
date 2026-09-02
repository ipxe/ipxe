/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

FILE_LICENCE ( BSD2 );

/** @file
 *
 * Cryptographic test vectors imported from Project Wycheproof
 *
 * Project Wycheproof (https://github.com/C2SP/wycheproof) provides
 * test vectors designed to exercise cryptographic algorithms with
 * corner cases that are not covered by the standard known-answer
 * tests such as those provided by NIST or in RFCs.
 *
 * The full applicable test set is extremely large and slow to run.
 * We deliberately choose not to included these tests within the
 * standard per-commit test suite, since this would substantially
 * delay all test runs for no significant benefit.
 *
 * The test vectors are provided as JSON files, and are imported into
 * iPXE using the import tool within the "wycheproof" directory.
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <ipxe/test.h>
#include "wycheproof_test.h"

/* Drag in all Wycheproof self-tests */
PROVIDE_REQUIRING_SYMBOL();
REQUIRE_OBJECT ( wycheproof_aes_gcm );
REQUIRE_OBJECT ( wycheproof_hkdf_sha1 );
REQUIRE_OBJECT ( wycheproof_hkdf_sha256 );
REQUIRE_OBJECT ( wycheproof_hkdf_sha384 );
REQUIRE_OBJECT ( wycheproof_hkdf_sha512 );
REQUIRE_OBJECT ( wycheproof_hmac_sha1 );
REQUIRE_OBJECT ( wycheproof_hmac_sha224 );
REQUIRE_OBJECT ( wycheproof_hmac_sha256 );
REQUIRE_OBJECT ( wycheproof_hmac_sha384 );
REQUIRE_OBJECT ( wycheproof_hmac_sha512_224 );
REQUIRE_OBJECT ( wycheproof_hmac_sha512_256 );
REQUIRE_OBJECT ( wycheproof_hmac_sha512 );
REQUIRE_OBJECT ( wycheproof_p256 );
REQUIRE_OBJECT ( wycheproof_p384 );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_1024_sign );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_1536_sign );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_2048_decrypt );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_2048_sha224_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_2048_sha256_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_2048_sha384_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_2048_sha512_224_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_2048_sha512_256_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_2048_sha512_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_2048_sign );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_3072_decrypt );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_3072_sha256_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_3072_sha384_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_3072_sha512_256_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_3072_sha512_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_3072_sign );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_4096_decrypt );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_4096_sha256_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_4096_sha384_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_4096_sha512_256_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_4096_sha512_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_4096_sign );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_8192_sha256_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_8192_sha384_verify );
REQUIRE_OBJECT ( wycheproof_rsa_pkcs1_8192_sha512_verify );
REQUIRE_OBJECT ( wycheproof_x25519 );
