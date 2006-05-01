
#define CR0_PE 1
	
	
	.arch i386
	.section ".prefix", "awx", @progbits

/****************************************************************************
 * alloc_basemem (real-mode near call)
 *
 * Allocate space from base memory via the BIOS free base memory
 * counter at 40: 13
 *
 * Parameters: 
 *   %cx : Number of bytes to allocate
 * Returns:
 *   %es : Segment address of newly allocated memory
 ****************************************************************************
 */
	.section ".prefix"
	.code16
alloc_basemem:
	/* Preserve registers */
	pushw	%cx
	pushw	%ax
	
	/* %fs = 0x40, %ax = fbms */
	movw	$40, %ax
	movw	%ax, %fs

	/* Round up %cx to nearest kB, subtract from FBMS */
	addw	$0x03ff, %cx
	andw	$0xfc00, %cx
	shrw	$10, %cx
	movw	%fs:0x13, %ax
	subw	%cx, %ax
	movw	%ax, %fs:0x13

	/* Convert to segment address in %es */
	shlw	$6, %ax
	movw	%ax, %es

	/* Restore registers and return */
	popw	%ax
	popw	%cx
	ret


	.section ".prefix"
	.align 16
gdt:
gdt_limit:		.word gdt_length - 1
gdt_base:		.long gdt
			.word 0 /* padding */

cs16: 		/* 16 bit code segment, base at real-mode %cs:0000 */
	.equ	CS16, cs16 - gdt
	.word	0xffff, 0
	.byte	0, 0x9b, 0, 0
	
ss16:		/* 16 bit stack segment, base at real-mode %ss:0000 */
	.equ	SS16, ss16 - gdt
	.word	0xffff, 0
	.byte	0, 0x93, 0, 0

flat_ds:	/* 16 bit data segment, zero base, 4GB limit */
	.equ	FLAT_DS, flat_ds - gdt
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0
	
gdt_end:
	.equ	gdt_length, gdt_end - gdt



	
	.section ".prefix"
	.code16
prot16_call:


	/* Install .data16 to top of base memory */
	movw	%cs, %ax
	addw	$_data16_load_offset_pgh, %ax
	movw	%ax, %ds
	movw	$_data16_size, %cx
	call	alloc_basemem
	xorw	%si, %si
	xorw	%di, %di	
	movw	$_data16_progbits_size, %cx
	rep movsb /* or "call decompress16" */

	/* Install .code16 to top of base memory */
	movw	%cs, %ax
	addw	$_code16_load_offset_pgh, %ax
	movw	%ax, %ds
	movw	$_code16_size, %cx
	call	alloc_basemem
	xorw	%si, %si
	xorw	%di, %di	
	rep movsb /* or "call decompress16" */
	
	/* Push flags and real-mode segment registers */
	pushfl
	push	%gs
	push	%fs
	push	%es
	push	%ds
	push	%ss
	push	%cs

	/* Physical address of %cs:0000 to %ebx, of %ss:0000 to %eax */
	xorl	%ebx, %ebx
	movw	%cs, %bx
	shll	$4, %ebx
	xorl	%eax, %eax
	movw	%ss, %ax
	shll	$4, %eax
	
	/* Set up GDT and switch to protected mode */
	addl	%ebx, %cs:gdt_base
	orl	%ebx, %cs:(cs16+2)
	orl	%eax, %cs:(ss16+2)
	cli
	data32 lgdt	%cs:gdt
	movl	%cr0, %eax
	orb	$CR0_PE, %al
	movl	%eax, %cr0
	data32 ljmp	$CS16, $1f
1:	movw	$SS16, %ax
	movw	%ax, %ss
	movw	$FLAT_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	/* Install .text and .data to 2MB mark.  Use 2MB to avoid
	 * having to deal with A20.
	 */
	leal	_text_load_offset(%ebx), %esi
	movl	$( 2 * 1024 * 1024 ), %edi
	movl	$_text_and_data_progbits_size, %ecx
	addr32 rep movsb /* or "call decompress16" */
	
	/* Restore real-mode segment limits */
	movw	%ss, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	
	/* Return to real mode, restore segment registers and flags */
	pushw	$1f
	movl	%cr0, %eax
	andb	$0!CR0_PE, %al
	movl	%eax, %cr0
	lret	/* used as equivalent of pop %cs */
1:	pop	%ss
	pop	%ds
	pop	%es
	pop	%fs
	pop	%gs
	popfl

	/* Call init_gdt */
	pushw	%cs
	pushw	$1f
	pushw	%es
	pushw	$init_gdt
	lret /* lcall %es:init_gdt */
1:	
	

	
	ret
	

