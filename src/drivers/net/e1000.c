/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
Inter Pro 1000 for Etherboot
Drivers are port from Intel's Linux driver e1000-4.3.15

***************************************************************************/
/*******************************************************************************

  
  Copyright(c) 1999 - 2003 Intel Corporation. All rights reserved.
  
  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
  
  Contact Information:
  Linux NICS <linux.nics@intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/
/*
 *  Copyright (C) Archway Digital Solutions.
 *
 *  written by Chrsitopher Li <cli at arcyway dot com> or <chrisl at gnuchina dot org>
 *  2/9/2002
 *
 *  Copyright (C) Linux Networx.
 *  Massive upgrade to work with the new intel gigabit NICs.
 *  <ebiederman at lnxi dot com>
 *
 *  Support for 82541ei & 82547ei chips from Intel's Linux driver 5.1.13 added by
 *  Georg Baum <gbaum@users.sf.net>, sponsored by PetaMem GmbH and linkLINE Communications, Inc.
 *
 *  01/2004: Updated to Linux driver 5.2.22 by Georg Baum <gbaum@users.sf.net>
 */

/* to get some global routines like printf */
#include "etherboot.h"
/* to get the interface to the body of the program */
#include "nic.h"
/* to get the PCI support functions, if this is a PCI NIC */
#include "pci.h"
#include "timer.h"

typedef unsigned char *dma_addr_t;

typedef enum {
	FALSE = 0,
	TRUE = 1
} boolean_t;

#define DEBUG 0


/* Some pieces of code are disabled with #if 0 ... #endif.
 * They are not deleted to show where the etherboot driver differs
 * from the linux driver below the function level.
 * Some member variables of the hw struct have been eliminated
 * and the corresponding inplace checks inserted instead.
 * Pieces such as LED handling that we definitely don't need are deleted.
 *
 * The following defines should not be needed normally,
 * but may be helpful for debugging purposes. */

/* Define this if you want to program the transmission control register
 * the way the Linux driver does it. */
#undef LINUX_DRIVER_TCTL

/* Define this to behave more like the Linux driver. */
#undef LINUX_DRIVER

#include "e1000_hw.h"

/* NIC specific static variables go here */
static struct e1000_hw hw;
static char tx_pool[128 + 16];
static char rx_pool[128 + 16];
static char packet[2096];

static struct e1000_tx_desc *tx_base;
static struct e1000_rx_desc *rx_base;

static int tx_tail;
static int rx_tail, rx_last;

/* Function forward declarations */
static int e1000_setup_link(struct e1000_hw *hw);
static int e1000_setup_fiber_serdes_link(struct e1000_hw *hw);
static int e1000_setup_copper_link(struct e1000_hw *hw);
static int e1000_phy_setup_autoneg(struct e1000_hw *hw);
static void e1000_config_collision_dist(struct e1000_hw *hw);
static int e1000_config_mac_to_phy(struct e1000_hw *hw);
static int e1000_config_fc_after_link_up(struct e1000_hw *hw);
static int e1000_check_for_link(struct e1000_hw *hw);
static int e1000_wait_autoneg(struct e1000_hw *hw);
static void e1000_get_speed_and_duplex(struct e1000_hw *hw, uint16_t *speed, uint16_t *duplex);
static int e1000_read_phy_reg(struct e1000_hw *hw, uint32_t reg_addr, uint16_t *phy_data);
static int e1000_read_phy_reg_ex(struct e1000_hw *hw, uint32_t reg_addr, uint16_t *phy_data);
static int e1000_write_phy_reg(struct e1000_hw *hw, uint32_t reg_addr, uint16_t phy_data);
static int e1000_write_phy_reg_ex(struct e1000_hw *hw, uint32_t reg_addr, uint16_t phy_data);
static void e1000_phy_hw_reset(struct e1000_hw *hw);
static int e1000_phy_reset(struct e1000_hw *hw);
static int e1000_detect_gig_phy(struct e1000_hw *hw);
static void e1000_irq(struct nic *nic, irq_action_t action);

/* Printing macros... */

#define E1000_ERR(args...) printf("e1000: " args)

#if DEBUG >= 3
#define E1000_DBG(args...) printf("e1000: " args)
#else
#define E1000_DBG(args...)
#endif

#define MSGOUT(S, A, B)     printk(S "\n", A, B)
#if DEBUG >= 2
#define DEBUGFUNC(F)        DEBUGOUT(F "\n");
#else
#define DEBUGFUNC(F)
#endif
#if DEBUG >= 1
#define DEBUGOUT(S) printf(S)
#define DEBUGOUT1(S,A) printf(S,A)
#define DEBUGOUT2(S,A,B) printf(S,A,B)
#define DEBUGOUT3(S,A,B,C) printf(S,A,B,C)
#define DEBUGOUT7(S,A,B,C,D,E,F,G) printf(S,A,B,C,D,E,F,G)
#else
#define DEBUGOUT(S)
#define DEBUGOUT1(S,A)
#define DEBUGOUT2(S,A,B)
#define DEBUGOUT3(S,A,B,C)
#define DEBUGOUT7(S,A,B,C,D,E,F,G)
#endif

#define E1000_WRITE_REG(a, reg, value) ( \
    ((a)->mac_type >= e1000_82543) ? \
        (writel((value), ((a)->hw_addr + E1000_##reg))) : \
        (writel((value), ((a)->hw_addr + E1000_82542_##reg))))

#define E1000_READ_REG(a, reg) ( \
    ((a)->mac_type >= e1000_82543) ? \
        readl((a)->hw_addr + E1000_##reg) : \
        readl((a)->hw_addr + E1000_82542_##reg))

#define E1000_WRITE_REG_ARRAY(a, reg, offset, value) ( \
    ((a)->mac_type >= e1000_82543) ? \
        writel((value), ((a)->hw_addr + E1000_##reg + ((offset) << 2))) : \
        writel((value), ((a)->hw_addr + E1000_82542_##reg + ((offset) << 2))))

#define E1000_READ_REG_ARRAY(a, reg, offset) ( \
    ((a)->mac_type >= e1000_82543) ? \
        readl((a)->hw_addr + E1000_##reg + ((offset) << 2)) : \
        readl((a)->hw_addr + E1000_82542_##reg + ((offset) << 2)))

#define E1000_WRITE_FLUSH(a) {uint32_t x; x = E1000_READ_REG(a, STATUS);}

uint32_t
e1000_io_read(struct e1000_hw *hw __unused, uint32_t port)
{
        return inl(port);
}

void
e1000_io_write(struct e1000_hw *hw __unused, uint32_t port, uint32_t value)
{
        outl(value, port);
}

static inline void e1000_pci_set_mwi(struct e1000_hw *hw)
{
	pci_write_config_word(hw->pdev, PCI_COMMAND, hw->pci_cmd_word);
}

static inline void e1000_pci_clear_mwi(struct e1000_hw *hw)
{
	pci_write_config_word(hw->pdev, PCI_COMMAND,
			      hw->pci_cmd_word & ~PCI_COMMAND_INVALIDATE);
}

/******************************************************************************
 * Raises the EEPROM's clock input.
 *
 * hw - Struct containing variables accessed by shared code
 * eecd - EECD's current value
 *****************************************************************************/
static void
e1000_raise_ee_clk(struct e1000_hw *hw,
                   uint32_t *eecd)
{
	/* Raise the clock input to the EEPROM (by setting the SK bit), and then
	 * wait <delay> microseconds.
	 */
	*eecd = *eecd | E1000_EECD_SK;
	E1000_WRITE_REG(hw, EECD, *eecd);
	E1000_WRITE_FLUSH(hw);
	udelay(hw->eeprom.delay_usec);
}

/******************************************************************************
 * Lowers the EEPROM's clock input.
 *
 * hw - Struct containing variables accessed by shared code 
 * eecd - EECD's current value
 *****************************************************************************/
static void
e1000_lower_ee_clk(struct e1000_hw *hw,
                   uint32_t *eecd)
{
	/* Lower the clock input to the EEPROM (by clearing the SK bit), and then 
	 * wait 50 microseconds. 
	 */
	*eecd = *eecd & ~E1000_EECD_SK;
	E1000_WRITE_REG(hw, EECD, *eecd);
	E1000_WRITE_FLUSH(hw);
	udelay(hw->eeprom.delay_usec);
}

/******************************************************************************
 * Shift data bits out to the EEPROM.
 *
 * hw - Struct containing variables accessed by shared code
 * data - data to send to the EEPROM
 * count - number of bits to shift out
 *****************************************************************************/
static void
e1000_shift_out_ee_bits(struct e1000_hw *hw,
                        uint16_t data,
                        uint16_t count)
{
	struct e1000_eeprom_info *eeprom = &hw->eeprom;
	uint32_t eecd;
	uint32_t mask;
	
	/* We need to shift "count" bits out to the EEPROM. So, value in the
	 * "data" parameter will be shifted out to the EEPROM one bit at a time.
	 * In order to do this, "data" must be broken down into bits. 
	 */
	mask = 0x01 << (count - 1);
	eecd = E1000_READ_REG(hw, EECD);
	if (eeprom->type == e1000_eeprom_microwire) {
		eecd &= ~E1000_EECD_DO;
	} else if (eeprom->type == e1000_eeprom_spi) {
		eecd |= E1000_EECD_DO;
	}
	do {
		/* A "1" is shifted out to the EEPROM by setting bit "DI" to a "1",
		 * and then raising and then lowering the clock (the SK bit controls
		 * the clock input to the EEPROM).  A "0" is shifted out to the EEPROM
		 * by setting "DI" to "0" and then raising and then lowering the clock.
		 */
		eecd &= ~E1000_EECD_DI;
		
		if(data & mask)
			eecd |= E1000_EECD_DI;
		
		E1000_WRITE_REG(hw, EECD, eecd);
		E1000_WRITE_FLUSH(hw);
		
		udelay(eeprom->delay_usec);
		
		e1000_raise_ee_clk(hw, &eecd);
		e1000_lower_ee_clk(hw, &eecd);
		
		mask = mask >> 1;
		
	} while(mask);

	/* We leave the "DI" bit set to "0" when we leave this routine. */
	eecd &= ~E1000_EECD_DI;
	E1000_WRITE_REG(hw, EECD, eecd);
}

/******************************************************************************
 * Shift data bits in from the EEPROM
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
static uint16_t
e1000_shift_in_ee_bits(struct e1000_hw *hw,
                       uint16_t count)
{
	uint32_t eecd;
	uint32_t i;
	uint16_t data;
	
	/* In order to read a register from the EEPROM, we need to shift 'count' 
	 * bits in from the EEPROM. Bits are "shifted in" by raising the clock
	 * input to the EEPROM (setting the SK bit), and then reading the value of
	 * the "DO" bit.  During this "shifting in" process the "DI" bit should
	 * always be clear.
	 */
	
	eecd = E1000_READ_REG(hw, EECD);
	
	eecd &= ~(E1000_EECD_DO | E1000_EECD_DI);
	data = 0;
	
	for(i = 0; i < count; i++) {
		data = data << 1;
		e1000_raise_ee_clk(hw, &eecd);
		
		eecd = E1000_READ_REG(hw, EECD);
		
		eecd &= ~(E1000_EECD_DI);
		if(eecd & E1000_EECD_DO)
			data |= 1;
		
		e1000_lower_ee_clk(hw, &eecd);
	}
	
	return data;
}

/******************************************************************************
 * Prepares EEPROM for access
 *
 * hw - Struct containing variables accessed by shared code
 *
 * Lowers EEPROM clock. Clears input pin. Sets the chip select pin. This 
 * function should be called before issuing a command to the EEPROM.
 *****************************************************************************/
static int32_t
e1000_acquire_eeprom(struct e1000_hw *hw)
{
	struct e1000_eeprom_info *eeprom = &hw->eeprom;
	uint32_t eecd, i=0;

	eecd = E1000_READ_REG(hw, EECD);

	/* Request EEPROM Access */
	if(hw->mac_type > e1000_82544) {
		eecd |= E1000_EECD_REQ;
		E1000_WRITE_REG(hw, EECD, eecd);
		eecd = E1000_READ_REG(hw, EECD);
		while((!(eecd & E1000_EECD_GNT)) &&
		      (i < E1000_EEPROM_GRANT_ATTEMPTS)) {
			i++;
			udelay(5);
			eecd = E1000_READ_REG(hw, EECD);
		}
		if(!(eecd & E1000_EECD_GNT)) {
			eecd &= ~E1000_EECD_REQ;
			E1000_WRITE_REG(hw, EECD, eecd);
			DEBUGOUT("Could not acquire EEPROM grant\n");
			return -E1000_ERR_EEPROM;
		}
	}

	/* Setup EEPROM for Read/Write */

	if (eeprom->type == e1000_eeprom_microwire) {
		/* Clear SK and DI */
		eecd &= ~(E1000_EECD_DI | E1000_EECD_SK);
		E1000_WRITE_REG(hw, EECD, eecd);

		/* Set CS */
		eecd |= E1000_EECD_CS;
		E1000_WRITE_REG(hw, EECD, eecd);
	} else if (eeprom->type == e1000_eeprom_spi) {
		/* Clear SK and CS */
		eecd &= ~(E1000_EECD_CS | E1000_EECD_SK);
		E1000_WRITE_REG(hw, EECD, eecd);
		udelay(1);
	}

	return E1000_SUCCESS;
}

/******************************************************************************
 * Returns EEPROM to a "standby" state
 * 
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
static void
e1000_standby_eeprom(struct e1000_hw *hw)
{
	struct e1000_eeprom_info *eeprom = &hw->eeprom;
	uint32_t eecd;
	
	eecd = E1000_READ_REG(hw, EECD);

	if(eeprom->type == e1000_eeprom_microwire) {

		/* Deselect EEPROM */
		eecd &= ~(E1000_EECD_CS | E1000_EECD_SK);
		E1000_WRITE_REG(hw, EECD, eecd);
		E1000_WRITE_FLUSH(hw);
		udelay(eeprom->delay_usec);
	
		/* Clock high */
		eecd |= E1000_EECD_SK;
		E1000_WRITE_REG(hw, EECD, eecd);
		E1000_WRITE_FLUSH(hw);
		udelay(eeprom->delay_usec);
	
		/* Select EEPROM */
		eecd |= E1000_EECD_CS;
		E1000_WRITE_REG(hw, EECD, eecd);
		E1000_WRITE_FLUSH(hw);
		udelay(eeprom->delay_usec);

		/* Clock low */
		eecd &= ~E1000_EECD_SK;
		E1000_WRITE_REG(hw, EECD, eecd);
		E1000_WRITE_FLUSH(hw);
		udelay(eeprom->delay_usec);
	} else if(eeprom->type == e1000_eeprom_spi) {
		/* Toggle CS to flush commands */
		eecd |= E1000_EECD_CS;
		E1000_WRITE_REG(hw, EECD, eecd);
		E1000_WRITE_FLUSH(hw);
		udelay(eeprom->delay_usec);
		eecd &= ~E1000_EECD_CS;
		E1000_WRITE_REG(hw, EECD, eecd);
		E1000_WRITE_FLUSH(hw);
		udelay(eeprom->delay_usec);
	}
}

/******************************************************************************
 * Terminates a command by inverting the EEPROM's chip select pin
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
static void
e1000_release_eeprom(struct e1000_hw *hw)
{
	uint32_t eecd;

	eecd = E1000_READ_REG(hw, EECD);

	if (hw->eeprom.type == e1000_eeprom_spi) {
		eecd |= E1000_EECD_CS;  /* Pull CS high */
		eecd &= ~E1000_EECD_SK; /* Lower SCK */

		E1000_WRITE_REG(hw, EECD, eecd);

		udelay(hw->eeprom.delay_usec);
	} else if(hw->eeprom.type == e1000_eeprom_microwire) {
		/* cleanup eeprom */

		/* CS on Microwire is active-high */
		eecd &= ~(E1000_EECD_CS | E1000_EECD_DI);

		E1000_WRITE_REG(hw, EECD, eecd);

		/* Rising edge of clock */
		eecd |= E1000_EECD_SK;
		E1000_WRITE_REG(hw, EECD, eecd);
		E1000_WRITE_FLUSH(hw);
		udelay(hw->eeprom.delay_usec);

		/* Falling edge of clock */
		eecd &= ~E1000_EECD_SK;
		E1000_WRITE_REG(hw, EECD, eecd);
		E1000_WRITE_FLUSH(hw);
		udelay(hw->eeprom.delay_usec);
	}

	/* Stop requesting EEPROM access */
	if(hw->mac_type > e1000_82544) {
		eecd &= ~E1000_EECD_REQ;
		E1000_WRITE_REG(hw, EECD, eecd);
	}
}

/******************************************************************************
 * Reads a 16 bit word from the EEPROM.
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
static int32_t
e1000_spi_eeprom_ready(struct e1000_hw *hw)
{
	uint16_t retry_count = 0;
	uint8_t spi_stat_reg;

	/* Read "Status Register" repeatedly until the LSB is cleared.  The
	 * EEPROM will signal that the command has been completed by clearing
	 * bit 0 of the internal status register.  If it's not cleared within
	 * 5 milliseconds, then error out.
	 */
	retry_count = 0;
	do {
		e1000_shift_out_ee_bits(hw, EEPROM_RDSR_OPCODE_SPI,
		hw->eeprom.opcode_bits);
		spi_stat_reg = (uint8_t)e1000_shift_in_ee_bits(hw, 8);
		if (!(spi_stat_reg & EEPROM_STATUS_RDY_SPI))
			break;

		udelay(5);
		retry_count += 5;

	} while(retry_count < EEPROM_MAX_RETRY_SPI);

	/* ATMEL SPI write time could vary from 0-20mSec on 3.3V devices (and
	 * only 0-5mSec on 5V devices)
	 */
	if(retry_count >= EEPROM_MAX_RETRY_SPI) {
		DEBUGOUT("SPI EEPROM Status error\n");
		return -E1000_ERR_EEPROM;
	}

	return E1000_SUCCESS;
}

/******************************************************************************
 * Reads a 16 bit word from the EEPROM.
 *
 * hw - Struct containing variables accessed by shared code
 * offset - offset of  word in the EEPROM to read
 * data - word read from the EEPROM
 * words - number of words to read
 *****************************************************************************/
static int
e1000_read_eeprom(struct e1000_hw *hw,
                  uint16_t offset,
		  uint16_t words,
                  uint16_t *data)
{
	struct e1000_eeprom_info *eeprom = &hw->eeprom;
	uint32_t i = 0;
	
	DEBUGFUNC("e1000_read_eeprom");

	/* A check for invalid values:  offset too large, too many words, and not
	 * enough words.
	 */
	if((offset > eeprom->word_size) || (words > eeprom->word_size - offset) ||
	   (words == 0)) {
		DEBUGOUT("\"words\" parameter out of bounds\n");
		return -E1000_ERR_EEPROM;
	}

	/*  Prepare the EEPROM for reading  */
	if(e1000_acquire_eeprom(hw) != E1000_SUCCESS)
		return -E1000_ERR_EEPROM;

	if(eeprom->type == e1000_eeprom_spi) {
		uint16_t word_in;
		uint8_t read_opcode = EEPROM_READ_OPCODE_SPI;

		if(e1000_spi_eeprom_ready(hw)) {
			e1000_release_eeprom(hw);
			return -E1000_ERR_EEPROM;
		}

		e1000_standby_eeprom(hw);

		/* Some SPI eeproms use the 8th address bit embedded in the opcode */
		if((eeprom->address_bits == 8) && (offset >= 128))
			read_opcode |= EEPROM_A8_OPCODE_SPI;

		/* Send the READ command (opcode + addr)  */
		e1000_shift_out_ee_bits(hw, read_opcode, eeprom->opcode_bits);
		e1000_shift_out_ee_bits(hw, (uint16_t)(offset*2), eeprom->address_bits);

		/* Read the data.  The address of the eeprom internally increments with
		 * each byte (spi) being read, saving on the overhead of eeprom setup
		 * and tear-down.  The address counter will roll over if reading beyond
		 * the size of the eeprom, thus allowing the entire memory to be read
		 * starting from any offset. */
		for (i = 0; i < words; i++) {
			word_in = e1000_shift_in_ee_bits(hw, 16);
			data[i] = (word_in >> 8) | (word_in << 8);
		}
	} else if(eeprom->type == e1000_eeprom_microwire) {
		for (i = 0; i < words; i++) {
			/*  Send the READ command (opcode + addr)  */
			e1000_shift_out_ee_bits(hw, EEPROM_READ_OPCODE_MICROWIRE,
						eeprom->opcode_bits);
			e1000_shift_out_ee_bits(hw, (uint16_t)(offset + i),
			                        eeprom->address_bits);

			/* Read the data.  For microwire, each word requires the overhead
			 * of eeprom setup and tear-down. */
			data[i] = e1000_shift_in_ee_bits(hw, 16);
			e1000_standby_eeprom(hw);
		}
	}

	/* End this read operation */
	e1000_release_eeprom(hw);

	return E1000_SUCCESS;
}

/******************************************************************************
 * Verifies that the EEPROM has a valid checksum
 * 
 * hw - Struct containing variables accessed by shared code
 *
 * Reads the first 64 16 bit words of the EEPROM and sums the values read.
 * If the the sum of the 64 16 bit words is 0xBABA, the EEPROM's checksum is
 * valid.
 *****************************************************************************/
static int
e1000_validate_eeprom_checksum(struct e1000_hw *hw)
{
	uint16_t checksum = 0;
	uint16_t i, eeprom_data;

	DEBUGFUNC("e1000_validate_eeprom_checksum");

	for(i = 0; i < (EEPROM_CHECKSUM_REG + 1); i++) {
		if(e1000_read_eeprom(hw, i, 1, &eeprom_data) < 0) {
			DEBUGOUT("EEPROM Read Error\n");
			return -E1000_ERR_EEPROM;
		}
		checksum += eeprom_data;
	}
	
	if(checksum == (uint16_t) EEPROM_SUM)
		return E1000_SUCCESS;
	else {
		DEBUGOUT("EEPROM Checksum Invalid\n");    
		return -E1000_ERR_EEPROM;
	}
}

/******************************************************************************
 * Reads the adapter's MAC address from the EEPROM and inverts the LSB for the
 * second function of dual function devices
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
static int 
e1000_read_mac_addr(struct e1000_hw *hw)
{
	uint16_t offset;
	uint16_t eeprom_data;
	int i;

	DEBUGFUNC("e1000_read_mac_addr");

	for(i = 0; i < NODE_ADDRESS_SIZE; i += 2) {
		offset = i >> 1;
		if(e1000_read_eeprom(hw, offset, 1, &eeprom_data) < 0) {
			DEBUGOUT("EEPROM Read Error\n");
			return -E1000_ERR_EEPROM;
		}
		hw->mac_addr[i] = eeprom_data & 0xff;
		hw->mac_addr[i+1] = (eeprom_data >> 8) & 0xff;
	}
	if(((hw->mac_type == e1000_82546) || (hw->mac_type == e1000_82546_rev_3)) &&
		(E1000_READ_REG(hw, STATUS) & E1000_STATUS_FUNC_1))
		/* Invert the last bit if this is the second device */
		hw->mac_addr[5] ^= 1;
	return E1000_SUCCESS;
}

/******************************************************************************
 * Initializes receive address filters.
 *
 * hw - Struct containing variables accessed by shared code 
 *
 * Places the MAC address in receive address register 0 and clears the rest
 * of the receive addresss registers. Clears the multicast table. Assumes
 * the receiver is in reset when the routine is called.
 *****************************************************************************/
static void
e1000_init_rx_addrs(struct e1000_hw *hw)
{
	uint32_t i;
	uint32_t addr_low;
	uint32_t addr_high;
	
	DEBUGFUNC("e1000_init_rx_addrs");
	
	/* Setup the receive address. */
	DEBUGOUT("Programming MAC Address into RAR[0]\n");
	addr_low = (hw->mac_addr[0] |
		(hw->mac_addr[1] << 8) |
		(hw->mac_addr[2] << 16) | (hw->mac_addr[3] << 24));
	
	addr_high = (hw->mac_addr[4] |
		(hw->mac_addr[5] << 8) | E1000_RAH_AV);
	
	E1000_WRITE_REG_ARRAY(hw, RA, 0, addr_low);
	E1000_WRITE_REG_ARRAY(hw, RA, 1, addr_high);
	
	/* Zero out the other 15 receive addresses. */
	DEBUGOUT("Clearing RAR[1-15]\n");
	for(i = 1; i < E1000_RAR_ENTRIES; i++) {
		E1000_WRITE_REG_ARRAY(hw, RA, (i << 1), 0);
		E1000_WRITE_REG_ARRAY(hw, RA, ((i << 1) + 1), 0);
	}
}

/******************************************************************************
 * Clears the VLAN filer table
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
static void
e1000_clear_vfta(struct e1000_hw *hw)
{
	uint32_t offset;
    
	for(offset = 0; offset < E1000_VLAN_FILTER_TBL_SIZE; offset++)
		E1000_WRITE_REG_ARRAY(hw, VFTA, offset, 0);
}

/******************************************************************************
* Writes a value to one of the devices registers using port I/O (as opposed to
* memory mapped I/O). Only 82544 and newer devices support port I/O. *
* hw - Struct containing variables accessed by shared code
* offset - offset to write to * value - value to write
*****************************************************************************/
void e1000_write_reg_io(struct e1000_hw *hw, uint32_t offset, uint32_t value){
	uint32_t io_addr = hw->io_base;
	uint32_t io_data = hw->io_base + 4;
	e1000_io_write(hw, io_addr, offset);
	e1000_io_write(hw, io_data, value);
}

/******************************************************************************
 * Set the phy type member in the hw struct.
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
static int32_t
e1000_set_phy_type(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_set_phy_type");

	switch(hw->phy_id) {
	case M88E1000_E_PHY_ID:
	case M88E1000_I_PHY_ID:
	case M88E1011_I_PHY_ID:
		hw->phy_type = e1000_phy_m88;
		break;
	case IGP01E1000_I_PHY_ID:
		hw->phy_type = e1000_phy_igp;
		break;
	default:
		/* Should never have loaded on this device */
		hw->phy_type = e1000_phy_undefined;
		return -E1000_ERR_PHY_TYPE;
	}

	return E1000_SUCCESS;
}

/******************************************************************************
 * IGP phy init script - initializes the GbE PHY
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
static void
e1000_phy_init_script(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_phy_init_script");

#if 0
	/* See e1000_sw_init() of the Linux driver */
	if(hw->phy_init_script) {
#else
	if((hw->mac_type == e1000_82541) ||
	   (hw->mac_type == e1000_82547) ||
	   (hw->mac_type == e1000_82541_rev_2) ||
	   (hw->mac_type == e1000_82547_rev_2)) {
#endif
		mdelay(20);

		e1000_write_phy_reg(hw,0x0000,0x0140);

		mdelay(5);

		if(hw->mac_type == e1000_82541 || hw->mac_type == e1000_82547) {
			e1000_write_phy_reg(hw, 0x1F95, 0x0001);

			e1000_write_phy_reg(hw, 0x1F71, 0xBD21);

			e1000_write_phy_reg(hw, 0x1F79, 0x0018);

			e1000_write_phy_reg(hw, 0x1F30, 0x1600);

			e1000_write_phy_reg(hw, 0x1F31, 0x0014);

			e1000_write_phy_reg(hw, 0x1F32, 0x161C);

			e1000_write_phy_reg(hw, 0x1F94, 0x0003);

			e1000_write_phy_reg(hw, 0x1F96, 0x003F);

			e1000_write_phy_reg(hw, 0x2010, 0x0008);
		} else {
			e1000_write_phy_reg(hw, 0x1F73, 0x0099);
		}

		e1000_write_phy_reg(hw, 0x0000, 0x3300);


		if(hw->mac_type == e1000_82547) {
			uint16_t fused, fine, coarse;

			/* Move to analog registers page */
			e1000_read_phy_reg(hw, IGP01E1000_ANALOG_SPARE_FUSE_STATUS, &fused);

			if(!(fused & IGP01E1000_ANALOG_SPARE_FUSE_ENABLED)) {
				e1000_read_phy_reg(hw, IGP01E1000_ANALOG_FUSE_STATUS, &fused);

				fine = fused & IGP01E1000_ANALOG_FUSE_FINE_MASK;
				coarse = fused & IGP01E1000_ANALOG_FUSE_COARSE_MASK;

				if(coarse > IGP01E1000_ANALOG_FUSE_COARSE_THRESH) {
					coarse -= IGP01E1000_ANALOG_FUSE_COARSE_10;
					fine -= IGP01E1000_ANALOG_FUSE_FINE_1;
				} else if(coarse == IGP01E1000_ANALOG_FUSE_COARSE_THRESH)
					fine -= IGP01E1000_ANALOG_FUSE_FINE_10;

				fused = (fused & IGP01E1000_ANALOG_FUSE_POLY_MASK) |
					(fine & IGP01E1000_ANALOG_FUSE_FINE_MASK) |
					(coarse & IGP01E1000_ANALOG_FUSE_COARSE_MASK);

				e1000_write_phy_reg(hw, IGP01E1000_ANALOG_FUSE_CONTROL, fused);
				e1000_write_phy_reg(hw, IGP01E1000_ANALOG_FUSE_BYPASS,
						IGP01E1000_ANALOG_FUSE_ENABLE_SW_CONTROL);
			}
		}
	}
}

/******************************************************************************
 * Set the mac type member in the hw struct.
 * 
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
static int
e1000_set_mac_type(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_set_mac_type");

	switch (hw->device_id) {
	case E1000_DEV_ID_82542:
		switch (hw->revision_id) {
		case E1000_82542_2_0_REV_ID:
			hw->mac_type = e1000_82542_rev2_0;
			break;
		case E1000_82542_2_1_REV_ID:
			hw->mac_type = e1000_82542_rev2_1;
			break;
		default:
			/* Invalid 82542 revision ID */
			return -E1000_ERR_MAC_TYPE;
		}
		break;
	case E1000_DEV_ID_82543GC_FIBER:
	case E1000_DEV_ID_82543GC_COPPER:
		hw->mac_type = e1000_82543;
		break;
	case E1000_DEV_ID_82544EI_COPPER:
	case E1000_DEV_ID_82544EI_FIBER:
	case E1000_DEV_ID_82544GC_COPPER:
	case E1000_DEV_ID_82544GC_LOM:
		hw->mac_type = e1000_82544;
		break;
	case E1000_DEV_ID_82540EM:
	case E1000_DEV_ID_82540EM_LOM:
	case E1000_DEV_ID_82540EP:
	case E1000_DEV_ID_82540EP_LOM:
	case E1000_DEV_ID_82540EP_LP:
		hw->mac_type = e1000_82540;
		break;
	case E1000_DEV_ID_82545EM_COPPER:
	case E1000_DEV_ID_82545EM_FIBER:
		hw->mac_type = e1000_82545;
		break;
	case E1000_DEV_ID_82545GM_COPPER:
	case E1000_DEV_ID_82545GM_FIBER:
	case E1000_DEV_ID_82545GM_SERDES:
		hw->mac_type = e1000_82545_rev_3;
		break;
	case E1000_DEV_ID_82546EB_COPPER:
	case E1000_DEV_ID_82546EB_FIBER:
	case E1000_DEV_ID_82546EB_QUAD_COPPER:
		hw->mac_type = e1000_82546;
		break;
	case E1000_DEV_ID_82546GB_COPPER:
	case E1000_DEV_ID_82546GB_FIBER:
	case E1000_DEV_ID_82546GB_SERDES:
		hw->mac_type = e1000_82546_rev_3;
		break;
	case E1000_DEV_ID_82541EI:
	case E1000_DEV_ID_82541EI_MOBILE:
		hw->mac_type = e1000_82541;
		break;
	case E1000_DEV_ID_82541ER:
	case E1000_DEV_ID_82541GI:
	case E1000_DEV_ID_82541GI_MOBILE:
		hw->mac_type = e1000_82541_rev_2;
		break;
	case E1000_DEV_ID_82547EI:
		hw->mac_type = e1000_82547;
		break;
	case E1000_DEV_ID_82547GI:
		hw->mac_type = e1000_82547_rev_2;
		break;
	default:
		/* Should never have loaded on this device */
		return -E1000_ERR_MAC_TYPE;
	}

	return E1000_SUCCESS;
}

/*****************************************************************************
 * Set media type and TBI compatibility.
 *
 * hw - Struct containing variables accessed by shared code
 * **************************************************************************/
static void
e1000_set_media_type(struct e1000_hw *hw)
{
	uint32_t status;

	DEBUGFUNC("e1000_set_media_type");
	
	if(hw->mac_type != e1000_82543) {
		/* tbi_compatibility is only valid on 82543 */
		hw->tbi_compatibility_en = FALSE;
	}

	switch (hw->device_id) {
		case E1000_DEV_ID_82545GM_SERDES:
		case E1000_DEV_ID_82546GB_SERDES:
			hw->media_type = e1000_media_type_internal_serdes;
			break;
		default:
			if(hw->mac_type >= e1000_82543) {
				status = E1000_READ_REG(hw, STATUS);
				if(status & E1000_STATUS_TBIMODE) {
					hw->media_type = e1000_media_type_fiber;
					/* tbi_compatibility not valid on fiber */
					hw->tbi_compatibility_en = FALSE;
				} else {
					hw->media_type = e1000_media_type_copper;
				}
			} else {
				/* This is an 82542 (fiber only) */
				hw->media_type = e1000_media_type_fiber;
			}
	}
}

/******************************************************************************
 * Reset the transmit and receive units; mask and clear all interrupts.
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
static void
e1000_reset_hw(struct e1000_hw *hw)
{
	uint32_t ctrl;
	uint32_t ctrl_ext;
	uint32_t icr;
	uint32_t manc;
	
	DEBUGFUNC("e1000_reset_hw");
	
	/* For 82542 (rev 2.0), disable MWI before issuing a device reset */
	if(hw->mac_type == e1000_82542_rev2_0) {
		DEBUGOUT("Disabling MWI on 82542 rev 2.0\n");
		e1000_pci_clear_mwi(hw);
	}

	/* Clear interrupt mask to stop board from generating interrupts */
	DEBUGOUT("Masking off all interrupts\n");
	E1000_WRITE_REG(hw, IMC, 0xffffffff);
	
	/* Disable the Transmit and Receive units.  Then delay to allow
	 * any pending transactions to complete before we hit the MAC with
	 * the global reset.
	 */
	E1000_WRITE_REG(hw, RCTL, 0);
	E1000_WRITE_REG(hw, TCTL, E1000_TCTL_PSP);
	E1000_WRITE_FLUSH(hw);

	/* The tbi_compatibility_on Flag must be cleared when Rctl is cleared. */
	hw->tbi_compatibility_on = FALSE;

	/* Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */ 
	mdelay(10);

	ctrl = E1000_READ_REG(hw, CTRL);

	/* Must reset the PHY before resetting the MAC */
	if((hw->mac_type == e1000_82541) || (hw->mac_type == e1000_82547)) {
		E1000_WRITE_REG_IO(hw, CTRL, (ctrl | E1000_CTRL_PHY_RST));
		mdelay(5);
	}

	/* Issue a global reset to the MAC.  This will reset the chip's
	 * transmit, receive, DMA, and link units.  It will not effect
	 * the current PCI configuration.  The global reset bit is self-
	 * clearing, and should clear within a microsecond.
	 */
	DEBUGOUT("Issuing a global reset to MAC\n");

	switch(hw->mac_type) {
		case e1000_82544:
		case e1000_82540:
		case e1000_82545:
		case e1000_82546:
		case e1000_82541:
		case e1000_82541_rev_2:
			/* These controllers can't ack the 64-bit write when issuing the
			 * reset, so use IO-mapping as a workaround to issue the reset */
			E1000_WRITE_REG_IO(hw, CTRL, (ctrl | E1000_CTRL_RST));
			break;
		case e1000_82545_rev_3:
		case e1000_82546_rev_3:
			/* Reset is performed on a shadow of the control register */
			E1000_WRITE_REG(hw, CTRL_DUP, (ctrl | E1000_CTRL_RST));
			break;
		default:
			E1000_WRITE_REG(hw, CTRL, (ctrl | E1000_CTRL_RST));
			break;
	}

	/* After MAC reset, force reload of EEPROM to restore power-on settings to
	 * device.  Later controllers reload the EEPROM automatically, so just wait
	 * for reload to complete.
	 */
	switch(hw->mac_type) {
		case e1000_82542_rev2_0:
		case e1000_82542_rev2_1:
		case e1000_82543:
		case e1000_82544:
			/* Wait for reset to complete */
			udelay(10);
			ctrl_ext = E1000_READ_REG(hw, CTRL_EXT);
			ctrl_ext |= E1000_CTRL_EXT_EE_RST;
			E1000_WRITE_REG(hw, CTRL_EXT, ctrl_ext);
			E1000_WRITE_FLUSH(hw);
			/* Wait for EEPROM reload */
			mdelay(2);
			break;
		case e1000_82541:
		case e1000_82541_rev_2:
		case e1000_82547:
		case e1000_82547_rev_2:
			/* Wait for EEPROM reload */
			mdelay(20);
			break;
		default:
			/* Wait for EEPROM reload (it happens automatically) */
			mdelay(5);
			break;
	}

	/* Disable HW ARPs on ASF enabled adapters */
	if(hw->mac_type >= e1000_82540) {
		manc = E1000_READ_REG(hw, MANC);
		manc &= ~(E1000_MANC_ARP_EN);
		E1000_WRITE_REG(hw, MANC, manc);
	}

	if((hw->mac_type == e1000_82541) || (hw->mac_type == e1000_82547)) {
		e1000_phy_init_script(hw);
	}

	/* Clear interrupt mask to stop board from generating interrupts */
	DEBUGOUT("Masking off all interrupts\n");
	E1000_WRITE_REG(hw, IMC, 0xffffffff);
	
	/* Clear any pending interrupt events. */
	icr = E1000_READ_REG(hw, ICR);

	/* If MWI was previously enabled, reenable it. */
	if(hw->mac_type == e1000_82542_rev2_0) {
#ifdef LINUX_DRIVER
		if(hw->pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
#endif
			e1000_pci_set_mwi(hw);
	}
}

/******************************************************************************
 * Performs basic configuration of the adapter.
 *
 * hw - Struct containing variables accessed by shared code
 * 
 * Assumes that the controller has previously been reset and is in a 
 * post-reset uninitialized state. Initializes the receive address registers,
 * multicast table, and VLAN filter table. Calls routines to setup link
 * configuration and flow control settings. Clears all on-chip counters. Leaves
 * the transmit and receive units disabled and uninitialized.
 *****************************************************************************/
static int
e1000_init_hw(struct e1000_hw *hw)
{
	uint32_t ctrl, status;
	uint32_t i;
	int32_t ret_val;
	uint16_t pcix_cmd_word;
	uint16_t pcix_stat_hi_word;
	uint16_t cmd_mmrbc;
	uint16_t stat_mmrbc;
	e1000_bus_type bus_type = e1000_bus_type_unknown;

	DEBUGFUNC("e1000_init_hw");

	/* Set the media type and TBI compatibility */
	e1000_set_media_type(hw);

	/* Disabling VLAN filtering. */
	DEBUGOUT("Initializing the IEEE VLAN\n");
	E1000_WRITE_REG(hw, VET, 0);
	
	e1000_clear_vfta(hw);
	
	/* For 82542 (rev 2.0), disable MWI and put the receiver into reset */
	if(hw->mac_type == e1000_82542_rev2_0) {
		DEBUGOUT("Disabling MWI on 82542 rev 2.0\n");
		e1000_pci_clear_mwi(hw);
		E1000_WRITE_REG(hw, RCTL, E1000_RCTL_RST);
		E1000_WRITE_FLUSH(hw);
		mdelay(5);
	}
	
	/* Setup the receive address. This involves initializing all of the Receive
	 * Address Registers (RARs 0 - 15).
	 */
	e1000_init_rx_addrs(hw);
	
	/* For 82542 (rev 2.0), take the receiver out of reset and enable MWI */
	if(hw->mac_type == e1000_82542_rev2_0) {
		E1000_WRITE_REG(hw, RCTL, 0);
		E1000_WRITE_FLUSH(hw);
		mdelay(1);
#ifdef LINUX_DRIVER
		if(hw->pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
#endif
			e1000_pci_set_mwi(hw);
	}
	
	/* Zero out the Multicast HASH table */
	DEBUGOUT("Zeroing the MTA\n");
	for(i = 0; i < E1000_MC_TBL_SIZE; i++)
		E1000_WRITE_REG_ARRAY(hw, MTA, i, 0);
	
#if 0
	/* Set the PCI priority bit correctly in the CTRL register.  This
	 * determines if the adapter gives priority to receives, or if it
	 * gives equal priority to transmits and receives.
	 */
	if(hw->dma_fairness) {
		ctrl = E1000_READ_REG(hw, CTRL);
		E1000_WRITE_REG(hw, CTRL, ctrl | E1000_CTRL_PRIOR);
	}
#endif

	switch(hw->mac_type) {
		case e1000_82545_rev_3:
		case e1000_82546_rev_3:
			break;
		default:
			if (hw->mac_type >= e1000_82543) {
				/* See e1000_get_bus_info() of the Linux driver */
				status = E1000_READ_REG(hw, STATUS);
				bus_type = (status & E1000_STATUS_PCIX_MODE) ?
					e1000_bus_type_pcix : e1000_bus_type_pci;
			}

			/* Workaround for PCI-X problem when BIOS sets MMRBC incorrectly. */
			if(bus_type == e1000_bus_type_pcix) {
				pci_read_config_word(hw->pdev, PCIX_COMMAND_REGISTER, &pcix_cmd_word);
				pci_read_config_word(hw->pdev, PCIX_STATUS_REGISTER_HI, &pcix_stat_hi_word);
				cmd_mmrbc = (pcix_cmd_word & PCIX_COMMAND_MMRBC_MASK) >>
					PCIX_COMMAND_MMRBC_SHIFT;
				stat_mmrbc = (pcix_stat_hi_word & PCIX_STATUS_HI_MMRBC_MASK) >>
					PCIX_STATUS_HI_MMRBC_SHIFT;
				if(stat_mmrbc == PCIX_STATUS_HI_MMRBC_4K)
					stat_mmrbc = PCIX_STATUS_HI_MMRBC_2K;
				if(cmd_mmrbc > stat_mmrbc) {
					pcix_cmd_word &= ~PCIX_COMMAND_MMRBC_MASK;
					pcix_cmd_word |= stat_mmrbc << PCIX_COMMAND_MMRBC_SHIFT;
					pci_write_config_word(hw->pdev, PCIX_COMMAND_REGISTER, pcix_cmd_word);
				}
			}
			break;
	}

	/* Call a subroutine to configure the link and setup flow control. */
	ret_val = e1000_setup_link(hw);
	
	/* Set the transmit descriptor write-back policy */
	if(hw->mac_type > e1000_82544) {
		ctrl = E1000_READ_REG(hw, TXDCTL);
		ctrl = (ctrl & ~E1000_TXDCTL_WTHRESH) | E1000_TXDCTL_FULL_TX_DESC_WB;
		E1000_WRITE_REG(hw, TXDCTL, ctrl);
	}

#if 0
	/* Clear all of the statistics registers (clear on read).  It is
	 * important that we do this after we have tried to establish link
	 * because the symbol error count will increment wildly if there
	 * is no link.
	 */
	e1000_clear_hw_cntrs(hw);
#endif

	return ret_val;
}

/******************************************************************************
 * Adjust SERDES output amplitude based on EEPROM setting.
 *
 * hw - Struct containing variables accessed by shared code.
 *****************************************************************************/
static int32_t
e1000_adjust_serdes_amplitude(struct e1000_hw *hw)
{
	uint16_t eeprom_data;
	int32_t  ret_val;

	DEBUGFUNC("e1000_adjust_serdes_amplitude");

	if(hw->media_type != e1000_media_type_internal_serdes)
		return E1000_SUCCESS;

	switch(hw->mac_type) {
		case e1000_82545_rev_3:
		case e1000_82546_rev_3:
			break;
		default:
			return E1000_SUCCESS;
	}

	if ((ret_val = e1000_read_eeprom(hw, EEPROM_SERDES_AMPLITUDE, 1,
					&eeprom_data))) {
		return ret_val;
	}

	if(eeprom_data != EEPROM_RESERVED_WORD) {
		/* Adjust SERDES output amplitude only. */
		eeprom_data &= EEPROM_SERDES_AMPLITUDE_MASK;
		if((ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_EXT_CTRL,
		                                  eeprom_data)))
			return ret_val;
	}

	return E1000_SUCCESS;
}
								   
/******************************************************************************
 * Configures flow control and link settings.
 * 
 * hw - Struct containing variables accessed by shared code
 * 
 * Determines which flow control settings to use. Calls the apropriate media-
 * specific link configuration function. Configures the flow control settings.
 * Assuming the adapter has a valid link partner, a valid link should be
 * established. Assumes the hardware has previously been reset and the 
 * transmitter and receiver are not enabled.
 *****************************************************************************/
static int
e1000_setup_link(struct e1000_hw *hw)
{
	uint32_t ctrl_ext;
	int32_t ret_val;
	uint16_t eeprom_data;

	DEBUGFUNC("e1000_setup_link");
	
	/* Read and store word 0x0F of the EEPROM. This word contains bits
	 * that determine the hardware's default PAUSE (flow control) mode,
	 * a bit that determines whether the HW defaults to enabling or
	 * disabling auto-negotiation, and the direction of the
	 * SW defined pins. If there is no SW over-ride of the flow
	 * control setting, then the variable hw->fc will
	 * be initialized based on a value in the EEPROM.
	 */
	if(e1000_read_eeprom(hw, EEPROM_INIT_CONTROL2_REG, 1, &eeprom_data) < 0) {
		DEBUGOUT("EEPROM Read Error\n");
		return -E1000_ERR_EEPROM;
	}
	
	if(hw->fc == e1000_fc_default) {
		if((eeprom_data & EEPROM_WORD0F_PAUSE_MASK) == 0)
			hw->fc = e1000_fc_none;
		else if((eeprom_data & EEPROM_WORD0F_PAUSE_MASK) == 
			EEPROM_WORD0F_ASM_DIR)
			hw->fc = e1000_fc_tx_pause;
		else
			hw->fc = e1000_fc_full;
	}
	
	/* We want to save off the original Flow Control configuration just
	 * in case we get disconnected and then reconnected into a different
	 * hub or switch with different Flow Control capabilities.
	 */
	if(hw->mac_type == e1000_82542_rev2_0)
		hw->fc &= (~e1000_fc_tx_pause);

#if 0
	/* See e1000_sw_init() of the Linux driver */
	if((hw->mac_type < e1000_82543) && (hw->report_tx_early == 1))
#else
	if((hw->mac_type < e1000_82543) && (hw->mac_type >= e1000_82543))
#endif
		hw->fc &= (~e1000_fc_rx_pause);
	
#if 0
	hw->original_fc = hw->fc;
#endif

	DEBUGOUT1("After fix-ups FlowControl is now = %x\n", hw->fc);
	
	/* Take the 4 bits from EEPROM word 0x0F that determine the initial
	 * polarity value for the SW controlled pins, and setup the
	 * Extended Device Control reg with that info.
	 * This is needed because one of the SW controlled pins is used for
	 * signal detection.  So this should be done before e1000_setup_pcs_link()
	 * or e1000_phy_setup() is called.
	 */
	if(hw->mac_type == e1000_82543) {
		ctrl_ext = ((eeprom_data & EEPROM_WORD0F_SWPDIO_EXT) << 
			SWDPIO__EXT_SHIFT);
		E1000_WRITE_REG(hw, CTRL_EXT, ctrl_ext);
	}
	
	/* Call the necessary subroutine to configure the link. */
	ret_val = (hw->media_type == e1000_media_type_copper) ?
		e1000_setup_copper_link(hw) :
		e1000_setup_fiber_serdes_link(hw);
	if (ret_val < 0) {
		return ret_val;
	}
	
	/* Initialize the flow control address, type, and PAUSE timer
	 * registers to their default values.  This is done even if flow
	 * control is disabled, because it does not hurt anything to
	 * initialize these registers.
	 */
	DEBUGOUT("Initializing the Flow Control address, type and timer regs\n");
	
	E1000_WRITE_REG(hw, FCAL, FLOW_CONTROL_ADDRESS_LOW);
	E1000_WRITE_REG(hw, FCAH, FLOW_CONTROL_ADDRESS_HIGH);
	E1000_WRITE_REG(hw, FCT, FLOW_CONTROL_TYPE);
#if 0
	E1000_WRITE_REG(hw, FCTTV, hw->fc_pause_time);
#else
	E1000_WRITE_REG(hw, FCTTV, FC_DEFAULT_TX_TIMER);
#endif
	
	/* Set the flow control receive threshold registers.  Normally,
	 * these registers will be set to a default threshold that may be
	 * adjusted later by the driver's runtime code.  However, if the
	 * ability to transmit pause frames in not enabled, then these
	 * registers will be set to 0. 
	 */
	if(!(hw->fc & e1000_fc_tx_pause)) {
		E1000_WRITE_REG(hw, FCRTL, 0);
		E1000_WRITE_REG(hw, FCRTH, 0);
	} else {
		/* We need to set up the Receive Threshold high and low water marks
		 * as well as (optionally) enabling the transmission of XON frames.
		 */
#if 0
		if(hw->fc_send_xon) {
			E1000_WRITE_REG(hw, FCRTL, (hw->fc_low_water | E1000_FCRTL_XONE));
			E1000_WRITE_REG(hw, FCRTH, hw->fc_high_water);
		} else {
			E1000_WRITE_REG(hw, FCRTL, hw->fc_low_water);
			E1000_WRITE_REG(hw, FCRTH, hw->fc_high_water);
		}
#else
		E1000_WRITE_REG(hw, FCRTL, (FC_DEFAULT_LO_THRESH | E1000_FCRTL_XONE));
		E1000_WRITE_REG(hw, FCRTH, FC_DEFAULT_HI_THRESH);
#endif
	}
	return ret_val;
}

/******************************************************************************
 * Sets up link for a fiber based or serdes based adapter
 *
 * hw - Struct containing variables accessed by shared code
 *
 * Manipulates Physical Coding Sublayer functions in order to configure
 * link. Assumes the hardware has been previously reset and the transmitter
 * and receiver are not enabled.
 *****************************************************************************/
static int
e1000_setup_fiber_serdes_link(struct e1000_hw *hw)
{
	uint32_t ctrl;
	uint32_t status;
	uint32_t txcw = 0;
	uint32_t i;
	uint32_t signal = 0;
	int32_t ret_val;

	DEBUGFUNC("e1000_setup_fiber_serdes_link");

	/* On adapters with a MAC newer than 82544, SW Defineable pin 1 will be 
	 * set when the optics detect a signal. On older adapters, it will be 
	 * cleared when there is a signal.  This applies to fiber media only.
	 * If we're on serdes media, adjust the output amplitude to value set in
	 * the EEPROM.
	 */
	ctrl = E1000_READ_REG(hw, CTRL);
	if(hw->media_type == e1000_media_type_fiber)
		signal = (hw->mac_type > e1000_82544) ? E1000_CTRL_SWDPIN1 : 0;

	if((ret_val = e1000_adjust_serdes_amplitude(hw)))
		return ret_val;

	/* Take the link out of reset */
	ctrl &= ~(E1000_CTRL_LRST);

#if 0
	/* Adjust VCO speed to improve BER performance */
	if((ret_val = e1000_set_vco_speed(hw)))
		return ret_val;
#endif

	e1000_config_collision_dist(hw);
	
	/* Check for a software override of the flow control settings, and setup
	 * the device accordingly.  If auto-negotiation is enabled, then software
	 * will have to set the "PAUSE" bits to the correct value in the Tranmsit
	 * Config Word Register (TXCW) and re-start auto-negotiation.  However, if
	 * auto-negotiation is disabled, then software will have to manually 
	 * configure the two flow control enable bits in the CTRL register.
	 *
	 * The possible values of the "fc" parameter are:
	 *      0:  Flow control is completely disabled
	 *      1:  Rx flow control is enabled (we can receive pause frames, but 
	 *          not send pause frames).
	 *      2:  Tx flow control is enabled (we can send pause frames but we do
	 *          not support receiving pause frames).
	 *      3:  Both Rx and TX flow control (symmetric) are enabled.
	 */
	switch (hw->fc) {
	case e1000_fc_none:
		/* Flow control is completely disabled by a software over-ride. */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD);
		break;
	case e1000_fc_rx_pause:
		/* RX Flow control is enabled and TX Flow control is disabled by a 
		 * software over-ride. Since there really isn't a way to advertise 
		 * that we are capable of RX Pause ONLY, we will advertise that we
		 * support both symmetric and asymmetric RX PAUSE. Later, we will
		 *  disable the adapter's ability to send PAUSE frames.
		 */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
		break;
	case e1000_fc_tx_pause:
		/* TX Flow control is enabled, and RX Flow control is disabled, by a 
		 * software over-ride.
		 */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_ASM_DIR);
		break;
	case e1000_fc_full:
		/* Flow control (both RX and TX) is enabled by a software over-ride. */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly\n");
		return -E1000_ERR_CONFIG;
		break;
	}
	
	/* Since auto-negotiation is enabled, take the link out of reset (the link
	 * will be in reset, because we previously reset the chip). This will
	 * restart auto-negotiation.  If auto-neogtiation is successful then the
	 * link-up status bit will be set and the flow control enable bits (RFCE
	 * and TFCE) will be set according to their negotiated value.
	 */
	DEBUGOUT("Auto-negotiation enabled\n");
	
	E1000_WRITE_REG(hw, TXCW, txcw);
	E1000_WRITE_REG(hw, CTRL, ctrl);
	E1000_WRITE_FLUSH(hw);
	
	hw->txcw = txcw;
	mdelay(1);
	
	/* If we have a signal (the cable is plugged in) then poll for a "Link-Up"
	 * indication in the Device Status Register.  Time-out if a link isn't 
	 * seen in 500 milliseconds seconds (Auto-negotiation should complete in 
	 * less than 500 milliseconds even if the other end is doing it in SW).
	 * For internal serdes, we just assume a signal is present, then poll.
	 */
	if(hw->media_type == e1000_media_type_internal_serdes ||
	   (E1000_READ_REG(hw, CTRL) & E1000_CTRL_SWDPIN1) == signal) {
		DEBUGOUT("Looking for Link\n");
		for(i = 0; i < (LINK_UP_TIMEOUT / 10); i++) {
			mdelay(10);
			status = E1000_READ_REG(hw, STATUS);
			if(status & E1000_STATUS_LU) break;
		}
		if(i == (LINK_UP_TIMEOUT / 10)) {
			DEBUGOUT("Never got a valid link from auto-neg!!!\n");
			hw->autoneg_failed = 1;
			/* AutoNeg failed to achieve a link, so we'll call 
			 * e1000_check_for_link. This routine will force the link up if
			 * we detect a signal. This will allow us to communicate with
			 * non-autonegotiating link partners.
			 */
			if((ret_val = e1000_check_for_link(hw))) {
				DEBUGOUT("Error while checking for link\n");
				return ret_val;
			}
			hw->autoneg_failed = 0;
		} else {
			hw->autoneg_failed = 0;
			DEBUGOUT("Valid Link Found\n");
		}
	} else {
		DEBUGOUT("No Signal Detected\n");
	}
	return E1000_SUCCESS;
}

/******************************************************************************
* Detects which PHY is present and the speed and duplex
*
* hw - Struct containing variables accessed by shared code
******************************************************************************/
static int
e1000_setup_copper_link(struct e1000_hw *hw)
{
	uint32_t ctrl;
	int32_t ret_val;
	uint16_t i;
	uint16_t phy_data;
	
	DEBUGFUNC("e1000_setup_copper_link");
	
	ctrl = E1000_READ_REG(hw, CTRL);
	/* With 82543, we need to force speed and duplex on the MAC equal to what
	 * the PHY speed and duplex configuration is. In addition, we need to
	 * perform a hardware reset on the PHY to take it out of reset.
	 */
	if(hw->mac_type > e1000_82543) {
		ctrl |= E1000_CTRL_SLU;
		ctrl &= ~(E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
		E1000_WRITE_REG(hw, CTRL, ctrl);
	} else {
		ctrl |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX | E1000_CTRL_SLU);
		E1000_WRITE_REG(hw, CTRL, ctrl);
		e1000_phy_hw_reset(hw);
	}
	
	/* Make sure we have a valid PHY */
	if((ret_val = e1000_detect_gig_phy(hw))) {
		DEBUGOUT("Error, did not detect valid phy.\n");
		return ret_val;
	}
	DEBUGOUT1("Phy ID = %x \n", hw->phy_id);

	if(hw->mac_type <= e1000_82543 ||
	   hw->mac_type == e1000_82541 || hw->mac_type == e1000_82547 ||
#if 0
	   hw->mac_type == e1000_82541_rev_2 || hw->mac_type == e1000_82547_rev_2)
		hw->phy_reset_disable = FALSE;

	if(!hw->phy_reset_disable) {
#else
	   hw->mac_type == e1000_82541_rev_2 || hw->mac_type == e1000_82547_rev_2) {
#endif
	if (hw->phy_type == e1000_phy_igp) {

		if((ret_val = e1000_phy_reset(hw))) {
			DEBUGOUT("Error Resetting the PHY\n");
			return ret_val;
		}

		/* Wait 10ms for MAC to configure PHY from eeprom settings */
		mdelay(15);

#if 0
		/* disable lplu d3 during driver init */
		if((ret_val = e1000_set_d3_lplu_state(hw, FALSE))) {
			DEBUGOUT("Error Disabling LPLU D3\n");
			return ret_val;
		}

		/* Configure mdi-mdix settings */
		if((ret_val = e1000_read_phy_reg(hw, IGP01E1000_PHY_PORT_CTRL,
		                                 &phy_data)))
			return ret_val;

		if((hw->mac_type == e1000_82541) || (hw->mac_type == e1000_82547)) {
			hw->dsp_config_state = e1000_dsp_config_disabled;
			/* Force MDI for IGP B-0 PHY */
			phy_data &= ~(IGP01E1000_PSCR_AUTO_MDIX |
			              IGP01E1000_PSCR_FORCE_MDI_MDIX);
			hw->mdix = 1;

		} else {
			hw->dsp_config_state = e1000_dsp_config_enabled;
			phy_data &= ~IGP01E1000_PSCR_AUTO_MDIX;

			switch (hw->mdix) {
			case 1:
				phy_data &= ~IGP01E1000_PSCR_FORCE_MDI_MDIX;
				break;
			case 2:
				phy_data |= IGP01E1000_PSCR_FORCE_MDI_MDIX;
				break;
			case 0:
			default:
				phy_data |= IGP01E1000_PSCR_AUTO_MDIX;
				break;
			}
		}
		if((ret_val = e1000_write_phy_reg(hw, IGP01E1000_PHY_PORT_CTRL,
		                                  phy_data)))
			return ret_val;

		/* set auto-master slave resolution settings */
		e1000_ms_type phy_ms_setting = hw->master_slave;

		if(hw->ffe_config_state == e1000_ffe_config_active)
			hw->ffe_config_state = e1000_ffe_config_enabled;

		if(hw->dsp_config_state == e1000_dsp_config_activated)
			hw->dsp_config_state = e1000_dsp_config_enabled;
#endif

		/* when autonegotiation advertisment is only 1000Mbps then we
		 * should disable SmartSpeed and enable Auto MasterSlave
		 * resolution as hardware default. */
		if(hw->autoneg_advertised == ADVERTISE_1000_FULL) {
			/* Disable SmartSpeed */
			if((ret_val = e1000_read_phy_reg(hw,
			                                 IGP01E1000_PHY_PORT_CONFIG,
			                                 &phy_data)))
				return ret_val;
			phy_data &= ~IGP01E1000_PSCFR_SMART_SPEED;
			if((ret_val = e1000_write_phy_reg(hw,
			                                  IGP01E1000_PHY_PORT_CONFIG,
			                                  phy_data)))
				return ret_val;
			/* Set auto Master/Slave resolution process */
			if((ret_val = e1000_read_phy_reg(hw, PHY_1000T_CTRL,
			                                 &phy_data)))
				return ret_val;
			phy_data &= ~CR_1000T_MS_ENABLE;
			if((ret_val = e1000_write_phy_reg(hw, PHY_1000T_CTRL,
			                                  phy_data)))
				return ret_val;
		}

		if((ret_val = e1000_read_phy_reg(hw, PHY_1000T_CTRL,
		                                 &phy_data)))
			return ret_val;

#if 0
		/* load defaults for future use */
		hw->original_master_slave = (phy_data & CR_1000T_MS_ENABLE) ?
		                            ((phy_data & CR_1000T_MS_VALUE) ?
		                             e1000_ms_force_master :
		                             e1000_ms_force_slave) :
		                             e1000_ms_auto;

		switch (phy_ms_setting) {
		case e1000_ms_force_master:
			phy_data |= (CR_1000T_MS_ENABLE | CR_1000T_MS_VALUE);
			break;
		case e1000_ms_force_slave:
			phy_data |= CR_1000T_MS_ENABLE;
			phy_data &= ~(CR_1000T_MS_VALUE);
			break;
		case e1000_ms_auto:
			phy_data &= ~CR_1000T_MS_ENABLE;
		default:
			break;
		}
#endif

		if((ret_val = e1000_write_phy_reg(hw, PHY_1000T_CTRL,
		                                  phy_data)))
			return ret_val;
	} else {
		/* Enable CRS on TX. This must be set for half-duplex operation. */
		if((ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL,
		                                 &phy_data)))
			return ret_val;

		phy_data |= M88E1000_PSCR_ASSERT_CRS_ON_TX;

		/* Options:
		 *   MDI/MDI-X = 0 (default)
		 *   0 - Auto for all speeds
		 *   1 - MDI mode
		 *   2 - MDI-X mode
		 *   3 - Auto for 1000Base-T only (MDI-X for 10/100Base-T modes)
		 */
#if 0
		phy_data &= ~M88E1000_PSCR_AUTO_X_MODE;

		switch (hw->mdix) {
		case 1:
			phy_data |= M88E1000_PSCR_MDI_MANUAL_MODE;
			break;
		case 2:
			phy_data |= M88E1000_PSCR_MDIX_MANUAL_MODE;
			break;
		case 3:
			phy_data |= M88E1000_PSCR_AUTO_X_1000T;
			break;
		case 0:
		default:
#endif
			phy_data |= M88E1000_PSCR_AUTO_X_MODE;
#if 0
			break;
		}
#endif

		/* Options:
		 *   disable_polarity_correction = 0 (default)
		 *       Automatic Correction for Reversed Cable Polarity
		 *   0 - Disabled
		 *   1 - Enabled
		 */
		phy_data &= ~M88E1000_PSCR_POLARITY_REVERSAL;
		if((ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_SPEC_CTRL,
		                                  phy_data)))
			return ret_val;

		/* Force TX_CLK in the Extended PHY Specific Control Register
		 * to 25MHz clock.
		 */
		if((ret_val = e1000_read_phy_reg(hw, M88E1000_EXT_PHY_SPEC_CTRL,
		                                 &phy_data)))
			return ret_val;

		phy_data |= M88E1000_EPSCR_TX_CLK_25;

#ifdef LINUX_DRIVER
		if (hw->phy_revision < M88E1011_I_REV_4) {
#endif
			/* Configure Master and Slave downshift values */
			phy_data &= ~(M88E1000_EPSCR_MASTER_DOWNSHIFT_MASK |
				M88E1000_EPSCR_SLAVE_DOWNSHIFT_MASK);
			phy_data |= (M88E1000_EPSCR_MASTER_DOWNSHIFT_1X |
				M88E1000_EPSCR_SLAVE_DOWNSHIFT_1X);
			if((ret_val = e1000_write_phy_reg(hw,
			                                  M88E1000_EXT_PHY_SPEC_CTRL,
			                                  phy_data)))
				return ret_val;
		}
	
		/* SW Reset the PHY so all changes take effect */
		if((ret_val = e1000_phy_reset(hw))) {
			DEBUGOUT("Error Resetting the PHY\n");
			return ret_val;
#ifdef LINUX_DRIVER
		}
#endif
	}
	
	/* Options:
	 *   autoneg = 1 (default)
	 *      PHY will advertise value(s) parsed from
	 *      autoneg_advertised and fc
	 *   autoneg = 0
	 *      PHY will be set to 10H, 10F, 100H, or 100F
	 *      depending on value parsed from forced_speed_duplex.
	 */
	
	/* Is autoneg enabled?  This is enabled by default or by software
	 * override.  If so, call e1000_phy_setup_autoneg routine to parse the
	 * autoneg_advertised and fc options. If autoneg is NOT enabled, then
	 * the user should have provided a speed/duplex override.  If so, then
	 * call e1000_phy_force_speed_duplex to parse and set this up.
	 */
	/* Perform some bounds checking on the hw->autoneg_advertised
	 * parameter.  If this variable is zero, then set it to the default.
	 */
	hw->autoneg_advertised &= AUTONEG_ADVERTISE_SPEED_DEFAULT;
	
	/* If autoneg_advertised is zero, we assume it was not defaulted
	 * by the calling code so we set to advertise full capability.
	 */
	if(hw->autoneg_advertised == 0)
		hw->autoneg_advertised = AUTONEG_ADVERTISE_SPEED_DEFAULT;
	
	DEBUGOUT("Reconfiguring auto-neg advertisement params\n");
	if((ret_val = e1000_phy_setup_autoneg(hw))) {
		DEBUGOUT("Error Setting up Auto-Negotiation\n");
		return ret_val;
	}
	DEBUGOUT("Restarting Auto-Neg\n");
	
	/* Restart auto-negotiation by setting the Auto Neg Enable bit and
	 * the Auto Neg Restart bit in the PHY control register.
	 */
	if((ret_val = e1000_read_phy_reg(hw, PHY_CTRL, &phy_data)))
		return ret_val;

	phy_data |= (MII_CR_AUTO_NEG_EN | MII_CR_RESTART_AUTO_NEG);
	if((ret_val = e1000_write_phy_reg(hw, PHY_CTRL, phy_data)))
		return ret_val;

#if 0	
	/* Does the user want to wait for Auto-Neg to complete here, or
	 * check at a later time (for example, callback routine).
	 */
	if(hw->wait_autoneg_complete) {
		if((ret_val = e1000_wait_autoneg(hw))) {
			DEBUGOUT("Error while waiting for autoneg to complete\n");
			return ret_val;
		}
	}
#else
	/* If we do not wait for autonegotiation to complete I 
	 * do not see a valid link status.
	 */
	if((ret_val = e1000_wait_autoneg(hw))) {
		DEBUGOUT("Error while waiting for autoneg to complete\n");
		return ret_val;
	}
#endif
	} /* !hw->phy_reset_disable */
	
	/* Check link status. Wait up to 100 microseconds for link to become
	 * valid.
	 */
	for(i = 0; i < 10; i++) {
		if((ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data)))
			return ret_val;
		if((ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data)))
			return ret_val;

		if(phy_data & MII_SR_LINK_STATUS) {
			/* We have link, so we need to finish the config process:
			 *   1) Set up the MAC to the current PHY speed/duplex
			 *      if we are on 82543.  If we
			 *      are on newer silicon, we only need to configure
			 *      collision distance in the Transmit Control Register.
			 *   2) Set up flow control on the MAC to that established with
			 *      the link partner.
			 */
			if(hw->mac_type >= e1000_82544) {
				e1000_config_collision_dist(hw);
			} else {
				if((ret_val = e1000_config_mac_to_phy(hw))) {
					DEBUGOUT("Error configuring MAC to PHY settings\n");
					return ret_val;
				}
			}
			if((ret_val = e1000_config_fc_after_link_up(hw))) {
				DEBUGOUT("Error Configuring Flow Control\n");
				return ret_val;
			}
#if 0
			if(hw->phy_type == e1000_phy_igp) {
				if((ret_val = e1000_config_dsp_after_link_change(hw, TRUE))) {
					DEBUGOUT("Error Configuring DSP after link up\n");
					return ret_val;
				}
			}
#endif
			DEBUGOUT("Valid link established!!!\n");
			return E1000_SUCCESS;
		}
		udelay(10);
	}
	
	DEBUGOUT("Unable to establish link!!!\n");
	return -E1000_ERR_NOLINK;
}

/******************************************************************************
* Configures PHY autoneg and flow control advertisement settings
*
* hw - Struct containing variables accessed by shared code
******************************************************************************/
static int
e1000_phy_setup_autoneg(struct e1000_hw *hw)
{
	int32_t ret_val;
	uint16_t mii_autoneg_adv_reg;
	uint16_t mii_1000t_ctrl_reg;

	DEBUGFUNC("e1000_phy_setup_autoneg");
	
	/* Read the MII Auto-Neg Advertisement Register (Address 4). */
	if((ret_val = e1000_read_phy_reg(hw, PHY_AUTONEG_ADV,
	                                 &mii_autoneg_adv_reg)))
		return ret_val;

	/* Read the MII 1000Base-T Control Register (Address 9). */
	if((ret_val = e1000_read_phy_reg(hw, PHY_1000T_CTRL, &mii_1000t_ctrl_reg)))
		return ret_val;

	/* Need to parse both autoneg_advertised and fc and set up
	 * the appropriate PHY registers.  First we will parse for
	 * autoneg_advertised software override.  Since we can advertise
	 * a plethora of combinations, we need to check each bit
	 * individually.
	 */
	
	/* First we clear all the 10/100 mb speed bits in the Auto-Neg
	 * Advertisement Register (Address 4) and the 1000 mb speed bits in
	 * the  1000Base-T Control Register (Address 9).
	 */
	mii_autoneg_adv_reg &= ~REG4_SPEED_MASK;
	mii_1000t_ctrl_reg &= ~REG9_SPEED_MASK;

	DEBUGOUT1("autoneg_advertised %x\n", hw->autoneg_advertised);

	/* Do we want to advertise 10 Mb Half Duplex? */
	if(hw->autoneg_advertised & ADVERTISE_10_HALF) {
		DEBUGOUT("Advertise 10mb Half duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_10T_HD_CAPS;
	}

	/* Do we want to advertise 10 Mb Full Duplex? */
	if(hw->autoneg_advertised & ADVERTISE_10_FULL) {
		DEBUGOUT("Advertise 10mb Full duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_10T_FD_CAPS;
	}

	/* Do we want to advertise 100 Mb Half Duplex? */
	if(hw->autoneg_advertised & ADVERTISE_100_HALF) {
		DEBUGOUT("Advertise 100mb Half duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_100TX_HD_CAPS;
	}

	/* Do we want to advertise 100 Mb Full Duplex? */
	if(hw->autoneg_advertised & ADVERTISE_100_FULL) {
		DEBUGOUT("Advertise 100mb Full duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_100TX_FD_CAPS;
	}

	/* We do not allow the Phy to advertise 1000 Mb Half Duplex */
	if(hw->autoneg_advertised & ADVERTISE_1000_HALF) {
		DEBUGOUT("Advertise 1000mb Half duplex requested, request denied!\n");
	}

	/* Do we want to advertise 1000 Mb Full Duplex? */
	if(hw->autoneg_advertised & ADVERTISE_1000_FULL) {
		DEBUGOUT("Advertise 1000mb Full duplex\n");
		mii_1000t_ctrl_reg |= CR_1000T_FD_CAPS;
	}

	/* Check for a software override of the flow control settings, and
	 * setup the PHY advertisement registers accordingly.  If
	 * auto-negotiation is enabled, then software will have to set the
	 * "PAUSE" bits to the correct value in the Auto-Negotiation
	 * Advertisement Register (PHY_AUTONEG_ADV) and re-start auto-negotiation.
	 *
	 * The possible values of the "fc" parameter are:
	 *      0:  Flow control is completely disabled
	 *      1:  Rx flow control is enabled (we can receive pause frames
	 *          but not send pause frames).
	 *      2:  Tx flow control is enabled (we can send pause frames
	 *          but we do not support receiving pause frames).
	 *      3:  Both Rx and TX flow control (symmetric) are enabled.
	 *  other:  No software override.  The flow control configuration
	 *          in the EEPROM is used.
	 */
	switch (hw->fc) {
	case e1000_fc_none: /* 0 */
		/* Flow control (RX & TX) is completely disabled by a
		 * software over-ride.
		 */
		mii_autoneg_adv_reg &= ~(NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	case e1000_fc_rx_pause: /* 1 */
		/* RX Flow control is enabled, and TX Flow control is
		 * disabled, by a software over-ride.
		 */
		/* Since there really isn't a way to advertise that we are
		 * capable of RX Pause ONLY, we will advertise that we
		 * support both symmetric and asymmetric RX PAUSE.  Later
		 * (in e1000_config_fc_after_link_up) we will disable the
		 *hw's ability to send PAUSE frames.
		 */
		mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	case e1000_fc_tx_pause: /* 2 */
		/* TX Flow control is enabled, and RX Flow control is
		 * disabled, by a software over-ride.
		 */
		mii_autoneg_adv_reg |= NWAY_AR_ASM_DIR;
		mii_autoneg_adv_reg &= ~NWAY_AR_PAUSE;
		break;
	case e1000_fc_full: /* 3 */
		/* Flow control (both RX and TX) is enabled by a software
		 * over-ride.
		 */
		mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly\n");
		return -E1000_ERR_CONFIG;
	}

	if((ret_val = e1000_write_phy_reg(hw, PHY_AUTONEG_ADV,
	                       mii_autoneg_adv_reg)))
		return ret_val;

	DEBUGOUT1("Auto-Neg Advertising %x\n", mii_autoneg_adv_reg);

	if((ret_val = e1000_write_phy_reg(hw, PHY_1000T_CTRL, mii_1000t_ctrl_reg)))
		return ret_val;

	return E1000_SUCCESS;
}

/******************************************************************************
* Sets the collision distance in the Transmit Control register
*
* hw - Struct containing variables accessed by shared code
*
* Link should have been established previously. Reads the speed and duplex
* information from the Device Status register.
******************************************************************************/
static void
e1000_config_collision_dist(struct e1000_hw *hw)
{
	uint32_t tctl;

	tctl = E1000_READ_REG(hw, TCTL);
	
	tctl &= ~E1000_TCTL_COLD;
	tctl |= E1000_COLLISION_DISTANCE << E1000_COLD_SHIFT;
	
	E1000_WRITE_REG(hw, TCTL, tctl);
	E1000_WRITE_FLUSH(hw);
}

/******************************************************************************
* Sets MAC speed and duplex settings to reflect the those in the PHY
*
* hw - Struct containing variables accessed by shared code
* mii_reg - data to write to the MII control register
*
* The contents of the PHY register containing the needed information need to
* be passed in.
******************************************************************************/
static int
e1000_config_mac_to_phy(struct e1000_hw *hw)
{
	uint32_t ctrl;
	int32_t ret_val;
	uint16_t phy_data;

	DEBUGFUNC("e1000_config_mac_to_phy");

	/* Read the Device Control Register and set the bits to Force Speed
	 * and Duplex.
	 */
	ctrl = E1000_READ_REG(hw, CTRL);
	ctrl |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	ctrl &= ~(E1000_CTRL_SPD_SEL | E1000_CTRL_ILOS);

	/* Set up duplex in the Device Control and Transmit Control
	 * registers depending on negotiated values.
	 */
	if (hw->phy_type == e1000_phy_igp) {
		if((ret_val = e1000_read_phy_reg(hw, IGP01E1000_PHY_PORT_STATUS,
		                                 &phy_data)))
			return ret_val;

		if(phy_data & IGP01E1000_PSSR_FULL_DUPLEX) ctrl |= E1000_CTRL_FD;
		else ctrl &= ~E1000_CTRL_FD;

		e1000_config_collision_dist(hw);

		/* Set up speed in the Device Control register depending on
		 * negotiated values.
		 */
		if((phy_data & IGP01E1000_PSSR_SPEED_MASK) ==
		   IGP01E1000_PSSR_SPEED_1000MBPS)
			ctrl |= E1000_CTRL_SPD_1000;
		else if((phy_data & IGP01E1000_PSSR_SPEED_MASK) ==
			IGP01E1000_PSSR_SPEED_100MBPS)
			ctrl |= E1000_CTRL_SPD_100;
	} else {
		if((ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_STATUS,
		                                 &phy_data)))
			return ret_val;
		
		if(phy_data & M88E1000_PSSR_DPLX) ctrl |= E1000_CTRL_FD;
		else ctrl &= ~E1000_CTRL_FD;

		e1000_config_collision_dist(hw);

		/* Set up speed in the Device Control register depending on
		 * negotiated values.
		 */
		if((phy_data & M88E1000_PSSR_SPEED) == M88E1000_PSSR_1000MBS)
			ctrl |= E1000_CTRL_SPD_1000;
		else if((phy_data & M88E1000_PSSR_SPEED) == M88E1000_PSSR_100MBS)
			ctrl |= E1000_CTRL_SPD_100;
	}
	/* Write the configured values back to the Device Control Reg. */
	E1000_WRITE_REG(hw, CTRL, ctrl);
	return E1000_SUCCESS;
}

/******************************************************************************
 * Forces the MAC's flow control settings.
 * 
 * hw - Struct containing variables accessed by shared code
 *
 * Sets the TFCE and RFCE bits in the device control register to reflect
 * the adapter settings. TFCE and RFCE need to be explicitly set by
 * software when a Copper PHY is used because autonegotiation is managed
 * by the PHY rather than the MAC. Software must also configure these
 * bits when link is forced on a fiber connection.
 *****************************************************************************/
static int
e1000_force_mac_fc(struct e1000_hw *hw)
{
	uint32_t ctrl;
	
	DEBUGFUNC("e1000_force_mac_fc");
	
	/* Get the current configuration of the Device Control Register */
	ctrl = E1000_READ_REG(hw, CTRL);
	
	/* Because we didn't get link via the internal auto-negotiation
	 * mechanism (we either forced link or we got link via PHY
	 * auto-neg), we have to manually enable/disable transmit an
	 * receive flow control.
	 *
	 * The "Case" statement below enables/disable flow control
	 * according to the "hw->fc" parameter.
	 *
	 * The possible values of the "fc" parameter are:
	 *      0:  Flow control is completely disabled
	 *      1:  Rx flow control is enabled (we can receive pause
	 *          frames but not send pause frames).
	 *      2:  Tx flow control is enabled (we can send pause frames
	 *          frames but we do not receive pause frames).
	 *      3:  Both Rx and TX flow control (symmetric) is enabled.
	 *  other:  No other values should be possible at this point.
	 */
	
	switch (hw->fc) {
	case e1000_fc_none:
		ctrl &= (~(E1000_CTRL_TFCE | E1000_CTRL_RFCE));
		break;
	case e1000_fc_rx_pause:
		ctrl &= (~E1000_CTRL_TFCE);
		ctrl |= E1000_CTRL_RFCE;
		break;
	case e1000_fc_tx_pause:
		ctrl &= (~E1000_CTRL_RFCE);
		ctrl |= E1000_CTRL_TFCE;
		break;
	case e1000_fc_full:
		ctrl |= (E1000_CTRL_TFCE | E1000_CTRL_RFCE);
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly\n");
		return -E1000_ERR_CONFIG;
	}
	
	/* Disable TX Flow Control for 82542 (rev 2.0) */
	if(hw->mac_type == e1000_82542_rev2_0)
		ctrl &= (~E1000_CTRL_TFCE);
	
	E1000_WRITE_REG(hw, CTRL, ctrl);
	return E1000_SUCCESS;
}

/******************************************************************************
 * Configures flow control settings after link is established
 * 
 * hw - Struct containing variables accessed by shared code
 *
 * Should be called immediately after a valid link has been established.
 * Forces MAC flow control settings if link was forced. When in MII/GMII mode
 * and autonegotiation is enabled, the MAC flow control settings will be set
 * based on the flow control negotiated by the PHY. In TBI mode, the TFCE
 * and RFCE bits will be automaticaly set to the negotiated flow control mode.
 *****************************************************************************/
static int
e1000_config_fc_after_link_up(struct e1000_hw *hw)
{
	int32_t ret_val;
	uint16_t mii_status_reg;
	uint16_t mii_nway_adv_reg;
	uint16_t mii_nway_lp_ability_reg;
	uint16_t speed;
	uint16_t duplex;
	
	DEBUGFUNC("e1000_config_fc_after_link_up");
	
	/* Check for the case where we have fiber media and auto-neg failed
	 * so we had to force link.  In this case, we need to force the
	 * configuration of the MAC to match the "fc" parameter.
	 */
	if(((hw->media_type == e1000_media_type_fiber) && (hw->autoneg_failed)) ||
	   ((hw->media_type == e1000_media_type_internal_serdes) && (hw->autoneg_failed))) { 
		if((ret_val = e1000_force_mac_fc(hw))) {
			DEBUGOUT("Error forcing flow control settings\n");
			return ret_val;
		}
	}
	
	/* Check for the case where we have copper media and auto-neg is
	 * enabled.  In this case, we need to check and see if Auto-Neg
	 * has completed, and if so, how the PHY and link partner has
	 * flow control configured.
	 */
	if(hw->media_type == e1000_media_type_copper) {
		/* Read the MII Status Register and check to see if AutoNeg
		 * has completed.  We read this twice because this reg has
		 * some "sticky" (latched) bits.
		 */
		if((ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg)))
			return ret_val;
		if((ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg)))
			return ret_val;
		
		if(mii_status_reg & MII_SR_AUTONEG_COMPLETE) {
			/* The AutoNeg process has completed, so we now need to
			 * read both the Auto Negotiation Advertisement Register
			 * (Address 4) and the Auto_Negotiation Base Page Ability
			 * Register (Address 5) to determine how flow control was
			 * negotiated.
			 */
			if((ret_val = e1000_read_phy_reg(hw, PHY_AUTONEG_ADV,
			                                 &mii_nway_adv_reg)))
				return ret_val;
			if((ret_val = e1000_read_phy_reg(hw, PHY_LP_ABILITY,
			                                 &mii_nway_lp_ability_reg)))
				return ret_val;

			/* Two bits in the Auto Negotiation Advertisement Register
			 * (Address 4) and two bits in the Auto Negotiation Base
			 * Page Ability Register (Address 5) determine flow control
			 * for both the PHY and the link partner.  The following
			 * table, taken out of the IEEE 802.3ab/D6.0 dated March 25,
			 * 1999, describes these PAUSE resolution bits and how flow
			 * control is determined based upon these settings.
			 * NOTE:  DC = Don't Care
			 *
			 *   LOCAL DEVICE  |   LINK PARTNER
			 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | NIC Resolution
			 *-------|---------|-------|---------|--------------------
			 *   0   |    0    |  DC   |   DC    | e1000_fc_none
			 *   0   |    1    |   0   |   DC    | e1000_fc_none
			 *   0   |    1    |   1   |    0    | e1000_fc_none
			 *   0   |    1    |   1   |    1    | e1000_fc_tx_pause
			 *   1   |    0    |   0   |   DC    | e1000_fc_none
			 *   1   |   DC    |   1   |   DC    | e1000_fc_full
			 *   1   |    1    |   0   |    0    | e1000_fc_none
			 *   1   |    1    |   0   |    1    | e1000_fc_rx_pause
			 *
			 */
			/* Are both PAUSE bits set to 1?  If so, this implies
			 * Symmetric Flow Control is enabled at both ends.  The
			 * ASM_DIR bits are irrelevant per the spec.
			 *
			 * For Symmetric Flow Control:
			 *
			 *   LOCAL DEVICE  |   LINK PARTNER
			 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
			 *-------|---------|-------|---------|--------------------
			 *   1   |   DC    |   1   |   DC    | e1000_fc_full
			 *
			 */
			if((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
				(mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE)) {
				/* Now we need to check if the user selected RX ONLY
				 * of pause frames.  In this case, we had to advertise
				 * FULL flow control because we could not advertise RX
				 * ONLY. Hence, we must now check to see if we need to
				 * turn OFF  the TRANSMISSION of PAUSE frames.
				 */
#if 0
				if(hw->original_fc == e1000_fc_full) {
					hw->fc = e1000_fc_full;
#else
				if(hw->fc == e1000_fc_full) {
#endif
					DEBUGOUT("Flow Control = FULL.\r\n");
				} else {
					hw->fc = e1000_fc_rx_pause;
					DEBUGOUT("Flow Control = RX PAUSE frames only.\r\n");
				}
			}
			/* For receiving PAUSE frames ONLY.
			 *
			 *   LOCAL DEVICE  |   LINK PARTNER
			 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
			 *-------|---------|-------|---------|--------------------
			 *   0   |    1    |   1   |    1    | e1000_fc_tx_pause
			 *
			 */
			else if(!(mii_nway_adv_reg & NWAY_AR_PAUSE) &&
				(mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
				(mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
				(mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
				hw->fc = e1000_fc_tx_pause;
				DEBUGOUT("Flow Control = TX PAUSE frames only.\r\n");
			}
			/* For transmitting PAUSE frames ONLY.
			 *
			 *   LOCAL DEVICE  |   LINK PARTNER
			 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
			 *-------|---------|-------|---------|--------------------
			 *   1   |    1    |   0   |    1    | e1000_fc_rx_pause
			 *
			 */
			else if((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
				(mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
				!(mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
				(mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
				hw->fc = e1000_fc_rx_pause;
				DEBUGOUT("Flow Control = RX PAUSE frames only.\r\n");
			}
			/* Per the IEEE spec, at this point flow control should be
			 * disabled.  However, we want to consider that we could
			 * be connected to a legacy switch that doesn't advertise
			 * desired flow control, but can be forced on the link
			 * partner.  So if we advertised no flow control, that is
			 * what we will resolve to.  If we advertised some kind of
			 * receive capability (Rx Pause Only or Full Flow Control)
			 * and the link partner advertised none, we will configure
			 * ourselves to enable Rx Flow Control only.  We can do
			 * this safely for two reasons:  If the link partner really
			 * didn't want flow control enabled, and we enable Rx, no
			 * harm done since we won't be receiving any PAUSE frames
			 * anyway.  If the intent on the link partner was to have
			 * flow control enabled, then by us enabling RX only, we
			 * can at least receive pause frames and process them.
			 * This is a good idea because in most cases, since we are
			 * predominantly a server NIC, more times than not we will
			 * be asked to delay transmission of packets than asking
			 * our link partner to pause transmission of frames.
			 */
#if 0
			else if(hw->original_fc == e1000_fc_none ||
				hw->original_fc == e1000_fc_tx_pause) {
#else
			else if(hw->fc == e1000_fc_none)
				DEBUGOUT("Flow Control = NONE.\r\n");
			else if(hw->fc == e1000_fc_tx_pause) {
#endif
				hw->fc = e1000_fc_none;
				DEBUGOUT("Flow Control = NONE.\r\n");
			} else {
				hw->fc = e1000_fc_rx_pause;
				DEBUGOUT("Flow Control = RX PAUSE frames only.\r\n");
			}
			
			/* Now we need to do one last check...  If we auto-
			 * negotiated to HALF DUPLEX, flow control should not be
			 * enabled per IEEE 802.3 spec.
			 */
			e1000_get_speed_and_duplex(hw, &speed, &duplex);
			
			if(duplex == HALF_DUPLEX)
				hw->fc = e1000_fc_none;
			
			/* Now we call a subroutine to actually force the MAC
			 * controller to use the correct flow control settings.
			 */
			if((ret_val = e1000_force_mac_fc(hw))) {
				DEBUGOUT("Error forcing flow control settings\n");
				return ret_val;
			}
		} else {
			DEBUGOUT("Copper PHY and Auto Neg has not completed.\r\n");
		}
	}
	return E1000_SUCCESS;
}

/******************************************************************************
 * Checks to see if the link status of the hardware has changed.
 *
 * hw - Struct containing variables accessed by shared code
 *
 * Called by any function that needs to check the link status of the adapter.
 *****************************************************************************/
static int
e1000_check_for_link(struct e1000_hw *hw)
{
	uint32_t rxcw;
	uint32_t ctrl;
	uint32_t status;
	uint32_t rctl;
	uint32_t signal = 0;
	int32_t ret_val;
	uint16_t phy_data;
	uint16_t lp_capability;
	
	DEBUGFUNC("e1000_check_for_link");
	
	/* On adapters with a MAC newer than 82544, SW Defineable pin 1 will be 
	 * set when the optics detect a signal. On older adapters, it will be 
	 * cleared when there is a signal.  This applies to fiber media only.
	 */
	if(hw->media_type == e1000_media_type_fiber)
		signal = (hw->mac_type > e1000_82544) ? E1000_CTRL_SWDPIN1 : 0;

	ctrl = E1000_READ_REG(hw, CTRL);
	status = E1000_READ_REG(hw, STATUS);
	rxcw = E1000_READ_REG(hw, RXCW);
	
	/* If we have a copper PHY then we only want to go out to the PHY
	 * registers to see if Auto-Neg has completed and/or if our link
	 * status has changed.  The get_link_status flag will be set if we
	 * receive a Link Status Change interrupt or we have Rx Sequence
	 * Errors.
	 */
#if 0
	if((hw->media_type == e1000_media_type_copper) && hw->get_link_status) {
#else
	if(hw->media_type == e1000_media_type_copper) {
#endif
		/* First we want to see if the MII Status Register reports
		 * link.  If so, then we want to get the current speed/duplex
		 * of the PHY.
		 * Read the register twice since the link bit is sticky.
		 */
		if((ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data)))
			return ret_val;
		if((ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data)))
			return ret_val;
		
		if(phy_data & MII_SR_LINK_STATUS) {
#if 0
			hw->get_link_status = FALSE;
#endif
		} else {
			/* No link detected */
			return -E1000_ERR_NOLINK;
		}

		/* We have a M88E1000 PHY and Auto-Neg is enabled.  If we
		 * have Si on board that is 82544 or newer, Auto
		 * Speed Detection takes care of MAC speed/duplex
		 * configuration.  So we only need to configure Collision
		 * Distance in the MAC.  Otherwise, we need to force
		 * speed/duplex on the MAC to the current PHY speed/duplex
		 * settings.
		 */
		if(hw->mac_type >= e1000_82544)
			e1000_config_collision_dist(hw);
		else {
			if((ret_val = e1000_config_mac_to_phy(hw))) {
				DEBUGOUT("Error configuring MAC to PHY settings\n");
				return ret_val;
			}
		}
		
		/* Configure Flow Control now that Auto-Neg has completed. First, we 
		 * need to restore the desired flow control settings because we may
		 * have had to re-autoneg with a different link partner.
		 */
		if((ret_val = e1000_config_fc_after_link_up(hw))) {
			DEBUGOUT("Error configuring flow control\n");
			return ret_val;
		}
		
		/* At this point we know that we are on copper and we have
		 * auto-negotiated link.  These are conditions for checking the link
		 * parter capability register.  We use the link partner capability to
		 * determine if TBI Compatibility needs to be turned on or off.  If
		 * the link partner advertises any speed in addition to Gigabit, then
		 * we assume that they are GMII-based, and TBI compatibility is not
		 * needed. If no other speeds are advertised, we assume the link
		 * partner is TBI-based, and we turn on TBI Compatibility.
		 */
		if(hw->tbi_compatibility_en) {
			if((ret_val = e1000_read_phy_reg(hw, PHY_LP_ABILITY,
			                                 &lp_capability)))
				return ret_val;
			if(lp_capability & (NWAY_LPAR_10T_HD_CAPS |
                                NWAY_LPAR_10T_FD_CAPS |
                                NWAY_LPAR_100TX_HD_CAPS |
                                NWAY_LPAR_100TX_FD_CAPS |
                                NWAY_LPAR_100T4_CAPS)) {
				/* If our link partner advertises anything in addition to 
				 * gigabit, we do not need to enable TBI compatibility.
				 */
				if(hw->tbi_compatibility_on) {
					/* If we previously were in the mode, turn it off. */
					rctl = E1000_READ_REG(hw, RCTL);
					rctl &= ~E1000_RCTL_SBP;
					E1000_WRITE_REG(hw, RCTL, rctl);
					hw->tbi_compatibility_on = FALSE;
				}
			} else {
				/* If TBI compatibility is was previously off, turn it on. For
				 * compatibility with a TBI link partner, we will store bad
				 * packets. Some frames have an additional byte on the end and
				 * will look like CRC errors to to the hardware.
				 */
				if(!hw->tbi_compatibility_on) {
					hw->tbi_compatibility_on = TRUE;
					rctl = E1000_READ_REG(hw, RCTL);
					rctl |= E1000_RCTL_SBP;
					E1000_WRITE_REG(hw, RCTL, rctl);
				}
			}
		}
	}
	/* If we don't have link (auto-negotiation failed or link partner cannot
	 * auto-negotiate), the cable is plugged in (we have signal), and our
	 * link partner is not trying to auto-negotiate with us (we are receiving
	 * idles or data), we need to force link up. We also need to give
	 * auto-negotiation time to complete, in case the cable was just plugged
	 * in. The autoneg_failed flag does this.
	 */
	else if((((hw->media_type == e1000_media_type_fiber) &&
	        ((ctrl & E1000_CTRL_SWDPIN1) == signal)) ||
	        (hw->media_type == e1000_media_type_internal_serdes)) &&
		(!(status & E1000_STATUS_LU)) &&
		(!(rxcw & E1000_RXCW_C))) {
		if(hw->autoneg_failed == 0) {
			hw->autoneg_failed = 1;
			return 0;
		}
		DEBUGOUT("NOT RXing /C/, disable AutoNeg and force link.\r\n");
		
		/* Disable auto-negotiation in the TXCW register */
		E1000_WRITE_REG(hw, TXCW, (hw->txcw & ~E1000_TXCW_ANE));
		
		/* Force link-up and also force full-duplex. */
		ctrl = E1000_READ_REG(hw, CTRL);
		ctrl |= (E1000_CTRL_SLU | E1000_CTRL_FD);
		E1000_WRITE_REG(hw, CTRL, ctrl);
		
		/* Configure Flow Control after forcing link up. */
		if((ret_val = e1000_config_fc_after_link_up(hw))) {
			DEBUGOUT("Error configuring flow control\n");
			return ret_val;
		}
	}
	/* If we are forcing link and we are receiving /C/ ordered sets, re-enable
	 * auto-negotiation in the TXCW register and disable forced link in the
	 * Device Control register in an attempt to auto-negotiate with our link
	 * partner.
	 */
	else if(((hw->media_type == e1000_media_type_fiber)  ||
	         (hw->media_type == e1000_media_type_internal_serdes)) &&
		(ctrl & E1000_CTRL_SLU) &&
		(rxcw & E1000_RXCW_C)) {
		DEBUGOUT("RXing /C/, enable AutoNeg and stop forcing link.\r\n");
		E1000_WRITE_REG(hw, TXCW, hw->txcw);
		E1000_WRITE_REG(hw, CTRL, (ctrl & ~E1000_CTRL_SLU));
	}
#if 0
	/* If we force link for non-auto-negotiation switch, check link status
	 * based on MAC synchronization for internal serdes media type.
	 */
	else if((hw->media_type == e1000_media_type_internal_serdes) &&
			!(E1000_TXCW_ANE & E1000_READ_REG(hw, TXCW))) {
		/* SYNCH bit and IV bit are sticky. */
		udelay(10);
		if(E1000_RXCW_SYNCH & E1000_READ_REG(hw, RXCW)) {
			if(!(rxcw & E1000_RXCW_IV)) {
				hw->serdes_link_down = FALSE;
				DEBUGOUT("SERDES: Link is up.\n");
			}
		} else {
			hw->serdes_link_down = TRUE;
			DEBUGOUT("SERDES: Link is down.\n");
		}
	}
#endif
	return E1000_SUCCESS;
}

/******************************************************************************
 * Detects the current speed and duplex settings of the hardware.
 *
 * hw - Struct containing variables accessed by shared code
 * speed - Speed of the connection
 * duplex - Duplex setting of the connection
 *****************************************************************************/
static void 
e1000_get_speed_and_duplex(struct e1000_hw *hw,
                           uint16_t *speed,
                           uint16_t *duplex)
{
	uint32_t status;
	
	DEBUGFUNC("e1000_get_speed_and_duplex");
	
	if(hw->mac_type >= e1000_82543) {
		status = E1000_READ_REG(hw, STATUS);
		if(status & E1000_STATUS_SPEED_1000) {
			*speed = SPEED_1000;
			DEBUGOUT("1000 Mbs, ");
		} else if(status & E1000_STATUS_SPEED_100) {
			*speed = SPEED_100;
			DEBUGOUT("100 Mbs, ");
		} else {
			*speed = SPEED_10;
			DEBUGOUT("10 Mbs, ");
		}
		
		if(status & E1000_STATUS_FD) {
			*duplex = FULL_DUPLEX;
			DEBUGOUT("Full Duplex\r\n");
		} else {
			*duplex = HALF_DUPLEX;
			DEBUGOUT(" Half Duplex\r\n");
		}
	} else {
		DEBUGOUT("1000 Mbs, Full Duplex\r\n");
		*speed = SPEED_1000;
		*duplex = FULL_DUPLEX;
	}
}

/******************************************************************************
* Blocks until autoneg completes or times out (~4.5 seconds)
*
* hw - Struct containing variables accessed by shared code
******************************************************************************/
static int
e1000_wait_autoneg(struct e1000_hw *hw)
{
	int32_t ret_val;
	uint16_t i;
	uint16_t phy_data;
	
	DEBUGFUNC("e1000_wait_autoneg");
	DEBUGOUT("Waiting for Auto-Neg to complete.\n");
	
	/* We will wait for autoneg to complete or 4.5 seconds to expire. */
	for(i = PHY_AUTO_NEG_TIME; i > 0; i--) {
		/* Read the MII Status Register and wait for Auto-Neg
		 * Complete bit to be set.
		 */
		if((ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data)))
			return ret_val;
		if((ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data)))
			return ret_val;
		if(phy_data & MII_SR_AUTONEG_COMPLETE) {
			DEBUGOUT("Auto-Neg complete.\n");
			return E1000_SUCCESS;
		}
		mdelay(100);
	}
	DEBUGOUT("Auto-Neg timedout.\n");
	return -E1000_ERR_TIMEOUT;
}

/******************************************************************************
* Raises the Management Data Clock
*
* hw - Struct containing variables accessed by shared code
* ctrl - Device control register's current value
******************************************************************************/
static void
e1000_raise_mdi_clk(struct e1000_hw *hw,
                    uint32_t *ctrl)
{
	/* Raise the clock input to the Management Data Clock (by setting the MDC
	 * bit), and then delay 10 microseconds.
	 */
	E1000_WRITE_REG(hw, CTRL, (*ctrl | E1000_CTRL_MDC));
	E1000_WRITE_FLUSH(hw);
	udelay(10);
}

/******************************************************************************
* Lowers the Management Data Clock
*
* hw - Struct containing variables accessed by shared code
* ctrl - Device control register's current value
******************************************************************************/
static void
e1000_lower_mdi_clk(struct e1000_hw *hw,
                    uint32_t *ctrl)
{
	/* Lower the clock input to the Management Data Clock (by clearing the MDC
	 * bit), and then delay 10 microseconds.
	 */
	E1000_WRITE_REG(hw, CTRL, (*ctrl & ~E1000_CTRL_MDC));
	E1000_WRITE_FLUSH(hw);
	udelay(10);
}

/******************************************************************************
* Shifts data bits out to the PHY
*
* hw - Struct containing variables accessed by shared code
* data - Data to send out to the PHY
* count - Number of bits to shift out
*
* Bits are shifted out in MSB to LSB order.
******************************************************************************/
static void
e1000_shift_out_mdi_bits(struct e1000_hw *hw,
                         uint32_t data,
                         uint16_t count)
{
	uint32_t ctrl;
	uint32_t mask;

	/* We need to shift "count" number of bits out to the PHY. So, the value
	 * in the "data" parameter will be shifted out to the PHY one bit at a 
	 * time. In order to do this, "data" must be broken down into bits.
	 */
	mask = 0x01;
	mask <<= (count - 1);
	
	ctrl = E1000_READ_REG(hw, CTRL);
	
	/* Set MDIO_DIR and MDC_DIR direction bits to be used as output pins. */
	ctrl |= (E1000_CTRL_MDIO_DIR | E1000_CTRL_MDC_DIR);
	
	while(mask) {
		/* A "1" is shifted out to the PHY by setting the MDIO bit to "1" and
		 * then raising and lowering the Management Data Clock. A "0" is
		 * shifted out to the PHY by setting the MDIO bit to "0" and then
		 * raising and lowering the clock.
		 */
		if(data & mask) ctrl |= E1000_CTRL_MDIO;
		else ctrl &= ~E1000_CTRL_MDIO;
		
		E1000_WRITE_REG(hw, CTRL, ctrl);
		E1000_WRITE_FLUSH(hw);
		
		udelay(10);

		e1000_raise_mdi_clk(hw, &ctrl);
		e1000_lower_mdi_clk(hw, &ctrl);

		mask = mask >> 1;
	}
}

/******************************************************************************
* Shifts data bits in from the PHY
*
* hw - Struct containing variables accessed by shared code
*
* Bits are shifted in in MSB to LSB order. 
******************************************************************************/
static uint16_t
e1000_shift_in_mdi_bits(struct e1000_hw *hw)
{
	uint32_t ctrl;
	uint16_t data = 0;
	uint8_t i;

	/* In order to read a register from the PHY, we need to shift in a total
	 * of 18 bits from the PHY. The first two bit (turnaround) times are used
	 * to avoid contention on the MDIO pin when a read operation is performed.
	 * These two bits are ignored by us and thrown away. Bits are "shifted in"
	 * by raising the input to the Management Data Clock (setting the MDC bit),
	 * and then reading the value of the MDIO bit.
	 */ 
	ctrl = E1000_READ_REG(hw, CTRL);
	
	/* Clear MDIO_DIR (SWDPIO1) to indicate this bit is to be used as input. */
	ctrl &= ~E1000_CTRL_MDIO_DIR;
	ctrl &= ~E1000_CTRL_MDIO;
	
	E1000_WRITE_REG(hw, CTRL, ctrl);
	E1000_WRITE_FLUSH(hw);
	
	/* Raise and Lower the clock before reading in the data. This accounts for
	 * the turnaround bits. The first clock occurred when we clocked out the
	 * last bit of the Register Address.
	 */
	e1000_raise_mdi_clk(hw, &ctrl);
	e1000_lower_mdi_clk(hw, &ctrl);
	
	for(data = 0, i = 0; i < 16; i++) {
		data = data << 1;
		e1000_raise_mdi_clk(hw, &ctrl);
		ctrl = E1000_READ_REG(hw, CTRL);
		/* Check to see if we shifted in a "1". */
		if(ctrl & E1000_CTRL_MDIO) data |= 1;
		e1000_lower_mdi_clk(hw, &ctrl);
	}
	
	e1000_raise_mdi_clk(hw, &ctrl);
	e1000_lower_mdi_clk(hw, &ctrl);
	
	return data;
}

/*****************************************************************************
* Reads the value from a PHY register, if the value is on a specific non zero
* page, sets the page first.
*
* hw - Struct containing variables accessed by shared code
* reg_addr - address of the PHY register to read
******************************************************************************/
static int
e1000_read_phy_reg(struct e1000_hw *hw,
                   uint32_t reg_addr,
                   uint16_t *phy_data)
{
	uint32_t ret_val;

	DEBUGFUNC("e1000_read_phy_reg");

	if(hw->phy_type == e1000_phy_igp &&
	   (reg_addr > MAX_PHY_MULTI_PAGE_REG)) {
		if((ret_val = e1000_write_phy_reg_ex(hw, IGP01E1000_PHY_PAGE_SELECT,
		                                     (uint16_t)reg_addr)))
			return ret_val;
	}

	ret_val = e1000_read_phy_reg_ex(hw, IGP01E1000_PHY_PAGE_SELECT & reg_addr,
	                                phy_data);

	return ret_val;
}

static int
e1000_read_phy_reg_ex(struct e1000_hw *hw,
                      uint32_t reg_addr,
                      uint16_t *phy_data)
{
	uint32_t i;
	uint32_t mdic = 0;
	const uint32_t phy_addr = 1;

	DEBUGFUNC("e1000_read_phy_reg_ex");
	
	if(reg_addr > MAX_PHY_REG_ADDRESS) {
		DEBUGOUT1("PHY Address %d is out of range\n", reg_addr);
		return -E1000_ERR_PARAM;
	}
	
	if(hw->mac_type > e1000_82543) {
		/* Set up Op-code, Phy Address, and register address in the MDI
		 * Control register.  The MAC will take care of interfacing with the
		 * PHY to retrieve the desired data.
		 */
		mdic = ((reg_addr << E1000_MDIC_REG_SHIFT) |
			(phy_addr << E1000_MDIC_PHY_SHIFT) | 
			(E1000_MDIC_OP_READ));
		
		E1000_WRITE_REG(hw, MDIC, mdic);

		/* Poll the ready bit to see if the MDI read completed */
		for(i = 0; i < 64; i++) {
			udelay(50);
			mdic = E1000_READ_REG(hw, MDIC);
			if(mdic & E1000_MDIC_READY) break;
		}
		if(!(mdic & E1000_MDIC_READY)) {
			DEBUGOUT("MDI Read did not complete\n");
			return -E1000_ERR_PHY;
		}
		if(mdic & E1000_MDIC_ERROR) {
			DEBUGOUT("MDI Error\n");
			return -E1000_ERR_PHY;
		}
		*phy_data = (uint16_t) mdic;
	} else {
		/* We must first send a preamble through the MDIO pin to signal the
		 * beginning of an MII instruction.  This is done by sending 32
		 * consecutive "1" bits.
		 */
		e1000_shift_out_mdi_bits(hw, PHY_PREAMBLE, PHY_PREAMBLE_SIZE);
		
		/* Now combine the next few fields that are required for a read
		 * operation.  We use this method instead of calling the
		 * e1000_shift_out_mdi_bits routine five different times. The format of
		 * a MII read instruction consists of a shift out of 14 bits and is
		 * defined as follows:
		 *    <Preamble><SOF><Op Code><Phy Addr><Reg Addr>
		 * followed by a shift in of 18 bits.  This first two bits shifted in
		 * are TurnAround bits used to avoid contention on the MDIO pin when a
		 * READ operation is performed.  These two bits are thrown away
		 * followed by a shift in of 16 bits which contains the desired data.
		 */
		mdic = ((reg_addr) | (phy_addr << 5) | 
			(PHY_OP_READ << 10) | (PHY_SOF << 12));
		
		e1000_shift_out_mdi_bits(hw, mdic, 14);
		
		/* Now that we've shifted out the read command to the MII, we need to
		 * "shift in" the 16-bit value (18 total bits) of the requested PHY
		 * register address.
		 */
		*phy_data = e1000_shift_in_mdi_bits(hw);
	}
	return E1000_SUCCESS;
}

/******************************************************************************
* Writes a value to a PHY register
*
* hw - Struct containing variables accessed by shared code
* reg_addr - address of the PHY register to write
* data - data to write to the PHY
******************************************************************************/
static int 
e1000_write_phy_reg(struct e1000_hw *hw,
                    uint32_t reg_addr,
                    uint16_t phy_data)
{
	uint32_t ret_val;

	DEBUGFUNC("e1000_write_phy_reg");

	if(hw->phy_type == e1000_phy_igp &&
	   (reg_addr > MAX_PHY_MULTI_PAGE_REG)) {
		if((ret_val = e1000_write_phy_reg_ex(hw, IGP01E1000_PHY_PAGE_SELECT,
		                                     (uint16_t)reg_addr)))
			return ret_val;
	}

	ret_val = e1000_write_phy_reg_ex(hw, IGP01E1000_PHY_PAGE_SELECT & reg_addr,
	                                 phy_data);

	return ret_val;
}

static int
e1000_write_phy_reg_ex(struct e1000_hw *hw,
                       uint32_t reg_addr,
                       uint16_t phy_data)
{
	uint32_t i;
	uint32_t mdic = 0;
	const uint32_t phy_addr = 1;
	
	DEBUGFUNC("e1000_write_phy_reg_ex");
	
	if(reg_addr > MAX_PHY_REG_ADDRESS) {
		DEBUGOUT1("PHY Address %d is out of range\n", reg_addr);
		return -E1000_ERR_PARAM;
	}
	
	if(hw->mac_type > e1000_82543) {
		/* Set up Op-code, Phy Address, register address, and data intended
		 * for the PHY register in the MDI Control register.  The MAC will take
		 * care of interfacing with the PHY to send the desired data.
		 */
		mdic = (((uint32_t) phy_data) |
			(reg_addr << E1000_MDIC_REG_SHIFT) |
			(phy_addr << E1000_MDIC_PHY_SHIFT) | 
			(E1000_MDIC_OP_WRITE));
		
		E1000_WRITE_REG(hw, MDIC, mdic);
		
		/* Poll the ready bit to see if the MDI read completed */
		for(i = 0; i < 640; i++) {
			udelay(5);
			mdic = E1000_READ_REG(hw, MDIC);
			if(mdic & E1000_MDIC_READY) break;
		}
		if(!(mdic & E1000_MDIC_READY)) {
			DEBUGOUT("MDI Write did not complete\n");
			return -E1000_ERR_PHY;
		}
	} else {
		/* We'll need to use the SW defined pins to shift the write command
		 * out to the PHY. We first send a preamble to the PHY to signal the
		 * beginning of the MII instruction.  This is done by sending 32 
		 * consecutive "1" bits.
		 */
		e1000_shift_out_mdi_bits(hw, PHY_PREAMBLE, PHY_PREAMBLE_SIZE);
		
		/* Now combine the remaining required fields that will indicate a 
		 * write operation. We use this method instead of calling the
		 * e1000_shift_out_mdi_bits routine for each field in the command. The
		 * format of a MII write instruction is as follows:
		 * <Preamble><SOF><Op Code><Phy Addr><Reg Addr><Turnaround><Data>.
		 */
		mdic = ((PHY_TURNAROUND) | (reg_addr << 2) | (phy_addr << 7) |
			(PHY_OP_WRITE << 12) | (PHY_SOF << 14));
		mdic <<= 16;
		mdic |= (uint32_t) phy_data;
		
		e1000_shift_out_mdi_bits(hw, mdic, 32);
	}

	return E1000_SUCCESS;
}

/******************************************************************************
* Returns the PHY to the power-on reset state
*
* hw - Struct containing variables accessed by shared code
******************************************************************************/
static void
e1000_phy_hw_reset(struct e1000_hw *hw)
{
	uint32_t ctrl, ctrl_ext;

	DEBUGFUNC("e1000_phy_hw_reset");
	
	DEBUGOUT("Resetting Phy...\n");
	
	if(hw->mac_type > e1000_82543) {
		/* Read the device control register and assert the E1000_CTRL_PHY_RST
		 * bit. Then, take it out of reset.
		 */
		ctrl = E1000_READ_REG(hw, CTRL);
		E1000_WRITE_REG(hw, CTRL, ctrl | E1000_CTRL_PHY_RST);
		E1000_WRITE_FLUSH(hw);
		mdelay(10);
		E1000_WRITE_REG(hw, CTRL, ctrl);
		E1000_WRITE_FLUSH(hw);
	} else {
		/* Read the Extended Device Control Register, assert the PHY_RESET_DIR
		 * bit to put the PHY into reset. Then, take it out of reset.
		 */
		ctrl_ext = E1000_READ_REG(hw, CTRL_EXT);
		ctrl_ext |= E1000_CTRL_EXT_SDP4_DIR;
		ctrl_ext &= ~E1000_CTRL_EXT_SDP4_DATA;
		E1000_WRITE_REG(hw, CTRL_EXT, ctrl_ext);
		E1000_WRITE_FLUSH(hw);
		mdelay(10);
		ctrl_ext |= E1000_CTRL_EXT_SDP4_DATA;
		E1000_WRITE_REG(hw, CTRL_EXT, ctrl_ext);
		E1000_WRITE_FLUSH(hw);
	}
	udelay(150);
}

/******************************************************************************
* Resets the PHY
*
* hw - Struct containing variables accessed by shared code
*
* Sets bit 15 of the MII Control regiser
******************************************************************************/
static int 
e1000_phy_reset(struct e1000_hw *hw)
{
	int32_t ret_val;
	uint16_t phy_data;

	DEBUGFUNC("e1000_phy_reset");

	if(hw->mac_type != e1000_82541_rev_2) {
		if((ret_val = e1000_read_phy_reg(hw, PHY_CTRL, &phy_data)))
			return ret_val;
		
		phy_data |= MII_CR_RESET;
		if((ret_val = e1000_write_phy_reg(hw, PHY_CTRL, phy_data)))
			return ret_val;
		
		udelay(1);
	} else e1000_phy_hw_reset(hw);

	if(hw->phy_type == e1000_phy_igp)
		e1000_phy_init_script(hw);

	return E1000_SUCCESS;
}

/******************************************************************************
* Probes the expected PHY address for known PHY IDs
*
* hw - Struct containing variables accessed by shared code
******************************************************************************/
static int
e1000_detect_gig_phy(struct e1000_hw *hw)
{
	int32_t phy_init_status, ret_val;
	uint16_t phy_id_high, phy_id_low;
	boolean_t match = FALSE;

	DEBUGFUNC("e1000_detect_gig_phy");
	
	/* Read the PHY ID Registers to identify which PHY is onboard. */
	if((ret_val = e1000_read_phy_reg(hw, PHY_ID1, &phy_id_high)))
		return ret_val;

	hw->phy_id = (uint32_t) (phy_id_high << 16);
	udelay(20);
	if((ret_val = e1000_read_phy_reg(hw, PHY_ID2, &phy_id_low)))
		return ret_val;
	
	hw->phy_id |= (uint32_t) (phy_id_low & PHY_REVISION_MASK);
#ifdef LINUX_DRIVER
	hw->phy_revision = (uint32_t) phy_id_low & ~PHY_REVISION_MASK;
#endif
	
	switch(hw->mac_type) {
	case e1000_82543:
		if(hw->phy_id == M88E1000_E_PHY_ID) match = TRUE;
		break;
	case e1000_82544:
		if(hw->phy_id == M88E1000_I_PHY_ID) match = TRUE;
		break;
	case e1000_82540:
	case e1000_82545:
	case e1000_82545_rev_3:
	case e1000_82546:
	case e1000_82546_rev_3:
		if(hw->phy_id == M88E1011_I_PHY_ID) match = TRUE;
		break;
	case e1000_82541:
	case e1000_82541_rev_2:
	case e1000_82547:
	case e1000_82547_rev_2:
		if(hw->phy_id == IGP01E1000_I_PHY_ID) match = TRUE;
		break;
	default:
		DEBUGOUT1("Invalid MAC type %d\n", hw->mac_type);
		return -E1000_ERR_CONFIG;
	}
	phy_init_status = e1000_set_phy_type(hw);

	if ((match) && (phy_init_status == E1000_SUCCESS)) {
		DEBUGOUT1("PHY ID 0x%X detected\n", hw->phy_id);
		return E1000_SUCCESS;
	}
	DEBUGOUT1("Invalid PHY ID 0x%X\n", hw->phy_id);
	return -E1000_ERR_PHY;
}

/******************************************************************************
 * Sets up eeprom variables in the hw struct.  Must be called after mac_type
 * is configured.
 *
 * hw - Struct containing variables accessed by shared code
 *****************************************************************************/
static void
e1000_init_eeprom_params(struct e1000_hw *hw)
{
	struct e1000_eeprom_info *eeprom = &hw->eeprom;
	uint32_t eecd = E1000_READ_REG(hw, EECD);
	uint16_t eeprom_size;

	DEBUGFUNC("e1000_init_eeprom_params");

	switch (hw->mac_type) {
	case e1000_82542_rev2_0:
	case e1000_82542_rev2_1:
	case e1000_82543:
	case e1000_82544:
		eeprom->type = e1000_eeprom_microwire;
		eeprom->word_size = 64;
		eeprom->opcode_bits = 3;
		eeprom->address_bits = 6;
		eeprom->delay_usec = 50;
		break;
	case e1000_82540:
	case e1000_82545:
	case e1000_82545_rev_3:
	case e1000_82546:
	case e1000_82546_rev_3:
		eeprom->type = e1000_eeprom_microwire;
		eeprom->opcode_bits = 3;
		eeprom->delay_usec = 50;
		if(eecd & E1000_EECD_SIZE) {
			eeprom->word_size = 256;
			eeprom->address_bits = 8;
		} else {
			eeprom->word_size = 64;
			eeprom->address_bits = 6;
		}
		break;
	case e1000_82541:
	case e1000_82541_rev_2:
	case e1000_82547:
	case e1000_82547_rev_2:
		if (eecd & E1000_EECD_TYPE) {
			eeprom->type = e1000_eeprom_spi;
			if (eecd & E1000_EECD_ADDR_BITS) {
				eeprom->page_size = 32;
				eeprom->address_bits = 16;
			} else {
				eeprom->page_size = 8;
				eeprom->address_bits = 8;
			}
		} else {
			eeprom->type = e1000_eeprom_microwire;
			eeprom->opcode_bits = 3;
			eeprom->delay_usec = 50;
			if (eecd & E1000_EECD_ADDR_BITS) {
				eeprom->word_size = 256;
				eeprom->address_bits = 8;
			} else {
				eeprom->word_size = 64;
				eeprom->address_bits = 6;
			}
		}
		break;
	default:
		eeprom->type = e1000_eeprom_spi;
		if (eecd & E1000_EECD_ADDR_BITS) {
			eeprom->page_size = 32;
			eeprom->address_bits = 16;
		} else {
			eeprom->page_size = 8;
			eeprom->address_bits = 8;
		}
		break;
	}

	if (eeprom->type == e1000_eeprom_spi) {
		eeprom->opcode_bits = 8;
		eeprom->delay_usec = 1;
		eeprom->word_size = 64;
		if (e1000_read_eeprom(hw, EEPROM_CFG, 1, &eeprom_size) == 0) {
			eeprom_size &= EEPROM_SIZE_MASK;

			switch (eeprom_size) {
			case EEPROM_SIZE_16KB:
				eeprom->word_size = 8192;
				break;
			case EEPROM_SIZE_8KB:
				eeprom->word_size = 4096;
				break;
			case EEPROM_SIZE_4KB:
				eeprom->word_size = 2048;
				break;
			case EEPROM_SIZE_2KB:
				eeprom->word_size = 1024;
				break;
			case EEPROM_SIZE_1KB:
				eeprom->word_size = 512;
				break;
			case EEPROM_SIZE_512B:
				eeprom->word_size = 256;
				break;
			case EEPROM_SIZE_128B:
			default:
				break;
			}
		}
	}
}

/**
 * e1000_reset - Reset the adapter
 */

static int
e1000_reset(struct e1000_hw *hw)
{
	uint32_t pba;
	/* Repartition Pba for greater than 9k mtu
	 * To take effect CTRL.RST is required.
	 */

	if(hw->mac_type < e1000_82547) {
		pba = E1000_PBA_48K;
	} else {
		pba = E1000_PBA_30K;
	}
	E1000_WRITE_REG(hw, PBA, pba);

	/* flow control settings */
#if 0
	hw->fc_high_water = FC_DEFAULT_HI_THRESH;
	hw->fc_low_water = FC_DEFAULT_LO_THRESH;
	hw->fc_pause_time = FC_DEFAULT_TX_TIMER;
	hw->fc_send_xon = 1;
	hw->fc = hw->original_fc;
#endif
	
	e1000_reset_hw(hw);
	if(hw->mac_type >= e1000_82544)
		E1000_WRITE_REG(hw, WUC, 0);
	return e1000_init_hw(hw);
}

/**
 * e1000_sw_init - Initialize general software structures (struct e1000_adapter)
 * @adapter: board private structure to initialize
 *
 * e1000_sw_init initializes the Adapter private data structure.
 * Fields are initialized based on PCI device information and
 * OS network device settings (MTU size).
 **/

static int 
e1000_sw_init(struct pci_device *pdev, struct e1000_hw *hw)
{
	int result;

	/* PCI config space info */
	pci_read_config_word(pdev, PCI_VENDOR_ID, &hw->vendor_id);
	pci_read_config_word(pdev, PCI_DEVICE_ID, &hw->device_id);
	pci_read_config_byte(pdev, PCI_REVISION, &hw->revision_id);
#if 0
	pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID,
                             &hw->subsystem_vendor_id);
	pci_read_config_word(pdev, PCI_SUBSYSTEM_ID, &hw->subsystem_id);
#endif

	pci_read_config_word(pdev, PCI_COMMAND, &hw->pci_cmd_word);

	/* identify the MAC */

	result = e1000_set_mac_type(hw);
	if (result) {
		E1000_ERR("Unknown MAC Type\n");
		return result;
	}

	/* initialize eeprom parameters */

	e1000_init_eeprom_params(hw);

#if 0
	if((hw->mac_type == e1000_82541) ||
	   (hw->mac_type == e1000_82547) ||
	   (hw->mac_type == e1000_82541_rev_2) ||
	   (hw->mac_type == e1000_82547_rev_2))
		hw->phy_init_script = 1;
#endif

	e1000_set_media_type(hw);

#if 0
	if(hw->mac_type < e1000_82543)
		hw->report_tx_early = 0;
	else
		hw->report_tx_early = 1;

	hw->wait_autoneg_complete = FALSE;
#endif
	hw->tbi_compatibility_en = TRUE;
#if 0
	hw->adaptive_ifs = TRUE;

	/* Copper options */

	if(hw->media_type == e1000_media_type_copper) {
		hw->mdix = AUTO_ALL_MODES;
		hw->disable_polarity_correction = FALSE;
		hw->master_slave = E1000_MASTER_SLAVE;
	}
#endif
	return E1000_SUCCESS;
}

static void fill_rx (void)
{
	struct e1000_rx_desc *rd;
	rx_last = rx_tail;
	rd = rx_base + rx_tail;
	rx_tail = (rx_tail + 1) % 8;
	memset (rd, 0, 16);
	rd->buffer_addr = virt_to_bus(&packet);
	E1000_WRITE_REG (&hw, RDT, rx_tail);
}

static void init_descriptor (void)
{
	unsigned long ptr;
	unsigned long tctl;

	ptr = virt_to_phys(tx_pool);
	if (ptr & 0xf)
		ptr = (ptr + 0x10) & (~0xf);

	tx_base = phys_to_virt(ptr);

	E1000_WRITE_REG (&hw, TDBAL, virt_to_bus(tx_base));
	E1000_WRITE_REG (&hw, TDBAH, 0);
	E1000_WRITE_REG (&hw, TDLEN, 128);

	/* Setup the HW Tx Head and Tail descriptor pointers */

	E1000_WRITE_REG (&hw, TDH, 0);
	E1000_WRITE_REG (&hw, TDT, 0);
	tx_tail = 0;

	/* Program the Transmit Control Register */

#ifdef LINUX_DRIVER_TCTL
	tctl = E1000_READ_REG(&hw, TCTL);

	tctl &= ~E1000_TCTL_CT;
	tctl |= E1000_TCTL_EN | E1000_TCTL_PSP |
		(E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);
#else
	tctl = E1000_TCTL_PSP | E1000_TCTL_EN |
		(E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT) | 
		(E1000_HDX_COLLISION_DISTANCE << E1000_COLD_SHIFT);
#endif

	E1000_WRITE_REG (&hw, TCTL, tctl);

	e1000_config_collision_dist(&hw);


	rx_tail = 0;
	/* disable receive */
	E1000_WRITE_REG (&hw, RCTL, 0);
	ptr = virt_to_phys(rx_pool);
	if (ptr & 0xf)
		ptr = (ptr + 0x10) & (~0xf);
	rx_base = phys_to_virt(ptr);

	/* Setup the Base and Length of the Rx Descriptor Ring */

	E1000_WRITE_REG (&hw, RDBAL, virt_to_bus(rx_base));
	E1000_WRITE_REG (&hw, RDBAH, 0);

	E1000_WRITE_REG (&hw, RDLEN, 128);

	/* Setup the HW Rx Head and Tail Descriptor Pointers */
	E1000_WRITE_REG (&hw, RDH, 0);
	E1000_WRITE_REG (&hw, RDT, 0);

	E1000_WRITE_REG (&hw, RCTL, 
		E1000_RCTL_EN | 
		E1000_RCTL_BAM | 
		E1000_RCTL_SZ_2048 | 
		E1000_RCTL_MPE);
	fill_rx();
}



/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int
e1000_poll (struct nic *nic, int retrieve)
{
	/* return true if there's an ethernet packet ready to read */
	/* nic->packet should contain data on return */
	/* nic->packetlen should contain length of data */
	struct e1000_rx_desc *rd;
	uint32_t icr;

	rd = rx_base + rx_last;
	if (!rd->status & E1000_RXD_STAT_DD)
		return 0;

	if ( ! retrieve ) return 1;

	//      printf("recv: packet %! -> %! len=%d \n", packet+6, packet,rd->Length);
	memcpy (nic->packet, packet, rd->length);
	nic->packetlen = rd->length;
	fill_rx ();

	/* Acknowledge interrupt. */
	icr = E1000_READ_REG(&hw, ICR);

	return 1;
}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void
e1000_transmit (struct nic *nic, const char *d,	/* Destination */
		    unsigned int type,	/* Type */
		    unsigned int size,	/* size */
		    const char *p)	/* Packet */
{
	/* send the packet to destination */
	struct eth_hdr {
		unsigned char dst_addr[ETH_ALEN];
		unsigned char src_addr[ETH_ALEN];
		unsigned short type;
	} hdr;
	struct e1000_tx_desc *txhd;	/* header */
	struct e1000_tx_desc *txp;	/* payload */
	DEBUGFUNC("send");

	memcpy (&hdr.dst_addr, d, ETH_ALEN);
	memcpy (&hdr.src_addr, nic->node_addr, ETH_ALEN);

	hdr.type = htons (type);
	txhd = tx_base + tx_tail;
	tx_tail = (tx_tail + 1) % 8;
	txp = tx_base + tx_tail;
	tx_tail = (tx_tail + 1) % 8;

	txhd->buffer_addr = virt_to_bus (&hdr);
	txhd->lower.data = sizeof (hdr);
	txhd->upper.data = 0;

	txp->buffer_addr = virt_to_bus(p);
	txp->lower.data = E1000_TXD_CMD_RPS | E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | size;
	txp->upper.data = 0;

	E1000_WRITE_REG (&hw, TDT, tx_tail);
	while (!(txp->upper.data & E1000_TXD_STAT_DD)) {
		udelay(10);	/* give the nic a chance to write to the register */
		poll_interruptions();
	}
	DEBUGFUNC("send end");
}


/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void e1000_disable (struct dev *dev __unused)
{
	/* Clear the transmit ring */
	E1000_WRITE_REG (&hw, TDH, 0);
	E1000_WRITE_REG (&hw, TDT, 0);

	/* Clear the receive ring */
	E1000_WRITE_REG (&hw, RDH, 0);
	E1000_WRITE_REG (&hw, RDT, 0);

	/* put the card in its initial state */
	switch(hw.mac_type) {
		case e1000_82544:
		case e1000_82540:
		case e1000_82545:
		case e1000_82546:
		case e1000_82541:
		case e1000_82541_rev_2:
			/* These controllers can't ack the 64-bit write when issuing the
			 * reset, so use IO-mapping as a workaround to issue the reset */
			E1000_WRITE_REG_IO(&hw, CTRL, E1000_CTRL_RST);
			break;
		case e1000_82545_rev_3:
		case e1000_82546_rev_3:
			/* Reset is performed on a shadow of the control register */
			E1000_WRITE_REG(&hw, CTRL_DUP, E1000_CTRL_RST);
			break;
		default:
			E1000_WRITE_REG(&hw, CTRL, E1000_CTRL_RST);
			break;
	}

	/* Turn off the ethernet interface */
	E1000_WRITE_REG (&hw, RCTL, 0);
	E1000_WRITE_REG (&hw, TCTL, 0);
	mdelay (10);

	/* Unmap my window to the device */
	iounmap(hw.hw_addr);
}

/**************************************************************************
IRQ - Enable, Disable, or Force interrupts
***************************************************************************/
static void e1000_irq(struct nic *nic __unused, irq_action_t action)
{
	switch ( action ) {
	case DISABLE :
		E1000_WRITE_REG(&hw, IMC, ~0);
		E1000_WRITE_FLUSH(&hw);
		break;
	case ENABLE :
		E1000_WRITE_REG(&hw, IMS,
				E1000_IMS_RXT0 | E1000_IMS_RXSEQ);
		E1000_WRITE_FLUSH(&hw);
		break;
	case FORCE :
		E1000_WRITE_REG(&hw, ICS, E1000_ICS_RXT0);
		break;
	}
}

#define IORESOURCE_IO	0x00000100     /* Resource type */
#define BAR_0		0
#define BAR_1		1
#define BAR_5		5

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
You should omit the last argument struct pci_device * for a non-PCI NIC
***************************************************************************/
static int e1000_probe(struct dev *dev, struct pci_device *p)
{
	struct nic *nic = (struct nic *)dev;
	unsigned long mmio_start, mmio_len;
	int ret_val, i;

	if (p == 0)
		return 0;
	/* Initialize hw with default values */
	memset(&hw, 0, sizeof(hw));
	hw.pdev = p;

#if 1
	/* Are these variables needed? */
	hw.fc                    = e1000_fc_none;
#if 0
	hw.original_fc           = e1000_fc_none;
#endif
	hw.autoneg_failed        = 0;
#if 0
	hw.get_link_status       = TRUE;
#endif
#endif

	mmio_start = pci_bar_start(p, PCI_BASE_ADDRESS_0);
	mmio_len   = pci_bar_size(p,  PCI_BASE_ADDRESS_0);
	hw.hw_addr = ioremap(mmio_start, mmio_len);

	for(i = BAR_1; i <= BAR_5; i++) {
		if(pci_bar_size(p, i) == 0)
			continue;                
		if(pci_find_capability(p, i) & IORESOURCE_IO) {
			hw.io_base = pci_bar_start(p, i);
			break;
                }        
	}

	adjust_pci_device(p);

	nic->ioaddr   = p->ioaddr & ~3;
	nic->irqno    = p->irq;

	/* From Matt Hortman <mbhortman@acpthinclient.com> */
	/* MAC and Phy settings */

	/* setup the private structure */
	if (e1000_sw_init(p, &hw) < 0) {
		iounmap(hw.hw_addr);
		return 0;
	}

	/* make sure the EEPROM is good */

	if (e1000_validate_eeprom_checksum(&hw) < 0) {
		printf ("The EEPROM Checksum Is Not Valid\n");
		iounmap(hw.hw_addr);
		return 0;
	}

	/* copy the MAC address out of the EEPROM */

	e1000_read_mac_addr(&hw);
	memcpy (nic->node_addr, hw.mac_addr, ETH_ALEN);
	
	printf("Ethernet addr: %!\n", nic->node_addr);

	/* reset the hardware with the new settings */

	ret_val = e1000_reset(&hw);
	if (ret_val < 0) {
		if ((ret_val == -E1000_ERR_NOLINK) ||
			(ret_val == -E1000_ERR_TIMEOUT)) {
			E1000_ERR("Valid Link not detected\n");
		} else {
			E1000_ERR("Hardware Initialization Failed\n");
		}
		iounmap(hw.hw_addr);
		return 0;
	}
	init_descriptor();

	/* point to NIC specific routines */
	dev->disable  = e1000_disable;
	nic->poll     = e1000_poll;
	nic->transmit = e1000_transmit;
	nic->irq      = e1000_irq;

	return 1;
}

static struct pci_id e1000_nics[] = {
PCI_ROM(0x8086, 0x1000, "e1000-82542",               "Intel EtherExpressPro1000"),
PCI_ROM(0x8086, 0x1001, "e1000-82543gc-fiber",       "Intel EtherExpressPro1000 82543GC Fiber"),
PCI_ROM(0x8086, 0x1004, "e1000-82543gc-copper",	     "Intel EtherExpressPro1000 82543GC Copper"),
PCI_ROM(0x8086, 0x1008, "e1000-82544ei-copper",      "Intel EtherExpressPro1000 82544EI Copper"),
PCI_ROM(0x8086, 0x1009, "e1000-82544ei-fiber",       "Intel EtherExpressPro1000 82544EI Fiber"),
PCI_ROM(0x8086, 0x100C, "e1000-82544gc-copper",      "Intel EtherExpressPro1000 82544GC Copper"),
PCI_ROM(0x8086, 0x100D, "e1000-82544gc-lom",         "Intel EtherExpressPro1000 82544GC LOM"),
PCI_ROM(0x8086, 0x100E, "e1000-82540em",     	     "Intel EtherExpressPro1000 82540EM"),
PCI_ROM(0x8086, 0x100F, "e1000-82545em-copper",      "Intel EtherExpressPro1000 82545EM Copper"),
PCI_ROM(0x8086, 0x1010, "e1000-82546eb-copper",      "Intel EtherExpressPro1000 82546EB Copper"),
PCI_ROM(0x8086, 0x1011, "e1000-82545em-fiber",       "Intel EtherExpressPro1000 82545EM Fiber"),
PCI_ROM(0x8086, 0x1012, "e1000-82546eb-fiber", 	     "Intel EtherExpressPro1000 82546EB Copper"),
PCI_ROM(0x8086, 0x1013, "e1000-82541ei",	     "Intel EtherExpressPro1000 82541EI"),
PCI_ROM(0x8086, 0x1015, "e1000-82540em-lom",  	     "Intel EtherExpressPro1000 82540EM LOM"),
PCI_ROM(0x8086, 0x1016, "e1000-82540ep-lom",	     "Intel EtherExpressPro1000 82540EP LOM"),
PCI_ROM(0x8086, 0x1017, "e1000-82540ep",	     "Intel EtherExpressPro1000 82540EP"),
PCI_ROM(0x8086, 0x1018, "e1000-82541ep",	     "Intel EtherExpressPro1000 82541EP"),
PCI_ROM(0x8086, 0x1019, "e1000-82547ei",	     "Intel EtherExpressPro1000 82547EI"),
PCI_ROM(0x8086, 0x101d, "e1000-82546eb-quad-copper", "Intel EtherExpressPro1000 82546EB Quad Copper"),
PCI_ROM(0x8086, 0x101e, "e1000-82540ep-lp",	     "Intel EtherExpressPro1000 82540EP LP"),
PCI_ROM(0x8086, 0x1026, "e1000-82545gm-copper",	     "Intel EtherExpressPro1000 82545GM Copper"),
PCI_ROM(0x8086, 0x1027, "e1000-82545gm-fiber",	     "Intel EtherExpressPro1000 82545GM Fiber"),
PCI_ROM(0x8086, 0x1028, "e1000-82545gm-serdes",	     "Intel EtherExpressPro1000 82545GM SERDES"),
PCI_ROM(0x8086, 0x1075, "e1000-82547gi",	     "Intel EtherExpressPro1000 82547GI"),
PCI_ROM(0x8086, 0x1076, "e1000-82541gi",	     "Intel EtherExpressPro1000 82541GI"),
PCI_ROM(0x8086, 0x1077, "e1000-82541gi-mobile",	     "Intel EtherExpressPro1000 82541GI Mobile"),
PCI_ROM(0x8086, 0x1078, "e1000-82541er",	     "Intel EtherExpressPro1000 82541ER"),
PCI_ROM(0x8086, 0x1079, "e1000-82546gb-copper",	     "Intel EtherExpressPro1000 82546GB Copper"),
PCI_ROM(0x8086, 0x107a, "e1000-82546gb-fiber",	     "Intel EtherExpressPro1000 82546GB Fiber"),
PCI_ROM(0x8086, 0x107b, "e1000-82546gb-serdes",	     "Intel EtherExpressPro1000 82546GB SERDES"),
};

static struct pci_driver e1000_driver __pci_driver = {
	.type     = NIC_DRIVER,
	.name     = "E1000",
	.probe    = e1000_probe,
	.ids      = e1000_nics,
	.id_count = sizeof(e1000_nics)/sizeof(e1000_nics[0]),
	.class    = 0,
};
