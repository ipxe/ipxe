#include <malloc.h>
#include <gpxe/heap.h>

/**
 * @file
 *
 * Heap
 *
 */

/**
 * Heap size
 *
 * Currently fixed at 48kB.
 */
#define HEAP_SIZE ( 48 * 1024 )

/** The heap itself */
char heap[HEAP_SIZE] __attribute__ (( aligned ( __alignof__(void *) )));

/**
 * Initialise the heap
 *
 */
void init_heap ( void ) {
	mpopulate ( heap, sizeof ( heap ) );
}
