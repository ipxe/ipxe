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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**
 * @file
 *
 * Linux bzImage image format
 *
 */

#include <errno.h>
#include <assert.h>
#include <realmode.h>
#include <bzimage.h>
#include <gpxe/uaccess.h>
#include <gpxe/image.h>
#include <gpxe/segment.h>
#include <gpxe/memmap.h>
#include <gpxe/shutdown.h>

struct image_type bzimage_image_type __image_type ( PROBE_NORMAL );

/**
 * bzImage load context
 */
struct bzimage_load_context {
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
	/** Real-mode kernel portion total memory size */
	size_t rm_memsz;
	/** Non-real-mode kernel portion load address */
	userptr_t pm_kernel;
	/** Non-real-mode kernel portion file and memory size */
	size_t pm_sz;
};

/**
 * bzImage execution context
 */
struct bzimage_exec_context {
	/** Kernel real-mode data segment */
	uint16_t kernel_seg;
	/** Kernel real-mode stack pointer */
	uint16_t stack;
};

/**
 * Execute bzImage image
 *
 * @v image		bzImage image
 * @ret rc		Return status code
 */
static int bzimage_exec ( struct image *image ) {
	union {
		struct bzimage_exec_context bz;
		unsigned long ul;
	} exec_ctx;

	/* Retrieve stored execution context */
	exec_ctx.ul = image->priv.ul;

	/* Prepare for exiting */
	shutdown();

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
			       : : "r" ( exec_ctx.bz.kernel_seg ),
			           "r" ( exec_ctx.bz.stack ),
			           "r" ( exec_ctx.bz.kernel_seg + 0x20 ) );

	/* There is no way for the image to return, since we provide
	 * no return address.
	 */
	assert ( 0 );

	return -ECANCELED; /* -EIMPOSSIBLE */
}

/**
 * Load and parse bzImage header
 *
 * @v image		bzImage file
 * @v load_ctx		Load context
 * @v bzhdr		Buffer for bzImage header
 * @ret rc		Return status code
 */
static int bzimage_load_header ( struct image *image,
				 struct bzimage_load_context *load_ctx,
				 struct bzimage_header *bzhdr ) {

	/* Sanity check */
	if ( image->len < ( BZI_HDR_OFFSET + sizeof ( *bzhdr ) ) ) {
		DBGC ( image, "bzImage %p too short for kernel header\n",
		       image );
		return -ENOEXEC;
	}

	/* Read and verify header */
	copy_from_user ( bzhdr, image->data, BZI_HDR_OFFSET,
			 sizeof ( *bzhdr ) );
	if ( bzhdr->header != BZI_SIGNATURE ) {
		DBGC ( image, "bzImage %p bad signature\n", image );
		return -ENOEXEC;
	}

	/* We don't support ancient kernels */
	if ( bzhdr->version < 0x0200 ) {
		DBGC ( image, "bzImage %p version %04x not supported\n",
		       image, bzhdr->version );
		return -ENOTSUP;
	}

	/* Calculate load address and size of real-mode portion */
	load_ctx->rm_kernel_seg = 0x1000; /* place RM kernel at 1000:0000 */
	load_ctx->rm_kernel = real_to_user ( load_ctx->rm_kernel_seg, 0 );
	load_ctx->rm_filesz = load_ctx->rm_memsz =
		( ( bzhdr->setup_sects ? bzhdr->setup_sects : 4 ) + 1 ) << 9;
	if ( load_ctx->rm_filesz > image->len ) {
		DBGC ( image, "bzImage %p too short for %zd byte of setup\n",
		       image, load_ctx->rm_filesz );
		return -ENOEXEC;
	}

	/* Calculate load address and size of non-real-mode portion */
	load_ctx->pm_kernel = ( ( bzhdr->loadflags & BZI_LOAD_HIGH ) ?
				phys_to_user ( BZI_LOAD_HIGH_ADDR ) :
				phys_to_user ( BZI_LOAD_LOW_ADDR ) );
	load_ctx->pm_sz = ( image->len - load_ctx->rm_filesz );

	DBGC ( image, "bzImage %p version %04x RM %#zx bytes PM %#zx bytes\n",
	       image, bzhdr->version, load_ctx->rm_filesz, load_ctx->pm_sz );
	return 0;
}

/**
 * Load real-mode portion of bzImage
 *
 * @v image		bzImage file
 * @v load_ctx		Load context
 * @v cmdline		Kernel command line
 * @ret rc		Return status code
 */
static int bzimage_load_real ( struct image *image,
			       struct bzimage_load_context *load_ctx,
			       const char *cmdline ) {
	size_t cmdline_len = ( strlen ( cmdline ) + 1 );
	int rc;

	/* Allow space for the stack and heap */
	load_ctx->rm_memsz += BZI_STACK_SIZE;
	load_ctx->rm_heap = load_ctx->rm_memsz;

	/* Allow space for the command line, if one exists */
	load_ctx->rm_cmdline = load_ctx->rm_memsz;
	load_ctx->rm_memsz += cmdline_len;

	/* Prepare, verify, and load the real-mode segment */
	if ( ( rc = prep_segment ( load_ctx->rm_kernel, load_ctx->rm_filesz,
				   load_ctx->rm_memsz ) ) != 0 ) {
		DBGC ( image, "bzImage %p could not prepare RM segment: %s\n",
		       image, strerror ( rc ) );
		return rc;
	}
	memcpy_user ( load_ctx->rm_kernel, 0, image->data, 0,
		      load_ctx->rm_filesz );

	/* Copy command line */
	copy_to_user ( load_ctx->rm_kernel, load_ctx->rm_cmdline,
		       cmdline, cmdline_len );

	return 0;
}

/**
 * Load non-real-mode portion of bzImage
 *
 * @v image		bzImage file
 * @v load_ctx		Load context
 * @ret rc		Return status code
 */
static int bzimage_load_non_real ( struct image *image,
				   struct bzimage_load_context *load_ctx ) {
	int rc;

	/* Prepare, verify and load the non-real-mode segment */
	if ( ( rc = prep_segment ( load_ctx->pm_kernel, load_ctx->pm_sz,
				   load_ctx->pm_sz ) ) != 0 ) {
		DBGC ( image, "bzImage %p could not prepare PM segment: %s\n",
		       image, strerror ( rc ) );
		return rc;
	}
	memcpy_user ( load_ctx->pm_kernel, 0, image->data, load_ctx->rm_filesz,
		      load_ctx->pm_sz );

	return 0;
}


/**
 * Update and store bzImage header
 *
 * @v image		bzImage file
 * @v load_ctx		Load context
 * @v bzhdr		Original bzImage header
 * @ret rc		Return status code
 */
static int bzimage_write_header ( struct image *image __unused,
				  struct bzimage_load_context *load_ctx,
				  struct bzimage_header *bzhdr ) {
	struct bzimage_cmdline cmdline;

	/* Update the header and copy it into the loaded kernel */
	bzhdr->type_of_loader = BZI_LOADER_TYPE_ETHERBOOT;
	if ( bzhdr->version >= 0x0201 ) {
		bzhdr->heap_end_ptr = ( load_ctx->rm_heap - 0x200 );
		bzhdr->loadflags |= BZI_CAN_USE_HEAP;
	}
	if ( bzhdr->version >= 0x0202 ) {
		bzhdr->cmd_line_ptr = user_to_phys ( load_ctx->rm_kernel,
						     load_ctx->rm_cmdline );
	} else {
		cmdline.magic = BZI_CMDLINE_MAGIC;
		cmdline.offset = load_ctx->rm_cmdline;
		copy_to_user ( load_ctx->rm_kernel, BZI_CMDLINE_OFFSET,
			       &cmdline, sizeof ( cmdline ) );
		bzhdr->setup_move_size = load_ctx->rm_memsz;
	}
	copy_to_user ( load_ctx->rm_kernel, BZI_HDR_OFFSET,
		       bzhdr, sizeof ( *bzhdr ) );

	return 0;
}

/**
 * Load bzImage image into memory
 *
 * @v image		bzImage file
 * @ret rc		Return status code
 */
int bzimage_load ( struct image *image ) {
	struct bzimage_header bzhdr;
	struct bzimage_load_context load_ctx;
	union {
		struct bzimage_exec_context bz;
		unsigned long ul;
	} exec_ctx;
	const char *cmdline = ( image->cmdline ? image->cmdline : "" );
	int rc;

	/* Load and verify header */
	if ( ( rc = bzimage_load_header ( image, &load_ctx, &bzhdr ) ) != 0 )
		return rc;

	/* This is a bzImage image, valid or otherwise */
	if ( ! image->type )
		image->type = &bzimage_image_type;

	/* Load real-mode portion */
	if ( ( rc = bzimage_load_real ( image, &load_ctx, cmdline ) ) != 0 )
		return rc;

	/* Load non-real-mode portion */
	if ( ( rc = bzimage_load_non_real ( image, &load_ctx ) ) != 0 )
		return rc;

	/* Update and write out header */
	if ( ( rc = bzimage_write_header ( image, &load_ctx, &bzhdr ) ) != 0 )
		return rc;

	/* Record execution context in image private data field */
	exec_ctx.bz.kernel_seg = load_ctx.rm_kernel_seg;
	exec_ctx.bz.stack = load_ctx.rm_heap;
	image->priv.ul = exec_ctx.ul;

	return 0;
}

/** Linux bzImage image type */
struct image_type bzimage_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "bzImage",
	.load = bzimage_load,
	.exec = bzimage_exec,
};
