/* Header for pxe_callbacks.c.
 */

#ifndef PXE_CALLBACKS_H
#define PXE_CALLBACKS_H

#include "etherboot.h"
#include "segoff.h"
#include "pxe.h"

typedef struct {
	segoff_t	orig_retaddr;
	uint16_t	opcode;
	segoff_t	segoff;
} PACKED pxe_call_params_t;

/*
 * These values are hard-coded into the PXE spec
 */
#define PXE_LOAD_SEGMENT	(0x0000)
#define PXE_LOAD_OFFSET		(0x7c00)
#define PXE_LOAD_ADDRESS	( ( PXE_LOAD_SEGMENT << 4 ) + PXE_LOAD_OFFSET )

/* Function prototypes
 */
extern pxe_stack_t * install_pxe_stack ( void *base );
extern void use_undi_ds_for_rm_stack ( uint16_t ds );
extern int hook_pxe_stack ( void );
extern int unhook_pxe_stack ( void );
extern void remove_pxe_stack ( void );
extern int xstartpxe ( void );

#endif /* PXE_CALLBACKS_H */
