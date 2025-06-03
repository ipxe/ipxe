/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * DES tests
 *
 * These test vectors are originally provided by NBS (the precursor of
 * NIST) in SP 500-20, downloadable as a scan of the typewritten
 * original from:
 *
 * https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nbsspecialpublication500-20e1980.pdf
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <assert.h>
#include <ipxe/des.h>
#include <ipxe/test.h>
#include "cipher_test.h"

/** Define a DES 64-bit test value */
#define DES_VALUE(value) {						\
	( ( ( ( uint64_t ) (value) ) >> 56 ) & 0xff ),			\
	( ( ( ( uint64_t ) (value) ) >> 48 ) & 0xff ),			\
	( ( ( ( uint64_t ) (value) ) >> 40 ) & 0xff ),			\
	( ( ( ( uint64_t ) (value) ) >> 32 ) & 0xff ),			\
	( ( ( ( uint64_t ) (value) ) >> 24 ) & 0xff ),			\
	( ( ( ( uint64_t ) (value) ) >> 16 ) & 0xff ),			\
	( ( ( ( uint64_t ) (value) ) >>  8 ) & 0xff ),			\
	( ( ( ( uint64_t ) (value) ) >>  0 ) & 0xff )			\
	}

/** Define a DES test */
#define DES_TEST( name, key, plaintext, ciphertext )			\
	CIPHER_TEST ( name, &des_ecb_algorithm,	DES_VALUE ( key ),	\
		      IV(), ADDITIONAL(), DES_VALUE ( plaintext ),	\
		      DES_VALUE ( ciphertext ), AUTH() )

/* Sample round outputs (page 9) */
DES_TEST ( des_round_sample,
	   0x10316e028c8f3b4a, 0x0000000000000000, 0x82dcbafbdeab6602 );

/* Test 1: Initial permutation and expansion tests
 *
 * "Set Key=0 and encrypt the 64-bit data vectors e[i]: i=1,...,64"
 *
 * Appendix B, page 28 ("IP and E test")
 */
DES_TEST ( des_test1_1,
	   0x0101010101010101, 0x8000000000000000, 0x95f8a5e5dd31d900 );
DES_TEST ( des_test1_2,
	   0x0101010101010101, 0x4000000000000000, 0xdd7f121ca5015619 );
DES_TEST ( des_test1_3,
	   0x0101010101010101, 0x2000000000000000, 0x2e8653104f3834ea );
DES_TEST ( des_test1_4,
	   0x0101010101010101, 0x1000000000000000, 0x4bd388ff6cd81d4f );
DES_TEST ( des_test1_5,
	   0x0101010101010101, 0x0800000000000000, 0x20b9e767b2fb1456 );
DES_TEST ( des_test1_6,
	   0x0101010101010101, 0x0400000000000000, 0x55579380d77138ef );
DES_TEST ( des_test1_7,
	   0x0101010101010101, 0x0200000000000000, 0x6cc5defaaf04512f );
DES_TEST ( des_test1_8,
	   0x0101010101010101, 0x0100000000000000, 0x0d9f279ba5d87260 );
DES_TEST ( des_test1_9,
	   0x0101010101010101, 0x0080000000000000, 0xd9031b0271bd5a0a );
DES_TEST ( des_test1_10,
	   0x0101010101010101, 0x0040000000000000, 0x424250b37c3dd951 );
DES_TEST ( des_test1_11,
	   0x0101010101010101, 0x0020000000000000, 0xb8061b7ecd9a21e5 );
DES_TEST ( des_test1_12,
	   0x0101010101010101, 0x0010000000000000, 0xf15d0f286b65bd28 );
DES_TEST ( des_test1_13,
	   0x0101010101010101, 0x0008000000000000, 0xadd0cc8d6e5deba1 );
DES_TEST ( des_test1_14,
	   0x0101010101010101, 0x0004000000000000, 0xe6d5f82752ad63d1 );
DES_TEST ( des_test1_15,
	   0x0101010101010101, 0x0002000000000000, 0xecbfe3bd3f591a5e );
DES_TEST ( des_test1_16,
	   0x0101010101010101, 0x0001000000000000, 0xf356834379d165cd );
DES_TEST ( des_test1_17,
	   0x0101010101010101, 0x0000800000000000, 0x2b9f982f20037fa9 );
DES_TEST ( des_test1_18,
	   0x0101010101010101, 0x0000400000000000, 0x889de068a16f0be6 );
DES_TEST ( des_test1_19,
	   0x0101010101010101, 0x0000200000000000, 0xe19e275d846a1298 );
DES_TEST ( des_test1_20,
	   0x0101010101010101, 0x0000100000000000, 0x329a8ed523d71aec );
DES_TEST ( des_test1_21,
	   0x0101010101010101, 0x0000080000000000, 0xe7fce22557d23c97 );
DES_TEST ( des_test1_22,
	   0x0101010101010101, 0x0000040000000000, 0x12a9f5817ff2d65d );
DES_TEST ( des_test1_23,
	   0x0101010101010101, 0x0000020000000000, 0xa484c3ad38dc9c19 );
DES_TEST ( des_test1_24,
	   0x0101010101010101, 0x0000010000000000, 0xfbe00a8a1ef8ad72 );
DES_TEST ( des_test1_25,
	   0x0101010101010101, 0x0000008000000000, 0x750d079407521363 );
DES_TEST ( des_test1_26,
	   0x0101010101010101, 0x0000004000000000, 0x64feed9c724c2faf );
DES_TEST ( des_test1_27,
	   0x0101010101010101, 0x0000002000000000, 0xf02b263b328e2b60 );
DES_TEST ( des_test1_28,
	   0x0101010101010101, 0x0000001000000000, 0x9d64555a9a10b852 );
DES_TEST ( des_test1_29,
	   0x0101010101010101, 0x0000000800000000, 0xd106ff0bed5255d7 );
DES_TEST ( des_test1_30,
	   0x0101010101010101, 0x0000000400000000, 0xe1652c6b138c64a5 );
DES_TEST ( des_test1_31,
	   0x0101010101010101, 0x0000000200000000, 0xe428581186ec8f46 );
DES_TEST ( des_test1_32,
	   0x0101010101010101, 0x0000000100000000, 0xaeb5f5ede22d1a36 );
DES_TEST ( des_test1_33,
	   0x0101010101010101, 0x0000000080000000, 0xe943d7568aec0c5c );
DES_TEST ( des_test1_34,
	   0x0101010101010101, 0x0000000040000000, 0xdf98c8276f54b04b );
DES_TEST ( des_test1_35,
	   0x0101010101010101, 0x0000000020000000, 0xb160e4680f6c696f );
DES_TEST ( des_test1_36,
	   0x0101010101010101, 0x0000000010000000, 0xfa0752b07d9c4ab8 );
DES_TEST ( des_test1_37,
	   0x0101010101010101, 0x0000000008000000, 0xca3a2b036dbc8502 );
DES_TEST ( des_test1_38,
	   0x0101010101010101, 0x0000000004000000, 0x5e0905517bb59bcf );
DES_TEST ( des_test1_39,
	   0x0101010101010101, 0x0000000002000000, 0x814eeb3b91d90726 );
DES_TEST ( des_test1_40,
	   0x0101010101010101, 0x0000000001000000, 0x4d49db1532919c9f );
DES_TEST ( des_test1_41,
	   0x0101010101010101, 0x0000000000800000, 0x25eb5fc3f8cf0621 );
DES_TEST ( des_test1_42,
	   0x0101010101010101, 0x0000000000400000, 0xab6a20c0620d1c6f );
DES_TEST ( des_test1_43,
	   0x0101010101010101, 0x0000000000200000, 0x79e90dbc98f92cca );
DES_TEST ( des_test1_44,
	   0x0101010101010101, 0x0000000000100000, 0x866ecedd8072bb0e );
DES_TEST ( des_test1_45,
	   0x0101010101010101, 0x0000000000080000, 0x8b54536f2f3e64a8 );
DES_TEST ( des_test1_46,
	   0x0101010101010101, 0x0000000000040000, 0xea51d3975595b86b );
DES_TEST ( des_test1_47,
	   0x0101010101010101, 0x0000000000020000, 0xcaffc6ac4542de31 );
DES_TEST ( des_test1_48,
	   0x0101010101010101, 0x0000000000010000, 0x8dd45a2ddf90796c );
DES_TEST ( des_test1_49,
	   0x0101010101010101, 0x0000000000008000, 0x1029d55e880ec2d0 );
DES_TEST ( des_test1_50,
	   0x0101010101010101, 0x0000000000004000, 0x5d86cb23639dbea9 );
DES_TEST ( des_test1_51,
	   0x0101010101010101, 0x0000000000002000, 0x1d1ca853ae7c0c5f );
DES_TEST ( des_test1_52,
	   0x0101010101010101, 0x0000000000001000, 0xce332329248f3228 );
DES_TEST ( des_test1_53,
	   0x0101010101010101, 0x0000000000000800, 0x8405d1abe24fb942 );
DES_TEST ( des_test1_54,
	   0x0101010101010101, 0x0000000000000400, 0xe643d78090ca4207 );
DES_TEST ( des_test1_55,
	   0x0101010101010101, 0x0000000000000200, 0x48221b9937748a23 );
DES_TEST ( des_test1_56,
	   0x0101010101010101, 0x0000000000000100, 0xdd7c0bbd61fafd54 );
DES_TEST ( des_test1_57,
	   0x0101010101010101, 0x0000000000000080, 0x2fbc291a570db5c4 );
DES_TEST ( des_test1_58,
	   0x0101010101010101, 0x0000000000000040, 0xe07c30d7e4e26e12 );
DES_TEST ( des_test1_59,
	   0x0101010101010101, 0x0000000000000020, 0x0953e2258e8e90a1 );
DES_TEST ( des_test1_60,
	   0x0101010101010101, 0x0000000000000010, 0x5b711bc4ceebf2ee );
DES_TEST ( des_test1_61,
	   0x0101010101010101, 0x0000000000000008, 0xcc083f1e6d9e85f6 );
DES_TEST ( des_test1_62,
	   0x0101010101010101, 0x0000000000000004, 0xd2fd8867d50d2dfe );
DES_TEST ( des_test1_63,
	   0x0101010101010101, 0x0000000000000002, 0x06e7ea22ce92708f );
DES_TEST ( des_test1_64,
	   0x0101010101010101, 0x0000000000000001, 0x166b40b44aba4bd6 );

/* Test 2: Inverse permutation and expansion tests
 *
 * "Set Key=0 and encrypt the results c[i] obtained in Test 1"
 *
 * Appendix B, page 28 ("IP and E test")
 */
DES_TEST ( des_test2_1,
	   0x0101010101010101, 0x95f8a5e5dd31d900, 0x8000000000000000 );
DES_TEST ( des_test2_2,
	   0x0101010101010101, 0xdd7f121ca5015619, 0x4000000000000000 );
DES_TEST ( des_test2_3,
	   0x0101010101010101, 0x2e8653104f3834ea, 0x2000000000000000 );
DES_TEST ( des_test2_4,
	   0x0101010101010101, 0x4bd388ff6cd81d4f, 0x1000000000000000 );
DES_TEST ( des_test2_5,
	   0x0101010101010101, 0x20b9e767b2fb1456, 0x0800000000000000 );
DES_TEST ( des_test2_6,
	   0x0101010101010101, 0x55579380d77138ef, 0x0400000000000000 );
DES_TEST ( des_test2_7,
	   0x0101010101010101, 0x6cc5defaaf04512f, 0x0200000000000000 );
DES_TEST ( des_test2_8,
	   0x0101010101010101, 0x0d9f279ba5d87260, 0x0100000000000000 );
DES_TEST ( des_test2_9,
	   0x0101010101010101, 0xd9031b0271bd5a0a, 0x0080000000000000 );
DES_TEST ( des_test2_10,
	   0x0101010101010101, 0x424250b37c3dd951, 0x0040000000000000 );
DES_TEST ( des_test2_11,
	   0x0101010101010101, 0xb8061b7ecd9a21e5, 0x0020000000000000 );
DES_TEST ( des_test2_12,
	   0x0101010101010101, 0xf15d0f286b65bd28, 0x0010000000000000 );
DES_TEST ( des_test2_13,
	   0x0101010101010101, 0xadd0cc8d6e5deba1, 0x0008000000000000 );
DES_TEST ( des_test2_14,
	   0x0101010101010101, 0xe6d5f82752ad63d1, 0x0004000000000000 );
DES_TEST ( des_test2_15,
	   0x0101010101010101, 0xecbfe3bd3f591a5e, 0x0002000000000000 );
DES_TEST ( des_test2_16,
	   0x0101010101010101, 0xf356834379d165cd, 0x0001000000000000 );
DES_TEST ( des_test2_17,
	   0x0101010101010101, 0x2b9f982f20037fa9, 0x0000800000000000 );
DES_TEST ( des_test2_18,
	   0x0101010101010101, 0x889de068a16f0be6, 0x0000400000000000 );
DES_TEST ( des_test2_19,
	   0x0101010101010101, 0xe19e275d846a1298, 0x0000200000000000 );
DES_TEST ( des_test2_20,
	   0x0101010101010101, 0x329a8ed523d71aec, 0x0000100000000000 );
DES_TEST ( des_test2_21,
	   0x0101010101010101, 0xe7fce22557d23c97, 0x0000080000000000 );
DES_TEST ( des_test2_22,
	   0x0101010101010101, 0x12a9f5817ff2d65d, 0x0000040000000000 );
DES_TEST ( des_test2_23,
	   0x0101010101010101, 0xa484c3ad38dc9c19, 0x0000020000000000 );
DES_TEST ( des_test2_24,
	   0x0101010101010101, 0xfbe00a8a1ef8ad72, 0x0000010000000000 );
DES_TEST ( des_test2_25,
	   0x0101010101010101, 0x750d079407521363, 0x0000008000000000 );
DES_TEST ( des_test2_26,
	   0x0101010101010101, 0x64feed9c724c2faf, 0x0000004000000000 );
DES_TEST ( des_test2_27,
	   0x0101010101010101, 0xf02b263b328e2b60, 0x0000002000000000 );
DES_TEST ( des_test2_28,
	   0x0101010101010101, 0x9d64555a9a10b852, 0x0000001000000000 );
DES_TEST ( des_test2_29,
	   0x0101010101010101, 0xd106ff0bed5255d7, 0x0000000800000000 );
DES_TEST ( des_test2_30,
	   0x0101010101010101, 0xe1652c6b138c64a5, 0x0000000400000000 );
DES_TEST ( des_test2_31,
	   0x0101010101010101, 0xe428581186ec8f46, 0x0000000200000000 );
DES_TEST ( des_test2_32,
	   0x0101010101010101, 0xaeb5f5ede22d1a36, 0x0000000100000000 );
DES_TEST ( des_test2_33,
	   0x0101010101010101, 0xe943d7568aec0c5c, 0x0000000080000000 );
DES_TEST ( des_test2_34,
	   0x0101010101010101, 0xdf98c8276f54b04b, 0x0000000040000000 );
DES_TEST ( des_test2_35,
	   0x0101010101010101, 0xb160e4680f6c696f, 0x0000000020000000 );
DES_TEST ( des_test2_36,
	   0x0101010101010101, 0xfa0752b07d9c4ab8, 0x0000000010000000 );
DES_TEST ( des_test2_37,
	   0x0101010101010101, 0xca3a2b036dbc8502, 0x0000000008000000 );
DES_TEST ( des_test2_38,
	   0x0101010101010101, 0x5e0905517bb59bcf, 0x0000000004000000 );
DES_TEST ( des_test2_39,
	   0x0101010101010101, 0x814eeb3b91d90726, 0x0000000002000000 );
DES_TEST ( des_test2_40,
	   0x0101010101010101, 0x4d49db1532919c9f, 0x0000000001000000 );
DES_TEST ( des_test2_41,
	   0x0101010101010101, 0x25eb5fc3f8cf0621, 0x0000000000800000 );
DES_TEST ( des_test2_42,
	   0x0101010101010101, 0xab6a20c0620d1c6f, 0x0000000000400000 );
DES_TEST ( des_test2_43,
	   0x0101010101010101, 0x79e90dbc98f92cca, 0x0000000000200000 );
DES_TEST ( des_test2_44,
	   0x0101010101010101, 0x866ecedd8072bb0e, 0x0000000000100000 );
DES_TEST ( des_test2_45,
	   0x0101010101010101, 0x8b54536f2f3e64a8, 0x0000000000080000 );
DES_TEST ( des_test2_46,
	   0x0101010101010101, 0xea51d3975595b86b, 0x0000000000040000 );
DES_TEST ( des_test2_47,
	   0x0101010101010101, 0xcaffc6ac4542de31, 0x0000000000020000 );
DES_TEST ( des_test2_48,
	   0x0101010101010101, 0x8dd45a2ddf90796c, 0x0000000000010000 );
DES_TEST ( des_test2_49,
	   0x0101010101010101, 0x1029d55e880ec2d0, 0x0000000000008000 );
DES_TEST ( des_test2_50,
	   0x0101010101010101, 0x5d86cb23639dbea9, 0x0000000000004000 );
DES_TEST ( des_test2_51,
	   0x0101010101010101, 0x1d1ca853ae7c0c5f, 0x0000000000002000 );
DES_TEST ( des_test2_52,
	   0x0101010101010101, 0xce332329248f3228, 0x0000000000001000 );
DES_TEST ( des_test2_53,
	   0x0101010101010101, 0x8405d1abe24fb942, 0x0000000000000800 );
DES_TEST ( des_test2_54,
	   0x0101010101010101, 0xe643d78090ca4207, 0x0000000000000400 );
DES_TEST ( des_test2_55,
	   0x0101010101010101, 0x48221b9937748a23, 0x0000000000000200 );
DES_TEST ( des_test2_56,
	   0x0101010101010101, 0xdd7c0bbd61fafd54, 0x0000000000000100 );
DES_TEST ( des_test2_57,
	   0x0101010101010101, 0x2fbc291a570db5c4, 0x0000000000000080 );
DES_TEST ( des_test2_58,
	   0x0101010101010101, 0xe07c30d7e4e26e12, 0x0000000000000040 );
DES_TEST ( des_test2_59,
	   0x0101010101010101, 0x0953e2258e8e90a1, 0x0000000000000020 );
DES_TEST ( des_test2_60,
	   0x0101010101010101, 0x5b711bc4ceebf2ee, 0x0000000000000010 );
DES_TEST ( des_test2_61,
	   0x0101010101010101, 0xcc083f1e6d9e85f6, 0x0000000000000008 );
DES_TEST ( des_test2_62,
	   0x0101010101010101, 0xd2fd8867d50d2dfe, 0x0000000000000004 );
DES_TEST ( des_test2_63,
	   0x0101010101010101, 0x06e7ea22ce92708f, 0x0000000000000002 );
DES_TEST ( des_test2_64,
	   0x0101010101010101, 0x166b40b44aba4bd6, 0x0000000000000001 );

/* Test 3: Data permutation tests
 *
 * "Set the plaintext to zero and process the 32 keys in PTEST"
 *
 * Appendix B, page 32 ("PTEST")
 */
DES_TEST ( des_test3_1,
	   0x1046913489980131, 0x0000000000000000, 0x88d55e54f54c97b4 );
DES_TEST ( des_test3_2,
	   0x1007103489988020, 0x0000000000000000, 0x0c0cc00c83ea48fd );
DES_TEST ( des_test3_3,
	   0x10071034c8980120, 0x0000000000000000, 0x83bc8ef3a6570183 );
DES_TEST ( des_test3_4,
	   0x1046103489988020, 0x0000000000000000, 0xdf725dcad94ea2e9 );
DES_TEST ( des_test3_5,
	   0x1086911519190101, 0x0000000000000000, 0xe652b53b550be8b0 );
DES_TEST ( des_test3_6,
	   0x1086911519580101, 0x0000000000000000, 0xaf527120c485cbb0 );
DES_TEST ( des_test3_7,
	   0x5107b01519580101, 0x0000000000000000, 0x0f04ce393db926d5 );
DES_TEST ( des_test3_8,
	   0x1007b01519190101, 0x0000000000000000, 0xc9f00ffc74079067 );
DES_TEST ( des_test3_9,
	   0x3107915498080101, 0x0000000000000000, 0x7cfd82a593252b4e );
DES_TEST ( des_test3_10,
	   0x3107919498080101, 0x0000000000000000, 0xcb49a2f9e91363e3 );
DES_TEST ( des_test3_11,
	   0x10079115b9080140, 0x0000000000000000, 0x00b588be70d23f56 );
DES_TEST ( des_test3_12,
	   0x3107911598080140, 0x0000000000000000, 0x406a9a6ab43399ae );
DES_TEST ( des_test3_13,
	   0x1007d01589980101, 0x0000000000000000, 0x6cb773611dca9ada );
DES_TEST ( des_test3_14,
	   0x9107911589980101, 0x0000000000000000, 0x67fd21c17dbb5d70 );
DES_TEST ( des_test3_15,
	   0x9107d01589190101, 0x0000000000000000, 0x9592cb4110430787 );
DES_TEST ( des_test3_16,
	   0x1007d01598980120, 0x0000000000000000, 0xa6b7ff68a318ddd3 );
DES_TEST ( des_test3_17,
	   0x1007940498190101, 0x0000000000000000, 0x4d102196c914ca16 );
DES_TEST ( des_test3_18,
	   0x0107910491190401, 0x0000000000000000, 0x2dfa9f4573594965 );
DES_TEST ( des_test3_19,
	   0x0107910491190101, 0x0000000000000000, 0xb46604816c0e0774 );
DES_TEST ( des_test3_20,
	   0x0107940491190401, 0x0000000000000000, 0x6e7e6221a4f34e87 );
DES_TEST ( des_test3_21,
	   0x19079210981a0101, 0x0000000000000000, 0xaa85e74643233199 );
DES_TEST ( des_test3_22,
	   0x1007911998190801, 0x0000000000000000, 0x2e5a19db4d1962d6 );
DES_TEST ( des_test3_23,
	   0x10079119981a0801, 0x0000000000000000, 0x23a866a809d30894 );
DES_TEST ( des_test3_24,
	   0x1007921098190101, 0x0000000000000000, 0xd812d961f017d320 );
DES_TEST ( des_test3_25,
	   0x100791159819010b, 0x0000000000000000, 0x055605816e58608f );
DES_TEST ( des_test3_26,
	   0x1004801598190101, 0x0000000000000000, 0xabd88e8b1b7716f1 );
DES_TEST ( des_test3_27,
	   0x1004801598190102, 0x0000000000000000, 0x537ac95be69da1e1 );
DES_TEST ( des_test3_28,
	   0x1004801598190108, 0x0000000000000000, 0xaed0f6ae3c25cdd8 );
DES_TEST ( des_test3_29,
	   0x1002911498100104, 0x0000000000000000, 0xb3e35a5ee53e7b8d );
DES_TEST ( des_test3_30,
	   0x1002911598190104, 0x0000000000000000, 0x61c79c71921a2ef8 );
DES_TEST ( des_test3_31,
	   0x1002911598100201, 0x0000000000000000, 0xe2f5728f0995013c );
DES_TEST ( des_test3_32,
	   0x1002911698100101, 0x0000000000000000, 0x1aeac39a61f0a464 );

/* Test 4: Key permutation tests
 *
 * "Set Data=0 and use the keys e[i]: i=1,...,64 ignoring i=8,16,...,64"
 *
 * Test 4 part 1 is the forward direction as described above.  Test 4
 * part 2 ("set data=c[i] from part 1 ... then decipher") is carried
 * out for us automatically, since CIPHER_TEST() performs both
 * encryption and decryption tests.
 *
 * Appendix B, page 30 ("PC1 and PC2 test")
 */
DES_TEST ( des_test4_1,
	   0x8001010101010101, 0x0000000000000000, 0x95a8d72813daa94d );
DES_TEST ( des_test4_2,
	   0x4001010101010101, 0x0000000000000000, 0x0eec1487dd8c26d5 );
DES_TEST ( des_test4_3,
	   0x2001010101010101, 0x0000000000000000, 0x7ad16ffb79c45926 );
DES_TEST ( des_test4_4,
	   0x1001010101010101, 0x0000000000000000, 0xd3746294ca6a6cf3 );
DES_TEST ( des_test4_5,
	   0x0801010101010101, 0x0000000000000000, 0x809f5f873c1fd761 );
DES_TEST ( des_test4_6,
	   0x0401010101010101, 0x0000000000000000, 0xc02faffec989d1fc );
DES_TEST ( des_test4_7,
	   0x0201010101010101, 0x0000000000000000, 0x4615aa1d33e72f10 );
DES_TEST ( des_test4_8,
	   0x0180010101010101, 0x0000000000000000, 0x2055123350c00858 );
DES_TEST ( des_test4_9,
	   0x0140010101010101, 0x0000000000000000, 0xdf3b99d6577397c8 );
DES_TEST ( des_test4_10,
	   0x0120010101010101, 0x0000000000000000, 0x31fe17369b5288c9 );
DES_TEST ( des_test4_11,
	   0x0110010101010101, 0x0000000000000000, 0xdfdd3cc64dae1642 );
DES_TEST ( des_test4_12,
	   0x0108010101010101, 0x0000000000000000, 0x178c83ce2b399d94 );
DES_TEST ( des_test4_13,
	   0x0104010101010101, 0x0000000000000000, 0x50f636324a9b7f80 );
DES_TEST ( des_test4_14,
	   0x0102010101010101, 0x0000000000000000, 0xa8468ee3bc18f06d );
DES_TEST ( des_test4_15,
	   0x0101800101010101, 0x0000000000000000, 0xa2dc9e92fd3cde92 );
DES_TEST ( des_test4_16,
	   0x0101400101010101, 0x0000000000000000, 0xcac09f797d031287 );
DES_TEST ( des_test4_17,
	   0x0101200101010101, 0x0000000000000000, 0x90ba680b22aeb525 );
DES_TEST ( des_test4_18,
	   0x0101100101010101, 0x0000000000000000, 0xce7a24f350e280b6 );
DES_TEST ( des_test4_19,
	   0x0101080101010101, 0x0000000000000000, 0x882bff0aa01a0b87 );
DES_TEST ( des_test4_20,
	   0x0101040101010101, 0x0000000000000000, 0x25610288924511c2 );
DES_TEST ( des_test4_21,
	   0x0101020101010101, 0x0000000000000000, 0xc71516c29c75d170 );
DES_TEST ( des_test4_22,
	   0x0101018001010101, 0x0000000000000000, 0x5199c29a52c9f059 );
DES_TEST ( des_test4_23,
	   0x0101014001010101, 0x0000000000000000, 0xc22f0a294a71f29f );
DES_TEST ( des_test4_24,
	   0x0101012001010101, 0x0000000000000000, 0xee371483714c02ea );
DES_TEST ( des_test4_25,
	   0x0101011001010101, 0x0000000000000000, 0xa81fbd448f9e522f );
DES_TEST ( des_test4_26,
	   0x0101010801010101, 0x0000000000000000, 0x4f644c92e192dfed );
DES_TEST ( des_test4_27,
	   0x0101010401010101, 0x0000000000000000, 0x1afa9a66a6df92ae );
DES_TEST ( des_test4_28,
	   0x0101010201010101, 0x0000000000000000, 0xb3c1cc715cb879d8 );
DES_TEST ( des_test4_29,
	   0x0101010180010101, 0x0000000000000000, 0x19d032e64ab0bd8b );
DES_TEST ( des_test4_30,
	   0x0101010140010101, 0x0000000000000000, 0x3cfaa7a7dc8720dc );
DES_TEST ( des_test4_31,
	   0x0101010120010101, 0x0000000000000000, 0xb7265f7f447ac6f3 );
DES_TEST ( des_test4_32,
	   0x0101010110010101, 0x0000000000000000, 0x9db73b3c0d163f54 );
DES_TEST ( des_test4_33,
	   0x0101010108010101, 0x0000000000000000, 0x8181b65babf4a975 );
DES_TEST ( des_test4_34,
	   0x0101010104010101, 0x0000000000000000, 0x93c9b64042eaa240 );
DES_TEST ( des_test4_35,
	   0x0101010102010101, 0x0000000000000000, 0x5570530829705592 );
DES_TEST ( des_test4_36,
	   0x0101010101800101, 0x0000000000000000, 0x8638809e878787a0 );
DES_TEST ( des_test4_37,
	   0x0101010101400101, 0x0000000000000000, 0x41b9a79af79ac208 );
DES_TEST ( des_test4_38,
	   0x0101010101200101, 0x0000000000000000, 0x7a9be42f2009a892 );
DES_TEST ( des_test4_39,
	   0x0101010101100101, 0x0000000000000000, 0x29038d56ba6d2745 );
DES_TEST ( des_test4_40,
	   0x0101010101080101, 0x0000000000000000, 0x5495c6abf1e5df51 );
DES_TEST ( des_test4_41,
	   0x0101010101040101, 0x0000000000000000, 0xae13dbd561488933 );
DES_TEST ( des_test4_42,
	   0x0101010101020101, 0x0000000000000000, 0x024d1ffa8904e389 );
DES_TEST ( des_test4_43,
	   0x0101010101018001, 0x0000000000000000, 0xd1399712f99bf02e );
DES_TEST ( des_test4_44,
	   0x0101010101014001, 0x0000000000000000, 0x14c1d7c1cffec79e );
DES_TEST ( des_test4_45,
	   0x0101010101012001, 0x0000000000000000, 0x1de5279dae3bed6f );
DES_TEST ( des_test4_46,
	   0x0101010101011001, 0x0000000000000000, 0xe941a33f85501303 );
DES_TEST ( des_test4_47,
	   0x0101010101010801, 0x0000000000000000, 0xda99dbbc9a03f379 );
DES_TEST ( des_test4_48,
	   0x0101010101010401, 0x0000000000000000, 0xb7fc92f91d8e92e9 );
DES_TEST ( des_test4_49,
	   0x0101010101010201, 0x0000000000000000, 0xae8e5caa3ca04e85 );
DES_TEST ( des_test4_50,
	   0x0101010101010180, 0x0000000000000000, 0x9cc62df43b6eed74 );
DES_TEST ( des_test4_51,
	   0x0101010101010140, 0x0000000000000000, 0xd863dbb5c59a91a0 );
DES_TEST ( des_test4_52,
	   0x0101010101010120, 0x0000000000000000, 0xa1ab2190545b91d7 );
DES_TEST ( des_test4_53,
	   0x0101010101010110, 0x0000000000000000, 0x0875041e64c570f7 );
DES_TEST ( des_test4_54,
	   0x0101010101010108, 0x0000000000000000, 0x5a594528bebef1cc );
DES_TEST ( des_test4_55,
	   0x0101010101010104, 0x0000000000000000, 0xfcdb3291de21f0c0 );
DES_TEST ( des_test4_56,
	   0x0101010101010102, 0x0000000000000000, 0x869efd7f9f265a09 );

/* Test 5: S-box tests
 *
 * "Set Data and Key equal to the inputs defined in the Substitution
 * Table test"
 *
 * Appendix B, page 33 ("19 key data pairs which exercise every S-box entry")
 */
DES_TEST ( des_test5_1,
	   0x7ca110454a1a6e57, 0x01a1d6d039776742, 0x690f5b0d9a26939b );
DES_TEST ( des_test5_2,
	   0x0131d9619dc1376e, 0x5cd54ca83def57da, 0x7a389d10354bd271 );
DES_TEST ( des_test5_3,
	   0x07a1133e4a0b2686, 0x0248d43806f67172, 0x868ebb51cab4599a );
DES_TEST ( des_test5_4,
	   0x3849674c2602319e, 0x51454b582ddf440a, 0x7178876e01f19b2a );
DES_TEST ( des_test5_5,
	   0x04b915ba43feb5b6, 0x42fd443059577fa2, 0xaf37fb421f8c4095 );
DES_TEST ( des_test5_6,
	   0x0113b970fd34f2ce, 0x059b5e0851cf143a, 0x86a560f10ec6d85b );
DES_TEST ( des_test5_7,
	   0x0170f175468fb5e6, 0x0756d8e0774761d2, 0x0cd3da020021dc09 );
DES_TEST ( des_test5_8,
	   0x43297fad38e373fe, 0x762514b829bf486a, 0xea676b2cb7db2b7a );
DES_TEST ( des_test5_9,
	   0x07a7137045da2a16, 0x3bdd119049372802, 0xdfd64a815caf1a0f );
DES_TEST ( des_test5_10,
	   0x04689104c2fd3b2f, 0x26955f6835af609a, 0x5c513c9c4886c088 );
DES_TEST ( des_test5_11,
	   0x37d06bb516cb7546, 0x164d5e404f275232, 0x0a2aeeae3ff4ab77 );
DES_TEST ( des_test5_12,
	   0x1f08260d1ac2465e, 0x6b056e18759f5cca, 0xef1bf03e5dfa575a );
DES_TEST ( des_test5_13,
	   0x584023641aba6176, 0x004bd6ef09176062, 0x88bf0db6d70dee56 );
DES_TEST ( des_test5_14,
	   0x025816164629b007, 0x480d39006ee762f2, 0xa1f9915541020b56 );
DES_TEST ( des_test5_15,
	   0x49793ebc79b3258f, 0x437540c8698f3cfa, 0x6fbf1cafcffd0556 );
DES_TEST ( des_test5_16,
	   0x4fb05e1515ab73a7, 0x072d43a077075292, 0x2f22e49bab7ca1ac );
DES_TEST ( des_test5_17,
	   0x49e95d6d4ca229bf, 0x02fe55778117f12a, 0x5a6b612cc26cce4a );
DES_TEST ( des_test5_18,
	   0x018310dc409b26d6, 0x1d9d5c5018f728c2, 0x5f4c038ed12b2e41 );
DES_TEST ( des_test5_19,
	   0x1c587f1c13924fef, 0x305532286d6f295a, 0x63fac0d034d9f793 );

/* Unofficial tests
 *
 * The official tests are all exactly one block in length.  Add some
 * multi-block tests (generated in Python).
 */
CIPHER_TEST ( des_unofficial_ecb, &des_ecb_algorithm,
	      KEY ( 0x6e, 0x6f, 0x70, 0x61, 0x72, 0x69, 0x74, 0x79 ),
	      IV(), ADDITIONAL(),
	      PLAINTEXT ( 0x53, 0x6f, 0x20, 0x63, 0x75, 0x74, 0x65, 0x20,
			  0x74, 0x6f, 0x20, 0x73, 0x65, 0x65, 0x20, 0x61,
			  0x20, 0x73, 0x70, 0x65, 0x63, 0x69, 0x66, 0x69,
			  0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x77,
			  0x72, 0x69, 0x74, 0x74, 0x65, 0x6e, 0x20, 0x6f,
			  0x6e, 0x20, 0x61, 0x6e, 0x20, 0x61, 0x63, 0x74,
			  0x75, 0x61, 0x6c, 0x20, 0x74, 0x79, 0x70, 0x65,
			  0x77, 0x72, 0x69, 0x74, 0x65, 0x72, 0x21, 0x21 ),
	      CIPHERTEXT ( 0x1a, 0x02, 0x17, 0xcb, 0x93, 0xa3, 0xd2, 0xf2,
			   0xf9, 0x45, 0x71, 0x1c, 0x33, 0xb1, 0x5c, 0xa4,
			   0x8b, 0x6b, 0x11, 0x7a, 0x7c, 0x86, 0x7c, 0x7f,
			   0x9f, 0x56, 0x61, 0x46, 0x7f, 0xa6, 0xae, 0xf1,
			   0x49, 0xf7, 0x53, 0xe0, 0xbc, 0x15, 0x6a, 0x30,
			   0xe7, 0xf8, 0xf3, 0x29, 0x11, 0xd8, 0x7d, 0x04,
			   0x62, 0x5a, 0xaa, 0xa1, 0x89, 0x61, 0x4c, 0xf6,
			   0x5a, 0x47, 0x3b, 0xc6, 0x04, 0x15, 0xce, 0xf6 ),
	      AUTH() );
CIPHER_TEST ( des_unofficial_cbc, &des_cbc_algorithm,
	      KEY ( 0x6e, 0x6f, 0x70, 0x61, 0x72, 0x69, 0x74, 0x79 ),
	      IV ( 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 ),
	      ADDITIONAL(),
	      PLAINTEXT ( 0x53, 0x6f, 0x20, 0x63, 0x75, 0x74, 0x65, 0x20,
			  0x74, 0x6f, 0x20, 0x73, 0x65, 0x65, 0x20, 0x61,
			  0x20, 0x73, 0x70, 0x65, 0x63, 0x69, 0x66, 0x69,
			  0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x77,
			  0x72, 0x69, 0x74, 0x74, 0x65, 0x6e, 0x20, 0x6f,
			  0x6e, 0x20, 0x61, 0x6e, 0x20, 0x61, 0x63, 0x74,
			  0x75, 0x61, 0x6c, 0x20, 0x74, 0x79, 0x70, 0x65,
			  0x77, 0x72, 0x69, 0x74, 0x65, 0x72, 0x21, 0x21 ),
	      CIPHERTEXT ( 0x4c, 0x5f, 0x62, 0xfc, 0xf4, 0x93, 0x09, 0xb5,
			   0x1d, 0x52, 0x25, 0xec, 0xc7, 0x42, 0x3c, 0x29,
			   0x33, 0x67, 0xf5, 0xe9, 0xd6, 0x3c, 0x27, 0x5b,
			   0x49, 0x69, 0xc5, 0xa9, 0x08, 0xa3, 0x14, 0x66,
			   0x3c, 0x95, 0x33, 0x30, 0xcf, 0x3c, 0x7c, 0xaf,
			   0xa3, 0xe4, 0xf8, 0x2e, 0xc3, 0x55, 0x57, 0x81,
			   0x33, 0xd9, 0x90, 0xe2, 0x99, 0xdc, 0x32, 0x10,
			   0x13, 0x21, 0xb6, 0xc1, 0x6b, 0x0f, 0x22, 0xa9 ),
	      AUTH() );

/**
 * Perform DES self-test
 *
 */
static void des_test_exec ( void ) {

	/* Sample round outputs (page 9) */
	cipher_ok ( &des_round_sample );

	/* Test 1: Initial permutation and expansion tests */
	cipher_ok ( &des_test1_1 );
	cipher_ok ( &des_test1_2 );
	cipher_ok ( &des_test1_3 );
	cipher_ok ( &des_test1_4 );
	cipher_ok ( &des_test1_5 );
	cipher_ok ( &des_test1_6 );
	cipher_ok ( &des_test1_7 );
	cipher_ok ( &des_test1_8 );
	cipher_ok ( &des_test1_9 );
	cipher_ok ( &des_test1_10 );
	cipher_ok ( &des_test1_11 );
	cipher_ok ( &des_test1_12 );
	cipher_ok ( &des_test1_13 );
	cipher_ok ( &des_test1_14 );
	cipher_ok ( &des_test1_15 );
	cipher_ok ( &des_test1_16 );
	cipher_ok ( &des_test1_17 );
	cipher_ok ( &des_test1_18 );
	cipher_ok ( &des_test1_19 );
	cipher_ok ( &des_test1_20 );
	cipher_ok ( &des_test1_21 );
	cipher_ok ( &des_test1_22 );
	cipher_ok ( &des_test1_23 );
	cipher_ok ( &des_test1_24 );
	cipher_ok ( &des_test1_25 );
	cipher_ok ( &des_test1_26 );
	cipher_ok ( &des_test1_27 );
	cipher_ok ( &des_test1_28 );
	cipher_ok ( &des_test1_29 );
	cipher_ok ( &des_test1_30 );
	cipher_ok ( &des_test1_31 );
	cipher_ok ( &des_test1_32 );
	cipher_ok ( &des_test1_33 );
	cipher_ok ( &des_test1_34 );
	cipher_ok ( &des_test1_35 );
	cipher_ok ( &des_test1_36 );
	cipher_ok ( &des_test1_37 );
	cipher_ok ( &des_test1_38 );
	cipher_ok ( &des_test1_39 );
	cipher_ok ( &des_test1_40 );
	cipher_ok ( &des_test1_41 );
	cipher_ok ( &des_test1_42 );
	cipher_ok ( &des_test1_43 );
	cipher_ok ( &des_test1_44 );
	cipher_ok ( &des_test1_45 );
	cipher_ok ( &des_test1_46 );
	cipher_ok ( &des_test1_47 );
	cipher_ok ( &des_test1_48 );
	cipher_ok ( &des_test1_49 );
	cipher_ok ( &des_test1_50 );
	cipher_ok ( &des_test1_51 );
	cipher_ok ( &des_test1_52 );
	cipher_ok ( &des_test1_53 );
	cipher_ok ( &des_test1_54 );
	cipher_ok ( &des_test1_55 );
	cipher_ok ( &des_test1_56 );
	cipher_ok ( &des_test1_57 );
	cipher_ok ( &des_test1_58 );
	cipher_ok ( &des_test1_59 );
	cipher_ok ( &des_test1_60 );
	cipher_ok ( &des_test1_61 );
	cipher_ok ( &des_test1_62 );
	cipher_ok ( &des_test1_63 );
	cipher_ok ( &des_test1_64 );

	/* Test 2: Inverse permutation and expansion tests */
	cipher_ok ( &des_test2_1 );
	cipher_ok ( &des_test2_2 );
	cipher_ok ( &des_test2_3 );
	cipher_ok ( &des_test2_4 );
	cipher_ok ( &des_test2_5 );
	cipher_ok ( &des_test2_6 );
	cipher_ok ( &des_test2_7 );
	cipher_ok ( &des_test2_8 );
	cipher_ok ( &des_test2_9 );
	cipher_ok ( &des_test2_10 );
	cipher_ok ( &des_test2_11 );
	cipher_ok ( &des_test2_12 );
	cipher_ok ( &des_test2_13 );
	cipher_ok ( &des_test2_14 );
	cipher_ok ( &des_test2_15 );
	cipher_ok ( &des_test2_16 );
	cipher_ok ( &des_test2_17 );
	cipher_ok ( &des_test2_18 );
	cipher_ok ( &des_test2_19 );
	cipher_ok ( &des_test2_20 );
	cipher_ok ( &des_test2_21 );
	cipher_ok ( &des_test2_22 );
	cipher_ok ( &des_test2_23 );
	cipher_ok ( &des_test2_24 );
	cipher_ok ( &des_test2_25 );
	cipher_ok ( &des_test2_26 );
	cipher_ok ( &des_test2_27 );
	cipher_ok ( &des_test2_28 );
	cipher_ok ( &des_test2_29 );
	cipher_ok ( &des_test2_30 );
	cipher_ok ( &des_test2_31 );
	cipher_ok ( &des_test2_32 );
	cipher_ok ( &des_test2_33 );
	cipher_ok ( &des_test2_34 );
	cipher_ok ( &des_test2_35 );
	cipher_ok ( &des_test2_36 );
	cipher_ok ( &des_test2_37 );
	cipher_ok ( &des_test2_38 );
	cipher_ok ( &des_test2_39 );
	cipher_ok ( &des_test2_40 );
	cipher_ok ( &des_test2_41 );
	cipher_ok ( &des_test2_42 );
	cipher_ok ( &des_test2_43 );
	cipher_ok ( &des_test2_44 );
	cipher_ok ( &des_test2_45 );
	cipher_ok ( &des_test2_46 );
	cipher_ok ( &des_test2_47 );
	cipher_ok ( &des_test2_48 );
	cipher_ok ( &des_test2_49 );
	cipher_ok ( &des_test2_50 );
	cipher_ok ( &des_test2_51 );
	cipher_ok ( &des_test2_52 );
	cipher_ok ( &des_test2_53 );
	cipher_ok ( &des_test2_54 );
	cipher_ok ( &des_test2_55 );
	cipher_ok ( &des_test2_56 );
	cipher_ok ( &des_test2_57 );
	cipher_ok ( &des_test2_58 );
	cipher_ok ( &des_test2_59 );
	cipher_ok ( &des_test2_60 );
	cipher_ok ( &des_test2_61 );
	cipher_ok ( &des_test2_62 );
	cipher_ok ( &des_test2_63 );
	cipher_ok ( &des_test2_64 );

	/* Test 3: Data permutation tests */
	cipher_ok ( &des_test3_1 );
	cipher_ok ( &des_test3_2 );
	cipher_ok ( &des_test3_3 );
	cipher_ok ( &des_test3_4 );
	cipher_ok ( &des_test3_5 );
	cipher_ok ( &des_test3_6 );
	cipher_ok ( &des_test3_7 );
	cipher_ok ( &des_test3_8 );
	cipher_ok ( &des_test3_9 );
	cipher_ok ( &des_test3_10 );
	cipher_ok ( &des_test3_11 );
	cipher_ok ( &des_test3_12 );
	cipher_ok ( &des_test3_13 );
	cipher_ok ( &des_test3_14 );
	cipher_ok ( &des_test3_15 );
	cipher_ok ( &des_test3_16 );
	cipher_ok ( &des_test3_17 );
	cipher_ok ( &des_test3_18 );
	cipher_ok ( &des_test3_19 );
	cipher_ok ( &des_test3_20 );
	cipher_ok ( &des_test3_21 );
	cipher_ok ( &des_test3_22 );
	cipher_ok ( &des_test3_23 );
	cipher_ok ( &des_test3_24 );
	cipher_ok ( &des_test3_25 );
	cipher_ok ( &des_test3_26 );
	cipher_ok ( &des_test3_27 );
	cipher_ok ( &des_test3_28 );
	cipher_ok ( &des_test3_29 );
	cipher_ok ( &des_test3_30 );
	cipher_ok ( &des_test3_31 );
	cipher_ok ( &des_test3_32 );

	/* Test 4: Key permutation tests */
	cipher_ok ( &des_test4_1 );
	cipher_ok ( &des_test4_2 );
	cipher_ok ( &des_test4_3 );
	cipher_ok ( &des_test4_4 );
	cipher_ok ( &des_test4_5 );
	cipher_ok ( &des_test4_6 );
	cipher_ok ( &des_test4_7 );
	cipher_ok ( &des_test4_8 );
	cipher_ok ( &des_test4_9 );
	cipher_ok ( &des_test4_10 );
	cipher_ok ( &des_test4_11 );
	cipher_ok ( &des_test4_12 );
	cipher_ok ( &des_test4_13 );
	cipher_ok ( &des_test4_14 );
	cipher_ok ( &des_test4_15 );
	cipher_ok ( &des_test4_16 );
	cipher_ok ( &des_test4_17 );
	cipher_ok ( &des_test4_18 );
	cipher_ok ( &des_test4_19 );
	cipher_ok ( &des_test4_20 );
	cipher_ok ( &des_test4_21 );
	cipher_ok ( &des_test4_22 );
	cipher_ok ( &des_test4_23 );
	cipher_ok ( &des_test4_24 );
	cipher_ok ( &des_test4_25 );
	cipher_ok ( &des_test4_26 );
	cipher_ok ( &des_test4_27 );
	cipher_ok ( &des_test4_28 );
	cipher_ok ( &des_test4_29 );
	cipher_ok ( &des_test4_30 );
	cipher_ok ( &des_test4_31 );
	cipher_ok ( &des_test4_32 );
	cipher_ok ( &des_test4_33 );
	cipher_ok ( &des_test4_34 );
	cipher_ok ( &des_test4_35 );
	cipher_ok ( &des_test4_36 );
	cipher_ok ( &des_test4_37 );
	cipher_ok ( &des_test4_38 );
	cipher_ok ( &des_test4_39 );
	cipher_ok ( &des_test4_40 );
	cipher_ok ( &des_test4_41 );
	cipher_ok ( &des_test4_42 );
	cipher_ok ( &des_test4_43 );
	cipher_ok ( &des_test4_44 );
	cipher_ok ( &des_test4_45 );
	cipher_ok ( &des_test4_46 );
	cipher_ok ( &des_test4_47 );
	cipher_ok ( &des_test4_48 );
	cipher_ok ( &des_test4_49 );
	cipher_ok ( &des_test4_50 );
	cipher_ok ( &des_test4_51 );
	cipher_ok ( &des_test4_52 );
	cipher_ok ( &des_test4_53 );
	cipher_ok ( &des_test4_54 );
	cipher_ok ( &des_test4_55 );
	cipher_ok ( &des_test4_56 );

	/* Test 5: S-box tests */
	cipher_ok ( &des_test5_1 );
	cipher_ok ( &des_test5_2 );
	cipher_ok ( &des_test5_3 );
	cipher_ok ( &des_test5_4 );
	cipher_ok ( &des_test5_5 );
	cipher_ok ( &des_test5_6 );
	cipher_ok ( &des_test5_7 );
	cipher_ok ( &des_test5_8 );
	cipher_ok ( &des_test5_9 );
	cipher_ok ( &des_test5_10 );
	cipher_ok ( &des_test5_11 );
	cipher_ok ( &des_test5_12 );
	cipher_ok ( &des_test5_13 );
	cipher_ok ( &des_test5_14 );
	cipher_ok ( &des_test5_15 );
	cipher_ok ( &des_test5_16 );
	cipher_ok ( &des_test5_17 );
	cipher_ok ( &des_test5_18 );
	cipher_ok ( &des_test5_19 );

	/* Multi-block tests */
	cipher_ok ( &des_unofficial_ecb );
	cipher_ok ( &des_unofficial_cbc );

	/* Speed tests */
	DBG ( "DES-ECB encryption required %ld cycles per byte\n",
	      cipher_cost_encrypt ( &des_ecb_algorithm, 8 ) );
	DBG ( "DES-ECB decryption required %ld cycles per byte\n",
	      cipher_cost_decrypt ( &des_ecb_algorithm, 8 ) );
	DBG ( "DES-CBC encryption required %ld cycles per byte\n",
	      cipher_cost_encrypt ( &des_cbc_algorithm, 8 ) );
	DBG ( "DES-CBC decryption required %ld cycles per byte\n",
	      cipher_cost_decrypt ( &des_cbc_algorithm, 8 ) );
}

/** DES self-test */
struct self_test des_test __self_test = {
	.name = "des",
	.exec = des_test_exec,
};
