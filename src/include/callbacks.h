/* Callout/callback interface for Etherboot
 *
 * This file provides the mechanisms for making calls from Etherboot
 * to external programs and vice-versa.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 *
 * $Id$
 */

#ifndef CALLBACKS_H
#define CALLBACKS_H

/* Opcodes and flags for in_call()
 */
#define EB_OPCODE(x) ( (x) & 0xffff )
#define EB_OPCODE_MAIN		(0x0000)
#define EB_OPCODE_CHECK		(0x6948)	/* 'Hi' */
#define EB_OPCODE_PXE		(0x7850)	/* 'Px' */
#define EB_OPCODE_PXENV		(0x7650)	/* 'Pv' */
#define EB_USE_INTERNAL_STACK	( 1 << 16 )
#define EB_CALL_FROM_REAL_MODE	( 1 << 17 )	/* i386 only */
#define EB_SKIP_OPCODE		( 1 << 18 )

/* Standard return codes
 */
#define EB_CHECK_RESULT		(0x6f486948)	/* 'HiHo' */

/* Include arch-specific callbacks bits
 */
#include "callbacks_arch.h"

/* Skip the definitions that won't make sense to the assembler */
#ifndef ASSEMBLY

#include <stdarg.h>

#ifndef in_call_data_t
typedef struct {} empty_struct_t;
#define in_call_data_t empty_struct_t
#endif

#endif /* ASSEMBLY */

#endif /* CALLBACKS_H */
