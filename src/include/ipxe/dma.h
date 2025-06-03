#ifndef _IPXE_DMA_H
#define _IPXE_DMA_H

/** @file
 *
 * DMA mappings
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/api.h>
#include <ipxe/io.h>
#include <ipxe/malloc.h>
#include <ipxe/umalloc.h>
#include <config/ioapi.h>

#ifdef DMAAPI_OP
#define DMAAPI_PREFIX_op
#else
#define DMAAPI_PREFIX_op __op_
#endif

#ifdef DMAAPI_FLAT
#define DMAAPI_PREFIX_flat
#else
#define DMAAPI_PREFIX_flat __flat_
#endif

/** A DMA mapping */
struct dma_mapping {
	/** Address offset
	 *
	 * This is the value that must be added to a physical address
	 * within the mapping in order to produce the corresponding
	 * device-side DMA address.
	 */
	physaddr_t offset;
	/** DMA device (if unmapping is required) */
	struct dma_device *dma;
	/** Platform mapping token */
	void *token;
};

/** A DMA-capable device */
struct dma_device {
	/** DMA operations */
	struct dma_operations *op;
	/** Addressable space mask */
	physaddr_t mask;
	/** Total number of mappings (for debugging) */
	unsigned int mapped;
	/** Total number of allocations (for debugging) */
	unsigned int allocated;
};

/** DMA operations */
struct dma_operations {
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
	int ( * map ) ( struct dma_device *dma, struct dma_mapping *map,
			physaddr_t addr, size_t len, int flags );
	/**
	 * Unmap buffer
	 *
	 * @v dma		DMA device
	 * @v map		DMA mapping
	 */
	void ( * unmap ) ( struct dma_device *dma, struct dma_mapping *map );
	/**
	 * Allocate and map DMA-coherent buffer
	 *
	 * @v dma		DMA device
	 * @v map		DMA mapping to fill in
	 * @v len		Length of buffer
	 * @v align		Physical alignment
	 * @ret addr		Buffer address, or NULL on error
	 */
	void * ( * alloc ) ( struct dma_device *dma, struct dma_mapping *map,
			     size_t len, size_t align );
	/**
	 * Unmap and free DMA-coherent buffer
	 *
	 * @v dma		DMA device
	 * @v map		DMA mapping
	 * @v addr		Buffer address
	 * @v len		Length of buffer
	 */
	void ( * free ) ( struct dma_device *dma, struct dma_mapping *map,
			  void *addr, size_t len );
	/**
	 * Allocate and map DMA-coherent buffer from external (user) memory
	 *
	 * @v dma		DMA device
	 * @v map		DMA mapping to fill in
	 * @v len		Length of buffer
	 * @v align		Physical alignment
	 * @ret addr		Buffer address, or NULL on error
	 */
	void * ( * umalloc ) ( struct dma_device *dma,
			       struct dma_mapping *map,
			       size_t len, size_t align );
	/**
	 * Unmap and free DMA-coherent buffer from external (user) memory
	 *
	 * @v dma		DMA device
	 * @v map		DMA mapping
	 * @v addr		Buffer address
	 * @v len		Length of buffer
	 */
	void ( * ufree ) ( struct dma_device *dma, struct dma_mapping *map,
			   void *addr, size_t len );
	/**
	 * Set addressable space mask
	 *
	 * @v dma		DMA device
	 * @v mask		Addressable space mask
	 */
	void ( * set_mask ) ( struct dma_device *dma, physaddr_t mask );
};

/** Device will read data from host memory */
#define DMA_TX 0x01

/** Device will write data to host memory */
#define DMA_RX 0x02

/** Device will both read data from and write data to host memory */
#define DMA_BI ( DMA_TX | DMA_RX )

/**
 * Calculate static inline DMA I/O API function name
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @ret _subsys_func	Subsystem API function
 */
#define DMAAPI_INLINE( _subsys, _api_func ) \
	SINGLE_API_INLINE ( DMAAPI_PREFIX_ ## _subsys, _api_func )

/**
 * Provide a DMA I/O API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @v _func		Implementing function
 */
#define PROVIDE_DMAAPI( _subsys, _api_func, _func ) \
	PROVIDE_SINGLE_API ( DMAAPI_PREFIX_ ## _subsys, _api_func, _func )

/**
 * Provide a static inline DMA I/O API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 */
#define PROVIDE_DMAAPI_INLINE( _subsys, _api_func ) \
	PROVIDE_SINGLE_API_INLINE ( DMAAPI_PREFIX_ ## _subsys, _api_func )

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
static inline __always_inline int
DMAAPI_INLINE ( flat, dma_map ) ( struct dma_device *dma,
				  struct dma_mapping *map,
				  physaddr_t addr __unused,
				  size_t len __unused, int flags __unused ) {

	/* Increment mapping count (for debugging) */
	if ( DBG_LOG ) {
		map->dma = dma;
		dma->mapped++;
	}

	return 0;
}

/**
 * Unmap buffer
 *
 * @v map		DMA mapping
 */
static inline __always_inline void
DMAAPI_INLINE ( flat, dma_unmap ) ( struct dma_mapping *map ) {

	/* Decrement mapping count (for debugging) */
	if ( DBG_LOG ) {
		assert ( map->dma != NULL );
		map->dma->mapped--;
		map->dma = NULL;
	}
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
static inline __always_inline void *
DMAAPI_INLINE ( flat, dma_alloc ) ( struct dma_device *dma,
				    struct dma_mapping *map,
				    size_t len, size_t align ) {
	void *addr;

	/* Allocate buffer */
	addr = malloc_phys ( len, align );

	/* Increment mapping count (for debugging) */
	if ( DBG_LOG && addr ) {
		map->dma = dma;
		dma->mapped++;
	}

	return addr;
}

/**
 * Unmap and free DMA-coherent buffer
 *
 * @v map		DMA mapping
 * @v addr		Buffer address
 * @v len		Length of buffer
 */
static inline __always_inline void
DMAAPI_INLINE ( flat, dma_free ) ( struct dma_mapping *map,
				   void *addr, size_t len ) {

	/* Free buffer */
	free_phys ( addr, len );

	/* Decrement mapping count (for debugging) */
	if ( DBG_LOG ) {
		assert ( map->dma != NULL );
		map->dma->mapped--;
		map->dma = NULL;
	}
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
static inline __always_inline void *
DMAAPI_INLINE ( flat, dma_umalloc ) ( struct dma_device *dma,
				      struct dma_mapping *map,
				      size_t len, size_t align __unused ) {
	void *addr;

	/* Allocate buffer */
	addr = umalloc ( len );

	/* Increment mapping count (for debugging) */
	if ( DBG_LOG && addr ) {
		map->dma = dma;
		dma->mapped++;
	}

	return addr;
}

/**
 * Unmap and free DMA-coherent buffer from external (user) memory
 *
 * @v map		DMA mapping
 * @v addr		Buffer address
 * @v len		Length of buffer
 */
static inline __always_inline void
DMAAPI_INLINE ( flat, dma_ufree ) ( struct dma_mapping *map,
				    void *addr, size_t len __unused ) {

	/* Free buffer */
	ufree ( addr );

	/* Decrement mapping count (for debugging) */
	if ( DBG_LOG ) {
		assert ( map->dma != NULL );
		map->dma->mapped--;
		map->dma = NULL;
	}
}

/**
 * Set addressable space mask
 *
 * @v dma		DMA device
 * @v mask		Addressable space mask
 */
static inline __always_inline void
DMAAPI_INLINE ( flat, dma_set_mask ) ( struct dma_device *dma __unused,
				       physaddr_t mask __unused ) {

	/* Nothing to do */
}

/**
 * Get DMA address from physical address
 *
 * @v map		DMA mapping
 * @v addr		Physical address within the mapped region
 * @ret addr		Device-side DMA address
 */
static inline __always_inline physaddr_t
DMAAPI_INLINE ( flat, dma_phys ) ( struct dma_mapping *map __unused,
				   physaddr_t addr ) {

	/* Use physical address as device address */
	return addr;
}

/**
 * Get DMA address from physical address
 *
 * @v map		DMA mapping
 * @v addr		Physical address within the mapped region
 * @ret addr		Device-side DMA address
 */
static inline __always_inline physaddr_t
DMAAPI_INLINE ( op, dma_phys ) ( struct dma_mapping *map, physaddr_t addr ) {

	/* Adjust physical address using mapping offset */
	return ( addr + map->offset );
}

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
int dma_map ( struct dma_device *dma, struct dma_mapping *map,
	      physaddr_t addr, size_t len, int flags );

/**
 * Unmap buffer
 *
 * @v map		DMA mapping
 */
void dma_unmap ( struct dma_mapping *map );

/**
 * Allocate and map DMA-coherent buffer
 *
 * @v dma		DMA device
 * @v map		DMA mapping to fill in
 * @v len		Length of buffer
 * @v align		Physical alignment
 * @ret addr		Buffer address, or NULL on error
 */
void * dma_alloc ( struct dma_device *dma, struct dma_mapping *map,
		   size_t len, size_t align );

/**
 * Unmap and free DMA-coherent buffer
 *
 * @v map		DMA mapping
 * @v addr		Buffer address
 * @v len		Length of buffer
 */
void dma_free ( struct dma_mapping *map, void *addr, size_t len );

/**
 * Allocate and map DMA-coherent buffer from external (user) memory
 *
 * @v dma		DMA device
 * @v map		DMA mapping to fill in
 * @v len		Length of buffer
 * @v align		Physical alignment
 * @ret addr		Buffer address, or NULL on error
 */
void * dma_umalloc ( struct dma_device *dma, struct dma_mapping *map,
		     size_t len, size_t align );

/**
 * Unmap and free DMA-coherent buffer from external (user) memory
 *
 * @v map		DMA mapping
 * @v addr		Buffer address
 * @v len		Length of buffer
 */
void dma_ufree ( struct dma_mapping *map, void *addr, size_t len );

/**
 * Set addressable space mask
 *
 * @v dma		DMA device
 * @v mask		Addressable space mask
 */
void dma_set_mask ( struct dma_device *dma, physaddr_t mask );

/**
 * Get DMA address from physical address
 *
 * @v map		DMA mapping
 * @v addr		Physical address within the mapped region
 * @ret addr		Device-side DMA address
 */
physaddr_t dma_phys ( struct dma_mapping *map, physaddr_t addr );

/**
 * Get DMA address from virtual address
 *
 * @v map		DMA mapping
 * @v addr		Virtual address within the mapped region
 * @ret addr		Device-side DMA address
 */
static inline __always_inline physaddr_t dma ( struct dma_mapping *map,
					       void *addr ) {

	/* Get DMA address from corresponding physical address */
	return dma_phys ( map, virt_to_phys ( addr ) );
}

/**
 * Check if DMA unmapping is required
 *
 * @v map		DMA mapping
 * @v unmap		Unmapping is required
 */
static inline __always_inline int dma_mapped ( struct dma_mapping *map ) {

	/* Unmapping is required if a DMA device was recorded */
	return ( map->dma != NULL );
}

/**
 * Initialise DMA device
 *
 * @v dma		DMA device
 * @v op		DMA operations
 */
static inline __always_inline void dma_init ( struct dma_device *dma,
					      struct dma_operations *op ) {

	/* Set operations table */
	dma->op = op;
}

/**
 * Set 64-bit addressable space mask
 *
 * @v dma		DMA device
 */
static inline __always_inline void
dma_set_mask_64bit ( struct dma_device *dma ) {

	/* Set mask to maximum physical address */
	dma_set_mask ( dma, ~( ( physaddr_t ) 0 ) );
}

#endif /* _IPXE_DMA_H */
