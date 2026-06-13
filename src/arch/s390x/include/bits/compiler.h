#ifndef _BITS_COMPILER_H
#define _BITS_COMPILER_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** Dummy relocation type */
#define RELOC_TYPE_NONE R_390_NONE

#ifndef ASSEMBLY

/** Unprefixed constant operand modifier */
#define ASM_NO_PREFIX "c"

/** Declare a function with standard calling conventions */
#define __asmcall

/** Declare a function with libgcc implicit linkage */
#define __libgcc

/** An even/odd register pair for scalar values */
struct s390x_scalar_pair {
	/** Scalar value in even 64-bit register */
	unsigned long even;
	/** Scalar value in odd 64-bit register */
	unsigned long odd;
};

/** An even/odd register pair for pointer and length values */
struct s390x_pointer_pair {
	/** Pointer value in even 64-bit register */
	const void *even;
	/** Length value in odd 64-bit register */
	unsigned long odd;
};

#endif /* ASSEMBLY */

#endif /* _BITS_COMPILER_H */
