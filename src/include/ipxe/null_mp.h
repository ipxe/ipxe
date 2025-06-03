#ifndef _IPXE_NULL_MP_H
#define _IPXE_NULL_MP_H

/** @file
 *
 * Null multiprocessor API implementation
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef MPAPI_NULL
#define MPAPI_PREFIX_null
#else
#define MPAPI_PREFIX_null __null_
#endif

static inline __attribute__ (( always_inline )) mp_addr_t
MPAPI_INLINE ( null, mp_address ) ( void *address ) {

	return ( ( mp_addr_t ) address );
}

static inline __attribute__ (( always_inline )) void
MPAPI_INLINE ( null, mp_exec_boot ) ( mp_func_t func __unused,
				      void *opaque __unused ) {
	/* Do nothing */
}

static inline __attribute__ (( always_inline )) void
MPAPI_INLINE ( null, mp_start_all ) ( mp_func_t func __unused,
				      void *opaque __unused ) {
	/* Do nothing */
}

#endif /* _IPXE_NULL_MP_H */
