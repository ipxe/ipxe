#ifndef _BITS_COMPILER_H
#define _BITS_COMPILER_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** Dummy relocation type */
#define RELOC_TYPE_NONE R_RISCV_NONE

/* Determine load/store instructions for natural bit width */
#if __riscv_xlen == 128
#define NATURAL_SUFFIX q
#elif __riscv_xlen == 64
#define NATURAL_SUFFIX d
#elif __riscv_xlen == 32
#define NATURAL_SUFFIX w
#else
#error "Unsupported bit width"
#endif
#ifdef ASSEMBLY
#define LOADN _C2 ( L, NATURAL_SUFFIX )
#define STOREN _C2 ( S, NATURAL_SUFFIX )
#else
#define LOADN "L" _S2 ( NATURAL_SUFFIX )
#define STOREN "S" _S2 ( NATURAL_SUFFIX )
#endif

#ifndef ASSEMBLY

/** Unprefixed constant operand modifier */
#define ASM_NO_PREFIX ""

/** Declare a function with standard calling conventions */
#define __asmcall

/** Declare a function with libgcc implicit linkage */
#define __libgcc

#endif /* ASSEMBLY */

#endif /* _BITS_COMPILER_H */
