#ifndef _IPXE_CPUID_H
#define _IPXE_CPUID_H

/** @file
 *
 * x86 CPU feature detection
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

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

/** CPUID extended function */
#define CPUID_EXTENDED 0x80000000UL

/** CPUID hypervisor function */
#define CPUID_HYPERVISOR 0x40000000UL

/** Get vendor ID and largest standard function */
#define CPUID_VENDOR_ID 0x00000000UL

/** Get standard features */
#define CPUID_FEATURES 0x00000001UL

/** RDRAND instruction is supported */
#define CPUID_FEATURES_INTEL_ECX_RDRAND 0x40000000UL

/** Hypervisor is present */
#define CPUID_FEATURES_INTEL_ECX_HYPERVISOR 0x80000000UL

/** TSC is present */
#define CPUID_FEATURES_INTEL_EDX_TSC 0x00000010UL

/** FXSAVE and FXRSTOR are supported */
#define CPUID_FEATURES_INTEL_EDX_FXSR 0x01000000UL

/** Get largest extended function */
#define CPUID_AMD_MAX_FN 0x80000000UL

/** Extended function existence check */
#define CPUID_AMD_CHECK 0x80000000UL

/** Extended function existence check mask */
#define CPUID_AMD_CHECK_MASK 0xffff0000UL

/** Get extended features */
#define CPUID_AMD_FEATURES 0x80000001UL

/** Get CPU model */
#define CPUID_MODEL 0x80000002UL

/** Get APM information */
#define CPUID_APM 0x80000007UL

/** Invariant TSC */
#define CPUID_APM_EDX_TSC_INVARIANT 0x00000100UL

/**
 * Issue CPUID instruction
 *
 * @v function		CPUID function (input via %eax)
 * @v subfunction	CPUID subfunction (input via %ecx)
 * @v eax		Output via %eax
 * @v ebx		Output via %ebx
 * @v ecx		Output via %ecx
 * @v edx		Output via %edx
 */
static inline __attribute__ (( always_inline )) void
cpuid ( uint32_t function, uint32_t subfunction, uint32_t *eax, uint32_t *ebx,
	uint32_t *ecx, uint32_t *edx ) {

	__asm__ ( "cpuid"
		  : "=a" ( *eax ), "=b" ( *ebx ), "=c" ( *ecx ), "=d" ( *edx )
		  : "0" ( function ), "2" ( subfunction ) );
}

extern int cpuid_supported ( uint32_t function );
extern void x86_features ( struct x86_features *features );

#endif /* _IPXE_CPUID_H */
