/*
	Prefix for .exe images
	Doesn't work yet, even though it starts off the same as a .com
	image as shown by DOS debug.
*/

	.text
	.code16
	.arch i386
	.section	".prefix", "ax", @progbits

_prefix:
	.byte	'M', 'Z'
	.short	_exe_size_tail			/* tail */
	.short	_exe_size_pages			/* pages */
	.short	0				/* relocations */
	.short	2				/* header paras */
	.short	_exe_bss_size			/* min */
	.short	0xFFFF				/* max paras */
	.short	_exe_ss_offset			/* SS */
	.short	_stack_size			/* SP */
	.short	0				/* checksum */
	.short	0				/* IP */
	.short	0				/* CS */
	.short	0x1C				/* reloc offset */
	.short	0				/* overlay number */
	.short	0				/* fill */
	.short	0				/* fill */

	.section ".text16", "ax", @progbits
prefix_exit:
	movw	$0x4c00,%ax		/* return to DOS */
	int	$0x21			/* reach this on Quit */
prefix_exit_end:
	.previous

/* The body of etherboot is attached here at build time.
 * Force 16 byte alignment
 */
	.align 16,0
_body: 
