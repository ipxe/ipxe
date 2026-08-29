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

/** Self-tests */
struct self_test wycheproof_test[] __self_test = {
	{
		.name = "wycheproof:p256",
		.exec = wycheproof_p256_exec,
	},
	{
		.name = "wycheproof:p384",
		.exec = wycheproof_p384_exec,
	},
	{
		.name = "wycheproof:x25519",
		.exec = wycheproof_x25519_exec,
	},
};
