/*******************************************************************************

  Intel 82599 Virtual Function driver
  Copyright(c) 1999 - 2012 Intel Corporation.

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
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/


/* glue for the OS independent part of ixgbe
 * includes register access macros
 */

#ifndef _IXGBEVF_OSDEP_H_
#define _IXGBEVF_OSDEP_H_


#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <byteswap.h>
#include <errno.h>

#include <ipxe/pci.h>
#include <ipxe/malloc.h>
#include <ipxe/if_ether.h>
#include <ipxe/io.h>
#include <ipxe/timer.h>
#include <ipxe/ethernet.h>
#include <ipxe/iobuf.h>
#include <ipxe/netdevice.h>

typedef enum {
    false = 0,
    true = 1
} boolean_t;
#define bool       boolean_t
#define TRUE  1
#define FALSE 0

#ifndef VLAN_ETH_FRAME_LEN
#define VLAN_ETH_FRAME_LEN 1518
#endif
#define ETH_FCS_LEN 4

#define dma_addr_t	unsigned long

#define __iomem
#define __devinit


#define msleep(x)	mdelay(x)

#undef ASSERT

#define hw_dbg(hw, S, A...)      do {} while (0)

#define IXGBE_WRITE_REG(a, reg, value) writel((value), ((a)->hw_addr + (reg)))

#define IXGBE_READ_REG(a, reg) readl((a)->hw_addr + (reg))

#define IXGBE_WRITE_REG_ARRAY(a, reg, offset, value) do{ \
    writel((value), ((a)->hw_addr + (reg) + ((offset) << 2))); } while(0)

#define IXGBE_READ_REG_ARRAY(a, reg, offset) ( \
    readl((a)->hw_addr + (reg) + ((offset) << 2)))

#ifndef writeq
#define writeq(val, addr) writel((u32) (val), addr); \
	writel((u32) (val >> 32), (addr + 4));
#endif

#define IXGBE_WRITE_REG64(a, reg, value) writeq((value), ((a)->hw_addr + (reg)))

#define IXGBE_WRITE_FLUSH(a) IXGBE_READ_REG(a, IXGBE_VFSTATUS)
struct ixgbe_hw;
extern u16 ixgbe_read_pci_cfg_word(struct ixgbe_hw *hw, u32 reg);
extern void ixgbe_write_pci_cfg_word(struct ixgbe_hw *hw, u32 reg, u16 value);
#define IXGBE_READ_PCIE_WORD ixgbe_read_pci_cfg_word
#define IXGBE_WRITE_PCIE_WORD ixgbe_write_pci_cfg_word
#define IXGBE_EEPROM_GRANT_ATTEMPS 100
#define IXGBE_HTONL(_i) htonl(_i)
#define IXGBE_HTONS(_i) htons(_i)

#endif /* _IXGBE_OSDEP_H_ */
