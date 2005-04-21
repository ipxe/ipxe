#ifdef ALLMULTI
#error multicast support is not yet implemented
#endif
 /*------------------------------------------------------------------------
 * smc9000.c
 * This is a Etherboot driver for SMC's 9000 series of Ethernet cards.
 *
 * Copyright (C) 1998 Daniel Engström <daniel.engstrom@riksnett.no>
 * Based on the Linux SMC9000 driver, smc9194.c by Eric Stahlman
 * Copyright (C) 1996 by Erik Stahlman <eric@vt.edu>
 *
 * This software may be used and distributed according to the terms
 * of the GNU Public License, incorporated herein by reference.
 *
 * "Features" of the SMC chip:
 *   4608 byte packet memory. ( for the 91C92/4.  Others have more )
 *   EEPROM for configuration
 *   AUI/TP selection
 *
 * Authors
 *	Erik Stahlman				<erik@vt.edu>
 *      Daniel Engström                         <daniel.engstrom@riksnett.no>
 *
 * History
 * 98-09-25              Daniel Engström Etherboot driver crated from Eric's
 *                                       Linux driver.
 *
 *---------------------------------------------------------------------------*/
#define LINUX_OUT_MACROS 1
#define SMC9000_DEBUG    0

#include "etherboot.h"
#include "nic.h"
#include "isa.h"
#include "smc9000.h"

# define _outb outb
# define _outw outw

static const char       smc9000_version[] = "Version 0.99 98-09-30";
static const char       *interfaces[ 2 ] = { "TP", "AUI" };
static const char       *chip_ids[ 15 ] =  {
   NULL, NULL, NULL,
   /* 3 */ "SMC91C90/91C92",
   /* 4 */ "SMC91C94",
   /* 5 */ "SMC91C95",
   NULL,
   /* 7 */ "SMC91C100",
   /* 8 */ "SMC91C100FD",
   NULL, NULL, NULL,
   NULL, NULL, NULL
};
static const char      smc91c96_id[] = "SMC91C96";

/*
 * Function: smc_reset( int ioaddr )
 * Purpose:
 *	This sets the SMC91xx chip to its normal state, hopefully from whatever
 *	mess that any other DOS driver has put it in.
 *
 * Maybe I should reset more registers to defaults in here?  SOFTRESET  should
 * do that for me.
 *
 * Method:
 *	1.  send a SOFT RESET
 *	2.  wait for it to finish
 *	3.  reset the memory management unit
 *      4.  clear all interrupts
 *
*/
static void smc_reset(int ioaddr)
{
   /* This resets the registers mostly to defaults, but doesn't
    * affect EEPROM.  That seems unnecessary */
   SMC_SELECT_BANK(ioaddr, 0);
   _outw( RCR_SOFTRESET, ioaddr + RCR );

   /* this should pause enough for the chip to be happy */
   SMC_DELAY(ioaddr);

   /* Set the transmit and receive configuration registers to
    * default values */
   _outw(RCR_CLEAR, ioaddr + RCR);
   _outw(TCR_CLEAR, ioaddr + TCR);

   /* Reset the MMU */
   SMC_SELECT_BANK(ioaddr, 2);
   _outw( MC_RESET, ioaddr + MMU_CMD );

   /* Note:  It doesn't seem that waiting for the MMU busy is needed here,
    * but this is a place where future chipsets _COULD_ break.  Be wary
    * of issuing another MMU command right after this */
   _outb(0, ioaddr + INT_MASK);
}


/*----------------------------------------------------------------------
 * Function: smc9000_probe_addr( int ioaddr )
 *
 * Purpose:
 *	Tests to see if a given ioaddr points to an SMC9xxx chip.
 *	Returns a 1 on success
 *
 * Algorithm:
 *	(1) see if the high byte of BANK_SELECT is 0x33
 *	(2) compare the ioaddr with the base register's address
 *	(3) see if I recognize the chip ID in the appropriate register
 *
 * ---------------------------------------------------------------------
 */
static int smc9000_probe_addr( isa_probe_addr_t ioaddr )
{
   word bank;
   word	revision_register;
   word base_address_register;

   /* First, see if the high byte is 0x33 */
   bank = inw(ioaddr + BANK_SELECT);
   if ((bank & 0xFF00) != 0x3300) {
      return 0;
   }
   /* The above MIGHT indicate a device, but I need to write to further
    *	test this.  */
   _outw(0x0, ioaddr + BANK_SELECT);
   bank = inw(ioaddr + BANK_SELECT);
   if ((bank & 0xFF00) != 0x3300) {
      return 0;
   }

   /* well, we've already written once, so hopefully another time won't
    *  hurt.  This time, I need to switch the bank register to bank 1,
    *  so I can access the base address register */
   SMC_SELECT_BANK(ioaddr, 1);
   base_address_register = inw(ioaddr + BASE);

   if (ioaddr != (base_address_register >> 3 & 0x3E0))  {
      DBG("SMC9000: IOADDR %hX doesn't match configuration (%hX)."
	  "Probably not a SMC chip\n",
	  ioaddr, base_address_register >> 3 & 0x3E0);
      /* well, the base address register didn't match.  Must not have
       * been a SMC chip after all. */
      return 0;
   }


   /* check if the revision register is something that I recognize.
    * These might need to be added to later, as future revisions
    * could be added.  */
   SMC_SELECT_BANK(ioaddr, 3);
   revision_register  = inw(ioaddr + REVISION);
   if (!chip_ids[(revision_register >> 4) & 0xF]) {
      /* I don't recognize this chip, so... */
      DBG( "SMC9000: IO %hX: Unrecognized revision register:"
	   " %hX, Contact author.\n", ioaddr, revision_register );
      return 0;
   }

   /* at this point I'll assume that the chip is an SMC9xxx.
    * It might be prudent to check a listing of MAC addresses
    * against the hardware address, or do some other tests. */
   return 1;
}


/**************************************************************************
 * ETH_TRANSMIT - Transmit a frame
 ***************************************************************************/
static void smc9000_transmit(
	struct nic *nic,
	const char *d,			/* Destination */
	unsigned int t,			/* Type */
	unsigned int s,			/* size */
	const char *p)			/* Packet */
{
   word length; /* real, length incl. header */
   word numPages;
   unsigned long time_out;
   byte	packet_no;
   word status;
   int i;

   /* We dont pad here since we can have the hardware doing it for us */
   length = (s + ETH_HLEN + 1)&~1;

   /* convert to MMU pages */
   numPages = length / 256;

   if (numPages > 7 ) {
      DBG("SMC9000: Far too big packet error. \n");
      return;
   }

   /* dont try more than, say 30 times */
   for (i=0;i<30;i++) {
      /* now, try to allocate the memory */
      SMC_SELECT_BANK(nic->ioaddr, 2);
      _outw(MC_ALLOC | numPages, nic->ioaddr + MMU_CMD);

      status = 0;
      /* wait for the memory allocation to finnish */
      for (time_out = currticks() + 5*TICKS_PER_SEC; currticks() < time_out; ) {
	 status = inb(nic->ioaddr + INTERRUPT);
	 if ( status & IM_ALLOC_INT ) {
	    /* acknowledge the interrupt */
	    _outb(IM_ALLOC_INT, nic->ioaddr + INTERRUPT);
	    break;
	 }
      }

      if ((status & IM_ALLOC_INT) != 0 ) {
	 /* We've got the memory */
	 break;
      } else {
	 printf("SMC9000: Memory allocation timed out, resetting MMU.\n");
	 _outw(MC_RESET, nic->ioaddr + MMU_CMD);
      }
   }

   /* If I get here, I _know_ there is a packet slot waiting for me */
   packet_no = inb(nic->ioaddr + PNR_ARR + 1);
   if (packet_no & 0x80) {
      /* or isn't there?  BAD CHIP! */
      printf("SMC9000: Memory allocation failed. \n");
      return;
   }

   /* we have a packet address, so tell the card to use it */
   _outb(packet_no, nic->ioaddr + PNR_ARR);

   /* point to the beginning of the packet */
   _outw(PTR_AUTOINC, nic->ioaddr + POINTER);

#if	SMC9000_DEBUG > 2
   printf("Trying to xmit packet of length %hX\n", length );
#endif

   /* send the packet length ( +6 for status, length and ctl byte )
    * and the status word ( set to zeros ) */
   _outw(0, nic->ioaddr + DATA_1 );

   /* send the packet length ( +6 for status words, length, and ctl) */
   _outb((length+6) & 0xFF,  nic->ioaddr + DATA_1);
   _outb((length+6) >> 8 ,   nic->ioaddr + DATA_1);

   /* Write the contents of the packet */

   /* The ethernet header first... */
   outsw(nic->ioaddr + DATA_1, d, ETH_ALEN >> 1);
   outsw(nic->ioaddr + DATA_1, nic->node_addr, ETH_ALEN >> 1);
   _outw(htons(t), nic->ioaddr + DATA_1);

   /* ... the data ... */
   outsw(nic->ioaddr + DATA_1 , p, s >> 1);

   /* ... and the last byte, if there is one.   */
   if ((s & 1) == 0) {
      _outw(0, nic->ioaddr + DATA_1);
   } else {
      _outb(p[s-1], nic->ioaddr + DATA_1);
      _outb(0x20, nic->ioaddr + DATA_1);
   }

   /* and let the chipset deal with it */
   _outw(MC_ENQUEUE , nic->ioaddr + MMU_CMD);

   status = 0; time_out = currticks() + 5*TICKS_PER_SEC;
   do {
      status = inb(nic->ioaddr + INTERRUPT);

      if ((status & IM_TX_INT ) != 0) {
	 word tx_status;

	 /* ack interrupt */
	 _outb(IM_TX_INT, nic->ioaddr + INTERRUPT);

	 packet_no = inw(nic->ioaddr + FIFO_PORTS);
	 packet_no &= 0x7F;

	 /* select this as the packet to read from */
	 _outb( packet_no, nic->ioaddr + PNR_ARR );

	 /* read the first word from this packet */
	 _outw( PTR_AUTOINC | PTR_READ, nic->ioaddr + POINTER );

	 tx_status = inw( nic->ioaddr + DATA_1 );

	 if (0 == (tx_status & TS_SUCCESS)) {
	    DBG("SMC9000: TX FAIL STATUS: %hX \n", tx_status);
	    /* re-enable transmit */
	    SMC_SELECT_BANK(nic->ioaddr, 0);
	    _outw(inw(nic->ioaddr + TCR ) | TCR_ENABLE, nic->ioaddr + TCR );
	 }

	 /* kill the packet */
	 SMC_SELECT_BANK(nic->ioaddr, 2);
	 _outw(MC_FREEPKT, nic->ioaddr + MMU_CMD);

	 return;
      }
   }while(currticks() < time_out);

   printf("SMC9000: TX timed out, resetting board\n");
   smc_reset(nic->ioaddr);
   return;
}

/**************************************************************************
 * ETH_POLL - Wait for a frame
 ***************************************************************************/
static int smc9000_poll(struct nic *nic, int retrieve)
{
   SMC_SELECT_BANK(nic->ioaddr, 2);
   if (inw(nic->ioaddr + FIFO_PORTS) & FP_RXEMPTY)
     return 0;
   
   if ( ! retrieve ) return 1;

   /*  start reading from the start of the packet */
   _outw(PTR_READ | PTR_RCV | PTR_AUTOINC, nic->ioaddr + POINTER);

   /* First read the status and check that we're ok */
   if (!(inw(nic->ioaddr + DATA_1) & RS_ERRORS)) {
      /* Next: read the packet length and mask off the top bits */
      nic->packetlen = (inw(nic->ioaddr + DATA_1) & 0x07ff);

      /* the packet length includes the 3 extra words */
      nic->packetlen -= 6;
#if	SMC9000_DEBUG > 2
      printf(" Reading %d words (and %d byte(s))\n",
	       (nic->packetlen >> 1), nic->packetlen & 1);
#endif
      /* read the packet (and the last "extra" word) */
      insw(nic->ioaddr + DATA_1, nic->packet, (nic->packetlen+2) >> 1);
      /* is there an odd last byte ? */
      if (nic->packet[nic->packetlen+1] & 0x20)
	 nic->packetlen++;

      /*  error or good, tell the card to get rid of this packet */
      _outw(MC_RELEASE, nic->ioaddr + MMU_CMD);
      return 1;
   }

   printf("SMC9000: RX error\n");
   /*  error or good, tell the card to get rid of this packet */
   _outw(MC_RELEASE, nic->ioaddr + MMU_CMD);
   return 0;
}

static void smc9000_disable ( struct nic *nic, struct isa_device *isa __unused ) {
   nic_disable ( nic );
   smc_reset(nic->ioaddr);

   /* no more interrupts for me */
   SMC_SELECT_BANK(nic->ioaddr, 2);
   _outb( 0, nic->ioaddr + INT_MASK);

   /* and tell the card to stay away from that nasty outside world */
   SMC_SELECT_BANK(nic->ioaddr, 0);
   _outb( RCR_CLEAR, nic->ioaddr + RCR );
   _outb( TCR_CLEAR, nic->ioaddr + TCR );
}

static void smc9000_irq(struct nic *nic __unused, irq_action_t action __unused)
{
  switch ( action ) {
  case DISABLE :
    break;
  case ENABLE :
    break;
  case FORCE :
    break;
  }
}

static struct nic_operations smc9000_operations = {
	.connect	= dummy_connect,
	.poll		= smc9000_poll,
	.transmit	= smc9000_transmit,
	.irq		= smc9000_irq,

};

/**************************************************************************
 * ETH_PROBE - Look for an adapter
 ***************************************************************************/

static int smc9000_probe ( struct nic *nic, struct isa_device *isa ) {

   unsigned short   revision;
   int	            memory;
   int              media;
   const char *	    version_string;
   const char *	    if_string;
   int              i;

   nic->irqno  = 0;
   isa_fill_nic ( nic, isa );
   nic->ioaddr = isa->ioaddr;

   /*
    * Get the MAC address ( bank 1, regs 4 - 9 )
    */
   SMC_SELECT_BANK(nic->ioaddr, 1);
   for ( i = 0; i < 6; i += 2 ) {
      word address;

      address = inw(nic->ioaddr + ADDR0 + i);
      nic->node_addr[i+1] = address >> 8;
      nic->node_addr[i] = address & 0xFF;
   }

   /* get the memory information */
   SMC_SELECT_BANK(nic->ioaddr, 0);
   memory = ( inw(nic->ioaddr + MCR) >> 9 )  & 0x7;  /* multiplier */
   memory *= 256 * (inw(nic->ioaddr + MIR) & 0xFF);

   /*
    * Now, I want to find out more about the chip.  This is sort of
    * redundant, but it's cleaner to have it in both, rather than having
    * one VERY long probe procedure.
    */
   SMC_SELECT_BANK(nic->ioaddr, 3);
   revision  = inw(nic->ioaddr + REVISION);
   version_string = chip_ids[(revision >> 4) & 0xF];

   if (((revision & 0xF0) >> 4 == CHIP_9196) &&
       ((revision & 0x0F) >= REV_9196)) {
      /* This is a 91c96. 'c96 has the same chip id as 'c94 (4) but
       * a revision starting at 6 */
      version_string = smc91c96_id;
   }

   if ( !version_string ) {
      /* I shouldn't get here because this call was done before.... */
      return 0;
   }

   /* is it using AUI or 10BaseT ? */
   SMC_SELECT_BANK(nic->ioaddr, 1);
   if (inw(nic->ioaddr + CONFIG) & CFG_AUI_SELECT)
     media = 2;
   else
     media = 1;

   if_string = interfaces[media - 1];

   /* now, reset the chip, and put it into a known state */
   smc_reset(nic->ioaddr);

   printf("SMC9000 %s\n", smc9000_version);
   DBG("Copyright (C) 1998 Daniel Engstr\x94m\n");
   DBG("Copyright (C) 1996 Eric Stahlman\n");

   printf("%s rev:%d I/O port:%hX Interface:%s RAM:%d bytes \n",
	  version_string, revision & 0xF,
	  nic->ioaddr, if_string, memory );
   /*
    * Print the Ethernet address
    */
   printf("Ethernet MAC address: %!\n", nic->node_addr);

   SMC_SELECT_BANK(nic->ioaddr, 0);

   /* see the header file for options in TCR/RCR NORMAL*/
   _outw(TCR_NORMAL, nic->ioaddr + TCR);
   _outw(RCR_NORMAL, nic->ioaddr + RCR);

   /* Select which interface to use */
   SMC_SELECT_BANK(nic->ioaddr, 1);
   if ( media == 1 ) {
      _outw( inw( nic->ioaddr + CONFIG ) & ~CFG_AUI_SELECT,
	   nic->ioaddr + CONFIG );
   }
   else if ( media == 2 ) {
      _outw( inw( nic->ioaddr + CONFIG ) | CFG_AUI_SELECT,
	   nic->ioaddr + CONFIG );
   }

   nic->nic_op	= &smc9000_operations;
   return 1;
}

/*
 * The SMC9000 can be at any of the following port addresses.  To
 * change for a slightly different card, you can add it to the array.
 *
 */
static isa_probe_addr_t smc9000_probe_addrs[] = {
   0x200, 0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x2E0,
   0x300, 0x320, 0x340, 0x360, 0x380, 0x3A0, 0x3C0, 0x3E0,
};

static struct isa_driver smc9000_driver =
	ISA_DRIVER ( smc9000_probe_addrs, smc9000_probe_addr,
		     GENERIC_ISAPNP_VENDOR, 0x8228 );

DRIVER ( "SMC9000", nic_driver, isa_driver, smc9000_driver,
	 smc9000_probe, smc9000_disable );

ISA_ROM ( "smc9000", "SMC9000" );

/*
 * Local variables:
 *  c-basic-offset: 3
 * End:
 */
