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
 * Get underlying e820 memory region to underlying_e820 buffer
 *
 * Parameters:
 *   As for INT 15,e820
 * Returns:
 *   As for INT 15,e820
 *
 * Wraps the underlying INT 15,e820 call so that the continuation
 * value (%ebx) is a 16-bit simple sequence counter (with the high 16
 * bits ignored), and termination is always via CF=1 rather than
 * %ebx=0.
 *
 ****************************************************************************
 */
	.section ".text16"
get_underlying_e820:

	/* If the requested region is in the cache, return it */
	cmpw	%bx, underlying_e820_index
	jne	1f
	pushw	%di
	pushw	%si
	movw	$underlying_e820_cache, %si
	movw	$20, %cx
	rep movsb
	popw	%si
	popw	%di
	movw	$20, %cx
	incw	%bx
	movl	%edx, %eax
	ret
1:	
	/* If the requested region is earlier than the cached region,
	 * invalidate the cache.
	 */
	cmpw	%bx, underlying_e820_index
	jbe	1f
	movw	$0xffff, underlying_e820_index
1:
	/* If the cache is invalid, reset the underlying %ebx */
	cmpw	$0xffff, underlying_e820_index
	jne	1f
	andl	$0, underlying_e820_ebx
1:	
	/* If the cache is valid but the continuation value is zero,
	 * this means that the previous underlying call returned with
	 * %ebx=0.  Return with CF=1 in this case.
	 */
	cmpw	$0xffff, underlying_e820_index
	je	1f
	cmpl	$0, underlying_e820_ebx
	jne	1f
	stc
	ret
1:	
	/* Get the next region into the cache */
	pushl	%eax
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	movl	underlying_e820_ebx, %ebx
	stc
	pushfw
	lcall	*%cs:int15_vector
	jc	1f /* CF set: error */
	cmpl	$SMAP, %eax
	je	2f /* 'SMAP' missing: error */
1:	/* An error occurred: return values returned by underlying e820 call */
	stc	/* Force CF set if SMAP was missing */
	leal	16(%esp), %esp /* avoid changing other flags */
	ret
2:	/* No error occurred */
	movl	%ebx, underlying_e820_ebx
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%eax
	/* Copy result to cache */
	pushw	%es
	pushw	%fs
	pushw	%si
	pushw	%di
	pushw	%cx
	pushw	%es
	popw	%fs
	movw	%di, %si
	pushw	%ds
	popw	%es
	movw	$underlying_e820_cache, %di
	movw	$20, %cx
	fs rep movsb
	popw	%cx
	popw	%di
	popw	%si
	popw	%fs
	popw	%es
	/* Mark cache as containing this result */
	incw	underlying_e820_index

	/* Loop until found */
	jmp	get_underlying_e820
	.size	get_underlying_e820, . - get_underlying_e820

	.section ".data16"
underlying_e820_index:
	.word	0xffff /* Initialise to an invalid value */
	.size underlying_e820_index, . - underlying_e820_index

	.section ".bss16"
underlying_e820_ebx:
	.long	0
	.size underlying_e820_ebx, . - underlying_e820_ebx

	.section ".bss16"
underlying_e820_cache:
	.space	20
	.size underlying_e820_cache, . - underlying_e820_cache

/****************************************************************************
 * Get windowed e820 region, without empty region stripping
 *
 * Parameters:
 *   As for INT 15,e820
 * Returns:
 *   As for INT 15,e820
 *
 * Wraps the underlying INT 15,e820 call so that each underlying
 * region is returned N times, windowed to fit within N visible-memory
 * windows.  Termination is always via CF=1.
 *
 ****************************************************************************
 */

	.section ".tbl.data16.memory_windows.00"
	.align 16
memory_windows:

	// Dummy memory window encompassing entire 64-bit address space
	.long 0, 0, 0xffffffff, 0xffffffff

	.section ".tbl.data16.memory_windows.99"
	.align 16
memory_windows_end:
	
	.section ".text16"
get_windowed_e820:

	/* Preserve registers */
	pushl	%esi
	pushw	%bp

	/* Split %ebx into %si:%bx, store original %bx in %bp */
	pushl	%ebx
	xorl	%esi, %esi
	popw	%bp
	popw	%si

	/* %si == 0 => start of memory_windows list */
	testw	%si, %si
	jne	1f
	movw	$memory_windows, %si
1:	
	/* Get (cached) underlying e820 region to buffer */
	call	get_underlying_e820
	jc	99f /* Abort on error */

	/* Preserve registers */
	pushal
	/* start => %edx:%eax, len => %ecx:%ebx */
	movl	%es:0(%di), %eax
	movl	%es:4(%di), %edx
	movl	%es:8(%di), %ebx
	movl	%es:12(%di), %ecx
	/* Convert (start,len) to (start, end) */
	addl	%eax, %ebx
	adcl	%edx, %ecx
	/* Truncate to window start */
	cmpl	4(%esi), %edx
	jne	1f
	cmpl	0(%esi), %eax
1:	jae	2f
	movl	4(%esi), %edx
	movl	0(%esi), %eax
2:	/* Truncate to window end */
	cmpl	12(%esi), %ecx
	jne	1f
	cmpl	8(%esi), %ebx
1:	jbe	2f
	movl	12(%esi), %ecx
	movl	8(%esi), %ebx
2:	/* Convert (start, end) back to (start, len) */
	subl	%eax, %ebx
	sbbl	%edx, %ecx
	/* If length is <0, set length to 0 */
	jae	1f
	xorl	%ebx, %ebx
	xorl	%ecx, %ecx
1:	/* Store modified values in e820 map entry */
	movl	%eax, %es:0(%di)
	movl	%edx, %es:4(%di)
	movl	%ebx, %es:8(%di)
	movl	%ecx, %es:12(%di)
	/* Restore registers */
	popal

	/* Derive continuation value for next call */
	addw	$16, %si
	cmpw	$memory_windows_end, %si
	jne	1f
	/* End of memory windows: reset %si and allow %bx to continue */
	xorw	%si, %si
	jmp	2f
1:	/* More memory windows to go: restore original %bx */
	movw	%bp, %bx
2:	/* Construct %ebx from %si:%bx */
	pushw	%si
	pushw	%bx
	popl	%ebx

98:	/* Clear CF */
	clc
99:	/* Restore registers and return */
	popw	%bp
	popl	%esi
	ret
	.size get_windowed_e820, . - get_windowed_e820

/****************************************************************************
 * Get windowed e820 region, with empty region stripping
 *
 * Parameters:
 *   As for INT 15,e820
 * Returns:
 *   As for INT 15,e820
 *
 * Wraps the underlying INT 15,e820 call so that each underlying
 * region is returned up to N times, windowed to fit within N
 * visible-memory windows.  Empty windows are never returned.
 * Termination is always via CF=1.
 *
 ****************************************************************************
 */
	.section ".text16"
get_nonempty_e820:

	/* Record entry parameters */
	pushl	%eax
	pushl	%ecx
	pushl	%edx

	/* Get next windowed region */
	call	get_windowed_e820
	jc	99f /* abort on error */

	/* If region is non-empty, finish here */
	cmpl	$0, %es:8(%di)
	jne	98f
	cmpl	$0, %es:12(%di)
	jne	98f

	/* Region was empty: restore entry parameters and go to next region */
	popl	%edx
	popl	%ecx
	popl	%eax
	jmp	get_nonempty_e820

98:	/* Clear CF */
	clc
99:	/* Return values from underlying call */
	leal	12(%esp), %esp /* avoid changing flags */
	ret
	.size get_nonempty_e820, . - get_nonempty_e820

/****************************************************************************
 * Get mangled e820 region, with empty region stripping
 *
 * Parameters:
 *   As for INT 15,e820
 * Returns:
 *   As for INT 15,e820
 *
 * Wraps the underlying INT 15,e820 call so that underlying regions
 * are windowed to the allowed memory regions.  Empty regions are
 * stripped from the map.  Termination is always via %ebx=0.
 *
 ****************************************************************************
 */
	.section ".text16"
get_mangled_e820:

	/* Get a nonempty region */
	call	get_nonempty_e820
	jc	99f /* Abort on error */

	/* Peek ahead to see if there are any further nonempty regions */
	pushal
	subw	$20, %sp
	movl	$0xe820, %eax
	movl	$SMAP, %edx
	movl	$20, %ecx
	pushw	%ss
	popw	%es
	movw	%sp, %di
	call	get_nonempty_e820
	leal	20(%esp), %esp /* avoid changing flags */
	popal
	jnc	99f /* There are further nonempty regions */

	/* No futher nonempty regions: zero %ebx and clear CF */
	xorl	%ebx, %ebx
	
99:	/* Return */
	ret
	.size get_mangled_e820, . - get_mangled_e820

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
 * Split E820 memory map entry if necessary
 *
 * Parameters: 
 *   As for INT 15,e820
 * Returns:
 *   As for INT 15,e820
 *
 * Calls the underlying INT 15,e820 and returns a modified memory map.
 * Regions will be split around any hidden regions.
 ****************************************************************************
 */
	.section ".text16"
split_e820:
	pushw	%si
	pushw	%bp
	/* Caller's %bx => %si, real %ebx to %ebx, call previous handler */
	pushfw
	movw	%bx, %si
	testl	%ebx, %ebx
	jnz	1f
	movl	%ebx, %cs:real_ebx
1:	movl	%cs:real_ebx, %ebx

//	lcall	*%cs:int15_vector
	/* Hacked in call to get_mangled_e820 in place of underlying INT15 */
	popfw
	pushw	%ds
	pushw	%cs:rm_ds
	popw	%ds
	call	get_mangled_e820
	popw	%ds

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
	ret
	.size split_e820, . - split_e820

	.section ".text16.data"
real_ebx:
	.long 0
	.size real_ebx, . - real_ebx

/****************************************************************************
 * INT 15,e820 handler
 ****************************************************************************
 */
	.section ".text16"
int15_e820:
	pushl	%eax
	pushl	%ecx
	pushl	%edx	
	call	split_e820
	pushfw
	/* If we've hit an error, exit immediately */
	jc	99f
	/* If region is non-empty, return this region */
	pushl	%eax
	movl	%es:8(%di), %eax
	orl	%es:12(%di), %eax
	popl	%eax
	jnz	99f
	/* Region is empty.  If this is not the end of the map,
	 * skip over this region.
	 */
	testl	%ebx, %ebx
	jz	1f
	popfw
	popl	%edx
	popl	%ecx
	popl	%eax
	jmp	int15_e820
1:	/* Region is empty and this is the end of the map.  Return
	 * with CF set to avoid placing an empty region at the end of
	 * the map.
	 */
	popfw
	stc
	pushfw
99:	/* Restore flags from original INT 15,e820 call and return */
	popfw
	addr32 leal	12(%esp), %esp /* avoid changing flags */
	lret	$2
	.size int15_e820, . - int15_e820
	
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
