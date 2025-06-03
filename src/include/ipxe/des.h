#ifndef _IPXE_DES_H
#define _IPXE_DES_H

/** @file
 *
 * DES algorithm
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/crypto.h>

/** A DES 32-bit dword value
 *
 * DES views data as 64-bit big-endian values, typically handled as a
 * most-significant "left" half and a least-significant "right" half.
 */
union des_dword {
	/** Raw bytes */
	uint8_t byte[4];
	/** 32-bit big-endian dword */
	uint32_t dword;
};

/** A DES 64-bit block */
union des_block {
	/** Raw bytes */
	uint8_t byte[8];
	/** 32-bit big-endian dwords */
	uint32_t dword[2];
	/** Named left and right halves */
	struct {
		/** Left (most significant) half */
		union des_dword left;
		/** Right (least significant) half */
		union des_dword right;
	};
	/** Named "C" and "D" halves */
	struct {
		/** "C" (most significant) half */
		union des_dword c;
		/** "D" (least significant) half */
		union des_dword d;
	};
};

/** DES blocksize */
#define DES_BLOCKSIZE sizeof ( union des_block )

/** A DES round key
 *
 * A DES round key is a 48-bit value, consumed as 8 groups of 6 bits.
 * We store these as 8 separate bytes, for simplicity of consumption.
 */
union des_round_key {
	/** Raw bytes */
	uint8_t byte[8];
	/** 32-bit big-endian dwords */
	uint32_t dword[2];
	/** 6-bit step key byte
	 *
	 * There are 8 steps within a DES round (one step per S-box).
	 * Each step requires six bits of the round key.
	 *
	 * As an optimisation, we store the least significant of the 6
	 * bits in the sign bit of a signed 8-bit value, and the
	 * remaining 5 bits in the least significant 5 bits of the
	 * 8-bit value.  See the comments in des_sbox() for further
	 * details.
	 */
	int8_t step[8];
};

/** Number of DES rounds */
#define DES_ROUNDS 16

/** DES context */
struct des_context {
	/** Round keys */
	union des_round_key rkey[DES_ROUNDS];
};

/** DES context size */
#define DES_CTX_SIZE sizeof ( struct des_context )

extern struct cipher_algorithm des_algorithm;
extern struct cipher_algorithm des_ecb_algorithm;
extern struct cipher_algorithm des_cbc_algorithm;

#endif /* _IPXE_DES_H */
