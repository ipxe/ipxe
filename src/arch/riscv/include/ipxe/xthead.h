#ifndef _IPXE_XTHEAD_H
#define _IPXE_XTHEAD_H

/** @file
 *
 * T-Head vendor extensions
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** T-Head machine vendor ID */
#define THEAD_MVENDORID 0x5b7

/** T-Head SXSTATUS CSR */
#define THEAD_SXSTATUS 0x5c0
#define THEAD_SXSTATUS_THEADISAEE 0x00400000	/**< General ISA extensions */

extern int xthead_supported ( unsigned long feature );

#endif /* _IPXE_XTHEAD_H */
