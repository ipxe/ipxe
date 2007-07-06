/*
	Copyright (C) 2000, Entity Cyber, Inc.

	Authors: Gary Byers (gb@thinguin.org)
		 Marty Connor (mdc@thinguin.org)
		 Eric Biederman (ebiederman@lnxi.com)

	This code also derives a lot from arch/i386/boot/setup.S in
	the linux kernel.

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

	The setup code that is present here does a lot of things
	exactly the way the linux kernel does them instead of in
	ways more typical of etherboot.  Generally this is so
	the code can be strongly compatible with the linux kernel.
	In addition the general etherboot technique of enabling the a20
	after we switch into protected mode does not work if etherboot
	is being loaded at 1MB.
*/

	.equ	CR0_PE,1

#ifdef	GAS291
#define DATA32 data32;
#define ADDR32 addr32;
#define	LJMPI(x)	ljmp	x
#else
#define DATA32 data32
#define ADDR32 addr32
/* newer GAS295 require #define	LJMPI(x)	ljmp	*x */
#define	LJMPI(x)	ljmp	x
#endif

/* Simple and small GDT entries for booting only */
#define GDT_ENTRY_BOOT_CS	2
#define GDT_ENTRY_BOOT_DS	(GDT_ENTRY_BOOT_CS + 1)
#define __BOOT_CS	(GDT_ENTRY_BOOT_CS * 8)
#define __BOOT_DS	(GDT_ENTRY_BOOT_DS * 8)


#define	SETUPSECS 4		/* Minimal nr of setup-sectors */
#define PREFIXSIZE ((SETUPSECS+1)*512)
#define PREFIXPGH (PREFIXSIZE / 16 )
#define	BOOTSEG  0x07C0		/* original address of boot-sector */
#define	INITSEG  0x9000		/* we move boot here - out of the way */
#define	SETUPSEG 0x9020		/* setup starts here */
#define SYSSEG   0x1000		/* system loaded at 0x10000 (65536). */

#define DELTA_INITSEG  (SETUPSEG - INITSEG) /* 0x0020 */
	
/* Signature words to ensure LILO loaded us right */
#define SIG1	0xAA55
#define SIG2	0x5A5A

	.text
	.code16
	.arch i386
	.org	0
	.section ".prefix", "ax", @progbits
_prefix:

/* 
	This is a minimal boot sector.	If anyone tries to execute it (e.g., if
	a .lkrn file is dd'ed to a floppy), print an error message. 
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
	jmp	trampoline
# This is the setup header, and it must start at %cs:2 (old 0x9020:2)

		.ascii	"HdrS"		# header signature
		.word	0x0203		# header version number (>= 0x0105)
					# or else old loadlin-1.5 will fail)
realmode_swtch:	.word	0, 0		# default_switch, SETUPSEG
start_sys_seg:	.word	SYSSEG		# low load segment (obsolete)
		.word	kernel_version - setup_code
					# pointing to kernel version string
					# above section of header is compatible
					# with loadlin-1.5 (header v1.5). Don't
					# change it.

type_of_loader:	.byte	0		# = 0, old one (LILO, Loadlin,
					#      Bootlin, SYSLX, bootsect...)
					# See Documentation/i386/boot.txt for
					# assigned ids
	
# flags, unused bits must be zero (RFU) bit within loadflags
loadflags:
LOADED_HIGH	= 1			# If set, the kernel is loaded high
CAN_USE_HEAP	= 0x80			# If set, the loader also has set
					# heap_end_ptr to tell how much
					# space behind setup.S can be used for
					# heap purposes.
					# Only the loader knows what is free
		.byte	LOADED_HIGH

setup_move_size: .word  0x8000		# size to move, when setup is not
					# loaded at 0x90000. We will move setup 
					# to 0x90000 then just before jumping
					# into the kernel. However, only the
					# loader knows how much data behind
					# us also needs to be loaded.

code32_start:				# here loaders can put a different
					# start address for 32-bit code.
		.long	0x100000	# 0x100000 = default for big kernel

ramdisk_image:	.long	0		# address of loaded ramdisk image
					# Here the loader puts the 32-bit
					# address where it loaded the image.
					# This only will be read by the kernel.

ramdisk_size:	.long	0		# its size in bytes

bootsect_kludge:
		.long	0		# obsolete

heap_end_ptr:	.word	0		# (Header version 0x0201 or later)
					# space from here (exclusive) down to
					# end of setup code can be used by setup
					# for local heap purposes.

pad1:		.word	0
cmd_line_ptr:	.long 0			# (Header version 0x0202 or later)
					# If nonzero, a 32-bit pointer
					# to the kernel command line.
					# The command line should be
					# located between the start of
					# setup and the end of low
					# memory (0xa0000), or it may
					# get overwritten before it
					# gets read.  If this field is
					# used, there is no longer
					# anything magical about the
					# 0x90000 segment; the setup
					# can be located anywhere in
					# low memory 0x10000 or higher.

ramdisk_max:	.long 0			# (Header version 0x0203 or later)
					# The highest safe address for
					# the contents of an initrd

trampoline:	call	start_of_setup
trampoline_end:
		.space	1024
# End of setup header #####################################################

start_of_setup:
# Set %ds = %cs, we know that SETUPSEG = %cs at this point
	movw	%cs, %ax		# aka SETUPSEG
	movw	%ax, %ds
# Check signature at end of setup
	cmpw	$SIG1, (setup_sig1 - setup_code)
	jne	bad_sig

	cmpw	$SIG2, (setup_sig2 - setup_code)
	jne	bad_sig

	jmp	good_sig1

# Routine to print asciiz string at ds:si
prtstr:
	lodsb
	andb	%al, %al
	jz	fin

	call	prtchr
	jmp	prtstr

fin:	ret

# Part of above routine, this one just prints ascii al
prtchr:	pushw	%ax
	pushw	%cx
	movw	$7,%bx
	movw	$0x01, %cx
	movb	$0x0e, %ah
	int	$0x10
	popw	%cx
	popw	%ax
	ret

no_sig_mess: .string	"No setup signature found ..."

good_sig1:
	jmp	good_sig

# We now have to find the rest of the setup code/data
bad_sig:
	movw	%cs, %ax			# SETUPSEG
	subw	$DELTA_INITSEG, %ax		# INITSEG
	movw	%ax, %ds
	xorb	%bh, %bh
	movb	(497), %bl			# get setup sect from bootsect
	subw	$4, %bx				# LILO loads 4 sectors of setup
	shlw	$8, %bx				# convert to words (1sect=2^8 words)
	movw	%bx, %cx
	shrw	$3, %bx				# convert to segment
	addw	$SYSSEG, %bx
	movw	%bx, %cs:(start_sys_seg - setup_code)
# Move rest of setup code/data to here
	movw	$2048, %di			# four sectors loaded by LILO
	subw	%si, %si
	pushw	%cs
	popw	%es
	movw	$SYSSEG, %ax
	movw	%ax, %ds
	rep
	movsw
	movw	%cs, %ax			# aka SETUPSEG
	movw	%ax, %ds
	cmpw	$SIG1, (setup_sig1 - setup_code)
	jne	no_sig

	cmpw	$SIG2, (setup_sig2 - setup_code)
	jne	no_sig

	jmp	good_sig

no_sig:
	lea	(no_sig_mess - setup_code), %si
	call	prtstr

no_sig_loop:
	hlt
	jmp	no_sig_loop

good_sig:
	cmpw	$0, %cs:(realmode_swtch - setup_code)
	jz	rmodeswtch_normal

	lcall	*%cs:(realmode_swtch - setup_code)
	jmp	rmodeswtch_end

rmodeswtch_normal:
	pushw	%cs
	call	default_switch

rmodeswtch_end:
# we get the code32 start address and modify the below 'jmpi'
# (loader may have changed it)
	movl	%cs:(code32_start - setup_code), %eax
	movl	%eax, %cs:(code32 - setup_code)

# then we load the segment descriptors
	movw	%cs, %ax			# aka SETUPSEG
	movw	%ax, %ds

#
# Enable A20.  This is at the very best an annoying procedure.
# A20 code ported from SYSLINUX 1.52-1.63 by H. Peter Anvin.
#

A20_TEST_LOOPS		=  32		# Iterations per wait
A20_ENABLE_LOOPS	= 255		# Total loops to try		

a20_try_loop:

	# First, see if we are on a system with no A20 gate.
a20_none:
	call	a20_test
	jnz	a20_done

	# Next, try the BIOS (INT 0x15, AX=0x2401)
a20_bios:
	movw	$0x2401, %ax
	pushfl					# Be paranoid about flags
	int	$0x15
	popfl

	call	a20_test
	jnz	a20_done

	# Try enabling A20 through the keyboard controller
a20_kbc:
	call	empty_8042

	call	a20_test			# Just in case the BIOS worked
	jnz	a20_done			# but had a delayed reaction.

	movb	$0xD1, %al			# command write
	outb	%al, $0x64
	call	empty_8042

	movb	$0xDF, %al			# A20 on
	outb	%al, $0x60
	call	empty_8042

	# Wait until a20 really *is* enabled; it can take a fair amount of
	# time on certain systems; Toshiba Tecras are known to have this
	# problem.
a20_kbc_wait:
	xorw	%cx, %cx
a20_kbc_wait_loop:
	call	a20_test
	jnz	a20_done
	loop	a20_kbc_wait_loop

	# Final attempt: use "configuration port A"
a20_fast:
	inb	$0x92, %al			# Configuration Port A
	orb	$0x02, %al			# "fast A20" version
	andb	$0xFE, %al			# don't accidentally reset
	outb	%al, $0x92

	# Wait for configuration port A to take effect
a20_fast_wait:
	xorw	%cx, %cx
a20_fast_wait_loop:
	call	a20_test
	jnz	a20_done
	loop	a20_fast_wait_loop

	# A20 is still not responding.  Try frobbing it again.
	# 
	decb	(a20_tries - setup_code)
	jnz	a20_try_loop
	
	movw	$(a20_err_msg - setup_code), %si
	call	prtstr

a20_die:
	hlt
	jmp	a20_die

a20_tries:
	.byte	A20_ENABLE_LOOPS

a20_err_msg:
	.ascii	"linux: fatal error: A20 gate not responding!"
	.byte	13, 10, 0

	# If we get here, all is good
a20_done:
	# Leave the idt alone
	
	# set up gdt 
	xorl	%eax, %eax				# Compute gdt_base
	movw	%ds, %ax				# (Convert %ds:gdt to a linear ptr)
	shll	$4, %eax
	addl	$(bImage_gdt - setup_code), %eax
	movl	%eax, (bImage_gdt_48+2 - setup_code)
	DATA32 lgdt %ds:(bImage_gdt_48 - setup_code)	# load gdt with whatever is
							# appropriate

	# Switch to protected mode
	movl	%cr0, %eax
	orb	$CR0_PE, %al
	movl	%eax, %cr0

	DATA32 ljmp *%ds:(code32 - setup_code)
code32:
	.long	0x100000
	.word	__BOOT_CS, 0
	
# Here's a bunch of information about your current kernel..
kernel_version:	.ascii	"Etherboot "
		.ascii	VERSION
		.byte	0

# This is the default real mode switch routine.
# to be called just before protected mode transition
default_switch:
	cli					# no interrupts allowed !
	movb	$0x80, %al			# disable NMI for bootup
						# sequence
	outb	%al, $0x70
	lret

# This routine tests whether or not A20 is enabled.  If so, it
# exits with zf = 0.
#
# The memory address used, 0x200, is the int $0x80 vector, which
# should be safe.

A20_TEST_ADDR = 4*0x80

a20_test:
	pushw	%cx
	pushw	%ax
	xorw	%cx, %cx
	movw	%cx, %fs			# Low memory
	decw	%cx
	movw	%cx, %gs			# High memory area
	movw	$A20_TEST_LOOPS, %cx
	movw	%fs:(A20_TEST_ADDR), %ax
	pushw	%ax
a20_test_wait:
	incw	%ax
	movw	%ax, %fs:(A20_TEST_ADDR)
	call	delay				# Serialize and make delay constant
	cmpw	%gs:(A20_TEST_ADDR+0x10), %ax
	loope	a20_test_wait

	popw	%fs:(A20_TEST_ADDR)
	popw	%ax
	popw	%cx
	ret	


# This routine checks that the keyboard command queue is empty
# (after emptying the output buffers)
#
# Some machines have delusions that the keyboard buffer is always full
# with no keyboard attached...
#
# If there is no keyboard controller, we will usually get 0xff
# to all the reads.  With each IO taking a microsecond and
# a timeout of 100,000 iterations, this can take about half a
# second ("delay" == outb to port 0x80). That should be ok,
# and should also be plenty of time for a real keyboard controller
# to empty.
#

empty_8042:
	pushl	%ecx
	movl	$100000, %ecx

empty_8042_loop:
	decl	%ecx
	jz	empty_8042_end_loop

	call	delay

	inb	$0x64, %al			# 8042 status port
	testb	$1, %al				# output buffer?
	jz	no_output

	call	delay
	inb	$0x60, %al			# read it
	jmp	empty_8042_loop

no_output:
	testb	$2, %al				# is input buffer full?
	jnz	empty_8042_loop			# yes - loop
empty_8042_end_loop:
	popl	%ecx

		
# Delay is needed after doing I/O
delay:
	outb	%al,$0x80
	ret

# Descriptor tables
#
# NOTE: The intel manual says gdt should be sixteen bytes aligned for
# efficiency reasons.  However, there are machines which are known not
# to boot with misaligned GDTs, so alter this at your peril!  If you alter
# GDT_ENTRY_BOOT_CS (in asm/segment.h) remember to leave at least two
# empty GDT entries (one for NULL and one reserved).
#
# NOTE:	On some CPUs, the GDT must be 8 byte aligned.  This is
# true for the Voyager Quad CPU card which will not boot without
# This directive.  16 byte aligment is recommended by intel.
#
	.balign 16
bImage_gdt:
	.fill GDT_ENTRY_BOOT_CS,8,0

	.word	0xFFFF				# 4Gb - (0x100000*0x1000 = 4Gb)
	.word	0				# base address = 0
	.word	0x9A00				# code read/exec
	.word	0x00CF				# granularity = 4096, 386
						#  (+5th nibble of limit)

	.word	0xFFFF				# 4Gb - (0x100000*0x1000 = 4Gb)
	.word	0				# base address = 0
	.word	0x9200				# data read/write
	.word	0x00CF				# granularity = 4096, 386
						#  (+5th nibble of limit)
bImage_gdt_end:
	.balign	4
	
	.word	0				# alignment byte
bImage_idt_48:
	.word	0				# idt limit = 0
	.long	0				# idt base = 0L

	.word	0				# alignment byte
bImage_gdt_48:
	.word	bImage_gdt_end - bImage_gdt - 1	# gdt limit
	.long	bImage_gdt_48 - setup_code	# gdt base (filled in later)

	.section ".text16", "ax", @progbits
prefix_exit:
	int	$0x19		/* should try to boot machine */
prefix_exit_end:
	.previous
	
	
	.org (PREFIXSIZE - 4)
# Setup signature -- must be last
setup_sig1:	.word	SIG1
setup_sig2:	.word	SIG2
	/* Etherboot expects to be contiguous in memory once loaded.
	 * The linux bImage protocol does not do this, but since we
	 * don't need any information that's left in the prefix, it
	 * doesn't matter: we just have to ensure that we make it to _start
	 *
	 * protected_start will live at 0x100000 and it will be the
	 * the first code called as we enter protected mode.
	 */
	.code32
protected_start:
	/* Load segment registers */
	movw	$__BOOT_DS, %ax
	movw	%ax, %ss
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	/* Use the internal etherboot stack */
	movl	$(_prefix_stack_end - protected_start + 0x100000), %esp

	pushl	$0		/* No parameters to preserve for exit path */
	pushl	$0		/* Use prefix exit path mechanism */
	
	jmp	_start
/*
	That's about it.
*/
