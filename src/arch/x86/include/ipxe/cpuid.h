#ifndef _IPXE_CPUID_H
#define _IPXE_CPUID_H

/** @file
 *
 * x86 CPU feature detection
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>

/** An x86 CPU feature register set */
struct x86_feature_registers {
	/** Features returned via %ecx */
	uint32_t ecx;
	/** Features returned via %edx */
	uint32_t edx;
};

/** x86 CPU features */
struct x86_features {
	/** Intel-defined features (%eax=0x00000001) */
	struct x86_feature_registers intel;
	/** AMD-defined features (%eax=0x80000001) */
	struct x86_feature_registers amd;
};

/** CPUID support flag */
#define CPUID_FLAG 0x00200000UL

/** Get vendor ID and largest standard function */
#define CPUID_VENDOR_ID 0x00000000UL

/** Get standard features */
#define CPUID_FEATURES 0x00000001UL

/** Get largest extended function */
#define CPUID_AMD_MAX_FN 0x80000000UL

/** Extended function existence check */
#define CPUID_AMD_CHECK 0x80000000UL

/** Extended function existence check mask */
#define CPUID_AMD_CHECK_MASK 0xffff0000UL

/** Get extended features */
#define CPUID_AMD_FEATURES 0x80000001UL

extern void x86_features ( struct x86_features *features );

#endif /* _IPXE_CPUID_H */
