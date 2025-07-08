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
static int riscv_dma_map ( struct dma_device *dma __unused,
			   struct dma_mapping *map __unused,
			   void *addr, size_t len, int flags ) {

	/* Sanity check: we cannot support bidirectional mappings */
	assert ( ! ( ( flags & DMA_TX ) & ( flags & DMA_RX ) ) );

	/* Flush cached data to transmit buffers */
	if ( flags & DMA_TX )
		cache_clean ( addr, len );

	/* Invalidate cached data in receive buffers */
	if ( flags & DMA_RX )
		cache_invalidate ( addr, len );

	return 0;
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
	caddr = ( ( void * ) ( intptr_t ) ( phys + SVPAGE_DMA32 ) );
	assert ( phys == virt_to_phys ( caddr ) );
	DBGC ( dma, "DMA allocated [%#08lx,%#08lx) via %p\n",
	       phys, ( phys + len ), caddr );

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

	/* Sanity check */
	assert ( virt_to_phys ( addr ) == virt_to_phys ( map->token ) );

	/* Free original allocation */
	free_phys ( map->token, len );

	/* Clear mapping */
	map->dma = NULL;
	map->token = NULL;
}

PROVIDE_DMAAPI ( riscv, dma_map, riscv_dma_map );
PROVIDE_DMAAPI_INLINE ( riscv, dma_unmap );
PROVIDE_DMAAPI ( riscv, dma_alloc, riscv_dma_alloc );
PROVIDE_DMAAPI ( riscv, dma_free, riscv_dma_free );
PROVIDE_DMAAPI ( riscv, dma_umalloc, riscv_dma_alloc );
PROVIDE_DMAAPI ( riscv, dma_ufree, riscv_dma_free );
PROVIDE_DMAAPI_INLINE ( riscv, dma_set_mask );
PROVIDE_DMAAPI_INLINE ( riscv, dma );
