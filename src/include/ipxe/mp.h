#ifndef _IPXE_MP_H
#define _IPXE_MP_H

/** @file
 *
 * Multiprocessor functions
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/api.h>
#include <config/defaults.h>

/**
 * An address within the address space for a multiprocessor function
 *
 * Application processors may be started in a different address space
 * from the normal iPXE runtime environment.  For example: under
 * legacy BIOS the application processors will use flat 32-bit
 * physical addressing (with no paging or virtual address offset).
 */
typedef unsigned long mp_addr_t;

/** A multiprocessor function
 *
 * @v opaque		Opaque data pointer
 * @v cpuid		CPU identifier
 *
 * iPXE does not set up a normal multiprocessor environment.  In
 * particular, there is no support for dispatching code to individual
 * processors and there is no per-CPU stack allocation.
 *
 * Multiprocessor code must be prepared to run wth no stack space (and
 * with a zero stack pointer).  Functions may use the CPU identifier
 * to construct a pointer to per-CPU result storage.
 *
 * Multiprocessor functions are permitted to overwrite all registers
 * apart from the stack pointer.  On exit, the function should check
 * the stack pointer value: if zero then the function should halt the
 * CPU, if non-zero then the function should return in the normal way.
 *
 * Multiprocessor functions do not have access to any capabilities
 * typically provided by the firmware: they cannot, for example, write
 * any console output.
 *
 * All parameters are passed in registers, since there may be no stack
 * available.  These functions cannot be called directly from C code.
 */
typedef void ( mp_func_t ) ( mp_addr_t opaque, unsigned int cpuid );

/**
 * Call a multiprocessor function from C code on the current CPU
 *
 * @v func		Multiprocessor function
 * @v opaque		Opaque data pointer
 *
 * This function must be provided for each CPU architecture to bridge
 * the normal C ABI to the iPXE multiprocessor function ABI.  It must
 * therefore preserve any necessary registers, determine the CPU
 * identifier, call the multiprocessor function (which may destroy any
 * registers other than the stack pointer), restore registers, and
 * return to the C caller.
 *
 * This function must be called from within the multiprocessor address
 * space (e.g. with flat 32-bit physical addressing for BIOS).  It can
 * be called directly from C code if the multiprocessor address space
 * is identical to the address space used for C code (e.g. under EFI,
 * where everything uses flat physical addresses).
 */
extern void __asmcall mp_call ( mp_addr_t func, mp_addr_t opaque );

/**
 * Calculate static inline multiprocessor API function name
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @ret _subsys_func	Subsystem API function
 */
#define MPAPI_INLINE( _subsys, _api_func ) \
	SINGLE_API_INLINE ( MPAPI_PREFIX_ ## _subsys, _api_func )

/**
 * Provide a multiprocessor API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @v _func		Implementing function
 */
#define PROVIDE_MPAPI( _subsys, _api_func, _func ) \
	PROVIDE_SINGLE_API ( MPAPI_PREFIX_ ## _subsys, _api_func, _func )

/**
 * Provide a static inline multiprocessor API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 */
#define PROVIDE_MPAPI_INLINE( _subsys, _api_func ) \
	PROVIDE_SINGLE_API_INLINE ( MPAPI_PREFIX_ ## _subsys, _api_func )

/* Include all architecture-independent multiprocessor API headers */
#include <ipxe/null_mp.h>
#include <ipxe/efi/efi_mp.h>

/* Include all architecture-dependent multiprocessor API headers */
#include <bits/mp.h>

/**
 * Calculate address as seen by a multiprocessor function
 *
 * @v address		Address in normal iPXE address space
 * @ret address		Address in application processor address space
 */
mp_addr_t mp_address ( void *address );

/**
 * Execute a multiprocessor function on the boot processor
 *
 * @v func		Multiprocessor function
 * @v opaque		Opaque data pointer
 *
 * This is a blocking operation: the call will return only when the
 * multiprocessor function exits.
 */
void mp_exec_boot ( mp_func_t func, void *opaque );

/**
 * Start a multiprocessor function on all application processors
 *
 * @v func		Multiprocessor function
 * @v opaque		Opaque data pointer
 *
 * This is a non-blocking operation: it is the caller's responsibility
 * to provide a way to determine when the multiprocessor function has
 * finished executing and halted its CPU.
 */
void mp_start_all ( mp_func_t func, void *opaque );

/**
 * Update maximum observed CPU identifier
 *
 * @v opaque		Opaque data pointer
 * @v cpuid		CPU identifier
 *
 * This may be invoked on each processor to update a shared maximum
 * CPU identifier value.
 */
extern mp_func_t mp_update_max_cpuid;

extern unsigned int mp_boot_cpuid ( void );
extern unsigned int mp_max_cpuid ( void );

#endif /* _IPXE_MP_H */
