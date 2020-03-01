FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL )

	.text
	.arch i386
	.section ".prefix", "awx", @progbits
	.code16
	.org 0

#include "mbr.S"

/* Partition table: 64 heads, 32 sectors/track (ZIP-drive compatible) */
#define HEADS 64
#define SECTORS 32
#define CYLADDR(cyl) ((((cyl) * HEADS + (((cyl) == 0) & 1)) * SECTORS) * 512)

#define LOGSTART 0
#define LOGCOUNT 1
#define BOOTSTART 1
#define BOOTCOUNT 2

	/* Construct a C/H/S address */
	.macro	chs cylinder, head, sector
	.byte	\head
	.byte	(((\cylinder & 0x300) >> 2) | \sector)
	.byte	(\cylinder & 0x0ff)
	.endm

	/* Construct a linear address */
	.macro	linear cylinders, heads, sectors
	.long	((((\cylinders * HEADS) + \heads) * SECTORS) + \sectors - 1)
	.endm

	/* Construct a partition table entry */
	.macro	partition bootflag, type, start, count
	.byte	\bootflag
	chs	\start, ((\start == 0) & 1), 1
	.byte	\type
	chs	(\start + \count - 1), (HEADS - 1), SECTORS
	linear	\start, ((\start == 0) & 1), 1
	linear	\count, 0, (1 - (((\start == 0) & 1) * SECTORS))
	.endm

	/* Partition table */
	.org 446
	.space 16
	.space 16
	/* Partition 3: log partition (for CONSOLE_INT13) */
	partition 0x00, 0xe0, LOGSTART, LOGCOUNT
	/* Partition 4: boot partition */
	partition 0x80, 0xeb, BOOTSTART, BOOTCOUNT

	/* Disk signature */
	.org 510
	.byte 0x55, 0xaa

/* Skip to start of log partition */
	.org CYLADDR(LOGSTART)
	.ascii "iPXE LOG\n\n"

/* Skip to start of boot partition */
	.org CYLADDR(BOOTSTART)
