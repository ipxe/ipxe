/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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

	FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL )

/** @file
 *
 * TCP/IP checksum
 *
 */

	.section ".note.GNU-stack", "", @progbits
	.text

/**
 * Calculate continued TCP/IP checkum
 *
 * @v partial		Checksum of already-summed data, in network byte order
 * @v data		Data buffer
 * @v len		Length of data buffer
 * @ret cksum		Updated checksum, in network byte order
 *
 * In practice, this routine will only ever be called with a data
 * pointer aligned to a 16-bit boundary.  We optimise for this case,
 * ensuring that the code would still give correct output if called
 * with a misaligned pointer.
 */
	.section ".text.tcpip_continue_chksum", "ax", @progbits
	.globl	tcpip_continue_chksum
tcpip_continue_chksum:

	/* Set up register usage:
	 *
	 * a0: checksum low xlen bits
	 * a1: data pointer
	 * a2: end of data pointer
	 * a3: end of data pointer minus a constant offset of interest
	 * a4: checksum high bits (guaranteed to never carry) / constant 0xffff
	 * a5: temporary register
	 */
	not	a0, a0
	add	a2, a2, a1
	addi	a3, a2, -( __riscv_xlen / 8 )
	mv	a4, zero

	/* Skip aligned checksumming if data is too short */
	bgtu	a1, a3, post_aligned

	/* Checksum 16-bit words until we reach xlen-bit alignment (or
	 * one byte past xlen-bit alignment).
	 */
	j	2f
1:	lhu	a5, (a1)
	addi	a1, a1, 2
	add	a4, a4, a5
2:	andi	a5, a1, ( ( ( __riscv_xlen / 8 ) - 1 ) & ~1 )
	bnez	a5, 1b

	/* Checksum aligned xlen-bit words */
	j	2f
1:	LOADN	a5, (a1)
	addi	a1, a1, ( __riscv_xlen / 8 )
	add	a0, a0, a5
	sltu	a5, a0, a5
	add	a4, a4, a5
2:	bleu	a1, a3, 1b

post_aligned:
	/* Checksum remaining 16-bit words */
	addi	a3, a2, -2
	j	2f
1:	lhu	a5, (a1)
	addi	a1, a1, 2
	add	a4, a4, a5
2:	bleu	a1, a3, 1b

	/* Checksum final byte if present */
	beq	a1, a2, 1f
	lbu	a5, (a1)
	add	a4, a4, a5
1:
	/* Fold down to xlen+1 bits */
	add	a0, a0, a4
	sltu	a4, a0, a4

	/* Fold down to (xlen/2)+2 bits */
	slli	a5, a0, ( __riscv_xlen / 2 )
	srli	a0, a0, ( __riscv_xlen / 2 )
	srli	a5, a5, ( __riscv_xlen / 2 )
	add	a0, a0, a4
	add	a0, a0, a5

	/* Load constant 0xffff for use in subsequent folding */
	li	a4, 0xffff

#if __riscv_xlen >= 64
	/* Fold down to (xlen/4)+3 bits (if xlen >= 64) */
	and	a5, a0, a4
	srli	a0, a0, ( __riscv_xlen / 4 )
	add	a0, a0, a5
#endif

	/* Fold down to 16+1 bits */
	and	a5, a0, a4
	srli	a0, a0, 16
	add	a0, a0, a5

	/* Fold down to 16 bits */
	srli	a5, a0, 16
	add	a0, a0, a5
	srli	a5, a0, 17
	add	a0, a0, a5
	and	a0, a0, a4

	/* Negate and return */
	xor	a0, a0, a4
	ret
	.size	tcpip_continue_chksum, . - tcpip_continue_chksum
