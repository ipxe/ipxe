#ifndef _OLD_REALMODE_H
#define _OLD_REALMODE_H

#include <realmode.h>

#warning "Anything including this header is obsolete and must be rewritten"

/* Just for now */
#define SEGMENT(x) ( virt_to_phys ( x ) >> 4 )
#define OFFSET(x) ( virt_to_phys ( x ) & 0xf )
#define SEGOFF(x) { OFFSET(x), SEGMENT(x) }

/* To make basemem.c compile */
extern int lock_real_mode_stack;
extern char *real_mode_stack;
extern char real_mode_stack_size[];

#define RM_FRAGMENT(name,asm) \
	void name ( void ) {} \
	extern char name ## _size[];

#endif /* _OLD_REALMODE_H */
