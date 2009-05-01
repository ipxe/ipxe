#ifndef INT13_H
#define INT13_H

/** @file
 *
 * INT 13 emulation
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <gpxe/list.h>
#include <realmode.h>

struct block_device;

/**
 * @defgroup int13ops INT 13 operation codes
 * @{
 */

/** Reset disk system */
#define INT13_RESET			0x00
/** Get status of last operation */
#define INT13_GET_LAST_STATUS		0x01
/** Read sectors */
#define INT13_READ_SECTORS		0x02
/** Write sectors */
#define INT13_WRITE_SECTORS		0x03
/** Get drive parameters */
#define INT13_GET_PARAMETERS		0x08
/** Get disk type */
#define INT13_GET_DISK_TYPE		0x15
/** Extensions installation check */
#define INT13_EXTENSION_CHECK		0x41
/** Extended read */
#define INT13_EXTENDED_READ		0x42
/** Extended write */
#define INT13_EXTENDED_WRITE		0x43
/** Get extended drive parameters */
#define INT13_GET_EXTENDED_PARAMETERS	0x48
/** Get CD-ROM status / terminate emulation */
#define INT13_CDROM_STATUS_TERMINATE	0x4b

/** @} */

/**
 * @defgroup int13status INT 13 status codes
 * @{
 */

/** Operation completed successfully */
#define INT13_STATUS_SUCCESS		0x00
/** Invalid function or parameter */
#define INT13_STATUS_INVALID		0x01
/** Read error */
#define INT13_STATUS_READ_ERROR		0x04
/** Write error */
#define INT13_STATUS_WRITE_ERROR	0xcc

/** @} */

/** Block size for non-extended INT 13 calls */
#define INT13_BLKSIZE 512

/** An INT 13 emulated drive */
struct int13_drive {
	/** List of all registered drives */
	struct list_head list;

	/** Underlying block device */
	struct block_device *blockdev;

	/** BIOS in-use drive number (0x80-0xff) */
	unsigned int drive;
	/** BIOS natural drive number (0x80-0xff)
	 *
	 * This is the drive number that would have been assigned by
	 * 'naturally' appending the drive to the end of the BIOS
	 * drive list.
	 *
	 * If the emulated drive replaces a preexisting drive, this is
	 * the drive number that the preexisting drive gets remapped
	 * to.
	 */
	unsigned int natural_drive;

	/** Number of cylinders
	 *
	 * The cylinder number field in an INT 13 call is ten bits
	 * wide, giving a maximum of 1024 cylinders.  Conventionally,
	 * when the 7.8GB limit of a CHS address is exceeded, it is
	 * the number of cylinders that is increased beyond the
	 * addressable limit.
	 */
	unsigned int cylinders;
	/** Number of heads
	 *
	 * The head number field in an INT 13 call is eight bits wide,
	 * giving a maximum of 256 heads.  However, apparently all
	 * versions of MS-DOS up to and including Win95 fail with 256
	 * heads, so the maximum encountered in practice is 255.
	 */
	unsigned int heads;
	/** Number of sectors per track
	 *
	 * The sector number field in an INT 13 call is six bits wide,
	 * giving a maximum of 63 sectors, since sector numbering
	 * (unlike head and cylinder numbering) starts at 1, not 0.
	 */
	unsigned int sectors_per_track;

	/** Status of last operation */
	int last_status;
};

/** An INT 13 disk address packet */
struct int13_disk_address {
	/** Size of the packet, in bytes */
	uint8_t bufsize;
	/** Reserved, must be zero */
	uint8_t reserved;
	/** Block count */
	uint16_t count;
	/** Data buffer */
	struct segoff buffer;
	/** Starting block number */
	uint64_t lba;
	/** Data buffer (EDD-3.0 only) */
	uint64_t buffer_phys;
} __attribute__ (( packed ));

/** INT 13 disk parameters */
struct int13_disk_parameters {
	/** Size of this structure */
	uint16_t bufsize;
	/** Flags */
	uint16_t flags;
	/** Number of cylinders */
	uint32_t cylinders;
	/** Number of heads */
	uint32_t heads;
	/** Number of sectors per track */
	uint32_t sectors_per_track;
	/** Total number of sectors on drive */
	uint64_t sectors;
	/** Bytes per sector */
	uint16_t sector_size;
	
} __attribute__ (( packed ));

/**
 * @defgroup int13types INT 13 disk types
 * @{
 */

/** No such drive */
#define INT13_DISK_TYPE_NONE	0x00
/** Floppy without change-line support */
#define INT13_DISK_TYPE_FDD	0x01
/** Floppy with change-line support */
#define INT13_DISK_TYPE_FDD_CL	0x02
/** Hard disk */
#define INT13_DISK_TYPE_HDD	0x03

/** @} */

/**
 * @defgroup int13flags INT 13 disk parameter flags
 * @{
 */

/** DMA boundary errors handled transparently */
#define INT13_FL_DMA_TRANSPARENT 0x01
/** CHS information is valid */
#define INT13_FL_CHS_VALID	 0x02
/** Removable drive */
#define INT13_FL_REMOVABLE	 0x04
/** Write with verify supported */
#define INT13_FL_VERIFIABLE	 0x08
/** Has change-line supported (valid only for removable drives) */
#define INT13_FL_CHANGE_LINE	 0x10
/** Drive can be locked (valid only for removable drives) */
#define INT13_FL_LOCKABLE	 0x20
/** CHS is max possible, not current media (valid only for removable drives) */
#define INT13_FL_CHS_MAX	 0x40

/** @} */

/**
 * @defgroup int13exts INT 13 extension flags
 * @{
 */

/** Extended disk access functions supported */
#define INT13_EXTENSION_LINEAR		0x01
/** Removable drive functions supported */
#define INT13_EXTENSION_REMOVABLE	0x02
/** EDD functions supported */
#define INT13_EXTENSION_EDD		0x04

/** @} */

/**
 * @defgroup int13vers INT 13 extension versions
 * @{
 */

/** INT13 extensions version 1.x */
#define INT13_EXTENSION_VER_1_X		0x01
/** INT13 extensions version 2.0 (EDD-1.0) */
#define INT13_EXTENSION_VER_2_0		0x20
/** INT13 extensions version 2.1 (EDD-1.1) */
#define INT13_EXTENSION_VER_2_1		0x21
/** INT13 extensions version 3.0 (EDD-3.0) */
#define INT13_EXTENSION_VER_3_0		0x30

/** @} */ 

/** Bootable CD-ROM specification packet */
struct int13_cdrom_specification {
	/** Size of packet in bytes */
	uint8_t size;
	/** Boot media type */
	uint8_t media_type;
	/** Drive number */
	uint8_t drive;
	/** CD-ROM controller number */
	uint8_t controller;
	/** LBA of disk image to emulate */
	uint32_t lba;
	/** Device specification */
	uint16_t device;
	/** Segment of 3K buffer for caching CD-ROM reads */
	uint16_t cache_segment;
	/** Load segment for initial boot image */
	uint16_t load_segment;
	/** Number of 512-byte sectors to load */
	uint16_t load_sectors;
	/** Low 8 bits of cylinder number */
	uint8_t cyl;
	/** Sector number, plus high 2 bits of cylinder number */
	uint8_t cyl_sector;
	/** Head number */
	uint8_t head;
} __attribute__ (( packed ));

/** A C/H/S address within a partition table entry */
struct partition_chs {
	/** Head number */
	uint8_t head;
	/** Sector number, plus high 2 bits of cylinder number */
	uint8_t cyl_sector;
	/** Low 8 bits of cylinder number */
	uint8_t cyl;
} __attribute__ (( packed ));

#define PART_HEAD(chs) ( (chs).head )
#define PART_SECTOR(chs) ( (chs).cyl_sector & 0x3f )
#define PART_CYLINDER(chs) ( (chs).cyl | ( ( (chs).cyl_sector & 0xc0 ) << 2 ) )

/** A partition table entry within the MBR */
struct partition_table_entry {
	/** Bootable flag */
	uint8_t bootable;
	/** C/H/S start address */
	struct partition_chs chs_start;
	/** System indicator (partition type) */
	uint8_t type;
	/** C/H/S end address */
	struct partition_chs chs_end;
	/** Linear start address */
	uint32_t start;
	/** Linear length */
	uint32_t length;
} __attribute__ (( packed ));

/** A Master Boot Record */
struct master_boot_record {
	uint8_t pad[446];
	/** Partition table */
	struct partition_table_entry partitions[4];
	/** 0x55aa MBR signature */
	uint16_t signature;
} __attribute__ (( packed ));

extern void register_int13_drive ( struct int13_drive *drive );
extern void unregister_int13_drive ( struct int13_drive *drive );
extern int int13_boot ( unsigned int drive );

#endif /* INT13_H */
