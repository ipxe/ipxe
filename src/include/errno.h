#ifndef ERRNO_H
#define ERRNO_H

/** @file
 *
 * Error codes
 *
 * Return status codes as used within gPXE are designed to allow for
 * maximum visibility into the source of an error even in an end-user
 * build with no debugging.  They are constructed in three parts: a
 * PXE error code, a POSIX error code, and a gPXE-specific error code.
 *
 * The low byte is the closest equivalent PXE error code
 * (e.g. PXENV_STATUS_OUT_OF_RESOURCES), and is the only part of the
 * error that will be returned via the PXE API, since PXE has
 * predefined error codes.
 *
 * The next byte is the closest equivalent POSIX error code
 * (e.g. ENOMEM).
 *
 * The remaining bytes are the gPXE-specific error code, which allow
 * us to disambiguate between errors which should have the same POSIX
 * error code but which mean very different things to the user
 * (e.g. ENOENT due to a DNS name not existing versus ENOENT due to
 * a web server returning HTTP/404 Not Found).
 *
 * The convention within the code is that errors are negative and
 * expressed as the bitwise OR of a POSIX error code and (optionally)
 * a gPXE error code, as in
 *
 *     return -( ENOENT | NO_SUCH_FILE );
 *
 * The POSIX error code is #defined to include the closest matching
 * PXE error code (which, in most cases, is just
 * PXENV_STATUS_FAILURE), so we don't need to litter the codebase with
 * PXEisms.
 *
 * Functions that wish to return failure should be declared as
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
#define ENOERR				      ( PXENV_STATUS_SUCCESS | 0x0000 )

/** Arg list too long */
#define E2BIG				     ( PXENV_STATUS_BAD_FUNC | 0x0100 )

/** Permission denied */
#define EACCES			( PXENV_STATUS_TFTP_ACCESS_VIOLATION | 0x0200 )

/** Address in use */
#define EADDRINUSE			     ( PXENV_STATUS_UDP_OPEN | 0x0300 )

/** Address not available */
#define EADDRNOTAVAIL			     ( PXENV_STATUS_UDP_OPEN | 0x0400 )

/** Address family not supported */
#define EAFNOSUPPORT			  ( PXENV_STATUS_UNSUPPORTED | 0x0500 )

/** Resource temporarily unavailable */
#define EAGAIN				      ( PXENV_STATUS_FAILURE | 0x0600 )

/** Connection already in progress */
#define EALREADY			     ( PXENV_STATUS_UDP_OPEN | 0x0700 )

/** Bad file descriptor */
#define EBADF				  ( PXENV_STATUS_TFTP_CLOSED | 0x0800 )

/** Bad message */
#define EBADMSG				      ( PXENV_STATUS_FAILURE | 0x0900 )

/** Resource busy */
#define EBUSY			     ( PXENV_STATUS_OUT_OF_RESOURCES | 0x0a00 )

/** Operation canceled */
#define ECANCELED	   ( PXENV_STATUS_BINL_CANCELED_BY_KEYSTROKE | 0x0b00 )

/** No child processes */
#define ECHILD			  ( PXENV_STATUS_TFTP_FILE_NOT_FOUND | 0x0c00 )

/** Connection aborted */
#define ECONNABORTED ( PXENV_STATUS_TFTP_CANNOT_READ_FROM_CONNECTION | 0x0d00 )

/** Connection refused */
#define ECONNREFUSED	  ( PXENV_STATUS_TFTP_CANNOT_OPEN_CONNECTION | 0x0e00 )

/** Connection reset */
#define ECONNRESET   ( PXENV_STATUS_TFTP_CANNOT_READ_FROM_CONNECTION | 0x0f00 )

/** Resource deadlock avoided */
#define EDEADLK				      ( PXENV_STATUS_FAILURE | 0x1000 )

/** Destination address required */
#define EDESTADDRREQ			     ( PXENV_STATUS_BAD_FUNC | 0x1100 )

/** Domain error */
#define EDOM				      ( PXENV_STATUS_FAILURE | 0x1200 )

/** Reserved */
#define EDQUOT				      ( PXENV_STATUS_FAILURE | 0x1300 )

/** File exists */
#define EEXIST				      ( PXENV_STATUS_FAILURE | 0x1400 )

/** Bad address */
#define EFAULT				( PXENV_STATUS_MCOPY_PROBLEM | 0x1500 )

/** File too large */
#define EFBIG				( PXENV_STATUS_MCOPY_PROBLEM | 0x1600 )

/** Host is unreachable */
#define EHOSTUNREACH			  ( PXENV_STATUS_ARP_TIMEOUT | 0x1700 )

/** Identifier removed */
#define EIDRM				      ( PXENV_STATUS_FAILURE | 0x1800 )

/** Illegal byte sequence */
#define EILSEQ				      ( PXENV_STATUS_FAILURE | 0x1900 )

/** Operation in progress */
#define EINPROGRESS			      ( PXENV_STATUS_FAILURE | 0x1a00 )

/** Interrupted function call */
#define EINTR				      ( PXENV_STATUS_FAILURE | 0x1b00 )

/** Invalid argument */
#define EINVAL				     ( PXENV_STATUS_BAD_FUNC | 0x1c00 )

/** Input/output error */
#define EIO	     ( PXENV_STATUS_TFTP_CANNOT_READ_FROM_CONNECTION | 0x1d00 )

/** Socket is connected */
#define EISCONN				     ( PXENV_STATUS_UDP_OPEN | 0x1e00 )

/** Is a directory */
#define EISDIR				      ( PXENV_STATUS_FAILURE | 0x1f00 )

/** Too many levels of symbolic links */
#define ELOOP				      ( PXENV_STATUS_FAILURE | 0x2000 )

/** Too many open files */
#define EMFILE			     ( PXENV_STATUS_OUT_OF_RESOURCES | 0x2100 )

/** Too many links */
#define EMLINK				      ( PXENV_STATUS_FAILURE | 0x2200 )

/** Inappropriate message buffer length */
#define EMSGSIZE			     ( PXENV_STATUS_BAD_FUNC | 0x2300 )

/** Reserved */
#define EMULTIHOP			      ( PXENV_STATUS_FAILURE | 0x2400 )

/** Filename too long */
#define ENAMETOOLONG			      ( PXENV_STATUS_FAILURE | 0x2500 )

/** Network is down */
#define ENETDOWN			  ( PXENV_STATUS_ARP_TIMEOUT | 0x2600 )

/** Connection aborted by network */
#define ENETRESET			      ( PXENV_STATUS_FAILURE | 0x2700 )

/** Network unreachable */
#define ENETUNREACH			  ( PXENV_STATUS_ARP_TIMEOUT | 0x2800 )

/** Too many open files in system */
#define ENFILE			     ( PXENV_STATUS_OUT_OF_RESOURCES | 0x2900 )

/** No buffer space available */
#define ENOBUFS			     ( PXENV_STATUS_OUT_OF_RESOURCES | 0x2a00 )

/** No message is available on the STREAM head read queue */
#define ENODATA				      ( PXENV_STATUS_FAILURE | 0x2b00 )

/** No such device */
#define ENODEV			  ( PXENV_STATUS_TFTP_FILE_NOT_FOUND | 0x2c00 )

/** No such file or directory */
#define ENOENT			  ( PXENV_STATUS_TFTP_FILE_NOT_FOUND | 0x2d00 )

/** Exec format error */
#define ENOEXEC				      ( PXENV_STATUS_FAILURE | 0x2e00 )

/** No locks available */
#define ENOLCK				      ( PXENV_STATUS_FAILURE | 0x2f00 )

/** Reserved */
#define ENOLINK				      ( PXENV_STATUS_FAILURE | 0x3000 )

/** Not enough space */
#define ENOMEM			     ( PXENV_STATUS_OUT_OF_RESOURCES | 0x3100 )

/** No message of the desired type */
#define ENOMSG				      ( PXENV_STATUS_FAILURE | 0x3200 )

/** Protocol not available */
#define ENOPROTOOPT			  ( PXENV_STATUS_UNSUPPORTED | 0x3300 )

/** No space left on device */
#define ENOSPC			     ( PXENV_STATUS_OUT_OF_RESOURCES | 0x3400 )

/** No STREAM resources */
#define ENOSR			     ( PXENV_STATUS_OUT_OF_RESOURCES | 0x3500 )

/** Not a STREAM */
#define ENOSTR				      ( PXENV_STATUS_FAILURE | 0x3600 )

/** Function not implemented */
#define ENOSYS				  ( PXENV_STATUS_UNSUPPORTED | 0x3700 )

/** The socket is not connected */
#define ENOTCONN			      ( PXENV_STATUS_FAILURE | 0x3800 )

/** Not a directory */
#define ENOTDIR				      ( PXENV_STATUS_FAILURE | 0x3900 )

/** Directory not empty */
#define ENOTEMPTY			      ( PXENV_STATUS_FAILURE | 0x3a00 )

/** Not a socket */
#define ENOTSOCK			      ( PXENV_STATUS_FAILURE | 0x3b00 )

/** Not supported */
#define ENOTSUP				  ( PXENV_STATUS_UNSUPPORTED | 0x3c00 )

/** Inappropriate I/O control operation */
#define ENOTTY				      ( PXENV_STATUS_FAILURE | 0x3d00 )

/** No such device or address */
#define ENXIO			  ( PXENV_STATUS_TFTP_FILE_NOT_FOUND | 0x3e00 )

/** Operation not supported on socket */
#define EOPNOTSUPP			  ( PXENV_STATUS_UNSUPPORTED | 0x3f00 )

/** Value too large to be stored in data type */
#define EOVERFLOW			      ( PXENV_STATUS_FAILURE | 0x4000 )

/** Operation not permitted */
#define EPERM			( PXENV_STATUS_TFTP_ACCESS_VIOLATION | 0x4100 )

/** Broken pipe */
#define EPIPE				      ( PXENV_STATUS_FAILURE | 0x4200 )

/** Protocol error */
#define EPROTO				      ( PXENV_STATUS_FAILURE | 0x4300 )

/** Protocol not supported */
#define EPROTONOSUPPORT			  ( PXENV_STATUS_UNSUPPORTED | 0x4400 )

/** Protocol wrong type for socket */
#define EPROTOTYPE			      ( PXENV_STATUS_FAILURE | 0x4500 )

/** Result too large */
#define ERANGE				      ( PXENV_STATUS_FAILURE | 0x4600 )

/** Read-only file system */
#define EROFS				      ( PXENV_STATUS_FAILURE | 0x4700 )

/** Invalid seek */
#define ESPIPE				      ( PXENV_STATUS_FAILURE | 0x4800 )

/** No such process */
#define ESRCH			  ( PXENV_STATUS_TFTP_FILE_NOT_FOUND | 0x4900 )

/** Stale file handle */
#define ESTALE				      ( PXENV_STATUS_FAILURE | 0x4a00 )

/** STREAM ioctl() timeout */
#define ETIME				      ( PXENV_STATUS_FAILURE | 0x4b00 )

/** Operation timed out */
#define ETIMEDOUT		    ( PXENV_STATUS_TFTP_READ_TIMEOUT | 0x4c00 )

/** Text file busy */
#define ETXTBSY				      ( PXENV_STATUS_FAILURE | 0x4d00 )

/** Operation would block */
#define EWOULDBLOCK			      ( PXENV_STATUS_FAILURE | 0x4e00 )

/** Improper link */
#define EXDEV				      ( PXENV_STATUS_FAILURE | 0x4f00 )

/** @} */

/**
 * @defgroup gpxeerrors gPXE-specific error codes
 *
 * The names, meanings, and values of these error codes are defined by
 * this file.  A gPXE-specific error code should be defined only where
 * the POSIX error code does not identify the error with sufficient
 * specificity.  For example, ENOMEM probably encapsulates everything
 * that needs to be known about the error (we've run out of heap
 * space), while EACCES does not (did the server refuse the
 * connection, or did we decide that the server failed to provide a
 * valid SSL/TLS certificate?).
 *
 * @{
 */

/** @} */

extern int errno;

#endif /* ERRNO_H */
