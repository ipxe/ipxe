FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL )

	.section ".note.GNU-stack", "", @progbits

/****************************************************************************
 * Internal stack
 ****************************************************************************
 */
	.section ".stack16", "aw", @nobits
	.balign 8
	.globl _stack16
_stack16:
	.space 4096
	.globl _estack16
_estack16:
