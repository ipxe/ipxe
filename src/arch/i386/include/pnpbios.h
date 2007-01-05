#ifndef _PNPBIOS_H
#define _PNPBIOS_H

/** @file
 *
 * PnP BIOS
 *
 */

/* BIOS segment address */
#define BIOS_SEG 0xf000

extern int find_pnp_bios ( void );

#endif /* _PNPBIOS_H */
