/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

#if 0
/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define if you have the iconv() function and it works. */
/* #undef HAVE_ICONV */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if using pthread is enabled. */
#define HAVE_LIBPTHREAD 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if using libpng is enabled. */
#define HAVE_PNG 1

/* Define to 1 if using SDL is enabled. */
/* #undef HAVE_SDL */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1
#endif /* 0 */

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

#if 0
/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

#endif /* 0 */

/* Major version number */
#define MAJOR_VERSION 4

/* Micro version number */
#define MICRO_VERSION 0

/* Minor version number */
#define MINOR_VERSION 1

#if 0

/* Name of package */
#define PACKAGE "qrencode"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "QRencode"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "QRencode 4.1.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "qrencode"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "4.1.0"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

#endif /* 0 */
/* Version number of package */
#define VERSION "4.1.0"

#if 0

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif
#endif /* 0 */

/* Define to 'static' if no test programs will be compiled. */
#define STATIC_IN_RELEASE static
/* #undef WITH_TESTS */


/* Do not include ERRFILE portion in the numbers in the error table */
#include "errno.h"
#include "string.h"
#include "stdio.h"
#include "ipxe/errortab.h"
#include "config/branding.h"

#undef ERRFILE
#define ERRFILE 0


#define SHOEHORN_THIS_AS_A_WASTEFUL_PRIVATE static

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
/** The most common errors */
SHOEHORN_THIS_AS_A_WASTEFUL_PRIVATE struct errortab common_errors[] __errortab = {
        __einfo_errortab ( EINFO_ENOERR ),
        __einfo_errortab ( EINFO_EACCES ),
        __einfo_errortab ( EINFO_ECANCELED ),
        __einfo_errortab ( EINFO_ECONNRESET ),
        __einfo_errortab ( EINFO_EINVAL ),
        __einfo_errortab ( EINFO_EIO ),
        __einfo_errortab ( EINFO_ENETUNREACH ),
        __einfo_errortab ( EINFO_ENODEV ),
        __einfo_errortab ( EINFO_ENOENT ),
        __einfo_errortab ( EINFO_ENOEXEC ),
        __einfo_errortab ( EINFO_ENOMEM ),
        __einfo_errortab ( EINFO_ENOSPC ),
        __einfo_errortab ( EINFO_ENOTCONN ),
        __einfo_errortab ( EINFO_ENOTSUP ),
        __einfo_errortab ( EINFO_EPERM ),
        __einfo_errortab ( EINFO_ERANGE ),
        __einfo_errortab ( EINFO_ETIMEDOUT ),
};
#pragma GCC diagnostic pop
