/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <assert.h>
#include <ipxe/zicbom.h>
#include <ipxe/iomap.h>
#include <ipxe/dma.h>

/** @file
 *
 * iPXE DMA API for RISC-V
 *
 */

/** Minimum alignment for coherent DMA allocations
 *
 * We set this sufficiently high to ensure that we do not end up with
 * both cached and uncached uses in the same cacheline.
 */
#define RISCV_DMA_ALIGN 256

/**
 * Map buffer for DMA
 *
 * @v dma		DMA device
 * @v map		DMA mapping to fill in
 * @v addr		Buffer address
 * @v len		Length of buffer
 * @v flags		Mapping flags
 * @ret rc		Return status code
 */
static int riscv_dma_map ( struct dma_device *dma,
			   struct dma_mapping *map,
			   void *addr, size_t len, int flags ) {

	/* Sanity check: we cannot support bidirectional mappings */
	assert ( ! ( ( flags & DMA_TX ) & ( flags & DMA_RX ) ) );

	/* Populate mapping */
	map->dma = dma;
	map->offset = 0;
	map->token = NULL;

	/* Flush cached data to transmit buffers */
	if ( flags & DMA_TX )
		cache_clean ( addr, len );

	/* Invalidate cached data in receive buffers and record address */
	if ( flags & DMA_RX ) {
		cache_invalidate ( addr, len );
		map->token = addr;
	}

	/* Increment mapping count (for debugging) */
	if ( DBG_LOG )
		dma->mapped++;

	return 0;
}

/**
 * Unmap buffer
 *
 * @v map		DMA mapping
 * @v len		Used length
 */
static void riscv_dma_unmap ( struct dma_mapping *map, size_t len ) {
	struct dma_device *dma = map->dma;
	void *addr = map->token;

	/* Invalidate cached data in receive buffers */
	if ( addr )
		cache_invalidate ( addr, len );

	/* Clear mapping */
	map->dma = NULL;

	/* Decrement mapping count (for debugging) */
	if ( DBG_LOG )
		dma->mapped--;
}

/**
 * Allocate and map DMA-coherent buffer
 *
 * @v dma		DMA device
 * @v map		DMA mapping to fill in
 * @v len		Length of buffer
 * @v align		Physical alignment
 * @ret addr		Buffer address, or NULL on error
 */
static void * riscv_dma_alloc ( struct dma_device *dma,
				struct dma_mapping *map,
				size_t len, size_t align ) {
	physaddr_t phys;
	void *addr;
	void *caddr;

	/* Round up length and alignment */
	len = ( ( len + RISCV_DMA_ALIGN - 1 ) & ~( RISCV_DMA_ALIGN - 1 ) );
	if ( align < RISCV_DMA_ALIGN )
		align = RISCV_DMA_ALIGN;

	/* Allocate from heap */
	addr = malloc_phys ( len, align );
	if ( ! addr )
		return NULL;

	/* Invalidate any existing cached data */
	cache_invalidate ( addr, len );

	/* Record mapping */
	map->dma = dma;
	map->token = addr;

	/* Calculate coherently-mapped virtual address */
	phys = virt_to_phys ( addr );
	assert ( phys == ( ( uint32_t ) phys ) );
	caddr = ( ( void * ) ( intptr_t ) ( phys + svpage_dma32() ) );
	assert ( phys == virt_to_phys ( caddr ) );
	DBGC ( dma, "DMA allocated [%#08lx,%#08lx) via %p\n",
	       phys, ( phys + len ), caddr );

	/* Increment allocation count (for debugging) */
	if ( DBG_LOG )
		dma->allocated++;

	return caddr;
}

/**
 * Unmap and free DMA-coherent buffer
 *
 * @v dma		DMA device
 * @v map		DMA mapping
 * @v addr		Buffer address
 * @v len		Length of buffer
 */
static void riscv_dma_free ( struct dma_mapping *map,
			     void *addr, size_t len ) {
	struct dma_device *dma = map->dma;

	/* Sanity check */
	assert ( virt_to_phys ( addr ) == virt_to_phys ( map->token ) );

	/* Round up length to match allocation */
	len = ( ( len + RISCV_DMA_ALIGN - 1 ) & ~( RISCV_DMA_ALIGN - 1 ) );

	/* Free original allocation */
	free_phys ( map->token, len );

	/* Clear mapping */
	map->dma = NULL;
	map->token = NULL;

	/* Decrement allocation count (for debugging) */
	if ( DBG_LOG )
		dma->allocated--;
}

PROVIDE_DMAAPI ( riscv, dma_map, riscv_dma_map );
PROVIDE_DMAAPI ( riscv, dma_unmap, riscv_dma_unmap );
PROVIDE_DMAAPI ( riscv, dma_alloc, riscv_dma_alloc );
PROVIDE_DMAAPI ( riscv, dma_free, riscv_dma_free );
PROVIDE_DMAAPI ( riscv, dma_umalloc, riscv_dma_alloc );
PROVIDE_DMAAPI ( riscv, dma_ufree, riscv_dma_free );
PROVIDE_DMAAPI_INLINE ( riscv, dma_set_mask );
PROVIDE_DMAAPI_INLINE ( riscv, dma );
