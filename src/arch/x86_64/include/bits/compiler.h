#ifndef _BITS_COMPILER_H
#define _BITS_COMPILER_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** Dummy relocation type */
#define RELOC_TYPE_NONE R_X86_64_NONE

#ifndef ASSEMBLY

/** Unprefixed constant operand modifier */
#define ASM_NO_PREFIX "c"

/** Declare a function with standard calling conventions */
#define __asmcall __attribute__ (( regparm(0) ))

/** Declare a function with libgcc implicit linkage */
#define __libgcc

#endif /* ASSEMBLY */

#endif /* _BITS_COMPILER_H */
