/*--------------------------------------------------------------------------*/
/* Project:        ANSI C Standard Header Files                             */
/* File:           LIMITS.H                                                 */
/* Edited by:      hyperstone electronics GmbH                              */
/*                 Am Seerhein 8                                            */
/*                 D-78467 Konstanz, Germany                                */
/* Date:           January 30, 1996                                         */
/*--------------------------------------------------------------------------*/
/* Purpose:                                                                 */
/* The header file <limits.h> defines limits of ordinal types               */
/* (char, short, int, long)                                                 */
/*--------------------------------------------------------------------------*/

#ifndef __LIMITS_H
#define __LIMITS_H  1

#define MB_LEN_MAX        1
#define CHAR_BIT          8
#define SCHAR_MIN         -128L
#define SCHAR_MAX         127L
#define UCHAR_MAX         255
#define CHAR_MIN          0
#define CHAR_MAX          UCHAR_MAX
#define SHRT_MIN          -32768
#define SHRT_MAX          32767
#define USHRT_MAX         65535
#define INT_MIN           0x80000000
#define INT_MAX           0x7FFFFFFF
#define UINT_MAX          0xFFFFFFFFL
#define LONG_MIN          INT_MIN
#define LONG_MAX          INT_MAX
#define ULONG_MAX         UINT_MAX

#endif
