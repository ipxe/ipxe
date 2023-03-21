#ifndef _BITS_COMPILER_H
#define _BITS_COMPILER_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** Dummy relocation type */
#define RELOC_TYPE_NONE R_LARCH_NONE

#ifndef ASSEMBLY

/** Unprefixed constant operand modifier */
#define ASM_NO_PREFIX "a"

#define __asmcall
#define __libgcc

#endif /* ASSEMBLY */

#endif /*_BITS_COMPILER_H */
