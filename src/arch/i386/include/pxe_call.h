#ifndef _PXE_CALL_H
#define _PXE_CALL_H

/** @file
 *
 * PXE API entry point
 */

#include <pxe_api.h>
#include <realmode.h>

/** !PXE structure */
extern struct s_PXE __text16 ( ppxe );
#define ppxe __use_text16 ( ppxe )

/** PXENV+ structure */
extern struct s_PXENV __text16 ( pxenv );
#define pxenv __use_text16 ( pxenv )

extern void pxe_hook_int1a ( void );
extern int pxe_unhook_int1a ( void );
extern void pxe_init_structures ( void );

#endif /* _PXE_CALL_H */
