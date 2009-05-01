#ifndef ERRNO_H
#define ERRNO_H

FILE_LICENCE ( GPL2_OR_LATER );

/** @file
 *
 * Error codes
 *
 * Return status codes as used within gPXE are designed to allow for
 * maximum visibility into the source of an error even in an end-user
 * build with no debugging.  They are constructed as follows:
 *
 * Bits 7-0 : PXE error code
 *
 * This is the closest equivalent PXE error code
 * (e.g. PXENV_STATUS_OUT_OF_RESOURCES), and is the only part of the
 * error that will be returned via the PXE API, since PXE has
 * predefined error codes.
 *
 * Bits 12-8 : Per-file disambiguator
 *
 * When the same error number can be generated from multiple points
 * within a file, this field can be used to identify the unique
 * instance.
 *
 * Bits 23-13 : File identifier
 *
 * This is a unique identifier for the file generating the error
 * (e.g. ERRFILE_tcp for tcp.c).
 *
 * Bits 30-24 : POSIX error code
 *
 * This is the closest equivalent POSIX error code (e.g. ENOMEM).
 *
 * Bit 31 : Reserved
 *
 * Errors are usually return as negative error numbers (e.g. -EINVAL);
 * bit 31 is therefore unusable.
 *
 *
 * The convention within the code is that errors are negative and
 * expressed using the POSIX error code and (optionally) a per-file
 * disambiguator, e.g.
 *
 *     return -EINVAL;
 *
 * or
 *
 *     #define ETCP_BAD_CHECKSUM EUNIQ_02
 *     return -( EINVAL | ETCP_BAD_CHECKSUM )
 *
 * By various bits of preprocessor magic, the PXE error code and file
 * identifier are already incorporated into the definition of the
 * POSIX error code, which keeps the code relatively clean.
 *
 *
 * Functions that wish to return failures should be declared as
 * returning an integer @c rc "Return status code".  A return value of
 * zero indicates success, a non-zero value indicates failure.  The
 * return value can be passed directly to strerror() in order to
 * generate a human-readable error message, e.g.
 *
 *     if ( ( rc = some_function ( ... ) ) != 0 ) {
 *         DBG ( "Whatever I was trying to do failed: %s\n", strerror ( rc ) );
 *         return rc;
 *     }
 *
 * As illustrated in the above example, error returns should generally
 * be directly propagated upward to the calling function.
 *
 */

/* Get definitions for file identifiers */
#include <gpxe/errfile.h>

/* If we do not have a valid file identifier, generate a compiler
 * warning upon usage of any error codes.  (Don't just use a #warning,
 * because some files include errno.h but don't ever actually use any
 * error codes.)
 */
#if ! ERRFILE
extern char missing_errfile_declaration[] __attribute__ (( deprecated ));
#undef ERRFILE
#define ERRFILE ( 0 * ( ( int ) missing_errfile_declaration ) )
#endif

/** Derive PXENV_STATUS code from gPXE error number */
#define PXENV_STATUS( rc ) ( (-(rc)) & 0x00ff )

/**
 * @defgroup pxeerrors PXE error codes
 *
 * The names, meanings and values of these error codes are defined by
 * the PXE specification.
 *
 * @{
 */

/* Generic errors */
#define	PXENV_STATUS_SUCCESS					       0x0000
#define	PXENV_STATUS_FAILURE					       0x0001
#define	PXENV_STATUS_BAD_FUNC					       0x0002
#define	PXENV_STATUS_UNSUPPORTED				       0x0003
#define	PXENV_STATUS_KEEP_UNDI					       0x0004
#define	PXENV_STATUS_KEEP_ALL					       0x0005
#define	PXENV_STATUS_OUT_OF_RESOURCES				       0x0006

/* ARP errors (0x0010 to 0x001f) */
#define	PXENV_STATUS_ARP_TIMEOUT				       0x0011

/* Base-Code state errors */
#define	PXENV_STATUS_UDP_CLOSED					       0x0018
#define	PXENV_STATUS_UDP_OPEN					       0x0019
#define	PXENV_STATUS_TFTP_CLOSED				       0x001a
#define	PXENV_STATUS_TFTP_OPEN					       0x001b

/* BIOS/system errors (0x0020 to 0x002f) */
#define	PXENV_STATUS_MCOPY_PROBLEM				       0x0020
#define	PXENV_STATUS_BIS_INTEGRITY_FAILURE			       0x0021
#define	PXENV_STATUS_BIS_VALIDATE_FAILURE			       0x0022
#define	PXENV_STATUS_BIS_INIT_FAILURE				       0x0023
#define	PXENV_STATUS_BIS_SHUTDOWN_FAILURE			       0x0024
#define	PXENV_STATUS_BIS_GBOA_FAILURE				       0x0025
#define	PXENV_STATUS_BIS_FREE_FAILURE				       0x0026
#define	PXENV_STATUS_BIS_GSI_FAILURE				       0x0027
#define	PXENV_STATUS_BIS_BAD_CKSUM				       0x0028

/* TFTP/MTFTP errors (0x0030 to 0x003f) */
#define	PXENV_STATUS_TFTP_CANNOT_ARP_ADDRESS			       0x0030
#define	PXENV_STATUS_TFTP_OPEN_TIMEOUT				       0x0032
#define	PXENV_STATUS_TFTP_UNKNOWN_OPCODE			       0x0033
#define	PXENV_STATUS_TFTP_READ_TIMEOUT				       0x0035
#define	PXENV_STATUS_TFTP_ERROR_OPCODE				       0x0036
#define	PXENV_STATUS_TFTP_CANNOT_OPEN_CONNECTION		       0x0038
#define	PXENV_STATUS_TFTP_CANNOT_READ_FROM_CONNECTION		       0x0039
#define	PXENV_STATUS_TFTP_TOO_MANY_PACKAGES			       0x003a
#define	PXENV_STATUS_TFTP_FILE_NOT_FOUND			       0x003b
#define	PXENV_STATUS_TFTP_ACCESS_VIOLATION			       0x003c
#define	PXENV_STATUS_TFTP_NO_MCAST_ADDRESS			       0x003d
#define	PXENV_STATUS_TFTP_NO_FILESIZE				       0x003e
#define	PXENV_STATUS_TFTP_INVALID_PACKET_SIZE			       0x003f

/* Reserved errors 0x0040 to 0x004f) */

/* DHCP/BOOTP errors (0x0050 to 0x005f) */
#define	PXENV_STATUS_DHCP_TIMEOUT				       0x0051
#define	PXENV_STATUS_DHCP_NO_IP_ADDRESS				       0x0052
#define	PXENV_STATUS_DHCP_NO_BOOTFILE_NAME			       0x0053
#define	PXENV_STATUS_DHCP_BAD_IP_ADDRESS			       0x0054

/* Driver errors (0x0060 to 0x006f) */
#define	PXENV_STATUS_UNDI_INVALID_FUNCTION			       0x0060
#define	PXENV_STATUS_UNDI_MEDIATEST_FAILED			       0x0061
#define	PXENV_STATUS_UNDI_CANNOT_INIT_NIC_FOR_MCAST		       0x0062
#define	PXENV_STATUS_UNDI_CANNOT_INITIALIZE_NIC			       0x0063
#define	PXENV_STATUS_UNDI_CANNOT_INITIALIZE_PHY			       0x0064
#define	PXENV_STATUS_UNDI_CANNOT_READ_CONFIG_DATA		       0x0065
#define	PXENV_STATUS_UNDI_CANNOT_READ_INIT_DATA			       0x0066
#define	PXENV_STATUS_UNDI_BAD_MAC_ADDRESS			       0x0067
#define	PXENV_STATUS_UNDI_BAD_EEPROM_CHECKSUM			       0x0068
#define	PXENV_STATUS_UNDI_ERROR_SETTING_ISR			       0x0069
#define	PXENV_STATUS_UNDI_INVALID_STATE				       0x006a
#define	PXENV_STATUS_UNDI_TRANSMIT_ERROR			       0x006b
#define	PXENV_STATUS_UNDI_INVALID_PARAMETER			       0x006c

/* ROM and NBP bootstrap errors (0x0070 to 0x007f) */
#define	PXENV_STATUS_BSTRAP_PROMPT_MENU				       0x0074
#define	PXENV_STATUS_BSTRAP_MCAST_ADDR				       0x0076
#define	PXENV_STATUS_BSTRAP_MISSING_LIST			       0x0077
#define	PXENV_STATUS_BSTRAP_NO_RESPONSE				       0x0078
#define	PXENV_STATUS_BSTRAP_FILE_TOO_BIG			       0x0079

/* Environment NBP errors (0x0080 to 0x008f) */

/* Reserved errors (0x0090 to 0x009f) */

/* Miscellaneous errors (0x00a0 to 0x00af) */
#define	PXENV_STATUS_BINL_CANCELED_BY_KEYSTROKE			       0x00a0
#define	PXENV_STATUS_BINL_NO_PXE_SERVER				       0x00a1
#define	PXENV_STATUS_NOT_AVAILABLE_IN_PMODE			       0x00a2
#define	PXENV_STATUS_NOT_AVAILABLE_IN_RMODE			       0x00a3

/* BUSD errors (0x00b0 to 0x00bf) */
#define	PXENV_STATUS_BUSD_DEVICE_NOT_SUPPORTED			       0x00b0

/* Loader errors (0x00c0 to 0x00cf) */
#define	PXENV_STATUS_LOADER_NO_FREE_BASE_MEMORY			       0x00c0
#define	PXENV_STATUS_LOADER_NO_BC_ROMID				       0x00c1
#define	PXENV_STATUS_LOADER_BAD_BC_ROMID			       0x00c2
#define	PXENV_STATUS_LOADER_BAD_BC_RUNTIME_IMAGE		       0x00c3
#define	PXENV_STATUS_LOADER_NO_UNDI_ROMID			       0x00c4
#define	PXENV_STATUS_LOADER_BAD_UNDI_ROMID			       0x00c5
#define	PXENV_STATUS_LOADER_BAD_UNDI_DRIVER_IMAGE		       0x00c6
#define	PXENV_STATUS_LOADER_NO_PXE_STRUCT			       0x00c8
#define	PXENV_STATUS_LOADER_NO_PXENV_STRUCT			       0x00c9
#define	PXENV_STATUS_LOADER_UNDI_START				       0x00ca
#define	PXENV_STATUS_LOADER_BC_START				       0x00cb

/** @} */

/**
 * @defgroup posixerrors POSIX error codes
 *
 * The names and meanings (but not the values) of these error codes
 * are defined by POSIX.  We choose to assign unique values which
 * incorporate the closest equivalent PXE error code, so that code may
 * simply use ENOMEM, rather than having to use the cumbersome
 * (ENOMEM|PXENV_STATUS_OUT_OF_RESOURCES).
 *
 * @{
 */

/** Operation completed successfully */
#define ENOERR			( ERRFILE | PXENV_STATUS_SUCCESS | 0x00000000 )

/** Arg list too long */
#define E2BIG		       ( ERRFILE | PXENV_STATUS_BAD_FUNC | 0x01000000 )

/** Permission denied */
#define EACCES	  ( ERRFILE | PXENV_STATUS_TFTP_ACCESS_VIOLATION | 0x02000000 )

/** Address in use */
#define EADDRINUSE	       ( ERRFILE | PXENV_STATUS_UDP_OPEN | 0x03000000 )

/** Address not available */
#define EADDRNOTAVAIL	       ( ERRFILE | PXENV_STATUS_UDP_OPEN | 0x04000000 )

/** Address family not supported */
#define EAFNOSUPPORT	    ( ERRFILE | PXENV_STATUS_UNSUPPORTED | 0x05000000 )

/** Resource temporarily unavailable */
#define EAGAIN			( ERRFILE | PXENV_STATUS_FAILURE | 0x06000000 )

/** Connection already in progress */
#define EALREADY	       ( ERRFILE | PXENV_STATUS_UDP_OPEN | 0x07000000 )

/** Bad file descriptor */
#define EBADF		    ( ERRFILE | PXENV_STATUS_TFTP_CLOSED | 0x08000000 )

/** Bad message */
#define EBADMSG			( ERRFILE | PXENV_STATUS_FAILURE | 0x09000000 )

/** Resource busy */
#define EBUSY	       ( ERRFILE | PXENV_STATUS_OUT_OF_RESOURCES | 0x0a000000 )

/** Operation canceled */
#define ECANCELED \
	     ( ERRFILE | PXENV_STATUS_BINL_CANCELED_BY_KEYSTROKE | 0x0b000000 )

/** No child processes */
#define ECHILD	    ( ERRFILE | PXENV_STATUS_TFTP_FILE_NOT_FOUND | 0x0c000000 )

/** Connection aborted */
#define ECONNABORTED \
       ( ERRFILE | PXENV_STATUS_TFTP_CANNOT_READ_FROM_CONNECTION | 0x0d000000 )

/** Connection refused */
#define ECONNREFUSED \
	    ( ERRFILE | PXENV_STATUS_TFTP_CANNOT_OPEN_CONNECTION | 0x0e000000 )

/** Connection reset */
#define ECONNRESET \
       ( ERRFILE | PXENV_STATUS_TFTP_CANNOT_READ_FROM_CONNECTION | 0x0f000000 )

/** Resource deadlock avoided */
#define EDEADLK			( ERRFILE | PXENV_STATUS_FAILURE | 0x10000000 )

/** Destination address required */
#define EDESTADDRREQ	       ( ERRFILE | PXENV_STATUS_BAD_FUNC | 0x11000000 )

/** Domain error */
#define EDOM			( ERRFILE | PXENV_STATUS_FAILURE | 0x12000000 )

/** Reserved */
#define EDQUOT			( ERRFILE | PXENV_STATUS_FAILURE | 0x13000000 )

/** File exists */
#define EEXIST			( ERRFILE | PXENV_STATUS_FAILURE | 0x14000000 )

/** Bad address */
#define EFAULT		  ( ERRFILE | PXENV_STATUS_MCOPY_PROBLEM | 0x15000000 )

/** File too large */
#define EFBIG		  ( ERRFILE | PXENV_STATUS_MCOPY_PROBLEM | 0x16000000 )

/** Host is unreachable */
#define EHOSTUNREACH	    ( ERRFILE | PXENV_STATUS_ARP_TIMEOUT | 0x17000000 )

/** Identifier removed */
#define EIDRM			( ERRFILE | PXENV_STATUS_FAILURE | 0x18000000 )

/** Illegal byte sequence */
#define EILSEQ			( ERRFILE | PXENV_STATUS_FAILURE | 0x19000000 )

/** Operation in progress */
#define EINPROGRESS		( ERRFILE | PXENV_STATUS_FAILURE | 0x1a000000 )

/** Interrupted function call */
#define EINTR			( ERRFILE | PXENV_STATUS_FAILURE | 0x1b000000 )

/** Invalid argument */
#define EINVAL		       ( ERRFILE | PXENV_STATUS_BAD_FUNC | 0x1c000000 )

/** Input/output error */
#define EIO \
       ( ERRFILE | PXENV_STATUS_TFTP_CANNOT_READ_FROM_CONNECTION | 0x1d000000 )

/** Socket is connected */
#define EISCONN		       ( ERRFILE | PXENV_STATUS_UDP_OPEN | 0x1e000000 )

/** Is a directory */
#define EISDIR			( ERRFILE | PXENV_STATUS_FAILURE | 0x1f000000 )

/** Too many levels of symbolic links */
#define ELOOP			( ERRFILE | PXENV_STATUS_FAILURE | 0x20000000 )

/** Too many open files */
#define EMFILE	       ( ERRFILE | PXENV_STATUS_OUT_OF_RESOURCES | 0x21000000 )

/** Too many links */
#define EMLINK			( ERRFILE | PXENV_STATUS_FAILURE | 0x22000000 )

/** Inappropriate message buffer length */
#define EMSGSIZE	       ( ERRFILE | PXENV_STATUS_BAD_FUNC | 0x23000000 )

/** Reserved */
#define EMULTIHOP		( ERRFILE | PXENV_STATUS_FAILURE | 0x24000000 )

/** Filename too long */
#define ENAMETOOLONG		( ERRFILE | PXENV_STATUS_FAILURE | 0x25000000 )

/** Network is down */
#define ENETDOWN	    ( ERRFILE | PXENV_STATUS_ARP_TIMEOUT | 0x26000000 )

/** Connection aborted by network */
#define ENETRESET		( ERRFILE | PXENV_STATUS_FAILURE | 0x27000000 )

/** Network unreachable */
#define ENETUNREACH	    ( ERRFILE | PXENV_STATUS_ARP_TIMEOUT | 0x28000000 )

/** Too many open files in system */
#define ENFILE	       ( ERRFILE | PXENV_STATUS_OUT_OF_RESOURCES | 0x29000000 )

/** No buffer space available */
#define ENOBUFS	       ( ERRFILE | PXENV_STATUS_OUT_OF_RESOURCES | 0x2a000000 )

/** No message is available on the STREAM head read queue */
#define ENODATA			( ERRFILE | PXENV_STATUS_FAILURE | 0x2b000000 )

/** No such device */
#define ENODEV	    ( ERRFILE | PXENV_STATUS_TFTP_FILE_NOT_FOUND | 0x2c000000 )

/** No such file or directory */
#define ENOENT	    ( ERRFILE | PXENV_STATUS_TFTP_FILE_NOT_FOUND | 0x2d000000 )

/** Exec format error */
#define ENOEXEC			( ERRFILE | PXENV_STATUS_FAILURE | 0x2e000000 )

/** No locks available */
#define ENOLCK			( ERRFILE | PXENV_STATUS_FAILURE | 0x2f000000 )

/** Reserved */
#define ENOLINK			( ERRFILE | PXENV_STATUS_FAILURE | 0x30000000 )

/** Not enough space */
#define ENOMEM	       ( ERRFILE | PXENV_STATUS_OUT_OF_RESOURCES | 0x31000000 )

/** No message of the desired type */
#define ENOMSG			( ERRFILE | PXENV_STATUS_FAILURE | 0x32000000 )

/** Protocol not available */
#define ENOPROTOOPT	    ( ERRFILE | PXENV_STATUS_UNSUPPORTED | 0x33000000 )

/** No space left on device */
#define ENOSPC	       ( ERRFILE | PXENV_STATUS_OUT_OF_RESOURCES | 0x34000000 )

/** No STREAM resources */
#define ENOSR	       ( ERRFILE | PXENV_STATUS_OUT_OF_RESOURCES | 0x35000000 )

/** Not a STREAM */
#define ENOSTR			( ERRFILE | PXENV_STATUS_FAILURE | 0x36000000 )

/** Function not implemented */
#define ENOSYS		    ( ERRFILE | PXENV_STATUS_UNSUPPORTED | 0x37000000 )

/** The socket is not connected */
#define ENOTCONN		( ERRFILE | PXENV_STATUS_FAILURE | 0x38000000 )

/** Not a directory */
#define ENOTDIR			( ERRFILE | PXENV_STATUS_FAILURE | 0x39000000 )

/** Directory not empty */
#define ENOTEMPTY		( ERRFILE | PXENV_STATUS_FAILURE | 0x3a000000 )

/** Not a socket */
#define ENOTSOCK		( ERRFILE | PXENV_STATUS_FAILURE | 0x3b000000 )

/** Not supported */
#define ENOTSUP		    ( ERRFILE | PXENV_STATUS_UNSUPPORTED | 0x3c000000 )

/** Inappropriate I/O control operation */
#define ENOTTY			( ERRFILE | PXENV_STATUS_FAILURE | 0x3d000000 )

/** No such device or address */
#define ENXIO	    ( ERRFILE | PXENV_STATUS_TFTP_FILE_NOT_FOUND | 0x3e000000 )

/** Operation not supported on socket */
#define EOPNOTSUPP	    ( ERRFILE | PXENV_STATUS_UNSUPPORTED | 0x3f000000 )

/** Value too large to be stored in data type */
#define EOVERFLOW		( ERRFILE | PXENV_STATUS_FAILURE | 0x40000000 )

/** Operation not permitted */
#define EPERM	  ( ERRFILE | PXENV_STATUS_TFTP_ACCESS_VIOLATION | 0x41000000 )

/** Broken pipe */
#define EPIPE			( ERRFILE | PXENV_STATUS_FAILURE | 0x42000000 )

/** Protocol error */
#define EPROTO			( ERRFILE | PXENV_STATUS_FAILURE | 0x43000000 )

/** Protocol not supported */
#define EPROTONOSUPPORT	    ( ERRFILE | PXENV_STATUS_UNSUPPORTED | 0x44000000 )

/** Protocol wrong type for socket */
#define EPROTOTYPE		( ERRFILE | PXENV_STATUS_FAILURE | 0x45000000 )

/** Result too large */
#define ERANGE			( ERRFILE | PXENV_STATUS_FAILURE | 0x46000000 )

/** Read-only file system */
#define EROFS			( ERRFILE | PXENV_STATUS_FAILURE | 0x47000000 )

/** Invalid seek */
#define ESPIPE			( ERRFILE | PXENV_STATUS_FAILURE | 0x48000000 )

/** No such process */
#define ESRCH	    ( ERRFILE | PXENV_STATUS_TFTP_FILE_NOT_FOUND | 0x49000000 )

/** Stale file handle */
#define ESTALE			( ERRFILE | PXENV_STATUS_FAILURE | 0x4a000000 )

/** STREAM ioctl() timeout */
#define ETIME			( ERRFILE | PXENV_STATUS_FAILURE | 0x4b000000 )

/** Operation timed out */
#define ETIMEDOUT     ( ERRFILE | PXENV_STATUS_TFTP_READ_TIMEOUT | 0x4c000000 )

/** Text file busy */
#define ETXTBSY			( ERRFILE | PXENV_STATUS_FAILURE | 0x4d000000 )

/** Operation would block (different from EAGAIN!) */
#define EWOULDBLOCK	      ( ERRFILE | PXENV_STATUS_TFTP_OPEN | 0x4e000000 )

/** Improper link */
#define EXDEV			( ERRFILE | PXENV_STATUS_FAILURE | 0x4f000000 )

/** @} */

/**
 * @defgroup euniq Per-file error disambiguators
 *
 * Files which use the same error number multiple times should
 * probably define their own error subspace using these
 * disambiguators.  For example:
 *
 *     #define ETCP_HEADER_TOO_SHORT	EUNIQ_01
 *     #define ETCP_BAD_CHECKSUM	EUNIQ_02
 *
 * @{
 */

#define EUNIQ_01	0x00000100
#define EUNIQ_02	0x00000200
#define EUNIQ_03	0x00000300
#define EUNIQ_04	0x00000400
#define EUNIQ_05	0x00000500
#define EUNIQ_06	0x00000600
#define EUNIQ_07	0x00000700
#define EUNIQ_08	0x00000800
#define EUNIQ_09	0x00000900
#define EUNIQ_0A	0x00000a00
#define EUNIQ_0B	0x00000b00
#define EUNIQ_0C	0x00000c00
#define EUNIQ_0D	0x00000d00
#define EUNIQ_0E	0x00000e00
#define EUNIQ_0F	0x00000f00
#define EUNIQ_10	0x00001000
#define EUNIQ_11	0x00001100
#define EUNIQ_12	0x00001200
#define EUNIQ_13	0x00001300
#define EUNIQ_14	0x00001400
#define EUNIQ_15	0x00001500
#define EUNIQ_16	0x00001600
#define EUNIQ_17	0x00001700
#define EUNIQ_18	0x00001800
#define EUNIQ_19	0x00001900
#define EUNIQ_1A	0x00001a00
#define EUNIQ_1B	0x00001b00
#define EUNIQ_1C	0x00001c00
#define EUNIQ_1D	0x00001d00
#define EUNIQ_1E	0x00001e00
#define EUNIQ_1F	0x00001f00

/** @} */

extern int errno;

#endif /* ERRNO_H */
