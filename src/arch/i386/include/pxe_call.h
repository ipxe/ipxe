#ifndef _PXE_CALL_H
#define _PXE_CALL_H

/** @file
 *
 * PXE API entry point
 */

#include <pxe_api.h>
#include <realmode.h>

/** PXE load address segment */
#define PXE_LOAD_SEGMENT 0

/** PXE load address offset */
#define PXE_LOAD_OFFSET 0x7c00

/** PXE physical load address */
#define PXE_LOAD_PHYS ( ( PXE_LOAD_SEGMENT << 4 ) + PXE_LOAD_OFFSET )

/** !PXE structure */
extern struct s_PXE __text16 ( ppxe );
#define ppxe __use_text16 ( ppxe )

/** PXENV+ structure */
extern struct s_PXENV __text16 ( pxenv );
#define pxenv __use_text16 ( pxenv )

extern void pxe_hook_int1a ( void );
extern int pxe_unhook_int1a ( void );
extern void pxe_init_structures ( void );
extern int pxe_start_nbp ( void );
extern __asmcall void pxe_api_call ( struct i386_all_regs *ix86 );

#endif /* _PXE_CALL_H */
