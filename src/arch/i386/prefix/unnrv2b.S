/* 
 * Copyright (C) 1996-2002 Markus Franz Xaver Johannes Oberhumer
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * Originally this code was part of ucl the data compression library
 * for upx the ``Ultimate Packer of eXecutables''.
 *
 * - Converted to gas assembly, and refitted to work with etherboot.
 *   Eric Biederman 20 Aug 2002
 *
 * - Structure modified to be a subroutine call rather than an
 *   executable prefix.
 *   Michael Brown 30 Mar 2004
 *
 * - Modified to be compilable as either 16-bit or 32-bit code.
 *   Michael Brown 9 Mar 2005
 */

/****************************************************************************
 * This file provides the decompress_block() and decompress_block16()
 * functions which can be called in order to decompress an image
 * compressed with the nrv2b utility in src/util.
 *
 * These functions are designed to be called by the prefix.  They are
 * position-independent code.
 *
 * The same basic assembly code is used to compile both
 * decompress_block() and decompress_block16().
 ****************************************************************************
 */

	.text
	.arch i386
	.section ".prefix", "ax", @progbits

#ifdef CODE16
/****************************************************************************
 * decompress_block16 (real-mode near call, position independent)
 *
 * Parameters (passed via registers):
 *   %ds:%si - Pointer to compressed input data
 *   %es:%di - Pointer to output buffer
 * Returns:
 *   All registers are preserved
 *
 * NOTE: The compressed data size must be in the range [1,65533-%si]
 * and the uncompressed data size must be in the range [1,65536-%di]
 * (where %si and %di are the input values for those registers).  Note
 * particularly that the lower limit is 1, not 0, and that the upper
 * limit on the input (compressed) data really is 65533, since the
 * algorithm may read up to three bytes beyond the end of the input
 * data, since it reads dwords.
 *
 * Although splitting up the data into (almost) 64kB chunks for
 * compression is awkward and worsens the compression ratio, it has
 * little to no practical effect since our image size is currently
 * <64kB for all single drivers.  Having a decompression routine that
 * can run in real-mode avoids the need to duplicate RM-to-PM
 * transition code from librm (or have part of librm kept
 * uncompressed, which is itself awkward) and means that we don't need
 * to set up the PM stack until we hit the setup routine itself.
 ****************************************************************************
 */

#define REG(x) x

	.code16
	.globl	decompress_block16
decompress_block16:
	
#else /* CODE16 */

/****************************************************************************
 * decompress_block (32-bit protected-mode near call, position independent)
 *
 * Parameters (passed via registers):
 *   %ds:%esi - Pointer to compressed input data
 *   %es:%edi - Pointer to output buffer
 * Returns:
 *   All registers are preserved
 ****************************************************************************
 */

#define REG(x) e ## x
	
	.code32
	.globl	decompress_block
decompress_block:

#endif /* CODE16 */

#define xAX	REG(ax)
#define xCX	REG(cx)
#define xBP	REG(bp)
#define xSI	REG(si)
#define xDI	REG(di)

	/* Save registers */
	pushal
	/* Do the decompression */
	cld
	xor	%xBP, %xBP
	dec	%xBP		/* last_m_off = -1 */
	jmp	dcl1_n2b
	
decompr_literals_n2b:
	movsb
decompr_loop_n2b:
	addl	%ebx, %ebx
	jnz	dcl2_n2b
dcl1_n2b:
	call	getbit32
dcl2_n2b:
	jc	decompr_literals_n2b
	xor	%xAX, %xAX
	inc	%xAX		/* m_off = 1 */
loop1_n2b:
	call	getbit1
	adc	%xAX, %xAX	/* m_off = m_off*2 + getbit() */
	call	getbit1
	jnc	loop1_n2b	/* while(!getbit()) */
	sub	$3, %xAX
	jb	decompr_ebpeax_n2b	/* if (m_off == 2) goto decompr_ebpeax_n2b ? */
	shl	$8, %xAX	
	movb	(%xSI), %al	/* m_off = (m_off - 3)*256 + src[ilen++] */
	inc	%xSI
	not	%xAX	
	jz	decompr_end_n2b	/* if (m_off == 0xffffffff) goto decomp_end_n2b */
	mov	%xAX, %xBP	/* last_m_off = m_off ?*/
decompr_ebpeax_n2b:
	xor	%xCX, %xCX
	call	getbit1
	adc	%xCX, %xCX	/* m_len = getbit() */
	call	getbit1
	adc	%xCX, %xCX	/* m_len = m_len*2 + getbit()) */
	jnz	decompr_got_mlen_n2b	/* if (m_len == 0) goto decompr_got_mlen_n2b */
	inc	%xCX		/* m_len++ */
loop2_n2b:
	call	getbit1	
	adc	%xCX, %xCX	/* m_len = m_len*2 + getbit() */
	call	getbit1
	jnc	loop2_n2b	/* while(!getbit()) */
	inc	%xCX
	inc	%xCX		/* m_len += 2 */
decompr_got_mlen_n2b:
	cmp	$-0xd00, %xBP
	adc	$1, %xCX	/* m_len = m_len + 1 + (last_m_off > 0xd00) */
	push	%xSI
	lea	(%xBP,%xDI), %xSI	/* m_pos = dst + olen + -m_off  */
	rep
	es movsb		/* dst[olen++] = *m_pos++ while(m_len > 0) */
	pop	%xSI
	jmp	decompr_loop_n2b


getbit1:
	addl	%ebx, %ebx
	jnz	1f
getbit32:
	movl	(%xSI), %ebx
	sub	$-4, %xSI	/* sets carry flag */
	adcl	%ebx, %ebx
1:
	ret

decompr_end_n2b:
	/* Restore registers and return */
	popal
	ret
