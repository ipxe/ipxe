#ifndef BASEMEM_H
#define BASEMEM_H

#include "stdint.h"

extern uint32_t get_free_base_memory ( void );
extern void * alloc_base_memory ( size_t size );
extern void free_base_memory ( void *ptr, size_t size );

#endif /* BASEMEM_H */
