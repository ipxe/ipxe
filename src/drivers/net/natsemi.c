/* natsemi.c - gPXE driver for the NatSemi DP8381x series. */

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
  02 JUL 2007 Udayan Kumar	 1.2 ported the driver from etherboot to gPXE API.
				     Fully rewritten,adapting the old driver.
		      	      	     Added a circular buffer for transmit and receive.
		                     transmit routine will not wait for transmission to finish.
			             poll routine deals with it.

  13 Dec 2003 timlegge 	         1.1 Enabled Multicast Support
  29 May 2001  mdc     		 1.0
     Initial Release.  		 Tested with Netgear FA311 and FA312 boards
*/
 



#include <stdint.h>
#include <pic8259.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <errno.h>
#include <timer.h>
#include <byteswap.h>
#include <gpxe/pci.h>
#include <gpxe/if_ether.h>
#include <gpxe/ethernet.h>
#include <gpxe/iobuf.h>
#include <gpxe/netdevice.h>
#include <gpxe/spi_bit.h>
#include <gpxe/threewire.h>
#include <gpxe/nvo.h>

#define TX_RING_SIZE 4
#define NUM_RX_DESC  4
#define RX_BUF_SIZE 1536
#define OWN       0x80000000
#define DSIZE     0x00000FFF
#define CRC_SIZE  4

struct natsemi_tx {
	uint32_t link;
	uint32_t cmdsts;
	uint32_t bufptr;
};

struct natsemi_rx {
	uint32_t link;
	uint32_t cmdsts;
	uint32_t bufptr;
};

struct natsemi_nic {
	unsigned short ioaddr;
	unsigned short tx_cur;
	unsigned short tx_dirty;
	unsigned short rx_cur;
	struct natsemi_tx tx[TX_RING_SIZE];
	struct natsemi_rx rx[NUM_RX_DESC];
	/* need to add iobuf as we cannot free iobuf->data in close without this 
	 * alternatively substracting sizeof(head) and sizeof(list_head) can also 
	 * give the same.
	 */
	struct io_buffer *iobuf[NUM_RX_DESC];
	/* netdev_tx_complete needs pointer to the iobuf of the data so as to free 
	 * it from the memory.
	 */
	struct io_buffer *tx_iobuf[TX_RING_SIZE];
	struct spi_bit_basher spibit;
	struct spi_device eeprom;
	struct nvo_block nvo;
};

/* NATSEMI: Offsets to the device registers.
 * Unlike software-only systems, device drivers interact with complex hardware.
 * It's not useful to define symbolic names for every register bit in the
 * device.
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
    /* These are from the spec, around page 78... on a separate table. 
     */
    PGSEL        = 0xCC, 
    PMDCSR       = 0xE4, 
    TSTDAT       = 0xFC, 
    DSPCFG       = 0xF4, 
    SDCFG        = 0x8C,
    BasicControl = 0x80,	
    BasicStatus  = 0x84
	    
};

/* Bit in ChipCmd.
 */
enum ChipCmdBits {
    ChipReset = 0x100, 
    RxReset   = 0x20, 
    TxReset   = 0x10, 
    RxOff     = 0x08, 
    RxOn      = 0x04,
    TxOff     = 0x02, 
    TxOn      = 0x01
};

/* Bits in the RxMode register.
 */
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

/* Bits in network_desc.status
 */
enum desc_status_bits {
    DescOwn   = 0x80000000, 
    DescMore  = 0x40000000, 
    DescIntr  = 0x20000000,
    DescNoCRC = 0x10000000,
    DescPktOK = 0x08000000, 
    RxTooLong = 0x00400000
};

/*Bits in Interrupt Mask register
 */
enum Intr_mask_register_bits {
    RxOk       = 0x001,
    RxErr      = 0x004,
    TxOk       = 0x040,
    TxErr      = 0x100 
};	

/*  EEPROM access , values are devices specific
 */
#define EE_CS		0x08	/* EEPROM chip select */
#define EE_SK		0x04	/* EEPROM shift clock */
#define EE_DI		0x01	/* Data in */
#define EE_DO		0x02	/* Data out */

/* Offsets within EEPROM (these are word offsets)
 */
#define EE_MAC 7
#define EE_REG  EECtrl
static uint32_t SavedClkRun;	

static const uint8_t nat_ee_bits[] = {
	[SPI_BIT_SCLK]	= EE_SK,
	[SPI_BIT_MOSI]	= EE_DI,
	[SPI_BIT_MISO]	= EE_DO,
	[SPI_BIT_SS(0)]	= EE_CS,
};

static int nat_spi_read_bit ( struct bit_basher *basher,
			      unsigned int bit_id ) {
	struct natsemi_nic *nat = container_of ( basher, struct natsemi_nic,
						 spibit.basher );
	uint8_t mask = nat_ee_bits[bit_id];
	uint8_t eereg;

	eereg = inb ( nat->ioaddr + EE_REG);
	return ( eereg & mask );
}

static void nat_spi_write_bit ( struct bit_basher *basher,
				unsigned int bit_id, unsigned long data ) {
	struct natsemi_nic *nat = container_of ( basher, struct natsemi_nic,
						 spibit.basher );
	uint8_t mask = nat_ee_bits[bit_id];
	uint8_t eereg;

	eereg = inb ( nat->ioaddr + EE_REG );
	eereg &= ~mask;
	eereg |= ( data & mask );
	outb ( eereg, nat->ioaddr + EE_REG);
}

static struct bit_basher_operations nat_basher_ops = {
	.read = nat_spi_read_bit,
	.write = nat_spi_write_bit,
};

/* It looks that this portion of EEPROM can be used for 
 * non-volatile stored options. Data sheet does not talk about this region.
 * Currently it is not working. But with some efforts it can.
 */
static struct nvo_fragment nat_nvo_fragments[] = {
	{ 0x0c, 0x68 },
	{ 0, 0 }
};

/*
 * Set up for EEPROM access
 *
 * @v NAT		NATSEMI NIC
 */
 void nat_init_eeprom ( struct natsemi_nic *nat ) {

	/* Initialise three-wire bus 
	 */
	nat->spibit.basher.op = &nat_basher_ops;
	nat->spibit.bus.mode = SPI_MODE_THREEWIRE;
	nat->spibit.endianness = SPI_BIT_LITTLE_ENDIAN;
	init_spi_bit_basher ( &nat->spibit );

	/*natsemi DP 83815 only supports at93c46
	 */
	init_at93c46 ( &nat->eeprom, 16 );
	nat->eeprom.bus = &nat->spibit.bus;

	nat->nvo.nvs = &nat->eeprom.nvs;
	nat->nvo.fragments = nat_nvo_fragments;
}

/*
 * Reset NIC
 *
 * @v		NATSEMI NIC
 *
 * Issues a hardware reset and waits for the reset to complete.
 */
static void nat_reset ( struct natsemi_nic *nat ) {

	int i;
	/* Reset chip
	 */
	outl ( ChipReset, nat->ioaddr + ChipCmd );
	mdelay ( 10 );
	nat->tx_dirty=0;
	nat->tx_cur=0;
	for(i=0;i<TX_RING_SIZE;i++) {
		nat->tx[i].link=0;
		nat->tx[i].cmdsts=0;
		nat->tx[i].bufptr=0;
	}
	nat->rx_cur = 0;
	outl(virt_to_bus(&nat->tx[0]),nat->ioaddr+TxRingPtr);
	outl(virt_to_bus(&nat->rx[0]), nat->ioaddr + RxRingPtr);

	outl(TxOff|RxOff, nat->ioaddr + ChipCmd);

	/* Restore PME enable bit
	 */
	outl(SavedClkRun, nat->ioaddr + ClkRun);
}

/*
 * Open NIC
 *
 * @v netdev		Net device
 * @ret rc		Return status code
 */
static int nat_open ( struct net_device *netdev ) {
	struct natsemi_nic *nat = netdev->priv;
	int i;
	uint32_t tx_config,rx_config;
	
	/* Disable PME:
         * The PME bit is initialized from the EEPROM contents.
         * PCI cards probably have PME disabled, but motherboard
         * implementations may have PME set to enable WakeOnLan. 
         * With PME set the chip will scan incoming packets but
         * nothing will be written to memory. 
         */
        SavedClkRun = inl(nat->ioaddr + ClkRun);
        outl(SavedClkRun & ~0x100, nat->ioaddr + ClkRun);

	/* Setting up Mac address in the NIC
	 */
	for ( i = 0 ; i < ETH_ALEN ; i+=2 ) {
		outl(i,nat->ioaddr+RxFilterAddr);
		outw ( netdev->ll_addr[i] + (netdev->ll_addr[i+1]<<8),
			 nat->ioaddr +RxFilterData);
	}

	/*Set up the Tx Ring
	 */
	nat->tx_cur=0;
	nat->tx_dirty=0;
	for (i=0;i<TX_RING_SIZE;i++) {
		nat->tx[i].link   = virt_to_bus((i+1 < TX_RING_SIZE) ? &nat->tx[i+1] : &nat->tx[0]);
		nat->tx[i].cmdsts = 0;
		nat->tx[i].bufptr = 0;
	}

	/* Set up RX ring
	 */
	nat->rx_cur=0;
	for (i=0;i<NUM_RX_DESC;i++) {
		nat->iobuf[i] = alloc_iob ( RX_BUF_SIZE );
		if (!nat->iobuf[i])
			goto memory_alloc_err;
		nat->rx[i].link   = virt_to_bus((i+1 < NUM_RX_DESC) ? &nat->rx[i+1] : &nat->rx[0]);
		nat->rx[i].cmdsts = (uint32_t) RX_BUF_SIZE;
		nat->rx[i].bufptr = virt_to_bus(nat->iobuf[i]->data);
	}

	/* load Receive Descriptor Register
	 */
	outl(virt_to_bus(&nat->rx[0]), nat->ioaddr + RxRingPtr);
	DBG("Natsemi Rx descriptor loaded with: %X\n",
		(unsigned int)inl(nat->ioaddr+RxRingPtr));		

	/* setup Tx ring
	 */
	outl(virt_to_bus(&nat->tx[0]),nat->ioaddr+TxRingPtr);
	DBG("Natsemi Tx descriptor loaded with: %X\n",
		(unsigned int)inl(nat->ioaddr+TxRingPtr));

	/* Enables RX
	 */
	outl(RxFilterEnable|AcceptBroadcast|AcceptAllMulticast|AcceptMyPhys,
		 nat->ioaddr+RxFilterAddr);

	/* Initialize other registers. 
	 * Configure the PCI bus bursts and FIFO thresholds. 
	 * Configure for standard, in-spec Ethernet. 
	 */
	if (inl(nat->ioaddr + ChipConfig) & 0x20000000) {	/* Full duplex */
		tx_config = 0xD0801002;
		rx_config = 0x10000020;
	} else {
		tx_config = 0x10801002;
		rx_config = 0x0020;
	}
	outl(tx_config, nat->ioaddr + TxConfig);
	outl(rx_config, nat->ioaddr + RxConfig);

	/*start the receiver 
	 */
        outl(RxOn, nat->ioaddr + ChipCmd);

	/*enable interrupts
	 */
	outl((RxOk|RxErr|TxOk|TxErr),nat->ioaddr + IntrMask); 
	//outl(1,nat->ioaddr +IntrEnable);
	return 0;
		       
memory_alloc_err:
	/* this block frees the previously allocated buffers
	 * if memory for all the buffers is not available
	 */
	i=0;
	while(nat->rx[i].cmdsts == (uint32_t) RX_BUF_SIZE) {
		free_iob(nat->iobuf[i]);
		i++;
	}
	return -ENOMEM;	
}

/**
 * Close NIC
 *
 * @v netdev		Net device
 */
static void nat_close ( struct net_device *netdev ) {
	struct natsemi_nic *nat = netdev->priv;
	int i;
	/* Reset the hardware to disable everything in one go
	 */
	nat_reset ( nat );

	/* Free RX ring
	 */
	for (i=0;i<NUM_RX_DESC;i++) {
		
		free_iob( nat->iobuf[i] );
	}
	/* disable interrupts
	 */
	//outl(0,nat->ioaddr + IntrMask) ;
}

/** 
 * Transmit packet
 *
 * @v netdev	Network device
 * @v iobuf	I/O buffer
 * @ret rc	Return status code
 */
static int nat_transmit ( struct net_device *netdev, struct io_buffer *iobuf ) {
	struct natsemi_nic *nat = netdev->priv;

        /* check for space in TX ring
	 */
	if (nat->tx[nat->tx_cur].cmdsts !=0) {
		DBG( "TX overflow\n" );
		return -ENOBUFS;
	}

	/* to be used in netdev_tx_complete
	 */
	nat->tx_iobuf[nat->tx_cur]=iobuf;

	/* Pad and align packet has not been used because its not required here
	 * iob_pad ( iobuf, ETH_ZLEN ); can be used to achieve it
	 */

	/* Add to TX ring
	 */
	DBG ( "TX id %d at %lx+%x\n", nat->tx_cur,
	      virt_to_bus ( &iobuf->data ), iob_len ( iobuf ) );

	nat->tx[nat->tx_cur].bufptr = virt_to_bus(iobuf->data);
	nat->tx[nat->tx_cur].cmdsts= iob_len(iobuf)|OWN;
	/* increment the circular buffer pointer to the next buffer location
	 */
	nat->tx_cur=(nat->tx_cur+1) % TX_RING_SIZE;

	/*start the transmitter 
	 */
        outl(TxOn, nat->ioaddr + ChipCmd);

	return 0;
}

/** 
 * Poll for received packets
 *
 * @v netdev	Network device
 * @v rx_quota	Maximum number of packets to receive
 */
static void nat_poll ( struct net_device *netdev, unsigned int rx_quota ) {
	struct natsemi_nic *nat = netdev->priv;
	unsigned int status;
	unsigned int rx_status;
	unsigned int intr_status;
	unsigned int rx_len;
	struct io_buffer *rx_iob;
	int i;
	
	//outl(1,nat->ioaddr +IntrEnable);
	/* read the interrupt register
	 */
	intr_status=inl(nat->ioaddr+IntrStatus);
	if(!intr_status)
	goto end;

	/* check the status of packets given to card for transmission
	 */	
	DBG("Intr status %X\n",intr_status);

	i=nat->tx_dirty;
	while(i!=nat->tx_cur) {
		status=nat->tx[nat->tx_dirty].cmdsts;
		DBG("value of tx_dirty = %d tx_cur=%d status=%X\n",
			nat->tx_dirty,nat->tx_cur,status);
		
		/* check if current packet has been transmitted or not
		 */
		if(status & OWN) 
			break;
		/* Check if any errors in transmission
		 */
		if (! (status & DescPktOK)) {
			DBG("Error in sending Packet status:%X\n",
					(unsigned int)status);
			netdev_tx_complete_err(netdev,nat->tx_iobuf[nat->tx_dirty],-EINVAL);
		} else {
			DBG("Success in transmitting Packet\n");
			netdev_tx_complete(netdev,nat->tx_iobuf[nat->tx_dirty]);
		}
		/* setting cmdsts zero, indicating that it can be reused 
		 */
		nat->tx[nat->tx_dirty].cmdsts=0;
		nat->tx_dirty=(nat->tx_dirty +1) % TX_RING_SIZE;
		i=(i+1) % TX_RING_SIZE;
	}
	
	/* Handle received packets 
	 */
	rx_status=(unsigned int)nat->rx[nat->rx_cur].cmdsts; 
	while (rx_quota && (rx_status & OWN)) {
		rx_len= (rx_status & DSIZE) - CRC_SIZE;
		/*check for the corrupt packet 
		 */
		if((rx_status & (DescMore|DescPktOK|RxTooLong)) != DescPktOK) {
			 DBG("natsemi_poll: Corrupted packet received, "
					"buffer status = %X ^ %X \n",rx_status,
					(unsigned int) nat->rx[nat->rx_cur].cmdsts);
			 netdev_rx_err(netdev,NULL,-EINVAL);
		} else 	{
			rx_iob = alloc_iob(rx_len);
			if(!rx_iob) 
				/* leave packet for next call to poll
				 */
				goto end;
			memcpy(iob_put(rx_iob,rx_len),
					nat->iobuf[nat->rx_cur]->data,rx_len);
			DBG("received packet\n");
			/* add to the receive queue. 
			 */
			netdev_rx(netdev,rx_iob);
			rx_quota--;
		}
		nat->rx[nat->rx_cur].cmdsts = RX_BUF_SIZE;
		nat->rx_cur=(nat->rx_cur+1) % NUM_RX_DESC;
		rx_status=nat->rx[nat->rx_cur].cmdsts; 
	}
end:
	/* re-enable the potentially idle receive state machine 
	 */
	outl(RxOn, nat->ioaddr + ChipCmd);	
//	outl(1,nat->ioaddr +IntrEnable);
}				

/** RTL8139 net device operations */
static struct net_device_operations nat_operations = {
        .open           = nat_open,
        .close          = nat_close,
        .transmit       = nat_transmit,
        .poll           = nat_poll,
};

/*
 * Probe PCI device
 *
 * @v pci	PCI device
 * @v id	PCI ID
 * @ret rc	Return status code
 */
static int nat_probe ( struct pci_device *pci,
		       const struct pci_device_id *id __unused ) {
	struct net_device *netdev;
	struct natsemi_nic *nat = NULL;
	int rc;
	int i;
	uint8_t ll_addr_encoded[MAX_LL_ADDR_LEN];
	uint8_t last=0;
	uint8_t last1=0;

	/* Allocate net device 
	 */
	netdev = alloc_etherdev ( sizeof ( *nat ) );
	if ( ! netdev ) 
		return -ENOMEM;
	netdev_init(netdev,&nat_operations);
	nat = netdev->priv;
	pci_set_drvdata ( pci, netdev );
	netdev->dev = &pci->dev;
	memset ( nat, 0, sizeof ( *nat ) );
	nat->ioaddr = pci->ioaddr;

	/* Fix up PCI device
	 */
	adjust_pci_device ( pci );

	/* Reset the NIC, set up EEPROM access and read MAC address
	 */
	nat_reset ( nat );
	nat_init_eeprom ( nat );
	nvs_read ( &nat->eeprom.nvs, EE_MAC, ll_addr_encoded, ETH_ALEN );
	
	/* decoding the MAC address read from NVS 
	 * and save it in netdev->ll_addr
         */
	for ( i = 0 ; i < ETH_ALEN ; i+=2 ) {
		last1=ll_addr_encoded[i]>>7;
	 	netdev->ll_addr[i]=ll_addr_encoded[i]<<1|last;
		last=(ll_addr_encoded[i+1]>>7);
		netdev->ll_addr[i+1]=(ll_addr_encoded[i+1]<<1)+last1;
	}

	/* Register network device
	 */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	/* Register non-volatile storage 
	 */
	if ( nat->nvo.nvs ) {
		if ( ( rc = nvo_register ( &nat->nvo ) ) != 0 )
			goto err_register_nvo;
	}
	return 0;

err_register_nvo:
	unregister_netdev ( netdev );
err_register_netdev:
	/* Disable NIC
	 */
	nat_reset ( nat );
	/* Free net device
	 */
	netdev_put ( netdev );
	return rc;
}

/**
 * Remove PCI device
 *
 * @v pci	PCI device
 */
static void nat_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );
	struct natsemi_nic *nat = netdev->priv;
 
	if ( nat->nvo.nvs )
		nvo_unregister ( &nat->nvo );
		
	unregister_netdev ( netdev );
	nat_reset ( nat );
	netdev_put ( netdev );
}

static struct pci_device_id natsemi_nics[] = {
	PCI_ROM(0x100b, 0x0020, "dp83815", "DP83815"),
};

struct pci_driver natsemi_driver __pci_driver = {
	.ids = natsemi_nics,
	.id_count = ( sizeof ( natsemi_nics ) / sizeof ( natsemi_nics[0] ) ),
	.probe = nat_probe,
	.remove = nat_remove,
};
