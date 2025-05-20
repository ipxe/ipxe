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
#include <ipxe/io.h>
#include <ipxe/fdt.h>
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
		memmap_dump ( &region );
		if ( ! ( region.flags & MEMMAP_FL_MEMORY ) )
			continue;
		ctx->ram = region.addr;
		DBGC ( image, "LKRN %s RAM starts at %#08lx\n",
		       image->name, ctx->ram );
		return 0;
	}

	DBGC ( image, "LKRN %s found no RAM\n", image->name );
	return -ENOTSUP;
}

/**
 * Load kernel image
 *
 * @v image		Kernel image
 * @v ctx		Kernel image context
 * @ret rc		Return status code
 */
static int lkrn_load ( struct image *image, struct lkrn_context *ctx ) {
	void *dest;
	int rc;

	/* Record entry point */
	ctx->entry = ( ctx->ram + ctx->offset );
	dest = phys_to_virt ( ctx->entry );
	DBGC ( image, "LKRN %s loading to [%#08lx,%#08lx,%#08lx)\n",
	       image->name, ctx->entry, ( ctx->entry + ctx->filesz ),
	       ( ctx->entry + ctx->memsz ) );

	/* Prepare segment */
	if ( ( rc = prep_segment ( dest, ctx->filesz, ctx->memsz ) ) != 0 ) {
		DBGC ( image, "LKRN %s could not prepare kernel "
		       "segment: %s\n", image->name, strerror ( rc ) );
		return rc;
	}

	/* Copy to segment */
	memcpy ( dest, image->data, ctx->filesz );

	return 0;
}

/**
 * Construct device tree
 *
 * @v image		Kernel image
 * @v ctx		Kernel image context
 * @ret rc		Return status code
 */
static int lkrn_fdt ( struct image *image, struct lkrn_context *ctx ) {
	struct fdt_header *fdt;
	void *dest;
	size_t len;
	int rc;

	/* Build device tree (which may change system memory map) */
	if ( ( rc = fdt_create ( &fdt, image->cmdline ) ) != 0 )
		goto err_create;
	len = be32_to_cpu ( fdt->totalsize );

	/* Place device tree after kernel, rounded up to a page boundary */
	ctx->fdt = ( ( ctx->ram + ctx->offset + ctx->memsz + PAGE_SIZE - 1 ) &
		     ~( PAGE_SIZE - 1 ) );
	dest = phys_to_virt ( ctx->fdt );
	DBGC ( image, "LKRN %s FDT at [%#08lx,%#08lx)\n",
	       image->name, ctx->fdt, ( ctx->fdt + len ) );

	/*
	 * No further allocations are permitted after this point,
	 * since we are about to start loading segments.
	 *
	 */

	/* Prepare segment */
	if ( ( rc = prep_segment ( dest, len, len ) ) != 0 ) {
		DBGC ( image, "LKRN %s could not prepare FDT segment: %s\n",
		       image->name, strerror ( rc ) );
		goto err_segment;
	}

	/* Copy to segment */
	memcpy ( dest, fdt, len );

	/* Success */
	rc = 0;

 err_segment:
	fdt_remove ( fdt );
 err_create:
	return rc;
}

/**
 * Execute kernel image
 *
 * @v image		Kernel image
 * @ret rc		Return status code
 */
static int lkrn_exec ( struct image *image ) {
	struct lkrn_context ctx;
	int rc;

	/* Parse header */
	if ( ( rc = lkrn_parse ( image, &ctx ) ) != 0 )
		return rc;

	/* Locate start of RAM */
	if ( ( rc = lkrn_ram ( image, &ctx ) ) != 0 )
		return rc;

	/* Create device tree (which may change system memory map) */
	if ( ( rc = lkrn_fdt ( image, &ctx ) ) != 0 )
		return rc;

	/* Load kernel image (after all allocations are finished) */
	if ( ( rc = lkrn_load ( image, &ctx ) ) != 0 )
		return rc;

	/* Jump to kernel entry point */
	DBGC ( image, "LKRN %s jumping to kernel at %#08lx\n",
	       image->name, ctx.entry );
	lkrn_jump ( ctx.entry, ctx.fdt );

	/* There is no way for the image to return, since we provide
	 * no return address.
	 */
	assert ( 0 );

	return -ECANCELED; /* -EIMPOSSIBLE */
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
