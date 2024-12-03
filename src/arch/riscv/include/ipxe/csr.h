#ifndef _IPXE_CSR_H
#define _IPXE_CSR_H

/** @file
 *
 * Control and status registers (CSRs)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**
 * Check if CSR can be read
 *
 * @v name		CSR name
 * @v allowed		CSR can be read
 */
#define csr_can_read( name ) ( {					\
	unsigned long stvec_orig;					\
	unsigned long stvec_temp;					\
	unsigned long csr;						\
	int allowed = 0;						\
									\
	__asm__ __volatile__ ( /* Set temporary trap vector */		\
			       "la %3, 1f\n\t"				\
			       "csrrw %2, stvec, %3\n\t"		\
			       /* Try reading CSR */			\
			       "csrr %1, " name "\n\t"			\
			       /* Mark as allowed if not trapped */	\
			       "addi %0, %0, 1\n\t"			\
			       /* Temporary trap vector */		\
			       ".balign 4\n\t"				\
			       "\n1:\n\t"				\
			       /* Restore original trap vector */	\
			       "csrw stvec, %2\n\t"			\
			       : "+r" ( allowed ),			\
				 "=r" ( csr ),				\
				 "=r" ( stvec_orig ),			\
				 "=r" ( stvec_temp ) );			\
	allowed;							\
	} )

/**
 * Check if CSR can be written
 *
 * @v name		CSR name
 * @v value		Value to write
 * @v allowed		CSR can be written
 */
#define csr_can_write( name, value ) ( {				\
	unsigned long stvec_orig;					\
	unsigned long stvec_temp;					\
	unsigned long csr = (value);					\
	int allowed = 0;						\
									\
	__asm__ __volatile__ ( /* Set temporary trap vector */		\
			       "la %3, 1f\n\t"				\
			       "csrrw %2, stvec, %3\n\t"		\
			       /* Try writing CSR */			\
			       "csrrw %1, " name ", %1\n\t"		\
			       /* Mark as allowed if not trapped */	\
			       "addi %0, %0, 1\n\t"			\
			       /* Temporary trap vector */		\
			       ".balign 4\n\t"				\
			       "\n1:\n\t"				\
			       /* Restore original trap vector */	\
			       "csrw stvec, %2\n\t"			\
			       : "+r" ( allowed ),			\
				 "+r" ( csr ),				\
				 "=r" ( stvec_orig ),			\
				 "=r" ( stvec_temp ) );			\
	allowed;							\
	} )

#endif /* _IPXE_CSR_H */
