/* We need a real mode stack that won't be stomped on by Etherboot
   which starts at 0x20000. Choose something that's sufficiently high,
   but not in DOC territory. Note that we couldn't do this in a real
   .com program since stack variables are in the same segment as the
   code and data, but this isn't really a .com program, it just looks
   like one to make DOS load it into memory. It still has the 64kB
   limitation of .com files though. */
#define STACK_SEG	0x7000
#define STACK_SIZE	0x4000

	.text
	.code16
	.arch i386
	.section ".prefix", "ax", @progbits

/* Cheat a little with the relocations: .COM files are loaded at 0x100 */
_prefix:
	/* Set up temporary stack */ 
	movw	$STACK_SEG, %ax
	movw	%ax, %ss
	movw	$STACK_SIZE, %sp

	pushl	$0	/* No parameters to preserve for exit path */
	pushw	$0	/* Dummy return address - use prefix_exit */

	/* Calculate segment address of image start */
	pushw	%cs
	popw	%ax
	addw	$(0x100/16), %ax
	pushw	%ax
	pushw	$_start
	/* Calculated lcall to _start with %cs:0000 = image start */
	lret

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
