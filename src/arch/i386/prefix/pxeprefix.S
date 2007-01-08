#define	PXENV_STOP_UNDI		0x15
#define PXENV_UNLOAD_STACK	0x70
#define PXENV_STOP_BASE		0x76

#define PXE_STACK_MAGIC		0x57ac	/* 'STac' */

	.text
	.code16
	.arch i386
	.org 0
	.section ".prefix", "ax", @progbits
/*****************************************************************************
 * Entry point:	set cs, ds, bp, print welcome message
 *****************************************************************************
 */	
	jmp	$0x7c0, $code_start
10:	.asciz	"PXE->EB "
code_start:
	/* Preserve registers for return to PXE stack */
	pushfl
	pushal
	pushw	%gs
	pushw	%fs
	pushw	%es
	pushw	%ds
	pushw	%ss
	pushw	%cs
	pushw	$PXE_STACK_MAGIC	/* PXE stack magic marker */
	/* Set up stack just below 0x7c00 */
	pushw	%ss
	popw	%es
	movw	%sp, %di
	xorw	%ax, %ax
	movw	%ax, %ss
	movw	$0x7c00, %sp
	pushw	%es			/* Save old PXE stack pointer */
	pushw	%di
	/* Set up our other segment registers */
	pushw	%cs
	popw	%ds
	movw	$0x40, %ax		/* BIOS data segment access */
	movw	%ax, %fs
	/* Print welcome message */
	movw	$10b, %si
	call	print_message

/*****************************************************************************
 * Detect type of PXE available (!PXE, PXENV+ or none)
 *****************************************************************************
 */
detect_pxe:
	les	%es:54(%di), %di /* !PXE structure */
	cmpl	$0x45585021, %es:(%di)	/* '!PXE' signature */
	je	detected_pxe
	movw	$0x5650, %ax
	int	$0x1a
	cmpw	$0x564e, %ax
	jne	detected_nothing
	cmpl	$0x4e455850, %es:(%bx)	/* 'PXEN' signature */
	jne	detected_nothing
	cmpw	$0x2b56, %es:4(%bx)	/* 'V+' signature */
	je	detected_pxenv

detected_nothing:
	movw	$10f, %si
	call	print_message
	jmp	finished_with_error
10:	.asciz	"No PXE "

detected_pxenv: /* es:bx points to PXENV+ structure */
	pushw	%es
	pushw	%bx
	pushw	%es:0x24(%bx)		/* UNDI code segment */
	pushw	%es:0x26(%bx)		/* UNDI code size */
	pushw	%es:0x20(%bx)		/* UNDI data segment */
	pushw	%es:0x22(%bx)		/* UNDI data size */
	les	%es:0x0a(%bx), %di	/* Entry point to %es:%di */
	movw	$10f, %si
	jmp	pxe_setup_done
10:	.asciz	"PXENV+ "

detected_pxe:	/* es:di points to !PXE structure */
	pushw	%es
	pushw	%di
	pushw	%es:0x30(%di)		/* UNDI code segment */
	pushw	%es:0x36(%di)		/* UNDI code size */
	pushw	%es:0x28(%di)		/* UNDI data segment */
	pushw	%es:0x2e(%di)		/* UNDI data size */
	les	%es:0x10(%di), %di	/* Entry point to %es:%di */
	movw	$10f, %si
	jmp	pxe_setup_done
10:	.asciz	"!PXE "

pxe_setup_done:
	movw	%es, pxe_entry_segment
	movw	%di, pxe_entry_offset
	popw	undi_data_size
	popw	undi_data_segment
	popw	undi_code_size
	popw	undi_code_segment
	call	print_message
	popw	%di
	popw	%es	/* Exit with %es:%di containing structure address */

/*****************************************************************************
 * Print information about located structure
 *****************************************************************************
 */
print_structure_information:
	call	print_segoff	/* %es:%di contains address of structure */
	les	pxe_entry_segoff, %di
	call	print_segoff
	les	undi_code_segoff, %di
	call	print_segoff
	les	undi_data_segoff, %di
	call	print_segoff

/*****************************************************************************
 * Calculate base memory usage by UNDI
 *****************************************************************************
 */
find_undi_basemem_usage:
	movw	undi_code_segment, %ax
	movw	undi_code_size, %bx
	movw	undi_data_segment, %cx
	movw	undi_data_size, %dx
	cmpw	%ax, %cx
	ja	1f
	xchgw	%ax, %cx
	xchgw	%bx, %dx
1:	/* %ax:%bx now describes the lower region, %cx:%dx the higher */
	shrw	$6, %ax			/* Round down to nearest kB */
	movw	%ax, undi_fbms_start
	addw	$0x0f, %dx		/* Round up to next segment */
	shrw	$4, %dx
	addw	%dx, %cx
	addw	$((1024 / 16) - 1), %cx	/* Round up to next kB */
	shrw	$6, %cx
	movw	%cx, undi_fbms_end

/*****************************************************************************
 * Unload PXE base code
 *****************************************************************************
 */	
unload_base_code:
	movw	$PXENV_STOP_BASE, %bx
	call	pxe_call
	movw	$PXENV_UNLOAD_STACK, %bx
	call	pxe_call
	jnz	do_not_free_base_code
free_base_code:
	movw	%fs:(0x13), %si
	movw	undi_fbms_start, %di
	call	free_basemem
do_not_free_base_code:

/*****************************************************************************
 * Unload UNDI driver
 *****************************************************************************
 */
unload_undi:
	movw	$PXENV_STOP_UNDI, %bx
	call	pxe_call
#ifndef PXELOADER_KEEP_UNDI
	jnz	do_not_free_undi
free_undi:
	movw	undi_fbms_start, %si
	movw	undi_fbms_end, %di
	call	free_basemem
do_not_free_undi:
#endif /* PXELOADER_KEEP_UNDI */

/*****************************************************************************
 * Exit point
 *****************************************************************************
 */	
finished:
	movw	$10f, %si
	movw	pxe_overall_status, %ax
	testw	%ax, %ax
	jz	1f
finished_with_error:
	movw	$20f, %si
1:
	call	print_message
	jmp	run_etherboot
10:	.asciz "ok\n"
20:	.asciz "err\n"

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
	movw	$4, %cx
1:
	pushw	%ax
	shrw	$12, %ax
	/* Courtesy of Norbert Juffa <norbert.juffa@amd.com> */
	cmpb	$10, %al	
	sbbb	$0x69, %al
	das
	call	print_character
	popw	%ax
	shlw	$4, %ax
	loop	1b
	ret
	
/*****************************************************************************
 * Subroutine: print segment:offset address in %es:%di
 *****************************************************************************
 */
print_segoff:
	pushw	%di
	pushw	%es
	popw	%ax
	call	print_hex_word
	movb	$0x3a,%al			/* ':' */
	call	print_character
	popw	%ax
	call	print_hex_word
	movb	$0x20, %al			/* ' ' */
	call	print_character
	ret

/*****************************************************************************
 * Subroutine: free and zero base memory from %si kB to %di kB
 *****************************************************************************
 */
free_basemem:
	movw	%fs:(0x13), %ax		/* Current FBMS to %ax */
	cmpw	%ax, %si		/* Update FBMS only if "old" value  */
	jne	1f			/* is correct			    */
	movw	%di, %fs:(0x13)
1:	movw	%di, %bx
zero_kb:
	movw	%si, %ax		/* Zero kB at %si */
	shlw	$6, %ax
	movw	%ax, %es
	xorw	%ax, %ax
	xorw	%di, %di
	movw	$0x400, %cx
	rep stosb
	incw	%si			/* Move to next kB */
	cmpw	%si, %bx
	jne	zero_kb			/* Loop until done */
	movw	%fs:(0x13), %ax		/* Print free base memory */
	call	print_hex_word
	movb	$0x20, %al			/* ' ' */
	call	print_character
	ret

/*****************************************************************************
 * Make a PXE API call.  Works with either !PXE or PXENV+ API.
 * Opcode in %bx.  pxe_parameter_structure always used.
 *
 * Returns status code (not exit code) in %bx and prints it.  Returns
 * with zero flag set if status code is zero (PXENV_STATUS_SUCCESS).
 *****************************************************************************
 */
pxe_call:
	/* Set up registers for PXENV+ API.  %bx already set up */
	pushw	%ds
	popw	%es
	movw	$pxe_parameter_structure, %di
	/* Set up stack for !PXE API */
	pushw   %cs
	pushw	%di
	pushw	%bx
	/* Make the API call */
	lcall	*pxe_entry_segoff
	/* Reset the stack */
	addw	$6, %sp
	movw	pxe_parameter_structure, %ax
	pushw	%ax
	call	print_hex_word
	movw	$0x20, %ax		/* ' ' */
	call	print_character
	popw	%bx
	orw	%bx, pxe_overall_status
	testw	%bx, %bx
	ret

/*****************************************************************************
 * PXE data structures
 *****************************************************************************
 */

pxe_entry_segoff:
pxe_entry_offset:	.word 0
pxe_entry_segment:	.word 0

undi_code_segoff:
undi_code_size:		.word 0
undi_code_segment:	.word 0

undi_data_segoff:
undi_data_size:		.word 0
undi_data_segment:	.word 0

undi_fbms_start:	.word 0
undi_fbms_end:		.word 0

pxe_parameter_structure:
	.word	0
	.word	0,0,0,0,0

pxe_overall_status:	.word 0

/*****************************************************************************
 * Run Etherboot main code
 *****************************************************************************
 */	
run_etherboot:
	/* Install Etherboot */
	call	install

	/* Jump to .text16 segment with %ds pointing to .data16*/
	movw	%bx, %ds
	pushw	%ax
	pushw	$1f
	lret
	.section ".text16", "ax", @progbits
1:
	/* Original PXE stack pointer to es:di.  We must hold it in
	 * registers, because our current stack may be vapourised by
	 * the time main() returns.  (main() will still be able to
	 * return, because prot_call() transfers the return address to
	 * the internal stack and back again).
	 */
	popw	%di
	popw	%es

	/* Run main program */
	pushl	$main
	pushw	%cs
	call	prot_call
	popl	%eax /* discard */

	/* If original PXE stack is intact, return via PXE, else via INT 18 */
	cmpw	$PXE_STACK_MAGIC, %es:0(%di)
	jne	exit_via_int18
exit_via_pxe:				/* Stack OK, return to PXE */
	movw	$exit_via_pxe_message, %si
	call	print_exit_message
	pushw	%es			/* Restore original PXE stack */
	popw	%ss
	movw	%di, %sp
	popw	%ax /* discard PXE_STACK_MAGIC */
	popw	%ax /* discard %cs */
	popw	%ax /* discard %ss */
	popw	%ds
	popw	%es
	popw	%fs
	popw	%gs
	popal
	popfl
	xorw	%ax, %ax		/* Return PXENV_STATUS_SUCCESS */
	lret
exit_via_int18:				/* Stack damaged, do int 18 */
	movw	$exit_via_int18_message, %si
	call	print_exit_message
	int	$0x18

print_exit_message:	
	movw	$0x0007, %bx		/* page 0, attribute 7 (normal) */
	movb	$0x0e, %ah		/* write char, tty mode */
1: 	lodsb
	testb	%al, %al
	je	2f
	int	$0x10
	jmp	1b
2:	ret

	.section ".data16", "aw", @progbits
exit_via_pxe_message:
	.asciz	"EB->PXE\r\n"
exit_via_int18_message:
	.asciz	"EB->BIOS\r\n"
