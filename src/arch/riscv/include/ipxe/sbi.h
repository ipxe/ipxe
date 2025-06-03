#ifndef _IPXE_SBI_H
#define _IPXE_SBI_H

/** @file
 *
 * Supervisor Binary Interface (SBI)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

/** An SBI function return value */
struct sbi_return {
	/** Error status (returned in a0) */
	long error;
	/** Data value (returned in a1) */
	long value;
};

/**
 * @defgroup sbierrors SBI errors
 *
 * *{
 */
#define SBI_SUCCESS 0			/**< Completed successfully */
#define SBI_ERR_FAILED -1		/**< Failed */
#define SBI_ERR_NOT_SUPPORTED -2	/**< Not supported */
#define SBI_ERR_INVALID_PARAM -3	/**< Invalid parameter(s) */
#define SBI_ERR_DENIED -4		/**< Denied or not allowed */
#define SBI_ERR_INVALID_ADDRESS -5	/**< Invalid address(es) */
#define SBI_ERR_ALREADY_AVAILABLE -6	/**< Already available */
#define SBI_ERR_ALREADY_STARTED -7	/**< Already started */
#define SBI_ERR_ALREADY_STOPPED -8	/**< Already stopped */
#define SBI_ERR_NO_SHMEM -9		/**< Shared memory not available */
#define SBI_ERR_INVALID_STATE -10	/**< Invalid state */
#define SBI_ERR_BAD_RANGE -11		/**< Bad (or invalid) range */
#define SBI_ERR_TIMEOUT -12		/**< Failed due to timeout */
#define SBI_ERR_IO -13			/**< Input/output error */
/** @} */

/** Construct SBI extension ID */
#define SBI_EID( c1, c2, c3, c4 ) \
	( (int) ( ( (c1) << 24 ) | ( (c2) << 16 ) | ( (c3) << 8 ) | (c4) ) )

/**
 * Call supervisor with no parameters
 *
 * @v eid		Extension ID
 * @v fid		Function ID
 * @ret ret		Return value
 */
static inline __attribute__ (( always_inline )) struct sbi_return
sbi_ecall_0 ( int eid, int fid ) {
	register unsigned long a7 asm ( "a7" ) = ( ( long ) eid );
	register unsigned long a6 asm ( "a6" ) = ( ( long ) fid );
	register unsigned long a0 asm ( "a0" );
	register unsigned long a1 asm ( "a1" );
	struct sbi_return ret;

	__asm__ __volatile__ ( "ecall"
			       : "=r" ( a0 ), "=r" ( a1 )
			       : "r" ( a6 ), "r" ( a7 )
			       : "memory" );
	ret.error = a0;
	ret.value = a1;
	return ret;
}

/**
 * Call supervisor with one parameter
 *
 * @v eid		Extension ID
 * @v fid		Function ID
 * @v p0		Parameter 0
 * @ret ret		Return value
 */
static inline __attribute__ (( always_inline )) struct sbi_return
sbi_ecall_1 ( int eid, int fid, unsigned long p0 ) {
	register unsigned long a7 asm ( "a7" ) = ( ( long ) eid );
	register unsigned long a6 asm ( "a6" ) = ( ( long ) fid );
	register unsigned long a0 asm ( "a0" ) = p0;
	register unsigned long a1 asm ( "a1" );
	struct sbi_return ret;

	__asm__ __volatile__ ( "ecall"
			       : "+r" ( a0 ), "=r" ( a1 )
			       : "r" ( a6 ), "r" ( a7 )
			       : "memory" );
	ret.error = a0;
	ret.value = a1;
	return ret;
}

/**
 * Call supervisor with two parameters
 *
 * @v eid		Extension ID
 * @v fid		Function ID
 * @v p0		Parameter 0
 * @v p1		Parameter 1
 * @ret ret		Return value
 */
static inline __attribute__ (( always_inline )) struct sbi_return
sbi_ecall_2 ( int eid, int fid, unsigned long p0, unsigned long p1 ) {
	register unsigned long a7 asm ( "a7" ) = ( ( long ) eid );
	register unsigned long a6 asm ( "a6" ) = ( ( long ) fid );
	register unsigned long a0 asm ( "a0" ) = p0;
	register unsigned long a1 asm ( "a1" ) = p1;
	struct sbi_return ret;

	__asm__ __volatile__ ( "ecall"
			       : "+r" ( a0 ), "+r" ( a1 )
			       : "r" ( a6 ), "r" ( a7 )
			       : "memory" );
	ret.error = a0;
	ret.value = a1;
	return ret;
}

/**
 * Call supervisor with three parameters
 *
 * @v eid		Extension ID
 * @v fid		Function ID
 * @v p0		Parameter 0
 * @v p1		Parameter 1
 * @v p2		Parameter 2
 * @ret ret		Return value
 */
static inline __attribute__ (( always_inline )) struct sbi_return
sbi_ecall_3 ( int eid, int fid, unsigned long p0, unsigned long p1,
	      unsigned long p2 ) {
	register unsigned long a7 asm ( "a7" ) = ( ( long ) eid );
	register unsigned long a6 asm ( "a6" ) = ( ( long ) fid );
	register unsigned long a0 asm ( "a0" ) = p0;
	register unsigned long a1 asm ( "a1" ) = p1;
	register unsigned long a2 asm ( "a2" ) = p2;
	struct sbi_return ret;

	__asm__ __volatile__ ( "ecall"
			       : "+r" ( a0 ), "+r" ( a1 )
			       : "r" ( a2 ), "r" ( a6 ), "r" ( a7 )
			       : "memory" );
	ret.error = a0;
	ret.value = a1;
	return ret;
}

/**
 * Call supervisor with no parameters
 *
 * @v fid		Legacy function ID
 * @ret ret		Return value
 */
static inline __attribute__ (( always_inline )) long
sbi_legacy_ecall_0 ( int fid ) {
	register unsigned long a7 asm ( "a7" ) = ( ( long ) fid );
	register unsigned long a0 asm ( "a0" );

	__asm__ __volatile__ ( "ecall"
			       : "=r" ( a0 )
			       : "r" ( a7 )
			       : "memory" );
	return a0;
}

/**
 * Call supervisor with one parameter
 *
 * @v fid		Legacy function ID
 * @v p0		Parameter 0
 * @ret ret		Return value
 */
static inline __attribute__ (( always_inline )) long
sbi_legacy_ecall_1 ( int fid, unsigned long p0 ) {
	register unsigned long a7 asm ( "a7" ) = ( ( long ) fid );
	register unsigned long a0 asm ( "a0" ) = p0;

	__asm__ __volatile__ ( "ecall"
			       : "+r" ( a0 )
			       : "r" ( a7 )
			       : "memory" );
	return a0;
}

/** Convert an SBI error code to an iPXE status code */
#define ESBI( error ) EPLATFORM ( EINFO_EPLATFORM, error )

/** Legacy extensions */
#define SBI_LEGACY_PUTCHAR 0x01		/**< Console Put Character */
#define SBI_LEGACY_GETCHAR 0x02		/**< Console Get Character */
#define SBI_LEGACY_SHUTDOWN 0x08	/**< System Shutdown */

/** System reset extension */
#define SBI_SRST SBI_EID ( 'S', 'R', 'S', 'T' )
#define SBI_SRST_SYSTEM_RESET 0x00	/**< Reset system */
#define SBI_RESET_SHUTDOWN 0x00000000	/**< Shutdown */
#define SBI_RESET_COLD 0x00000001	/**< Cold reboot */
#define SBI_RESET_WARM 0x00000002	/**< Warm reboot */

/** Debug console extension */
#define SBI_DBCN SBI_EID ( 'D', 'B', 'C', 'N' )
#define SBI_DBCN_WRITE 0x00		/**< Console Write */
#define SBI_DBCN_READ 0x01		/**< Console Read */
#define SBI_DBCN_WRITE_BYTE 0x02	/**< Console Write Byte */

#endif /* _IPXE_SBI_H */
