#define BOOT_SEG	0x07c0
#define EXEC_SEG	0x0100
#define STACK_SEG 	0x0200
#define STACK_SIZE	0x2000
	
	.text
	.arch i386
	.section ".prefix", "awx", @progbits
	.code16

/*
 * Find active partition
 *
 * Parameters:
 *   %dl	: BIOS drive number
 *   %bp	: Active partition handler routine
 */
find_active_partition:
	/* Set up stack at STACK_SEG:STACK_SIZE */
	movw	$STACK_SEG, %ax
	movw	%ax, %ss
	movw	$STACK_SIZE, %sp
	/* Relocate self to EXEC_SEG */
	pushw	$BOOT_SEG
	popw	%ds
	pushw	$EXEC_SEG
	popw	%es
	xorw	%si, %si
	xorw	%di, %di
	movw	$0x200, %cx
	rep movsb
	ljmp	$EXEC_SEG, $1f
1:	pushw	%ds
	popw	%es
	pushw	%cs
	popw	%ds
	/* Read and process root partition table */
	xorb	%dh, %dh
	movw	$0x0001, %cx
	xorl	%esi, %esi
	xorl	%edi, %edi
	call	process_table
	/* Print failure message */
	movw	$10f, %si
	movw	$(20f-10f), %cx
1:	movw	$0x0007, %bx
	movb	$0x0e, %ah
	lodsb
	int	$0x10
	loop	1b
	/* Boot next device */
	int	$0x18
10:	.ascii	"Could not locate active partition\r\n"
20:	

/*
 * Process partition table
 *
 * Parameters:
 *   %dl	: BIOS drive number
 *   %dh	: Head
 *   %cl	: Sector (bits 0-5), high two bits of cylinder (bits 6-7)
 *   %ch	: Low eight bits of cylinder
 *   %esi:%edi	: LBA address
 *   %bp	: Active partition handler routine
 *
 * Returns:
 *   CF set on error
 */
process_table:
	pushal
	movw	$446, %bx
1:	call	read_sector
	jc	99f
	call	process_partition
	addw	$16, %bx
	cmpw	$510, %bx
	jne	1b
99:	popal
	ret

/*
 * Process partition
 *
 * Parameters:
 *   %dl	: BIOS drive number
 *   %dh	: Head
 *   %cl	: Sector (bits 0-5), high two bits of cylinder (bits 6-7)
 *   %ch	: Low eight bits of cylinder
 *   %esi:%edi	: LBA address
 *   %bx	: Offset within partition table
 *   %bp	: Active partition handler routine
 */
process_partition:
	pushal
	/* Load C/H/S values from partition entry */
	movb	%es:1(%bx), %dh
	movw	%es:2(%bx), %cx
	/* Update LBA address from partition entry */
	addl	%es:8(%bx), %edi
	adcl	$0, %esi
	/* Check active flag */
	testb	$0x80, %es:(%bx)
	jz	1f
	call	read_sector
	jc	99f
	jmp	*%bp
1:	/* Check for extended partition */
	movb	%es:4(%bx), %al
	cmpb	$0x05, %al
	je	2f
	cmpb	$0x0f, %al
	je	2f
	cmpb	$0x85, %al
	jne	99f
2:	call	process_table
99:	popal
	ret

/*
 * Read single sector to 0000:7c00 and verify 0x55aa signature
 *
 * Parameters:
 *   %dl	: BIOS drive number
 *   %dh	: Head
 *   %cl	: Sector (bits 0-5), high two bits of cylinder (bits 6-7)
 *   %ch	: Low eight bits of cylinder
 *   %esi:%edi	: LBA address
 *
 * Returns:
 *   CF set on error
 */
read_sector:
	pushal
	/* Check for LBA extensions */
	call	check_lba
	jnc	read_lba
read_chs:
	/* Read sector using C/H/S address */
	movw	$0x0201, %ax
	xorw	%bx, %bx
	stc
	int	$0x13
	sti
	jmp	99f
read_lba:
	/* Read sector using LBA address */
	movb	$0x42, %ah
	movl	%esi, (lba_desc + 12)
	movl	%edi, (lba_desc + 8)
	movw	$lba_desc, %si
	int	$0x13
99:	/* Check for 55aa signature */
	jc	99f
	cmpw	$0xaa55, %es:(510)
	je	99f
	stc
99:	popal
	ret

lba_desc:
	.byte	0x10
	.byte	0
	.word	1
	.word	0x0000
	.word	0x07c0
	.long	0, 0

/*
 * Check for LBA extensions
 *	
 * Parameters:
 *   %dl	: BIOS drive number
 *
 * Returns:
 *   CF clear if LBA extensions supported
 */
check_lba:
	pushal
	movb	$0x41, %ah
	movw	$0x55aa, %bx
	stc
	int	$0x13
	jc	99f
	cmpw	$0xaa55, %bx
	je	99f
	stc
99:	popal
	ret
