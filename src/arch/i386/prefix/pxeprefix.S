#define PXENV_UNDI_SHUTDOWN		0x0005
#define	PXENV_UNDI_GET_NIC_TYPE		0x0012
#define	PXENV_STOP_UNDI			0x0015
#define PXENV_UNLOAD_STACK		0x0070

	.text
	.arch i386
	.org 0
	.code16

#include <undi.h>

#define STACK_MAGIC ( 'L' + ( 'R' << 8 ) + ( 'E' << 16 ) + ( 'T' << 24 ) )

/*****************************************************************************
 * Entry point:	set operating context, print welcome message
 *****************************************************************************
 */
	.section ".prefix", "ax", @progbits
	jmp	$0x7c0, $1f
1:
	/* Preserve registers for possible return to PXE */
	pushfl
	pushal
	pushw	%gs
	pushw	%fs
	pushw	%es
	pushw	%ds

	/* Store magic word on PXE stack and remember PXE %ss:esp */
	pushl	$STACK_MAGIC
	movw	%ss, %cs:pxe_ss
	movl	%esp, %cs:pxe_esp
	movw	%sp, %bp
	movl	(10*4+4*2+4)(%bp),%ebp	/* !PXE address */

	/* Set up %ds */
	movw	%cs, %ax
	movw	%ax, %ds
	/* Record PXENV+ and !PXE nominal addresses */
	movw	%es, pxenv_segment	/* PXENV+ address */
	movw	%bx, pxenv_offset
	movl	%ebp, ppxe_segoff	/* !PXE address */
	/* Set up %es and %fs */
	movw	%ax, %es
	movw	$0x40, %ax		/* BIOS data segment access */
	movw	%ax, %fs
	/* Set up stack just below 0x7c00 */
	xorw	%ax, %ax
	movw	%ax, %ss
	movl	$0x7c00, %esp
	/* Clear direction flag, for the sake of sanity */
	cld
	/* Print welcome message */
	movw	$10f, %si
	xorw	%di, %di
	call	print_message
	.section ".prefix.data", "aw", @progbits
10:	.asciz	"PXE->EB:"
	.previous

/*****************************************************************************
 * Verify PXENV+ structure and record parameters of interest
 *****************************************************************************
 */
detect_pxenv:
	/* Signature check */
	les	pxenv_segoff, %bx
	cmpl	$0x4e455850, %es:(%bx)	/* 'PXEN' signature */
	jne	no_pxenv
	cmpw	$0x2b56, %es:4(%bx)	/* 'V+' signature */
	jne	no_pxenv
	/* Record entry point and UNDI segments */
	pushl	%es:0x0a(%bx)		/* Entry point */
	popl	entry_segoff
	pushw	%es:0x24(%bx)		/* UNDI code segment */
	pushw	%es:0x26(%bx)		/* UNDI code size */
	popl	undi_code_segoff
	pushw	%es:0x20(%bx)		/* UNDI data segment */
	pushw	%es:0x22(%bx)		/* UNDI data size */
	popl	undi_data_segoff
	/* Print "PXENV+ at <address>" */
	movw	$10f, %si
	call	print_message
	call	print_segoff
	movb	$( ',' ), %al
	call	print_character
	jmp	99f
	.section ".prefix.data", "aw", @progbits
10:	.asciz	" PXENV+ at "
	.previous

no_pxenv:
	xorl	%eax, %eax
	movl	%eax, pxenv_segoff
	
99:
	
/*****************************************************************************
 * Verify !PXE structure and record parameters of interest
 *****************************************************************************
 */
detect_ppxe:
	/* Signature check */
	les	ppxe_segoff, %bx
	cmpl	$0x45585021, %es:(%bx)	/* '!PXE' signature */
	jne	no_ppxe
	/* Record structure address, entry point, and UNDI segments */
	pushw	%es
	popw	ppxe_segment
	movw	%bx, ppxe_offset
	pushl	%es:0x10(%bx)		/* Entry point */
	popl	entry_segoff
	pushw	%es:0x30(%bx)		/* UNDI code segment */
	pushw	%es:0x36(%bx)		/* UNDI code size */
	popl	undi_code_segoff
	pushw	%es:0x28(%bx)		/* UNDI data segment */
	pushw	%es:0x2e(%bx)		/* UNDI data size */
	popl	undi_data_segoff
	/* Print "!PXE at <address>" */
	movw	$10f, %si
	call	print_message
	call	print_segoff
	movb	$( ',' ), %al
	call	print_character
	jmp	99f
	.section ".prefix.data", "aw", @progbits
10:	.asciz	" !PXE at "
	.previous

no_ppxe:
	xorl	%eax, %eax
	movl	%eax, ppxe_segoff

99:

/*****************************************************************************
 * Sanity check: we must have an entry point
 *****************************************************************************
 */
check_have_stack:
	/* Check for entry point */
	movl	entry_segoff, %eax
	testl	%eax, %eax
	jnz	99f
	/* No entry point: print message and skip everything else */
	movw	$10f, %si
	call	print_message
	jmp	finished
	.section ".prefix.data", "aw", @progbits
10:	.asciz	" No PXE stack found!\n"
	.previous
99:	

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
 * Print information about detected PXE stack
 *****************************************************************************
 */
print_structure_information:
	/* Print entry point */
	movw	$10f, %si
	call	print_message
	les	entry_segoff, %bx
	call	print_segoff
	.section ".prefix.data", "aw", @progbits
10:	.asciz	" entry point at "
	.previous
	/* Print UNDI code segment */
	movw	$10f, %si
	call	print_message
	les	undi_code_segoff, %bx
	call	print_segoff
	.section ".prefix.data", "aw", @progbits
10:	.asciz	"\n         UNDI code segment "
	.previous
	/* Print UNDI data segment */
	movw	$10f, %si
	call	print_message
	les	undi_data_segoff, %bx
	call	print_segoff
	.section ".prefix.data", "aw", @progbits
10:	.asciz	", data segment "
	.previous
	/* Print UNDI memory usage */
	movw	$10f, %si
	call	print_message
	movw	undi_fbms_start, %ax
	call	print_word
	movb	$( '-' ), %al
	call	print_character
	movw	undi_fbms_end, %ax
	call	print_word
	movw	$20f, %si
	call	print_message
	.section ".prefix.data", "aw", @progbits
10:	.asciz	" ("
20:	.asciz	"kB)\n"
	.previous

/*****************************************************************************
 * Determine physical device
 *****************************************************************************
 */
get_physical_device:
	/* Issue PXENV_UNDI_GET_NIC_TYPE */
	movw	$PXENV_UNDI_GET_NIC_TYPE, %bx
	call	pxe_call
	jnc	1f
	call	print_pxe_error
	jmp	no_physical_device
1:	/* Determine physical device type */
	movb	( pxe_parameter_structure + 0x02 ), %al
	cmpb	$2, %al
	je	pci_physical_device
	jmp	no_physical_device

pci_physical_device:
	/* Record PCI bus:dev.fn and vendor/device IDs */
	movl	( pxe_parameter_structure + 0x03 ), %eax
	movl	%eax, pci_vendor
	movw	( pxe_parameter_structure + 0x0b ), %ax
	movw	%ax, pci_busdevfn
	movw	$10f, %si
	call	print_message
	call	print_pci_busdevfn
	movb	$0x0a, %al
	call	print_character
	jmp	99f
	.section ".prefix.data", "aw", @progbits
10:	.asciz	"         UNDI device is PCI "
	.previous

no_physical_device:
	/* No device found, or device type not understood */
	movw	$10f, %si
	call	print_message
	.section ".prefix.data", "aw", @progbits
10:	.asciz	"         Unable to determine UNDI physical device\n"
	.previous

99:

/*****************************************************************************
 * Leave NIC in a safe state
 *****************************************************************************
 */
#ifndef PXELOADER_KEEP_PXE
shutdown_nic:
	/* Issue PXENV_UNDI_SHUTDOWN */
	movw	$PXENV_UNDI_SHUTDOWN, %bx
	call	pxe_call
	jnc	1f
	call	print_pxe_error
1:
unload_base_code:
	/* Issue PXENV_UNLOAD_STACK */
	movw	$PXENV_UNLOAD_STACK, %bx
	call	pxe_call
	jnc	1f
	call	print_pxe_error
	jmp	99f
1:	/* Free base memory used by PXE base code */
	movw	undi_fbms_start, %ax
	movw	%fs:(0x13), %bx
	call	free_basemem
99:
	andw	$~( UNDI_FL_INITIALIZED | UNDI_FL_KEEP_ALL ), flags
#endif /* PXELOADER_KEEP_PXE */

/*****************************************************************************
 * Unload UNDI driver
 *****************************************************************************
 */
#ifndef PXELOADER_KEEP_UNDI
unload_undi:
	/* Issue PXENV_STOP_UNDI */
	movw	$PXENV_STOP_UNDI, %bx
	call	pxe_call
	jnc	1f
	call	print_pxe_error
	jmp	99f
1:	/* Free base memory used by UNDI */
	movw	undi_fbms_end, %ax
	movw	undi_fbms_start, %bx
	call	free_basemem
	/* Clear UNDI_FL_STARTED */
	andw	$~UNDI_FL_STARTED, flags
99:	
#endif /* PXELOADER_KEEP_UNDI */

/*****************************************************************************
 * Print remaining free base memory
 *****************************************************************************
 */
print_free_basemem:
	movw	$10f, %si
	call	print_message
	movw	%fs:(0x13), %ax
	call	print_word
	movw	$20f, %si
	call	print_message
	.section ".prefix.data", "aw", @progbits
10:	.asciz	"         "
20:	.asciz	"kB free base memory after PXE unload\n"
	.previous
	
/*****************************************************************************
 * Exit point
 *****************************************************************************
 */	
finished:
	jmp	run_gpxe

/*****************************************************************************
 * Subroutine: print segment:offset address
 *
 * Parameters:
 *   %es:%bx : segment:offset address to print
 *   %ds:di : output buffer (or %di=0 to print to console)
 * Returns:
 *   %ds:di : next character in output buffer (if applicable)
 *****************************************************************************
 */
print_segoff:
	/* Preserve registers */
	pushw	%ax
	/* Print "<segment>:offset" */
	movw	%es, %ax
	call	print_hex_word
	movb	$( ':' ), %al
	call	print_character
	movw	%bx, %ax
	call	print_hex_word
	/* Restore registers and return */
	popw	%ax
	ret

/*****************************************************************************
 * Subroutine: print decimal word
 *
 * Parameters:
 *   %ax : word to print
 *   %ds:di : output buffer (or %di=0 to print to console)
 * Returns:
 *   %ds:di : next character in output buffer (if applicable)
 *****************************************************************************
 */
print_word:
	/* Preserve registers */
	pushw	%ax
	pushw	%bx
	pushw	%cx
	pushw	%dx
	/* Build up digit sequence on stack */
	movw	$10, %bx
	xorw	%cx, %cx
1:	xorw	%dx, %dx
	divw	%bx, %ax
	pushw	%dx
	incw	%cx
	testw	%ax, %ax
	jnz	1b
	/* Print digit sequence */
1:	popw	%ax
	call	print_hex_nibble
	loop	1b
	/* Restore registers and return */
	popw	%dx
	popw	%cx
	popw	%bx
	popw	%ax
	ret
	
/*****************************************************************************
 * Subroutine: zero 1kB block of base memory
 *
 * Parameters:
 *   %bx : block to zero (in kB)
 * Returns:
 *   Nothing
 *****************************************************************************
 */
zero_kb:
	/* Preserve registers */
	pushw	%ax
	pushw	%cx
	pushw	%di
	pushw	%es
	/* Zero block */
	movw	%bx, %ax
	shlw	$6, %ax
	movw	%ax, %es
	movw	$0x400, %cx
	xorw	%di, %di
	xorw	%ax, %ax
	rep stosb
	/* Restore registers and return */
	popw	%es
	popw	%di
	popw	%cx
	popw	%ax
	ret
	
/*****************************************************************************
 * Subroutine: free and zero base memory
 *
 * Parameters:
 *   %ax : Desired new free base memory counter (in kB)
 *   %bx : Expected current free base memory counter (in kB)
 *   %fs : BIOS data segment (0x40)
 * Returns:
 *   None
 *
 * The base memory from %bx kB to %ax kB is unconditionally zeroed.
 * It will be freed if and only if the expected current free base
 * memory counter (%bx) matches the actual current free base memory
 * counter in 0x40:0x13; if this does not match then the memory will
 * be leaked.
 *****************************************************************************
 */
free_basemem:
	/* Zero base memory */
	pushw	%bx
1:	cmpw	%bx, %ax
	je	2f
	call	zero_kb
	incw	%bx
	jmp	1b
2:	popw	%bx
	/* Free base memory */
	cmpw	%fs:(0x13), %bx		/* Update FBMS only if "old" value  */
	jne	1f			/* is correct			    */
1:	movw	%ax, %fs:(0x13)
	ret

/*****************************************************************************
 * Subroutine: make a PXE API call.  Works with either !PXE or PXENV+ API.
 *
 * Parameters:
 *   %bx : PXE API call number
 *   %ds:pxe_parameter_structure : Parameters for PXE API call
 * Returns:
 *   %ax : PXE status code (not exit code)
 *   CF set if %ax is non-zero
 *****************************************************************************
 */
pxe_call:
	/* Preserve registers */
	pushw	%di
	pushw	%es
	/* Set up registers for PXENV+ API.  %bx already set up */
	pushw	%ds
	popw	%es
	movw	$pxe_parameter_structure, %di
	/* Set up stack for !PXE API */
	pushw   %es
	pushw	%di
	pushw	%bx
	/* Make the API call */
	lcall	*entry_segoff
	/* Reset the stack */
	addw	$6, %sp
	movw	pxe_parameter_structure, %ax
	clc
	testw	%ax, %ax
	jz	1f
	stc
1:	/* Restore registers and return */
	popw	%es
	popw	%di
	ret

/*****************************************************************************
 * Subroutine: print PXE API call error message
 *
 * Parameters:
 *   %ax : PXE status code
 *   %bx : PXE API call number
 * Returns:
 *   Nothing
 *****************************************************************************
 */
print_pxe_error:
	pushw	%si
	movw	$10f, %si
	call	print_message
	xchgw	%ax, %bx
	call	print_hex_word
	movw	$20f, %si
	call	print_message
	xchgw	%ax, %bx
	call	print_hex_word
	movw	$30f, %si
	call	print_message
	popw	%si
	ret
	.section ".prefix.data", "aw", @progbits
10:	.asciz	"         UNDI API call "
20:	.asciz	" failed: status code "
30:	.asciz	"\n"
	.previous

/*****************************************************************************
 * PXE data structures
 *****************************************************************************
 */
	.section ".prefix.data"

pxe_ss:			.word 0
pxe_esp:		.long 0

pxe_parameter_structure: .fill 20

undi_code_segoff:
undi_code_size:		.word 0
undi_code_segment:	.word 0

undi_data_segoff:
undi_data_size:		.word 0
undi_data_segment:	.word 0

/* The following fields are part of a struct undi_device */

undi_device:

pxenv_segoff:
pxenv_offset:		.word 0
pxenv_segment:		.word 0

ppxe_segoff:
ppxe_offset:		.word 0
ppxe_segment:		.word 0
	
entry_segoff:
entry_offset:		.word 0
entry_segment:		.word 0

undi_fbms_start:	.word 0
undi_fbms_end:		.word 0

pci_busdevfn:		.word UNDI_NO_PCI_BUSDEVFN
isapnp_csn:		.word UNDI_NO_ISAPNP_CSN
isapnp_read_port:	.word UNDI_NO_ISAPNP_READ_PORT

pci_vendor:		.word 0
pci_device:		.word 0
flags:
	.word ( UNDI_FL_INITIALIZED | UNDI_FL_STARTED | UNDI_FL_KEEP_ALL )

	.equ undi_device_size, ( . - undi_device )

/*****************************************************************************
 * Run gPXE main code
 *****************************************************************************
 */
	.section ".prefix"
run_gpxe:
	/* Install gPXE */
	call	install

	/* Set up real-mode stack */
	movw	%bx, %ss
	movw	$_estack16, %sp

#ifdef PXELOADER_KEEP_UNDI
	/* Copy our undi_device structure to the preloaded_undi variable */
	movw	%bx, %es
	movw	$preloaded_undi, %di
	movw	$undi_device, %si
	movw	$undi_device_size, %cx
	rep movsb
#endif

	/* Retrieve PXE %ss:esp */
	movw	pxe_ss,	%di
	movl	pxe_esp, %ebp

	/* Jump to .text16 segment with %ds pointing to .data16 */
	movw	%bx, %ds
	pushw	%ax
	pushw	$1f
	lret
	.section ".text16", "ax", @progbits
1:
	/* Run main program */
	pushl	$main
	pushw	%cs
	call	prot_call
	popl	%ecx /* discard */

	/* Uninstall gPXE */
	call	uninstall

	/* Restore PXE stack */
	movw	%di, %ss
	movl	%ebp, %esp

	/* Check PXE stack magic */
	popl	%eax
	cmpl	$STACK_MAGIC, %eax
	jne	1f

	/* PXE stack OK: return to caller */
	popw	%ds
	popw	%es
	popw	%fs
	popw	%gs
	popal
	popfl
	xorw	%ax, %ax	/* Return success */
	lret

1:	/* PXE stack corrupt or removed: use INT 18 */
	int	$0x18
	.previous
