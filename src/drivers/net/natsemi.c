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
	unsigned short tx_next;
	struct natsemi_tx tx[TX_RING_SIZE];
	struct natsemi_rx rx[NUM_RX_DESC];
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
}


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
 void nat_init_eeprom ( struct natsemi_nic *nat ) {
	int ee9356;
	int vpd;

	/* Initialise three-wire bus */
	nat->spibit.basher.op = &rtl_basher_ops;
	rtl->spibit.bus.mode = SPI_MODE_THREEWIRE;
	init_spi_bit_basher ( &rtl->spibit );

	/* Detect EEPROM type and initialise three-wire device */
	ee9356 = ( inw ( rtl->ioaddr + RxConfig ) & Eeprom9356 );
	if ( ee9356 ) {
		DBG ( "EEPROM is an AT93C56\n" );
		init_at93c56 ( &rtl->eeprom, 16 );
	} else {
		DBG ( "EEPROM is an AT93C46\n" );
		init_at93c46 ( &rtl->eeprom, 16 );
	}
	rtl->eeprom.bus = &rtl->spibit.bus;

	/* Initialise space for non-volatile options, if available */
	vpd = ( inw ( rtl->ioaddr + Config1 ) & VPDEnable );
	if ( vpd ) {
		DBG ( "EEPROM in use for VPD; cannot use for options\n" );
	} else {
		rtl->nvo.nvs = &rtl->eeprom.nvs;
		rtl->nvo.fragments = rtl_nvo_fragments;
	}
}

/**
 * Reset NIC
 *
 * @v rtl		NATSEMI NIC
 *
 * Issues a hardware reset and waits for the reset to complete.
 */
static void nat_reset ( struct nat_nic *nat ) {

	/* Reset chip */
	outb ( ChipReset, nat->ioaddr + ChipCmd );
	mdelay ( 10 );
	memset ( &nat->tx, 0, sizeof ( nat->tx ) );
	nat->rx.offset = 0;

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
	struct io_buffer *iobuf;
	int i;
	
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
	/* Set up RX ring */

	for (i=0;i<NUM_RX_DESC;i++)
	{

		iobuf = alloc_iob ( RX_BUF_SIZE );
		if (!iobuf)
		       return -ENOMEM;	
		nat->rx[i].link   = virt_to_bus((i+1 < NUM_RX_DESC) ? &nat->rx[i+1] : &nat->rx[0]);
		nat->rx[i].cmdsts = (u32) RX_BUF_SIZE;
		nat->rx[i].bufptr = virt_to_bus(iobuf->data);
	}


	 /* load Receive Descriptor Register */
	outl(virt_to_bus(&nat->rx[0]), ioaddr + RxRingPtr);
	DBG("Natsemi Rx descriptor loaded with: %X\n",inl(nat->ioaddr+RingPtr));		

	/* setup Tx ring */
	outl(virt_to_bus(&nat->tx[0]),nat->ioaddr+TxRingPtr);
	DBG("Natsemi Tx descriptor loaded with: %X\n",inl(nat->ioaddr+TxRingPtr));

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



	/*start the receiver and transmitter */
        outl(RxOn|TxOn, nat->ioaddr + ChipCmd);


	return 0;
}

/**
 * Close NIC
 *
 * @v netdev		Net device
 */
static void rtl_close ( struct net_device *netdev ) {
	struct rtl8139_nic *rtl = netdev->priv;

	/* Reset the hardware to disable everything in one go */
	rtl_reset ( rtl );

	/* Free RX ring */
	free ( rtl->rx.ring );
	rtl->rx.ring = NULL;
}

/** 
 * Transmit packet
 *
 * @v netdev	Network device
 * @v iobuf	I/O buffer
 * @ret rc	Return status code
 */
static int natsemi_transmit ( struct net_device *netdev, struct io_buffer *iobuf ) {
	struct natsemi_nic *nat = netdev->priv;

	/* Check for space in TX ring */
	if ( nat->tx.iobuf[nat->tx.next] != NULL ) {
		printf ( "TX overflow\n" );
		return -ENOBUFS;
	}

	/* Pad and align packet */
	iob_pad ( iobuf, ETH_ZLEN );

	/* Add to TX ring */
	DBG ( "TX id %d at %lx+%x\n", rtl->tx.next,
	      virt_to_bus ( iobuf->data ), iob_len ( iobuf ) );
	rtl->tx.iobuf[rtl->tx.next] = iobuf;
	outl ( virt_to_bus ( iobuf->data ),
	       rtl->ioaddr + TxAddr0 + 4 * rtl->tx.next );
	outl ( ( ( ( TX_FIFO_THRESH & 0x7e0 ) << 11 ) | iob_len ( iobuf ) ),
	       rtl->ioaddr + TxStatus0 + 4 * rtl->tx.next );
	rtl->tx.next = ( rtl->tx.next + 1 ) % TX_RING_SIZE;

	return 0;
}

/** 
 * Poll for received packets
 *
 * @v netdev	Network device
 * @v rx_quota	Maximum number of packets to receive
 */
static void rtl_poll ( struct net_device *netdev, unsigned int rx_quota ) {
	struct rtl8139_nic *rtl = netdev->priv;
	unsigned int status;
	unsigned int tsad;
	unsigned int rx_status;
	unsigned int rx_len;
	struct io_buffer *rx_iob;
	int wrapped_len;
	int i;

	/* Acknowledge interrupts */
	status = inw ( rtl->ioaddr + IntrStatus );
	if ( ! status )
		return;
	outw ( status, rtl->ioaddr + IntrStatus );

	/* Handle TX completions */
	tsad = inw ( rtl->ioaddr + TxSummary );
	for ( i = 0 ; i < TX_RING_SIZE ; i++ ) {
		if ( ( rtl->tx.iobuf[i] != NULL ) && ( tsad & ( 1 << i ) ) ) {
			DBG ( "TX id %d complete\n", i );
			netdev_tx_complete ( netdev, rtl->tx.iobuf[i] );
			rtl->tx.iobuf[i] = NULL;
		}
	}

	/* Handle received packets */
	while ( rx_quota && ! ( inw ( rtl->ioaddr + ChipCmd ) & RxBufEmpty ) ){
		rx_status = * ( ( uint16_t * )
				( rtl->rx.ring + rtl->rx.offset ) );
		rx_len = * ( ( uint16_t * )
			     ( rtl->rx.ring + rtl->rx.offset + 2 ) );
		if ( rx_status & RxOK ) {
			DBG ( "RX packet at offset %x+%x\n", rtl->rx.offset,
			      rx_len );

			rx_iob = alloc_iob ( rx_len );
			if ( ! rx_iob ) {
				/* Leave packet for next call to poll() */
				break;
			}

			wrapped_len = ( ( rtl->rx.offset + 4 + rx_len )
					- RX_BUF_LEN );
			if ( wrapped_len < 0 )
				wrapped_len = 0;

			memcpy ( iob_put ( rx_iob, rx_len - wrapped_len ),
				 rtl->rx.ring + rtl->rx.offset + 4,
				 rx_len - wrapped_len );
			memcpy ( iob_put ( rx_iob, wrapped_len ),
				 rtl->rx.ring, wrapped_len );

			netdev_rx ( netdev, rx_iob );
			rx_quota--;
		} else {
			DBG ( "RX bad packet (status %#04x len %d)\n",
			      rx_status, rx_len );
		}
		rtl->rx.offset = ( ( ( rtl->rx.offset + 4 + rx_len + 3 ) & ~3 )
				   % RX_BUF_LEN );
		outw ( rtl->rx.offset - 16, rtl->ioaddr + RxBufPtr );
	}
}

#if 0
static void rtl_irq(struct nic *nic, irq_action_t action)
{
	unsigned int mask;
	/* Bit of a guess as to which interrupts we should allow */
	unsigned int interested = ROK | RER | RXOVW | FOVW | SERR;

	switch ( action ) {
	case DISABLE :
	case ENABLE :
		mask = inw(rtl->ioaddr + IntrMask);
		mask = mask & ~interested;
		if ( action == ENABLE ) mask = mask | interested;
		outw(mask, rtl->ioaddr + IntrMask);
		break;
	case FORCE :
		/* Apparently writing a 1 to this read-only bit of a
		 * read-only and otherwise unrelated register will
		 * force an interrupt.  If you ever want to see how
		 * not to write a datasheet, read the one for the
		 * RTL8139...
		 */
		outb(EROK, rtl->ioaddr + RxEarlyStatus);
		break;
	}
}
#endif

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
		nat_reset ( rtl );
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
static void rtl_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );
	struct rtl8139_nic *rtl = netdev->priv;

	if ( rtl->nvo.nvs )
		nvo_unregister ( &rtl->nvo );
	unregister_netdev ( netdev );
	rtl_reset ( rtl );
	free_netdev ( netdev );
}

static struct pci_device_id rtl8139_nics[] = {
PCI_ROM(0x10ec, 0x8129, "rtl8129",       "Realtek 8129"),
PCI_ROM(0x10ec, 0x8139, "rtl8139",       "Realtek 8139"),
PCI_ROM(0x10ec, 0x8138, "rtl8139b",      "Realtek 8139B"),
PCI_ROM(0x1186, 0x1300, "dfe538",        "DFE530TX+/DFE538TX"),
PCI_ROM(0x1113, 0x1211, "smc1211-1",     "SMC EZ10/100"),
PCI_ROM(0x1112, 0x1211, "smc1211",       "SMC EZ10/100"),
PCI_ROM(0x1500, 0x1360, "delta8139",     "Delta Electronics 8139"),
PCI_ROM(0x4033, 0x1360, "addtron8139",   "Addtron Technology 8139"),
PCI_ROM(0x1186, 0x1340, "dfe690txd",     "D-Link DFE690TXD"),
PCI_ROM(0x13d1, 0xab06, "fe2000vx",      "AboCom FE2000VX"),
PCI_ROM(0x1259, 0xa117, "allied8139",    "Allied Telesyn 8139"),
PCI_ROM(0x14ea, 0xab06, "fnw3603tx",     "Planex FNW-3603-TX"),
PCI_ROM(0x14ea, 0xab07, "fnw3800tx",     "Planex FNW-3800-TX"),
PCI_ROM(0xffff, 0x8139, "clone-rtl8139", "Cloned 8139"),
};

struct pci_driver rtl8139_driver __pci_driver = {
	.ids = rtl8139_nics,
	.id_count = ( sizeof ( rtl8139_nics ) / sizeof ( rtl8139_nics[0] ) ),
	.probe = rtl_probe,
	.remove = rtl_remove,
};
