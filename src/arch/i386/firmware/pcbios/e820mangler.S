#undef CODE16
#if defined(PCBIOS)
#define	CODE16
#endif

#ifdef CODE16
	
#define BOCHSBP xchgw %bx,%bx
	
	.text
	.arch i386
	.section ".text16", "ax", @progbits
	.code16

/****************************************************************************
 * Memory map mangling code
 ****************************************************************************
 */

	.globl	e820mangler
e820mangler:

/* Macro to calculate offset of labels within code segment in
 * installed copy of code.
 */
#define INSTALLED(x) ( (x) - e820mangler )
	
/****************************************************************************
 * Intercept INT 15 memory calls and remove the hidden memory ranges
 * from the resulting memory map.
 ****************************************************************************
 */
	.globl	_intercept_int15
_intercept_int15:
	/* Preserve registers */
	pushw	%bp
	/* Store %ax for future reference */
	pushw	%ax
	/* Make INT-style call to old INT15 routine */
	pushfw
	lcall	%cs:*INSTALLED(_intercepted_int15)
	/* Preserve flags returned by original E820 routine */
	pushfw
	/* Check for valid INT15 routine */
	jc	intercept_int15_exit
	/* Check for a routine we want to intercept */
	movw	%sp, %bp
	cmpw	$0xe820, 2(%bp)
	je	intercept_e820
	cmpw	$0xe801, 2(%bp)
	je	intercept_e801
	cmpb	$0x88, 3(%bp)
	je	intercept_88
intercept_int15_exit:
	/* Restore registers and return */
	popfw
	popw	%bp			/* discard original %ax */
	popw	%bp
	lret	$2			/* 'iret' - flags already loaded */

	.globl	_intercepted_int15
_intercepted_int15:	.word 0,0
		
/****************************************************************************
 * Exclude an address range from a potentially overlapping address range
 *
 * Note: this *can* be called even if the range doesn't overlap; it
 * will simply return the range unaltered.  It copes with all the
 * possible cases of overlap, including total overlap (which will
 * modify the range to length zero).  If the to-be-excluded range is
 * in the middle of the target range, then the larger remaining
 * portion will be returned.  If %di is nonzero on entry then the
 * range will only be truncated from the high end, i.e. the base
 * address will never be altered.  All this in less than 30
 * instructions.  :)
 *
 * Parameters:
 *  %eax	Base address of memory range
 *  %ecx	Length of memory range
 *  %ebx	Base address of memory range to exclude
 *  %edx	Length of memory range to exclude
 *  %di		0 => truncate either end, 1 => truncate high end only
 * Returns:
 *  %eax	Updated base address of range
 *  %ecx	Updated length of range
 *  %ebx,%edx	Undefined
 *		All other registers (including %di) preserved
 *
 * Note: "ja" is used rather than "jg" because we are comparing
 * unsigned ints
 ****************************************************************************
 */
#ifdef TEST_EXCLUDE_ALGORITHM
	.code32
#endif /* TEST_EXCLUDE_ALGORITHM */
exclude_memory_range:
	/* Convert (start,length) to (start,end) */
	addl	%eax, %ecx
	addl	%ebx, %edx
	/* Calculate "prefix" length */
	subl	%eax, %ebx		/* %ebx = "prefix" length */
	ja	1f
	xorl	%ebx, %ebx		/* Truncate to zero if negative */
1:	/* %di == 0 => truncate either end
	 * %di != 0 => truncate only high end
	 */
	testw	%di, %di
	je	use_either
	cmpl	%eax, %edx
	jbe	99f			/* excl. range is below target range */
use_prefix:	/* Use prefix, discard suffix */
	addl	%eax, %ebx		/* %ebx = candidate end address */
	cmpl	%ecx, %ebx		/* %ecx = min ( %ebx, %ecx ) */
	ja	1f
	movl	%ebx, %ecx
1:	jmp	99f
use_either:		
	/* Calculate "suffix" length */
	subl	%ecx, %edx		/* %edx = -( "suffix" length ) */
	jb	1f
	xorl	%edx, %edx		/* Truncate to zero if negative */
1:	negl	%edx			/* %edx = "suffix" length */
	/* Use whichever is longest of "prefix" and "suffix" */
	cmpl	%ebx, %edx
	jbe	use_prefix
use_suffix:	/* Use suffix, discard prefix */
	negl	%edx
	addl	%ecx, %edx		/* %edx = candidate start address */
	cmpl	%eax, %edx		/* %eax = max ( %eax, %edx ) */
	jb	1f
	movl	%edx, %eax
1:	
99:	subl	%eax, %ecx		/* Convert back to (start,length) */
	ret

#ifdef TEST_EXCLUDE_ALGORITHM
	.globl	__test_exclude
__test_exclude:
	pushl   %ebx
	pushl	%edi
	movl    12(%esp), %eax
	movl    16(%esp), %ecx
	movl    20(%esp), %ebx
	movl    24(%esp), %edx
	movl	28(%esp), %edi
	call    exclude_memory_range
	shll    $16, %eax
	orl     %ecx, %eax
	popl	%edi
	popl	%ebx
	ret
	.code16
#endif /* TEST_EXCLUDE_ALGORITHM */
	
/****************************************************************************
 * Exclude Etherboot-reserved address ranges from a potentially
 * overlapping address range
 *
 * Parameters:
 *  %eax	Base address of memory range
 *  %ecx	Length of memory range
 *  %di		0 => truncate either end, 1 => truncate high end only
 * Returns:
 *  %eax	Updated base address of range
 *  %ecx	Updated length of range
 *		All other registers (including %di) preserved
 ****************************************************************************
 */
exclude_hidden_memory_ranges:
	pushw	%si
	pushl	%ebx
	pushl	%edx
	movw	$INSTALLED(_hide_memory), %si
2:	movl	%cs:0(%si), %ebx
	movl	%cs:4(%si), %edx
	call	exclude_memory_range
	addw	$8, %si
	cmpw	$INSTALLED(_hide_memory_end), %si
	jl	2b
	popl	%edx
	popl	%ebx
	popw	%si
	ret
	
	.globl	_hide_memory	
_hide_memory:
	.long	0,0			/* Etherboot text (base,length) */
	.long	0,0			/* Heap (base,length) */
_hide_memory_end:

/****************************************************************************
 * Intercept INT 15,E820 calls and remove the hidden memory ranges
 * from the resulting memory map.
 ****************************************************************************
 */
#define SMAP ( 0x534d4150 )
intercept_e820:
	/* Check for valid E820 routine */
	cmpl	$SMAP, %eax
	jne	intercept_int15_exit
	/* If base address isn't in the low 4GB, return unaltered
	 * (since we never claim memory above 4GB).  WARNING: we cheat
	 * by assuming that no E820 region will straddle the 4GB
	 * boundary: if this is not a valid assumption then things
	 * will probably break.
	 */
	cmpl	$0, %es:4(%di)
	jne	intercept_int15_exit
	/* Preserve registers */
	pushl	%eax
	pushl	%ecx
	/* Update returned memory range */
	movl	%es:0(%di), %eax	/* Base */
	movl	%es:8(%di), %ecx	/* Length */
	pushw	%di
	xorw	%di, %di		/* "truncate either end" flag */
	call	exclude_hidden_memory_ranges
	popw	%di
	movl	%eax, %es:0(%di)	/* Store updated base */
	movl	%ecx, %es:8(%di)	/* Store updated length */
	/* Restore registers and return */
	popl	%ecx
	popl	%eax
	jmp	intercept_int15_exit

/****************************************************************************
 * Intercept INT 15,E801 calls and remove the hidden memory ranges
 * from the resulting memory map.
 ****************************************************************************
 */
intercept_e801:
	/* Adjust return values */
	call	e801_adjust
	xchgw	%ax, %cx
	xchgw	%bx, %dx
	call	e801_adjust
	xchgw	%ax, %cx
	xchgw	%bx, %dx
	jmp	intercept_int15_exit
	
	/* %ax = #KB from 1MB+, %bx = #64KB from 16MB+
	 * Return with modified values in %ax, %bx.  Preserver other regs.
	 */
e801_adjust:
	pushw	%di
	pushl	%ecx
	pushl	%eax
	movw	$1, %di			/* "truncate only high end" flag */

	/* Truncate #64KB from 16MB+ as appropriate */
	movw	%bx, %cx		/* (no need to zero high word) */
	shll	$16, %ecx		/* %ecx = length in bytes */
	movl	$(1<<24), %eax		/* 16MB start address */
	call	exclude_hidden_memory_ranges
	shrl	$16, %ecx		/* %cx = updated length in 64KB */
	movw	%cx, %bx		/* Return in %bx */
	
	/* Truncate #KB from 1MB+ as appropriate */
	popw	%cx			/* Orig. %ax (high word already 0) */
	shll	$10, %ecx		/* %ecx = length in bytes */
	shrl	$4, %eax		/* 1MB start address */
	call	exclude_hidden_memory_ranges
	shrl	$10, %ecx		/* %cx = updated length in KB */
	pushw	%cx			/* Will be picked up in %eax */
	
	popl	%eax
	popl	%ecx
	popw	%di
	ret

/****************************************************************************
 * Intercept INT 15,88 calls and remove the hidden memory ranges
 * from the resulting memory map.
 ****************************************************************************
 */
intercept_88:
	pushw	%bx			/* E801 adjust, ignore %bx */
	call	e801_adjust
	popw	%bx
	jmp	intercept_int15_exit

	.globl	e820mangler_end
e820mangler_end:

	.globl	_e820mangler_size
	.equ	_e820mangler_size, e820mangler_end - e820mangler
	.globl	e820mangler_size
e820mangler_size:
	.word	_e820mangler_size

#else

	.globl	_e820mangler_size
	.equ	_e820mangler_size, 0
	
#endif /* CODE16 */
