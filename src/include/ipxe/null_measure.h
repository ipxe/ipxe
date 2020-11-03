#ifndef _IPXE_NULL_MEASURE_H
#define _IPXE_NULL_MEASURE_H

/*
 * Copyright C 2020, Oracle and/or its affiliates.
 * Licensed under the GPL v2 only.
 */

FILE_LICENCE ( GPL2_ONLY );

/** @file
 *
 * iPXE do-nothing measure API
 *
 */

#ifdef MEASURE_NULL
#define MEASURE_PREFIX_null
#else
#define MEASURE_PREFIX_null __null_
#endif

#endif /* _IPXE_NULL_MEASURE_H */
