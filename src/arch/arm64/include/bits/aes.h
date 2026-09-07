#ifndef _BITS_AES_H
#define _BITS_AES_H

/** @file
 *
 * AES Cryptographic Extensions
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

/* AArch64 Instruction Set Attribute Register 0 */
#define AA64ISAR0_AES_MASK	0x00000000000000f0 /**< AES bits */
#define AA64ISAR0_AES_AES	0x0000000000000010 /**< AES instructions */

extern void aes_accelerate ( void );

#endif /* _BITS_AES_H */
