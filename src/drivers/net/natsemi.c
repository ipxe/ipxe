/* -*- Mode:C; c-basic-offset:4; -*- */

/* 
   natsemi.c: An Etherboot driver for the NatSemi DP8381x series.

   Copyright (C) 2001 Entity Cyber, Inc.
   
   This development of this Etherboot driver was funded by 
   
      Sicom Systems: http://www.sicompos.com/
   
   Author: Marty Connor (mdc@thinguin.org)	   
   Adapted from a Linux driver which was written by Donald Becker
   
   This software may be used and distributed according to the terms
   of the GNU Public License (GPL), incorporated herein by reference.
   
   Original Copyright Notice:
   
   Written/copyright 1999-2001 by Donald Becker.
   
   This software may be used and distributed according to the terms of
   the GNU General Public License (GPL), incorporated herein by reference.
   Drivers based on or derived from this code fall under the GPL and must
   retain the authorship, copyright and license notice.  This file is not
   a complete program and may only be used when the entire operating
   system is licensed under the GPL.  License for under other terms may be
   available.  Contact the original author for details.
   
   The original author may be reached as becker@scyld.com, or at
   Scyld Computing Corporation
   410 Severn Ave., Suite 210
   Annapolis MD 21403
   
   Support information and updates available at
   http://www.scyld.com/network/netsemi.html
   
   References:
   
   http://www.scyld.com/expert/100mbps.html
   http://www.scyld.com/expert/NWay.html
   Datasheet is available from:
   http://www.national.com/pf/DP/DP83815.html

*/

/* Revision History */

/*
  13 Dec 2003 timlegge 1.1 Enabled Multicast Support
  29 May 2001  mdc     1.0
     Initial Release.  Tested with Netgear FA311 and FA312 boards
*/
/* Includes */

#include "etherboot.h"
#include "nic.h"
#include "pci.h"

/* defines */

#define OWN       0x80000000
#define DSIZE     0x00000FFF
#define CRC_SIZE  4

/* Time in ticks before concluding the transmitter is hung. */
#define TX_TIMEOUT       (4*TICKS_PER_SEC)

#define TX_BUF_SIZE    1536
#define RX_BUF_SIZE    1536

#define NUM_RX_DESC    4              /* Number of Rx descriptor registers. */

typedef uint8_t    u8;
typedef int8_t     s8;
typedef uint16_t   u16;
typedef int16_t    s16;
typedef uint32_t   u32;
typedef int32_t    s32;

/* helpful macroes if on a big_endian machine for changing byte order.
   not strictly needed on Intel */
#define get_unaligned(ptr) (*(ptr))
#define put_unaligned(val, ptr) ((void)( *(ptr) = (val) ))
#define get_u16(ptr) (*(u16 *)(ptr))
#define virt_to_le32desc(addr)  virt_to_bus(addr)

enum pcistuff {
    PCI_USES_IO     = 0x01,
    PCI_USES_MEM    = 0x02,
    PCI_USES_MASTER = 0x04,
    PCI_ADDR0       = 0x08,
    PCI_ADDR1       = 0x10,
};

/* MMIO operations required */
#define PCI_IOTYPE (PCI_USES_MASTER | PCI_USES_MEM | PCI_ADDR1)

/* Offsets to the device registers.
   Unlike software-only systems, device drivers interact with complex hardware.
   It's not useful to define symbolic names for every register bit in the
   device.
*/
enum register_offsets {
    ChipCmd      = 0x00, 
    ChipConfig   = 0x04, 
    EECtrl       = 0x08, 
    PCIBusCfg    = 0x0C,
    IntrStatus   = 0x10, 
    IntrMask     = 0x14, 
    IntrEnable   = 0x18,
    TxRingPtr    = 0x20, 
    TxConfig     = 0x24,
    RxRingPtr    = 0x30,
    RxConfig     = 0x34, 
    ClkRun       = 0x3C,
    WOLCmd       = 0x40, 
    PauseCmd     = 0x44,
    RxFilterAddr = 0x48, 
    RxFilterData = 0x4C,
    BootRomAddr  = 0x50, 
    BootRomData  = 0x54, 
    SiliconRev   = 0x58, 
    StatsCtrl    = 0x5C,
    StatsData    = 0x60, 
    RxPktErrs    = 0x60, 
    RxMissed     = 0x68, 
    RxCRCErrs    = 0x64,
    PCIPM        = 0x44,
    PhyStatus    = 0xC0, 
    MIntrCtrl    = 0xC4, 
    MIntrStatus  = 0xC8,

    /* These are from the spec, around page 78... on a separate table. */
    PGSEL        = 0xCC, 
    PMDCSR       = 0xE4, 
    TSTDAT       = 0xFC, 
    DSPCFG       = 0xF4, 
    SDCFG        = 0x8C
};

/* Bit in ChipCmd. */
enum ChipCmdBits {
    ChipReset = 0x100, 
    RxReset   = 0x20, 
    TxReset   = 0x10, 
    RxOff     = 0x08, 
    RxOn      = 0x04,
    TxOff     = 0x02, 
    TxOn      = 0x01
};

/* Bits in the RxMode register. */
enum rx_mode_bits {
    AcceptErr          = 0x20,
    AcceptRunt         = 0x10,
    AcceptBroadcast    = 0xC0000000,
    AcceptMulticast    = 0x00200000, 
    AcceptAllMulticast = 0x20000000,
    AcceptAllPhys      = 0x10000000, 
    AcceptMyPhys       = 0x08000000,
    RxFilterEnable     = 0x80000000
};

typedef struct _BufferDesc {
    u32              link;
    volatile u32     cmdsts;
    u32              bufptr;
    u32				 software_use;
} BufferDesc;

/* Bits in network_desc.status */
enum desc_status_bits {
    DescOwn   = 0x80000000, 
    DescMore  = 0x40000000, 
    DescIntr  = 0x20000000,
    DescNoCRC = 0x10000000,
    DescPktOK = 0x08000000, 
    RxTooLong = 0x00400000
};

/* Globals */

static int natsemi_debug = 1;			/* 1 normal messages, 0 quiet .. 7 verbose. */

const char *nic_name;

static u32 SavedClkRun;


static unsigned short vendor, dev_id;
static unsigned long ioaddr;

static unsigned int cur_rx;

static unsigned int advertising;

static unsigned int rx_config;
static unsigned int tx_config;

/* Note: transmit and receive buffers and descriptors must be 
   longword aligned 
*/

static BufferDesc txd              __attribute__ ((aligned(4)));
static BufferDesc rxd[NUM_RX_DESC] __attribute__ ((aligned(4)));

static unsigned char txb[TX_BUF_SIZE] __attribute__ ((aligned(4)));
static unsigned char rxb[NUM_RX_DESC * RX_BUF_SIZE] __attribute__ ((aligned(4)));

/* Function Prototypes */

static int natsemi_probe(struct dev *dev, struct pci_device *pci);
static int eeprom_read(long addr, int location);
static int mdio_read(int phy_id, int location);
static void natsemi_init(struct nic *nic);
static void natsemi_reset(struct nic *nic);
static void natsemi_init_rxfilter(struct nic *nic);
static void natsemi_init_txd(struct nic *nic);
static void natsemi_init_rxd(struct nic *nic);
static void natsemi_set_rx_mode(struct nic *nic);
static void natsemi_check_duplex(struct nic *nic);
static void natsemi_transmit(struct nic *nic, const char *d, unsigned int t, unsigned int s, const char *p);
static int  natsemi_poll(struct nic *nic, int retrieve);
static void natsemi_disable(struct dev *dev);
static void natsemi_irq(struct nic *nic, irq_action_t action);

/* 
 * Function: natsemi_probe
 *
 * Description: Retrieves the MAC address of the card, and sets up some
 * globals required by other routines,  and initializes the NIC, making it
 * ready to send and receive packets.
 *
 * Side effects:
 *            leaves the ioaddress of the natsemi chip in the variable ioaddr.
 *            leaves the natsemi initialized, and ready to recieve packets.
 *
 * Returns:   struct nic *:          pointer to NIC data structure
 */

static int
natsemi_probe(struct dev *dev, struct pci_device *pci)
{
    struct nic *nic = (struct nic *)dev;
    int i;
    int prev_eedata;
    u32 tmp;

    if (pci->ioaddr == 0)
        return 0;

    adjust_pci_device(pci);

    /* initialize some commonly used globals */
	
    nic->irqno  = 0;
    nic->ioaddr = pci->ioaddr & ~3;

    ioaddr     = pci->ioaddr & ~3;
    vendor     = pci->vendor;
    dev_id     = pci->dev_id;
    nic_name   = pci->name;

    /* natsemi has a non-standard PM control register
     * in PCI config space.  Some boards apparently need
     * to be brought to D0 in this manner.
     */
    pcibios_read_config_dword(pci->bus, pci->devfn, PCIPM, &tmp);
    if (tmp & (0x03|0x100)) {
	/* D0 state, disable PME assertion */
	u32 newtmp = tmp & ~(0x03|0x100);
	pcibios_write_config_dword(pci->bus, pci->devfn, PCIPM, newtmp);
    }

    /* get MAC address */

    prev_eedata = eeprom_read(ioaddr, 6);
    for (i = 0; i < 3; i++) {
	int eedata = eeprom_read(ioaddr, i + 7);
	nic->node_addr[i*2] = (eedata << 1) + (prev_eedata >> 15);
	nic->node_addr[i*2+1] = eedata >> 7;
	prev_eedata = eedata;
    }

    printf("\nnatsemi_probe: MAC addr %! at ioaddr %#hX\n",
           nic->node_addr, ioaddr);
    printf("natsemi_probe: Vendor:%#hX Device:%#hX\n", vendor, dev_id);
    
    /* Reset the chip to erase any previous misconfiguration. */
    outl(ChipReset, ioaddr + ChipCmd);

    advertising = mdio_read(1, 4);
    {
	u32 chip_config = inl(ioaddr + ChipConfig);
	printf("%s: Transceiver default autoneg. %s "
	       "10%s %s duplex.\n",
	       nic_name,
	       chip_config & 0x2000 ? "enabled, advertise" : "disabled, force",
	       chip_config & 0x4000 ? "0" : "",
	       chip_config & 0x8000 ? "full" : "half");
    }
    printf("%s: Transceiver status %hX advertising %hX\n",
	   nic_name, (int)inl(ioaddr + 0x84), advertising);

    /* Disable PME:
     * The PME bit is initialized from the EEPROM contents.
     * PCI cards probably have PME disabled, but motherboard
     * implementations may have PME set to enable WakeOnLan. 
     * With PME set the chip will scan incoming packets but
     * nothing will be written to memory. */
    SavedClkRun = inl(ioaddr + ClkRun);
    outl(SavedClkRun & ~0x100, ioaddr + ClkRun);

    /* initialize device */
    natsemi_init(nic);

    dev->disable  = natsemi_disable;
    nic->poll     = natsemi_poll;
    nic->transmit = natsemi_transmit;
    nic->irq      = natsemi_irq;

    return 1;
}

/* Read the EEPROM and MII Management Data I/O (MDIO) interfaces.
   The EEPROM code is for the common 93c06/46 EEPROMs with 6 bit addresses. 
*/

/* Delay between EEPROM clock transitions.
   No extra delay is needed with 33Mhz PCI, but future 66Mhz access may need
   a delay. */
#define eeprom_delay(ee_addr)	inl(ee_addr)

enum EEPROM_Ctrl_Bits {
    EE_ShiftClk   = 0x04, 
    EE_DataIn     = 0x01, 
    EE_ChipSelect = 0x08, 
    EE_DataOut    = 0x02
};

#define EE_Write0 (EE_ChipSelect)
#define EE_Write1 (EE_ChipSelect | EE_DataIn)

/* The EEPROM commands include the alway-set leading bit. */
enum EEPROM_Cmds {
    EE_WriteCmd=(5 << 6), EE_ReadCmd=(6 << 6), EE_EraseCmd=(7 << 6),
};

static int eeprom_read(long addr, int location)
{
    int i;
    int retval = 0;
    int ee_addr = addr + EECtrl;
    int read_cmd = location | EE_ReadCmd;
    outl(EE_Write0, ee_addr);

    /* Shift the read command bits out. */
    for (i = 10; i >= 0; i--) {
	short dataval = (read_cmd & (1 << i)) ? EE_Write1 : EE_Write0;
	outl(dataval, ee_addr);
	eeprom_delay(ee_addr);
	outl(dataval | EE_ShiftClk, ee_addr);
	eeprom_delay(ee_addr);
    }
    outl(EE_ChipSelect, ee_addr);
    eeprom_delay(ee_addr);

    for (i = 0; i < 16; i++) {
	outl(EE_ChipSelect | EE_ShiftClk, ee_addr);
	eeprom_delay(ee_addr);
	retval |= (inl(ee_addr) & EE_DataOut) ? 1 << i : 0;
	outl(EE_ChipSelect, ee_addr);
	eeprom_delay(ee_addr);
    }

    /* Terminate the EEPROM access. */
    outl(EE_Write0, ee_addr);
    outl(0, ee_addr);

    return retval;
}

/*  MII transceiver control section.
	The 83815 series has an internal transceiver, and we present the
	management registers as if they were MII connected. */

static int mdio_read(int phy_id, int location)
{
    if (phy_id == 1 && location < 32)
	return inl(ioaddr + 0x80 + (location<<2)) & 0xffff;
    else
	return 0xffff;
}

/* Function: natsemi_init
 *
 * Description: resets the ethernet controller chip and configures
 *    registers and data structures required for sending and receiving packets.
 *    
 * Arguments: struct nic *nic:          NIC data structure
 *
 * returns:   void.
 */

static void
natsemi_init(struct nic *nic)
{
    natsemi_reset(nic);
		
    /* Disable PME:
     * The PME bit is initialized from the EEPROM contents.
     * PCI cards probably have PME disabled, but motherboard
     * implementations may have PME set to enable WakeOnLan. 
     * With PME set the chip will scan incoming packets but
     * nothing will be written to memory. */
    outl(SavedClkRun & ~0x100, ioaddr + ClkRun);

    natsemi_init_rxfilter(nic);

    natsemi_init_txd(nic);
    natsemi_init_rxd(nic);

    /* Initialize other registers. */
    /* Configure the PCI bus bursts and FIFO thresholds. */
    /* Configure for standard, in-spec Ethernet. */
    if (inl(ioaddr + ChipConfig) & 0x20000000) {	/* Full duplex */
	tx_config = 0xD0801002;
	rx_config = 0x10000020;
    } else {
	tx_config = 0x10801002;
	rx_config = 0x0020;
    }
    outl(tx_config, ioaddr + TxConfig);
    outl(rx_config, ioaddr + RxConfig);

    natsemi_check_duplex(nic);
    natsemi_set_rx_mode(nic);

    outl(RxOn, ioaddr + ChipCmd);
}

/* 
 * Function: natsemi_reset
 *
 * Description: soft resets the controller chip
 *
 * Arguments: struct nic *nic:          NIC data structure
 *
 * Returns:   void.
 */
static void 
natsemi_reset(struct nic *nic __unused)
{
    outl(ChipReset, ioaddr + ChipCmd);
	
    /* On page 78 of the spec, they recommend some settings for "optimum
       performance" to be done in sequence.  These settings optimize some
       of the 100Mbit autodetection circuitry.  Also, we only want to do
       this for rev C of the chip.
    */
    if (inl(ioaddr + SiliconRev) == 0x302) {
	outw(0x0001, ioaddr + PGSEL);
	outw(0x189C, ioaddr + PMDCSR);
	outw(0x0000, ioaddr + TSTDAT);
	outw(0x5040, ioaddr + DSPCFG);
	outw(0x008C, ioaddr + SDCFG);
    }
    /* Disable interrupts using the mask. */
    outl(0, ioaddr + IntrMask);
    outl(0, ioaddr + IntrEnable);
}

/* Function: natsemi_init_rxfilter
 *
 * Description: sets receive filter address to our MAC address
 *
 * Arguments: struct nic *nic:          NIC data structure
 *
 * returns:   void.
 */

static void
natsemi_init_rxfilter(struct nic *nic)
{
    int i;

    for (i = 0; i < ETH_ALEN; i += 2) {
	outl(i, ioaddr + RxFilterAddr);
	outw(nic->node_addr[i] + (nic->node_addr[i+1] << 8), ioaddr + RxFilterData);
    }
}

/* 
 * Function: natsemi_init_txd
 *
 * Description: initializes the Tx descriptor
 *
 * Arguments: struct nic *nic:          NIC data structure
 *
 * returns:   void.
 */

static void
natsemi_init_txd(struct nic *nic __unused)
{
    txd.link   = (u32) 0;
    txd.cmdsts = (u32) 0;
    txd.bufptr = virt_to_bus(&txb[0]);

    /* load Transmit Descriptor Register */
    outl(virt_to_bus(&txd), ioaddr + TxRingPtr); 
    if (natsemi_debug > 1)
        printf("natsemi_init_txd: TX descriptor register loaded with: %X\n", 
               inl(ioaddr + TxRingPtr));
}

/* Function: natsemi_init_rxd
 *
 * Description: initializes the Rx descriptor ring
 *    
 * Arguments: struct nic *nic:          NIC data structure
 *
 * Returns:   void.
 */
 
static void 
natsemi_init_rxd(struct nic *nic __unused) 
{ 
    int i;

    cur_rx = 0; 

    /* init RX descriptor */
    for (i = 0; i < NUM_RX_DESC; i++) {
        rxd[i].link   = virt_to_bus((i+1 < NUM_RX_DESC) ? &rxd[i+1] : &rxd[0]);
        rxd[i].cmdsts = (u32) RX_BUF_SIZE;
        rxd[i].bufptr = virt_to_bus(&rxb[i*RX_BUF_SIZE]);
        if (natsemi_debug > 1)
            printf("natsemi_init_rxd: rxd[%d]=%X link=%X cmdsts=%X bufptr=%X\n", 
                   i, &rxd[i], rxd[i].link, rxd[i].cmdsts, rxd[i].bufptr);
    }

    /* load Receive Descriptor Register */
    outl(virt_to_bus(&rxd[0]), ioaddr + RxRingPtr);

    if (natsemi_debug > 1)
        printf("natsemi_init_rxd: RX descriptor register loaded with: %X\n", 
               inl(ioaddr + RxRingPtr));
}

/* Function: natsemi_set_rx_mode
 *
 * Description: 
 *    sets the receive mode to accept all broadcast packets and packets
 *    with our MAC address, and reject all multicast packets.      
 *    
 * Arguments: struct nic *nic:          NIC data structure
 *
 * Returns:   void.
 */

static void natsemi_set_rx_mode(struct nic *nic __unused)
{
    u32 rx_mode = RxFilterEnable | AcceptBroadcast |
	    AcceptAllMulticast | AcceptMyPhys;
	
    outl(rx_mode, ioaddr + RxFilterAddr);
}

static void natsemi_check_duplex(struct nic *nic __unused)
{
    int duplex = inl(ioaddr + ChipConfig) & 0x20000000 ? 1 : 0;
	
    if (natsemi_debug)
	printf("%s: Setting %s-duplex based on negotiated link"
	       " capability.\n", nic_name,
	       duplex ? "full" : "half");
    if (duplex) {
	rx_config |= 0x10000000;
	tx_config |= 0xC0000000;
    } else {
	rx_config &= ~0x10000000;
	tx_config &= ~0xC0000000;
    }
    outl(tx_config, ioaddr + TxConfig);
    outl(rx_config, ioaddr + RxConfig);
}

/* Function: natsemi_transmit
 *
 * Description: transmits a packet and waits for completion or timeout.
 *
 * Arguments: char d[6]:          destination ethernet address.
 *            unsigned short t:   ethernet protocol type.
 *            unsigned short s:   size of the data-part of the packet.
 *            char *p:            the data for the packet.
 *    
 * Returns:   void.
 */

static void
natsemi_transmit(struct nic  *nic,
		 const char  *d,     /* Destination */
		 unsigned int t,     /* Type */
		 unsigned int s,     /* size */
		 const char  *p)     /* Packet */
{
    u32 to, nstype;
    u32 tx_status;
    
    /* Stop the transmitter */
    outl(TxOff, ioaddr + ChipCmd);

    /* load Transmit Descriptor Register */
    outl(virt_to_bus(&txd), ioaddr + TxRingPtr);
    if (natsemi_debug > 1)
        printf("natsemi_transmit: TX descriptor register loaded with: %X\n", 
               inl(ioaddr + TxRingPtr));

    memcpy(txb, d, ETH_ALEN);
    memcpy(txb + ETH_ALEN, nic->node_addr, ETH_ALEN);
    nstype = htons(t);
    memcpy(txb + 2 * ETH_ALEN, (char*)&nstype, 2);
    memcpy(txb + ETH_HLEN, p, s);

    s += ETH_HLEN;
    s &= DSIZE;

    if (natsemi_debug > 1)
        printf("natsemi_transmit: sending %d bytes ethtype %hX\n", (int) s, t);

    /* pad to minimum packet size */
    while (s < ETH_ZLEN)  
        txb[s++] = '\0';

    /* set the transmit buffer descriptor and enable Transmit State Machine */
    txd.bufptr = virt_to_bus(&txb[0]);
    txd.cmdsts = (u32) OWN | s;

    /* restart the transmitter */
    outl(TxOn, ioaddr + ChipCmd);

    if (natsemi_debug > 1)
        printf("natsemi_transmit: Queued Tx packet size %d.\n", (int) s);

    to = currticks() + TX_TIMEOUT;

    while ((((volatile u32) tx_status=txd.cmdsts) & OWN) && (currticks() < to))
        /* wait */ ;

    if (currticks() >= to) {
        printf("natsemi_transmit: TX Timeout! Tx status %X.\n", tx_status);
    }

    if (!(tx_status & 0x08000000)) {
	printf("natsemi_transmit: Transmit error, Tx status %X.\n", tx_status);
    }
}

/* Function: natsemi_poll
 *
 * Description: checks for a received packet and returns it if found.
 *
 * Arguments: struct nic *nic:          NIC data structure
 *
 * Returns:   1 if    packet was received.
 *            0 if no packet was received.
 *
 * Side effects:
 *            Returns (copies) the packet to the array nic->packet.
 *            Returns the length of the packet in nic->packetlen.
 */

static int
natsemi_poll(struct nic *nic, int retrieve)
{
    u32 rx_status = rxd[cur_rx].cmdsts;
    int retstat = 0;

    if (natsemi_debug > 2)
        printf("natsemi_poll: cur_rx:%d, status:%X\n", cur_rx, rx_status);

    if (!(rx_status & OWN))
        return retstat;

    if ( ! retrieve ) return 1;

    if (natsemi_debug > 1)
        printf("natsemi_poll: got a packet: cur_rx:%d, status:%X\n",
               cur_rx, rx_status);

    nic->packetlen = (rx_status & DSIZE) - CRC_SIZE;

    if ((rx_status & (DescMore|DescPktOK|RxTooLong)) != DescPktOK) {
        /* corrupted packet received */
        printf("natsemi_poll: Corrupted packet received, buffer status = %X\n",
               rx_status);
        retstat = 0;
    } else {
        /* give packet to higher level routine */
        memcpy(nic->packet, (rxb + cur_rx*RX_BUF_SIZE), nic->packetlen);
        retstat = 1;
    }

    /* return the descriptor and buffer to receive ring */
    rxd[cur_rx].cmdsts = RX_BUF_SIZE;
    rxd[cur_rx].bufptr = virt_to_bus(&rxb[cur_rx*RX_BUF_SIZE]);
        
    if (++cur_rx == NUM_RX_DESC)
        cur_rx = 0;

    /* re-enable the potentially idle receive state machine */
    outl(RxOn, ioaddr + ChipCmd);

    return retstat;
}

/* Function: natsemi_disable
 *
 * Description: Turns off interrupts and stops Tx and Rx engines
 *    
 * Arguments: struct nic *nic:          NIC data structure
 *
 * Returns:   void.
 */

static void
natsemi_disable(struct dev *dev)
{
    struct nic *nic = (struct nic *)dev;
    /* merge reset and disable */
    natsemi_init(nic);

    /* Disable interrupts using the mask. */
    outl(0, ioaddr + IntrMask);
    outl(0, ioaddr + IntrEnable);

    /* Stop the chip's Tx and Rx processes. */
    outl(RxOff | TxOff, ioaddr + ChipCmd);
	
    /* Restore PME enable bit */
    outl(SavedClkRun, ioaddr + ClkRun);
}

/* Function: natsemi_irq
 *
 * Description: Enable, Disable, or Force interrupts
 *    
 * Arguments: struct nic *nic:          NIC data structure
 *            irq_action_t action:      requested action to perform
 *
 * Returns:   void.
 */

static void 
natsemi_irq(struct nic *nic __unused, irq_action_t action __unused)
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

static struct pci_id natsemi_nics[] = {
PCI_ROM(0x100b, 0x0020, "dp83815", "DP83815"),
};

static struct pci_driver natsemi_driver __pci_driver = {
	.type     = NIC_DRIVER,
	.name     = "NATSEMI",
	.probe    = natsemi_probe,
	.ids      = natsemi_nics,
	.id_count = sizeof(natsemi_nics)/sizeof(natsemi_nics[0]),
	.class    = 0,
};
