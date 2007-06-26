/* natsemi.c - gPXE driver for the NatSemi DP8381x series. 
 

*/

#include <stdint.h>
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
	 * give the same.*/
	struct io_buffer *iobuf[NUM_RX_DESC];
	struct spi_bit_basher spibit;
	struct spi_device eeprom;
	struct nvo_block nvo;
};

/* Tuning Parameters */
#define TX_FIFO_THRESH	256	/* In bytes, rounded down to 32 byte units. */
#define RX_FIFO_THRESH	4	/* Rx buffer level before first PCI xfer.  */
#define RX_DMA_BURST	4	/* Maximum PCI burst, '4' is 256 bytes */
#define TX_DMA_BURST	4	/* Calculate as 16<<val. */
#define TX_IPG		3	/* This is the only valid value */
//#define RX_BUF_LEN_IDX	0	/*  */
#define RX_BUF_LEN    8192   /*buffer size should be multiple of 32 */ 
#define RX_BUF_PAD 4
#define RX_BUF_SIZE 1536
#define OWN       0x80000000
#define DSIZE     0x00000FFF
#define CRC_SIZE  4

/* NATSEMI: Offsets to the device registers.
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
    SDCFG        = 0x8C,
    BasicControl = 0x80,	
    BasicStatus  = 0x84
	    
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

/* Bits in network_desc.status */
enum desc_status_bits {
    DescOwn   = 0x80000000, 
    DescMore  = 0x40000000, 
    DescIntr  = 0x20000000,
    DescNoCRC = 0x10000000,
    DescPktOK = 0x08000000, 
    RxTooLong = 0x00400000
};



/*  EEPROM access */
#define EE_M1		0x80	/* Mode select bit 1 */
#define EE_M0		0x40	/* Mode select bit 0 */
#define EE_CS		0x08	/* EEPROM chip select */
#define EE_SK		0x04	/* EEPROM shift clock */
#define EE_DI		0x02	/* Data in */
#define EE_DO		0x01	/* Data out */

/* Offsets within EEPROM (these are word offsets) */
#define EE_MAC 7

static uint32_t SavedClkRun;	


/* TODO
static const uint8_t rtl_ee_bits[] = {
	[SPI_BIT_SCLK]	= EE_SK,
	[SPI_BIT_MOSI]	= EE_DI,
	[SPI_BIT_MISO]	= EE_DO,
	[SPI_BIT_SS(0)]	= ( EE_CS | EE_M1 ),
};

static int rtl_spi_read_bit ( struct bit_basher *basher,
			      unsigned int bit_id ) {
	struct rtl8139_nic *rtl = container_of ( basher, struct rtl8139_nic,
						 spibit.basher );
	uint8_t mask = rtl_ee_bits[bit_id];
	uint8_t eereg;

	eereg = inb ( rtl->ioaddr + Cfg9346 );
	return ( eereg & mask );
}

static void rtl_spi_write_bit ( struct bit_basher *basher,
				unsigned int bit_id, unsigned long data ) {
	struct rtl8139_nic *rtl = container_of ( basher, struct rtl8139_nic,
						 spibit.basher );
	uint8_t mask = rtl_ee_bits[bit_id];
	uint8_t eereg;

	eereg = inb ( rtl->ioaddr + Cfg9346 );
	eereg &= ~mask;
	eereg |= ( data & mask );
	outb ( eereg, rtl->ioaddr + Cfg9346 );
}

static struct bit_basher_operations rtl_basher_ops = {
	.read = rtl_spi_read_bit,
	.write = rtl_spi_write_bit,
};
*/
/** Portion of EEPROM available for non-volatile stored options
 *
 * We use offset 0x40 (i.e. address 0x20), length 0x40.  This block is
 * marked as VPD in the rtl8139 datasheets, so we use it only if we
 * detect that the card is not supporting VPD.
 */
static struct nvo_fragment rtl_nvo_fragments[] = {
	{ 0x20, 0x40 },
	{ 0, 0 }
};

/**
 * Set up for EEPROM access
 *
 * @v NAT		NATSEMI NIC
 */
/* TODO
 void rtl_init_eeprom ( struct natsemi_nic *rtl ) {
	int ee9356;
	int vpd;

	// Initialise three-wire bus 
	rtl->spibit.basher.op = &rtl_basher_ops;
	rtl->spibit.bus.mode = SPI_MODE_THREEWIRE;
	init_spi_bit_basher ( &rtl->spibit );

	//Detect EEPROM type and initialise three-wire device 
	ee9356 = ( inw ( rtl->ioaddr + RxConfig ) & Eeprom9356 );
	if ( ee9356 ) {
		DBG ( "EEPROM is an AT93C56\n" );
		init_at93c56 ( &rtl->eeprom, 16 );
	} else {
		DBG ( "EEPROM is an AT93C46\n" );
		init_at93c46 ( &rtl->eeprom, 16 );
	}
	rtl->eeprom.bus = &rtl->spibit.bus;

	// Initialise space for non-volatile options, if available 
	vpd = ( inw ( rtl->ioaddr + Config1 ) & VPDEnable );
	if ( vpd ) {
		DBG ( "EEPROM in use for VPD; cannot use for options\n" );
	} else {
		rtl->nvo.nvs = &rtl->eeprom.nvs;
		rtl->nvo.fragments = rtl_nvo_fragments;
	}
}
*/
/**
 * Reset NIC
 *
 * @v		NATSEMI NIC
 *
 * Issues a hardware reset and waits for the reset to complete.
 */
static void nat_reset ( struct natsemi_nic *nat ) {

	int i;
	/* Reset chip */
	outl ( ChipReset, nat->ioaddr + ChipCmd );
	mdelay ( 10 );
	nat->tx_dirty=0;
	nat->tx_cur=0;
	for(i=0;i<TX_RING_SIZE;i++)
	{
		nat->tx[i].link=0;
		nat->tx[i].cmdsts=0;
		nat->tx[i].bufptr=0;
	}
	nat->rx_cur = 0;
	outl(virt_to_bus(&nat->tx[0]),nat->ioaddr+TxRingPtr);
	outl(virt_to_bus(&nat->rx[0]), nat->ioaddr + RxRingPtr);

	outl(TxOff|RxOff, nat->ioaddr + ChipCmd);

	/* Restore PME enable bit */
	outl(SavedClkRun, nat->ioaddr + ClkRun);
}

/**
 * Open NIC
 *
 * @v netdev		Net device
 * @ret rc		Return status code
 */
static int nat_open ( struct net_device *netdev ) {
	struct natsemi_nic *nat = netdev->priv;
	//struct io_buffer *iobuf;
	int i;
	uint32_t tx_config,rx_config;
	
	/* Disable PME:
        * The PME bit is initialized from the EEPROM contents.
        * PCI cards probably have PME disabled, but motherboard
        * implementations may have PME set to enable WakeOnLan. 
        * With PME set the chip will scan incoming packets but
        * nothing will be written to memory. */
        SavedClkRun = inl(nat->ioaddr + ClkRun);
        outl(SavedClkRun & ~0x100, nat->ioaddr + ClkRun);

		


	/* Program the MAC address TODO enable this comment */
	/*
	 for ( i = 0 ; i < ETH_ALEN ; i++ )
		outb ( netdev->ll_addr[i], rtl->ioaddr + MAC0 + i );
        */


	/*Set up the Tx Ring */
	nat->tx_cur=0;
	nat->tx_dirty=0;
	for (i=0;i<TX_RING_SIZE;i++)
	{
		nat->tx[i].link   = virt_to_bus((i+1 < TX_RING_SIZE) ? &nat->tx[i+1] : &nat->tx[0]);
		nat->tx[i].cmdsts = 0;
		nat->tx[i].bufptr = 0;
	}





	/* Set up RX ring */
	nat->rx_cur=0;
	for (i=0;i<NUM_RX_DESC;i++)
	{

		nat->iobuf[i] = alloc_iob ( RX_BUF_SIZE );
		if (!nat->iobuf[i])
		       return -ENOMEM;	
		nat->rx[i].link   = virt_to_bus((i+1 < NUM_RX_DESC) ? &nat->rx[i+1] : &nat->rx[0]);
		nat->rx[i].cmdsts = (uint32_t) RX_BUF_SIZE;
		nat->rx[i].bufptr = virt_to_bus(nat->iobuf[i]->data);
	}


	 /* load Receive Descriptor Register */
	outl(virt_to_bus(&nat->rx[0]), nat->ioaddr + RxRingPtr);
	DBG("Natsemi Rx descriptor loaded with: %X\n",(unsigned int)inl(nat->ioaddr+RxRingPtr));		

	/* setup Tx ring */
	outl(virt_to_bus(&nat->tx[0]),nat->ioaddr+TxRingPtr);
	DBG("Natsemi Tx descriptor loaded with: %X\n",(unsigned int)inl(nat->ioaddr+TxRingPtr));

	/* Enables RX */
	outl(RxFilterEnable|AcceptBroadcast|AcceptAllMulticast|AcceptMyPhys, nat->ioaddr+RxFilterAddr);

	/* Initialize other registers. */
	/* Configure the PCI bus bursts and FIFO thresholds. */
	/* Configure for standard, in-spec Ethernet. */
	if (inl(nat->ioaddr + ChipConfig) & 0x20000000) {	/* Full duplex */
		tx_config = 0xD0801002;
		rx_config = 0x10000020;
	} else {
		tx_config = 0x10801002;
		rx_config = 0x0020;
	}
	outl(tx_config, nat->ioaddr + TxConfig);
	outl(rx_config, nat->ioaddr + RxConfig);



	/*start the receiver  */
        outl(RxOn, nat->ioaddr + ChipCmd);


	return 0;
}

/**
 * Close NIC
 *
 * @v netdev		Net device
 */
static void nat_close ( struct net_device *netdev ) {
	struct natsemi_nic *nat = netdev->priv;
	int i;


	/* Reset the hardware to disable everything in one go */
	nat_reset ( nat );

	/* Free RX ring */
	for (i=0;i<NUM_RX_DESC;i++)
	{
		
		free_iob( nat->iobuf[i] );
	}
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

       /* check for space in TX ring */

	if (nat->tx[nat->tx_cur].cmdsts !=0)
	{
		printf ( "TX overflow\n" );
		return -ENOBUFS;
	}


	/* Pad and align packet */
	iob_pad ( iobuf, ETH_ZLEN );

	/* Add to TX ring */
	DBG ( "TX id %d at %lx+%x\n", nat->tx_cur,
	      virt_to_bus ( iobuf->data ), iob_len ( iobuf ) );

	nat->tx[nat->tx_cur].bufptr = virt_to_bus(iobuf->data);
	nat->tx[nat->tx_cur].cmdsts= (uint32_t) iob_len(iobuf)|OWN;

	nat->tx_cur=(nat->tx_cur+1) % TX_RING_SIZE;

	/*start the transmitter  */
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
	uint32_t status;
	unsigned int rx_status;
	unsigned int rx_len;
	struct io_buffer *rx_iob;
	int i;

	
	/* check the status of packets given to card for transmission */	
	for ( i = 0 ; i < TX_RING_SIZE ; i++ ) 
	{
		//status=(uint32_t)bus_to_virt(nat->tx[nat->tx_dirty].cmdsts);
		status=(uint32_t)nat->tx[nat->tx_dirty].cmdsts;
		/* check if current packet has been transmitted or not */
		if(status & OWN) 
			break;
		/* Check if any errors in transmission */
		if (! (status & DescPktOK))
		{
			printf("Error in sending Packet with data: %s\n and status:%X\n",
					(char *)nat->tx[nat->tx_dirty].bufptr,(unsigned int)status);
		}
		else
		{
			DBG("Success in transmitting Packet with data: %s",
				(char *)nat->tx[nat->tx_dirty].bufptr);
		}
		/* setting cmdsts zero, indicating that it can be reused */
		nat->tx[nat->tx_dirty].cmdsts=0;
		nat->tx_dirty=(nat->tx_dirty +1) % TX_RING_SIZE;
	}
			
	
	//rx_status=(unsigned int)bus_to_virt(nat->rx[nat->rx_cur].cmdsts); 
	rx_status=(unsigned int)nat->rx[nat->rx_cur].cmdsts; 
	/* Handle received packets */
	while (rx_quota && (rx_status & OWN))
	{
		rx_len= (rx_status & DSIZE) - CRC_SIZE;

		/*check for the corrupt packet */
		if((rx_status & (DescMore|DescPktOK|RxTooLong)) != DescPktOK)
		{
			 printf("natsemi_poll: Corrupted packet received, "
					"buffer status = %X ^ %X \n",rx_status,
					(unsigned int) nat->rx[nat->rx_cur].cmdsts);
		}
		else
		{
			rx_iob = alloc_iob(rx_len);
			if(!rx_iob) 
				/* leave packet for next call to poll*/
				return;
			memcpy(iob_put(rx_iob,rx_len),
					nat->rx[nat->rx_cur].bufptr,rx_len);
			/* add to the receive queue. */
			netdev_rx(netdev,rx_iob);
			rx_quota--;
		}
		nat->rx[nat->rx_cur].cmdsts = RX_BUF_SIZE;
		nat->rx_cur=(nat->rx_cur+1) % NUM_RX_DESC;
		//rx_status=(unsigned int)bus_to_virt(nat->rx[nat->rx_cur].cmdsts); 
		rx_status=(unsigned int)nat->rx[nat->rx_cur].cmdsts; 
	}


	 /* re-enable the potentially idle receive state machine */
	    outl(RxOn, nat->ioaddr + ChipCmd);	
}				






/**
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
	int registered_netdev = 0;
	int rc;
	uint32_t advertising;

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Allocate net device */
	netdev = alloc_etherdev ( sizeof ( *nat ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err;
	}
	nat = netdev->priv;
	pci_set_drvdata ( pci, netdev );
	netdev->dev = &pci->dev;
	memset ( nat, 0, sizeof ( *nat ) );
	nat->ioaddr = pci->ioaddr;

	/* Reset the NIC, set up EEPROM access and read MAC address */
	nat_reset ( nat );
	/* commenitng two line below. Have to be included in final natsemi.c TODO*/
	/*
	nat_init_eeprom ( rtl );
	nvs_read ( &nat->eeprom.nvs, EE_MAC, netdev->ll_addr, ETH_ALEN );
	
	*/
	

	/* mdio routine of etherboot-5.4.0 natsemi driver has been removed and 
	 * statement to read from MII transceiver control section is used directly
	 */

        advertising = inl(nat->ioaddr + 0x80 + (4<<2)) & 0xffff; 
        {
	   	uint32_t chip_config = inl(nat->ioaddr + ChipConfig);
		DBG("%s: Transceiver default autoneg. %s 10 %s %s duplex.\n",
	      	pci->driver_name,
	        chip_config & 0x2000 ? "enabled, advertise" : "disabled, force",
	        chip_config & 0x4000 ? "0" : "",
	        chip_config & 0x8000 ? "full" : "half");
    	}
	DBG("%s: Transceiver status %hX advertising %hX\n",pci->driver_name, (int)inl(nat->ioaddr + 0x84),(unsigned int) advertising);





	/* Point to NIC specific routines */
	netdev->open	 = nat_open;
	netdev->close	 = nat_close;
	netdev->transmit = nat_transmit;
	netdev->poll	 = nat_poll;

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err;
	registered_netdev = 1;

	/* Register non-volatile storagei
	 * uncomment lines below in final version*/
	/*
	 if ( rtl->nvo.nvs ) {
		if ( ( rc = nvo_register ( &rtl->nvo ) ) != 0 )
			goto err;
	}
	*/

	return 0;

 err:
	/* Disable NIC */
	if ( nat )
		nat_reset ( nat );
	if ( registered_netdev )
		unregister_netdev ( netdev );
	/* Free net device */
	free_netdev ( netdev );
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
/* TODO 
	if ( rtl->nvo.nvs )
		nvo_unregister ( &rtl->nvo );
		*/
	unregister_netdev ( netdev );
	nat_reset ( nat );
	free_netdev ( netdev );
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
