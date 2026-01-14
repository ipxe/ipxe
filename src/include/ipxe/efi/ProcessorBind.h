#ifndef _IPXE_EFI_PROCESSOR_BIND_H
#define _IPXE_EFI_PROCESSOR_BIND_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

/*
 * EFI header files rely on having the CPU architecture directory
 * present in the search path in order to pick up ProcessorBind.h.  We
 * use this header file as a quick indirection layer.
 *  - mcb30
 */

/* Determine EFI architecture name (if existent) */
#if defined ( __i386__ )
#define EFIARCH Ia32
#endif
#if defined ( __x86_64__ )
#define EFIARCH X64
#endif
#if defined ( __arm__ )
#define EFIARCH Arm
#endif
#if defined ( __aarch64__ )
#define EFIARCH AArch64
#endif
#if defined ( __loongarch__ )
#define EFIARCH LoongArch64
#endif
#if defined ( __riscv ) && ( _riscv_xlen == 64 )
#define EFIARCH RiscV64
#endif

/* Determine architecture-specific ProcessorBind.h path */
#define PROCESSORBIND(_arch) <ipxe/efi/_arch/ProcessorBind.h>

/*
 * We do not want to use any EFI-specific calling conventions etc when
 * compiling a binary for execution on the build host itself.
 */
#ifdef EFI_HOSTONLY
#undef EFIARCH
#endif

#if defined ( EFIARCH )

/* Include architecture-specific ProcessorBind.h if existent */
#include PROCESSORBIND(EFIARCH)

#else /* EFIARCH */

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
typedef char CHAR8;
typedef uint16_t CHAR16;
typedef uint8_t BOOLEAN;

/* Define EFIAPI as whatever API the host uses by default */
#define EFIAPI

/* Define an architecture-neutral MDE_CPU macro to prevent build errors */
#define MDE_CPU_EBC

/* Define a dummy boot file name to prevent build errors */
#define EFI_REMOVABLE_MEDIA_FILE_NAME L"\\EFI\\BOOT\\BOOTNONE.EFI"

/* Define MAX_BIT in terms of UINTN */
#define MAX_BIT ( ( ( UINTN ) 1U ) << ( ( 8 * sizeof ( UINTN ) ) - 1 ) )

#endif /* EFIARCH */

#endif /* _IPXE_EFI_PROCESSOR_BIND_H */
