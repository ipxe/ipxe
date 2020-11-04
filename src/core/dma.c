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
#include <ipxe/iobuf.h>
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
PROVIDE_DMAAPI_INLINE ( flat, dma_set_mask );

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
 * @v addr		Buffer address
 * @v len		Length of buffer
 * @v flags		Mapping flags
 * @v map		DMA mapping to fill in
 * @ret rc		Return status code
 */
static int dma_op_map ( struct dma_device *dma, physaddr_t addr, size_t len,
			int flags, struct dma_mapping *map ) {
	struct dma_operations *op = dma->op;

	if ( ! op )
		return -ENODEV;
	return op->map ( dma, addr, len, flags, map );
}

/**
 * Unmap buffer
 *
 * @v dma		DMA device
 * @v map		DMA mapping
 */
static void dma_op_unmap ( struct dma_device *dma, struct dma_mapping *map ) {
	struct dma_operations *op = dma->op;

	assert ( op != NULL );
	op->unmap ( dma, map );
}

/**
 * Allocate and map DMA-coherent buffer
 *
 * @v dma		DMA device
 * @v len		Length of buffer
 * @v align		Physical alignment
 * @v map		DMA mapping to fill in
 * @ret addr		Buffer address, or NULL on error
 */
static void * dma_op_alloc ( struct dma_device *dma, size_t len, size_t align,
			     struct dma_mapping *map ) {
	struct dma_operations *op = dma->op;

	if ( ! op )
		return NULL;
	return op->alloc ( dma, len, align, map );
}

/**
 * Unmap and free DMA-coherent buffer
 *
 * @v dma		DMA device
 * @v addr		Buffer address
 * @v len		Length of buffer
 * @v map		DMA mapping
 */
static void dma_op_free ( struct dma_device *dma, void *addr, size_t len,
			  struct dma_mapping *map ) {
	struct dma_operations *op = dma->op;

	assert ( op != NULL );
	op->free ( dma, addr, len, map );
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
PROVIDE_DMAAPI ( op, dma_set_mask, dma_op_set_mask );

/******************************************************************************
 *
 * Utility functions
 *
 ******************************************************************************
 */

/**
 * Allocate and map I/O buffer for receiving data from device
 *
 * @v dma		DMA device
 * @v len		Length of I/O buffer
 * @v map		DMA mapping to fill in
 * @ret iobuf		I/O buffer, or NULL on error
 */
struct io_buffer * dma_alloc_rx_iob ( struct dma_device *dma, size_t len,
				      struct dma_mapping *map ) {
	struct io_buffer *iobuf;
	int rc;

	/* Allocate I/O buffer */
	iobuf = alloc_iob ( len );
	if ( ! iobuf )
		goto err_alloc;

	/* Map I/O buffer */
	if ( ( rc = dma_map ( dma, virt_to_phys ( iobuf->data ), len,
			      DMA_RX, map ) ) != 0 )
		goto err_map;

	return iobuf;

	dma_unmap ( dma, map );
 err_map:
	free_iob ( iobuf );
 err_alloc:
	return NULL;
}
