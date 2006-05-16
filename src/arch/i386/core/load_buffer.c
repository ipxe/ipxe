#include "io.h"
#include "load_buffer.h"

/*
 * Initialise a buffer in an unused portion of memory, to be used for
 * loading an image
 *
 */

#ifdef KEEP_IT_REAL

/*
 * Under KEEP_IT_REAL, always use 07c0:0000 as the load buffer.
 *
 */

int init_load_buffer ( struct buffer *buffer ) {
	buffer->start = 0x7c00;
	buffer->end = 0xa0000;
	DBG ( "LOAD_BUFFER using [%x,%x)\n", buffer->start, buffer->end );
	init_buffer ( buffer );
	return 1;
}

void trim_load_buffer ( struct buffer *buffer ) {
	/* Nothing to do */
}

void done_load_buffer ( struct buffer *buffer ) {
	/* Nothing to do */
}

#else /* KEEP_IT_REAL */

/*
 * Without KEEP_IT_REAL, use all remaining heap space as the load buffer.
 *
 */
int init_load_buffer ( struct buffer *buffer ) {
	void *data;
	size_t size;
	
	data = emalloc_all ( &size );
	if ( ! data )
		return 0;

	buffer->start = virt_to_phys ( data );
	buffer->end = buffer->start + size;
	DBG ( "LOAD_BUFFER using [%x,%x)\n", buffer->start, buffer->end );
	init_buffer ( buffer );
	return 1;
}

void trim_load_buffer ( struct buffer *buffer ) {
	void *new_start;

	/* Shrink buffer */
	new_start = erealloc ( phys_to_virt ( buffer->start ), buffer->fill );
	DBG ( "LOAD_BUFFER shrunk from [%x,%x) to [%x,%x)\n", buffer->start,
	      buffer->end, virt_to_phys ( new_start ), buffer->end );
	buffer->start = virt_to_phys ( new_start );
}

void done_load_buffer ( struct buffer *buffer ) {
	efree ( phys_to_virt ( buffer->start ) );
	DBG ( "LOAD_BUFFER freed [%x,%x)\n", buffer->start, buffer->end );
}

#endif
