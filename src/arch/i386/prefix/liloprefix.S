/*
	Copyright (C) 2000, Entity Cyber, Inc.

	Authors: Gary Byers (gb@thinguin.org)
		 Marty Connor (mdc@thinguin.org)

	This software may be used and distributed according to the terms
	of the GNU Public License (GPL), incorporated herein by reference.

	Description:	

	This is just a little bit of code and data that can get prepended
	to an Etherboot ROM image in order to allow LILO to load the
	result as if it were a Linux kernel image.

	A real Linux kernel image consists of a one-sector boot loader
	(to load the image from a floppy disk), followed a few sectors
	of setup code, followed by the kernel code itself.  There's
	a table in the first sector (starting at offset 497) that indicates
	how many sectors of setup code follow the first sector and which
	contains some other parameters that aren't interesting in this
	case.

	When LILO loads the sectors that comprise a kernel image, it doesn't
	execute the code in the first sector (since that code would try to
	load the image from a floppy disk.)  The code in the first sector
	below doesn't expect to get executed (and prints an error message
	if it ever -is- executed.)  LILO's only interested in knowing the
	number of setup sectors advertised in the table (at offset 497 in
	the first sector.)

	Etherboot doesn't require much in the way of setup code.
	Historically, the Linux kernel required at least 4 sectors of
	setup code.  Current versions of LILO look at the byte at
	offset 497 in the first sector to indicate how many sectors
	of setup code are contained in the image.

*/

#define	SETUPSECS 4		/* Minimal nr of setup-sectors */
#define PREFIXSIZE ((SETUPSECS+1)*512)
#define PREFIXPGH (PREFIXSIZE / 16 )
#define	BOOTSEG  0x07C0		/* original address of boot-sector */
#define	INITSEG  0x9000		/* we move boot here - out of the way */
#define	SETUPSEG 0x9020		/* setup starts here */
#define SYSSEG   0x1000		/* system loaded at 0x10000 (65536). */

	.text
	.code16
	.arch i386
	.org	0
	.section ".prefix", "ax", @progbits
	.globl	_prefix
_prefix:

/* 
	This is a minimal boot sector.	If anyone tries to execute it (e.g., if
	a .lilo file is dd'ed to a floppy), print an error message. 
*/

bootsector: 
	jmp	$BOOTSEG, $go - _prefix	/* reload cs:ip to match relocation addr */
go: 
	movw	$0x2000, %di		/*  0x2000 is arbitrary value >= length
					    of bootsect + room for stack */

	movw	$BOOTSEG, %ax
	movw	%ax,%ds
	movw	%ax,%es

	cli
	movw	%ax, %ss		/* put stack at BOOTSEG:0x2000. */
	movw	%di,%sp
	sti

	movw	$why_end-why, %cx
	movw	$why - _prefix, %si

	movw	$0x0007, %bx		/* page 0, attribute 7 (normal) */
	movb	$0x0e, %ah		/* write char, tty mode */
prloop: 
	lodsb
	int	$0x10
	loop	prloop
freeze: jmp	freeze

why:	.ascii	"This image cannot be loaded from a floppy disk.\r\n"
why_end: 


	.org	497
setup_sects: 
	.byte	SETUPSECS
root_flags: 
	.word	0
syssize: 
	.word	_verbatim_size_pgh - PREFIXPGH
swap_dev: 
	.word	0
ram_size: 
	.word	0
vid_mode: 
	.word	0
root_dev: 
	.word	0
boot_flag: 
	.word	0xAA55

/*
	We're now at the beginning of the second sector of the image -
	where the setup code goes.

	We don't need to do too much setup for Etherboot.

	This code gets loaded at SETUPSEG:0.  It wants to start
	executing the Etherboot image that's loaded at SYSSEG:0 and
	whose entry point is SYSSEG:0.
*/
setup_code: 
	pushl	$0		/* No parameters to preserve for exit path */
	pushw	$0		/* Use prefix exit path mechanism */
	/* Etherboot expects to be contiguous in memory once loaded.
	 * LILO doesn't do this, but since we don't need any
	 * information that's left in the prefix, it doesn't matter:
	 * we just have to ensure that %cs:0000 is where the start of
	 * the Etherboot image *would* be.
	 */
	ljmp	$(SYSSEG-(PREFIXSIZE/16)), $_start

	.section ".text16", "ax", @progbits
	.globl	prefix_exit
prefix_exit:
	int	$0x19		/* should try to boot machine */
	.globl	prefix_exit_end
prefix_exit_end:
	.previous
	
	.org	(PREFIXSIZE-1)
	.byte	0
prefix_end:
/*
	That's about it.
*/

