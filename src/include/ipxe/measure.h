#ifndef _IPXE_MEASURE_H
#define _IPXE_MEASURE_H

/*
 * Copyright C 2020, Oracle and/or its affiliates.
 * Licensed under the GPL v2 only.
 */

FILE_LICENCE ( GPL2_ONLY );

/** @file
 *
 * Image measurement
 *
 */

#include <ipxe/api.h>
#include <ipxe/image.h>

/**
 * Calculate static inline measure API function name
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @ret _subsys_func	Subsystem API function
 */
#define MEASURE_INLINE( _subsys, _api_func ) \
	SINGLE_API_INLINE ( MEASURE_PREFIX_ ## _subsys, _api_func )

/**
 * Provide a measure API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @v _func		Implementing function
 */
#define PROVIDE_MEASURE( _subsys, _api_func, _func ) \
	PROVIDE_SINGLE_API ( MEASURE_PREFIX_ ## _subsys, _api_func, _func )

/**
 * Provide a static inline measure API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 */
#define PROVIDE_MEASURE_INLINE( _subsys, _api_func ) \
	PROVIDE_SINGLE_API_INLINE ( MEASURE_PREFIX_ ## _subsys, _api_func )

/* Include all architecture-independent measure API headers */
#include <ipxe/null_measure.h>
#include <ipxe/efi/efi_measure.h>

/**
 * Measure image
 *
 * @v			image to measure
 * @ret rc		Return status code
 */
int measure_image ( struct image *image );

#endif /* _IPXE_MEASURE_H */
