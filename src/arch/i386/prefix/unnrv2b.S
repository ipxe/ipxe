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
 */


	.text
	.arch i386
	.section ".prefix", "ax", @progbits
	.code32
	
	.globl decompress
decompress:
	/* Save the initial register values */
	pushal

	/*
	 * See where I am running, and compute %ebp
	 * %ebp holds delta between physical and virtual addresses.
	 */
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp

	/* "compressed" and "decompress_to" defined by linker script */
	/* move compressed image up to temporary area before decompressing */
	std
	movl	$_compressed_size, %ecx
	leal	_compressed+4-1(%ebp, %ecx), %esi
	leal	_compressed_copy-1(%ebp, %ecx), %edi
	rep movsb
	/* Setup to run the decompressor */
	cld
	leal	_compressed_copy(%ebp), %esi
	leal	decompress_to(%ebp), %edi
	movl	$-1, %ebp	/* last_m_off = -1 */
	jmp	dcl1_n2b

/* ------------- DECOMPRESSION -------------

 Input:
   %esi - source
   %edi - dest
   %ebp - -1
   cld

 Output:
   %eax - 0
   %ecx - 0
*/

.macro getbit bits
.if	\bits == 1
	addl	%ebx, %ebx
	jnz	1f
.endif
	movl	(%esi), %ebx
	subl	$-4, %esi	/* sets carry flag */
	adcl	%ebx, %ebx
1:
.endm

decompr_literals_n2b:
	movsb

decompr_loop_n2b:
	addl	%ebx, %ebx
	jnz	dcl2_n2b
dcl1_n2b:
	getbit	32
dcl2_n2b:
	jc	decompr_literals_n2b
	xorl	%eax, %eax
	incl	%eax		/* m_off = 1 */
loop1_n2b:
	getbit	1
	adcl	%eax, %eax	/* m_off = m_off*2 + getbit() */
	getbit	1
	jnc	loop1_n2b	/* while(!getbit()) */
	xorl	%ecx, %ecx
	subl	$3, %eax
	jb	decompr_ebpeax_n2b	/* if (m_off == 2) goto decompr_ebpeax_n2b ? */
	shll	$8, %eax	
	movb	(%esi), %al	/* m_off = (m_off - 3)*256 + src[ilen++] */
	incl	%esi
	xorl	$-1, %eax	
	jz	decompr_end_n2b	/* if (m_off == 0xffffffff) goto decomp_end_n2b */
	movl	%eax, %ebp	/* last_m_off = m_off ?*/
decompr_ebpeax_n2b:
	getbit	1		
	adcl	%ecx, %ecx	/* m_len = getbit() */
	getbit	1
	adcl	%ecx, %ecx	/* m_len = m_len*2 + getbit()) */
	jnz	decompr_got_mlen_n2b	/* if (m_len == 0) goto decompr_got_mlen_n2b */
	incl	%ecx		/* m_len++ */
loop2_n2b:
	getbit	1	
	adcl	%ecx, %ecx	/* m_len = m_len*2 + getbit() */
	getbit	1
	jnc	loop2_n2b	/* while(!getbit()) */
	incl	%ecx
	incl	%ecx		/* m_len += 2 */
decompr_got_mlen_n2b:
	cmpl	$-0xd00, %ebp
	adcl	$1, %ecx	/* m_len = m_len + 1 + (last_m_off > 0xd00) */
	pushl	%esi
	leal	(%edi,%ebp), %esi	/* m_pos = dst + olen + -m_off  */
	rep
	movsb			/* dst[olen++] = *m_pos++ while(m_len > 0) */
	popl	%esi
	jmp	decompr_loop_n2b
decompr_end_n2b:
	/* Restore the initial register values */
	popal
	ret
