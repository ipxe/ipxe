#ifndef _IPXE_EFI_PROCESSOR_BIND_H
#define _IPXE_EFI_PROCESSOR_BIND_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/*
 * EFI header files rely on having the CPU architecture directory
 * present in the search path in order to pick up ProcessorBind.h.  We
 * use this header file as a quick indirection layer.
 *  - mcb30
 */

#ifdef EFI_HOSTONLY

/*
 * We cannot rely on the EDK2 ProcessorBind.h headers when compiling a
 * binary for execution on the build host itself, since the host's CPU
 * architecture may not even be supported by EDK2.
 */

/* Define the basic integer types in terms of the host's <stdint.h> */
#include <stdint.h>
typedef int8_t INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int64_t INT64;
typedef uint8_t UINT8;
typedef long INTN;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef unsigned long UINTN;
typedef int8_t CHAR8;
typedef int16_t CHAR16;
typedef uint8_t BOOLEAN;

/* Define EFIAPI as whatever API the host uses by default */
#define EFIAPI

/* Define an architecture-neutral MDE_CPU macro to prevent build errors */
#define MDE_CPU_EBC

/* Define MAX_BIT in terms of UINTN */
#define MAX_BIT ( ( ( UINTN ) 1U ) << ( ( 8 * sizeof ( UINTN ) ) - 1 ) )

#else /* EFI_HOSTONLY */

#ifdef __i386__
#include <ipxe/efi/Ia32/ProcessorBind.h>
#endif

#ifdef __x86_64__
#include <ipxe/efi/X64/ProcessorBind.h>
#endif

#ifdef __arm__
#include <ipxe/efi/Arm/ProcessorBind.h>
#endif

#ifdef __aarch64__
#include <ipxe/efi/AArch64/ProcessorBind.h>
#endif

#ifdef __loongarch__
#include <ipxe/efi/LoongArch64/ProcessorBind.h>
#endif

#ifdef __riscv
#include <ipxe/efi/RiscV64/ProcessorBind.h>
#endif

#endif /* EFI_HOSTONLY */

#endif /* _IPXE_EFI_PROCESSOR_BIND_H */
