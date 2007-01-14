#ifndef _BZIMAGE_H
#define _BZIMAGE_H

#include <stdint.h>

/**
 * A bzImage header
 *
 * As documented in Documentation/i386/boot.txt
 */
struct bzimage_header {
	/** The size of the setup in sectors
	 *
	 * If this field contains 0, assume it contains 4.
	 */
	uint8_t setup_sects;
	/** If set, the root is mounted readonly */
	uint16_t root_flags;
	/** DO NOT USE - for bootsect.S use only */
	uint16_t syssize;
	/** DO NOT USE - obsolete */
	uint16_t swap_dev;
	/** DO NOT USE - for bootsect.S use only */
	uint16_t ram_size;
	/** Video mode control */
	uint16_t vid_mode;
	/** Default root device number */
	uint16_t root_dev;
	/** 0xAA55 magic number */
	uint16_t boot_flag;
	/** Jump instruction */
	uint16_t jump;
	/** Magic signature "HdrS" */
	uint32_t header;
	/** Boot protocol version supported */
	uint16_t version;
	/** Boot loader hook (see below) */
	uint32_t realmode_swtch;
	/** The load-low segment (0x1000) (obsolete) */
	uint16_t start_sys;
	/** Pointer to kernel version string */
	uint16_t kernel_version;
	/** Boot loader identifier */
	uint8_t type_of_loader;
	/** Boot protocol option flags */
	uint8_t loadflags;
	/** Move to high memory size (used with hooks) */
	uint16_t setup_move_size;
	/** Boot loader hook (see below) */
	uint32_t code32_start;
	/** initrd load address (set by boot loader) */
	uint32_t ramdisk_image;
	/** initrd size (set by boot loader) */
	uint32_t ramdisk_size;
	/** DO NOT USE - for bootsect.S use only */
	uint32_t bootsect_kludge;
	/** Free memory after setup end */
	uint16_t heap_end_ptr;
	/** Unused */
	uint16_t pad1;
	/** 32-bit pointer to the kernel command line */
	uint32_t cmd_line_ptr;
	/** Highest legal initrd address */
	uint32_t initrd_addr_max;
} __attribute__ (( packed ));

/** Offset of bzImage header within kernel image */
#define BZHDR_OFFSET 0x1f1

/** bzImage magic signature value */
#define BZIMAGE_SIGNATURE 0x53726448

#endif /* _BZIMAGE_H */
