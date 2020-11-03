/*
 * Copyright C 2020, Oracle and/or its affiliates.
 * Licensed under the GPL v2 only.
 */

FILE_LICENCE ( GPL2_ONLY );

/**
 * @file
 *
 * Null measure mechanism
 *
 */

#include <errno.h>
#include <stdio.h>
#include <ipxe/image.h>
#include <ipxe/measure.h>

/**
 * Measure data 
 *
 * @v			image to measure
 * @ret rc		Return status code
 */
static int null_measure_image ( struct image *image __unused ) {

	printf ( "Cannot measure image; not implemented\n" );

	return -ENOTSUP;
}

PROVIDE_MEASURE ( null, measure_image, null_measure_image );
