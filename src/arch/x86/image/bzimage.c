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
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <realmode.h>
#include <bzimage.h>
#include <initrd.h>
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
	userptr_t rm_kernel;
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
	userptr_t pm_kernel;
	/** Non-real-mode kernel portion file and memory size */
	size_t pm_sz;
	/** Video mode */
	unsigned int vid_mode;
	/** Memory limit */
	uint64_t mem_limit;
	/** Initrd address */
	physaddr_t ramdisk_image;
	/** Initrd size */
	physaddr_t ramdisk_size;

	/** Command line magic block */
	struct bzimage_cmdline cmdline_magic;
	/** bzImage header */
	struct bzimage_header bzhdr;
};

/**
 * Parse bzImage header
 *
 * @v image		bzImage file
 * @v bzimg		bzImage context
 * @v src		bzImage to parse
 * @ret rc		Return status code
 */
static int bzimage_parse_header ( struct image *image,
				  struct bzimage_context *bzimg,
				  userptr_t src ) {
	unsigned int syssize;
	int is_bzimage;

	/* Sanity check */
	if ( image->len < ( BZI_HDR_OFFSET + sizeof ( bzimg->bzhdr ) ) ) {
		DBGC ( image, "bzImage %p too short for kernel header\n",
		       image );
		return -ENOEXEC;
	}

	/* Read in header structures */
	memset ( bzimg, 0, sizeof ( *bzimg ) );
	copy_from_user ( &bzimg->cmdline_magic, src, BZI_CMDLINE_OFFSET,
			 sizeof ( bzimg->cmdline_magic ) );
	copy_from_user ( &bzimg->bzhdr, src, BZI_HDR_OFFSET,
			 sizeof ( bzimg->bzhdr ) );

	/* Calculate size of real-mode portion */
	bzimg->rm_filesz = ( ( ( bzimg->bzhdr.setup_sects ?
				 bzimg->bzhdr.setup_sects : 4 ) + 1 ) << 9 );
	if ( bzimg->rm_filesz > image->len ) {
		DBGC ( image, "bzImage %p too short for %zd byte of setup\n",
		       image, bzimg->rm_filesz );
		return -ENOEXEC;
	}
	bzimg->rm_memsz = BZI_ASSUMED_RM_SIZE;

	/* Calculate size of protected-mode portion */
	bzimg->pm_sz = ( image->len - bzimg->rm_filesz );
	syssize = ( ( bzimg->pm_sz + 15 ) / 16 );

	/* Check for signatures and determine version */
	if ( bzimg->bzhdr.boot_flag != BZI_BOOT_FLAG ) {
		DBGC ( image, "bzImage %p missing 55AA signature\n", image );
		return -ENOEXEC;
	}
	if ( bzimg->bzhdr.header == BZI_SIGNATURE ) {
		/* 2.00+ */
		bzimg->version = bzimg->bzhdr.version;
	} else {
		/* Pre-2.00.  Check that the syssize field is correct,
		 * as a guard against accepting arbitrary binary data,
		 * since the 55AA check is pretty lax.  Note that the
		 * syssize field is unreliable for protocols between
		 * 2.00 and 2.03 inclusive, so we should not always
		 * check this field.
		 */
		bzimg->version = 0x0100;
		if ( bzimg->bzhdr.syssize != syssize ) {
			DBGC ( image, "bzImage %p bad syssize %x (expected "
			       "%x)\n", image, bzimg->bzhdr.syssize, syssize );
			return -ENOEXEC;
		}
	}

	/* Determine image type */
	is_bzimage = ( ( bzimg->version >= 0x0200 ) ?
		       ( bzimg->bzhdr.loadflags & BZI_LOAD_HIGH ) : 0 );

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
	bzimg->vid_mode = bzimg->bzhdr.vid_mode;

	/* Extract memory limit */
	bzimg->mem_limit = ( ( bzimg->version >= 0x0203 ) ?
			     bzimg->bzhdr.initrd_addr_max : BZI_INITRD_MAX );

	/* Extract command line size */
	bzimg->cmdline_size = ( ( bzimg->version >= 0x0206 ) ?
				bzimg->bzhdr.cmdline_size : BZI_CMDLINE_SIZE );

	DBGC ( image, "bzImage %p version %04x RM %#lx+%#zx PM %#lx+%#zx "
	       "cmdlen %zd\n", image, bzimg->version,
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
 * @v dst		bzImage to update
 */
static void bzimage_update_header ( struct image *image,
				    struct bzimage_context *bzimg,
				    userptr_t dst ) {

	/* Set loader type */
	if ( bzimg->version >= 0x0200 )
		bzimg->bzhdr.type_of_loader = BZI_LOADER_TYPE_IPXE;

	/* Set heap end pointer */
	if ( bzimg->version >= 0x0201 ) {
		bzimg->bzhdr.heap_end_ptr = ( bzimg->rm_heap - 0x200 );
		bzimg->bzhdr.loadflags |= BZI_CAN_USE_HEAP;
	}

	/* Set command line */
	if ( bzimg->version >= 0x0202 ) {
		bzimg->bzhdr.cmd_line_ptr = ( virt_to_phys ( bzimg->rm_kernel )
					      + bzimg->rm_cmdline );
	} else {
		bzimg->cmdline_magic.magic = BZI_CMDLINE_MAGIC;
		bzimg->cmdline_magic.offset = bzimg->rm_cmdline;
		if ( bzimg->version >= 0x0200 )
			bzimg->bzhdr.setup_move_size = bzimg->rm_memsz;
	}

	/* Set video mode */
	bzimg->bzhdr.vid_mode = bzimg->vid_mode;

	/* Set initrd address */
	if ( bzimg->version >= 0x0200 ) {
		bzimg->bzhdr.ramdisk_image = bzimg->ramdisk_image;
		bzimg->bzhdr.ramdisk_size = bzimg->ramdisk_size;
	}

	/* Write out header structures */
	copy_to_user ( dst, BZI_CMDLINE_OFFSET, &bzimg->cmdline_magic,
		       sizeof ( bzimg->cmdline_magic ) );
	copy_to_user ( dst, BZI_HDR_OFFSET, &bzimg->bzhdr,
		       sizeof ( bzimg->bzhdr ) );

	DBGC ( image, "bzImage %p vidmode %d\n", image, bzimg->vid_mode );
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
				DBGC ( image, "bzImage %p strange \"vga=\" "
				       "terminator '%c'\n", image, *end );
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
			DBGC ( image, "bzImage %p strange \"mem=\" "
			       "terminator '%c'\n", image, *end );
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
	size_t cmdline_len;

	/* Copy command line down to real-mode portion */
	cmdline_len = ( strlen ( cmdline ) + 1 );
	if ( cmdline_len > bzimg->cmdline_size )
		cmdline_len = bzimg->cmdline_size;
	copy_to_user ( bzimg->rm_kernel, bzimg->rm_cmdline,
		       cmdline, cmdline_len );
	DBGC ( image, "bzImage %p command line \"%s\"\n", image, cmdline );
}

/**
 * Align initrd length
 *
 * @v len		Length
 * @ret len		Length rounded up to INITRD_ALIGN
 */
static inline size_t bzimage_align ( size_t len ) {

	return ( ( len + INITRD_ALIGN - 1 ) & ~( INITRD_ALIGN - 1 ) );
}

/**
 * Load initrd
 *
 * @v image		bzImage image
 * @v initrd		initrd image
 * @v address		Address at which to load, or UNULL
 * @ret len		Length of loaded image, excluding zero-padding
 */
static size_t bzimage_load_initrd ( struct image *image,
				    struct image *initrd,
				    userptr_t address ) {
	const char *filename = cpio_name ( initrd );
	struct cpio_header cpio;
	size_t offset;
	size_t cpio_len;
	size_t pad_len;
	size_t len;
	unsigned int i;

	/* Skip hidden images */
	if ( initrd->flags & IMAGE_HIDDEN )
		return 0;

	/* Determine length of cpio headers for non-prebuilt images */
	len = 0;
	for ( i = 0 ; ( cpio_len = cpio_header ( initrd, i, &cpio ) ) ; i++ )
		len += ( cpio_len + cpio_pad_len ( cpio_len ) );

	/* Copy in initrd image body and construct any cpio headers */
	if ( address ) {
		memmove ( ( address + len ), initrd->data, initrd->len );
		memset ( address, 0, len );
		offset = 0;
		for ( i = 0 ; ( cpio_len = cpio_header ( initrd, i, &cpio ) ) ;
		      i++ ) {
			copy_to_user ( address, offset, &cpio,
				       sizeof ( cpio ) );
			copy_to_user ( address, ( offset + sizeof ( cpio ) ),
				       filename,
				       ( cpio_len - sizeof ( cpio ) ) );
			offset += ( cpio_len + cpio_pad_len ( cpio_len ) );
		}
		assert ( offset == len );
		DBGC ( image, "bzImage %p initrd %p [%#08lx,%#08lx,%#08lx)"
		       "%s%s\n", image, initrd, virt_to_phys ( address ),
		       ( virt_to_phys ( address ) + offset ),
		       ( virt_to_phys ( address ) + offset + initrd->len ),
		       ( filename ? " " : "" ), ( filename ? filename : "" ) );
		DBGC2_MD5A ( image, ( virt_to_phys ( address ) + offset ),
			     ( address + offset ), initrd->len );
	}
	len += initrd->len;

	/* Zero-pad to next INITRD_ALIGN boundary */
	pad_len = ( ( -len ) & ( INITRD_ALIGN - 1 ) );
	if ( address )
		memset ( ( address + len ), 0, pad_len );

	return len;
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
	struct image *initrd;
	userptr_t bottom;
	size_t len = 0;
	int rc;

	/* Calculate total loaded length of initrds */
	for_each_image ( initrd ) {

		/* Calculate length */
		len += bzimage_load_initrd ( image, initrd, UNULL );
		len = bzimage_align ( len );

		DBGC ( image, "bzImage %p initrd %p from [%#08lx,%#08lx)%s%s\n",
		       image, initrd, virt_to_phys ( initrd->data ),
		       ( virt_to_phys ( initrd->data ) + initrd->len ),
		       ( initrd->cmdline ? " " : "" ),
		       ( initrd->cmdline ? initrd->cmdline : "" ) );
		DBGC2_MD5A ( image, virt_to_phys ( initrd->data ),
			     initrd->data, initrd->len );
	}

	/* Calculate lowest usable address */
	bottom = ( bzimg->pm_kernel + bzimg->pm_sz );

	/* Check that total length fits within space available for
	 * reshuffling.  This is a conservative check, since CPIO
	 * headers are not present during reshuffling, but this
	 * doesn't hurt and keeps the code simple.
	 */
	if ( ( rc = initrd_reshuffle_check ( len, bottom ) ) != 0 ) {
		DBGC ( image, "bzImage %p failed reshuffle check: %s\n",
		       image, strerror ( rc ) );
		return rc;
	}

	/* Check that total length fits within kernel's memory limit */
	if ( ( virt_to_phys ( bottom ) + len ) > bzimg->mem_limit ) {
		DBGC ( image, "bzImage %p not enough space for initrds\n",
		       image );
		return -ENOBUFS;
	}

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
	struct image *initrd;
	struct image *highest = NULL;
	struct image *other;
	userptr_t top;
	userptr_t dest;
	size_t offset;
	size_t len;

	/* Reshuffle initrds into desired order */
	initrd_reshuffle ( bzimg->pm_kernel + bzimg->pm_sz );

	/* Find highest initrd */
	for_each_image ( initrd ) {
		if ( ( highest == NULL ) || ( initrd->data > highest->data ) )
			highest = initrd;
	}

	/* Do nothing if there are no initrds */
	if ( ! highest )
		return;

	/* Find highest usable address */
	top = ( highest->data + bzimage_align ( highest->len ) );
	if ( ( virt_to_phys ( top ) - 1UL ) > bzimg->mem_limit ) {
		top = phys_to_virt ( ( bzimg->mem_limit + 1 ) &
				     ~( INITRD_ALIGN - 1 ) );
	}
	DBGC ( image, "bzImage %p loading initrds from %#08lx downwards\n",
	       image, ( virt_to_phys ( top ) - 1UL ) );

	/* Load initrds in order */
	for_each_image ( initrd ) {

		/* Calculate cumulative length of following
		 * initrds (including padding).
		 */
		offset = 0;
		for_each_image ( other ) {
			if ( other == initrd )
				offset = 0;
			offset += bzimage_load_initrd ( image, other, UNULL );
			offset = bzimage_align ( offset );
		}

		/* Load initrd at this address */
		dest = ( top - offset );
		len = bzimage_load_initrd ( image, initrd, dest );

		/* Record initrd location */
		if ( ! bzimg->ramdisk_image )
			bzimg->ramdisk_image = virt_to_phys ( dest );
		bzimg->ramdisk_size = ( virt_to_phys ( dest ) + len -
					bzimg->ramdisk_image );
	}
	DBGC ( image, "bzImage %p initrds at [%#08lx,%#08lx)\n",
	       image, bzimg->ramdisk_image,
	       ( bzimg->ramdisk_image + bzimg->ramdisk_size ) );
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
	if ( ( rc = bzimage_parse_header ( image, &bzimg,
					   image->data ) ) != 0 )
		return rc;

	/* Prepare segments */
	if ( ( rc = prep_segment ( bzimg.rm_kernel, bzimg.rm_filesz,
				   bzimg.rm_memsz ) ) != 0 ) {
		DBGC ( image, "bzImage %p could not prepare RM segment: %s\n",
		       image, strerror ( rc ) );
		return rc;
	}
	if ( ( rc = prep_segment ( bzimg.pm_kernel, bzimg.pm_sz,
				   bzimg.pm_sz ) ) != 0 ) {
		DBGC ( image, "bzImage %p could not prepare PM segment: %s\n",
		       image, strerror ( rc ) );
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
	bzimage_update_header ( image, &bzimg, bzimg.rm_kernel );

	DBGC ( image, "bzImage %p jumping to RM kernel at %04x:0000 "
	       "(stack %04x:%04zx)\n", image, ( bzimg.rm_kernel_seg + 0x20 ),
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
	if ( ( rc = bzimage_parse_header ( image, &bzimg,
					   image->data ) ) != 0 )
		return rc;

	return 0;
}

/** Linux bzImage image type */
struct image_type bzimage_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "bzImage",
	.probe = bzimage_probe,
	.exec = bzimage_exec,
};
