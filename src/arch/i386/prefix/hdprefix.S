	.text
	.arch i386
	.section ".prefix", "awx", @progbits
	.code16
	.org 0

	movw	$load_image, %bp
	jmp	find_active_partition

#include "bootpart.S"

load_image:
	movw	$_rom_size, %bp
1:	/* Truncate read length to end of track */
	movzwl	%bp, %eax
			movw	$1, %ax
	/* Read sectors */
	call	*read_sectors
	jc	load_failed
	/* Update %es */
	movw	%es, %bx
	shll	$5, %eax
	addw	%ax, %bx
	movw	%bx, %es
	shrl	$5, %eax
	/* Update LBA address */
	addl	%eax, %edi
	adcl	$0, %esi
	/* Update CHS address */
			// hmmmm
	/* Loop until whole image is read */
	subw	%ax, %bp
	jne	1b
	ljmp	$BOOT_SEG, $start_image

load_failed:
	movw	$10f, %si
	jmp	boot_error
10:	.asciz	"Could not load gPXE\r\n"

	.org 510
	.byte 0x55, 0xaa

start_image:
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
	pushl	$main
	pushw	%cs
	call	prot_call
	popl	%eax /* discard */

	/* Boot next device */
	int $0x18
