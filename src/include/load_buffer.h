#ifndef LOAD_BUFFER_H
#define LOAD_BUFFER_H

#include "buffer.h"

/*
 * These functions are architecture-dependent, but the interface must
 * be identical between architectures.
 *
 */

/*
 * Initialise a buffer suitable for loading an image.  Pass in a
 * pointer to an uninitialised struct buffer.
 *
 * Note that this function may (for example) allocate all remaining
 * allocatable memory, so it must be called *after* any other code
 * that might want to allocate memory (e.g. device driver
 * initialisation).
 *
 */
extern int init_load_buffer ( struct buffer *buffer );

/*
 * Cut a load buffer down to size once the image has been loaded.
 * This will shrink the buffer down to the size of the data contained
 * within the buffer, freeing up unused memory if applicable.
 *
 */
extern void trim_load_buffer ( struct buffer *buffer );

/*
 * Finish using a load buffer, once the image has been moved into its
 * target location in memory.
 *
 */
extern void done_load_buffer ( struct buffer *buffer );

#endif /* LOAD_BUFFER_H */
