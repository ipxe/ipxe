/*******************************************************************************

  Intel PRO/1000 Linux driver
  Copyright(c) 1999 - 2006 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

FILE_LICENCE ( GPL2_ONLY );

/* glue for the OS independent part of e1000
 * includes register access macros
 */

#ifndef _E1000_OSDEP_H_
#define _E1000_OSDEP_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <gpxe/io.h>
#include <errno.h>
#include <unistd.h>
#include <byteswap.h>
#include <gpxe/pci.h>
#include <gpxe/if_ether.h>
#include <gpxe/ethernet.h>
#include <gpxe/iobuf.h>
#include <gpxe/netdevice.h>

typedef enum {
#undef FALSE
    FALSE = 0,
#undef TRUE
    TRUE = 1
} boolean_t;

/* Debugging #defines */

#if 1
#define DEBUGFUNC(F) DBG(F "\n")
#else
#define DEBUGFUNC(F)
#endif

#if 1
#define DEBUGOUT(S)             DBG(S)
#define DEBUGOUT1(S, A...)      DBG(S, A)
#else
#define DEBUGOUT(S)
#define DEBUGOUT1(S, A...)
#endif

#define DEBUGOUT2 DEBUGOUT1
#define DEBUGOUT3 DEBUGOUT1
#define DEBUGOUT7 DEBUGOUT1

#define E1000_WRITE_REG(a, reg, value) \
    writel((value), ((a)->hw_addr + \
        (((a)->mac_type >= e1000_82543) ? E1000_##reg : E1000_82542_##reg)))

#define E1000_READ_REG(a, reg) \
    readl((a)->hw_addr + \
        (((a)->mac_type >= e1000_82543) ? E1000_##reg : E1000_82542_##reg))

#define E1000_WRITE_REG_ARRAY(a, reg, offset, value) \
    writel((value), ((a)->hw_addr + \
        (((a)->mac_type >= e1000_82543) ? E1000_##reg : E1000_82542_##reg) + \
        ((offset) << 2)))

#define E1000_READ_REG_ARRAY(a, reg, offset) \
    readl((a)->hw_addr + \
        (((a)->mac_type >= e1000_82543) ? E1000_##reg : E1000_82542_##reg) + \
        ((offset) << 2))

#define E1000_READ_REG_ARRAY_DWORD E1000_READ_REG_ARRAY
#define E1000_WRITE_REG_ARRAY_DWORD E1000_WRITE_REG_ARRAY

#define E1000_WRITE_REG_ARRAY_WORD(a, reg, offset, value) \
    writew((value), ((a)->hw_addr + \
        (((a)->mac_type >= e1000_82543) ? E1000_##reg : E1000_82542_##reg) + \
        ((offset) << 1)))

#define E1000_READ_REG_ARRAY_WORD(a, reg, offset) \
    readw((a)->hw_addr + \
        (((a)->mac_type >= e1000_82543) ? E1000_##reg : E1000_82542_##reg) + \
        ((offset) << 1))

#define E1000_WRITE_REG_ARRAY_BYTE(a, reg, offset, value) \
    writeb((value), ((a)->hw_addr + \
        (((a)->mac_type >= e1000_82543) ? E1000_##reg : E1000_82542_##reg) + \
        (offset)))

#define E1000_READ_REG_ARRAY_BYTE(a, reg, offset) \
    readb((a)->hw_addr + \
        (((a)->mac_type >= e1000_82543) ? E1000_##reg : E1000_82542_##reg) + \
        (offset))

#define E1000_WRITE_FLUSH(a) E1000_READ_REG(a, STATUS)

#define E1000_WRITE_ICH_FLASH_REG(a, reg, value) \
    writel((value), ((a)->flash_address + reg))

#define E1000_READ_ICH_FLASH_REG(a, reg) \
    readl((a)->flash_address + reg)

#define E1000_WRITE_ICH_FLASH_REG16(a, reg, value) \
    writew((value), ((a)->flash_address + reg))

#define E1000_READ_ICH_FLASH_REG16(a, reg) \
    readw((a)->flash_address + reg)

#define	msleep(n) 	mdelay(n)

#endif /* _E1000_OSDEP_H_ */

/*
 * Local variables:
 *  c-basic-offset: 8
 *  c-indent-level: 8
 *  tab-width: 8
 * End:
 */
