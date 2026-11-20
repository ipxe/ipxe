/*
 * Copyright (C) 2026 Bejean Mosher <bejean.mosher@intel.com>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**
 * @file
 *
 * Linux bzImage image format for Universal Payload
 *
 * There are no real-mode services available to a universal payload,
 * so the kernel is started via the 32-bit boot protocol (as described
 * in Documentation/arch/x86/boot.rst), leaving long mode if necessary.
 * We construct the "zero page" ourselves and jump directly to the
 * protected-mode kernel entry point.
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <bzimage.h>
#include <ipxe/initrd.h>
#include <ipxe/uaccess.h>
#include <ipxe/image.h>
#include <ipxe/segment.h>
#include <ipxe/init.h>
#include <ipxe/memmap.h>
#include <ipxe/features.h>
#include <ipxe/upl.h>

FEATURE ( FEATURE_IMAGE, "bzImage", DHCP_EB_FEATURE_BZIMAGE, 1 );

/** Minimum supported boot protocol version (for cmdline_size) */
#define UPL_BZI_MIN_VERSION 0x0206

/** Boot protocol version providing init_size */
#define UPL_BZI_INIT_SIZE_VERSION 0x020a

/** Offset of setup header length byte (relative to 0x202) */
#define UPL_BZI_HDR_LEN_OFFSET 0x201

/** Offset of end of setup header length calculation */
#define UPL_BZI_HDR_LEN_BASE 0x202

/** Offset of init_size field within kernel image */
#define UPL_BZI_INIT_SIZE_OFFSET 0x260

/** Boot loader may not reload segment registers */
#define UPL_BZI_KEEP_SEGMENTS 0x40

/** Maximum number of E820 entries within the zero page */
#define UPL_E820_MAX 128

/* E820 memory types */
#define UPL_E820_RAM		1	/**< Usable RAM */
#define UPL_E820_RESERVED	2	/**< Reserved */
#define UPL_E820_ACPI		3	/**< ACPI reclaimable */
#define UPL_E820_NVS		4	/**< ACPI NVS */

/** Linux EFI framebuffer video type */
#define UPL_VIDEO_TYPE_EFI 0x70

/** Linux 64-bit framebuffer base address capability */
#define UPL_VIDEO_CAPABILITY_64BIT_BASE 0x00000002

/** Maximum command line length (including NUL) */
#define UPL_BZI_CMDLINE_MAX 4096

/** Linux screen_info structure */
struct upl_screen_info {
	uint8_t orig_x;			/* 0x00 */
	uint8_t orig_y;			/* 0x01 */
	uint16_t ext_mem_k;		/* 0x02 */
	uint16_t orig_video_page;	/* 0x04 */
	uint8_t orig_video_mode;	/* 0x06 */
	uint8_t orig_video_cols;	/* 0x07 */
	uint8_t flags;			/* 0x08 */
	uint8_t unused2;		/* 0x09 */
	uint16_t orig_video_ega_bx;	/* 0x0a */
	uint16_t unused3;		/* 0x0c */
	uint8_t orig_video_lines;	/* 0x0e */
	uint8_t orig_video_is_vga;	/* 0x0f */
	uint16_t orig_video_points;	/* 0x10 */
	uint16_t lfb_width;		/* 0x12 */
	uint16_t lfb_height;		/* 0x14 */
	uint16_t lfb_depth;		/* 0x16 */
	uint32_t lfb_base;		/* 0x18 */
	uint32_t lfb_size;		/* 0x1c */
	uint16_t cl_magic;		/* 0x20 */
	uint16_t cl_offset;		/* 0x22 */
	uint16_t lfb_linelength;	/* 0x24 */
	uint8_t red_size;		/* 0x26 */
	uint8_t red_pos;		/* 0x27 */
	uint8_t green_size;		/* 0x28 */
	uint8_t green_pos;		/* 0x29 */
	uint8_t blue_size;		/* 0x2a */
	uint8_t blue_pos;		/* 0x2b */
	uint8_t rsvd_size;		/* 0x2c */
	uint8_t rsvd_pos;		/* 0x2d */
	uint16_t vesapm_seg;		/* 0x2e */
	uint16_t vesapm_off;		/* 0x30 */
	uint16_t pages;			/* 0x32 */
	uint16_t vesa_attributes;	/* 0x34 */
	uint32_t capabilities;		/* 0x36 */
	uint32_t ext_lfb_base;		/* 0x3a */
	uint8_t reserved[2];		/* 0x3e */
} __attribute__ (( packed ));

/** Linux E820 memory map entry */
struct upl_e820_entry {
	/** Base address */
	uint64_t addr;
	/** Length */
	uint64_t size;
	/** Type */
	uint32_t type;
} __attribute__ (( packed ));

/** Linux boot parameters ("zero page") */
struct upl_boot_params {
	/** Screen information */
	struct upl_screen_info screen_info;		/* 0x000 */
	uint8_t pad1[ 0x070 - 0x040 ];
	/** ACPI RSDP address */
	uint64_t acpi_rsdp_addr;			/* 0x070 */
	uint8_t pad2[ 0x0c0 - 0x078 ];
	/** High 32 bits of initrd address */
	uint32_t ext_ramdisk_image;			/* 0x0c0 */
	/** High 32 bits of initrd size */
	uint32_t ext_ramdisk_size;			/* 0x0c4 */
	/** High 32 bits of command line address */
	uint32_t ext_cmd_line_ptr;			/* 0x0c8 */
	uint8_t pad3[ 0x1e8 - 0x0cc ];
	/** Number of E820 entries */
	uint8_t e820_entries;				/* 0x1e8 */
	uint8_t pad4[ 0x1f1 - 0x1e9 ];
	/** Setup header (copied from kernel image) */
	uint8_t hdr[ 0x2d0 - 0x1f1 ];			/* 0x1f1 */
	/** E820 memory map */
	struct upl_e820_entry e820_table[UPL_E820_MAX];	/* 0x2d0 */
	uint8_t pad5[ 0x1000 - 0xcd0 ];
} __attribute__ (( packed ));

static_assert ( sizeof ( struct upl_screen_info ) == 0x40 );
static_assert ( offsetof ( struct upl_boot_params, acpi_rsdp_addr ) == 0x070 );
static_assert ( offsetof ( struct upl_boot_params, e820_entries ) == 0x1e8 );
static_assert ( offsetof ( struct upl_boot_params, hdr ) == BZI_HDR_OFFSET );
static_assert ( offsetof ( struct upl_boot_params, e820_table ) == 0x2d0 );
static_assert ( sizeof ( struct upl_boot_params ) == 0x1000 );

/** Boot parameters
 *
 * These lie within the iPXE image itself, which is located well above
 * the kernel's own load and decompression area.
 */
static struct upl_boot_params upl_bzimage_params
	__attribute__ (( aligned ( 4096 ) ));

/** Kernel command line */
static char upl_bzimage_cmdline[UPL_BZI_CMDLINE_MAX];

/** bzImage context */
struct upl_bzimage_context {
	/** Boot protocol version */
	unsigned int version;
	/** Setup header length */
	size_t hdr_len;
	/** Real-mode portion file size */
	size_t rm_filesz;
	/** Protected-mode kernel load address */
	void *pm_kernel;
	/** Protected-mode kernel file size */
	size_t pm_filesz;
	/** Protected-mode kernel memory size (including decompression) */
	size_t pm_memsz;
	/** Command line maximum length (excluding NUL) */
	size_t cmdline_size;
	/** Highest legal initrd address */
	uint64_t mem_limit;
	/** Initrd address */
	void *initrd;
	/** Initrd size */
	size_t initrd_size;
};

/**
 * Parse bzImage header
 *
 * @v image		bzImage file
 * @v bzimg		bzImage context
 * @ret rc		Return status code
 */
static int upl_bzimage_parse_header ( struct image *image,
				      struct upl_bzimage_context *bzimg ) {
	const uint8_t *data = image->data;
	const struct bzimage_header *bzhdr;
	uint32_t init_size;
	size_t hdr_end;

	/* Initialise context */
	memset ( bzimg, 0, sizeof ( *bzimg ) );

	/* Sanity check */
	if ( image->len < ( BZI_HDR_OFFSET + sizeof ( *bzhdr ) ) ) {
		DBGC ( image, "bzImage %s too short for kernel header\n",
		       image->name );
		return -ENOEXEC;
	}
	bzhdr = ( image->data + BZI_HDR_OFFSET );

	/* Check signatures and version */
	if ( bzhdr->boot_flag != BZI_BOOT_FLAG ) {
		DBGC ( image, "bzImage %s missing 55AA signature\n",
		       image->name );
		return -ENOEXEC;
	}
	if ( bzhdr->header != BZI_SIGNATURE ) {
		DBGC ( image, "bzImage %s missing HdrS signature\n",
		       image->name );
		return -ENOEXEC;
	}
	bzimg->version = bzhdr->version;
	if ( bzimg->version < UPL_BZI_MIN_VERSION ) {
		DBGC ( image, "bzImage %s version %04x not supported\n",
		       image->name, bzimg->version );
		return -ENOTSUP;
	}
	if ( ! ( bzhdr->loadflags & BZI_LOAD_HIGH ) ) {
		DBGC ( image, "bzImage %s is not a high-loaded kernel\n",
		       image->name );
		return -ENOTSUP;
	}

	/* Calculate size of setup header */
	hdr_end = ( UPL_BZI_HDR_LEN_BASE + data[UPL_BZI_HDR_LEN_OFFSET] );
	if ( ( hdr_end < ( BZI_HDR_OFFSET + sizeof ( *bzhdr ) ) ) ||
	     ( hdr_end > ( BZI_HDR_OFFSET +
			   sizeof ( upl_bzimage_params.hdr ) ) ) ||
	     ( hdr_end > image->len ) ) {
		DBGC ( image, "bzImage %s bad setup header end %#zx\n",
		       image->name, hdr_end );
		return -ENOEXEC;
	}
	bzimg->hdr_len = ( hdr_end - BZI_HDR_OFFSET );

	/* Calculate size of real-mode portion */
	bzimg->rm_filesz = ( ( ( bzhdr->setup_sects ?
				 bzhdr->setup_sects : 4 ) + 1 ) << 9 );
	if ( bzimg->rm_filesz > image->len ) {
		DBGC ( image, "bzImage %s too short for %zd byte of setup\n",
		       image->name, bzimg->rm_filesz );
		return -ENOEXEC;
	}

	/* Calculate size of protected-mode portion */
	bzimg->pm_kernel = phys_to_virt ( BZI_LOAD_HIGH_ADDR );
	bzimg->pm_filesz = ( image->len - bzimg->rm_filesz );
	bzimg->pm_memsz = bzimg->pm_filesz;
	if ( ( bzimg->version >= UPL_BZI_INIT_SIZE_VERSION ) &&
	     ( image->len >= ( UPL_BZI_INIT_SIZE_OFFSET +
			       sizeof ( init_size ) ) ) ) {
		memcpy ( &init_size, ( data + UPL_BZI_INIT_SIZE_OFFSET ),
			 sizeof ( init_size ) );
		if ( init_size > bzimg->pm_memsz )
			bzimg->pm_memsz = init_size;
	}

	/* Extract memory and command line limits */
	bzimg->mem_limit = bzhdr->initrd_addr_max;
	bzimg->cmdline_size = bzhdr->cmdline_size;

	DBGC ( image, "bzImage %s version %04x PM %#lx+%#zx(%#zx) cmdlen "
	       "%zd\n", image->name, bzimg->version,
	       virt_to_phys ( bzimg->pm_kernel ), bzimg->pm_filesz,
	       bzimg->pm_memsz, bzimg->cmdline_size );

	return 0;
}

/**
 * Check that initrds can be loaded
 *
 * @v image		bzImage image
 * @v bzimg		bzImage context
 * @ret rc		Return status code
 */
static int upl_bzimage_check_initrds ( struct image *image,
				       struct upl_bzimage_context *bzimg ) {
	struct memmap_region region;
	uint64_t min;
	uint64_t max;
	uint64_t dest;
	int rc;

	/* Calculate total loaded length of initrds */
	bzimg->initrd_size = initrd_len();

	/* Succeed if there are no initrds */
	if ( ! bzimg->initrd_size )
		return 0;

	/* Calculate available load region after reshuffling */
	if ( ( rc = initrd_region ( bzimg->initrd_size, &region ) ) != 0 ) {
		DBGC ( image, "bzImage %s no region for initrds: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	/* Limit region to avoid kernel itself */
	min = ( virt_to_phys ( bzimg->pm_kernel ) + bzimg->pm_memsz );
	if ( min < region.min )
		min = region.min;

	/* Limit region to kernel's memory limit */
	max = region.max;
	if ( max > bzimg->mem_limit )
		max = bzimg->mem_limit;

	/* Calculate installation address */
	if ( max < ( bzimg->initrd_size - 1 ) ) {
		DBGC ( image, "bzImage %s not enough space for initrds\n",
		       image->name );
		return -ENOBUFS;
	}
	dest = ( ( max + 1 - bzimg->initrd_size ) & ~( INITRD_ALIGN - 1 ) );
	if ( dest < min ) {
		DBGC ( image, "bzImage %s not enough space for initrds\n",
		       image->name );
		return -ENOBUFS;
	}
	bzimg->initrd = phys_to_virt ( dest );

	DBGC ( image, "bzImage %s loading initrds from %#08llx downwards\n",
	       image->name, ( ( unsigned long long ) max ) );
	return 0;
}

/**
 * Load initrds, if any
 *
 * @v image		bzImage image
 * @v bzimg		bzImage context
 */
static void upl_bzimage_load_initrds ( struct image *image,
				       struct upl_bzimage_context *bzimg ) {
	size_t len;

	/* Do nothing if there are no initrds */
	if ( ! bzimg->initrd )
		return;

	/* Reshuffle initrds into desired order */
	initrd_reshuffle();

	/* Load initrds */
	DBGC ( image, "bzImage %s initrds at [%#08lx,%#08lx)\n",
	       image->name, virt_to_phys ( bzimg->initrd ),
	       ( virt_to_phys ( bzimg->initrd ) + bzimg->initrd_size ) );
	len = initrd_load_all ( bzimg->initrd );
	assert ( len == bzimg->initrd_size );
}

/**
 * Calculate position and size of a colour field from a bit mask
 *
 * @v mask		Bit mask
 * @v pos		Position to fill in
 * @v size		Size to fill in
 */
static void upl_bzimage_mask ( uint32_t mask, uint8_t *pos, uint8_t *size ) {

	*pos = 0;
	*size = 0;
	if ( ! mask )
		return;
	while ( ! ( mask & 1 ) ) {
		mask >>= 1;
		(*pos)++;
	}
	while ( mask & 1 ) {
		mask >>= 1;
		(*size)++;
	}
}

/**
 * Describe framebuffer to kernel
 *
 * @v image		bzImage image
 * @v si		Screen information to fill in
 */
static void upl_bzimage_screen_info ( struct image *image,
				      struct upl_screen_info *si ) {
	struct upl_framebuffer fb;
	unsigned int bpp;

	/* Locate frame buffer */
	if ( upl_framebuffer ( &fb ) != 0 ) {
		DBGC ( image, "bzImage %s has no framebuffer\n", image->name );
		return;
	}

	/* Describe pixel format */
	upl_bzimage_mask ( fb.red_mask, &si->red_pos, &si->red_size );
	upl_bzimage_mask ( fb.green_mask, &si->green_pos, &si->green_size );
	upl_bzimage_mask ( fb.blue_mask, &si->blue_pos, &si->blue_size );
	upl_bzimage_mask ( fb.reserved_mask, &si->rsvd_pos, &si->rsvd_size );
	bpp = ( si->red_size + si->green_size + si->blue_size +
		si->rsvd_size );

	/* Describe framebuffer */
	si->orig_video_is_vga = UPL_VIDEO_TYPE_EFI;
	si->lfb_width = fb.width;
	si->lfb_height = fb.height;
	si->lfb_depth = bpp;
	si->lfb_linelength = fb.stride;
	si->lfb_base = fb.base;
	si->ext_lfb_base = ( fb.base >> 32 );
	if ( si->ext_lfb_base )
		si->capabilities |= UPL_VIDEO_CAPABILITY_64BIT_BASE;
	si->lfb_size = fb.size;
	si->pages = 1;

	DBGC ( image, "bzImage %s framebuffer %dx%d at %#08llx\n",
	       image->name, fb.width, fb.height,
	       ( ( unsigned long long ) fb.base ) );
}

/**
 * Add E820 memory map entry
 *
 * @v image		bzImage image
 * @v params		Boot parameters
 * @v addr		Base address
 * @v size		Length
 * @v type		E820 type
 */
static void upl_bzimage_e820_add ( struct image *image,
				   struct upl_boot_params *params,
				   uint64_t addr, uint64_t size,
				   uint32_t type ) {
	struct upl_e820_entry *e820;

	if ( ! size )
		return;
	if ( params->e820_entries >= UPL_E820_MAX ) {
		DBGC ( image, "bzImage %s memory map truncated\n",
		       image->name );
		return;
	}
	e820 = &params->e820_table[ params->e820_entries++ ];
	e820->addr = addr;
	e820->size = size;
	e820->type = type;
}

/** An E820 memory map construction */
struct upl_bzimage_e820_ctx {
	/** bzImage image */
	struct image *image;
	/** Boot parameters */
	struct upl_boot_params *params;
};

/**
 * Add E820 memory map entry for device tree region
 *
 * @v ctx		E820 memory map construction
 * @v start		Starting address
 * @v size		Size
 * @v type		Memory type
 */
static void upl_bzimage_e820_region ( void *ctx, uint64_t start,
				      uint64_t size,
				      enum upl_memory_type type ) {
	struct upl_bzimage_e820_ctx *e820 = ctx;
	static const uint32_t types[] = {
		[UPL_MEM_RAM] = UPL_E820_RAM,
		[UPL_MEM_RESERVED] = UPL_E820_RESERVED,
		[UPL_MEM_BOOT] = 0,
		[UPL_MEM_ACPI] = UPL_E820_ACPI,
		[UPL_MEM_NVS] = UPL_E820_NVS,
	};

	/* Memory used only by the bootloader remains usable by the OS */
	if ( ! types[type] )
		return;
	upl_bzimage_e820_add ( e820->image, e820->params, start, size,
			       types[type] );
}

/**
 * Construct E820 memory map
 *
 * @v image		bzImage image
 * @v params		Boot parameters
 *
 * System memory regions provide the overall memory layout, and
 * reserved memory regions are overlaid on top.  The kernel resolves
 * overlapping entries in favour of the higher type.
 */
static void upl_bzimage_e820 ( struct image *image,
			       struct upl_boot_params *params ) {
	struct upl_bzimage_e820_ctx e820 = {
		.image = image,
		.params = params,
	};

	upl_fdt_memory ( upl_bzimage_e820_region, &e820 );
}

/**
 * Construct boot parameters
 *
 * @v image		bzImage image
 * @v bzimg		bzImage context
 * @v params		Boot parameters to fill in
 */
static void upl_bzimage_params_init ( struct image *image,
				      struct upl_bzimage_context *bzimg,
				      struct upl_boot_params *params ) {
	struct bzimage_header *bzhdr;

	/* Copy setup header from kernel image */
	memset ( params, 0, sizeof ( *params ) );
	memcpy ( params->hdr, ( image->data + BZI_HDR_OFFSET ),
		 bzimg->hdr_len );
	bzhdr = ( ( struct bzimage_header * ) params->hdr );

	/* Fill in setup header fields owned by the boot loader */
	bzhdr->type_of_loader = BZI_LOADER_TYPE_IPXE;
	bzhdr->vid_mode = BZI_VID_MODE_NORMAL;
	bzhdr->loadflags &= ~UPL_BZI_KEEP_SEGMENTS;
	bzhdr->cmd_line_ptr = virt_to_phys ( upl_bzimage_cmdline );
	bzhdr->ramdisk_image = ( bzimg->initrd ?
				 virt_to_phys ( bzimg->initrd ) : 0 );
	bzhdr->ramdisk_size = bzimg->initrd_size;

	/* Fill in platform information */
	upl_bzimage_screen_info ( image, &params->screen_info );
	params->acpi_rsdp_addr = upl_acpi_rsdp();
	upl_bzimage_e820 ( image, params );
}

/**
 * Execute bzImage image
 *
 * @v image		bzImage image
 * @ret rc		Return status code
 */
static int upl_bzimage_exec ( struct image *image ) {
	struct upl_bzimage_context bzimg;
	const char *cmdline = ( image->cmdline ? image->cmdline : "" );
	size_t cmdline_len;
	int rc;

	/* Read and parse header from image */
	if ( ( rc = upl_bzimage_parse_header ( image, &bzimg ) ) != 0 )
		return rc;

	/* Prepare protected-mode kernel segment */
	if ( ( rc = prep_segment ( bzimg.pm_kernel, bzimg.pm_filesz,
				   bzimg.pm_memsz ) ) != 0 ) {
		DBGC ( image, "bzImage %s could not prepare PM segment: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	/* Check that command line fits */
	cmdline_len = strlen ( cmdline );
	if ( ( cmdline_len > bzimg.cmdline_size ) ||
	     ( cmdline_len >= sizeof ( upl_bzimage_cmdline ) ) ) {
		DBGC ( image, "bzImage %s command line too long (%zd)\n",
		       image->name, cmdline_len );
		return -ENOSPC;
	}

	/* Check that initrds can be loaded */
	if ( ( rc = upl_bzimage_check_initrds ( image, &bzimg ) ) != 0 )
		return rc;

	/* Remove kernel from image list (without invalidating image pointer) */
	unregister_image ( image_get ( image ) );

	/* Load protected-mode kernel */
	memcpy ( bzimg.pm_kernel, ( image->data + bzimg.rm_filesz ),
		 bzimg.pm_filesz );

	/* Store command line */
	memcpy ( upl_bzimage_cmdline, cmdline, ( cmdline_len + 1 ) );
	DBGC ( image, "bzImage %s command line \"%s\"\n",
	       image->name, upl_bzimage_cmdline );

	/* Prepare for exiting.  Must do this before loading initrds,
	 * since loading the initrds will corrupt the external heap.
	 */
	shutdown_boot();

	/* Construct boot parameters (while the kernel image is intact) */
	upl_bzimage_params_init ( image, &bzimg, &upl_bzimage_params );

	/* Load any initrds */
	upl_bzimage_load_initrds ( image, &bzimg );

	DBGC ( image, "bzImage %s jumping to PM kernel at %#08lx (params "
	       "%#08lx)\n", image->name, virt_to_phys ( bzimg.pm_kernel ),
	       virt_to_phys ( &upl_bzimage_params ) );

	/* Jump to the kernel */
	upl_jump_ia32 ( virt_to_phys ( bzimg.pm_kernel ),
			virt_to_phys ( &upl_bzimage_params ) );
}

/**
 * Probe bzImage image
 *
 * @v image		bzImage file
 * @ret rc		Return status code
 */
static int upl_bzimage_probe ( struct image *image ) {
	struct upl_bzimage_context bzimg;

	/* Read and parse header from image */
	return upl_bzimage_parse_header ( image, &bzimg );
}

/** Linux bzImage image type */
struct image_type upl_bzimage_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "bzImage",
	.probe = upl_bzimage_probe,
	.exec = upl_bzimage_exec,
};
