#define PXENV_UNDI_CLEANUP	0x02
#define PXENV_UNDI_SHUTDOWN	0x05
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
 * Unload PXE base code and UNDI driver
 *****************************************************************************
 */
#ifdef PXELOADER_KEEP_ALL
	xorw	%ax, %ax		/* Force zero flag to show success */
	jmp	do_not_free_base_mem	/* Skip the unloading */
#endif /* PXELOADER_KEEP_ALL */
	
unload_pxe:
	movw	$PXENV_UNLOAD_STACK, %bx
	call	pxe_call
	movw	$PXENV_STOP_UNDI, %bx
	call	pxe_call
	pushfw				/* Ignore PXENV_UNDI_CLEANUP errors */
	movw	$PXENV_UNDI_CLEANUP, %bx
	call	pxe_call
	popfw
	/* On exit, zero flag is set iff all calls were successful */
		
/*****************************************************************************
 * Free base memory
 *****************************************************************************
 */
free_base_mem:
	jnz	do_not_free_base_mem	/* Using zero flag from unload_pxe */

	movw	undi_code_segment, %bx
	movw	undi_data_segment, %cx
	movw	undi_code_size, %ax
	cmpw	%bx, %cx
	jb	1f
	movw	%cx, %bx
	movw	undi_data_size, %ax
1:	addw	$0x0f, %ax		/* Round up to next segment */
	shrw	$4, %ax
	addw	%bx, %ax		/* Highest segment address into %ax */
	addw	$(1024 / 16 - 1), %ax	/* Round up to next kb */
	shrw	$6, %ax			/* New free basemem size in %ax */
	movw	%fs:(0x13), %bx		/* Old free base memory in %bx */
	movw	%ax, %fs:(0x13)		/* Store new free base memory size */

	/* Note that zero_mem_loop will also zero out our stack, so make
	 * sure the stack is empty at this point.
	 */
	movw	%ax, %dx
	subw	%bx, %dx		/* numberof kb to zero in %dx */
	shlw	$6, %bx			/* Segment address into %bx */
zero_mem_loop:
	movw	%bx, %es		/* kB boundary into %es:00 */
	xorw	%ax, %ax
	xorw	%di, %di
	movw	$0x400, %cx
	rep	stosb			/* fill kB with zeroes */
	addw	$(1024 / 16), %bx
	decw	%dx
	jnz	zero_mem_loop
	/* Will exit here with zero flag set, so no need to set it explicitly
	 * in order to indicate success.
	 */
	
do_not_free_base_mem:
	pushfw				/* Save success (zero) flag status */
	movw	%fs:(0x13), %ax		/* Free base memory in %ax */
	call	print_hex_word		/* Print free base memory */
	popfw				/* Restore success (zero) flag */

/*****************************************************************************
 * Exit point
 * Jump to finished with the zero flag set to indicate success, or to
 * finished_with_error to always report an error
 *****************************************************************************
 */	
finished:
	movw	$10f, %si
	jz	1f
finished_with_error:
	movw	$20f, %si
1:
	call	print_message
	jmp	run_etherboot
10:	.asciz " ok\n"
20:	.asciz " err\n"

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
 * Make a PXE API call.  Works with either !PXE or PXENV+ API.
 * Opcode in %bx.  pxe_parameter_structure always used.
 * Returns status code (not exit code) in %bx and prints it.
 * ORs status code with overall status code in pxe_overall_status, returns
 * with zero flag set iff all PXE API calls have been successful.
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
