/* 
 * Copyright (C) 1996-2002 Markus Franz Xaver Johannes Oberhumer
 * Copyright (C) 2002 Eric Biederman
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
 * - Converted to functional ia64 assembly (Can this get smaller?)
 *   Eric Biederman 5 Dec 2002
 */
	.text
	.globl	_start
_start:
	/* See where I am running, and compute gp */
	{
		/* Do no call alloc here as I do not know how many argument
		 * registers are being passed through the decompressor, and if I report
		 * to few the unreported registers may get stomped.
		 *
		 * Instead just explicitly get the value of ar.pfs.
		 */
		mov		r17=0
		mov		r8=ar.pfs
		mov		gp = ip		/* The linker scripts sets gp at _start */
	
	}
	{.mlx
		movl		r9=0x123456789abcdef0	/* Get uncompressed_offset into r9 */
	}
	;;
	{
		add		r14 = @gprel(payload + 4),gp
		add		r15 = r9,gp
		mov		r16=1			/* last_m_off = 1 */
	}
	{
		mov		r20 = 0xd00
		add		r21 = r9,gp
		br.sptk.few	decompr_loop_n2b
	}
	
/* ------------- DECOMPRESSION -------------

 Input:
   r8   - ar.pfs
   r14  - source
   r15  - dest
   r16  - 1
   r17  - (buffer) 0
   r20  - 0xd00 (constant)
   r21  - start address
 Usage:
   r9   - scratch register for memory copies
   r18  - scratch register for getbit
   r19  - scratch register for loads and stores
 Output:
   r2   - 0
   r3   - 0
*/

getbit:
	add			r18 = r17,r17
	;;
	cmp.ne			p8,p0 = r0,r18
	cmp.leu			p6,p7 = r18,r17
	;; 
	mov			r17 = r18
(p8)	br.cond.sptk.few	getbit_end
	/* Do a unaligned 64bit load */
	;;
	ld1			r17 = [r14],1
	;;
	ld1			r18 = [r14],1
	;;
	dep			r17 = r18,r17,8,8
	ld1			r18 = [r14],1
	;;
	dep			r17 = r18,r17,16,8
	ld1			r18 = [r14],1
	;;
	dep			r17 = r18,r17,24,8
	ld1			r18 = [r14],1
	;;
	dep			r17 = r18,r17,32,8
	ld1			r18 = [r14],1
	;;
	dep			r17 = r18,r17,40,8
	ld1			r18 = [r14],1
	;;
	dep			r17 = r18,r17,48,8
	ld1			r18 = [r14],1
	;;
	dep			r17 = r18,r17,56,8
	;;
	add			r18 = r17,r17,1
	;;
	cmp.leu			p6,p7=r18,r17
	;; 
	mov			r17=r18
	;;
getbit_end:
	br.ret.sptk.few		b6
	
	
decompr_literals_n2b:
	ld1			r19 = [r14],1
	;;
	st1			[r15] = r19,1
	;; 
decompr_loop_n2b:
	br.call.sptk.few	b6 = getbit
	;;
(p6)	br.cond.sptk.few	decompr_literals_n2b
(p7)	add			r2 = 1,r0		/* m_off = 1 */
	;; 
loop1_n2b:
	br.call.sptk.few	b6 = getbit
	;; 
(p6)	add			r2 = r2,r2,1		/* m_off = m_off*2 + getbit() */
(p7)	add			r2 = r2,r2
	br.call.sptk.few	b6 = getbit
	;; 
(p7)	br.cond.sptk.few	loop1_n2b		/* while(!getbit()) */
	;; 
	mov			r3 = r0
	cmp.eq			p6,p0 = 2,r2
	add			r2 = -3,r2
(p6)	br.cond.sptk.few	decompr_ebpeax_n2b	/* if (m_off == 2) goto decompr_ebpeax_n2b ? */
	;;
	ld1			r19 = [r14],1
	shl			r2 = r2,8
	;;
	dep			r2 = r19,r2,0,8		/* m_off = (m_off - 3)*256 + src[ilen++] */
	;;
	cmp4.eq			p6,p0 = -1,r2		/* if (m_off == 0xffffffff) goto decomp_end_n2b */
	;; 
(p6)	br.cond.sptk.few	decompr_end_n2b
	mov			r16 = r2		/* last_m_off = m_off */
	;;
decompr_ebpeax_n2b:	
	br.call.sptk.few	b6 = getbit
	;; 
(p6)	add			r3 = r3,r3,1		/* m_len = getbit() */
(p7)	add			r3 = r3,r3
	br.call.sptk.few	b6 = getbit
	;; 
(p6)	add			r3 = r3,r3,1		/* m_len = m_len*2 + getbit()) */
(p7)	add			r3 = r3,r3
	;;
	cmp.ne			p6,p0 = r0,r3
(p6)	br.cond.sptk.few	decompr_got_mlen_n2b	/* if (m_len == 0) goto decompr_got_mlen_n2b */
	add			r3 = 1,r3		/* m_len++ */
	;; 
loop2_n2b:
	br.call.sptk.few	b6 = getbit
	;; 
(p6)	add			r3 = r3,r3,1		/* m_len = m_len*2 + getbit() */
(p7)	add			r3 = r3,r3
	br.call.sptk.few	b6 = getbit
	;;
(p7)	br.cond.sptk.few	loop2_n2b		/* while(!getbit()) */
	add			r3 = 2, r3		/* m_len += 2 */
	;; 
decompr_got_mlen_n2b:
	cmp.gtu			p6,p7 = r16, r20
	;; 
(p6)	add			r3 = 2, r3		/* m_len = m_len + 1 + (last_m_off > 0xd00) */
(p7)	add			r3 = 1, r3
	sub			r9 = r15, r16,1		/* m_pos = dst + olen - last_m_off - 1  */
	;;
1:
	ld1			r19 = [r9],1
	add			r3 = -1,r3
	;;
	st1			[r15] = r19,1		/* dst[olen++] = *m_pos++ while(m_len > 0) */
	cmp.ne			p6,p0 = r0,r3
(p6)	br.cond.sptk.few	1b
	;;
	br.cond.sptk.few	decompr_loop_n2b
decompr_end_n2b:
	/* Branch to the start address */
	mov			ar.pfs=r8
	;;
	mov			b6 = r21
	;; 
	br.sptk.few		b6

payload:
