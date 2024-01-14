FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL )

	.section ".note.GNU-stack", "", @progbits
	.code16
	.arch i386
	.section ".prefix", "awx", @progbits
	.org 0

	.globl mbr
mbr:
	movw	$exec_sector, %bp
	jmp	find_active_partition
exec_sector:
	ljmp	$0x0000, $0x7c00

#include "bootpart.S"
