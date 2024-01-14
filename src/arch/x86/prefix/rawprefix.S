/*
 * Raw binary prefix
 *
 * Assumes that entire image is already loaded as a contiguous block
 * on a paragraph boundary and entered in real mode.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL )

	.section ".note.GNU-stack", "", @progbits
	.code16
	.arch i386
	.org 0

#include <librm.h>

	.section ".prefix", "ax", @progbits
	.globl	_raw_start
_raw_start:

	/* Adjust %cs so that %cs:0000 is the start of the image */
	movw	%cs, %ax
	call	1f
1:	popw	%bx
	subw	$1b, %bx
	shrw	$4, %bx
	addw	%bx, %ax
	pushw	%ax
	pushw	$2f
	lret
2:
	/* Install iPXE */
	call	install

	/* Set up real-mode stack */
	movw	%bx, %ss
	movw	$_estack16, %sp

	/* Jump to .text16 segment */
	pushw	%ax
	pushw	$1f
	lret
	.section ".text16", "awx", @progbits
1:
	/* Run iPXE */
	virtcall main

	/* Uninstall iPXE */
	call	uninstall

	/* Boot next device */
	int $0x18
