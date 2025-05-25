/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/image.h>
#include <ipxe/memmap.h>
#include <ipxe/uaccess.h>
#include <ipxe/segment.h>
#include <ipxe/initrd.h>
#include <ipxe/io.h>
#include <ipxe/fdt.h>
#include <ipxe/init.h>
#include <ipxe/lkrn.h>

/** @file
 *
 * Linux kernel image format
 *
 */

/**
 * Parse kernel image
 *
 * @v image		Kernel image
 * @v ctx		Kernel image context
 * @ret rc		Return status code
 */
static int lkrn_parse ( struct image *image, struct lkrn_context *ctx ) {
	const struct lkrn_header *hdr;

	/* Initialise context */
	memset ( ctx, 0, sizeof ( *ctx ) );

	/* Read image header */
	if ( image->len < sizeof ( *hdr ) ) {
		DBGC ( image, "LKRN %s too short for header\n", image->name );
		return -ENOEXEC;
	}
	hdr = image->data;

	/* Check magic value */
	if ( hdr->magic != cpu_to_le32 ( LKRN_MAGIC_ARCH ) ) {
		DBGC ( image, "LKRN %s bad magic value %#08x\n",
		       image->name, le32_to_cpu ( hdr->magic ) );
		return -ENOEXEC;
	}

	/* Record load offset */
	ctx->offset = le64_to_cpu ( hdr->text_offset );
	if ( ctx->offset & ( ctx->offset - 1 ) ) {
		DBGC ( image, "LKRN %s offset %#zx is not a power of two\n",
		       image->name, ctx->offset );
		return -ENOEXEC;
	}

	/* Record and check image size */
	ctx->filesz = image->len;
	ctx->memsz = le64_to_cpu ( hdr->image_size );
	if ( ctx->filesz > ctx->memsz ) {
		DBGC ( image, "LKRN %s invalid image size %#zx/%#zx\n",
		       image->name, ctx->filesz, ctx->memsz );
		return -ENOEXEC;
	}

	return 0;
}

/**
 * Locate start of RAM
 *
 * @v image		Kernel image
 * @v ctx		Kernel image context
 * @ret rc		Return status code
 */
static int lkrn_ram ( struct image *image, struct lkrn_context *ctx ) {
	struct memmap_region region;

	/* Locate start of RAM */
	for_each_memmap ( &region, 0 ) {
		DBGC_MEMMAP ( image, &region );
		if ( ! ( region.flags & MEMMAP_FL_MEMORY ) )
			continue;
		ctx->ram = region.min;
		DBGC ( image, "LKRN %s RAM starts at %#08lx\n",
		       image->name, ctx->ram );
		return 0;
	}

	DBGC ( image, "LKRN %s found no RAM\n", image->name );
	return -ENOTSUP;
}

/**
 * Execute kernel image
 *
 * @v image		Kernel image
 * @ret rc		Return status code
 */
static int lkrn_exec ( struct image *image ) {
	static struct image fdtimg = {
		.refcnt = REF_INIT ( free_image ),
		.name = "<FDT>",
		.flags = ( IMAGE_STATIC | IMAGE_STATIC_NAME ),
	};
	struct lkrn_context ctx;
	struct memmap_region region;
	struct fdt_header *fdt;
	size_t initrdsz;
	size_t totalsz;
	void *dest;
	int rc;

	/* Parse header */
	if ( ( rc = lkrn_parse ( image, &ctx ) ) != 0 )
		goto err_parse;

	/* Locate start of RAM */
	if ( ( rc = lkrn_ram ( image, &ctx ) ) != 0 )
		goto err_ram;

	/* Place kernel at specified address from start of RAM */
	ctx.entry = ( ctx.ram + ctx.offset );
	DBGC ( image, "LKRN %s loading to [%#08lx,%#08lx,%#08lx)\n",
	       image->name, ctx.entry, ( ctx.entry + ctx.filesz ),
	       ( ctx.entry + ctx.memsz ) );

	/* Place initrd after kernel, aligned to the kernel's image offset */
	ctx.initrd = ( ctx.ram + initrd_align ( ctx.offset + ctx.memsz ) );
	ctx.initrd = ( ( ctx.initrd + ctx.offset - 1 ) & ~( ctx.offset - 1 ) );
	initrdsz = initrd_len();
	if ( initrdsz ) {
		DBGC ( image, "LKRN %s initrd at [%#08lx,%#08lx)\n",
		       image->name, ctx.initrd, ( ctx.initrd + initrdsz ) );
	}

	/* Place device tree after initrd */
	ctx.fdt = ( ctx.initrd + initrd_align ( initrdsz ) );

	/* Construct device tree and post-initrd image */
	if ( ( rc = fdt_create ( &fdt, image->cmdline, ctx.initrd,
				 initrdsz ) ) != 0 ) {
		goto err_fdt;
	}
	fdtimg.data = fdt;
	fdtimg.len = be32_to_cpu ( fdt->totalsize );
	list_add_tail ( &fdtimg.list, &images );
	DBGC ( image, "LKRN %s FDT at [%08lx,%08lx)\n",
	       image->name, ctx.fdt, ( ctx.fdt + fdtimg.len ) );

	/* Find post-reshuffle region */
	if ( ( rc = initrd_region ( initrdsz, &region ) ) != 0 ) {
		DBGC ( image, "LKRN %s no available region: %s\n",
		       image->name, strerror ( rc ) );
		goto err_region;
	}

	/* Check that everything can be placed at its target addresses */
	totalsz = ( ctx.fdt + fdtimg.len - ctx.ram );
	if ( ( ctx.entry >= region.min ) &&
	     ( ( ctx.offset + totalsz ) <= memmap_size ( &region ) ) ) {
		/* Target addresses are within the reshuffle region */
		DBGC ( image, "LKRN %s fits within reshuffle region\n",
		       image->name );
	} else {
		/* Target addresses are outside the reshuffle region */
		if ( ( rc = prep_segment ( phys_to_virt ( ctx.entry ),
					   totalsz, totalsz ) ) != 0 ) {
			DBGC ( image, "LKRN %s could not prepare segment: "
			       "%s\n", image->name, strerror ( rc ) );
			goto err_segment;
		}
	}

	/* This is the point of no return: we are about to reshuffle
	 * and thereby destroy the external heap.  No errors are
	 * allowed to occur after this point.
	 */

	/* Shut down ready for boot */
	shutdown_boot();

	/* Prepend kernel to reshuffle list, reshuffle, and remove kernel */
	list_add ( &image->list, &images );
	initrd_reshuffle();
	list_del ( &image->list );

	/* Load kernel to entry point and zero bss */
	dest = phys_to_virt ( ctx.entry );
	memmove ( dest, image->data, ctx.filesz );
	memset ( ( dest + ctx.filesz ), 0, ( ctx.memsz - ctx.filesz ) );

	/* Load initrds and device tree */
	dest = phys_to_virt ( ctx.initrd );
	initrd_load_all ( dest );

	/* Jump to kernel entry point */
	DBGC ( image, "LKRN %s jumping to kernel at %#08lx\n",
	       image->name, ctx.entry );
	lkrn_jump ( ctx.entry, ctx.fdt );

	/* There is no way for the image to return, since we provide
	 * no return address.
	 */
	assert ( 0 );

	return -ECANCELED; /* -EIMPOSSIBLE */

 err_segment:
 err_region:
	list_del ( &fdtimg.list );
	fdt_remove ( fdt );
 err_fdt:
 err_ram:
 err_parse:
	return rc;
}

/**
 * Probe kernel image
 *
 * @v image		Kernel image
 * @ret rc		Return status code
 */
static int lkrn_probe ( struct image *image ) {
	struct lkrn_context ctx;
	int rc;

	/* Parse header */
	if ( ( rc = lkrn_parse ( image, &ctx ) ) != 0 )
		return rc;

	DBGC ( image, "LKRN %s is a Linux kernel\n", image->name );
	return 0;
}

/** Linux kernel image type */
struct image_type lkrn_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "lkrn",
	.probe = lkrn_probe,
	.exec = lkrn_exec,
};

/**
 * Parse compressed kernel image
 *
 * @v image		Compressed kernel image
 * @v zctx		Compressed kernel image context
 * @ret rc		Return status code
 */
static int zimg_parse ( struct image *image, struct zimg_context *zctx ) {
	const struct zimg_header *zhdr;

	/* Initialise context */
	memset ( zctx, 0, sizeof ( *zctx ) );

	/* Parse header */
	if ( image->len < sizeof ( *zhdr ) ) {
		DBGC ( image, "ZIMG %s too short for header\n",
		       image->name );
		return -ENOEXEC;
	}
	zhdr = image->data;

	/* Check magic value */
	if ( zhdr->magic != cpu_to_le32 ( ZIMG_MAGIC ) ) {
		DBGC ( image, "ZIMG %s bad magic value %#08x\n",
		       image->name, le32_to_cpu ( zhdr->magic ) );
		return -ENOEXEC;
	}

	/* Record and check offset and length */
	zctx->offset = le32_to_cpu ( zhdr->offset );
	zctx->len = le32_to_cpu ( zhdr->len );
	if ( ( zctx->offset > image->len ) ||
	     ( zctx->len > ( image->len - zctx->offset ) ) ) {
		DBGC ( image, "ZIMG %s bad range [+%#zx,+%#zx)/%#zx\n",
		       image->name, zctx->offset,
		       (zctx->offset + zctx->len ), image->len );
		return -ENOEXEC;
	}

	/* Record compression type */
	zctx->type.raw = zhdr->type;

	return 0;
}

/**
 * Extract compresed kernel image
 *
 * @v image		Compressed kernel image
 * @v extracted		Extracted image
 * @ret rc		Return status code
 */
static int zimg_extract ( struct image *image, struct image *extracted ) {
	struct zimg_context zctx;
	const void *payload;
	int rc;

	/* Parse header */
	if ( ( rc = zimg_parse ( image, &zctx ) ) != 0 )
		return rc;
	DBGC ( image, "ZIMG %s has %s-compressed payload at [+%#zx,+%#zx)\n",
	       image->name, zctx.type.string, zctx.offset,
	       ( zctx.offset + zctx.len ) );

	/* Extract compressed payload */
	payload = ( image->data + zctx.offset );
	if ( ( rc = image_set_data ( extracted, payload, zctx.len ) ) != 0 ) {
		DBGC ( image, "ZIMG %s could not extract: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Probe compressed kernel image
 *
 * @v image		Compressed kernel image
 * @ret rc		Return status code
 */
static int zimg_probe ( struct image *image ) {
	struct zimg_context zctx;
	int rc;

	/* Parse header */
	if ( ( rc = zimg_parse ( image, &zctx ) ) != 0 )
		return rc;

	DBGC ( image, "ZIMG %s is a %s-compressed Linux kernel\n",
	       image->name, zctx.type.string );
	return 0;
}

/** Linux kernel compressed image type */
struct image_type zimg_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "zimg",
	.probe = zimg_probe,
	.extract = zimg_extract,
	.exec = image_extract_exec,
};
