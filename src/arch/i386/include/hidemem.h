#ifndef HIDEMEM_H
#define HIDEMEM_H

#include "segoff.h"

extern int install_e820mangler ( void *new_mangler );
extern int hide_etherboot ( void );
extern int unhide_etherboot ( void );

/* Symbols in e820mangler.S */
extern void e820mangler ( void );
extern void _intercept_int15 ( void );
extern segoff_t _intercepted_int15;
typedef struct {
	uint32_t start;
	uint32_t length;
} exclude_range_t;
extern exclude_range_t _hide_memory[2];
extern uint16_t e820mangler_size;

#endif /* HIDEMEM_H */
