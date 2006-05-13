#ifndef BIOSINT_H
#define BIOSINT_H

/**
 * @file BIOS interrupts
 *
 */

struct segoff;

extern void hook_bios_interrupt ( unsigned int interrupt, unsigned int handler,
				  struct segoff *chain_vector );
extern int unhook_bios_interrupt ( unsigned int interrupt,
				   unsigned int handler,
				   struct segoff *chain_vector );

#endif /* BIOSINT_H */
