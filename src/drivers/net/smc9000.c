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
#define SMC9000_VERBOSE  1
#define SMC9000_DEBUG    0

#include "etherboot.h"
#include "nic.h"
#include "isa.h"
#include "smc9000.h"

# define _outb outb
# define _outw outw

static const char       smc9000_version[] = "Version 0.99 98-09-30";
static unsigned int	smc9000_base=0;
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
 * Function: smc_probe( int ioaddr )
 *
 * Purpose:
 *	Tests to see if a given ioaddr points to an SMC9xxx chip.
 *	Returns a 0 on success
 *
 * Algorithm:
 *	(1) see if the high byte of BANK_SELECT is 0x33
 *	(2) compare the ioaddr with the base register's address
 *	(3) see if I recognize the chip ID in the appropriate register
 *
 * ---------------------------------------------------------------------
 */
static int smc_probe( int ioaddr )
{
   word bank;
   word	revision_register;
   word base_address_register;

   /* First, see if the high byte is 0x33 */
   bank = inw(ioaddr + BANK_SELECT);
   if ((bank & 0xFF00) != 0x3300) {
      return -1;
   }
   /* The above MIGHT indicate a device, but I need to write to further
    *	test this.  */
   _outw(0x0, ioaddr + BANK_SELECT);
   bank = inw(ioaddr + BANK_SELECT);
   if ((bank & 0xFF00) != 0x3300) {
      return -1;
   }

   /* well, we've already written once, so hopefully another time won't
    *  hurt.  This time, I need to switch the bank register to bank 1,
    *  so I can access the base address register */
   SMC_SELECT_BANK(ioaddr, 1);
   base_address_register = inw(ioaddr + BASE);

   if (ioaddr != (base_address_register >> 3 & 0x3E0))  {
#ifdef	SMC9000_VERBOSE
      printf("SMC9000: IOADDR %hX doesn't match configuration (%hX)."
	     "Probably not a SMC chip\n",
	     ioaddr, base_address_register >> 3 & 0x3E0);
#endif
      /* well, the base address register didn't match.  Must not have
       * been a SMC chip after all. */
      return -1;
   }


   /* check if the revision register is something that I recognize.
    * These might need to be added to later, as future revisions
    * could be added.  */
   SMC_SELECT_BANK(ioaddr, 3);
   revision_register  = inw(ioaddr + REVISION);
   if (!chip_ids[(revision_register >> 4) & 0xF]) {
      /* I don't recognize this chip, so... */
#ifdef	SMC9000_VERBOSE
      printf("SMC9000: IO %hX: Unrecognized revision register:"
	     " %hX, Contact author.\n", ioaddr, revision_register);
#endif
      return -1;
   }

   /* at this point I'll assume that the chip is an SMC9xxx.
    * It might be prudent to check a listing of MAC addresses
    * against the hardware address, or do some other tests. */
   return 0;
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
#ifdef	SMC9000_VERBOSE
      printf("SMC9000: Far too big packet error. \n");
#endif
      return;
   }

   /* dont try more than, say 30 times */
   for (i=0;i<30;i++) {
      /* now, try to allocate the memory */
      SMC_SELECT_BANK(smc9000_base, 2);
      _outw(MC_ALLOC | numPages, smc9000_base + MMU_CMD);

      status = 0;
      /* wait for the memory allocation to finnish */
      for (time_out = currticks() + 5*TICKS_PER_SEC; currticks() < time_out; ) {
	 status = inb(smc9000_base + INTERRUPT);
	 if ( status & IM_ALLOC_INT ) {
	    /* acknowledge the interrupt */
	    _outb(IM_ALLOC_INT, smc9000_base + INTERRUPT);
	    break;
	 }
      }

      if ((status & IM_ALLOC_INT) != 0 ) {
	 /* We've got the memory */
	 break;
      } else {
	 printf("SMC9000: Memory allocation timed out, resetting MMU.\n");
	 _outw(MC_RESET, smc9000_base + MMU_CMD);
      }
   }

   /* If I get here, I _know_ there is a packet slot waiting for me */
   packet_no = inb(smc9000_base + PNR_ARR + 1);
   if (packet_no & 0x80) {
      /* or isn't there?  BAD CHIP! */
      printf("SMC9000: Memory allocation failed. \n");
      return;
   }

   /* we have a packet address, so tell the card to use it */
   _outb(packet_no, smc9000_base + PNR_ARR);

   /* point to the beginning of the packet */
   _outw(PTR_AUTOINC, smc9000_base + POINTER);

#if	SMC9000_DEBUG > 2
   printf("Trying to xmit packet of length %hX\n", length );
#endif

   /* send the packet length ( +6 for status, length and ctl byte )
    * and the status word ( set to zeros ) */
   _outw(0, smc9000_base + DATA_1 );

   /* send the packet length ( +6 for status words, length, and ctl) */
   _outb((length+6) & 0xFF,  smc9000_base + DATA_1);
   _outb((length+6) >> 8 ,   smc9000_base + DATA_1);

   /* Write the contents of the packet */

   /* The ethernet header first... */
   outsw(smc9000_base + DATA_1, d, ETH_ALEN >> 1);
   outsw(smc9000_base + DATA_1, nic->node_addr, ETH_ALEN >> 1);
   _outw(htons(t), smc9000_base + DATA_1);

   /* ... the data ... */
   outsw(smc9000_base + DATA_1 , p, s >> 1);

   /* ... and the last byte, if there is one.   */
   if ((s & 1) == 0) {
      _outw(0, smc9000_base + DATA_1);
   } else {
      _outb(p[s-1], smc9000_base + DATA_1);
      _outb(0x20, smc9000_base + DATA_1);
   }

   /* and let the chipset deal with it */
   _outw(MC_ENQUEUE , smc9000_base + MMU_CMD);

   status = 0; time_out = currticks() + 5*TICKS_PER_SEC;
   do {
      status = inb(smc9000_base + INTERRUPT);

      if ((status & IM_TX_INT ) != 0) {
	 word tx_status;

	 /* ack interrupt */
	 _outb(IM_TX_INT, smc9000_base + INTERRUPT);

	 packet_no = inw(smc9000_base + FIFO_PORTS);
	 packet_no &= 0x7F;

	 /* select this as the packet to read from */
	 _outb( packet_no, smc9000_base + PNR_ARR );

	 /* read the first word from this packet */
	 _outw( PTR_AUTOINC | PTR_READ, smc9000_base + POINTER );

	 tx_status = inw( smc9000_base + DATA_1 );

	 if (0 == (tx_status & TS_SUCCESS)) {
#ifdef	SMC9000_VERBOSE
	    printf("SMC9000: TX FAIL STATUS: %hX \n", tx_status);
#endif
	    /* re-enable transmit */
	    SMC_SELECT_BANK(smc9000_base, 0);
	    _outw(inw(smc9000_base + TCR ) | TCR_ENABLE, smc9000_base + TCR );
	 }

	 /* kill the packet */
	 SMC_SELECT_BANK(smc9000_base, 2);
	 _outw(MC_FREEPKT, smc9000_base + MMU_CMD);

	 return;
      }
   }while(currticks() < time_out);

   printf("SMC9000: Waring TX timed out, resetting board\n");
   smc_reset(smc9000_base);
   return;
}

/**************************************************************************
 * ETH_POLL - Wait for a frame
 ***************************************************************************/
static int smc9000_poll(struct nic *nic, int retrieve)
{
   if(!smc9000_base)
     return 0;

   SMC_SELECT_BANK(smc9000_base, 2);
   if (inw(smc9000_base + FIFO_PORTS) & FP_RXEMPTY)
     return 0;
   
   if ( ! retrieve ) return 1;

   /*  start reading from the start of the packet */
   _outw(PTR_READ | PTR_RCV | PTR_AUTOINC, smc9000_base + POINTER);

   /* First read the status and check that we're ok */
   if (!(inw(smc9000_base + DATA_1) & RS_ERRORS)) {
      /* Next: read the packet length and mask off the top bits */
      nic->packetlen = (inw(smc9000_base + DATA_1) & 0x07ff);

      /* the packet length includes the 3 extra words */
      nic->packetlen -= 6;
#if	SMC9000_DEBUG > 2
      printf(" Reading %d words (and %d byte(s))\n",
	       (nic->packetlen >> 1), nic->packetlen & 1);
#endif
      /* read the packet (and the last "extra" word) */
      insw(smc9000_base + DATA_1, nic->packet, (nic->packetlen+2) >> 1);
      /* is there an odd last byte ? */
      if (nic->packet[nic->packetlen+1] & 0x20)
	 nic->packetlen++;

      /*  error or good, tell the card to get rid of this packet */
      _outw(MC_RELEASE, smc9000_base + MMU_CMD);
      return 1;
   }

   printf("SMC9000: RX error\n");
   /*  error or good, tell the card to get rid of this packet */
   _outw(MC_RELEASE, smc9000_base + MMU_CMD);
   return 0;
}

static void smc9000_disable(struct dev *dev __unused)
{
   if(!smc9000_base)
     return;

   smc_reset(smc9000_base);

   /* no more interrupts for me */
   SMC_SELECT_BANK(smc9000_base, 2);
   _outb( 0, smc9000_base + INT_MASK);

   /* and tell the card to stay away from that nasty outside world */
   SMC_SELECT_BANK(smc9000_base, 0);
   _outb( RCR_CLEAR, smc9000_base + RCR );
   _outb( TCR_CLEAR, smc9000_base + TCR );
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

/**************************************************************************
 * ETH_PROBE - Look for an adapter
 ***************************************************************************/

static int smc9000_probe(struct dev *dev, unsigned short *probe_addrs)
{
   struct nic *nic = (struct nic *)dev;
   unsigned short   revision;
   int	            memory;
   int              media;
   const char *	    version_string;
   const char *	    if_string;
   int              i;

   /*
    * the SMC9000 can be at any of the following port addresses.  To change,
    * for a slightly different card, you can add it to the array.  Keep in
    * mind that the array must end in zero.
    */
   static unsigned short portlist[] = {
#ifdef	SMC9000_SCAN
      SMC9000_SCAN,
#else
      0x200, 0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x2E0,
      0x300, 0x320, 0x340, 0x360, 0x380, 0x3A0, 0x3C0, 0x3E0,
#endif
      0 };

   /* if no addresses supplied, fall back on defaults */
   if (probe_addrs == 0 || probe_addrs[0] == 0)
     probe_addrs = portlist;

   /* check every ethernet address */
   for (i = 0; probe_addrs[i]; i++) {
      /* check this specific address */
      if (smc_probe(probe_addrs[i]) == 0)
	smc9000_base = probe_addrs[i];
   }

   /* couldn't find anything */
   if(0 == smc9000_base)
     goto out;

   nic->irqno  = 0;
   nic->ioaddr = smc9000_base;

   /*
    * Get the MAC address ( bank 1, regs 4 - 9 )
    */
   SMC_SELECT_BANK(smc9000_base, 1);
   for ( i = 0; i < 6; i += 2 ) {
      word address;

      address = inw(smc9000_base + ADDR0 + i);
      nic->node_addr[i+1] = address >> 8;
      nic->node_addr[i] = address & 0xFF;
   }


   /* get the memory information */
   SMC_SELECT_BANK(smc9000_base, 0);
   memory = ( inw(smc9000_base + MCR) >> 9 )  & 0x7;  /* multiplier */
   memory *= 256 * (inw(smc9000_base + MIR) & 0xFF);

   /*
    * Now, I want to find out more about the chip.  This is sort of
    * redundant, but it's cleaner to have it in both, rather than having
    * one VERY long probe procedure.
    */
   SMC_SELECT_BANK(smc9000_base, 3);
   revision  = inw(smc9000_base + REVISION);
   version_string = chip_ids[(revision >> 4) & 0xF];

   if (((revision & 0xF0) >> 4 == CHIP_9196) &&
       ((revision & 0x0F) >= REV_9196)) {
      /* This is a 91c96. 'c96 has the same chip id as 'c94 (4) but
       * a revision starting at 6 */
      version_string = smc91c96_id;
   }

   if ( !version_string ) {
      /* I shouldn't get here because this call was done before.... */
      goto out;
   }

   /* is it using AUI or 10BaseT ? */
   SMC_SELECT_BANK(smc9000_base, 1);
   if (inw(smc9000_base + CONFIG) & CFG_AUI_SELECT)
     media = 2;
   else
     media = 1;

   if_string = interfaces[media - 1];

   /* now, reset the chip, and put it into a known state */
   smc_reset(smc9000_base);

   printf("SMC9000 %s\n", smc9000_version);
#ifdef	SMC9000_VERBOSE
   printf("Copyright (C) 1998 Daniel Engstr\x94m\n");
   printf("Copyright (C) 1996 Eric Stahlman\n");
#endif

   printf("%s rev:%d I/O port:%hX Interface:%s RAM:%d bytes \n",
	  version_string, revision & 0xF,
	  smc9000_base, if_string, memory );
   /*
    * Print the Ethernet address
    */
   printf("Ethernet MAC address: %!\n", nic->node_addr);

   SMC_SELECT_BANK(smc9000_base, 0);

   /* see the header file for options in TCR/RCR NORMAL*/
   _outw(TCR_NORMAL, smc9000_base + TCR);
   _outw(RCR_NORMAL, smc9000_base + RCR);

   /* Select which interface to use */
   SMC_SELECT_BANK(smc9000_base, 1);
   if ( media == 1 ) {
      _outw( inw( smc9000_base + CONFIG ) & ~CFG_AUI_SELECT,
	   smc9000_base + CONFIG );
   }
   else if ( media == 2 ) {
      _outw( inw( smc9000_base + CONFIG ) | CFG_AUI_SELECT,
	   smc9000_base + CONFIG );
   }

   dev->disable  = smc9000_disable;
   nic->poll     = smc9000_poll;
   nic->transmit = smc9000_transmit;
   nic->irq      = smc9000_irq;

   /* Based on PnP ISA map */
   dev->devid.vendor_id = htons(GENERIC_ISAPNP_VENDOR);
   dev->devid.device_id = htons(0x8228);

   return 1;

out:
#ifdef	SMC9000_VERBOSE
   /* printf("No SMC9000 adapters found\n"); */
#endif
   smc9000_base = 0;

   return (0);
}

static struct isa_driver smc9000_driver __isa_driver = {
	.type    = NIC_DRIVER,
	.name    = "SMC9000",
	.probe   = smc9000_probe,
	.ioaddrs = 0,
};
