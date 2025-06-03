#ifndef _BITS_ERRFILE_H
#define _BITS_ERRFILE_H

/** @file
 *
 * RISC-V error file identifiers
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**
 * @addtogroup errfile Error file identifiers
 * @{
 */

#define ERRFILE_sbi_reboot	( ERRFILE_ARCH | ERRFILE_CORE | 0x00000000 )
#define ERRFILE_hart		( ERRFILE_ARCH | ERRFILE_CORE | 0x00010000 )
#define ERRFILE_zicntr		( ERRFILE_ARCH | ERRFILE_CORE | 0x00020000 )
#define ERRFILE_zkr		( ERRFILE_ARCH | ERRFILE_CORE | 0x00030000 )

/** @} */

#endif /* _BITS_ERRFILE_H */
