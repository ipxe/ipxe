/*
 * Copyright (C) 2020 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <assert.h>
#include <errno.h>
#include <ipxe/dma.h>

/** @file
 *
 * DMA mappings
 *
 */

/******************************************************************************
 *
 * Flat address space DMA API
 *
 ******************************************************************************
 */

PROVIDE_DMAAPI_INLINE ( flat, dma_map );
PROVIDE_DMAAPI_INLINE ( flat, dma_unmap );
PROVIDE_DMAAPI_INLINE ( flat, dma_alloc );
PROVIDE_DMAAPI_INLINE ( flat, dma_free );
PROVIDE_DMAAPI_INLINE ( flat, dma_umalloc );
PROVIDE_DMAAPI_INLINE ( flat, dma_ufree );
PROVIDE_DMAAPI_INLINE ( flat, dma_set_mask );
PROVIDE_DMAAPI_INLINE ( flat, dma_phys );

/******************************************************************************
 *
 * Operations-based DMA API
 *
 ******************************************************************************
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
static int dma_op_map ( struct dma_device *dma, struct dma_mapping *map,
			physaddr_t addr, size_t len, int flags ) {
	struct dma_operations *op = dma->op;

	if ( ! op )
		return -ENODEV;
	return op->map ( dma, map, addr, len, flags );
}

/**
 * Unmap buffer
 *
 * @v map		DMA mapping
 */
static void dma_op_unmap ( struct dma_mapping *map ) {
	struct dma_device *dma = map->dma;

	assert ( dma != NULL );
	assert ( dma->op != NULL );
	dma->op->unmap ( dma, map );
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
static void * dma_op_alloc ( struct dma_device *dma, struct dma_mapping *map,
			     size_t len, size_t align ) {
	struct dma_operations *op = dma->op;

	if ( ! op )
		return NULL;
	return op->alloc ( dma, map, len, align );
}

/**
 * Unmap and free DMA-coherent buffer
 *
 * @v map		DMA mapping
 * @v addr		Buffer address
 * @v len		Length of buffer
 */
static void dma_op_free ( struct dma_mapping *map, void *addr, size_t len ) {
	struct dma_device *dma = map->dma;

	assert ( dma != NULL );
	assert ( dma->op != NULL );
	dma->op->free ( dma, map, addr, len );
}

/**
 * Allocate and map DMA-coherent buffer from external (user) memory
 *
 * @v dma		DMA device
 * @v map		DMA mapping to fill in
 * @v len		Length of buffer
 * @v align		Physical alignment
 * @ret addr		Buffer address, or NULL on error
 */
static void * dma_op_umalloc ( struct dma_device *dma,
			       struct dma_mapping *map,
			       size_t len, size_t align ) {
	struct dma_operations *op = dma->op;

	if ( ! op )
		return NULL;
	return op->umalloc ( dma, map, len, align );
}

/**
 * Unmap and free DMA-coherent buffer from external (user) memory
 *
 * @v map		DMA mapping
 * @v addr		Buffer address
 * @v len		Length of buffer
 */
static void dma_op_ufree ( struct dma_mapping *map, void *addr, size_t len ) {
	struct dma_device *dma = map->dma;

	assert ( dma != NULL );
	assert ( dma->op != NULL );
	dma->op->ufree ( dma, map, addr, len );
}

/**
 * Set addressable space mask
 *
 * @v dma		DMA device
 * @v mask		Addressable space mask
 */
static void dma_op_set_mask ( struct dma_device *dma, physaddr_t mask ) {
	struct dma_operations *op = dma->op;

	if ( op )
		op->set_mask ( dma, mask );
}

PROVIDE_DMAAPI ( op, dma_map, dma_op_map );
PROVIDE_DMAAPI ( op, dma_unmap, dma_op_unmap );
PROVIDE_DMAAPI ( op, dma_alloc, dma_op_alloc );
PROVIDE_DMAAPI ( op, dma_free, dma_op_free );
PROVIDE_DMAAPI ( op, dma_umalloc, dma_op_umalloc );
PROVIDE_DMAAPI ( op, dma_ufree, dma_op_ufree );
PROVIDE_DMAAPI ( op, dma_set_mask, dma_op_set_mask );
PROVIDE_DMAAPI_INLINE ( op, dma_phys );
