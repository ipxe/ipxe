#ifndef ERRNO_H
#define ERRNO_H

/** @file
 *
 * Error codes
 *
 */

/* PXE error codes are determined by the PXE specification */

/* Generic errors */
#define	PXENV_STATUS_SUCCESS				0x00
#define	PXENV_STATUS_FAILURE				0x01
#define	PXENV_STATUS_BAD_FUNC				0x02
#define	PXENV_STATUS_UNSUPPORTED			0x03
#define	PXENV_STATUS_KEEP_UNDI				0x04
#define	PXENV_STATUS_KEEP_ALL				0x05
#define	PXENV_STATUS_OUT_OF_RESOURCES			0x06

/* ARP errors (0x10 to 0x1f) */
#define	PXENV_STATUS_ARP_TIMEOUT			0x11

/* Base-Code state errors */
#define	PXENV_STATUS_UDP_CLOSED				0x18
#define	PXENV_STATUS_UDP_OPEN				0x19
#define	PXENV_STATUS_TFTP_CLOSED			0x1a
#define	PXENV_STATUS_TFTP_OPEN				0x1b

/* BIOS/system errors (0x20 to 0x2f) */
#define	PXENV_STATUS_MCOPY_PROBLEM			0x20
#define	PXENV_STATUS_BIS_INTEGRITY_FAILURE		0x21
#define	PXENV_STATUS_BIS_VALIDATE_FAILURE		0x22
#define	PXENV_STATUS_BIS_INIT_FAILURE			0x23
#define	PXENV_STATUS_BIS_SHUTDOWN_FAILURE		0x24
#define	PXENV_STATUS_BIS_GBOA_FAILURE			0x25
#define	PXENV_STATUS_BIS_FREE_FAILURE			0x26
#define	PXENV_STATUS_BIS_GSI_FAILURE			0x27
#define	PXENV_STATUS_BIS_BAD_CKSUM			0x28

/* TFTP/MTFTP errors (0x30 to 0x3f) */
#define	PXENV_STATUS_TFTP_CANNOT_ARP_ADDRESS		0x30
#define	PXENV_STATUS_TFTP_OPEN_TIMEOUT			0x32
#define	PXENV_STATUS_TFTP_UNKNOWN_OPCODE		0x33
#define	PXENV_STATUS_TFTP_READ_TIMEOUT			0x35
#define	PXENV_STATUS_TFTP_ERROR_OPCODE			0x36
#define	PXENV_STATUS_TFTP_CANNOT_OPEN_CONNECTION	0x38
#define	PXENV_STATUS_TFTP_CANNOT_READ_FROM_CONNECTION	0x39
#define	PXENV_STATUS_TFTP_TOO_MANY_PACKAGES		0x3a
#define	PXENV_STATUS_TFTP_FILE_NOT_FOUND		0x3b
#define	PXENV_STATUS_TFTP_ACCESS_VIOLATION		0x3c
#define	PXENV_STATUS_TFTP_NO_MCAST_ADDRESS		0x3d
#define	PXENV_STATUS_TFTP_NO_FILESIZE			0x3e
#define	PXENV_STATUS_TFTP_INVALID_PACKET_SIZE		0x3f

/* Reserved errors 0x40 to 0x4f) */

/* DHCP/BOOTP errors (0x50 to 0x5f) */
#define	PXENV_STATUS_DHCP_TIMEOUT			0x51
#define	PXENV_STATUS_DHCP_NO_IP_ADDRESS			0x52
#define	PXENV_STATUS_DHCP_NO_BOOTFILE_NAME		0x53
#define	PXENV_STATUS_DHCP_BAD_IP_ADDRESS		0x54

/* Driver errors (0x60 to 0x6f) */
#define	PXENV_STATUS_UNDI_INVALID_FUNCTION		0x60
#define	PXENV_STATUS_UNDI_MEDIATEST_FAILED		0x61
#define	PXENV_STATUS_UNDI_CANNOT_INIT_NIC_FOR_MCAST	0x62
#define	PXENV_STATUS_UNDI_CANNOT_INITIALIZE_NIC		0x63
#define	PXENV_STATUS_UNDI_CANNOT_INITIALIZE_PHY		0x64
#define	PXENV_STATUS_UNDI_CANNOT_READ_CONFIG_DATA	0x65
#define	PXENV_STATUS_UNDI_CANNOT_READ_INIT_DATA		0x66
#define	PXENV_STATUS_UNDI_BAD_MAC_ADDRESS		0x67
#define	PXENV_STATUS_UNDI_BAD_EEPROM_CHECKSUM		0x68
#define	PXENV_STATUS_UNDI_ERROR_SETTING_ISR		0x69
#define	PXENV_STATUS_UNDI_INVALID_STATE			0x6a
#define	PXENV_STATUS_UNDI_TRANSMIT_ERROR		0x6b
#define	PXENV_STATUS_UNDI_INVALID_PARAMETER		0x6c

/* ROM and NBP bootstrap errors (0x70 to 0x7f) */
#define	PXENV_STATUS_BSTRAP_PROMPT_MENU			0x74
#define	PXENV_STATUS_BSTRAP_MCAST_ADDR			0x76
#define	PXENV_STATUS_BSTRAP_MISSING_LIST		0x77
#define	PXENV_STATUS_BSTRAP_NO_RESPONSE			0x78
#define	PXENV_STATUS_BSTRAP_FILE_TOO_BIG		0x79

/* Environment NBP errors (0x80 to 0x8f) */

/* Reserved errors (0x90 to 0x9f) */

/* Miscellaneous errors (0xa0 to 0xaf) */
#define	PXENV_STATUS_BINL_CANCELED_BY_KEYSTROKE		0xa0
#define	PXENV_STATUS_BINL_NO_PXE_SERVER			0xa1
#define	PXENV_STATUS_NOT_AVAILABLE_IN_PMODE		0xa2
#define	PXENV_STATUS_NOT_AVAILABLE_IN_RMODE		0xa3

/* BUSD errors (0xb0 to 0xbf) */
#define	PXENV_STATUS_BUSD_DEVICE_NOT_SUPPORTED		0xb0

/* Loader errors (0xc0 to 0xcf) */
#define	PXENV_STATUS_LOADER_NO_FREE_BASE_MEMORY		0xc0
#define	PXENV_STATUS_LOADER_NO_BC_ROMID			0xc1
#define	PXENV_STATUS_LOADER_BAD_BC_ROMID		0xc2
#define	PXENV_STATUS_LOADER_BAD_BC_RUNTIME_IMAGE	0xc3
#define	PXENV_STATUS_LOADER_NO_UNDI_ROMID		0xc4
#define	PXENV_STATUS_LOADER_BAD_UNDI_ROMID		0xc5
#define	PXENV_STATUS_LOADER_BAD_UNDI_DRIVER_IMAGE	0xc6
#define	PXENV_STATUS_LOADER_NO_PXE_STRUCT		0xc8
#define	PXENV_STATUS_LOADER_NO_PXENV_STRUCT		0xc9
#define	PXENV_STATUS_LOADER_UNDI_START			0xca
#define	PXENV_STATUS_LOADER_BC_START			0xcb

/*
 * The range 0xd0 to 0xff is defined as "Vendor errors" by the PXE
 * spec.  We use this space for POSIX-like errors that aren't
 * accounted for by the (somewhat implementation-specific) PXE error
 * list.
 */

#define ENOERR		0x00	/**< Operation completed successfully */
#define EACCES		0xd0	/**< Permission denied */
#define EADDRNOTAVAIL	0xd1	/**< Cannot assign requested address */
#define EADDRINUSE	EADDRNOTAVAIL /**< Address already in use */
#define EAFNOSUPPORT	0xd2	/**< Address family not supported by protocol*/
#define EAGAIN		0xd3	/**< Resource temporarily unavailable */
#define EBUSY		0xd4	/**< Device or resource busy */
/** Operation cancelled */
#define ECANCELED	PXENV_STATUS_BINL_CANCELED_BY_KEYSTROKE
#define ECONNABORTED	0xd5	/**< Software caused connection abort */
#define ECONNREFUSED	0xd6	/**< Connection refused */
#define ECONNRESET	0xd7	/**< Connection reset by peer */
#define EDESTADDRREQ	0xd8	/**< Destination address required */
#define EFBIG		0xd9	/**< File too large */
#define EHOSTUNREACH	0xda	/**< No route to host */
#define EINPROGRESS	0xdb	/**< Operation now in progress */
#define EINTR		0xdc	/**< Interrupted system call */
#define EINVAL		0xdd	/**< Invalid argument */
#define EIO		0xde	/**< Input/output error */
#define EISCONN		0xdf	/**< Transport endpoint is already connected */
#define EMFILE		0xe0	/**< Too many open files */
#define EMSGSIZE	0xe1	/**< Message too long */
#define ENAMETOOLONG	0xe2	/**< File name too long */
#define ENETDOWN	0xe3	/**< Network is down */
#define ENETRESET	0xe4	/**< Network dropped connection on reset */
#define ENETUNREACH	0xe5	/**< Network is unreachable */
#define ENFILE		EMFILE	/**< Too many open files in system */
/** Cannot allocate memory */
#define ENOMEM		PXENV_STATUS_OUT_OF_RESOURCES
#define ENOBUFS		ENOMEM	/**< No buffer space available */
#define ENODATA		0xe6	/**< No data available */
#define ENODEV		0xe7	/**< No such device */
#define ENOENT		0xe8	/**< No such file or directory */
#define ENOEXEC		0xe9	/**< Exec format error */
#define ENOMSG		ENODATA	/**< No message of the desired type */
#define ENOSPC		0xea	/**< No space left on device */
#define ENOSR		0xeb	/**< No stream resources */
#define ENOSTR		0xec	/**< Not a stream */
#define ENOSYS		0xed	/**< Function not implemented */
#define ENOTCONN	0xee	/**< Transport endpoint is not connected */
#define ENOTSOCK	0xef	/**< Socket operation on non-socket */
#define EOPNOTSUPP	0xf0	/**< Operation not supported */
#define ENOTSUP		EOPNOTSUPP /**< Not supported */
#define ENOTTY		0xf1	/**< Inappropriate ioctl for device */
#define ENXIO		ENODEV	/**< No such device or address */
#define EOVERFLOW	0xf2	/**< Result too large */
#define EPERM		EACCES	/**< Operation not permitted */
#define EPROTO		0xf3	/**< Protocol error */
#define EPROTONOSUPPORT	0xf4	/**< Protocol not supported */
#define EPROTOTYPE	0xf5	/**< Protocol wrong type for socket */
#define ERANGE		EOVERFLOW /**< Result too large */
#define ETIMEDOUT	0xf6	/**< Connection timed out */
#define EWOULDBLOCK	EAGAIN	/**< Resource temporarily unavailable */

/* Data structures and declarations */

#include <gpxe/tables.h>

extern int errno;

extern const char * strerror ( int errno );

struct errortab {
	int errno;
	const char *text;
};

#define __errortab __table(errortab,01)

#endif /* ERRNO_H */
