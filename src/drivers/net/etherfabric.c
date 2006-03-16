/**************************************************************************
 *
 * Etherboot driver for Level 5 Etherfabric network cards
 *
 * Written by Michael Brown <mbrown@fensystems.co.uk>
 *
 * Copyright Fen Systems Ltd. 2005
 * Copyright Level 5 Networks Inc. 2005
 *
 * This software may be used and distributed according to the terms of
 * the GNU General Public License (GPL), incorporated herein by
 * reference.  Drivers based on or derived from this code fall under
 * the GPL and must retain the authorship, copyright and license
 * notice.
 *
 **************************************************************************
 */

#include "etherboot.h"
#include "nic.h"
#include "pci.h"
#include "timer.h"
#define dma_addr_t unsigned long
#include "etherfabric.h"

/**************************************************************************
 *
 * Constants and macros
 *
 **************************************************************************
 */

#define DBG(...)

#define EFAB_ASSERT(x)                                                        \
        do {                                                                  \
                if ( ! (x) ) {                                                \
                        DBG ( "ASSERT(%s) failed at %s line %d [%s]\n", #x,   \
                              __FILE__, __LINE__, __FUNCTION__ );             \
                }                                                             \
        } while (0)

#define EFAB_TRACE(...)

#define EFAB_REGDUMP(...)

#define FALCON_USE_IO_BAR 1

/*
 * EtherFabric constants 
 *
 */

/* PCI Definitions */
#define EFAB_VENDID_LEVEL5	0x1924
#define FALCON_P_DEVID		0x0703  /* Temporary PCI ID */
#define EF1002_DEVID		0xC101

/**************************************************************************
 *
 * Data structures
 *
 **************************************************************************
 */

/*
 * Buffers used for TX, RX and event queue
 *
 */
#define EFAB_BUF_ALIGN		4096
#define EFAB_DATA_BUF_SIZE	2048
#define EFAB_RX_BUFS		16
#define EFAB_RXD_SIZE		512
#define EFAB_TXD_SIZE		512
#define EFAB_EVQ_SIZE		512
struct efab_buffers {
	uint8_t eventq[4096];
	uint8_t rxd[4096];
	uint8_t txd[4096];
	uint8_t tx_buf[EFAB_DATA_BUF_SIZE];
	uint8_t rx_buf[EFAB_RX_BUFS][EFAB_DATA_BUF_SIZE];
	uint8_t padding[EFAB_BUF_ALIGN-1];
};
static struct efab_buffers efab_buffers;

/** An RX buffer */
struct efab_rx_buf {
	uint8_t *addr;
	unsigned int len;
	int id;
};

/** A TX buffer */
struct efab_tx_buf {
	uint8_t *addr;
	unsigned int len;
	int id;
};

/** Etherfabric event type */
enum efab_event_type {
	EFAB_EV_NONE = 0,
	EFAB_EV_TX,
	EFAB_EV_RX,
};

/** Etherfabric event */
struct efab_event {
	/** Event type */
	enum efab_event_type type;
	/** RX buffer ID */
	int rx_id;
	/** RX length */
	unsigned int rx_len;
};

/*
 * Etherfabric abstraction layer
 *
 */
struct efab_nic;
struct efab_operations {
	void ( * get_membase ) ( struct efab_nic *efab );
	int ( * reset ) ( struct efab_nic *efab );
	int ( * init_nic ) ( struct efab_nic *efab );
	int ( * read_eeprom ) ( struct efab_nic *efab );
	void ( * build_rx_desc ) ( struct efab_nic *efab,
				   struct efab_rx_buf *rx_buf );
	void ( * notify_rx_desc ) ( struct efab_nic *efab );
	void ( * build_tx_desc ) ( struct efab_nic *efab,
				   struct efab_tx_buf *tx_buf );
	void ( * notify_tx_desc ) ( struct efab_nic *efab );
	int ( * fetch_event ) ( struct efab_nic *efab,
				struct efab_event *event );
	void ( * mask_irq ) ( struct efab_nic *efab, int enabled );
	void ( * generate_irq ) ( struct efab_nic *efab );
	void ( * mac_writel ) ( struct efab_nic *efab, efab_dword_t *value,
				unsigned int mac_reg );
	void ( * mac_readl ) ( struct efab_nic *efab, efab_dword_t *value,
			       unsigned int mac_reg );
	int ( * init_mac ) ( struct efab_nic *efab );
	void ( * mdio_write ) ( struct efab_nic *efab, int location,
				int value );
	int ( * mdio_read ) ( struct efab_nic *efab, int location );
};

/*
 * Driver private data structure
 *
 */
struct efab_nic {

	/** PCI device */
	struct pci_device *pci;

	/** Operations table */
	struct efab_operations *op;

	/** Memory base */
	void *membase;

	/** I/O base */
	unsigned int iobase;

	/** Buffers */
	uint8_t *eventq;		/* Falcon only */
	uint8_t *txd;			/* Falcon only */
	uint8_t *rxd;			/* Falcon only */
	struct efab_tx_buf tx_buf;
	struct efab_rx_buf rx_bufs[EFAB_RX_BUFS];

	/** Buffer pointers */
	unsigned int eventq_read_ptr;	/* Falcon only */
	unsigned int tx_write_ptr;
	unsigned int rx_write_ptr;
	int tx_in_progress;

	/** Port 0/1 on the NIC */
	int port;

	/** MAC address */
	uint8_t mac_addr[ETH_ALEN];
	/** GMII link options */
	unsigned int link_options;
	/** Link status */
	int link_up;

	/** INT_REG_KER for Falcon */
	efab_oword_t int_ker __attribute__ (( aligned ( 16 ) ));
};

/**************************************************************************
 *
 * EEPROM access
 *
 **************************************************************************
 */

#define EFAB_EEPROM_SDA		0x80000000u
#define EFAB_EEPROM_SCL		0x40000000u
#define ARIZONA_24xx00_SLAVE	0xa0
#define EFAB_EEPROM_READ_SELECT	( ARIZONA_24xx00_SLAVE | 1 )
#define EFAB_EEPROM_WRITE_SELECT ( ARIZONA_24xx00_SLAVE | 0 )

static void eeprom_release ( uint32_t *eeprom_reg ) {
	unsigned int dev;

	udelay ( 10 );
	dev = readl ( eeprom_reg );
	writel ( dev | ( EFAB_EEPROM_SDA | EFAB_EEPROM_SCL ),
		 eeprom_reg );
	udelay ( 10 );
}

static void eeprom_start ( uint32_t *eeprom_reg ) {
	unsigned int dev;
	
	udelay ( 10 );
	dev = readl ( eeprom_reg );
	
	if ( ( dev & ( EFAB_EEPROM_SDA | EFAB_EEPROM_SCL ) ) !=
	     ( EFAB_EEPROM_SDA | EFAB_EEPROM_SCL ) ) {
		udelay ( 10 );
		writel ( dev | ( EFAB_EEPROM_SDA | EFAB_EEPROM_SCL ),
			 eeprom_reg );
		udelay ( 1 );
	}
	dev &=~ ( EFAB_EEPROM_SDA | EFAB_EEPROM_SCL );
	
	udelay ( 10 );
	writel ( dev | EFAB_EEPROM_SCL, eeprom_reg) ;
	udelay ( 1) ;

	udelay ( 10 );
	writel ( dev, eeprom_reg );
	udelay ( 10 );
}

static void eeprom_stop ( uint32_t *eeprom_reg ) { 
	unsigned int dev;
	
	udelay ( 10 );
	dev = readl ( eeprom_reg );
	EFAB_ASSERT ( ! ( dev & EFAB_EEPROM_SCL ) );
	
	if ( dev & ( EFAB_EEPROM_SDA | EFAB_EEPROM_SCL ) ) {
		dev &=~ ( EFAB_EEPROM_SDA | EFAB_EEPROM_SCL );
		udelay ( 10 );
		writel ( dev, eeprom_reg );
		udelay ( 10 );
	}
	
	udelay ( 10 );
	dev |= EFAB_EEPROM_SCL;
	writel ( dev, eeprom_reg );
	udelay ( 10 );
	
	udelay ( 10 );
	dev |= EFAB_EEPROM_SDA;
	writel ( dev, eeprom_reg );
	udelay ( 10 );
}

static void eeprom_write ( uint32_t *eeprom_reg, unsigned char data ) {
	int i;
	unsigned int dev;
	
	udelay ( 10 );
	dev = readl ( eeprom_reg );
	udelay ( 10 );
	EFAB_ASSERT ( ! ( dev & EFAB_EEPROM_SCL ) );
	
	for ( i = 0 ; i < 8 ; i++, data <<= 1 ) {
		if ( data & 0x80 ) {
			dev |=  EFAB_EEPROM_SDA;
		} else {
			dev &=~ EFAB_EEPROM_SDA;
		}
		udelay ( 10 );
		writel ( dev, eeprom_reg );
		udelay ( 10 );
		
		udelay ( 10 );
		writel ( dev | EFAB_EEPROM_SCL, eeprom_reg );
		udelay ( 10 );
		
		udelay ( 10 );
		writel ( dev, eeprom_reg );
		udelay ( 10 );
	}
	
	if( ! ( dev & EFAB_EEPROM_SDA ) ) {
		udelay ( 10 );
		writel ( dev | EFAB_EEPROM_SDA, eeprom_reg );
		udelay ( 10 );
	}
}

static unsigned char eeprom_read ( uint32_t *eeprom_reg ) {
	unsigned int i, dev, rd;
	unsigned char val = 0;
	
	udelay ( 10 );
	dev = readl ( eeprom_reg );
	udelay ( 10 );
	EFAB_ASSERT ( ! ( dev & EFAB_EEPROM_SCL ) );
	
	if( ! ( dev & EFAB_EEPROM_SDA ) ) {
		dev |= EFAB_EEPROM_SDA;
		udelay ( 10 );
		writel ( dev, eeprom_reg );
		udelay ( 10 );
	}
	
	for( i = 0 ; i < 8 ; i++ ) {
		udelay ( 10 );
		writel ( dev | EFAB_EEPROM_SCL, eeprom_reg );
		udelay ( 10 );
		
		udelay ( 10 );
		rd = readl ( eeprom_reg );
		udelay ( 10 );
		val = ( val << 1 ) | ( ( rd & EFAB_EEPROM_SDA ) != 0 );
		
		udelay ( 10 );
		writel ( dev, eeprom_reg );
		udelay ( 10 );
	}

	return val;
}

static int eeprom_check_ack ( uint32_t *eeprom_reg ) {
	int ack;
	unsigned int dev;
	
	udelay ( 10 );
	dev = readl ( eeprom_reg );
	EFAB_ASSERT ( ! ( dev & EFAB_EEPROM_SCL ) );
	
	writel ( dev | EFAB_EEPROM_SCL, eeprom_reg );
	udelay ( 10 );
	
	udelay ( 10 );
	ack = readl ( eeprom_reg ) & EFAB_EEPROM_SDA;
	
	udelay ( 10 );
	writel ( ack & ~EFAB_EEPROM_SCL, eeprom_reg );
	udelay ( 10 );
	
	return ( ack == 0 );
}

static void eeprom_send_ack ( uint32_t *eeprom_reg ) {
	unsigned int dev;
	
	udelay ( 10 );
	dev = readl ( eeprom_reg );
	EFAB_ASSERT ( ! ( dev & EFAB_EEPROM_SCL ) );
	
	udelay ( 10 );
	dev &= ~EFAB_EEPROM_SDA;	
	writel ( dev, eeprom_reg );
	udelay ( 10 );
	
	udelay ( 10 );
	dev |= EFAB_EEPROM_SCL;     
	writel ( dev, eeprom_reg );
	udelay ( 10 );
	
	udelay ( 10 );
	dev |= EFAB_EEPROM_SDA;	
	writel ( dev & ~EFAB_EEPROM_SCL, eeprom_reg );
	udelay ( 10 );
}

static int efab_eeprom_read_mac ( uint32_t *eeprom_reg, uint8_t *mac_addr ) {
	int i;

	eeprom_start ( eeprom_reg );

	eeprom_write ( eeprom_reg, EFAB_EEPROM_WRITE_SELECT );
	if ( ! eeprom_check_ack ( eeprom_reg ) )
		return 0;
	
	eeprom_write ( eeprom_reg, 0 );
	if ( ! eeprom_check_ack ( eeprom_reg ) )
		return 0;
	
	eeprom_stop ( eeprom_reg );
	eeprom_start ( eeprom_reg );
	
	eeprom_write ( eeprom_reg, EFAB_EEPROM_READ_SELECT );
	if ( ! eeprom_check_ack ( eeprom_reg ) )
		return 0;
	
	for ( i = 0 ; i < ETH_ALEN ; i++ ) {
		mac_addr[i] = eeprom_read ( eeprom_reg );
		eeprom_send_ack ( eeprom_reg );
	}
	
	eeprom_stop ( eeprom_reg );
	
	eeprom_release ( eeprom_reg );
	
	return 1;
}

/**************************************************************************
 *
 * GMII routines
 *
 **************************************************************************
 */

/* GMII registers */
#define MII_BMSR		0x01	/* Basic mode status register  */
#define MII_ADVERTISE		0x04	/* Advertisement control register */
#define MII_LPA			0x05	/* Link partner ability register*/
#define GMII_GTCR		0x09	/* 1000BASE-T control register */
#define GMII_GTSR		0x0a	/* 1000BASE-T status register */
#define GMII_PSSR		0x11	/* PHY-specific status register */

/* Basic mode status register. */
#define BMSR_LSTATUS		0x0004	/* Link status                 */

/* Link partner ability register. */
#define LPA_10HALF              0x0020  /* Can do 10mbps half-duplex   */
#define LPA_10FULL              0x0040  /* Can do 10mbps full-duplex   */
#define LPA_100HALF             0x0080  /* Can do 100mbps half-duplex  */
#define LPA_100FULL             0x0100  /* Can do 100mbps full-duplex  */
#define LPA_100BASE4            0x0200  /* Can do 100mbps 4k packets   */
#define LPA_PAUSE		0x0400	/* Bit 10 - MAC pause */

/* Pseudo extensions to the link partner ability register */
#define LPA_1000FULL		0x00020000
#define LPA_1000HALF		0x00010000

#define LPA_100			(LPA_100FULL | LPA_100HALF | LPA_100BASE4)
#define LPA_1000		( LPA_1000FULL | LPA_1000HALF )
#define LPA_DUPLEX		( LPA_10FULL | LPA_100FULL | LPA_1000FULL )

/* Mask of bits not associated with speed or duplexity. */
#define LPA_OTHER		~( LPA_10FULL | LPA_10HALF | LPA_100FULL | \
				   LPA_100HALF | LPA_1000FULL | LPA_1000HALF )

/* PHY-specific status register */
#define PSSR_LSTATUS		0x0400	/* Bit 10 - link status */

/**
 * Retrieve GMII autonegotiation advertised abilities
 *
 */
static unsigned int gmii_autoneg_advertised ( struct efab_nic *efab ) {
	unsigned int mii_advertise;
	unsigned int gmii_advertise;
	
	/* Extended bits are in bits 8 and 9 of GMII_GTCR */
	mii_advertise = efab->op->mdio_read ( efab, MII_ADVERTISE );
	gmii_advertise = ( ( efab->op->mdio_read ( efab, GMII_GTCR ) >> 8 )
			   & 0x03 );
	return ( ( gmii_advertise << 16 ) | mii_advertise );
}

/**
 * Retrieve GMII autonegotiation link partner abilities
 *
 */
static unsigned int gmii_autoneg_lpa ( struct efab_nic *efab ) {
	unsigned int mii_lpa;
	unsigned int gmii_lpa;
	
	/* Extended bits are in bits 10 and 11 of GMII_GTSR */
	mii_lpa = efab->op->mdio_read ( efab, MII_LPA );
	gmii_lpa = ( efab->op->mdio_read ( efab, GMII_GTSR ) >> 10 ) & 0x03;
	return ( ( gmii_lpa << 16 ) | mii_lpa );
}

/**
 * Calculate GMII autonegotiated link technology
 *
 */
static unsigned int gmii_nway_result ( unsigned int negotiated ) {
	unsigned int other_bits;

	/* Mask out the speed and duplexity bits */
	other_bits = negotiated & LPA_OTHER;

	if ( negotiated & LPA_1000FULL )
		return ( other_bits | LPA_1000FULL );
	else if ( negotiated & LPA_1000HALF )
		return ( other_bits | LPA_1000HALF );
	else if ( negotiated & LPA_100FULL )
		return ( other_bits | LPA_100FULL );
	else if ( negotiated & LPA_100BASE4 )
		return ( other_bits | LPA_100BASE4 );
	else if ( negotiated & LPA_100HALF )
		return ( other_bits | LPA_100HALF );
	else if ( negotiated & LPA_10FULL )
		return ( other_bits | LPA_10FULL );
	else return ( other_bits | LPA_10HALF );
}

/**
 * Check GMII PHY link status
 *
 */
static int gmii_link_ok ( struct efab_nic *efab ) {
	int status;
	int phy_status;
	
	/* BMSR is latching - it returns "link down" if the link has
	 * been down at any point since the last read.  To get a
	 * real-time status, we therefore read the register twice and
	 * use the result of the second read.
	 */
	efab->op->mdio_read ( efab, MII_BMSR );
	status = efab->op->mdio_read ( efab, MII_BMSR );

	/* Read the PHY-specific Status Register.  This is
	 * non-latching, so we need do only a single read.
	 */
	phy_status = efab->op->mdio_read ( efab, GMII_PSSR );

	return ( ( status & BMSR_LSTATUS ) && ( phy_status & PSSR_LSTATUS ) );
}

/**************************************************************************
 *
 * Alaska PHY
 *
 **************************************************************************
 */

/**
 * Initialise Alaska PHY
 *
 */
static void alaska_init ( struct efab_nic *efab ) {
	unsigned int advertised, lpa;

	/* Read link up status */
	efab->link_up = gmii_link_ok ( efab );

	if ( ! efab->link_up )
		return;

	/* Determine link options from PHY. */
	advertised = gmii_autoneg_advertised ( efab );
	lpa = gmii_autoneg_lpa ( efab );
	efab->link_options = gmii_nway_result ( advertised & lpa );

	printf ( "%dMbps %s-duplex (%04x,%04x)\n",
		 ( efab->link_options & LPA_1000 ? 1000 :
		   ( efab->link_options & LPA_100 ? 100 : 10 ) ),
		 ( efab->link_options & LPA_DUPLEX ? "full" : "half" ),
		 advertised, lpa );
}

/**************************************************************************
 *
 * Mentor MAC
 *
 **************************************************************************
 */

/* GMAC configuration register 1 */
#define GM_CFG1_REG_MAC 0x00
#define GM_SW_RST_LBN 31
#define GM_SW_RST_WIDTH 1
#define GM_RX_FC_EN_LBN 5
#define GM_RX_FC_EN_WIDTH 1
#define GM_TX_FC_EN_LBN 4
#define GM_TX_FC_EN_WIDTH 1
#define GM_RX_EN_LBN 2
#define GM_RX_EN_WIDTH 1
#define GM_TX_EN_LBN 0
#define GM_TX_EN_WIDTH 1

/* GMAC configuration register 2 */
#define GM_CFG2_REG_MAC 0x01
#define GM_PAMBL_LEN_LBN 12
#define GM_PAMBL_LEN_WIDTH 4
#define GM_IF_MODE_LBN 8
#define GM_IF_MODE_WIDTH 2
#define GM_PAD_CRC_EN_LBN 2
#define GM_PAD_CRC_EN_WIDTH 1
#define GM_FD_LBN 0
#define GM_FD_WIDTH 1

/* GMAC maximum frame length register */
#define GM_MAX_FLEN_REG_MAC 0x04
#define GM_MAX_FLEN_LBN 0
#define GM_MAX_FLEN_WIDTH 16

/* GMAC MII management configuration register */
#define GM_MII_MGMT_CFG_REG_MAC 0x08
#define GM_MGMT_CLK_SEL_LBN 0
#define GM_MGMT_CLK_SEL_WIDTH 3

/* GMAC MII management command register */
#define GM_MII_MGMT_CMD_REG_MAC 0x09
#define GM_MGMT_SCAN_CYC_LBN 1
#define GM_MGMT_SCAN_CYC_WIDTH 1
#define GM_MGMT_RD_CYC_LBN 0
#define GM_MGMT_RD_CYC_WIDTH 1

/* GMAC MII management address register */
#define GM_MII_MGMT_ADR_REG_MAC 0x0a
#define GM_MGMT_PHY_ADDR_LBN 8
#define GM_MGMT_PHY_ADDR_WIDTH 5
#define GM_MGMT_REG_ADDR_LBN 0
#define GM_MGMT_REG_ADDR_WIDTH 5

/* GMAC MII management control register */
#define GM_MII_MGMT_CTL_REG_MAC 0x0b
#define GM_MGMT_CTL_LBN 0
#define GM_MGMT_CTL_WIDTH 16

/* GMAC MII management status register */
#define GM_MII_MGMT_STAT_REG_MAC 0x0c
#define GM_MGMT_STAT_LBN 0
#define GM_MGMT_STAT_WIDTH 16

/* GMAC MII management indicators register */
#define GM_MII_MGMT_IND_REG_MAC 0x0d
#define GM_MGMT_BUSY_LBN 0
#define GM_MGMT_BUSY_WIDTH 1

/* GMAC station address register 1 */
#define GM_ADR1_REG_MAC 0x10
#define GM_HWADDR_5_LBN 24
#define GM_HWADDR_5_WIDTH 8
#define GM_HWADDR_4_LBN 16
#define GM_HWADDR_4_WIDTH 8
#define GM_HWADDR_3_LBN 8
#define GM_HWADDR_3_WIDTH 8
#define GM_HWADDR_2_LBN 0
#define GM_HWADDR_2_WIDTH 8

/* GMAC station address register 2 */
#define GM_ADR2_REG_MAC 0x11
#define GM_HWADDR_1_LBN 24
#define GM_HWADDR_1_WIDTH 8
#define GM_HWADDR_0_LBN 16
#define GM_HWADDR_0_WIDTH 8

/* GMAC FIFO configuration register 0 */
#define GMF_CFG0_REG_MAC 0x12
#define GMF_FTFENREQ_LBN 12
#define GMF_FTFENREQ_WIDTH 1
#define GMF_STFENREQ_LBN 11
#define GMF_STFENREQ_WIDTH 1
#define GMF_FRFENREQ_LBN 10
#define GMF_FRFENREQ_WIDTH 1
#define GMF_SRFENREQ_LBN 9
#define GMF_SRFENREQ_WIDTH 1
#define GMF_WTMENREQ_LBN 8
#define GMF_WTMENREQ_WIDTH 1

/* GMAC FIFO configuration register 1 */
#define GMF_CFG1_REG_MAC 0x13
#define GMF_CFGFRTH_LBN 16
#define GMF_CFGFRTH_WIDTH 5
#define GMF_CFGXOFFRTX_LBN 0
#define GMF_CFGXOFFRTX_WIDTH 16

/* GMAC FIFO configuration register 2 */
#define GMF_CFG2_REG_MAC 0x14
#define GMF_CFGHWM_LBN 16
#define GMF_CFGHWM_WIDTH 6
#define GMF_CFGLWM_LBN 0
#define GMF_CFGLWM_WIDTH 6

/* GMAC FIFO configuration register 3 */
#define GMF_CFG3_REG_MAC 0x15
#define GMF_CFGHWMFT_LBN 16
#define GMF_CFGHWMFT_WIDTH 6
#define GMF_CFGFTTH_LBN 0
#define GMF_CFGFTTH_WIDTH 6

/* GMAC FIFO configuration register 4 */
#define GMF_CFG4_REG_MAC 0x16
#define GMF_HSTFLTRFRM_PAUSE_LBN 12
#define GMF_HSTFLTRFRM_PAUSE_WIDTH 12

/* GMAC FIFO configuration register 5 */
#define GMF_CFG5_REG_MAC 0x17
#define GMF_CFGHDPLX_LBN 22
#define GMF_CFGHDPLX_WIDTH 1
#define GMF_CFGBYTMODE_LBN 19
#define GMF_CFGBYTMODE_WIDTH 1
#define GMF_HSTDRPLT64_LBN 18
#define GMF_HSTDRPLT64_WIDTH 1
#define GMF_HSTFLTRFRMDC_PAUSE_LBN 12
#define GMF_HSTFLTRFRMDC_PAUSE_WIDTH 1

struct efab_mentormac_parameters {
	int gmf_cfgfrth;
	int gmf_cfgftth;
	int gmf_cfghwmft;
	int gmf_cfghwm;
	int gmf_cfglwm;
};

/**
 * Reset Mentor MAC
 *
 */
static void mentormac_reset ( struct efab_nic *efab, int reset ) {
	efab_dword_t reg;

	EFAB_POPULATE_DWORD_1 ( reg, GM_SW_RST, reset );
	efab->op->mac_writel ( efab, &reg, GM_CFG1_REG_MAC );
	udelay ( 1000 );

	if ( ( ! reset ) && ( efab->port == 0 ) ) {
		/* Configure GMII interface so PHY is accessible.
		 * Note that GMII interface is connected only to port
		 * 0
		 */
		EFAB_POPULATE_DWORD_1 ( reg, GM_MGMT_CLK_SEL, 0x4 );
		efab->op->mac_writel ( efab, &reg, GM_MII_MGMT_CFG_REG_MAC );
		udelay ( 10 );
	}
}

/**
 * Initialise Mentor MAC
 *
 */
static void mentormac_init ( struct efab_nic *efab,
			     struct efab_mentormac_parameters *params ) {
	int pause, if_mode, full_duplex, bytemode, half_duplex;
	efab_dword_t reg;

	/* Configuration register 1 */
	pause = ( efab->link_options & LPA_PAUSE ) ? 1 : 0;
	if ( ! ( efab->link_options & LPA_DUPLEX ) ) {
		/* Half-duplex operation requires TX flow control */
		pause = 1;
	}
	EFAB_POPULATE_DWORD_4 ( reg,
				GM_TX_EN, 1,
				GM_TX_FC_EN, pause,
				GM_RX_EN, 1,
				GM_RX_FC_EN, 1 );
	efab->op->mac_writel ( efab, &reg, GM_CFG1_REG_MAC );
	udelay ( 10 );

	/* Configuration register 2 */
	if_mode = ( efab->link_options & LPA_1000 ) ? 2 : 1;
	full_duplex = ( efab->link_options & LPA_DUPLEX ) ? 1 : 0;
	EFAB_POPULATE_DWORD_4 ( reg,
				GM_IF_MODE, if_mode,
				GM_PAD_CRC_EN, 1,
				GM_FD, full_duplex,
				GM_PAMBL_LEN, 0x7 /* ? */ );
	efab->op->mac_writel ( efab, &reg, GM_CFG2_REG_MAC );
	udelay ( 10 );

	/* Max frame len register */
	EFAB_POPULATE_DWORD_1 ( reg, GM_MAX_FLEN, ETH_FRAME_LEN );
	efab->op->mac_writel ( efab, &reg, GM_MAX_FLEN_REG_MAC );
	udelay ( 10 );

	/* FIFO configuration register 0 */
	EFAB_POPULATE_DWORD_5 ( reg,
				GMF_FTFENREQ, 1,
				GMF_STFENREQ, 1,
				GMF_FRFENREQ, 1,
				GMF_SRFENREQ, 1,
				GMF_WTMENREQ, 1 );
	efab->op->mac_writel ( efab, &reg, GMF_CFG0_REG_MAC );
	udelay ( 10 );

	/* FIFO configuration register 1 */
	EFAB_POPULATE_DWORD_2 ( reg,
				GMF_CFGFRTH, params->gmf_cfgfrth,
				GMF_CFGXOFFRTX, 0xffff );
	efab->op->mac_writel ( efab, &reg, GMF_CFG1_REG_MAC );
	udelay ( 10 );

	/* FIFO configuration register 2 */
	EFAB_POPULATE_DWORD_2 ( reg,
				GMF_CFGHWM, params->gmf_cfghwm,
				GMF_CFGLWM, params->gmf_cfglwm );
	efab->op->mac_writel ( efab, &reg, GMF_CFG2_REG_MAC );
	udelay ( 10 );

	/* FIFO configuration register 3 */
	EFAB_POPULATE_DWORD_2 ( reg,
				GMF_CFGHWMFT, params->gmf_cfghwmft,
				GMF_CFGFTTH, params->gmf_cfgftth );
	efab->op->mac_writel ( efab, &reg, GMF_CFG3_REG_MAC );
	udelay ( 10 );

	/* FIFO configuration register 4 */
	EFAB_POPULATE_DWORD_1 ( reg, GMF_HSTFLTRFRM_PAUSE, 1 );
	efab->op->mac_writel ( efab, &reg, GMF_CFG4_REG_MAC );
	udelay ( 10 );
	
	/* FIFO configuration register 5 */
	bytemode = ( efab->link_options & LPA_1000 ) ? 1 : 0;
	half_duplex = ( efab->link_options & LPA_DUPLEX ) ? 0 : 1;
	efab->op->mac_readl ( efab, &reg, GMF_CFG5_REG_MAC );
	EFAB_SET_DWORD_FIELD ( reg, GMF_CFGBYTMODE, bytemode );
	EFAB_SET_DWORD_FIELD ( reg, GMF_CFGHDPLX, half_duplex );
	EFAB_SET_DWORD_FIELD ( reg, GMF_HSTDRPLT64, half_duplex );
	EFAB_SET_DWORD_FIELD ( reg, GMF_HSTFLTRFRMDC_PAUSE, 0 );
	efab->op->mac_writel ( efab, &reg, GMF_CFG5_REG_MAC );
	udelay ( 10 );
	
	/* MAC address */
	EFAB_POPULATE_DWORD_4 ( reg,
				GM_HWADDR_5, efab->mac_addr[5],
				GM_HWADDR_4, efab->mac_addr[4],
				GM_HWADDR_3, efab->mac_addr[3],
				GM_HWADDR_2, efab->mac_addr[2] );
	efab->op->mac_writel ( efab, &reg, GM_ADR1_REG_MAC );
	udelay ( 10 );
	EFAB_POPULATE_DWORD_2 ( reg,
				GM_HWADDR_1, efab->mac_addr[1],
				GM_HWADDR_0, efab->mac_addr[0] );
	efab->op->mac_writel ( efab, &reg, GM_ADR2_REG_MAC );
	udelay ( 10 );
}

/**
 * Wait for GMII access to complete
 *
 */
static int mentormac_gmii_wait ( struct efab_nic *efab ) {
	int count;
	efab_dword_t indicator;

	for ( count = 0 ; count < 1000 ; count++ ) {
		udelay ( 10 );
		efab->op->mac_readl ( efab, &indicator,
				      GM_MII_MGMT_IND_REG_MAC );
		if ( EFAB_DWORD_FIELD ( indicator, GM_MGMT_BUSY ) == 0 )
			return 1;
	}
	printf ( "Timed out waiting for GMII\n" );
	return 0;
}

/**
 * Write a GMII register
 *
 */
static void mentormac_mdio_write ( struct efab_nic *efab, int phy_id,
				   int location, int value ) {
	efab_dword_t reg;
	int save_port;

	EFAB_TRACE ( "Writing GMII %d register %02x with %04x\n", phy_id,
		     location, value );

	/* Mentor MAC connects both PHYs to MAC 0 */
	save_port = efab->port;
	efab->port = 0;

	/* Check MII not currently being accessed */
	if ( ! mentormac_gmii_wait ( efab ) )
		goto out;

	/* Write the address register */
	EFAB_POPULATE_DWORD_2 ( reg,
				GM_MGMT_PHY_ADDR, phy_id,
				GM_MGMT_REG_ADDR, location );
	efab->op->mac_writel ( efab, &reg, GM_MII_MGMT_ADR_REG_MAC );
	udelay ( 10 );

	/* Write data */
	EFAB_POPULATE_DWORD_1 ( reg, GM_MGMT_CTL, value );
	efab->op->mac_writel ( efab, &reg, GM_MII_MGMT_CTL_REG_MAC );

	/* Wait for data to be written */
	mentormac_gmii_wait ( efab );

 out:
	/* Restore efab->port */
	efab->port = save_port;
}

/**
 * Read a GMII register
 *
 */
static int mentormac_mdio_read ( struct efab_nic *efab, int phy_id,
				 int location ) {
	efab_dword_t reg;
	int value = 0xffff;
	int save_port;

	/* Mentor MAC connects both PHYs to MAC 0 */
	save_port = efab->port;
	efab->port = 0;

	/* Check MII not currently being accessed */
	if ( ! mentormac_gmii_wait ( efab ) )
		goto out;

	/* Write the address register */
	EFAB_POPULATE_DWORD_2 ( reg,
				GM_MGMT_PHY_ADDR, phy_id,
				GM_MGMT_REG_ADDR, location );
	efab->op->mac_writel ( efab, &reg, GM_MII_MGMT_ADR_REG_MAC );
	udelay ( 10 );

	/* Request data to be read */
	EFAB_POPULATE_DWORD_1 ( reg, GM_MGMT_RD_CYC, 1 );
	efab->op->mac_writel ( efab, &reg, GM_MII_MGMT_CMD_REG_MAC );

	/* Wait for data to be become available */
	if ( mentormac_gmii_wait ( efab ) ) {
		/* Read data */
		efab->op->mac_readl ( efab, &reg, GM_MII_MGMT_STAT_REG_MAC );
		value = EFAB_DWORD_FIELD ( reg, GM_MGMT_STAT );
		EFAB_TRACE ( "Read from GMII %d register %02x, got %04x\n",
			     phy_id, location, value );
	}

	/* Signal completion */
	EFAB_ZERO_DWORD ( reg );
	efab->op->mac_writel ( efab, &reg, GM_MII_MGMT_CMD_REG_MAC );
	udelay ( 10 );

 out:
	/* Restore efab->port */
	efab->port = save_port;

	return value;
}

/**************************************************************************
 *
 * EF1002 routines
 *
 **************************************************************************
 */

/** Control and General Status */
#define EF1_CTR_GEN_STATUS0_REG 0x0
#define EF1_MASTER_EVENTS_LBN 12
#define EF1_MASTER_EVENTS_WIDTH 1
#define EF1_TX_ENGINE_EN_LBN 19
#define EF1_TX_ENGINE_EN_WIDTH 1
#define EF1_RX_ENGINE_EN_LBN 18
#define EF1_RX_ENGINE_EN_WIDTH 1
#define EF1_LB_RESET_LBN 3
#define EF1_LB_RESET_WIDTH 1
#define EF1_MAC_RESET_LBN 2
#define EF1_MAC_RESET_WIDTH 1
#define EF1_CAM_ENABLE_LBN 1
#define EF1_CAM_ENABLE_WIDTH 1

/** IRQ sources */
#define EF1_IRQ_SRC_REG 0x0008

/** IRQ mask */
#define EF1_IRQ_MASK_REG 0x000c
#define EF1_IRQ_PHY1_LBN 11
#define EF1_IRQ_PHY1_WIDTH 1
#define EF1_IRQ_PHY0_LBN 10
#define EF1_IRQ_PHY0_WIDTH 1
#define EF1_IRQ_SERR_LBN 7
#define EF1_IRQ_SERR_WIDTH 1
#define EF1_IRQ_EVQ_LBN 3
#define EF1_IRQ_EVQ_WIDTH 1

/** Event generation */
#define EF1_EVT3_REG 0x38

/** EEPROM access */
#define EF1_EEPROM_REG 0x0040

/** Control register 2 */
#define EF1_CTL2_REG 0x4c
#define EF1_MEM_MAP_4MB_LBN 11
#define EF1_MEM_MAP_4MB_WIDTH 1
#define EF1_EV_INTR_CLR_WRITE_LBN 6
#define EF1_EV_INTR_CLR_WRITE_WIDTH 1
#define EF1_SW_RESET_LBN 2
#define EF1_SW_RESET_WIDTH 1
#define EF1_INTR_AFTER_EVENT_LBN 1
#define EF1_INTR_AFTER_EVENT_WIDTH 1

/** Event FIFO */
#define EF1_EVENT_FIFO_REG 0x50

/** Event FIFO count */
#define EF1_EVENT_FIFO_COUNT_REG 0x5c
#define EF1_EV_COUNT_LBN 0
#define EF1_EV_COUNT_WIDTH 16

/** TX DMA control and status */
#define EF1_DMA_TX_CSR_REG 0x80
#define EF1_DMA_TX_CSR_CHAIN_EN_LBN 8
#define EF1_DMA_TX_CSR_CHAIN_EN_WIDTH 1
#define EF1_DMA_TX_CSR_ENABLE_LBN 4
#define EF1_DMA_TX_CSR_ENABLE_WIDTH 1
#define EF1_DMA_TX_CSR_INT_EN_LBN 0
#define EF1_DMA_TX_CSR_INT_EN_WIDTH 1

/** RX DMA control and status */
#define EF1_DMA_RX_CSR_REG 0xa0
#define EF1_DMA_RX_ABOVE_1GB_EN_LBN 6
#define EF1_DMA_RX_ABOVE_1GB_EN_WIDTH 1
#define EF1_DMA_RX_BELOW_1MB_EN_LBN 5
#define EF1_DMA_RX_BELOW_1MB_EN_WIDTH 1 
#define EF1_DMA_RX_CSR_ENABLE_LBN 0
#define EF1_DMA_RX_CSR_ENABLE_WIDTH 1

/** Level 5 watermark register (in MAC space) */
#define EF1_GMF_L5WM_REG_MAC 0x20
#define EF1_L5WM_LBN 0
#define EF1_L5WM_WIDTH 32

/** MAC clock */
#define EF1_GM_MAC_CLK_REG 0x112000
#define EF1_GM_PORT0_MAC_CLK_LBN 0
#define EF1_GM_PORT0_MAC_CLK_WIDTH 1
#define EF1_GM_PORT1_MAC_CLK_LBN 1
#define EF1_GM_PORT1_MAC_CLK_WIDTH 1

/** TX descriptor FIFO */
#define EF1_TX_DESC_FIFO 0x141000
#define EF1_TX_KER_EVQ_LBN 80
#define EF1_TX_KER_EVQ_WIDTH 12
#define EF1_TX_KER_IDX_LBN 64
#define EF1_TX_KER_IDX_WIDTH 16
#define EF1_TX_KER_MODE_LBN 63
#define EF1_TX_KER_MODE_WIDTH 1
#define EF1_TX_KER_PORT_LBN 60
#define EF1_TX_KER_PORT_WIDTH 1
#define EF1_TX_KER_CONT_LBN 56
#define EF1_TX_KER_CONT_WIDTH 1
#define EF1_TX_KER_BYTE_CNT_LBN 32
#define EF1_TX_KER_BYTE_CNT_WIDTH 24
#define EF1_TX_KER_BUF_ADR_LBN 0
#define EF1_TX_KER_BUF_ADR_WIDTH 32

/** TX descriptor FIFO flush */
#define EF1_TX_DESC_FIFO_FLUSH 0x141ffc

/** RX descriptor FIFO */
#define EF1_RX_DESC_FIFO 0x145000
#define EF1_RX_KER_EVQ_LBN 48
#define EF1_RX_KER_EVQ_WIDTH 12
#define EF1_RX_KER_IDX_LBN 32
#define EF1_RX_KER_IDX_WIDTH 16
#define EF1_RX_KER_BUF_ADR_LBN 0
#define EF1_RX_KER_BUF_ADR_WIDTH 32

/** RX descriptor FIFO flush */
#define EF1_RX_DESC_FIFO_FLUSH 0x145ffc 

/** CAM */
#define EF1_CAM_BASE 0x1c0000
#define EF1_CAM_WTF_DOES_THIS_DO_LBN 0
#define EF1_CAM_WTF_DOES_THIS_DO_WIDTH 32

/** Event queue pointers */
#define EF1_EVQ_PTR_BASE 0x260000
#define EF1_EVQ_SIZE_LBN 29
#define EF1_EVQ_SIZE_WIDTH 2
#define EF1_EVQ_SIZE_4K 3
#define EF1_EVQ_SIZE_2K 2
#define EF1_EVQ_SIZE_1K 1
#define EF1_EVQ_SIZE_512 0
#define EF1_EVQ_BUF_BASE_ID_LBN 0
#define EF1_EVQ_BUF_BASE_ID_WIDTH 29

/* MAC registers */
#define EF1002_MAC_REGBANK 0x110000
#define EF1002_MAC_REGBANK_SIZE 0x1000
#define EF1002_MAC_REG_SIZE 0x08

/** Offset of a MAC register within EF1002 */
#define EF1002_MAC_REG( efab, mac_reg )				\
	( EF1002_MAC_REGBANK +					\
	  ( (efab)->port * EF1002_MAC_REGBANK_SIZE ) +		\
	  ( (mac_reg) * EF1002_MAC_REG_SIZE ) )

/* Event queue entries */
#define EF1_EV_CODE_LBN 20
#define EF1_EV_CODE_WIDTH 8
#define EF1_RX_EV_DECODE 0x01
#define EF1_TX_EV_DECODE 0x02
#define EF1_DRV_GEN_EV_DECODE 0x0f

/* Receive events */
#define EF1_RX_EV_LEN_LBN 48
#define EF1_RX_EV_LEN_WIDTH 16
#define EF1_RX_EV_PORT_LBN 17
#define EF1_RX_EV_PORT_WIDTH 3
#define EF1_RX_EV_OK_LBN 16
#define EF1_RX_EV_OK_WIDTH 1
#define EF1_RX_EV_IDX_LBN 0
#define EF1_RX_EV_IDX_WIDTH 16

/* Transmit events */
#define EF1_TX_EV_PORT_LBN 17
#define EF1_TX_EV_PORT_WIDTH 3
#define EF1_TX_EV_OK_LBN 16
#define EF1_TX_EV_OK_WIDTH 1
#define EF1_TX_EV_IDX_LBN 0
#define EF1_TX_EV_IDX_WIDTH 16

/**
 * Write dword to EF1002 register
 *
 */
static inline void ef1002_writel ( struct efab_nic *efab, efab_dword_t *value,
				   unsigned int reg ) {
	EFAB_REGDUMP ( "Writing register %x with " EFAB_DWORD_FMT "\n",
		       reg, EFAB_DWORD_VAL ( *value ) );
	writel ( value->u32[0], efab->membase + reg );
}

/**
 * Read dword from an EF1002 register
 *
 */
static inline void ef1002_readl ( struct efab_nic *efab, efab_dword_t *value,
				  unsigned int reg ) {
	value->u32[0] = readl ( efab->membase + reg );
	EFAB_REGDUMP ( "Read from register %x, got " EFAB_DWORD_FMT "\n",
		       reg, EFAB_DWORD_VAL ( *value ) );
}

/**
 * Read dword from an EF1002 register, silently
 *
 */
static inline void ef1002_readl_silent ( struct efab_nic *efab,
					 efab_dword_t *value,
					 unsigned int reg ) {
	value->u32[0] = readl ( efab->membase + reg );
}

/**
 * Get memory base
 *
 */
static void ef1002_get_membase ( struct efab_nic *efab ) {
	unsigned long membase_phys;

	membase_phys = pci_bar_start ( efab->pci, PCI_BASE_ADDRESS_0 );
	efab->membase = ioremap ( membase_phys, 0x800000 );
}

/** PCI registers to backup/restore over a device reset */
static const unsigned int efab_pci_reg_addr[] = {
	PCI_COMMAND, 0x0c /* PCI_CACHE_LINE_SIZE */,
	PCI_BASE_ADDRESS_0, PCI_BASE_ADDRESS_1, PCI_BASE_ADDRESS_2,
	PCI_BASE_ADDRESS_3, PCI_ROM_ADDRESS, PCI_INTERRUPT_LINE,
};
/** Number of registers in efab_pci_reg_addr */
#define EFAB_NUM_PCI_REG \
	( sizeof ( efab_pci_reg_addr ) / sizeof ( efab_pci_reg_addr[0] ) )
/** PCI configuration space backup */
struct efab_pci_reg {
	uint32_t reg[EFAB_NUM_PCI_REG];
};

/**
 * Reset device
 *
 */
static int ef1002_reset ( struct efab_nic *efab ) {
	struct efab_pci_reg pci_reg;
	struct pci_device *pci_dev = efab->pci;
	efab_dword_t reg;
	unsigned int i;
	uint32_t tmp;

	/* Back up PCI configuration registers */
	for ( i = 0 ; i < EFAB_NUM_PCI_REG ; i++ ) {
		pci_read_config_dword ( pci_dev, efab_pci_reg_addr[i],
					&pci_reg.reg[i] );
	}

	/* Reset the whole device. */
	EFAB_POPULATE_DWORD_1 ( reg, EF1_SW_RESET, 1 );
	ef1002_writel ( efab, &reg, EF1_CTL2_REG );
	mdelay ( 200 );
	
	/* Restore PCI configuration space */
	for ( i = 0 ; i < EFAB_NUM_PCI_REG ; i++ ) {
		pci_write_config_dword ( pci_dev, efab_pci_reg_addr[i],
					 pci_reg.reg[i] );
	}

	/* Verify PCI configuration space */
	for ( i = 0 ; i < EFAB_NUM_PCI_REG ; i++ ) {
		pci_read_config_dword ( pci_dev, efab_pci_reg_addr[i], &tmp );
		if ( tmp != pci_reg.reg[i] ) {
			printf ( "PCI restore failed on register %02x "
				 "(is %08x, should be %08x); reboot\n",
				 i, tmp, pci_reg.reg[i] );
			return 0;
		}
	}

	/* Verify device reset complete */
	ef1002_readl ( efab, &reg, EF1_CTR_GEN_STATUS0_REG );
	if ( EFAB_DWORD_IS_ALL_ONES ( reg ) ) {
		printf ( "Reset failed\n" );
		return 0;
	}

	return 1;
}

/**
 * Initialise NIC
 *
 */
static int ef1002_init_nic ( struct efab_nic *efab ) {
	efab_dword_t reg;
	int save_port;

	/* No idea what CAM is, but the 'datasheet' says that we have
	 * to write these values in at start of day
	 */
	EFAB_POPULATE_DWORD_1 ( reg, EF1_CAM_WTF_DOES_THIS_DO, 0x6 );
	ef1002_writel ( efab, &reg, EF1_CAM_BASE + 0x20018 );
	udelay ( 1000 );
	EFAB_POPULATE_DWORD_1 ( reg, EF1_CAM_WTF_DOES_THIS_DO, 0x01000000 );
	ef1002_writel ( efab, &reg, EF1_CAM_BASE + 0x00018 );
	udelay ( 1000 );

	/* General control register 0 */
	ef1002_readl ( efab, &reg, EF1_CTR_GEN_STATUS0_REG );
	EFAB_SET_DWORD_FIELD ( reg, EF1_MASTER_EVENTS, 0 );
	EFAB_SET_DWORD_FIELD ( reg, EF1_CAM_ENABLE, 1 );
	ef1002_writel ( efab, &reg, EF1_CTR_GEN_STATUS0_REG );
	udelay ( 1000 );

	/* General control register 2 */
	ef1002_readl ( efab, &reg, EF1_CTL2_REG );
	EFAB_SET_DWORD_FIELD ( reg, EF1_INTR_AFTER_EVENT, 1 );
	EFAB_SET_DWORD_FIELD ( reg, EF1_EV_INTR_CLR_WRITE, 0 );
	EFAB_SET_DWORD_FIELD ( reg, EF1_MEM_MAP_4MB, 0 );
	ef1002_writel ( efab, &reg, EF1_CTL2_REG );
	udelay ( 1000 );

	/* Enable RX DMA */
	ef1002_readl ( efab, &reg, EF1_DMA_RX_CSR_REG );
	EFAB_SET_DWORD_FIELD ( reg, EF1_DMA_RX_CSR_ENABLE, 1 );
	EFAB_SET_DWORD_FIELD ( reg, EF1_DMA_RX_BELOW_1MB_EN, 1 );
	EFAB_SET_DWORD_FIELD ( reg, EF1_DMA_RX_ABOVE_1GB_EN, 1 );
	ef1002_writel ( efab, &reg, EF1_DMA_RX_CSR_REG );
	udelay ( 1000 );

	/* Enable TX DMA */
	ef1002_readl ( efab, &reg, EF1_DMA_TX_CSR_REG );
	EFAB_SET_DWORD_FIELD ( reg, EF1_DMA_TX_CSR_CHAIN_EN, 1 );
	EFAB_SET_DWORD_FIELD ( reg, EF1_DMA_TX_CSR_ENABLE, 0 /* ?? */ );
	EFAB_SET_DWORD_FIELD ( reg, EF1_DMA_TX_CSR_INT_EN, 0 /* ?? */ );
	ef1002_writel ( efab, &reg, EF1_DMA_TX_CSR_REG );
	udelay ( 1000 );

	/* Flush descriptor queues */
	EFAB_ZERO_DWORD ( reg );
	ef1002_writel ( efab, &reg, EF1_RX_DESC_FIFO_FLUSH );
	ef1002_writel ( efab, &reg, EF1_TX_DESC_FIFO_FLUSH );
	wmb();
	udelay ( 10000 );

	/* Reset both MACs */
	save_port = efab->port;
	efab->port = 0;
	mentormac_reset ( efab, 1 );
	efab->port = 1;
	mentormac_reset ( efab, 1 );

	/* Reset both PHYs */
	ef1002_readl ( efab, &reg, EF1_CTR_GEN_STATUS0_REG );
	EFAB_SET_DWORD_FIELD ( reg, EF1_MAC_RESET, 1 );
	ef1002_writel ( efab, &reg, EF1_CTR_GEN_STATUS0_REG );
	udelay ( 10000 );
	EFAB_SET_DWORD_FIELD ( reg, EF1_MAC_RESET, 0 );
	ef1002_writel ( efab, &reg, EF1_CTR_GEN_STATUS0_REG );
	udelay ( 10000 );

	/* Take MACs out of reset */
	efab->port = 0;
	mentormac_reset ( efab, 0 );
	efab->port = 1;
	mentormac_reset ( efab, 0 );
	efab->port = save_port;

	/* Give PHY time to wake up.  It takes a while. */
	sleep ( 2 );

	return 1;
}

/**
 * Read MAC address from EEPROM
 *
 */
static int ef1002_read_eeprom ( struct efab_nic *efab ) {
	return efab_eeprom_read_mac ( efab->membase + EF1_EEPROM_REG,
				      efab->mac_addr );
}

/** RX descriptor */
typedef efab_qword_t ef1002_rx_desc_t;

/**
 * Build RX descriptor
 *
 */
static void ef1002_build_rx_desc ( struct efab_nic *efab,
				   struct efab_rx_buf *rx_buf ) {
	ef1002_rx_desc_t rxd;

	EFAB_POPULATE_QWORD_3 ( rxd,
				EF1_RX_KER_EVQ, 0,
				EF1_RX_KER_IDX, rx_buf->id,
				EF1_RX_KER_BUF_ADR,
				virt_to_bus ( rx_buf->addr ) );
	ef1002_writel ( efab, &rxd.dword[0], EF1_RX_DESC_FIFO + 0 );
	ef1002_writel ( efab, &rxd.dword[1], EF1_RX_DESC_FIFO + 4 );
	udelay ( 10 );
}

/**
 * Update RX descriptor write pointer
 *
 */
static void ef1002_notify_rx_desc ( struct efab_nic *efab __unused ) {
	/* Nothing to do */
}

/** TX descriptor */
typedef efab_oword_t ef1002_tx_desc_t;

/**
 * Build TX descriptor
 *
 */
static void ef1002_build_tx_desc ( struct efab_nic *efab,
				   struct efab_tx_buf *tx_buf ) {
	ef1002_tx_desc_t txd;

	EFAB_POPULATE_OWORD_7 ( txd,
				EF1_TX_KER_EVQ, 0,
				EF1_TX_KER_IDX, tx_buf->id,
				EF1_TX_KER_MODE, 0 /* IP mode */,
				EF1_TX_KER_PORT, efab->port,
				EF1_TX_KER_CONT, 0,
				EF1_TX_KER_BYTE_CNT, tx_buf->len,
				EF1_TX_KER_BUF_ADR,
				virt_to_bus ( tx_buf->addr ) );

	ef1002_writel ( efab, &txd.dword[0], EF1_TX_DESC_FIFO + 0 );
	ef1002_writel ( efab, &txd.dword[1], EF1_TX_DESC_FIFO + 4 );
	ef1002_writel ( efab, &txd.dword[2], EF1_TX_DESC_FIFO + 8 );
	udelay ( 10 );
}

/**
 * Update TX descriptor write pointer
 *
 */
static void ef1002_notify_tx_desc ( struct efab_nic *efab __unused ) {
	/* Nothing to do */
}

/** An event */
typedef efab_qword_t ef1002_event_t;

/**
 * Retrieve event from event queue
 *
 */
static int ef1002_fetch_event ( struct efab_nic *efab,
				struct efab_event *event ) {
	efab_dword_t reg;
	int ev_code;
	int words;

	/* Check event FIFO depth */
	ef1002_readl_silent ( efab, &reg, EF1_EVENT_FIFO_COUNT_REG );
	words = EFAB_DWORD_FIELD ( reg, EF1_EV_COUNT );
	if ( ! words )
		return 0;

	/* Read event data */
	ef1002_readl ( efab, &reg, EF1_EVENT_FIFO_REG );
	DBG ( "Event is " EFAB_DWORD_FMT "\n", EFAB_DWORD_VAL ( reg ) );

	/* Decode event */
	ev_code = EFAB_DWORD_FIELD ( reg, EF1_EV_CODE );
	switch ( ev_code ) {
	case EF1_TX_EV_DECODE:
		event->type = EFAB_EV_TX;
		break;
	case EF1_RX_EV_DECODE:
		event->type = EFAB_EV_RX;
		event->rx_id = EFAB_DWORD_FIELD ( reg, EF1_RX_EV_IDX );
		/* RX len not available via event FIFO */
		event->rx_len = ETH_FRAME_LEN;
		break;
	default:
		printf ( "Unknown event type %d\n", ev_code );
		event->type = EFAB_EV_NONE;
	}

	/* Clear any pending interrupts */
	ef1002_readl ( efab, &reg, EF1_IRQ_SRC_REG );

	return 1;
}

/**
 * Enable/disable interrupts
 *
 */
static void ef1002_mask_irq ( struct efab_nic *efab, int enabled ) {
	efab_dword_t irq_mask;

	EFAB_POPULATE_DWORD_2 ( irq_mask,
				EF1_IRQ_SERR, enabled,
				EF1_IRQ_EVQ, enabled );
	ef1002_writel ( efab, &irq_mask, EF1_IRQ_MASK_REG );
}

/**
 * Generate interrupt
 *
 */
static void ef1002_generate_irq ( struct efab_nic *efab ) {
	ef1002_event_t test_event;

	EFAB_POPULATE_QWORD_1 ( test_event,
				EF1_EV_CODE, EF1_DRV_GEN_EV_DECODE );
	ef1002_writel ( efab, &test_event.dword[0], EF1_EVT3_REG );
}

/**
 * Write dword to an EF1002 MAC register
 *
 */
static void ef1002_mac_writel ( struct efab_nic *efab,
				efab_dword_t *value, unsigned int mac_reg ) {
	ef1002_writel ( efab, value, EF1002_MAC_REG ( efab, mac_reg ) );
}

/**
 * Read dword from an EF1002 MAC register
 *
 */
static void ef1002_mac_readl ( struct efab_nic *efab,
			       efab_dword_t *value, unsigned int mac_reg ) {
	ef1002_readl ( efab, value, EF1002_MAC_REG ( efab, mac_reg ) );
}

/**
 * Initialise MAC
 *
 */
static int ef1002_init_mac ( struct efab_nic *efab ) {
	static struct efab_mentormac_parameters ef1002_mentormac_params = {
		.gmf_cfgfrth = 0x13,
		.gmf_cfgftth = 0x10,
		.gmf_cfghwmft = 0x555,
		.gmf_cfghwm = 0x2a,
		.gmf_cfglwm = 0x15,
	};
	efab_dword_t reg;
	unsigned int mac_clk;

	/* Initialise PHY */
	alaska_init ( efab );

	/* Initialise MAC */
	mentormac_init ( efab, &ef1002_mentormac_params );

	/* Write Level 5 watermark register */
	EFAB_POPULATE_DWORD_1 ( reg, EF1_L5WM, 0x10040000 );
	efab->op->mac_writel ( efab, &reg, EF1_GMF_L5WM_REG_MAC );
	udelay ( 10 );

	/* Set MAC clock speed */
	ef1002_readl ( efab, &reg, EF1_GM_MAC_CLK_REG );
	mac_clk = ( efab->link_options & LPA_1000 ) ? 0 : 1;
	if ( efab->port == 0 ) {
		EFAB_SET_DWORD_FIELD ( reg, EF1_GM_PORT0_MAC_CLK, mac_clk );
	} else {
		EFAB_SET_DWORD_FIELD ( reg, EF1_GM_PORT1_MAC_CLK, mac_clk );
	}
	ef1002_writel ( efab, &reg, EF1_GM_MAC_CLK_REG );
	udelay ( 10 );

	return 1;
}

/** MDIO write */
static void ef1002_mdio_write ( struct efab_nic *efab, int location,
				int value ) {
	mentormac_mdio_write ( efab, efab->port + 2, location, value );
}

/** MDIO read */
static int ef1002_mdio_read ( struct efab_nic *efab, int location ) {
	return mentormac_mdio_read ( efab, efab->port + 2, location );
}

static struct efab_operations ef1002_operations = {
	.get_membase		= ef1002_get_membase,
	.reset			= ef1002_reset,
	.init_nic		= ef1002_init_nic,
	.read_eeprom		= ef1002_read_eeprom,
	.build_rx_desc		= ef1002_build_rx_desc,
	.notify_rx_desc		= ef1002_notify_rx_desc,
	.build_tx_desc		= ef1002_build_tx_desc,
	.notify_tx_desc		= ef1002_notify_tx_desc,
	.fetch_event		= ef1002_fetch_event,
	.mask_irq		= ef1002_mask_irq,
	.generate_irq		= ef1002_generate_irq,
	.mac_writel		= ef1002_mac_writel,
	.mac_readl		= ef1002_mac_readl,
	.init_mac		= ef1002_init_mac,
	.mdio_write		= ef1002_mdio_write,
	.mdio_read		= ef1002_mdio_read,
};

/**************************************************************************
 *
 * Falcon routines
 *
 **************************************************************************
 */

/* I/O BAR address register */
#define FCN_IOM_IND_ADR_REG 0x0

/* I/O BAR data register */
#define FCN_IOM_IND_DAT_REG 0x4

/* Interrupt enable register */
#define FCN_INT_EN_REG_KER 0x0010
#define FCN_MEM_PERR_INT_EN_KER_LBN 5
#define FCN_MEM_PERR_INT_EN_KER_WIDTH 1
#define FCN_KER_INT_CHAR_LBN 4
#define FCN_KER_INT_CHAR_WIDTH 1
#define FCN_KER_INT_KER_LBN 3
#define FCN_KER_INT_KER_WIDTH 1
#define FCN_ILL_ADR_ERR_INT_EN_KER_LBN 2
#define FCN_ILL_ADR_ERR_INT_EN_KER_WIDTH 1
#define FCN_SRM_PERR_INT_EN_KER_LBN 1
#define FCN_SRM_PERR_INT_EN_KER_WIDTH 1
#define FCN_DRV_INT_EN_KER_LBN 0
#define FCN_DRV_INT_EN_KER_WIDTH 1

/* Interrupt status register */
#define FCN_INT_ADR_REG_KER	0x0030
#define FCN_INT_ADR_KER_LBN 0
#define FCN_INT_ADR_KER_WIDTH EFAB_DMA_TYPE_WIDTH ( 64 )

/* Interrupt acknowledge register */
#define FCN_INT_ACK_KER_REG 0x0050

/* SPI host command register */
#define FCN_EE_SPI_HCMD_REG_KER 0x0100
#define FCN_EE_SPI_HCMD_CMD_EN_LBN 31
#define FCN_EE_SPI_HCMD_CMD_EN_WIDTH 1
#define FCN_EE_WR_TIMER_ACTIVE_LBN 28
#define FCN_EE_WR_TIMER_ACTIVE_WIDTH 1
#define FCN_EE_SPI_HCMD_SF_SEL_LBN 24
#define FCN_EE_SPI_HCMD_SF_SEL_WIDTH 1
#define FCN_EE_SPI_EEPROM 0
#define FCN_EE_SPI_FLASH 1
#define FCN_EE_SPI_HCMD_DABCNT_LBN 16
#define FCN_EE_SPI_HCMD_DABCNT_WIDTH 5
#define FCN_EE_SPI_HCMD_READ_LBN 15
#define FCN_EE_SPI_HCMD_READ_WIDTH 1
#define FCN_EE_SPI_READ 1
#define FCN_EE_SPI_WRITE 0
#define FCN_EE_SPI_HCMD_DUBCNT_LBN 12
#define FCN_EE_SPI_HCMD_DUBCNT_WIDTH 2
#define FCN_EE_SPI_HCMD_ADBCNT_LBN 8
#define FCN_EE_SPI_HCMD_ADBCNT_WIDTH 2
#define FCN_EE_SPI_HCMD_ENC_LBN 0
#define FCN_EE_SPI_HCMD_ENC_WIDTH 8

/* SPI host address register */
#define FCN_EE_SPI_HADR_REG_KER 0x0110
#define FCN_EE_SPI_HADR_DUBYTE_LBN 24
#define FCN_EE_SPI_HADR_DUBYTE_WIDTH 8
#define FCN_EE_SPI_HADR_ADR_LBN 0
#define FCN_EE_SPI_HADR_ADR_WIDTH 24

/* SPI host data register */
#define FCN_EE_SPI_HDATA_REG_KER 0x0120
#define FCN_EE_SPI_HDATA3_LBN 96
#define FCN_EE_SPI_HDATA3_WIDTH 32
#define FCN_EE_SPI_HDATA2_LBN 64
#define FCN_EE_SPI_HDATA2_WIDTH 32
#define FCN_EE_SPI_HDATA1_LBN 32
#define FCN_EE_SPI_HDATA1_WIDTH 32
#define FCN_EE_SPI_HDATA0_LBN 0
#define FCN_EE_SPI_HDATA0_WIDTH 32

/* GPIO control register */
#define FCN_GPIO_CTL_REG_KER 0x0210
#define FCN_FLASH_PRESENT_LBN 7
#define FCN_FLASH_PRESENT_WIDTH 1
#define FCN_EEPROM_PRESENT_LBN 6
#define FCN_EEPROM_PRESENT_WIDTH 1

/* Global control register */
#define FCN_GLB_CTL_REG_KER	0x0220
#define FCN_EXT_PHY_RST_CTL_LBN 63
#define FCN_EXT_PHY_RST_CTL_WIDTH 1
#define FCN_PCIE_SD_RST_CTL_LBN 61
#define FCN_PCIE_SD_RST_CTL_WIDTH 1
#define FCN_PCIX_RST_CTL_LBN 60
#define FCN_PCIX_RST_CTL_WIDTH 1
#define FCN_RST_EXT_PHY_LBN 31
#define FCN_RST_EXT_PHY_WIDTH 1
#define FCN_INT_RST_DUR_LBN 4
#define FCN_INT_RST_DUR_WIDTH 3
#define FCN_EXT_PHY_RST_DUR_LBN 1
#define FCN_EXT_PHY_RST_DUR_WIDTH 3
#define FCN_SWRST_LBN 0
#define FCN_SWRST_WIDTH 1
#define FCN_INCLUDE_IN_RESET 0
#define FCN_EXCLUDE_FROM_RESET 1

/* Timer table for kernel access */
#define FCN_TIMER_CMD_REG_KER 0x420
#define FCN_TIMER_MODE_LBN 12
#define FCN_TIMER_MODE_WIDTH 2
#define FCN_TIMER_MODE_DIS 0
#define FCN_TIMER_MODE_INT_HLDOFF 1
#define FCN_TIMER_VAL_LBN 0
#define FCN_TIMER_VAL_WIDTH 12

/* SRAM receive descriptor cache configuration register */
#define FCN_SRM_RX_DC_CFG_REG_KER 0x610
#define FCN_SRM_RX_DC_BASE_ADR_LBN 0
#define FCN_SRM_RX_DC_BASE_ADR_WIDTH 21

/* SRAM transmit descriptor cache configuration register */
#define FCN_SRM_TX_DC_CFG_REG_KER 0x620
#define FCN_SRM_TX_DC_BASE_ADR_LBN 0
#define FCN_SRM_TX_DC_BASE_ADR_WIDTH 21

/* Receive filter control register */
#define FCN_RX_FILTER_CTL_REG_KER 0x810
#define FCN_NUM_KER_LBN 24
#define FCN_NUM_KER_WIDTH 2

/* Receive descriptor update register */
#define FCN_RX_DESC_UPD_REG_KER 0x0830
#define FCN_RX_DESC_WPTR_LBN 96
#define FCN_RX_DESC_WPTR_WIDTH 12
#define FCN_RX_DESC_UPD_REG_KER_DWORD ( FCN_RX_DESC_UPD_REG_KER + 12 )
#define FCN_RX_DESC_WPTR_DWORD_LBN 0
#define FCN_RX_DESC_WPTR_DWORD_WIDTH 12

/* Receive descriptor cache configuration register */
#define FCN_RX_DC_CFG_REG_KER 0x840
#define FCN_RX_DC_SIZE_LBN 0
#define FCN_RX_DC_SIZE_WIDTH 2

/* Transmit descriptor update register */
#define FCN_TX_DESC_UPD_REG_KER 0x0a10
#define FCN_TX_DESC_WPTR_LBN 96
#define FCN_TX_DESC_WPTR_WIDTH 12
#define FCN_TX_DESC_UPD_REG_KER_DWORD ( FCN_TX_DESC_UPD_REG_KER + 12 )
#define FCN_TX_DESC_WPTR_DWORD_LBN 0
#define FCN_TX_DESC_WPTR_DWORD_WIDTH 12

/* Transmit descriptor cache configuration register */
#define FCN_TX_DC_CFG_REG_KER 0xa20
#define FCN_TX_DC_SIZE_LBN 0
#define FCN_TX_DC_SIZE_WIDTH 2

/* PHY management transmit data register */
#define FCN_MD_TXD_REG_KER 0xc00
#define FCN_MD_TXD_LBN 0
#define FCN_MD_TXD_WIDTH 16

/* PHY management receive data register */
#define FCN_MD_RXD_REG_KER 0xc10
#define FCN_MD_RXD_LBN 0
#define FCN_MD_RXD_WIDTH 16

/* PHY management configuration & status register */
#define FCN_MD_CS_REG_KER 0xc20
#define FCN_MD_GC_LBN 4
#define FCN_MD_GC_WIDTH 1
#define FCN_MD_RIC_LBN 2
#define FCN_MD_RIC_WIDTH 1
#define FCN_MD_WRC_LBN 0
#define FCN_MD_WRC_WIDTH 1

/* PHY management PHY address register */
#define FCN_MD_PHY_ADR_REG_KER 0xc30
#define FCN_MD_PHY_ADR_LBN 0
#define FCN_MD_PHY_ADR_WIDTH 16

/* PHY management ID register */
#define FCN_MD_ID_REG_KER 0xc40
#define FCN_MD_PRT_ADR_LBN 11
#define FCN_MD_PRT_ADR_WIDTH 5
#define FCN_MD_DEV_ADR_LBN 6
#define FCN_MD_DEV_ADR_WIDTH 5

/* PHY management status & mask register */
#define FCN_MD_STAT_REG_KER 0xc50
#define FCN_MD_BSY_LBN 0
#define FCN_MD_BSY_WIDTH 1

/* Port 0 and 1 MAC control registers */
#define FCN_MAC0_CTRL_REG_KER 0xc80
#define FCN_MAC1_CTRL_REG_KER 0xc90
#define FCN_MAC_XOFF_VAL_LBN 16
#define FCN_MAC_XOFF_VAL_WIDTH 16
#define FCN_MAC_BCAD_ACPT_LBN 4
#define FCN_MAC_BCAD_ACPT_WIDTH 1
#define FCN_MAC_UC_PROM_LBN 3
#define FCN_MAC_UC_PROM_WIDTH 1
#define FCN_MAC_LINK_STATUS_LBN 2
#define FCN_MAC_LINK_STATUS_WIDTH 1
#define FCN_MAC_SPEED_LBN 0
#define FCN_MAC_SPEED_WIDTH 2

/* XGMAC global configuration - port 0*/
#define FCN_XM_GLB_CFG_REG_P0_KER 0x1220
#define FCN_XM_RX_STAT_EN_LBN 11
#define FCN_XM_RX_STAT_EN_WIDTH 1
#define FCN_XM_TX_STAT_EN_LBN 10
#define FCN_XM_TX_STAT_EN_WIDTH 1
#define FCN_XM_CUT_THRU_MODE_LBN 7
#define FCN_XM_CUT_THRU_MODE_WIDTH 1
#define FCN_XM_RX_JUMBO_MODE_LBN 6
#define FCN_XM_RX_JUMBO_MODE_WIDTH 1

/* XGMAC transmit configuration - port 0 */
#define FCN_XM_TX_CFG_REG_P0_KER 0x1230
#define FCN_XM_IPG_LBN 16
#define FCN_XM_IPG_WIDTH 4
#define FCN_XM_WTF_DOES_THIS_DO_LBN 9
#define FCN_XM_WTF_DOES_THIS_DO_WIDTH 1
#define FCN_XM_TXCRC_LBN 8
#define FCN_XM_TXCRC_WIDTH 1
#define FCN_XM_AUTO_PAD_LBN 5
#define FCN_XM_AUTO_PAD_WIDTH 1
#define FCN_XM_TX_PRMBL_LBN 2
#define FCN_XM_TX_PRMBL_WIDTH 1
#define FCN_XM_TXEN_LBN 1
#define FCN_XM_TXEN_WIDTH 1

/* XGMAC receive configuration - port 0 */
#define FCN_XM_RX_CFG_REG_P0_KER 0x1240
#define FCN_XM_PASS_CRC_ERR_LBN 25
#define FCN_XM_PASS_CRC_ERR_WIDTH 1
#define FCN_XM_AUTO_DEPAD_LBN 8
#define FCN_XM_AUTO_DEPAD_WIDTH 1
#define FCN_XM_RXEN_LBN 1
#define FCN_XM_RXEN_WIDTH 1

/* Receive descriptor pointer table */
#define FCN_RX_DESC_PTR_TBL_KER 0x11800
#define FCN_RX_DESCQ_BUF_BASE_ID_LBN 36
#define FCN_RX_DESCQ_BUF_BASE_ID_WIDTH 20
#define FCN_RX_DESCQ_EVQ_ID_LBN 24
#define FCN_RX_DESCQ_EVQ_ID_WIDTH 12
#define FCN_RX_DESCQ_OWNER_ID_LBN 10
#define FCN_RX_DESCQ_OWNER_ID_WIDTH 14
#define FCN_RX_DESCQ_SIZE_LBN 3
#define FCN_RX_DESCQ_SIZE_WIDTH 2
#define FCN_RX_DESCQ_SIZE_4K 3
#define FCN_RX_DESCQ_SIZE_2K 2
#define FCN_RX_DESCQ_SIZE_1K 1
#define FCN_RX_DESCQ_SIZE_512 0
#define FCN_RX_DESCQ_TYPE_LBN 2
#define FCN_RX_DESCQ_TYPE_WIDTH 1
#define FCN_RX_DESCQ_JUMBO_LBN 1
#define FCN_RX_DESCQ_JUMBO_WIDTH 1
#define FCN_RX_DESCQ_EN_LBN 0
#define FCN_RX_DESCQ_EN_WIDTH 1

/* Transmit descriptor pointer table */
#define FCN_TX_DESC_PTR_TBL_KER 0x11900
#define FCN_TX_DESCQ_EN_LBN 88
#define FCN_TX_DESCQ_EN_WIDTH 1
#define FCN_TX_DESCQ_BUF_BASE_ID_LBN 36
#define FCN_TX_DESCQ_BUF_BASE_ID_WIDTH 20
#define FCN_TX_DESCQ_EVQ_ID_LBN 24
#define FCN_TX_DESCQ_EVQ_ID_WIDTH 12
#define FCN_TX_DESCQ_OWNER_ID_LBN 10
#define FCN_TX_DESCQ_OWNER_ID_WIDTH 14
#define FCN_TX_DESCQ_SIZE_LBN 3
#define FCN_TX_DESCQ_SIZE_WIDTH 2
#define FCN_TX_DESCQ_SIZE_4K 3
#define FCN_TX_DESCQ_SIZE_2K 2
#define FCN_TX_DESCQ_SIZE_1K 1
#define FCN_TX_DESCQ_SIZE_512 0
#define FCN_TX_DESCQ_TYPE_LBN 1
#define FCN_TX_DESCQ_TYPE_WIDTH 2
#define FCN_TX_DESCQ_FLUSH_LBN 0
#define FCN_TX_DESCQ_FLUSH_WIDTH 1

/* Event queue pointer */
#define FCN_EVQ_PTR_TBL_KER 0x11a00
#define FCN_EVQ_EN_LBN 23
#define FCN_EVQ_EN_WIDTH 1
#define FCN_EVQ_SIZE_LBN 20
#define FCN_EVQ_SIZE_WIDTH 3
#define FCN_EVQ_SIZE_32K 6
#define FCN_EVQ_SIZE_16K 5
#define FCN_EVQ_SIZE_8K 4
#define FCN_EVQ_SIZE_4K 3
#define FCN_EVQ_SIZE_2K 2
#define FCN_EVQ_SIZE_1K 1
#define FCN_EVQ_SIZE_512 0
#define FCN_EVQ_BUF_BASE_ID_LBN 0
#define FCN_EVQ_BUF_BASE_ID_WIDTH 20

/* Event queue read pointer */
#define FCN_EVQ_RPTR_REG_KER 0x11b00
#define FCN_EVQ_RPTR_LBN 0
#define FCN_EVQ_RPTR_WIDTH 14
#define FCN_EVQ_RPTR_REG_KER_DWORD ( FCN_EVQ_RPTR_REG_KER + 0 )
#define FCN_EVQ_RPTR_DWORD_LBN 0
#define FCN_EVQ_RPTR_DWORD_WIDTH 14

/* Special buffer descriptors */
#define FCN_BUF_FULL_TBL_KER 0x18000
#define FCN_IP_DAT_BUF_SIZE_LBN 50
#define FCN_IP_DAT_BUF_SIZE_WIDTH 1
#define FCN_IP_DAT_BUF_SIZE_8K 1
#define FCN_IP_DAT_BUF_SIZE_4K 0
#define FCN_BUF_ADR_FBUF_LBN 14
#define FCN_BUF_ADR_FBUF_WIDTH 34
#define FCN_BUF_OWNER_ID_FBUF_LBN 0
#define FCN_BUF_OWNER_ID_FBUF_WIDTH 14

/* MAC registers */
#define FALCON_MAC_REGBANK 0xe00
#define FALCON_MAC_REGBANK_SIZE 0x200
#define FALCON_MAC_REG_SIZE 0x10

/** Offset of a MAC register within Falcon */
#define FALCON_MAC_REG( efab, mac_reg )				\
	( FALCON_MAC_REGBANK +					\
	  ( (efab)->port * FALCON_MAC_REGBANK_SIZE ) +		\
	  ( (mac_reg) * FALCON_MAC_REG_SIZE ) )
#define FCN_MAC_DATA_LBN 0
#define FCN_MAC_DATA_WIDTH 32

/* Transmit descriptor */
#define FCN_TX_KER_PORT_LBN 63
#define FCN_TX_KER_PORT_WIDTH 1
#define FCN_TX_KER_BYTE_CNT_LBN 48
#define FCN_TX_KER_BYTE_CNT_WIDTH 14
#define FCN_TX_KER_BUF_ADR_LBN 0
#define FCN_TX_KER_BUF_ADR_WIDTH EFAB_DMA_TYPE_WIDTH ( 46 )

/* Receive descriptor */
#define FCN_RX_KER_BUF_SIZE_LBN 48
#define FCN_RX_KER_BUF_SIZE_WIDTH 14
#define FCN_RX_KER_BUF_ADR_LBN 0
#define FCN_RX_KER_BUF_ADR_WIDTH EFAB_DMA_TYPE_WIDTH ( 46 )

/* Event queue entries */
#define FCN_EV_CODE_LBN 60
#define FCN_EV_CODE_WIDTH 4
#define FCN_RX_IP_EV_DECODE 0
#define FCN_TX_IP_EV_DECODE 2
#define FCN_DRIVER_EV_DECODE 5

/* Receive events */
#define FCN_RX_PORT_LBN 30
#define FCN_RX_PORT_WIDTH 1
#define FCN_RX_EV_BYTE_CNT_LBN 16
#define FCN_RX_EV_BYTE_CNT_WIDTH 14
#define FCN_RX_EV_DESC_PTR_LBN 0
#define FCN_RX_EV_DESC_PTR_WIDTH 12

/* Transmit events */
#define FCN_TX_EV_DESC_PTR_LBN 0
#define FCN_TX_EV_DESC_PTR_WIDTH 12

/* Fixed special buffer numbers to use */
#define FALCON_EVQ_ID 0
#define FALCON_TXD_ID 1
#define FALCON_RXD_ID 2

#if FALCON_USE_IO_BAR

/* Write dword via the I/O BAR */
static inline void _falcon_writel ( struct efab_nic *efab, uint32_t value,
				    unsigned int reg ) {
	outl ( reg, efab->iobase + FCN_IOM_IND_ADR_REG );
	outl ( value, efab->iobase + FCN_IOM_IND_DAT_REG );
}

/* Read dword via the I/O BAR */
static inline uint32_t _falcon_readl ( struct efab_nic *efab,
				       unsigned int reg ) {
	outl ( reg, efab->iobase + FCN_IOM_IND_ADR_REG );
	return inl ( efab->iobase + FCN_IOM_IND_DAT_REG );
}

#else /* FALCON_USE_IO_BAR */

#define _falcon_writel( efab, value, reg ) \
	writel ( (value), (efab)->membase + (reg) )
#define _falcon_readl( efab, reg ) readl ( (efab)->membase + (reg) )

#endif /* FALCON_USE_IO_BAR */

/**
 * Write to a Falcon register
 *
 */
static inline void falcon_write ( struct efab_nic *efab, efab_oword_t *value,
				  unsigned int reg ) {

	EFAB_REGDUMP ( "Writing register %x with " EFAB_OWORD_FMT "\n",
		       reg, EFAB_OWORD_VAL ( *value ) );

	_falcon_writel ( efab, value->u32[0], reg + 0  );
	_falcon_writel ( efab, value->u32[1], reg + 4  );
	_falcon_writel ( efab, value->u32[2], reg + 8  );
	_falcon_writel ( efab, value->u32[3], reg + 12 );
	wmb();
}

/**
 * Write to Falcon SRAM
 *
 */
static inline void falcon_write_sram ( struct efab_nic *efab,
				       efab_qword_t *value,
				       unsigned int index ) {
	unsigned int reg = ( FCN_BUF_FULL_TBL_KER +
			     ( index * sizeof ( *value ) ) );

	EFAB_REGDUMP ( "Writing SRAM register %x with " EFAB_QWORD_FMT "\n",
		       reg, EFAB_QWORD_VAL ( *value ) );

	_falcon_writel ( efab, value->u32[0], reg + 0  );
	_falcon_writel ( efab, value->u32[1], reg + 4  );
	wmb();
}

/**
 * Write dword to Falcon register that allows partial writes
 *
 */
static inline void falcon_writel ( struct efab_nic *efab, efab_dword_t *value,
				   unsigned int reg ) {
	EFAB_REGDUMP ( "Writing partial register %x with " EFAB_DWORD_FMT "\n",
		       reg, EFAB_DWORD_VAL ( *value ) );
	_falcon_writel ( efab, value->u32[0], reg );
}

/**
 * Read from a Falcon register
 *
 */
static inline void falcon_read ( struct efab_nic *efab, efab_oword_t *value,
				 unsigned int reg ) {
	value->u32[0] = _falcon_readl ( efab, reg + 0  );
	value->u32[1] = _falcon_readl ( efab, reg + 4  );
	value->u32[2] = _falcon_readl ( efab, reg + 8  );
	value->u32[3] = _falcon_readl ( efab, reg + 12 );

	EFAB_REGDUMP ( "Read from register %x, got " EFAB_OWORD_FMT "\n",
		       reg, EFAB_OWORD_VAL ( *value ) );
}

/** 
 * Read from Falcon SRAM
 *
 */
static inline void falcon_read_sram ( struct efab_nic *efab,
				      efab_qword_t *value,
				      unsigned int index ) {
	unsigned int reg = ( FCN_BUF_FULL_TBL_KER +
			     ( index * sizeof ( *value ) ) );

	value->u32[0] = _falcon_readl ( efab, reg + 0 );
	value->u32[1] = _falcon_readl ( efab, reg + 4 );
	EFAB_REGDUMP ( "Read from SRAM register %x, got " EFAB_QWORD_FMT "\n",
		       reg, EFAB_QWORD_VAL ( *value ) );
}

/**
 * Read dword from a portion of a Falcon register
 *
 */
static inline void falcon_readl ( struct efab_nic *efab, efab_dword_t *value,
				  unsigned int reg ) {
	value->u32[0] = _falcon_readl ( efab, reg );
	EFAB_REGDUMP ( "Read from register %x, got " EFAB_DWORD_FMT "\n",
		       reg, EFAB_DWORD_VAL ( *value ) );
}

/**
 * Verified write to Falcon SRAM
 *
 */
static inline void falcon_write_sram_verify ( struct efab_nic *efab,
					     efab_qword_t *value,
					     unsigned int index ) {
	efab_qword_t verify;
	
	falcon_write_sram ( efab, value, index );
	udelay ( 1000 );
	falcon_read_sram ( efab, &verify, index );
	if ( memcmp ( &verify, value, sizeof ( verify ) ) != 0 ) {
		printf ( "SRAM index %x failure: wrote " EFAB_QWORD_FMT
			 " got " EFAB_QWORD_FMT "\n", index,
			 EFAB_QWORD_VAL ( *value ),
			 EFAB_QWORD_VAL ( verify ) );
	}
}

/**
 * Get memory base
 *
 */
static void falcon_get_membase ( struct efab_nic *efab ) {
	unsigned long membase_phys;

	membase_phys = pci_bar_start ( efab->pci, PCI_BASE_ADDRESS_2 );
	efab->membase = ioremap ( membase_phys, 0x20000 );
}

#define FCN_DUMP_REG( efab, _reg ) do {				\
		efab_oword_t reg;				\
		falcon_read ( efab, &reg, _reg );		\
		printf ( #_reg " = " EFAB_OWORD_FMT "\n",	\
			 EFAB_OWORD_VAL ( reg ) );		\
	} while ( 0 );

#define FCN_DUMP_MAC_REG( efab, _mac_reg ) do {			\
		efab_dword_t reg;				\
		efab->op->mac_readl ( efab, &reg, _mac_reg );	\
		printf ( #_mac_reg " = " EFAB_DWORD_FMT "\n",	\
			 EFAB_DWORD_VAL ( reg ) );		\
	} while ( 0 );

/**
 * Dump register contents (for debugging)
 *
 * Marked as static inline so that it will not be compiled in if not
 * used.
 */
static inline void falcon_dump_regs ( struct efab_nic *efab ) {
	FCN_DUMP_REG ( efab, FCN_INT_EN_REG_KER );
	FCN_DUMP_REG ( efab, FCN_INT_ADR_REG_KER );
	FCN_DUMP_REG ( efab, FCN_GLB_CTL_REG_KER );
	FCN_DUMP_REG ( efab, FCN_TIMER_CMD_REG_KER );
	FCN_DUMP_REG ( efab, FCN_SRM_RX_DC_CFG_REG_KER );
	FCN_DUMP_REG ( efab, FCN_SRM_TX_DC_CFG_REG_KER );
	FCN_DUMP_REG ( efab, FCN_RX_FILTER_CTL_REG_KER );
	FCN_DUMP_REG ( efab, FCN_RX_DC_CFG_REG_KER );
	FCN_DUMP_REG ( efab, FCN_TX_DC_CFG_REG_KER );
	FCN_DUMP_REG ( efab, FCN_MAC0_CTRL_REG_KER );
	FCN_DUMP_REG ( efab, FCN_MAC1_CTRL_REG_KER );
	FCN_DUMP_REG ( efab, FCN_XM_GLB_CFG_REG_P0_KER );
	FCN_DUMP_REG ( efab, FCN_XM_TX_CFG_REG_P0_KER );
	FCN_DUMP_REG ( efab, FCN_XM_RX_CFG_REG_P0_KER );
	FCN_DUMP_REG ( efab, FCN_RX_DESC_PTR_TBL_KER );
	FCN_DUMP_REG ( efab, FCN_TX_DESC_PTR_TBL_KER );
	FCN_DUMP_REG ( efab, FCN_EVQ_PTR_TBL_KER );
	FCN_DUMP_MAC_REG ( efab, GM_CFG1_REG_MAC );
	FCN_DUMP_MAC_REG ( efab, GM_CFG2_REG_MAC );
	FCN_DUMP_MAC_REG ( efab, GM_MAX_FLEN_REG_MAC );
	FCN_DUMP_MAC_REG ( efab, GM_MII_MGMT_CFG_REG_MAC );
	FCN_DUMP_MAC_REG ( efab, GM_ADR1_REG_MAC );
	FCN_DUMP_MAC_REG ( efab, GM_ADR2_REG_MAC );
	FCN_DUMP_MAC_REG ( efab, GMF_CFG0_REG_MAC );
	FCN_DUMP_MAC_REG ( efab, GMF_CFG1_REG_MAC );
	FCN_DUMP_MAC_REG ( efab, GMF_CFG2_REG_MAC );
	FCN_DUMP_MAC_REG ( efab, GMF_CFG3_REG_MAC );
	FCN_DUMP_MAC_REG ( efab, GMF_CFG4_REG_MAC );
	FCN_DUMP_MAC_REG ( efab, GMF_CFG5_REG_MAC );
}

/**
 * Create special buffer
 *
 */
static void falcon_create_special_buffer ( struct efab_nic *efab,
					   void *addr, unsigned int index ) {
	efab_qword_t buf_desc;
	unsigned long dma_addr;

	memset ( addr, 0, 4096 );
	dma_addr = virt_to_bus ( addr );
	EFAB_ASSERT ( ( dma_addr & ( EFAB_BUF_ALIGN - 1 ) ) == 0 );
	EFAB_POPULATE_QWORD_3 ( buf_desc,
				FCN_IP_DAT_BUF_SIZE, FCN_IP_DAT_BUF_SIZE_4K,
				FCN_BUF_ADR_FBUF, ( dma_addr >> 12 ),
				FCN_BUF_OWNER_ID_FBUF, 0 );
	falcon_write_sram_verify ( efab, &buf_desc, index );
}

/**
 * Update event queue read pointer
 *
 */
static void falcon_eventq_read_ack ( struct efab_nic *efab ) {
	efab_dword_t reg;

	EFAB_ASSERT ( efab->eventq_read_ptr < EFAB_EVQ_SIZE );

	EFAB_POPULATE_DWORD_1 ( reg, FCN_EVQ_RPTR_DWORD,
				efab->eventq_read_ptr );
	falcon_writel ( efab, &reg, FCN_EVQ_RPTR_REG_KER_DWORD );
}

/**
 * Reset device
 *
 */
static int falcon_reset ( struct efab_nic *efab ) {
	efab_oword_t glb_ctl_reg_ker;

	/* Initiate software reset */
	EFAB_POPULATE_OWORD_5 ( glb_ctl_reg_ker,
				FCN_EXT_PHY_RST_CTL, FCN_EXCLUDE_FROM_RESET,
				FCN_PCIE_SD_RST_CTL, FCN_EXCLUDE_FROM_RESET,
				FCN_PCIX_RST_CTL, FCN_EXCLUDE_FROM_RESET,
				FCN_INT_RST_DUR, 0x7 /* datasheet */,
				FCN_SWRST, 1 );
	falcon_write ( efab, &glb_ctl_reg_ker, FCN_GLB_CTL_REG_KER );

	/* Allow 20ms for reset */
	mdelay ( 20 );

	/* Check for device reset complete */
	falcon_read ( efab, &glb_ctl_reg_ker, FCN_GLB_CTL_REG_KER );
	if ( EFAB_OWORD_FIELD ( glb_ctl_reg_ker, FCN_SWRST ) != 0 ) {
		printf ( "Reset failed\n" );
		return 0;
	}

	return 1;
}

/**
 * Initialise NIC
 *
 */
static int falcon_init_nic ( struct efab_nic *efab ) {
	efab_oword_t reg;
	efab_dword_t timer_cmd;

	/* Set up TX and RX descriptor caches in SRAM */
	EFAB_POPULATE_OWORD_1 ( reg, FCN_SRM_TX_DC_BASE_ADR,
				0x130000 /* recommended in datasheet */ );
	falcon_write ( efab, &reg, FCN_SRM_TX_DC_CFG_REG_KER );
	EFAB_POPULATE_OWORD_1 ( reg, FCN_TX_DC_SIZE, 2 /* 32 descriptors */ );
	falcon_write ( efab, &reg, FCN_TX_DC_CFG_REG_KER );
	EFAB_POPULATE_OWORD_1 ( reg, FCN_SRM_RX_DC_BASE_ADR,
				0x100000 /* recommended in datasheet */ );
	falcon_write ( efab, &reg, FCN_SRM_RX_DC_CFG_REG_KER );
	EFAB_POPULATE_OWORD_1 ( reg, FCN_RX_DC_SIZE, 2 /* 32 descriptors */ );
	falcon_write ( efab, &reg, FCN_RX_DC_CFG_REG_KER );
	
	/* Set number of RSS CPUs */
	EFAB_POPULATE_OWORD_1 ( reg, FCN_NUM_KER, 0 );
	falcon_write ( efab, &reg, FCN_RX_FILTER_CTL_REG_KER );
	udelay ( 1000 );
	
	/* Reset the MAC */
	mentormac_reset ( efab, 1 );
	/* Take MAC out of reset */
	mentormac_reset ( efab, 0 );

	/* Set up event queue */
	falcon_create_special_buffer ( efab, efab->eventq, FALCON_EVQ_ID );
	EFAB_POPULATE_OWORD_3 ( reg,
				FCN_EVQ_EN, 1,
				FCN_EVQ_SIZE, FCN_EVQ_SIZE_512,
				FCN_EVQ_BUF_BASE_ID, FALCON_EVQ_ID );
	falcon_write ( efab, &reg, FCN_EVQ_PTR_TBL_KER );
	udelay ( 1000 );

	/* Set timer register */
	EFAB_POPULATE_DWORD_2 ( timer_cmd,
				FCN_TIMER_MODE, FCN_TIMER_MODE_DIS,
				FCN_TIMER_VAL, 0 );
	falcon_writel ( efab, &timer_cmd, FCN_TIMER_CMD_REG_KER );
	udelay ( 1000 );

	/* Initialise event queue read pointer */
	falcon_eventq_read_ack ( efab );
	
	/* Set up TX descriptor ring */
	falcon_create_special_buffer ( efab, efab->txd, FALCON_TXD_ID );
	EFAB_POPULATE_OWORD_5 ( reg,
				FCN_TX_DESCQ_EN, 1,
				FCN_TX_DESCQ_BUF_BASE_ID, FALCON_TXD_ID,
				FCN_TX_DESCQ_EVQ_ID, 0,
				FCN_TX_DESCQ_SIZE, FCN_TX_DESCQ_SIZE_512,
				FCN_TX_DESCQ_TYPE, 0 /* kernel queue */ );
	falcon_write ( efab, &reg, FCN_TX_DESC_PTR_TBL_KER );

	/* Set up RX descriptor ring */
	falcon_create_special_buffer ( efab, efab->rxd, FALCON_RXD_ID );
	EFAB_POPULATE_OWORD_6 ( reg,
				FCN_RX_DESCQ_BUF_BASE_ID, FALCON_RXD_ID,
				FCN_RX_DESCQ_EVQ_ID, 0,
				FCN_RX_DESCQ_SIZE, FCN_RX_DESCQ_SIZE_512,
				FCN_RX_DESCQ_TYPE, 0 /* kernel queue */,
				FCN_RX_DESCQ_JUMBO, 1,
				FCN_RX_DESCQ_EN, 1 );
	falcon_write ( efab, &reg, FCN_RX_DESC_PTR_TBL_KER );

	/* Program INT_ADR_REG_KER */
	EFAB_POPULATE_OWORD_1 ( reg,
				FCN_INT_ADR_KER,
				virt_to_bus ( &efab->int_ker ) );
	falcon_write ( efab, &reg, FCN_INT_ADR_REG_KER );
	udelay ( 1000 );

	return 1;
}

/** SPI device */
struct efab_spi_device {
	/** Device ID */
	unsigned int device_id;
	/** Address length (in bytes) */
	unsigned int addr_len;
	/** Read command */
	unsigned int read_command;
};

/**
 * Wait for SPI command completion
 *
 */
static int falcon_spi_wait ( struct efab_nic *efab ) {
	efab_oword_t reg;
	int count;

	count = 0;
	do {
		udelay ( 100 );
		falcon_read ( efab, &reg, FCN_EE_SPI_HCMD_REG_KER );
		if ( EFAB_OWORD_FIELD ( reg, FCN_EE_SPI_HCMD_CMD_EN ) == 0 )
			return 1;
	} while ( ++count < 1000 );
	printf ( "Timed out waiting for SPI\n" );
	return 0;
}

/**
 * Perform SPI read
 *
 */
static int falcon_spi_read ( struct efab_nic *efab,
			     struct efab_spi_device *spi,
			     int address, void *data, unsigned int len ) {
	efab_oword_t reg;

	/* Program address register */
	EFAB_POPULATE_OWORD_1 ( reg, FCN_EE_SPI_HADR_ADR, address );
	falcon_write ( efab, &reg, FCN_EE_SPI_HADR_REG_KER );
	
	/* Issue read command */
	EFAB_POPULATE_OWORD_7 ( reg,
				FCN_EE_SPI_HCMD_CMD_EN, 1, 
				FCN_EE_SPI_HCMD_SF_SEL, spi->device_id,
				FCN_EE_SPI_HCMD_DABCNT, len,
				FCN_EE_SPI_HCMD_READ, FCN_EE_SPI_READ,
				FCN_EE_SPI_HCMD_DUBCNT, 0,
				FCN_EE_SPI_HCMD_ADBCNT, spi->addr_len,
				FCN_EE_SPI_HCMD_ENC, spi->read_command );
	falcon_write ( efab, &reg, FCN_EE_SPI_HCMD_REG_KER );
	
	/* Wait for read to complete */
	if ( ! falcon_spi_wait ( efab ) )
		return 0;
	
	/* Read data */
	falcon_read ( efab, &reg, FCN_EE_SPI_HDATA_REG_KER );
	memcpy ( data, &reg, len );

	return 1;
}

#define SPI_READ_CMD 0x03
#define AT25F1024_ADDR_LEN 3
#define AT25F1024_READ_CMD SPI_READ_CMD
#define MC25XX640_ADDR_LEN 2
#define MC25XX640_READ_CMD SPI_READ_CMD

/** Falcon Flash SPI device */
static struct efab_spi_device falcon_spi_flash = {
	.device_id	= FCN_EE_SPI_FLASH,
	.addr_len	= AT25F1024_ADDR_LEN,
	.read_command	= AT25F1024_READ_CMD,
};

/** Falcon EEPROM SPI device */
static struct efab_spi_device falcon_spi_large_eeprom = {
	.device_id	= FCN_EE_SPI_EEPROM,
	.addr_len	= MC25XX640_ADDR_LEN,
	.read_command	= MC25XX640_READ_CMD,
};

/** Offset of MAC address within EEPROM or Flash */
#define FALCON_MAC_ADDRESS_OFFSET(port) ( 0x310 + 0x08 * (port) )

/**
 * Read MAC address from EEPROM
 *
 */
static int falcon_read_eeprom ( struct efab_nic *efab ) {
	efab_oword_t reg;
	int has_flash;
	struct efab_spi_device *spi;

	/* Determine the SPI device containing the MAC address */
	falcon_read ( efab, &reg, FCN_GPIO_CTL_REG_KER );
	has_flash = EFAB_OWORD_FIELD ( reg, FCN_FLASH_PRESENT );
	spi = has_flash ? &falcon_spi_flash : &falcon_spi_large_eeprom;

	return falcon_spi_read ( efab, spi,
				 FALCON_MAC_ADDRESS_OFFSET ( efab->port ),
				 efab->mac_addr, sizeof ( efab->mac_addr ) );
}

/** RX descriptor */
typedef efab_qword_t falcon_rx_desc_t;

/**
 * Build RX descriptor
 *
 */
static void falcon_build_rx_desc ( struct efab_nic *efab,
				   struct efab_rx_buf *rx_buf ) {
	falcon_rx_desc_t *rxd;

	rxd = ( ( falcon_rx_desc_t * ) efab->rxd ) + rx_buf->id;
	EFAB_POPULATE_QWORD_2 ( *rxd,
				FCN_RX_KER_BUF_SIZE, EFAB_DATA_BUF_SIZE,
				FCN_RX_KER_BUF_ADR,
				virt_to_bus ( rx_buf->addr ) );
}

/**
 * Update RX descriptor write pointer
 *
 */
static void falcon_notify_rx_desc ( struct efab_nic *efab ) {
	efab_dword_t reg;

	EFAB_POPULATE_DWORD_1 ( reg, FCN_RX_DESC_WPTR_DWORD,
				efab->rx_write_ptr );
	falcon_writel ( efab, &reg, FCN_RX_DESC_UPD_REG_KER_DWORD );
}

/** TX descriptor */
typedef efab_qword_t falcon_tx_desc_t;

/**
 * Build TX descriptor
 *
 */
static void falcon_build_tx_desc ( struct efab_nic *efab,
				   struct efab_tx_buf *tx_buf ) {
	falcon_rx_desc_t *txd;

	txd = ( ( falcon_rx_desc_t * ) efab->txd ) + tx_buf->id;
	EFAB_POPULATE_QWORD_3 ( *txd,
				FCN_TX_KER_PORT, efab->port,
				FCN_TX_KER_BYTE_CNT, tx_buf->len,
				FCN_TX_KER_BUF_ADR,
				virt_to_bus ( tx_buf->addr ) );
}

/**
 * Update TX descriptor write pointer
 *
 */
static void falcon_notify_tx_desc ( struct efab_nic *efab ) {
	efab_dword_t reg;

	EFAB_POPULATE_DWORD_1 ( reg, FCN_TX_DESC_WPTR_DWORD,
				efab->tx_write_ptr );
	falcon_writel ( efab, &reg, FCN_TX_DESC_UPD_REG_KER_DWORD );
}

/** An event */
typedef efab_qword_t falcon_event_t;

/**
 * Retrieve event from event queue
 *
 */
static int falcon_fetch_event ( struct efab_nic *efab,
				struct efab_event *event ) {
	falcon_event_t *evt;
	int ev_code;
	int rx_port;

	/* Check for event */
	evt = ( ( falcon_event_t * ) efab->eventq ) + efab->eventq_read_ptr;
	if ( EFAB_QWORD_IS_ZERO ( *evt ) ) {
		/* No event */
		return 0;
	}
	
	DBG ( "Event is " EFAB_QWORD_FMT "\n", EFAB_QWORD_VAL ( *evt ) );

	/* Decode event */
	ev_code = EFAB_QWORD_FIELD ( *evt, FCN_EV_CODE );
	switch ( ev_code ) {
	case FCN_TX_IP_EV_DECODE:
		event->type = EFAB_EV_TX;
		break;
	case FCN_RX_IP_EV_DECODE:
		event->type = EFAB_EV_RX;
		event->rx_id = EFAB_QWORD_FIELD ( *evt, FCN_RX_EV_DESC_PTR );
		event->rx_len = EFAB_QWORD_FIELD ( *evt, FCN_RX_EV_BYTE_CNT );
		rx_port = EFAB_QWORD_FIELD ( *evt, FCN_RX_PORT );
		if ( rx_port != efab->port ) {
			/* Ignore packets on the wrong port.  We can't
			 * just set event->type = EFAB_EV_NONE,
			 * because then the descriptor ring won't get
			 * refilled.
			 */
			event->rx_len = 0;
		}
		break;
	case FCN_DRIVER_EV_DECODE:
		/* Ignore start-of-day events */
		event->type = EFAB_EV_NONE;
		break;
	default:
		printf ( "Unknown event type %d\n", ev_code );
		event->type = EFAB_EV_NONE;
	}

	/* Clear event and any pending interrupts */
	EFAB_ZERO_QWORD ( *evt );
	falcon_writel ( efab, 0, FCN_INT_ACK_KER_REG );
	udelay ( 10 );

	/* Increment and update event queue read pointer */
	efab->eventq_read_ptr = ( ( efab->eventq_read_ptr + 1 )
				  % EFAB_EVQ_SIZE );
	falcon_eventq_read_ack ( efab );

	return 1;
}

/**
 * Enable/disable/generate interrupt
 *
 */
static inline void falcon_interrupts ( struct efab_nic *efab, int enabled,
				       int force ) {
	efab_oword_t int_en_reg_ker;

	EFAB_POPULATE_OWORD_2 ( int_en_reg_ker,
				FCN_KER_INT_KER, force,
				FCN_DRV_INT_EN_KER, enabled );
	falcon_write ( efab, &int_en_reg_ker, FCN_INT_EN_REG_KER );	
}

/**
 * Enable/disable interrupts
 *
 */
static void falcon_mask_irq ( struct efab_nic *efab, int enabled ) {
	falcon_interrupts ( efab, enabled, 0 );
	if ( enabled ) {
		/* Events won't trigger interrupts until we do this */
		falcon_eventq_read_ack ( efab );
	}
}

/**
 * Generate interrupt
 *
 */
static void falcon_generate_irq ( struct efab_nic *efab ) {
	falcon_interrupts ( efab, 1, 1 );
}

/**
 * Write dword to a Falcon MAC register
 *
 */
static void falcon_mac_writel ( struct efab_nic *efab,
				efab_dword_t *value, unsigned int mac_reg ) {
	efab_oword_t temp;

	EFAB_POPULATE_OWORD_1 ( temp, FCN_MAC_DATA,
				EFAB_DWORD_FIELD ( *value, FCN_MAC_DATA ) );
	falcon_write ( efab, &temp, FALCON_MAC_REG ( efab, mac_reg ) );
}

/**
 * Read dword from a Falcon MAC register
 *
 */
static void falcon_mac_readl ( struct efab_nic *efab, efab_dword_t *value,
			       unsigned int mac_reg ) {
	efab_oword_t temp;

	falcon_read ( efab, &temp, FALCON_MAC_REG ( efab, mac_reg ) );
	EFAB_POPULATE_DWORD_1 ( *value, FCN_MAC_DATA,
				EFAB_OWORD_FIELD ( temp, FCN_MAC_DATA ) );
}

/**
 * Initialise MAC
 *
 */
static int falcon_init_mac ( struct efab_nic *efab ) {
	static struct efab_mentormac_parameters falcon_mentormac_params = {
		.gmf_cfgfrth = 0x12,
		.gmf_cfgftth = 0x08,
		.gmf_cfghwmft = 0x1c,
		.gmf_cfghwm = 0x3f,
		.gmf_cfglwm = 0xa,
	};
	efab_oword_t reg;
	int link_speed;

	/* Initialise PHY */
	alaska_init ( efab );

	/* Initialise MAC */
	mentormac_init ( efab, &falcon_mentormac_params );

	/* Configure the Falcon MAC wrapper */
	EFAB_POPULATE_OWORD_4 ( reg,
				FCN_XM_RX_JUMBO_MODE, 0,
				FCN_XM_CUT_THRU_MODE, 0,
				FCN_XM_TX_STAT_EN, 1,
				FCN_XM_RX_STAT_EN, 1);
	falcon_write ( efab, &reg, FCN_XM_GLB_CFG_REG_P0_KER );

	EFAB_POPULATE_OWORD_6 ( reg, 
				FCN_XM_TXEN, 1,
				FCN_XM_TX_PRMBL, 1,
				FCN_XM_AUTO_PAD, 1,
				FCN_XM_TXCRC, 1,
				FCN_XM_WTF_DOES_THIS_DO, 1,
				FCN_XM_IPG, 0x3 );
	falcon_write ( efab, &reg, FCN_XM_TX_CFG_REG_P0_KER );

	EFAB_POPULATE_OWORD_3 ( reg,
				FCN_XM_RXEN, 1,
				FCN_XM_AUTO_DEPAD, 1,
				FCN_XM_PASS_CRC_ERR, 1 );
	falcon_write ( efab, &reg, FCN_XM_RX_CFG_REG_P0_KER );

#warning "10G support not yet present"
#define LPA_10000 0
	if ( efab->link_options & LPA_10000 ) {
		link_speed = 0x3;
	} else if ( efab->link_options & LPA_1000 ) {
		link_speed = 0x2;
	} else if ( efab->link_options & LPA_100 ) {
		link_speed = 0x1;
	} else {
		link_speed = 0x0;
	}
	EFAB_POPULATE_OWORD_5 ( reg,
				FCN_MAC_XOFF_VAL, 0xffff /* datasheet */,
				FCN_MAC_BCAD_ACPT, 1,
				FCN_MAC_UC_PROM, 0,
				FCN_MAC_LINK_STATUS, 1,
				FCN_MAC_SPEED, link_speed );
	falcon_write ( efab, &reg, ( efab->port == 0 ?
			     FCN_MAC0_CTRL_REG_KER : FCN_MAC1_CTRL_REG_KER ) );

	return 1;
}

/**
 * Wait for GMII access to complete
 *
 */
static int falcon_gmii_wait ( struct efab_nic *efab ) {
	efab_oword_t md_stat;
	int count;

	for ( count = 0 ; count < 1000 ; count++ ) {
		udelay ( 10 );
		falcon_read ( efab, &md_stat, FCN_MD_STAT_REG_KER );
		if ( EFAB_OWORD_FIELD ( md_stat, FCN_MD_BSY ) == 0 )
			return 1;
	}
	printf ( "Timed out waiting for GMII\n" );
	return 0;
}

/** MDIO write */
static void falcon_mdio_write ( struct efab_nic *efab, int location,
				int value ) {
	int phy_id = efab->port + 2;
	efab_oword_t reg;

#warning "10G PHY access not yet in place"

	EFAB_TRACE ( "Writing GMII %d register %02x with %04x\n",
		     phy_id, location, value );

	/* Check MII not currently being accessed */
	if ( ! falcon_gmii_wait ( efab ) )
		return;

	/* Write the address registers */
	EFAB_POPULATE_OWORD_1 ( reg, FCN_MD_PHY_ADR, 0 /* phy_id ? */ );
	falcon_write ( efab, &reg, FCN_MD_PHY_ADR_REG_KER );
	udelay ( 10 );
	EFAB_POPULATE_OWORD_2 ( reg,
				FCN_MD_PRT_ADR, phy_id,
				FCN_MD_DEV_ADR, location );
	falcon_write ( efab, &reg, FCN_MD_ID_REG_KER );
	udelay ( 10 );

	/* Write data */
	EFAB_POPULATE_OWORD_1 ( reg, FCN_MD_TXD, value );
	falcon_write ( efab, &reg, FCN_MD_TXD_REG_KER );
	udelay ( 10 );
	EFAB_POPULATE_OWORD_2 ( reg,
				FCN_MD_WRC, 1,
				FCN_MD_GC, 1 );
	falcon_write ( efab, &reg, FCN_MD_CS_REG_KER );
	udelay ( 10 );
	
	/* Wait for data to be written */
	falcon_gmii_wait ( efab );
}

/** MDIO read */
static int falcon_mdio_read ( struct efab_nic *efab, int location ) {
	int phy_id = efab->port + 2;
	efab_oword_t reg;
	int value;

	/* Check MII not currently being accessed */
	if ( ! falcon_gmii_wait ( efab ) )
		return 0xffff;

	/* Write the address registers */
	EFAB_POPULATE_OWORD_1 ( reg, FCN_MD_PHY_ADR, 0 /* phy_id ? */ );
	falcon_write ( efab, &reg, FCN_MD_PHY_ADR_REG_KER );
	udelay ( 10 );
	EFAB_POPULATE_OWORD_2 ( reg,
				FCN_MD_PRT_ADR, phy_id,
				FCN_MD_DEV_ADR, location );
	falcon_write ( efab, &reg, FCN_MD_ID_REG_KER );
	udelay ( 10 );

	/* Request data to be read */
	EFAB_POPULATE_OWORD_2 ( reg,
				FCN_MD_RIC, 1,
				FCN_MD_GC, 1 );
	falcon_write ( efab, &reg, FCN_MD_CS_REG_KER );
	udelay ( 10 );
	
	/* Wait for data to become available */
	falcon_gmii_wait ( efab );

	/* Read the data */
	falcon_read ( efab, &reg, FCN_MD_RXD_REG_KER );
	value = EFAB_OWORD_FIELD ( reg, FCN_MD_RXD );

	EFAB_TRACE ( "Read from GMII %d register %02x, got %04x\n",
		     phy_id, location, value );

	return value;
}

static struct efab_operations falcon_operations = {
	.get_membase		= falcon_get_membase,
	.reset			= falcon_reset,
	.init_nic		= falcon_init_nic,
	.read_eeprom		= falcon_read_eeprom,
	.build_rx_desc		= falcon_build_rx_desc,
	.notify_rx_desc		= falcon_notify_rx_desc,
	.build_tx_desc		= falcon_build_tx_desc,
	.notify_tx_desc		= falcon_notify_tx_desc,
	.fetch_event		= falcon_fetch_event,
	.mask_irq		= falcon_mask_irq,
	.generate_irq		= falcon_generate_irq,
	.mac_writel		= falcon_mac_writel,
	.mac_readl		= falcon_mac_readl,
	.init_mac		= falcon_init_mac,
	.mdio_write		= falcon_mdio_write,
	.mdio_read		= falcon_mdio_read,
};

/**************************************************************************
 *
 * Etherfabric abstraction layer
 *
 **************************************************************************
 */

/**
 * Push RX buffer to RXD ring
 *
 */
static inline void efab_push_rx_buffer ( struct efab_nic *efab,
					 struct efab_rx_buf *rx_buf ) {
	/* Create RX descriptor */
	rx_buf->id = efab->rx_write_ptr;
	efab->op->build_rx_desc ( efab, rx_buf );

	/* Update RX write pointer */
	efab->rx_write_ptr = ( efab->rx_write_ptr + 1 ) % EFAB_RXD_SIZE;
	efab->op->notify_rx_desc ( efab );

	DBG ( "Added RX id %x\n", rx_buf->id );
}

/**
 * Push TX buffer to TXD ring
 *
 */
static inline void efab_push_tx_buffer ( struct efab_nic *efab,
					 struct efab_tx_buf *tx_buf ) {
	/* Create TX descriptor */
	tx_buf->id = efab->tx_write_ptr;
	efab->op->build_tx_desc ( efab, tx_buf );

	/* Update TX write pointer */
	efab->tx_write_ptr = ( efab->tx_write_ptr + 1 ) % EFAB_TXD_SIZE;
	efab->op->notify_tx_desc ( efab );

	DBG ( "Added TX id %x\n", tx_buf->id );
}

/**
 * Initialise MAC and wait for link up
 *
 */
static int efab_init_mac ( struct efab_nic *efab ) {
	int count;

	/* This can take several seconds */
	printf ( "Waiting for link.." );
	count = 0;
	do {
		putchar ( '.' );
		if ( ! efab->op->init_mac ( efab ) ) {
			printf ( "failed\n" );
			return 0;
		}
		if ( efab->link_up ) {
			/* PHY init printed the message for us */
			return 1;
		}
		sleep ( 1 );
	} while ( ++count < 5 );
	printf ( "timed out\n" );

	return 0;
}

/**
 * Initialise NIC
 *
 */
static int efab_init_nic ( struct efab_nic *efab ) {
	int i;

	/* Reset NIC */
	if ( ! efab->op->reset ( efab ) )
		return 0;

	/* Initialise NIC */
	if ( ! efab->op->init_nic ( efab ) )
		return 0;

	/* Push RX descriptors */
	for ( i = 0 ; i < EFAB_RX_BUFS ; i++ ) {
		efab_push_rx_buffer ( efab, &efab->rx_bufs[i] );
	}

	/* Read MAC address from EEPROM */
	if ( ! efab->op->read_eeprom ( efab ) )
		return 0;
	efab->mac_addr[ETH_ALEN-1] += efab->port;

	/* Initialise MAC and wait for link up */
	if ( ! efab_init_mac ( efab ) )
		return 0;

	return 1;
}

/**************************************************************************
 *
 * Etherboot interface
 *
 **************************************************************************
 */

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int etherfabric_poll ( struct nic *nic, int retrieve ) {
	struct efab_nic *efab = nic->priv_data;
	struct efab_event event;
	static struct efab_rx_buf *rx_buf = NULL;
	int i;

	/* Process the event queue until we hit either a packet
	 * received event or an empty event slot.
	 */
	while ( ( rx_buf == NULL ) &&
		efab->op->fetch_event ( efab, &event ) ) {
		if ( event.type == EFAB_EV_TX ) {
			/* TX completed - mark as done */
			DBG ( "TX id %x complete\n",
			      efab->tx_buf.id );
			efab->tx_in_progress = 0;
		} else if ( event.type == EFAB_EV_RX ) {
			/* RX - find corresponding buffer */
			for ( i = 0 ; i < EFAB_RX_BUFS ; i++ ) {
				if ( efab->rx_bufs[i].id == event.rx_id ) {
					rx_buf = &efab->rx_bufs[i];
					rx_buf->len = event.rx_len;
					DBG ( "RX id %x (len %x) received\n",
					      rx_buf->id, rx_buf->len );
					break;
				}
			}
			if ( ! rx_buf ) {
				printf ( "Invalid RX ID %x\n", event.rx_id );
			}
		} else if ( event.type == EFAB_EV_NONE ) {
			DBG ( "Ignorable event\n" );
		} else {
			DBG ( "Unknown event\n" );
		}
	}

	/* If there is no packet, return 0 */
	if ( ! rx_buf )
		return 0;

	/* If we don't want to retrieve it just yet, return 1 */
	if ( ! retrieve )
		return 1;

	/* Copy packet contents */
	nic->packetlen = rx_buf->len;
	memcpy ( nic->packet, rx_buf->addr, nic->packetlen );

	/* Give this buffer back to the NIC */
	efab_push_rx_buffer ( efab, rx_buf );

	/* Prepare to receive next packet */
	rx_buf = NULL;

	return 1;
}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void etherfabric_transmit ( struct nic *nic, const char *dest,
				   unsigned int type, unsigned int size,
				   const char *data ) {
	struct efab_nic *efab = nic->priv_data;
	unsigned int nstype = htons ( type );

	/* We can only transmit one packet at a time; a TX completion
	 * event must be received before we can transmit the next
	 * packet.  Since there is only one static TX buffer, we don't
	 * worry unduly about overflow, but we report it anyway.
	 */
	if ( efab->tx_in_progress ) {
		printf ( "TX overflow!\n" );
	}

	/* Fill TX buffer, pad to ETH_ZLEN */
	memcpy ( efab->tx_buf.addr, dest, ETH_ALEN );
	memcpy ( efab->tx_buf.addr + ETH_ALEN, nic->node_addr, ETH_ALEN );
	memcpy ( efab->tx_buf.addr + 2 * ETH_ALEN, &nstype, 2 );
	memcpy ( efab->tx_buf.addr + ETH_HLEN, data, size );
	size += ETH_HLEN;
        while ( size < ETH_ZLEN ) {
                efab->tx_buf.addr[size++] = '\0';
        }
	efab->tx_buf.len = size;

	/* Push TX descriptor */
	efab_push_tx_buffer ( efab, &efab->tx_buf );

	/* There is no way to wait for TX complete (i.e. TX buffer
	 * available to re-use for the next transmit) without reading
	 * from the event queue.  We therefore simply leave the TX
	 * buffer marked as "in use" until a TX completion event
	 * happens to be picked up by a call to etherfabric_poll().
	 */
	efab->tx_in_progress = 1;

	return;
}

/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void etherfabric_disable ( struct dev *dev ) {
	struct nic *nic = ( struct nic * ) dev;
	struct efab_nic *efab = nic->priv_data;

	efab->op->reset ( efab );
	if ( efab->membase )
		iounmap ( efab->membase );
}

/**************************************************************************
IRQ - handle interrupts
***************************************************************************/
static void etherfabric_irq ( struct nic *nic, irq_action_t action ) {
	struct efab_nic *efab = nic->priv_data;
       
	switch ( action ) {
	case DISABLE :
		efab->op->mask_irq ( efab, 1 );
		break;
	case ENABLE :
		efab->op->mask_irq ( efab, 0 );
		break;
	case FORCE :
		/* Force NIC to generate a receive interrupt */
		efab->op->generate_irq ( efab );
		break;
	}
	
	return;
}

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
***************************************************************************/
static int etherfabric_probe ( struct dev *dev, struct pci_device *pci ) {
	struct nic *nic = ( struct nic * ) dev;
	static struct efab_nic efab;
	static int nic_port = 1;
	struct efab_buffers *buffers;
	int i;

	/* Set up our private data structure */
	nic->priv_data = &efab;
	memset ( &efab, 0, sizeof ( efab ) );
	memset ( &efab_buffers, 0, sizeof ( efab_buffers ) );

	/* Hook in appropriate operations table.  Do this early. */
	if ( pci->dev_id == EF1002_DEVID ) {
		efab.op = &ef1002_operations;
	} else {
		efab.op = &falcon_operations;
	}

	/* Initialise efab data structure */
	efab.pci = pci;
	buffers = ( ( struct efab_buffers * )
		    ( ( ( void * ) &efab_buffers ) +
		      ( - virt_to_bus ( &efab_buffers ) ) % EFAB_BUF_ALIGN ) );
	efab.eventq = buffers->eventq;
	efab.txd = buffers->txd;
	efab.rxd = buffers->rxd;
	efab.tx_buf.addr = buffers->tx_buf;
	for ( i = 0 ; i < EFAB_RX_BUFS ; i++ ) {
		efab.rx_bufs[i].addr = buffers->rx_buf[i];
	}

	/* Enable the PCI device */
	adjust_pci_device ( pci );
	nic->ioaddr = pci->ioaddr & ~3;
	nic->irqno = pci->irq;

	/* Get iobase/membase */
	efab.iobase = nic->ioaddr;
	efab.op->get_membase ( &efab );

	/* Switch NIC ports (i.e. try different ports on each probe) */
	nic_port = 1 - nic_port;
	efab.port = nic_port;

	/* Initialise hardware */
	if ( ! efab_init_nic ( &efab ) )
		return 0;
	memcpy ( nic->node_addr, efab.mac_addr, ETH_ALEN );

	/* hello world */
	printf ( "Found EtherFabric %s NIC %!\n", pci->name, nic->node_addr );

	/* point to NIC specific routines */
	dev->disable  = etherfabric_disable;
	nic->poll     = etherfabric_poll;
	nic->transmit = etherfabric_transmit;
	nic->irq      = etherfabric_irq;

	return 1;
}

static struct pci_id etherfabric_nics[] = {
PCI_ROM(0x1924, 0xC101, "ef1002", "EtherFabric EF1002"),
PCI_ROM(0x1924, 0x0703, "falcon", "EtherFabric Falcon"),
};

static struct pci_driver etherfabric_driver __pci_driver = {
	.type     = NIC_DRIVER,
	.name     = "EFAB",
	.probe    = etherfabric_probe,
	.ids      = etherfabric_nics,
	.id_count = sizeof(etherfabric_nics)/sizeof(etherfabric_nics[0]),
	.class    = 0,
};

/*
 * Local variables:
 *  c-basic-offset: 8
 *  c-indent-level: 8
 *  tab-width: 8
 * End:
 */
