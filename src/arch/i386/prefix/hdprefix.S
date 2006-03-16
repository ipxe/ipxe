/****************************************************************\

hdprefix.S Copyright (C) 2005 Per Dalgas Jakobsen

This code has been inspired/derived by the OSLoader by Vladislav Aleksandrov.
http://www.programmersheaven.com/zone5/cat469/40546.htm.

This software may be used and distributed according to the terms
of the GNU Public License (GPL), incorporated herein by reference.

hdprefix.S is loaded at 0x0000:0x7c00 by the bios-startup routines.

Actions performed by hdprefix:
1) Load the MBR to LOADSEG:0
2) Check which partition is active (or try first partition if none active)
3) Check wether LBA is supported.
3a) LBA
3a1) Load PAYLOAD_SECTS sectors from chosen partition to LOADSEG:0
3b) CHS (standard)
3b1) Load PAYLOAD_SECTS sectors from chosen partition to LOADSEG:0
4) Check loaded bootsector for BOOTMAGIC code.
5) Jump to payload LOADSEG:ENTRYPOINT.

Output with failure points (!#):
---
Loading (!1)partition #
Std. BIOS(!2) | Ext. BIOS(!3)
Booting...(!4)
(!5)
---

!1: Failed to load MBR with Int13,ah=2.
!2: Failed to load bootrecord+payload with Int13,ah=2.
!3: Failed to load bootrecord+payload with Int13,ah=42.
!4: Invalid BOOTMAGIC in loaded bootrecord.
!5: Jumping to payload.

\*****************************************************************/
 
.equ	BOOTSEG,	0x07c0
.equ	LOADSEG,	0x1000
.equ	ENTRYPOINT,	_start

.equ	BOOTMAGIC,	0x0aa55

.equ	partition_table,	0x1be
.equ	partition_rec_size,	0x10

.equ	boot_ind,	0	/* 80h=active */
.equ	start_head,	1
.equ	start_sector,	2	/* bits 0-5 */
.equ	start_cyl,	3	/* bits 8,9 in bits 6,7 of sector */
.equ	os_ind,		4	/* os indicator */
.equ	end_head,	5
.equ	end_sector,	6	/* bits 0-5 */
.equ	end_track,	7	/* bits 8,9 in bits 6,7 of sector */
.equ	nsect,		8	/* sectors preceding partition */
.equ	lenght,		0x0c	/* length of partition in sectors */

/-------------------------------------------------------------

	.arch i386
	.text
	.section ".prefix", "ax", @progbits
	.code16
	 
bootstart:
	jmp	$BOOTSEG,$_go		/* reload cs:ip */


/****************************************************************/
/* support routines.						*/
/*--------------------------------------------------------------*/
_failed:
	movw	$BOOTSEG,%ax
	movw	%ax,%ds
	movw	$_failed_msg_end-_failed_msg,%cx
	movw	$_failed_msg,%si
	call	_print_str

	/* stop execution - should probably have option to auto-reboot after delay. */
_failed_loop:
	jmp	_failed_loop

/*--------------------------------------------------------------*/
_print_str:
	/* cx = count, ds:si = string. */
	movw	$0x0007,%bx
	movb	$0x0e,%ah
_print_loop:
	lodsb
	int	$0x10
	loop	_print_loop
	ret

/*--------------------------------------------------------------*/			
_print_char:
	/* al = char. */
	movw	$0x0007,%bx
	movb	$0x0e,%ah
	int	$0x10
	ret

/*--------------------------------------------------------------*/
_print_nl:
	/* - */
	movb	$0x0d,%al
	call	_print_char
	movb	$0x0a,%al
	call	_print_char
	ret

/*--------------------------------------------------------------*/
_print_hex:
	/* dx = value */
	movb	$0x0e,%ah	/* write char, tty mode */
	movw	$0x0007,%bx	/* page 0, attribute 7 (normal) */
	call	_print_digit
	call	_print_digit
	call	_print_digit
	/* fall through */
_print_digit:
	rolw	$4,%dx		/* rotate so that lowest 4 bits are used */
	movb	$0x0f,%al	/* mask for nibble */
	andb	%dl,%al
	addb	$0x90,%al	/* convert al to ascii hex (four instructions) */
	daa
	adcb	$0x40,%al
	daa
	int	$0x10
	ret

/****************************************************************/


_go:
	cli
	movw	$BOOTSEG,%ax
	movw	%ax,%ds
	movw	%ax,%ss
	movw	$0x2000,%sp	/* good large stack. */
	sti
	cld
	movw	$LOADSEG,%ax
	movw	%ax,%es

	movw	$_load_msg_end-_load_msg,%cx
	movw	$_load_msg,%si
	call	_print_str

/*--- load MBR so we can use its partition table. ---*/
	xorw	%bx,%bx
	movw	$0x0001,%cx	/* chs: 0,0,1 */
	movb	%bh,%dh		/* - */
	movb	$0x80,%dl
	movw	$0x0201,%ax	/* read one sector (MBR) */
	int	$0x13
	jc	_failed

/*--- find the active partition ---*/
	movw	$_part_msg_end-_part_msg,%cx
	movw	$_part_msg,%si
	call	_print_str

	movw	$partition_table,%di
	movw	$4,%cx
_partition_loop:
	cmpb	$0x80,%es:(%di)		/* active? */
	je	_partition_found
	addw	$partition_rec_size,%di
	loop	_partition_loop

	/*- no partitions marked active - use 1. partition. */
	movw	$partition_table,%di
	movw	$4,%cx

_partition_found:
	movb	$'5',%al			/* convert to ascii */
	subb	%cl,%al
	call	_print_char
	call	_print_nl

/*--- check for lba support ---*/
	movw	$0x55aa,%bx
	movb	$0x80,%dl
	movb	$0x41,%ah
	int	$0x13
	jc	__bios
	cmpw	$0x0aa55,%bx
	jnz	__bios
	testb	$1,%cl
	jz	__bios

/*--- use lba bios calls to read sectors ---*/
_lba:
	movw	$_extbios_msg_end-_extbios_msg,%cx
	movw	$_extbios_msg,%si
	call	_print_str

	movw	%es:nsect(%di),%ax
	movw	%ax,_bios_lba_low
	movw	%es:nsect+2(%di),%ax
	movw	%ax,_bios_lba_high
	movb	$0x80,%dl
	movw	$_disk_address_packet,%si
	movw	$0x4200,%ax	/* read */
	int	$0x13
	jc	_failed
	jmp	__loaded

/*--- use standard bios calls to read sectors ---*/
__bios:
	movw	$_stdbios_msg_end-_stdbios_msg,%cx
	movw	$_stdbios_msg,%si
	call	_print_str

	movw	_disk_address_packet+2(,1),%ax	/* only low byte is used. */
	xorw	%bx,%bx
	movw	%es:start_sector(%di),%cx
	movb	%es:start_head(%di),%dh
	movb	$0x80,%dl
	movb	$0x02,%ah
	int	$0x13
	jc	_failed

__loaded:
	movw	$_boot_msg_end-_boot_msg,%cx
	movw	$_boot_msg,%si
	call	_print_str

	/* check if it has a valid bootrecord. */
	cmpw	$BOOTMAGIC,%es:510(,1)
	jne	_failed
	call	_print_nl

	/* call the payload. */
	pushl	$0		/* No parameters to preserve for exit path */
	pushw	$0		/* Use prefix exit path mechanism */
	jmp	$LOADSEG,$ENTRYPOINT

	.section ".text16", "ax", @progbits
	.globl	prefix_exit
prefix_exit:
	int	$0x19		/* should try to boot machine */
	.globl	prefix_exit_end
prefix_exit_end:
	.previous
	 

/*--------------------------------------------------------------*/

_load_msg:	.ascii	"Loading "
_load_msg_end:
_part_msg:	.ascii	"partition "
_part_msg_end:
_boot_msg:	.ascii	"Booting..."
_boot_msg_end:
_stdbios_msg:	.ascii	"Std. BIOS\r\n"
_stdbios_msg_end:
_extbios_msg:	.ascii	"Ext. BIOS\r\n"
_extbios_msg_end:
_failed_msg:	.ascii	"FAILED!!!\r\n"
_failed_msg_end:


/*--------------------------------------------------------------*/

_disk_address_packet:
		.byte	0x10		/* size of the packet */
		.byte	0		/* reserved */
		.word	_verbatim_size_sct	/* number of sectors to read */
		.word	0x0000		/* offset */
		.word	LOADSEG 	/* segment of buffer */
_bios_lba_low:	.word	0
_bios_lba_high:	.word	0
		.word	0
		.word	0

	.rept 32
		.byte	0
	.endr


/*--- Partition table ------------------------------------------*/

	.org 446, 0
	.rept 64
		.byte	0
	.endr


/*--- Magic code -----------------------------------------------*/
	.org 510, 0
		.word BOOTMAGIC

/*** END ********************************************************/
