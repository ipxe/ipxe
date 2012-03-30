#ifndef INT13_H
#define INT13_H

/** @file
 *
 * INT 13 emulation
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <ipxe/list.h>
#include <ipxe/edd.h>
#include <realmode.h>

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
/** Verify sectors */
#define INT13_EXTENDED_VERIFY		0x44
/** Extended seek */
#define INT13_EXTENDED_SEEK		0x47
/** Get extended drive parameters */
#define INT13_GET_EXTENDED_PARAMETERS	0x48
/** Get CD-ROM status / terminate emulation */
#define INT13_CDROM_STATUS_TERMINATE	0x4b
/** Read CD-ROM boot catalog */
#define INT13_CDROM_READ_BOOT_CATALOG	0x4d

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
/** Reset failed */
#define INT13_STATUS_RESET_FAILED	0x05
/** Write error */
#define INT13_STATUS_WRITE_ERROR	0xcc

/** @} */

/** Block size for non-extended INT 13 calls */
#define INT13_BLKSIZE 512

/** @defgroup int13fddtype INT 13 floppy disk drive types
 * @{
 */

/** 360K */
#define INT13_FDD_TYPE_360K		0x01
/** 1.2M */
#define INT13_FDD_TYPE_1M2		0x02
/** 720K */
#define INT13_FDD_TYPE_720K		0x03
/** 1.44M */
#define INT13_FDD_TYPE_1M44		0x04

/** An INT 13 disk address packet */
struct int13_disk_address {
	/** Size of the packet, in bytes */
	uint8_t bufsize;
	/** Reserved */
	uint8_t reserved_a;
	/** Block count */
	uint8_t count;
	/** Reserved */
	uint8_t reserved_b;
	/** Data buffer */
	struct segoff buffer;
	/** Starting block number */
	uint64_t lba;
	/** Data buffer (EDD 3.0+ only) */
	uint64_t buffer_phys;
	/** Block count (EDD 4.0+ only) */
	uint32_t long_count;
	/** Reserved */
	uint32_t reserved_c;
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
	/** Device parameter table extension */
	struct segoff dpte;
	/** Device path information */
	struct edd_device_path_information dpi;
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
/** 64-bit extensions are present */
#define INT13_EXTENSION_64BIT		0x08

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

/** Maximum number of sectors for which CHS geometry is allowed to be valid
 *
 * This number is taken from the EDD specification.
 */
#define INT13_MAX_CHS_SECTORS		15482880

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

/** Bootable CD-ROM boot catalog command packet */
struct int13_cdrom_boot_catalog_command {
	/** Size of packet in bytes */
	uint8_t size;
	/** Number of sectors of boot catalog to read */
	uint8_t count;
	/** Buffer for boot catalog */
	uint32_t buffer;
	/** First sector in boot catalog to transfer */
	uint16_t start;
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
	/** Code area */
	uint8_t code[440];
	/** Disk signature */
	uint32_t signature;
	/** Padding */
	uint8_t pad[2];
	/** Partition table */
	struct partition_table_entry partitions[4];
	/** 0x55aa MBR signature */
	uint16_t magic;
} __attribute__ (( packed ));

/** MBR magic signature */
#define INT13_MBR_MAGIC 0xaa55

/** ISO9660 block size */
#define ISO9660_BLKSIZE 2048

/** An ISO9660 Primary Volume Descriptor (fixed portion) */
struct iso9660_primary_descriptor_fixed {
	/** Descriptor type */
	uint8_t type;
	/** Identifier ("CD001") */
	uint8_t id[5];
} __attribute__ (( packed ));

/** An ISO9660 Primary Volume Descriptor */
struct iso9660_primary_descriptor {
	/** Fixed portion */
	struct iso9660_primary_descriptor_fixed fixed;
} __attribute__ (( packed ));

/** ISO9660 Primary Volume Descriptor type */
#define ISO9660_TYPE_PRIMARY 0x01

/** ISO9660 identifier */
#define ISO9660_ID "CD001"

/** ISO9660 Primary Volume Descriptor block address */
#define ISO9660_PRIMARY_LBA 16

/** An El Torito Boot Record Volume Descriptor (fixed portion) */
struct eltorito_descriptor_fixed {
	/** Descriptor type */
	uint8_t type;
	/** Identifier ("CD001") */
	uint8_t id[5];
	/** Version, must be 1 */
	uint8_t version;
	/** Boot system indicator; must be "EL TORITO SPECIFICATION" */
	uint8_t system_id[32];
} __attribute__ (( packed ));

/** An El Torito Boot Record Volume Descriptor */
struct eltorito_descriptor {
	/** Fixed portion */
	struct eltorito_descriptor_fixed fixed;
	/** Unused */
	uint8_t unused[32];
	/** Boot catalog sector */
	uint32_t sector;
} __attribute__ (( packed ));

/** ISO9660 Boot Volume Descriptor type */
#define ISO9660_TYPE_BOOT 0x00

/** El Torito Boot Record Volume Descriptor block address */
#define ELTORITO_LBA 17

/** An El Torito Boot Catalog Validation Entry */
struct eltorito_validation_entry {
	/** Header ID; must be 1 */
	uint8_t header_id;
	/** Platform ID
	 *
	 * 0 = 80x86
	 * 1 = PowerPC
	 * 2 = Mac
	 */
	uint8_t platform_id;
	/** Reserved */
	uint16_t reserved;
	/** ID string */
	uint8_t id_string[24];
	/** Checksum word */
	uint16_t checksum;
	/** Signature; must be 0xaa55 */
	uint16_t signature;
} __attribute__ (( packed ));

/** El Torito platform IDs */
enum eltorito_platform_id {
	ELTORITO_PLATFORM_X86 = 0x00,
	ELTORITO_PLATFORM_POWERPC = 0x01,
	ELTORITO_PLATFORM_MAC = 0x02,
};

/** A bootable entry in the El Torito Boot Catalog */
struct eltorito_boot_entry {
	/** Boot indicator
	 *
	 * Must be @c ELTORITO_BOOTABLE for a bootable ISO image
	 */
	uint8_t indicator;
	/** Media type
	 *
	 */
	uint8_t media_type;
	/** Load segment */
	uint16_t load_segment;
	/** System type */
	uint8_t filesystem;
	/** Unused */
	uint8_t reserved_a;
	/** Sector count */
	uint16_t length;
	/** Starting sector */
	uint32_t start;
	/** Unused */
	uint8_t reserved_b[20];
} __attribute__ (( packed ));

/** Boot indicator for a bootable ISO image */
#define ELTORITO_BOOTABLE 0x88

/** El Torito media types */
enum eltorito_media_type {
	/** No emulation */
	ELTORITO_NO_EMULATION = 0,
};

/** A floppy disk geometry */
struct int13_fdd_geometry {
	/** Number of tracks */
	uint8_t tracks;
	/** Number of heads and sectors per track */
	uint8_t heads_spt;
};

/** Define a floppy disk geometry */
#define INT13_FDD_GEOMETRY( cylinders, heads, sectors )			\
	{								\
		.tracks = (cylinders),					\
		.heads_spt = ( ( (heads) << 6 ) | (sectors) ),		\
	}

/** Get floppy disk number of cylinders */
#define INT13_FDD_CYLINDERS( geometry ) ( (geometry)->tracks )

/** Get floppy disk number of heads */
#define INT13_FDD_HEADS( geometry ) ( (geometry)->heads_spt >> 6 )

/** Get floppy disk number of sectors per track */
#define INT13_FDD_SECTORS( geometry ) ( (geometry)->heads_spt & 0x3f )

/** A floppy drive parameter table */
struct int13_fdd_parameters {
	uint8_t step_rate__head_unload;
	uint8_t head_load__ndma;
	uint8_t motor_off_delay;
	uint8_t bytes_per_sector;
	uint8_t sectors_per_track;
	uint8_t gap_length;
	uint8_t data_length;
	uint8_t format_gap_length;
	uint8_t format_filler;
	uint8_t head_settle_time;
	uint8_t motor_start_time;
} __attribute__ (( packed ));

#endif /* INT13_H */
