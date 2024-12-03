FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL )

	.section ".note.GNU-stack", "", @progbits
	.org	0
	.code16
	.arch i386

	.section ".prefix", "ax", @progbits
_prefix:

	.section ".text16", "ax", @progbits
prefix_exit:

prefix_exit_end:
	.previous
