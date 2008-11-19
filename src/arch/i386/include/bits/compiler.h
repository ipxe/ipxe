#ifndef _BITS_COMPILER_H
#define _BITS_COMPILER_H

#ifndef ASSEMBLY

/** Declare a function with standard calling conventions */
#define __asmcall __attribute__ (( cdecl, regparm(0) ))

#endif /* ASSEMBLY */

#endif /* _BITS_COMPILER_H */
