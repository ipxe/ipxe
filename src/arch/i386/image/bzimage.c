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
 * bzImage execution context
 */
union bzimage_exec_context {
	/** Real-mode parameters */
	struct {
		/** Kernel real-mode data segment */
		uint16_t kernel_seg;
		/** Kernel real-mode stack pointer */
		uint16_t stack;
	} rm;
	unsigned long ul;
};

/**
 * Execute bzImage image
 *
 * @v image		bzImage image
 * @ret rc		Return status code
 */
static int bzimage_exec ( struct image *image ) {
	union bzimage_exec_context context;

	/* Retrieve stored execution context */
	context.ul = image->priv.ul;

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
			       : : "r" ( context.rm.kernel_seg ),
			           "r" ( context.rm.stack ),
			           "r" ( context.rm.kernel_seg + 0x20 ) );

	/* There is no way for the image to return, since we provide
	 * no return address.
	 */
	assert ( 0 );

	return -ECANCELED; /* -EIMPOSSIBLE */
}

/**
 * Load bzImage image into memory
 *
 * @v image		bzImage file
 * @ret rc		Return status code
 */
int bzimage_load ( struct image *image ) {
	struct bzimage_header bzhdr;
	union bzimage_exec_context context;
	unsigned int rm_kernel_seg = 0x1000; /* place RM kernel at 1000:0000 */
	userptr_t rm_kernel = real_to_user ( rm_kernel_seg, 0 );
	userptr_t pm_kernel;
	size_t rm_filesz;
	size_t rm_memsz;
	size_t pm_filesz;
	size_t pm_memsz;
	size_t rm_heap_end;
	size_t rm_cmdline;
	int rc;

	/* Sanity check */
	if ( image->len < ( BZI_HDR_OFFSET + sizeof ( bzhdr ) ) ) {
		DBGC ( image, "bzImage %p too short for kernel header\n",
		       image );
		return -ENOEXEC;
	}

	/* Read and verify header */
	copy_from_user ( &bzhdr, image->data, BZI_HDR_OFFSET,
			 sizeof ( bzhdr ) );
	if ( bzhdr.header != BZI_SIGNATURE ) {
		DBGC ( image, "bzImage %p not a bzImage\n", image );
		return -ENOEXEC;
	}

	/* This is a bzImage image, valid or otherwise */
	if ( ! image->type )
		image->type = &bzimage_image_type;

	/* We don't support ancient kernels */
	if ( bzhdr.version < 0x0200 ) {
		DBGC ( image, "bzImage %p version %04x not supported\n",
		       image, bzhdr.version );
		return -ENOTSUP;
	}
	DBGC ( image, "bzImage %p version %04x\n", image, bzhdr.version );

	/* Check size of base memory portions */
	rm_filesz = ( ( bzhdr.setup_sects ? bzhdr.setup_sects : 4 ) + 1 ) << 9;
	if ( rm_filesz > image->len ) {
		DBGC ( image, "bzImage %p too short for %zd byte of setup\n",
		       image, rm_filesz );
		return -ENOEXEC;
	}
	rm_memsz = rm_filesz;

	/* Allow space for the stack and heap */
	rm_memsz += BZI_STACK_SIZE;
	rm_heap_end = rm_memsz;

	/* Allow space for the command line, if one exists */
	rm_cmdline = rm_memsz;
	if ( image->cmdline )
		rm_memsz += ( strlen ( image->cmdline ) + 1 );

	/* Prepare, verify, and load the real-mode segment */
	if ( ( rc = prep_segment ( rm_kernel, rm_filesz, rm_memsz ) ) != 0 ) {
		DBGC ( image, "bzImage %p could not prepare RM segment: %s\n",
		       image, strerror ( rc ) );
		return rc;
	}
	memcpy_user ( rm_kernel, 0, image->data, 0, rm_filesz );

	/* Prepare, verify and load the rest of the kernel */
	pm_kernel = ( ( bzhdr.loadflags & BZI_LOAD_HIGH ) ?
		      phys_to_user ( 0x100000 ) : phys_to_user ( 0x10000 ) );
	pm_filesz = pm_memsz = ( image->len - rm_filesz );
	if ( ( rc = prep_segment ( pm_kernel, pm_filesz, pm_memsz ) ) != 0 ) {
		DBGC ( image, "bzImage %p could not prepare PM segment: %s\n",
		       image, strerror ( rc ) );
		return rc;
	}
	memcpy_user ( pm_kernel, 0, image->data, rm_filesz, pm_filesz );

	/* Copy down the command line, if it exists */
	if ( image->cmdline ) {
		copy_to_user ( rm_kernel, rm_cmdline, image->cmdline,
			       strlen ( image->cmdline ) + 1 );
	}

	/* Update the header and copy it into the loaded kernel */
	bzhdr.type_of_loader = BZI_LOADER_TYPE_ETHERBOOT;
	if ( bzhdr.version >= 0x0201 ) {
		bzhdr.heap_end_ptr = ( rm_heap_end - 0x200 );
		bzhdr.loadflags |= BZI_CAN_USE_HEAP;
	}
	if ( bzhdr.version >= 0x0202 ) {
		bzhdr.cmd_line_ptr = user_to_phys ( rm_kernel, rm_cmdline );
	} else {
		uint16_t cmd_line_magic = BZI_CMD_LINE_MAGIC;
		uint16_t cmd_line_offset = rm_cmdline;

		put_real ( cmd_line_magic, rm_kernel_seg,
			   BZI_CMD_LINE_MAGIC_OFFSET );
		put_real ( cmd_line_offset, rm_kernel_seg,
			   BZI_CMD_LINE_OFFSET_OFFSET );
		bzhdr.setup_move_size = rm_memsz;
	}
	copy_to_user ( rm_kernel, BZI_HDR_OFFSET, &bzhdr, sizeof ( bzhdr ) );

	/* Record execution context in image private data field */
	context.rm.kernel_seg = rm_kernel_seg;
	context.rm.stack = rm_heap_end;
	image->priv.ul = context.ul;

	return 0;
}

/** Linux bzImage image type */
struct image_type bzimage_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "bzImage",
	.load = bzimage_load,
	.exec = bzimage_exec,
};
