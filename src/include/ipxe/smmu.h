#ifndef _IPXE_SMMU_H
#define _IPXE_SMMU_H

/** @file
 *
 * Arm System Memory Management Unit (SMMU)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>

/** SMMU register length */
#define SMMU_LEN 0x20000

/** Control register 0 */
#define SMMU_CR0 0x0020
#define SMMU_CR0_SMMUEN			0x00000001UL	/**< Enabled */

/** Stream table base address */
#define SMMU_STRTAB_BASE 0x0080
#define SMMU_STRTAB_BASE_MASK	0x000fffffffffffc0ULL	/**< Address mask */

/** Stream table base configuration */
#define SMMU_STRTAB_BASE_CFG 0x0088
#define SMMU_STRTAB_BASE_CFG_SPLIT( x ) \
	( ( (x) >> 6 ) & 0x1f )				/**< Split point */
#define SMMU_STRTAB_BASE_CFG_FMT_L2	0x00010000UL	/**< 2-level format */
#define SMMU_STRTAB_BASE_CFG_FMT_MASK	0x00030000UL	/**< Format mask */

/** Level 1 stream table descriptor */
#define SMMU_L1STD_SPAN_MASK	0x000000000000001fULL	/**< Span mask */
#define SMMU_L1STD_SPAN_SINGLE	0x0000000000000001ULL	/**< Single span */

/** Stream table entry */
struct smmu_ste {
	/** Configuration */
	uint64_t cfg[8];
} __attribute__ (( aligned ( 64 ) ));

/* Configuration word 0 */
#define SMMU_STE_CFG0_V		0x0000000000000001ULL	/**< Valid */
#define SMMU_STE_CFG0_BYPASS	0x0000000000000008ULL	/**< Bypass mode */

/* Configuration word 1 */
#define SMMU_STE_CFG1_SHCFG_INC	0x0000100000000000ULL	/**< Incoming */

#endif /* _IPXE_SMMU_H */
