#pragma once

#ifndef _IPXE_ERRNO_EFI_H
    #define _IPXE_ERRNO_EFI_H

/**
 * @file
 *
 * EFI platform error codes
 *
 * We derive our platform error codes from the possible values for
 * EFI_STATUS defined in the UEFI specification.
 *
 * EFI_STATUS codes are 32/64-bit values consisting of a top bit which
 * is set for errors and clear for warnings, and a mildly undefined
 * code of low bits indicating the precise error/warning code.  Errors
 * and warnings have completely separate namespaces.
 *
 * We assume that no EFI_STATUS code will ever be defined which uses
 * more than bits 0-6 of the low bits.  We then choose to encode our
 * platform-specific error by mapping bit 31/63 of the EFI_STATUS to
 * bit 7 of the platform-specific error code, and preserving bits 0-6
 * as-is.
 */

FILE_LICENCE(GPL2_OR_LATER_OR_UBDL);

    #include <ipxe/efi/efi.h>
    #include <ipxe/efi/Uefi/UefiBaseType.h>

    /** Bit shift for EFI error/warning bit */
    #define EFI_ERR_SHIFT (8 * (sizeof(EFI_STATUS) - 1))

    /**
     * Convert platform error code to platform component of iPXE error code
     *
     * @v platform		Platform error code
     * @ret errno		Platform component of iPXE error code
     */
    #define PLATFORM_TO_ERRNO(platform)                  \
        (((platform) |                                   \
          (((EFI_STATUS)(platform)) >> EFI_ERR_SHIFT)) & \
         0xff)

    /**
     * Convert iPXE error code to platform error code
     *
     * @v errno		iPXE error code
     * @ret platform	Platform error code
     */
    #define ERRNO_TO_PLATFORM(errno)                     \
        ((((EFI_STATUS)(errno)&0x80) << EFI_ERR_SHIFT) | \
         ((errno)&0x7f))

    /* Platform-specific error codes */
    #define PLATFORM_ENOERR EFI_SUCCESS
    #define PLATFORM_E2BIG EFI_BUFFER_TOO_SMALL
    #define PLATFORM_EACCES EFI_ACCESS_DENIED
    #define PLATFORM_EADDRINUSE EFI_ALREADY_STARTED
    #define PLATFORM_EADDRNOTAVAIL EFI_NOT_READY
    #define PLATFORM_EAFNOSUPPORT EFI_UNSUPPORTED
    #define PLATFORM_EAGAIN EFI_NOT_READY
    #define PLATFORM_EALREADY EFI_ALREADY_STARTED
    #define PLATFORM_EBADF EFI_INVALID_PARAMETER
    #define PLATFORM_EBADMSG EFI_PROTOCOL_ERROR
    #define PLATFORM_EBUSY EFI_NO_RESPONSE
    #define PLATFORM_ECANCELED EFI_ABORTED
    #define PLATFORM_ECHILD EFI_NOT_FOUND
    #define PLATFORM_ECONNABORTED EFI_ABORTED
    #define PLATFORM_ECONNREFUSED EFI_NO_RESPONSE
    #define PLATFORM_ECONNRESET EFI_ABORTED
    #define PLATFORM_EDEADLK EFI_NOT_READY
    #define PLATFORM_EDESTADDRREQ EFI_PROTOCOL_ERROR
    #define PLATFORM_EDOM EFI_INVALID_PARAMETER
    #define PLATFORM_EDQUOT EFI_VOLUME_FULL
    #define PLATFORM_EEXIST EFI_WRITE_PROTECTED
    #define PLATFORM_EFAULT EFI_INVALID_PARAMETER
    #define PLATFORM_EFBIG EFI_END_OF_MEDIA
    #define PLATFORM_EHOSTUNREACH EFI_NO_RESPONSE
    #define PLATFORM_EIDRM EFI_INVALID_PARAMETER
    #define PLATFORM_EILSEQ EFI_INVALID_PARAMETER
    #define PLATFORM_EINPROGRESS EFI_ALREADY_STARTED
    #define PLATFORM_EINTR EFI_NOT_READY
    #define PLATFORM_EINVAL EFI_INVALID_PARAMETER
    #define PLATFORM_EIO EFI_PROTOCOL_ERROR
    #define PLATFORM_EISCONN EFI_ALREADY_STARTED
    #define PLATFORM_EISDIR EFI_PROTOCOL_ERROR
    #define PLATFORM_ELOOP EFI_VOLUME_CORRUPTED
    #define PLATFORM_EMFILE EFI_OUT_OF_RESOURCES
    #define PLATFORM_EMLINK EFI_OUT_OF_RESOURCES
    #define PLATFORM_EMSGSIZE EFI_BAD_BUFFER_SIZE
    #define PLATFORM_EMULTIHOP EFI_INVALID_PARAMETER
    #define PLATFORM_ENAMETOOLONG EFI_INVALID_PARAMETER
    #define PLATFORM_ENETDOWN EFI_NO_RESPONSE
    #define PLATFORM_ENETRESET EFI_ABORTED
    #define PLATFORM_ENETUNREACH EFI_NO_RESPONSE
    #define PLATFORM_ENFILE EFI_OUT_OF_RESOURCES
    #define PLATFORM_ENOBUFS EFI_OUT_OF_RESOURCES
    #define PLATFORM_ENODATA EFI_NO_RESPONSE
    #define PLATFORM_ENODEV EFI_DEVICE_ERROR
    #define PLATFORM_ENOENT EFI_NOT_FOUND
    #define PLATFORM_ENOEXEC EFI_LOAD_ERROR
    #define PLATFORM_ENOLCK EFI_OUT_OF_RESOURCES
    #define PLATFORM_ENOLINK EFI_OUT_OF_RESOURCES
    #define PLATFORM_ENOMEM EFI_OUT_OF_RESOURCES
    #define PLATFORM_ENOMSG EFI_PROTOCOL_ERROR
    #define PLATFORM_ENOPROTOOPT EFI_UNSUPPORTED
    #define PLATFORM_ENOSPC EFI_VOLUME_FULL
    #define PLATFORM_ENOSR EFI_OUT_OF_RESOURCES
    #define PLATFORM_ENOSTR EFI_PROTOCOL_ERROR
    #define PLATFORM_ENOSYS EFI_UNSUPPORTED
    #define PLATFORM_ENOTCONN EFI_NOT_STARTED
    #define PLATFORM_ENOTDIR EFI_VOLUME_CORRUPTED
    #define PLATFORM_ENOTEMPTY EFI_VOLUME_CORRUPTED
    #define PLATFORM_ENOTSOCK EFI_INVALID_PARAMETER
    #define PLATFORM_ENOTSUP EFI_UNSUPPORTED
    #define PLATFORM_ENOTTY EFI_UNSUPPORTED
    #define PLATFORM_ENXIO EFI_NOT_FOUND
    #define PLATFORM_EOPNOTSUPP EFI_UNSUPPORTED
    #define PLATFORM_EOVERFLOW EFI_BUFFER_TOO_SMALL
    #define PLATFORM_EPERM EFI_ACCESS_DENIED
    #define PLATFORM_EPIPE EFI_ABORTED
    #define PLATFORM_EPROTO EFI_PROTOCOL_ERROR
    #define PLATFORM_EPROTONOSUPPORT EFI_UNSUPPORTED
    #define PLATFORM_EPROTOTYPE EFI_INVALID_PARAMETER
    #define PLATFORM_ERANGE EFI_BUFFER_TOO_SMALL
    #define PLATFORM_EROFS EFI_WRITE_PROTECTED
    #define PLATFORM_ESPIPE EFI_END_OF_FILE
    #define PLATFORM_ESRCH EFI_NOT_STARTED
    #define PLATFORM_ESTALE EFI_PROTOCOL_ERROR
    #define PLATFORM_ETIME EFI_TIMEOUT
    #define PLATFORM_ETIMEDOUT EFI_TIMEOUT
    #define PLATFORM_ETXTBSY EFI_MEDIA_CHANGED
    #define PLATFORM_EWOULDBLOCK EFI_NOT_READY
    #define PLATFORM_EXDEV EFI_VOLUME_CORRUPTED

#endif /* _IPXE_ERRNO_EFI_H */
