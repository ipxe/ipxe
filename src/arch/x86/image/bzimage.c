/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Linux bzImage image format
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <realmode.h>
#include <bzimage.h>
#include <ipxe/initrd.h>
#include <ipxe/uaccess.h>
#include <ipxe/image.h>
#include <ipxe/segment.h>
#include <ipxe/init.h>
#include <ipxe/cpio.h>
#include <ipxe/features.h>

FEATURE ( FEATURE_IMAGE, "bzImage", DHCP_EB_FEATURE_BZIMAGE, 1 );

/**
 * bzImage context
 */
struct bzimage_context {
	/** Boot protocol version */
	unsigned int version;
	/** Real-mode kernel portion load segment address */
	unsigned int rm_kernel_seg;
	/** Real-mode kernel portion load address */
	void *rm_kernel;
	/** Real-mode kernel portion file size */
	size_t rm_filesz;
	/** Real-mode heap top (offset from rm_kernel) */
	size_t rm_heap;
	/** Command line (offset from rm_kernel) */
	size_t rm_cmdline;
	/** Command line maximum length */
	size_t cmdline_size;
	/** Real-mode kernel portion total memory size */
	size_t rm_memsz;
	/** Non-real-mode kernel portion load address */
	void *pm_kernel;
	/** Non-real-mode kernel portion file and memory size */
	size_t pm_sz;
	/** Video mode */
	unsigned int vid_mode;
	/** Memory limit */
	uint64_t mem_limit;
	/** Initrd address */
	void *initrd;
	/** Initrd size */
	physaddr_t initrd_size;
};

/**
 * Parse bzImage header
 *
 * @v image		bzImage file
 * @v bzimg		bzImage context
 * @ret rc		Return status code
 */
static int bzimage_parse_header ( struct image *image,
				  struct bzimage_context *bzimg ) {
	const struct bzimage_header *bzhdr;
	unsigned int syssize;
	int is_bzimage;

	/* Initialise context */
	memset ( bzimg, 0, sizeof ( *bzimg ) );

	/* Sanity check */
	if ( image->len < ( BZI_HDR_OFFSET + sizeof ( *bzhdr ) ) ) {
		DBGC ( image, "bzImage %s too short for kernel header\n",
		       image->name );
		return -ENOEXEC;
	}
	bzhdr = ( image->data + BZI_HDR_OFFSET );

	/* Calculate size of real-mode portion */
	bzimg->rm_filesz = ( ( ( bzhdr->setup_sects ?
				 bzhdr->setup_sects : 4 ) + 1 ) << 9 );
	if ( bzimg->rm_filesz > image->len ) {
		DBGC ( image, "bzImage %s too short for %zd byte of setup\n",
		       image->name, bzimg->rm_filesz );
		return -ENOEXEC;
	}
	bzimg->rm_memsz = BZI_ASSUMED_RM_SIZE;

	/* Calculate size of protected-mode portion */
	bzimg->pm_sz = ( image->len - bzimg->rm_filesz );
	syssize = ( ( bzimg->pm_sz + 15 ) / 16 );

	/* Check for signatures and determine version */
	if ( bzhdr->boot_flag != BZI_BOOT_FLAG ) {
		DBGC ( image, "bzImage %s missing 55AA signature\n",
		       image->name );
		return -ENOEXEC;
	}
	if ( bzhdr->header == BZI_SIGNATURE ) {
		/* 2.00+ */
		bzimg->version = bzhdr->version;
	} else {
		/* Pre-2.00.  Check that the syssize field is correct,
		 * as a guard against accepting arbitrary binary data,
		 * since the 55AA check is pretty lax.  Note that the
		 * syssize field is unreliable for protocols between
		 * 2.00 and 2.03 inclusive, so we should not always
		 * check this field.
		 */
		bzimg->version = 0x0100;
		if ( bzhdr->syssize != syssize ) {
			DBGC ( image, "bzImage %s bad syssize %x (expected "
			       "%x)\n", image->name, bzhdr->syssize,
			       syssize );
			return -ENOEXEC;
		}
	}

	/* Determine image type */
	is_bzimage = ( ( bzimg->version >= 0x0200 ) ?
		       ( bzhdr->loadflags & BZI_LOAD_HIGH ) : 0 );

	/* Calculate load address of real-mode portion */
	bzimg->rm_kernel_seg = ( is_bzimage ? 0x1000 : 0x9000 );
	bzimg->rm_kernel = real_to_virt ( bzimg->rm_kernel_seg, 0 );

	/* Allow space for the stack and heap */
	bzimg->rm_memsz += BZI_STACK_SIZE;
	bzimg->rm_heap = bzimg->rm_memsz;

	/* Allow space for the command line */
	bzimg->rm_cmdline = bzimg->rm_memsz;
	bzimg->rm_memsz += BZI_CMDLINE_SIZE;

	/* Calculate load address of protected-mode portion */
	bzimg->pm_kernel = phys_to_virt ( is_bzimage ? BZI_LOAD_HIGH_ADDR
					: BZI_LOAD_LOW_ADDR );

	/* Extract video mode */
	bzimg->vid_mode = bzhdr->vid_mode;

	/* Extract memory limit */
	bzimg->mem_limit = ( ( bzimg->version >= 0x0203 ) ?
			     bzhdr->initrd_addr_max : BZI_INITRD_MAX );

	/* Extract command line size */
	bzimg->cmdline_size = ( ( bzimg->version >= 0x0206 ) ?
				bzhdr->cmdline_size : BZI_CMDLINE_SIZE );

	DBGC ( image, "bzImage %s version %04x RM %#lx+%#zx PM %#lx+%#zx "
	       "cmdlen %zd\n", image->name, bzimg->version,
	       virt_to_phys ( bzimg->rm_kernel ), bzimg->rm_filesz,
	       virt_to_phys ( bzimg->pm_kernel ), bzimg->pm_sz,
	       bzimg->cmdline_size );

	return 0;
}

/**
 * Update bzImage header in loaded kernel
 *
 * @v image		bzImage file
 * @v bzimg		bzImage context
 */
static void bzimage_update_header ( struct image *image,
				    struct bzimage_context *bzimg ) {
	struct bzimage_header *bzhdr = ( bzimg->rm_kernel + BZI_HDR_OFFSET );
	struct bzimage_cmdline *cmdline;

	/* Set loader type */
	if ( bzimg->version >= 0x0200 )
		bzhdr->type_of_loader = BZI_LOADER_TYPE_IPXE;

	/* Set heap end pointer */
	if ( bzimg->version >= 0x0201 ) {
		bzhdr->heap_end_ptr = ( bzimg->rm_heap - 0x200 );
		bzhdr->loadflags |= BZI_CAN_USE_HEAP;
	}

	/* Set command line */
	if ( bzimg->version >= 0x0202 ) {
		bzhdr->cmd_line_ptr = ( virt_to_phys ( bzimg->rm_kernel )
					+ bzimg->rm_cmdline );
	} else {
		cmdline = ( bzimg->rm_kernel + BZI_CMDLINE_OFFSET );
		cmdline->magic = BZI_CMDLINE_MAGIC;
		cmdline->offset = bzimg->rm_cmdline;
		if ( bzimg->version >= 0x0200 )
			bzhdr->setup_move_size = bzimg->rm_memsz;
	}

	/* Set video mode */
	bzhdr->vid_mode = bzimg->vid_mode;
	DBGC ( image, "bzImage %s vidmode %d\n",
	       image->name, bzhdr->vid_mode );

	/* Set initrd address */
	if ( bzimg->version >= 0x0200 ) {
		bzhdr->ramdisk_image = virt_to_phys ( bzimg->initrd );
		bzhdr->ramdisk_size = bzimg->initrd_size;
	}
}

/**
 * Parse kernel command line for bootloader parameters
 *
 * @v image		bzImage file
 * @v bzimg		bzImage context
 * @ret rc		Return status code
 */
static int bzimage_parse_cmdline ( struct image *image,
				   struct bzimage_context *bzimg ) {
	const char *vga;
	const char *mem;
	char *sep;
	char *end;

	/* Look for "vga=" */
	if ( ( vga = image_argument ( image, "vga=" ) ) ) {
		sep = strchr ( vga, ' ' );
		if ( sep )
			*sep = '\0';
		if ( strcmp ( vga, "normal" ) == 0 ) {
			bzimg->vid_mode = BZI_VID_MODE_NORMAL;
		} else if ( strcmp ( vga, "ext" ) == 0 ) {
			bzimg->vid_mode = BZI_VID_MODE_EXT;
		} else if ( strcmp ( vga, "ask" ) == 0 ) {
			bzimg->vid_mode = BZI_VID_MODE_ASK;
		} else {
			bzimg->vid_mode = strtoul ( vga, &end, 0 );
			if ( *end ) {
				DBGC ( image, "bzImage %s strange \"vga=\" "
				       "terminator '%c'\n",
				       image->name, *end );
			}
		}
		if ( sep )
			*sep = ' ';
	}

	/* Look for "mem=" */
	if ( ( mem = image_argument ( image, "mem=" ) ) ) {
		bzimg->mem_limit = strtoul ( mem, &end, 0 );
		switch ( *end ) {
		case 'G':
		case 'g':
			bzimg->mem_limit <<= 10;
			/* Fall through */
		case 'M':
		case 'm':
			bzimg->mem_limit <<= 10;
			/* Fall through */
		case 'K':
		case 'k':
			bzimg->mem_limit <<= 10;
			break;
		case '\0':
		case ' ':
			break;
		default:
			DBGC ( image, "bzImage %s strange \"mem=\" "
			       "terminator '%c'\n", image->name, *end );
			break;
		}
		bzimg->mem_limit -= 1;
	}

	return 0;
}

/**
 * Set command line
 *
 * @v image		bzImage image
 * @v bzimg		bzImage context
 */
static void bzimage_set_cmdline ( struct image *image,
				  struct bzimage_context *bzimg ) {
	const char *cmdline = ( image->cmdline ? image->cmdline : "" );
	char *rm_cmdline;

	/* Copy command line down to real-mode portion */
	rm_cmdline = ( bzimg->rm_kernel + bzimg->rm_cmdline );
	snprintf ( rm_cmdline, bzimg->cmdline_size, "%s", cmdline );
	DBGC ( image, "bzImage %s command line \"%s\"\n",
	       image->name, rm_cmdline );
}

/**
 * Check that initrds can be loaded
 *
 * @v image		bzImage image
 * @v bzimg		bzImage context
 * @ret rc		Return status code
 */
static int bzimage_check_initrds ( struct image *image,
				   struct bzimage_context *bzimg ) {
	struct memmap_region region;
	physaddr_t min;
	physaddr_t max;
	physaddr_t dest;
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

	/* Limit region to avoiding kernel itself */
	min = virt_to_phys ( bzimg->pm_kernel + bzimg->pm_sz );
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

	DBGC ( image, "bzImage %s loading initrds from %#08lx downwards\n",
	       image->name, max );
	return 0;
}

/**
 * Load initrds, if any
 *
 * @v image		bzImage image
 * @v bzimg		bzImage context
 */
static void bzimage_load_initrds ( struct image *image,
				   struct bzimage_context *bzimg ) {
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
 * Execute bzImage image
 *
 * @v image		bzImage image
 * @ret rc		Return status code
 */
static int bzimage_exec ( struct image *image ) {
	struct bzimage_context bzimg;
	int rc;

	/* Read and parse header from image */
	if ( ( rc = bzimage_parse_header ( image, &bzimg ) ) != 0 )
		return rc;

	/* Prepare segments */
	if ( ( rc = prep_segment ( bzimg.rm_kernel, bzimg.rm_filesz,
				   bzimg.rm_memsz ) ) != 0 ) {
		DBGC ( image, "bzImage %s could not prepare RM segment: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}
	if ( ( rc = prep_segment ( bzimg.pm_kernel, bzimg.pm_sz,
				   bzimg.pm_sz ) ) != 0 ) {
		DBGC ( image, "bzImage %s could not prepare PM segment: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	/* Parse command line for bootloader parameters */
	if ( ( rc = bzimage_parse_cmdline ( image, &bzimg ) ) != 0)
		return rc;

	/* Check that initrds can be loaded */
	if ( ( rc = bzimage_check_initrds ( image, &bzimg ) ) != 0 )
		return rc;

	/* Remove kernel from image list (without invalidating image pointer) */
	unregister_image ( image_get ( image ) );

	/* Load segments */
	memcpy ( bzimg.rm_kernel, image->data, bzimg.rm_filesz );
	memcpy ( bzimg.pm_kernel, ( image->data + bzimg.rm_filesz ),
		 bzimg.pm_sz );

	/* Store command line */
	bzimage_set_cmdline ( image, &bzimg );

	/* Prepare for exiting.  Must do this before loading initrds,
	 * since loading the initrds will corrupt the external heap.
	 */
	shutdown_boot();

	/* Load any initrds */
	bzimage_load_initrds ( image, &bzimg );

	/* Update kernel header */
	bzimage_update_header ( image, &bzimg );

	DBGC ( image, "bzImage %s jumping to RM kernel at %04x:0000 (stack "
	       "%04x:%04zx)\n", image->name, ( bzimg.rm_kernel_seg + 0x20 ),
	       bzimg.rm_kernel_seg, bzimg.rm_heap );

	/* Jump to the kernel */
	__asm__ __volatile__ ( REAL_CODE ( "movw %w0, %%ds\n\t"
					   "movw %w0, %%es\n\t"
					   "movw %w0, %%fs\n\t"
					   "movw %w0, %%gs\n\t"
					   "movw %w0, %%ss\n\t"
					   "movw %w1, %%sp\n\t"
					   "pushw %w2\n\t"
					   "pushw $0\n\t"
					   "lret\n\t" )
			       : : "R" ( bzimg.rm_kernel_seg ),
			           "R" ( bzimg.rm_heap ),
			           "R" ( bzimg.rm_kernel_seg + 0x20 ) );

	/* There is no way for the image to return, since we provide
	 * no return address.
	 */
	assert ( 0 );

	return -ECANCELED; /* -EIMPOSSIBLE */
}

/**
 * Probe bzImage image
 *
 * @v image		bzImage file
 * @ret rc		Return status code
 */
int bzimage_probe ( struct image *image ) {
	struct bzimage_context bzimg;
	int rc;

	/* Read and parse header from image */
	if ( ( rc = bzimage_parse_header ( image, &bzimg ) ) != 0 )
		return rc;

	return 0;
}

/** Linux bzImage image type */
struct image_type bzimage_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "bzImage",
	.probe = bzimage_probe,
	.exec = bzimage_exec,
};
