/* Offsets of words containing ROM's CS and size (in 512 byte blocks)
 * from start of floppy boot block at 0x7c00
 * Offsets must match those in etherboot.h
 */
#define FLOPPY_SEGMENT	0x7c0

#define PXENV_UNDI_CLEANUP	0x02
#define PXENV_UNDI_SHUTDOWN	0x05
#define	PXENV_STOP_UNDI		0x15
#define PXENV_UNLOAD_STACK	0x70
#define PXENV_STOP_BASE		0x76

#define PUSHA_SIZE		16
#define PXE_STACK_MAGIC		0x57ac	/* 'STac' */

	.text
	.code16
	.arch i386
	.org 0
	.section ".prefix", "ax", @progbits
	.globl _prefix
/*****************************************************************************
 * Entry point:	set cs, ds, bp, print welcome message
 *****************************************************************************
 */	
_prefix:
	jmp	$FLOPPY_SEGMENT, $code_start-_prefix
10:	.asciz	"PXE->EB "
code_start:
	pusha				/* Preserve all registers */
	push	%ds
	movw	%sp, %bp		/* %bp must be preserved, hence do
					 * this after the pusha */
	push	$PXE_STACK_MAGIC	/* PXE stack magic marker */

	push	%cs			/* Set up data segment */
	pop	%ds
	mov	$0x40, %cx		/* Set up %fs for access to 40:13 */
	mov	%cx, %fs
	movw	$10b-_prefix, %si	/* Print welcome message */
	call	print_message

/*****************************************************************************
 * Detect type of PXE available (!PXE, PXENV+ or none)
 *****************************************************************************
 */
detect_pxe:
	les	4+PUSHA_SIZE+2(%bp), %di /* !PXE structure */
	cmpl	$0x45585021, %es:(%di)	/* '!PXE' signature */
	je	detected_pxe
	mov	$0x5650, %ax
	int	$0x1a
	cmp	$0x564e, %ax
	jne	detected_nothing
	cmpl	$0x4e455850, %es:(%bx)	/* 'PXEN' signature */
	jne	detected_nothing
	cmpw	$0x2b56, %es:4(%bx)	/* 'V+' signature */
	je	detected_pxenv

detected_nothing:
	movw	$10f-_prefix, %si
	call	print_message
	jmp	finished_with_error
10:	.asciz	"No PXE "

detected_pxenv: /* es:bx points to PXENV+ structure */
	push	%es
	push	%bx
	push	%es:0x24(%bx)		/* UNDI code segment */
	push	%es:0x26(%bx)		/* UNDI code size */
	push	%es:0x20(%bx)		/* UNDI data segment */
	push	%es:0x22(%bx)		/* UNDI data size */
	les	%es:0x0a(%bx), %di	/* Entry point to %es:%di */
	movw	$10f-_prefix, %si
	jmp	pxe_setup_done
10:	.asciz	"PXENV+ "

detected_pxe:	/* es:di points to !PXE structure */
	push	%es
	push	%di
	push	%es:0x30(%di)		/* UNDI code segment */
	push	%es:0x36(%di)		/* UNDI code size */
	push	%es:0x28(%di)		/* UNDI data segment */
	push	%es:0x2e(%di)		/* UNDI data size */
	les	%es:0x10(%di), %di	/* Entry point to %es:%di */
	movw	$10f-_prefix, %si
	jmp	pxe_setup_done
10:	.asciz	"!PXE "

pxe_setup_done:
	mov	%es, pxe_entry_segment - _prefix
	mov	%di, pxe_entry_offset - _prefix
	pop	%ax
	mov	%ax, undi_data_size - _prefix
	pop	%ax
	mov	%ax, undi_data_segment - _prefix
	pop	%ax
	mov	%ax, undi_code_size - _prefix
	pop	%ax
	mov	%ax, undi_code_segment - _prefix
	call	print_message
	pop	%di
	pop	%es	/* Exit with %es:%di containing structure address */

/*****************************************************************************
 * Print information about located structure
 *****************************************************************************
 */
print_structure_information:
	call	print_segoff	/* %es:%di contains address of structure */
	les	%ds:(pxe_entry_segoff - _prefix), %di
	call	print_segoff
	les	%ds:(undi_code_segoff - _prefix), %di
	call	print_segoff
	les	%ds:(undi_data_segoff - _prefix), %di
	call	print_segoff

/*****************************************************************************
 * Unload PXE base code and UNDI driver
 *****************************************************************************
 */
#ifdef PXELOADER_KEEP_ALL
	xor	%ax, %ax		/* Force zero flag to show success */
	jmp	do_not_free_base_mem	/* Skip the unloading */
#endif /* PXELOADER_KEEP_ALL */
	
unload_pxe:
	mov	$PXENV_UNLOAD_STACK, %bx
	call	pxe_call
	mov	$PXENV_STOP_UNDI, %bx
	call	pxe_call
	pushfw				/* Ignore PXENV_UNDI_CLEANUP errors */
	mov	$PXENV_UNDI_CLEANUP, %bx
	call	pxe_call
	popfw
	/* On exit, zero flag is set iff all calls were successful */
		
/*****************************************************************************
 * Free base memory
 *****************************************************************************
 */
free_base_mem:
	jnz	do_not_free_base_mem	/* Using zero flag from unload_pxe */

	mov	undi_code_segment - _prefix, %bx
	mov	undi_data_segment - _prefix, %cx
	mov	undi_code_size - _prefix, %ax
	cmp	%bx, %cx
	jb	1f
	mov	%cx, %bx
	mov	undi_data_size - _prefix, %ax
1:	add	$0x0f, %ax		/* Round up to next segment */
	shr	$4, %ax
	add	%bx, %ax		/* Highest segment address into %ax */
	add	$(1024 / 16 - 1), %ax	/* Round up to next kb */
	shr	$6, %ax			/* New free basemem size in %ax */
	mov	%fs:(0x13), %bx		/* Old free base memory in %bx */
	mov	%ax, %fs:(0x13)		/* Store new free base memory size */

	/* Note that zero_mem_loop will also zero out our stack, so make
	 * sure the stack is empty at this point.
	 */
	mov	%ax, %dx
	sub	%bx, %dx		/* numberof kb to zero in %dx */
	shl	$6, %bx			/* Segment address into %bx */
zero_mem_loop:
	mov	%bx, %es		/* kB boundary into %es:00 */
	xor	%ax, %ax
	xor	%di, %di
	mov	$0x400, %cx
	rep	stosb			/* fill kB with zeroes */
	add	$(1024 / 16), %bx
	dec	%dx
	jnz	zero_mem_loop
	/* Will exit here with zero flag set, so no need to set it explicitly
	 * in order to indicate success.
	 */
	
do_not_free_base_mem:
	pushf				/* Save success (zero) flag status */
	mov	%fs:(0x13), %ax		/* Free base memory in %ax */
	call	print_hex_word		/* Print free base memory */
	popf				/* Restore success (zero) flag */

/*****************************************************************************
 * Exit point
 * Jump to finished with the zero flag set to indicate success, or to
 * finished_with_error to always report an error
 *****************************************************************************
 */	
finished:
	movw	$10f-_prefix, %si
	jz	1f
finished_with_error:
	movw	$20f-_prefix, %si
1:
	call	print_message
	jmp	99f
10:	.asciz " ok\n"
20:	.asciz " err\n"

	
	/* We place a stack here.  It doesn't get used until after all
	 * the above code is finished, so we can happily write all
	 * over it.  Putting the stack here ensures that it doesn't
	 * accidentally go over the 512 byte threshold, which would
	 * cause problems when returning via start32's prefix
	 * relocation mechanism.
	 */
_estack:	
99:

/*****************************************************************************
 * Run Etherboot main code
 *****************************************************************************
 */	
run_etherboot:
	/* Very temporarily switch stacks to one internal to the
	 * prefix.  Do this because the stack provided by the PXE ROM
	 * could be absolutely anywhere, including in an area of
	 * memory that the call to prelocate will vapourise...
	 */
	pushw	%ss			/* PXE stack pointer to ES:DI */
	popw	%es
	movw	%sp, %di
	pushw	%ds			/* Set up stack in "safe" area */
	popw	%ss
	movw	$_estack-_prefix, %sp
	pushw	%es			/* Record PXE stack pointer */
	pushw	%di
	/* Relocate payload and stack to claimed base memory */
	pushw	$4			/* Preserve old PXE stack pointer */
	call	prelocate
	popw	%ax			/* Remove parameter */
	pushl	$4			/* Preserve old PXE stack pointer */
	pushw	$0			/* Indicate prefix exit mechanism */
	jmp	_start			/* Run Etherboot */

	.section ".text16", "ax", @progbits
	.globl	prefix_exit
prefix_exit:
	pushw	%cs			/* Set %ds, %bp for access to text */
	popw	%ds
	call	1f
1:	popw	%bp
	popw	%di			/* Old PXE stack to %es:di */
	popw	%es
	cmpw	$PXE_STACK_MAGIC, %es:0(%di)  /* See if PXE stack intact */
	jne	exit_via_int18
exit_via_pxe:				/* Stack OK, return to PXE */
	push	%es			/* Restore PXE stack pointer */
	pop	%ss
	mov	%di, %sp
	pop	%ax			/* Discard PXE_STACK_MAGIC marker */
	leaw	(10f-1b)(%bp), %si
	call	print_exit_message
	pop	%ds			/* Restore PXE's DS */
	popa				/* Restore PXE's other registers */
	movw	$0, %ax			/* Return PXENV_STATUS_SUCCESS */
	lret				/* Return control to PXE ROM */
10:	.asciz	"EB->PXE\r\n"
exit_via_int18:				/* Stack damaged, do int 18 */
	leaw	(10f-1b)(%bp), %si
	call	print_exit_message
	int	$0x18
10:	.asciz	"EB->BIOS\r\n"
print_exit_message:	
	movw	$0x0007, %bx		/* page 0, attribute 7 (normal) */
	movb	$0x0e, %ah		/* write char, tty mode */
1: 	lodsb
	testb	%al, %al
	je	2f
	int	$0x10
	jmp	1b
2:	ret
	.globl	prefix_exit_end
prefix_exit_end:
	.previous

/*****************************************************************************
 * Subroutine: print character in %al (with LF -> LF,CR translation)
 *****************************************************************************
 */
print_character:
	movw	$0x0007, %bx		/* page 0, attribute 7 (normal) */
	movb	$0x0e, %ah		/* write char, tty mode */
	cmpb	$0x0a, %al		/* '\n'? */
	jne	1f
	int	$0x10
	movb	$0x0d, %al
1:	int	$0x10
	ret
	
/*****************************************************************************
 * Subroutine: print a zero-terminated message starting at %si
 *****************************************************************************
 */	
print_message:
1: 	lodsb
	testb	%al, %al
	je	2f
	call	print_character
	jmp	1b
2:	ret

/*****************************************************************************
 * Subroutine: print hex word in %ax
 *****************************************************************************
 */
print_hex_word:
	mov	$4, %cx
1:
	push	%ax
	shr	$12, %ax
	/* Courtesy of Norbert Juffa <norbert.juffa@amd.com> */
	cmp	$10, %al	
	sbb	$0x69, %al
	das
	call	print_character
	pop	%ax
	shl	$4, %ax
	loop	1b
	ret
	
/*****************************************************************************
 * Subroutine: print segment:offset address in %es:%di
 *****************************************************************************
 */
print_segoff:
	push	%di
	push	%es
	pop	%ax
	call	print_hex_word
	movb	$0x3a,%al			/* ':' */
	call	print_character
	pop	%ax
	call	print_hex_word
	mov	$0x20, %al			/* ' ' */
	call	print_character
	ret
	
/*****************************************************************************
 * Make a PXE API call.  Works with either !PXE or PXENV+ API.
 * Opcode in %bx.  pxe_parameter_structure always used.
 * Returns status code (not exit code) in %bx and prints it.
 * ORs status code with overall status code in pxe_overall_status, returns
 * with zero flag set iff all PXE API calls have been successful.
 *****************************************************************************
 */
pxe_call:
	/* Set up registers for PXENV+ API.  %bx already set up */
	push	%ds
	pop	%es
	mov	$pxe_parameter_structure - _prefix, %di
	/* Set up stack for !PXE API */
	pushw   %cs
	pushw	%di
	pushw	%bx
	/* Make the API call */
	lcall	*(pxe_entry_segoff - _prefix)
	/* Reset the stack */
	add	$6, %sp
	mov	pxe_parameter_structure - _prefix, %ax
	push	%ax
	call	print_hex_word
	mov	$0x20, %ax		/* ' ' */
	call	print_character
	pop	%bx
	or	%bx, pxe_overall_status - _prefix
	ret

/*****************************************************************************
 * PXE data structures
 *****************************************************************************
 */

pxe_overall_status:	.word 0

pxe_entry_segoff:
pxe_entry_offset:	.word 0
pxe_entry_segment:	.word 0

undi_code_segoff:
undi_code_size:		.word 0
undi_code_segment:	.word 0

undi_data_segoff:
undi_data_size:		.word 0
undi_data_segment:	.word 0

pxe_parameter_structure:
	.word	0
	.word	0,0,0,0,0

end_of_pxeloader:

	.balign 16, 0
payload:
