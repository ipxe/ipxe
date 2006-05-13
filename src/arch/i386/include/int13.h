#ifndef INT13_H
#define INT13_H

/** @file
 *
 * INT 13 emulation
 *
 */

#include <stdint.h>
#include <gpxe/list.h>

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
/** Extended read */
#define INT13_EXTENDED_READ		0x42
/** Extended write */
#define INT13_EXTENDED_WRITE		0x43
/** Get extended drive parameters */
#define INT13_GET_EXTENDED_PARAMETERS	0x48

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

	/** BIOS drive number (0x80-0xff) */
	unsigned int drive;
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
};

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
	
};

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

extern void register_int13_drive ( struct int13_drive *drive );
extern void unregister_int13_drive ( struct int13_drive *drive );

#endif /* INT13_H */
