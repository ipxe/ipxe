/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
	
	.text
	.arch i386
	.section ".text16", "ax", @progbits
	.section ".data16", "aw", @progbits
	.section ".text16.data", "aw", @progbits
	.code16

#define SMAP 0x534d4150

/****************************************************************************
 * Check for overlap
 *
 * Parameters:
 *  %edx:%eax	Region start
 *  %ecx:%ebx	Region end
 *  %si		Pointer to hidden region descriptor
 * Returns:
 *  CF set	Region overlaps
 *  CF clear	No overlap
 ****************************************************************************
 */ 
	.section ".text16"
check_overlap:
	/* If start >= hidden_end, there is no overlap. */
	testl	%edx, %edx
	jnz	no_overlap
	cmpl	4(%si), %eax
	jae	no_overlap
	/* If end <= hidden_start, there is no overlap; equivalently,
	 * if end > hidden_start, there is overlap.
	*/
	testl	%ecx, %ecx
	jnz	overlap
	cmpl	0(%si), %ebx
	ja	overlap
no_overlap:
	clc
	ret
overlap:
	stc
	ret
	.size check_overlap, . - check_overlap

/****************************************************************************
 * Check for overflow/underflow
 *
 * Parameters:
 *  %edx:%eax	Region start
 *  %ecx:%ebx	Region end
 * Returns:
 *  CF set	start < end
 *  CF clear	start >= end
 ****************************************************************************
 */
	.section ".text16"
check_overflow:
	pushl	%ecx
	pushl	%ebx
	subl	%eax, %ebx
	sbbl	%edx, %ecx
	popl	%ebx
	popl	%ecx
	ret
	.size check_overflow, . - check_overflow
	
/****************************************************************************
 * Truncate towards start of region
 *
 * Parameters:
 *  %edx:%eax	Region start
 *  %ecx:%ebx	Region end
 *  %si		Pointer to hidden region descriptor
 * Returns:
 *  %edx:%eax	Modified region start
 *  %ecx:%ebx	Modified region end
 *  CF set	Region was truncated
 *  CF clear	Region was not truncated
 ****************************************************************************
 */
	.section ".text16"
truncate_to_start:
	/* If overlaps, set region end = hidden region start */
	call	check_overlap
	jnc	99f
	movl	0(%si), %ebx
	xorl	%ecx, %ecx
	/* If region end < region start, set region end = region start */
	call	check_overflow
	jnc	1f
	movl	%eax, %ebx
	movl	%edx, %ecx
1:	stc
99:	ret
	.size truncate_to_start, . - truncate_to_start

/****************************************************************************
 * Truncate towards end of region
 *
 * Parameters:
 *  %edx:%eax	Region start
 *  %ecx:%ebx	Region end
 *  %si		Pointer to hidden region descriptor
 * Returns:
 *  %edx:%eax	Modified region start
 *  %ecx:%ebx	Modified region end
 *  CF set	Region was truncated
 *  CF clear	Region was not truncated
 ****************************************************************************
 */
	.section ".text16"
truncate_to_end:
	/* If overlaps, set region start = hidden region end */
	call	check_overlap
	jnc	99f
	movl	4(%si), %eax
	xorl	%edx, %edx
	/* If region start > region end, set region start = region end */
	call	check_overflow
	jnc	1f
	movl	%ebx, %eax
	movl	%ecx, %edx
1:	stc
99:	ret
	.size truncate_to_end, . - truncate_to_end
	
/****************************************************************************
 * Truncate region
 *
 * Parameters:
 *  %edx:%eax	Region start
 *  %ecx:%ebx	Region length (*not* region end)
 *  %bp		truncate_to_start or truncate_to_end
 * Returns:
 *  %edx:%eax	Modified region start
 *  %ecx:%ebx	Modified region length
 *  CF set	Region was truncated
 *  CF clear	Region was not truncated
 ****************************************************************************
 */
	.section ".text16"
truncate:
	pushw	%si
	pushfw
	/* Convert (start,len) to (start,end) */
	addl	%eax, %ebx
	adcl	%edx, %ecx
	/* Hide all hidden regions, truncating as directed */
	movw	$hidden_regions, %si
1:	call	*%bp
	jnc	2f
	popfw	/* If CF was set, set stored CF in flags word on stack */
	stc
	pushfw
2:	addw	$8, %si
	cmpl	$0, 0(%si)
	jne	1b
	/* Convert modified (start,end) back to (start,len) */
	subl	%eax, %ebx
	sbbl	%edx, %ecx
	popfw
	popw	%si
	ret
	.size truncate, . - truncate

/****************************************************************************
 * Patch "memory above 1MB" figure
 *
 * Parameters:
 *  %ax		Memory above 1MB, in 1kB blocks
 * Returns:
 *  %ax		Modified memory above 1M in 1kB blocks
 *  CF set	Region was truncated
 *  CF clear	Region was not truncated
 ****************************************************************************
 */
	.section ".text16"
patch_1m:
	pushal
	/* Convert to (start,len) format and call truncate */
	movw	$truncate_to_start, %bp
	xorl	%ecx, %ecx
	movzwl	%ax, %ebx
	shll	$10, %ebx
	xorl	%edx, %edx
	movl	$0x100000, %eax
	call	truncate
	/* Convert back to "memory above 1MB" format and return via %ax */
	pushfw
	shrl	$10, %ebx
	popfw
	movw	%sp, %bp
	movw	%bx, 28(%bp)
	popal
	ret
	.size patch_1m, . - patch_1m

/****************************************************************************
 * Patch "memory above 16MB" figure
 *
 * Parameters:
 *  %bx		Memory above 16MB, in 64kB blocks
 * Returns:
 *  %bx		Modified memory above 16M in 64kB blocks
 *  CF set	Region was truncated
 *  CF clear	Region was not truncated
 ****************************************************************************
 */
	.section ".text16"
patch_16m:
	pushal
	/* Convert to (start,len) format and call truncate */
	movw	$truncate_to_start, %bp
	xorl	%ecx, %ecx
	shll	$16, %ebx
	xorl	%edx, %edx
	movl	$0x1000000, %eax
	call	truncate
	/* Convert back to "memory above 16MB" format and return via %bx */
	pushfw
	shrl	$16, %ebx
	popfw
	movw	%sp, %bp
	movw	%bx, 16(%bp)
	popal
	ret
	.size patch_16m, . - patch_16m

/****************************************************************************
 * Patch "memory between 1MB and 16MB" and "memory above 16MB" figures
 *
 * Parameters:
 *  %ax		Memory between 1MB and 16MB, in 1kB blocks
 *  %bx		Memory above 16MB, in 64kB blocks
 * Returns:
 *  %ax		Modified memory between 1MB and 16MB, in 1kB blocks
 *  %bx		Modified memory above 16MB, in 64kB blocks
 *  CF set	Region was truncated
 *  CF clear	Region was not truncated
 ****************************************************************************
 */
	.section ".text16"
patch_1m_16m:
	call	patch_1m
	jc	1f
	call	patch_16m
	ret
1:	/* 1m region was truncated; kill the 16m region */
	xorw	%bx, %bx
	ret
	.size patch_1m_16m, . - patch_1m_16m

/****************************************************************************
 * Patch E820 memory map entry
 *
 * Parameters:
 *  %es:di	Pointer to E820 memory map descriptor
 *  %bp		truncate_to_start or truncate_to_end
 * Returns:
 *  %es:di	Pointer to now-modified E820 memory map descriptor
 *  CF set	Region was truncated
 *  CF clear	Region was not truncated
 ****************************************************************************
 */
	.section ".text16"
patch_e820:
	pushal
	movl	%es:0(%di), %eax
	movl	%es:4(%di), %edx
	movl	%es:8(%di), %ebx
	movl	%es:12(%di), %ecx
	call	truncate
	movl	%eax, %es:0(%di)
	movl	%edx, %es:4(%di)
	movl	%ebx, %es:8(%di)
	movl	%ecx, %es:12(%di)
	popal
	ret
	.size patch_e820, . - patch_e820

/****************************************************************************
 * INT 15,e820 handler
 ****************************************************************************
 */
	.section ".text16"
int15_e820:
	pushw	%si
	pushw	%bp
	/* Caller's %bx => %si, real %ebx to %ebx, call previous handler */
	pushfw
	movw	%bx, %si
	testl	%ebx, %ebx
	jnz	1f
	movl	%ebx, %cs:real_ebx
1:	movl	%cs:real_ebx, %ebx
	lcall	*%cs:int15_vector
	pushfw
	/* Edit result */
	pushw	%ds
	pushw	%cs:rm_ds
	popw	%ds
	movw	$truncate_to_start, %bp
	incw	%si
	jns	2f
	movw	$truncate_to_end, %bp
2:	call	patch_e820
	jnc	3f
	xorw	$0x8000, %si
3:	testw	%si, %si
	js	4f
	movl	%ebx, %cs:real_ebx
	testl	%ebx, %ebx
	jz	5f
4:	movw	%si, %bx
5:	popw	%ds
	/* Restore flags returned by previous handler and return */
	popfw
	popw	%bp
	popw	%si
	lret	$2
	.size int15_e820, . - int15_e820

	.section ".text16.data"
real_ebx:
	.long 0
	.size real_ebx, . - real_ebx
	
/****************************************************************************
 * INT 15,e801 handler
 ****************************************************************************
 */
	.section ".text16"
int15_e801:
	/* Call previous handler */
	pushfw
	lcall	*%cs:int15_vector
	pushfw
	/* Edit result */
	pushw	%ds
	pushw	%cs:rm_ds
	popw	%ds
	call	patch_1m_16m
	xchgw	%ax, %cx
	xchgw	%bx, %dx
	call	patch_1m_16m
	xchgw	%ax, %cx
	xchgw	%bx, %dx
	popw	%ds
	/* Restore flags returned by previous handler and return */
	popfw
	lret	$2
	.size int15_e801, . - int15_e801
	
/****************************************************************************
 * INT 15,88 handler
 ****************************************************************************
 */
	.section ".text16"
int15_88:
	/* Call previous handler */
	pushfw
	lcall	*%cs:int15_vector
	pushfw
	/* Edit result */
	pushw	%ds
	pushw	%cs:rm_ds
	popw	%ds
	call	patch_1m
	popw	%ds
	/* Restore flags returned by previous handler and return */
	popfw
	lret	$2
	.size int15_88, . - int15_88
		
/****************************************************************************
 * INT 15 handler
 ****************************************************************************
 */
	.section ".text16"
	.globl int15
int15:
	/* See if we want to intercept this call */
	pushfw
	cmpw	$0xe820, %ax
	jne	1f
	cmpl	$SMAP, %edx
	jne	1f
	popfw
	jmp	int15_e820
1:	cmpw	$0xe801, %ax
	jne	2f
	popfw
	jmp	int15_e801
2:	cmpb	$0x88, %ah
	jne	3f
	popfw
	jmp	int15_88
3:	popfw
	ljmp	*%cs:int15_vector
	.size int15, . - int15
	
	.section ".text16.data"
	.globl int15_vector
int15_vector:
	.long 0
	.size int15_vector, . - int15_vector
