/* Uses lance chip, not fixed for relocation */
#ifdef ALLMULTI
#error multicast support is not yet implemented
#endif
/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
Schneider & Koch G16 NIC driver for Etherboot
heavily based on SK G16 driver from Linux 2.0.36
Changes to make it work with Etherboot by Georg Baum <Georg.Baum@gmx.de>
***************************************************************************/

/*-
 * Copyright (C) 1994 by PJD Weichmann & SWS Bern, Switzerland
 *
 * This software may be used and distributed according to the terms
 * of the GNU Public License, incorporated herein by reference.
 *
 * Module         : sk_g16.c
 *
 * Version        : $Revision$
 *
 * Author         : Patrick J.D. Weichmann
 *
 * Date Created   : 94/05/26
 * Last Updated   : $Date$
 *
 * Description    : Schneider & Koch G16 Ethernet Device Driver for
 *                  Linux Kernel >= 1.1.22
 * Update History :
 *
-*/

/*
 * The Schneider & Koch (SK) G16 Network device driver is based
 * on the 'ni6510' driver from Michael Hipp which can be found at
 * ftp://sunsite.unc.edu/pub/Linux/system/Network/drivers/nidrivers.tar.gz
 *
 * Sources: 1) ni6510.c by M. Hipp
 *          2) depca.c  by D.C. Davies
 *          3) skeleton.c by D. Becker
 *          4) Am7990 Local Area Network Controller for Ethernet (LANCE),
 *             AMD, Pub. #05698, June 1989
 *
 * Many Thanks for helping me to get things working to:
 *
 *                 A. Cox (A.Cox@swansea.ac.uk)
 *                 M. Hipp (mhipp@student.uni-tuebingen.de)
 *                 R. Bolz (Schneider & Koch, Germany)
 *
 * See README.sk_g16 for details about limitations and bugs for the
 * current version.
 *
 * To Do:
 *        - Support of SK_G8 and other SK Network Cards.
 *        - Autoset memory mapped RAM. Check for free memory and then
 *          configure RAM correctly.
 *        - SK_close should really set card in to initial state.
 *        - Test if IRQ 3 is not switched off. Use autoirq() functionality.
 *          (as in /drivers/net/skeleton.c)
 *        - Implement Multicast addressing. At minimum something like
 *          in depca.c.
 *        - Redo the statistics part.
 *        - Try to find out if the board is in 8 Bit or 16 Bit slot.
 *          If in 8 Bit mode don't use IRQ 11.
 *        - (Try to make it slightly faster.)
 */

/* to get some global routines like printf */
#include "etherboot.h"
/* to get the interface to the body of the program */
#include "nic.h"
#include "isa.h"

/* From linux/if_ether.h: */
#define ETH_ZLEN	60		/* Min. octets in frame sans FCS */

#include "sk_g16.h"

/*
 * Schneider & Koch Card Definitions
 * =================================
 */

#define SK_NAME   "SK_G16"

/*
 * SK_G16 Configuration
 * --------------------
 */

/*
 * Abbreviations
 * -------------
 *
 * RAM - used for the 16KB shared memory
 * Boot_ROM, ROM - are used for referencing the BootEPROM
 *
 * SK_ADDR is a symbolic constant used to configure
 * the behaviour of the driver and the SK_G16.
 *
 * SK_ADDR defines the address where the RAM will be mapped into the real
 *         host memory.
 *         valid addresses are from 0xa0000 to 0xfc000 in 16Kbyte steps.
 */

#define SK_ADDR         0xcc000

/*
 * In POS3 are bits A14-A19 of the address bus. These bits can be set
 * to choose the RAM address. That's why we only can choose the RAM address
 * in 16KB steps.
 */

#define POS_ADDR       (rom_addr>>14)  /* Do not change this line */

/*
 * SK_G16 I/O PORT's + IRQ's + Boot_ROM locations
 * ----------------------------------------------
 */

/*
 * As nearly every card has also SK_G16 a specified I/O Port region and
 * only a few possible IRQ's.
 * In the Installation Guide from Schneider & Koch is listed a possible
 * Interrupt IRQ2. IRQ2 is always IRQ9 in boards with two cascaded interrupt
 * controllers. So we use in SK_IRQS IRQ9.
 */

/* Don't touch any of the following #defines. */

#define SK_IO_PORTS     { 0x100, 0x180, 0x208, 0x220, 0x288, 0x320, 0x328, 0x390, 0 }

/*
 * SK_G16 POS REGISTERS
 * --------------------
 */

/*
 * SK_G16 has a Programmable Option Select (POS) Register.
 * The POS is composed of 8 separate registers (POS0-7) which
 * are I/O mapped on an address set by the W1 switch.
 *
 */

#define SK_POS_SIZE 8           /* 8 I/O Ports are used by SK_G16 */

#define SK_POS0     ioaddr      /* Card-ID Low (R) */
#define SK_POS1     ioaddr+1    /* Card-ID High (R) */
#define SK_POS2     ioaddr+2    /* Card-Enable, Boot-ROM Disable (RW) */
#define SK_POS3     ioaddr+3    /* Base address of RAM */
#define SK_POS4     ioaddr+4    /* IRQ */

/* POS5 - POS7 are unused */

/*
 * SK_G16 MAC PREFIX
 * -----------------
 */

/*
 * Scheider & Koch manufacturer code (00:00:a5).
 * This must be checked, that we are sure it is a SK card.
 */

#define SK_MAC0         0x00
#define SK_MAC1         0x00
#define SK_MAC2         0x5a

/*
 * SK_G16 ID
 * ---------
 */

/*
 * If POS0,POS1 contain the following ID, then we know
 * at which I/O Port Address we are.
 */

#define SK_IDLOW  0xfd
#define SK_IDHIGH 0x6a


/*
 * LANCE POS Bit definitions
 * -------------------------
 */

#define SK_ROM_RAM_ON  (POS2_CARD)
#define SK_ROM_RAM_OFF (POS2_EPROM)
#define SK_ROM_ON      (inb(SK_POS2) & POS2_CARD)
#define SK_ROM_OFF     (inb(SK_POS2) | POS2_EPROM)
#define SK_RAM_ON      (inb(SK_POS2) | POS2_CARD)
#define SK_RAM_OFF     (inb(SK_POS2) & POS2_EPROM)

#define POS2_CARD  0x0001              /* 1 = SK_G16 on      0 = off */
#define POS2_EPROM 0x0002              /* 1 = Boot EPROM off 0 = on */

/*
 * SK_G16 Memory mapped Registers
 * ------------------------------
 *
 */

#define SK_IOREG        (board->ioreg) /* LANCE data registers.     */
#define SK_PORT         (board->port)  /* Control, Status register  */
#define SK_IOCOM        (board->iocom) /* I/O Command               */

/*
 * SK_G16 Status/Control Register bits
 * -----------------------------------
 *
 * (C) Controlreg (S) Statusreg
 */

/*
 * Register transfer: 0 = no transfer
 *                    1 = transferring data between LANCE and I/O reg
 */
#define SK_IORUN        0x20

/*
 * LANCE interrupt: 0 = LANCE interrupt occurred
 *                  1 = no LANCE interrupt occurred
 */
#define SK_IRQ          0x10

#define SK_RESET        0x08   /* Reset SK_CARD: 0 = RESET 1 = normal */
#define SK_RW           0x02   /* 0 = write to 1 = read from */
#define SK_ADR          0x01   /* 0 = REG DataPort 1 = RAP Reg addr port */


#define SK_RREG         SK_RW  /* Transferdirection to read from lance */
#define SK_WREG         0      /* Transferdirection to write to lance */
#define SK_RAP          SK_ADR /* Destination Register RAP */
#define SK_RDATA        0      /* Destination Register REG DataPort */

/*
 * SK_G16 I/O Command
 * ------------------
 */

/*
 * Any bitcombination sets the internal I/O bit (transfer will start)
 * when written to I/O Command
 */

#define SK_DOIO         0x80   /* Do Transfer */

/*
 * LANCE RAP (Register Address Port).
 * ---------------------------------
 */

/*
 * The LANCE internal registers are selected through the RAP.
 * The Registers are:
 *
 * CSR0 - Status and Control flags
 * CSR1 - Low order bits of initialize block (bits 15:00)
 * CSR2 - High order bits of initialize block (bits 07:00, 15:08 are reserved)
 * CSR3 - Allows redefinition of the Bus Master Interface.
 *        This register must be set to 0x0002, which means BSWAP = 0,
 *        ACON = 1, BCON = 0;
 *
 */

#define CSR0            0x00
#define CSR1            0x01
#define CSR2            0x02
#define CSR3            0x03

/*
 * General Definitions
 * ===================
 */

/*
 * Set the number of Tx and Rx buffers, using Log_2(# buffers).
 * We have 16KB RAM which can be accessed by the LANCE. In the
 * memory are not only the buffers but also the ring descriptors and
 * the initialize block.
 * Don't change anything unless you really know what you do.
 */

#define LC_LOG_TX_BUFFERS 1               /* (2 == 2^^1) 2 Transmit buffers */
#define LC_LOG_RX_BUFFERS 2               /* (8 == 2^^3) 8 Receive buffers */

/* Descriptor ring sizes */

#define TMDNUM (1 << (LC_LOG_TX_BUFFERS)) /* 2 Transmit descriptor rings */
#define RMDNUM (1 << (LC_LOG_RX_BUFFERS)) /* 8 Receive Buffers */

/* Define Mask for setting RMD, TMD length in the LANCE init_block */

#define TMDNUMMASK (LC_LOG_TX_BUFFERS << 29)
#define RMDNUMMASK (LC_LOG_RX_BUFFERS << 29)

/*
 * Data Buffer size is set to maximum packet length.
 */

#define PKT_BUF_SZ              1518

/*
 * The number of low I/O ports used by the ethercard.
 */

#define ETHERCARD_TOTAL_SIZE    SK_POS_SIZE

/*
 * Portreserve is there to mark the Card I/O Port region as used.
 * Check_region is to check if the region at ioaddr with the size "size"
 * is free or not.
 * Snarf_region allocates the I/O Port region.
 */

#ifndef	HAVE_PORTRESERVE

#define check_region(ioaddr1, size)              0
#define request_region(ioaddr1, size,name)       do ; while (0)

#endif

/*
 * SK_DEBUG
 *
 * Here you can choose what level of debugging wanted.
 *
 * If SK_DEBUG and SK_DEBUG2 are undefined, then only the
 *  necessary messages will be printed.
 *
 * If SK_DEBUG is defined, there will be many debugging prints
 *  which can help to find some mistakes in configuration or even
 *  in the driver code.
 *
 * If SK_DEBUG2 is defined, many many messages will be printed
 *  which normally you don't need. I used this to check the interrupt
 *  routine.
 *
 * (If you define only SK_DEBUG2 then only the messages for
 *  checking interrupts will be printed!)
 *
 * Normal way of live is:
 *
 * For the whole thing get going let both symbolic constants
 * undefined. If you face any problems and you know what's going
 * on (you know something about the card and you can interpret some
 * hex LANCE register output) then define SK_DEBUG
 *
 */

#undef  SK_DEBUG	/* debugging */
#undef  SK_DEBUG2	/* debugging with more verbose report */

#ifdef	SK_DEBUG
#define PRINTF(x) printf x
#else
#define PRINTF(x) /**/
#endif

#ifdef	SK_DEBUG2
#define PRINTF2(x) printf x
#else
#define PRINTF2(x) /**/
#endif

/*
 * SK_G16 RAM
 *
 * The components are memory mapped and can be set in a region from
 * 0x00000 through 0xfc000 in 16KB steps.
 *
 * The Network components are: dual ported RAM, Prom, I/O Reg, Status-,
 * Controlregister and I/O Command.
 *
 * dual ported RAM: This is the only memory region which the LANCE chip
 *      has access to. From the Lance it is addressed from 0x0000 to
 *      0x3fbf. The host accesses it normally.
 *
 * PROM: The PROM obtains the ETHERNET-MAC-Address. It is realised as a
 *       8-Bit PROM, this means only the 16 even addresses are used of the
 *       32 Byte Address region. Access to a odd address results in invalid
 *       data.
 *
 * LANCE I/O Reg: The I/O Reg is build of 4 single Registers, Low-Byte Write,
 *       Hi-Byte Write, Low-Byte Read, Hi-Byte Read.
 *       Transfer from or to the LANCE is always in 16Bit so Low and High
 *       registers are always relevant.
 *
 *       The Data from the Readregister is not the data in the Writeregister!!
 *
 * Port: Status- and Controlregister.
 *       Two different registers which share the same address, Status is
 *       read-only, Control is write-only.
 *
 * I/O Command:
 *       Any bitcombination written in here starts the transmission between
 *       Host and LANCE.
 */

typedef struct
{
	unsigned char  ram[0x3fc0];   /* 16KB dual ported ram */
	unsigned char  rom[0x0020];   /* 32Byte PROM containing 6Byte MAC */
	unsigned char  res1[0x0010];  /* reserved */
	unsigned volatile short ioreg;/* LANCE I/O Register */
	unsigned volatile char  port; /* Statusregister and Controlregister */
	unsigned char  iocom;         /* I/O Command Register */
} SK_RAM;

/* struct  */

/*
 * This is the structure for the dual ported ram. We
 * have exactly 16 320 Bytes. In here there must be:
 *
 *     - Initialize Block   (starting at a word boundary)
 *     - Receive and Transmit Descriptor Rings (quadword boundary)
 *     - Data Buffers (arbitrary boundary)
 *
 * This is because LANCE has on SK_G16 only access to the dual ported
 * RAM and nowhere else.
 */

struct SK_ram
{
    struct init_block ib;
    struct tmd tmde[TMDNUM];
    struct rmd rmde[RMDNUM];
    char tmdbuf[TMDNUM][PKT_BUF_SZ];
    char rmdbuf[RMDNUM][PKT_BUF_SZ];
};

/*
 * Structure where all necessary information is for ring buffer
 * management and statistics.
 */

struct priv
{
    struct SK_ram *ram;  /* dual ported ram structure */
    struct rmd *rmdhead; /* start of receive ring descriptors */
    struct tmd *tmdhead; /* start of transmit ring descriptors */
    int        rmdnum;   /* actual used ring descriptor */
    int        tmdnum;   /* actual transmit descriptor for transmitting data */
    int        tmdlast;  /* last sent descriptor used for error handling, etc */
    void       *rmdbufs[RMDNUM]; /* pointer to the receive buffers */
    void       *tmdbufs[TMDNUM]; /* pointer to the transmit buffers */
};

/* global variable declaration */

/* static variables */

static SK_RAM *board;  /* pointer to our memory mapped board components */
static unsigned short	ioaddr; /* base io address */
static struct priv	p_data;

/* Macros */


/* Function Prototypes */

/*
 * Device Driver functions
 * -----------------------
 * See for short explanation of each function its definitions header.
 */

static int   SK_probe1(struct nic *nic, short ioaddr1);

static int SK_poll(struct nic *nic, int retrieve);
static void SK_transmit(
struct nic *nic,
const char *d,			/* Destination */
unsigned int t,			/* Type */
unsigned int s,			/* size */
const char *p);			/* Packet */
static void SK_disable(struct dev *dev);
static int  SK_probe(struct dev *dev, unsigned short *probe_addrs);

/*
 * LANCE Functions
 * ---------------
 */

static int SK_lance_init(struct nic *nic, unsigned short mode);
static void SK_reset_board(void);
static void SK_set_RAP(int reg_number);
static int SK_read_reg(int reg_number);
static int SK_rread_reg(void);
static void SK_write_reg(int reg_number, int value);

/*
 * Debugging functions
 * -------------------
 */

#ifdef	SK_DEBUG
static void SK_print_pos(struct nic *nic, char *text);
static void SK_print_ram(struct nic *nic);
#endif


/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int SK_poll(struct nic *nic, int retrieve)
{
	/* return true if there's an ethernet packet ready to read */
	struct priv *p;         /* SK_G16 private structure */
	struct rmd *rmdp;
	int csr0, rmdstat, packet_there;
    PRINTF2(("## %s: At beginning of SK_poll(). CSR0: %#hX\n",
           SK_NAME, SK_read_reg(CSR0)));

	p = nic->priv_data;
    csr0 = SK_read_reg(CSR0);      /* store register for checking */

    rmdp = p->rmdhead + p->rmdnum;
    packet_there = 0;

    if ( !(rmdp->u.s.status & RX_OWN) && !retrieve ) return 1;
      
    /*
     * Acknowledge all of the current interrupt sources, disable
     * Interrupts (INEA = 0)
     */

    SK_write_reg(CSR0, csr0 & CSR0_CLRALL);

    if (csr0 & CSR0_ERR) /* LANCE Error */
    {
	printf("%s: error: %#hX", SK_NAME, csr0);

        if (csr0 & CSR0_MISS)      /* No place to store packet ? */
        {
		printf(", Packet dropped.");
        }
	putchar('\n');
    }

    /* As long as we own the next entry, check status and send
     * it up to higher layer
     */

    while (!( (rmdstat = rmdp->u.s.status) & RX_OWN))
    {
	/*
         * Start and end of packet must be set, because we use
	 * the ethernet maximum packet length (1518) as buffer size.
	 *
	 * Because our buffers are at maximum OFLO and BUFF errors are
	 * not to be concerned (see Data sheet)
	 */

	if ((rmdstat & (RX_STP | RX_ENP)) != (RX_STP | RX_ENP))
	{
	    /* Start of a frame > 1518 Bytes ? */

	    if (rmdstat & RX_STP)
	    {
		printf("%s: packet too long\n", SK_NAME);
	    }

	    /*
             * All other packets will be ignored until a new frame with
	     * start (RX_STP) set follows.
	     *
	     * What we do is just give descriptor free for new incoming
	     * packets.
	     */

	    rmdp->u.s.status = RX_OWN;      /* Relinquish ownership to LANCE */

	}
	else if (rmdstat & RX_ERR)          /* Receive Error ? */
	{
	    printf("%s: RX error: %#hX\n", SK_NAME, (int) rmdstat);
	    rmdp->u.s.status = RX_OWN;      /* Relinquish ownership to LANCE */
	}
	else /* We have a packet which can be queued for the upper layers */
	{

	    int len = (rmdp->mlen & 0x0fff);  /* extract message length from receive buffer */

	    /*
             * Copy data out of our receive descriptor into nic->packet.
	     *
	     * (rmdp->u.buffer & 0x00ffffff) -> get address of buffer and
	     * ignore status fields)
	     */

	    memcpy(nic->packet, (unsigned char *) (rmdp->u.buffer & 0x00ffffff), nic->packetlen = len);
	    packet_there = 1;


	    /*
             * Packet is queued and marked for processing so we
	     * free our descriptor
	     */

	    rmdp->u.s.status = RX_OWN;

	    p->rmdnum++;
	    p->rmdnum %= RMDNUM;

	    rmdp = p->rmdhead + p->rmdnum;
	}
    }
    SK_write_reg(CSR0, CSR0_INEA); /* Enable Interrupts */
	return (packet_there);
}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void SK_transmit(
struct nic *nic,
const char *d,			/* Destination */
unsigned int t,			/* Type */
unsigned int s,			/* size */
const char *pack)		/* Packet */
{
	/* send the packet to destination */
    struct priv *p;         /* SK_G16 private structure */
    struct tmd *tmdp;
    short len;
    int csr0, tmdstat;

    PRINTF2(("## %s: At beginning of SK_transmit(). CSR0: %#hX\n",
           SK_NAME, SK_read_reg(CSR0)));
	p = nic->priv_data;
	tmdp = p->tmdhead + p->tmdnum; /* Which descriptor for transmitting */

	/* Copy data into dual ported ram */

	memcpy(&p->ram->tmdbuf[p->tmdnum][0], d, ETH_ALEN);	/* dst */
	memcpy(&p->ram->tmdbuf[p->tmdnum][ETH_ALEN], nic->node_addr, ETH_ALEN); /* src */
	p->ram->tmdbuf[p->tmdnum][ETH_ALEN + ETH_ALEN] = t >> 8;	/* type */
	p->ram->tmdbuf[p->tmdnum][ETH_ALEN + ETH_ALEN + 1] = t;	/* type */
	memcpy(&p->ram->tmdbuf[p->tmdnum][ETH_HLEN], pack, s);
	s += ETH_HLEN;
	while (s < ETH_ZLEN)	/* pad to min length */
		p->ram->tmdbuf[p->tmdnum][s++] = 0;
	p->ram->tmde[p->tmdnum].status2 = 0x0;

	/* Evaluate Packet length */
	len = ETH_ZLEN < s ? s : ETH_ZLEN;

	/* Fill in Transmit Message Descriptor */

	tmdp->blen = -len;            /* set length to transmit */

	/*
	 * Packet start and end is always set because we use the maximum
	 * packet length as buffer length.
	 * Relinquish ownership to LANCE
	 */

	tmdp->u.s.status = TX_OWN | TX_STP | TX_ENP;

	/* Start Demand Transmission */
	SK_write_reg(CSR0, CSR0_TDMD | CSR0_INEA);

    csr0 = SK_read_reg(CSR0);      /* store register for checking */

    /*
     * Acknowledge all of the current interrupt sources, disable
     * Interrupts (INEA = 0)
     */

    SK_write_reg(CSR0, csr0 & CSR0_CLRALL);

    if (csr0 & CSR0_ERR) /* LANCE Error */
    {
	printf("%s: error: %#hX", SK_NAME, csr0);

        if (csr0 & CSR0_MISS)      /* No place to store packet ? */
        {
		printf(", Packet dropped.");
        }
	putchar('\n');
    }


    /* Set next buffer */
    p->tmdlast++;
    p->tmdlast &= TMDNUM-1;

    tmdstat = tmdp->u.s.status & 0xff00; /* filter out status bits 15:08 */

    /*
     * We check status of transmitted packet.
     * see LANCE data-sheet for error explanation
     */
    if (tmdstat & TX_ERR) /* Error occurred */
    {
	printf("%s: TX error: %#hX %#hX\n", SK_NAME, (int) tmdstat,
		(int) tmdp->status2);

	if (tmdp->status2 & TX_TDR)    /* TDR problems? */
	{
	    printf("%s: tdr-problems \n", SK_NAME);
	}

        if (tmdp->status2 & TX_UFLO)   /* Underflow error ? */
        {
            /*
             * If UFLO error occurs it will turn transmitter of.
             * So we must reinit LANCE
             */

            SK_lance_init(nic, MODE_NORMAL);
        }

	tmdp->status2 = 0;             /* Clear error flags */
    }

    SK_write_reg(CSR0, CSR0_INEA); /* Enable Interrupts */

	/* Set pointer to next transmit buffer */
	p->tmdnum++;
	p->tmdnum &= TMDNUM-1;

}

/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void SK_disable(struct dev *dev)
{
	struct nic *nic = (struct nic *)dev;

	/* put the card in its initial state */
	SK_lance_init(nic, MODE_NORMAL);	/* reset and disable merge */
	
	PRINTF(("## %s: At beginning of SK_disable(). CSR0: %#hX\n",
		SK_NAME, SK_read_reg(CSR0)));
	PRINTF(("%s: Shutting %s down CSR0 %#hX\n", SK_NAME, SK_NAME,
		(int) SK_read_reg(CSR0)));

	SK_write_reg(CSR0, CSR0_STOP); /* STOP the LANCE */
}

/**************************************************************************
IRQ - Enable, Disable, or Force interrupts
***************************************************************************/
static void SK_irq(struct nic *nic __unused, irq_action_t action __unused)
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
PROBE - Look for an adapter, this routine's visible to the outside
***************************************************************************/
static int SK_probe(struct dev *dev, unsigned short *probe_addrs)
{
	struct nic *nic = (struct nic *)dev;
	unsigned short		*p;
	static unsigned short	io_addrs[] = SK_IO_PORTS;
	/* if probe_addrs is 0, then routine can use a hardwired default */
	nic->priv_data = &p_data;
	if (probe_addrs == 0)
		probe_addrs = io_addrs;
	for (p = probe_addrs; (ioaddr = *p) != 0; ++p)
	{
		long		offset1, offset0 = inb(ioaddr);
		if ((offset0 == SK_IDLOW) &&
		 ((offset1 = inb(ioaddr + 1)) == SK_IDHIGH))
			if (SK_probe1(nic, ioaddr) >= 0)
				break;
	}
	/* if board found */
	if (ioaddr != 0)
	{
	        nic->ioaddr = ioaddr & ~3;
		nic->irqno  = 0;
		/* point to NIC specific routines */
		dev->disable  = SK_disable;
		nic->poll     = SK_poll;
		nic->transmit = SK_transmit;
		nic->irq      = SK_irq;
		/* FIXME set dev->devid */
		return 1;
	}
	/* else */
	{
		return 0;
	}
}

int SK_probe1(struct nic *nic, short ioaddr1 __unused)
{
    int i,j;                /* Counters */
    unsigned int rom_addr;  /* used to store RAM address used for POS_ADDR */

    struct priv *p;         /* SK_G16 private structure */

    if (SK_ADDR & 0x3fff || SK_ADDR < 0xa0000)
    {
       /*
        * Now here we could use a routine which searches for a free
        * place in the ram and set SK_ADDR if found. TODO.
        */
            printf("%s: SK_ADDR %#hX is not valid. Check configuration.\n",
                    SK_NAME, SK_ADDR);
            return -1;
    }

    rom_addr = SK_ADDR;

    outb(SK_ROM_RAM_OFF, SK_POS2); /* Boot_ROM + RAM off */
    outb(POS_ADDR, SK_POS3);       /* Set RAM address */
    outb(SK_ROM_RAM_ON, SK_POS2);  /* RAM on, BOOT_ROM on */
#ifdef	SK_DEBUG
    SK_print_pos(nic, "POS registers after ROM, RAM config");
#endif

    board = (SK_RAM *) rom_addr;
	PRINTF(("adr[0]: %hX, adr[1]: %hX, adr[2]: %hX\n",
	board->rom[0], board->rom[2], board->rom[4]));

    /* Read in station address */
    for (i = 0, j = 0; i < ETH_ALEN; i++, j+=2)
    {
	*(nic->node_addr+i) = board->rom[j];
    }

    /* Check for manufacturer code */
#ifdef	SK_DEBUG
    if (!(*(nic->node_addr+0) == SK_MAC0 &&
	  *(nic->node_addr+1) == SK_MAC1 &&
	  *(nic->node_addr+2) == SK_MAC2) )
    {
        PRINTF(("## %s: We did not find SK_G16 at RAM location.\n",
                SK_NAME));
	return -1;                     /* NO SK_G16 found */
    }
#endif

    p = nic->priv_data;

    /* Initialize private structure */

    p->ram = (struct SK_ram *) rom_addr; /* Set dual ported RAM addr */
    p->tmdhead = &(p->ram)->tmde[0];     /* Set TMD head */
    p->rmdhead = &(p->ram)->rmde[0];     /* Set RMD head */

    printf("Schneider & Koch G16 at %#hX, mem at %#hX, HW addr: %!\n",
	    (unsigned int) ioaddr, (unsigned int) p->ram, nic->node_addr);

    /* Initialize buffer pointers */

    for (i = 0; i < TMDNUM; i++)
    {
	p->tmdbufs[i] = p->ram->tmdbuf[i];
    }

    for (i = 0; i < RMDNUM; i++)
    {
	p->rmdbufs[i] = p->ram->rmdbuf[i];
    }
    i = 0;

    if (!(i = SK_lance_init(nic, MODE_NORMAL)))  /* LANCE init OK? */
    {

#ifdef	SK_DEBUG
        /*
         * This debug block tries to stop LANCE,
         * reinit LANCE with transmitter and receiver disabled,
         * then stop again and reinit with NORMAL_MODE
         */

        printf("## %s: After lance init. CSR0: %#hX\n",
               SK_NAME, SK_read_reg(CSR0));
        SK_write_reg(CSR0, CSR0_STOP);
        printf("## %s: LANCE stopped. CSR0: %#hX\n",
               SK_NAME, SK_read_reg(CSR0));
        SK_lance_init(nic, MODE_DTX | MODE_DRX);
        printf("## %s: Reinit with DTX + DRX off. CSR0: %#hX\n",
               SK_NAME, SK_read_reg(CSR0));
        SK_write_reg(CSR0, CSR0_STOP);
        printf("## %s: LANCE stopped. CSR0: %#hX\n",
               SK_NAME, SK_read_reg(CSR0));
        SK_lance_init(nic, MODE_NORMAL);
        printf("## %s: LANCE back to normal mode. CSR0: %#hX\n",
               SK_NAME, SK_read_reg(CSR0));
        SK_print_pos(nic, "POS regs before returning OK");

#endif	/* SK_DEBUG */

    }
    else /* LANCE init failed */
    {

	PRINTF(("## %s: LANCE init failed: CSR0: %#hX\n",
               SK_NAME, SK_read_reg(CSR0)));
	return -1;
    }

#ifdef	SK_DEBUG
    SK_print_pos(nic, "End of SK_probe1");
    SK_print_ram(nic);
#endif

    return 0;                            /* Initialization done */

} /* End of SK_probe1() */

static int SK_lance_init(struct nic *nic, unsigned short mode)
{
    int i;
    struct priv *p = (struct priv *) nic->priv_data;
    struct tmd  *tmdp;
    struct rmd  *rmdp;

    PRINTF(("## %s: At beginning of LANCE init. CSR0: %#hX\n",
           SK_NAME, SK_read_reg(CSR0)));

    /* Reset LANCE */
    SK_reset_board();

    /* Initialize TMD's with start values */
    p->tmdnum = 0;                   /* First descriptor for transmitting */
    p->tmdlast = 0;                  /* First descriptor for reading stats */

    for (i = 0; i < TMDNUM; i++)     /* Init all TMD's */
    {
	tmdp = p->tmdhead + i;

	tmdp->u.buffer = (unsigned long) p->tmdbufs[i]; /* assign buffer */

	/* Mark TMD as start and end of packet */
	tmdp->u.s.status = TX_STP | TX_ENP;
    }


    /* Initialize RMD's with start values */

    p->rmdnum = 0;                   /* First RMD which will be used */

    for (i = 0; i < RMDNUM; i++)     /* Init all RMD's */
    {
	rmdp = p->rmdhead + i;


	rmdp->u.buffer = (unsigned long) p->rmdbufs[i]; /* assign buffer */

	/*
         * LANCE must be owner at beginning so that he can fill in
	 * receiving packets, set status and release RMD
	 */

	rmdp->u.s.status = RX_OWN;

	rmdp->blen = -PKT_BUF_SZ;    /* Buffer Size in a two's complement */

	rmdp->mlen = 0;              /* init message length */

    }

    /* Fill LANCE Initialize Block */

    (p->ram)->ib.mode = mode;        /* Set operation mode */

    for (i = 0; i < ETH_ALEN; i++)   /* Set physical address */
    {
	(p->ram)->ib.paddr[i] = *(nic->node_addr+i);
    }

    for (i = 0; i < 8; i++)          /* Set multicast, logical address */
    {
	(p->ram)->ib.laddr[i] = 0;   /* We do not use logical addressing */
    }

    /* Set ring descriptor pointers and set number of descriptors */

    (p->ram)->ib.rdrp = (int)  p->rmdhead | RMDNUMMASK;
    (p->ram)->ib.tdrp = (int)  p->tmdhead | TMDNUMMASK;

    /* Prepare LANCE Control and Status Registers */

    SK_write_reg(CSR3, CSR3_ACON);   /* Ale Control !!!THIS MUST BE SET!!!! */

    /*
     * LANCE addresses the RAM from 0x0000 to 0x3fbf and has no access to
     * PC Memory locations.
     *
     * In structure SK_ram is defined that the first thing in ram
     * is the initialization block. So his address is for LANCE always
     * 0x0000
     *
     * CSR1 contains low order bits 15:0 of initialization block address
     * CSR2 is built of:
     *    7:0  High order bits 23:16 of initialization block address
     *   15:8  reserved, must be 0
     */

    /* Set initialization block address (must be on word boundary) */
    SK_write_reg(CSR1, 0);          /* Set low order bits 15:0 */
    SK_write_reg(CSR2, 0);          /* Set high order bits 23:16 */


    PRINTF(("## %s: After setting CSR1-3. CSR0: %#hX\n",
           SK_NAME, SK_read_reg(CSR0)));

    /* Initialize LANCE */

    /*
     * INIT = Initialize, when set, causes the LANCE to begin the
     * initialization procedure and access the Init Block.
     */

    SK_write_reg(CSR0, CSR0_INIT);

    /* Wait until LANCE finished initialization */

    SK_set_RAP(CSR0);              /* Register Address Pointer to CSR0 */

    for (i = 0; (i < 100) && !(SK_rread_reg() & CSR0_IDON); i++)
	; /* Wait until init done or go ahead if problems (i>=100) */

    if (i >= 100) /* Something is wrong ! */
    {
	printf("%s: can't init am7990, status: %#hX "
	       "init_block: %#hX\n",
		SK_NAME, (int) SK_read_reg(CSR0),
		(unsigned int) &(p->ram)->ib);

#ifdef	SK_DEBUG
	SK_print_pos(nic, "LANCE INIT failed");
#endif

	return -1;                 /* LANCE init failed */
    }

    PRINTF(("## %s: init done after %d ticks\n", SK_NAME, i));

    /* Clear Initialize done, enable Interrupts, start LANCE */

    SK_write_reg(CSR0, CSR0_IDON | CSR0_INEA | CSR0_STRT);

    PRINTF(("## %s: LANCE started. CSR0: %#hX\n", SK_NAME,
            SK_read_reg(CSR0)));

    return 0;                      /* LANCE is up and running */

} /* End of SK_lance_init() */

/* LANCE access functions
 *
 * ! CSR1-3 can only be accessed when in CSR0 the STOP bit is set !
 */

static void SK_reset_board(void)
{
    int i;

	PRINTF(("## %s: At beginning of SK_reset_board.\n", SK_NAME));
    SK_PORT = 0x00;           /* Reset active */
    for (i = 0; i < 10 ; i++) /* Delay min 5ms */
	;
    SK_PORT = SK_RESET;       /* Set back to normal operation */

} /* End of SK_reset_board() */

static void SK_set_RAP(int reg_number)
{
    SK_IOREG = reg_number;
    SK_PORT  = SK_RESET | SK_RAP | SK_WREG;
    SK_IOCOM = SK_DOIO;

    while (SK_PORT & SK_IORUN)
	;
} /* End of SK_set_RAP() */

static int SK_read_reg(int reg_number)
{
    SK_set_RAP(reg_number);

    SK_PORT  = SK_RESET | SK_RDATA | SK_RREG;
    SK_IOCOM = SK_DOIO;

    while (SK_PORT & SK_IORUN)
	;
    return (SK_IOREG);

} /* End of SK_read_reg() */

static int SK_rread_reg(void)
{
    SK_PORT  = SK_RESET | SK_RDATA | SK_RREG;

    SK_IOCOM = SK_DOIO;

    while (SK_PORT & SK_IORUN)
	;
    return (SK_IOREG);

} /* End of SK_rread_reg() */

static void SK_write_reg(int reg_number, int value)
{
    SK_set_RAP(reg_number);

    SK_IOREG = value;
    SK_PORT  = SK_RESET | SK_RDATA | SK_WREG;
    SK_IOCOM = SK_DOIO;

    while (SK_PORT & SK_IORUN)
	;
} /* End of SK_write_reg */

/*
 * Debugging functions
 * -------------------
 */

#ifdef	SK_DEBUG
static void SK_print_pos(struct nic *nic, char *text)
{

    unsigned char pos0 = inb(SK_POS0),
		  pos1 = inb(SK_POS1),
		  pos2 = inb(SK_POS2),
		  pos3 = inb(SK_POS3),
		  pos4 = inb(SK_POS4);


    printf("## %s: %s.\n"
           "##   pos0=%#hX pos1=%#hX pos2=%#hX pos3=%#hX pos4=%#hX\n",
           SK_NAME, text, pos0, pos1, pos2, (pos3<<14), pos4);

} /* End of SK_print_pos() */

static void SK_print_ram(struct nic *nic)
{

    int i;
    struct priv *p = (struct priv *) nic->priv_data;

    printf("## %s: RAM Details.\n"
           "##   RAM at %#hX tmdhead: %#hX rmdhead: %#hX initblock: %#hX\n",
           SK_NAME,
           (unsigned int) p->ram,
           (unsigned int) p->tmdhead,
           (unsigned int) p->rmdhead,
           (unsigned int) &(p->ram)->ib);

    printf("##   ");

    for(i = 0; i < TMDNUM; i++)
    {
           if (!(i % 3)) /* Every third line do a newline */
           {
               printf("\n##   ");
           }
        printf("tmdbufs%d: %#hX ", (i+1), (int) p->tmdbufs[i]);
    }
    printf("##   ");

    for(i = 0; i < RMDNUM; i++)
    {
         if (!(i % 3)) /* Every third line do a newline */
           {
               printf("\n##   ");
           }
        printf("rmdbufs%d: %#hX ", (i+1), (int) p->rmdbufs[i]);
    }
    putchar('\n');

} /* End of SK_print_ram() */
#endif

static struct isa_driver SK_driver __isa_driver = {
	.type    = NIC_DRIVER,
	.name    = "SK_G16",
	.probe   = SK_probe,
	.ioaddrs = 0,
};
