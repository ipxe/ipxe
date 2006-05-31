#ifndef _GPXE_ASYNC_H
#define _GPXE_ASYNC_H

/** @file
 *
 * Asynchronous operations
 *
 */

#include <errno.h>
#include <assert.h>

/** An asynchronous operation */
struct async_operation {
	/** Operation status
	 *
	 * This is an error code as defined in errno.h, plus an offset
	 * of EINPROGRESS.  This means that a status value of 0
	 * corresponds to a return status code of -EINPROGRESS,
	 * i.e. that the default state of an asynchronous operation is
	 * "not yet completed".
	 */
	int status;
};

/**
 * Set asynchronous operation status
 *
 * @v aop	Asynchronous operation
 * @v rc	Return status code
 */
static inline __attribute__ (( always_inline )) void
async_set_status ( struct async_operation *aop, int rc ) {
	aop->status = ( rc + EINPROGRESS );
}

/**
 * Get asynchronous operation status
 *
 * @v aop	Asynchronous operation
 * @ret rc	Return status code
 */
static inline __attribute__ (( always_inline )) int
async_status ( struct async_operation *aop ) {
	return ( aop->status - EINPROGRESS );
}

/**
 * Flag asynchronous operation as complete
 *
 * @v aop	Asynchronous operation
 * @v rc	Return status code
 */
static inline __attribute__ (( always_inline )) void
async_done ( struct async_operation *aop, int rc ) {
	assert ( rc != -EINPROGRESS );
	async_set_status ( aop, rc );
}

extern int async_wait ( struct async_operation *aop );

#endif /* _GPXE_ASYNC_H */
