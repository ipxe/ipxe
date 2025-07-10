#ifndef _IPXE_RISCV_DMA_H
#define _IPXE_RISCV_DMA_H

/** @file
 *
 * iPXE DMA API for RISC-V
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef DMAAPI_RISCV
#define DMAAPI_PREFIX_riscv
#else
#define DMAAPI_PREFIX_riscv __riscv_
#endif

/**
 * Set addressable space mask
 *
 * @v dma		DMA device
 * @v mask		Addressable space mask
 */
static inline __always_inline void
DMAAPI_INLINE ( riscv, dma_set_mask ) ( struct dma_device *dma __unused,
					physaddr_t mask __unused ) {

	/* Nothing to do */
}

/**
 * Get DMA address from virtual address
 *
 * @v map		DMA mapping
 * @v addr		Address within the mapped region
 * @ret addr		Device-side DMA address
 */
static inline __always_inline physaddr_t
DMAAPI_INLINE ( riscv, dma ) ( struct dma_mapping *map __unused, void *addr ) {

	/* Use physical address as device address */
	return virt_to_phys ( addr );
}

#endif /* _IPXE_RISCV_DMA_H */
