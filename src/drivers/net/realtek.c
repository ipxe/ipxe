/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * (EEPROM code originally implemented for rtl8139.c)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/iobuf.h>
#include <ipxe/malloc.h>
#include <ipxe/pci.h>
#include <ipxe/nvs.h>
#include <ipxe/threewire.h>
#include <ipxe/bitbash.h>
#include <ipxe/mii.h>
#include "realtek.h"

/** @file
 *
 * Realtek 10/100/1000 network card driver
 *
 * Based on the following datasheets:
 *
 *    http://www.datasheetarchive.com/dl/Datasheets-8/DSA-153536.pdf
 *    http://www.datasheetarchive.com/indexdl/Datasheet-028/DSA00494723.pdf
 */

/******************************************************************************
 *
 * EEPROM interface
 *
 ******************************************************************************
 */

/** Pin mapping for SPI bit-bashing interface */
static const uint8_t realtek_eeprom_bits[] = {
	[SPI_BIT_SCLK]	= RTL_9346CR_EESK,
	[SPI_BIT_MOSI]	= RTL_9346CR_EEDI,
	[SPI_BIT_MISO]	= RTL_9346CR_EEDO,
	[SPI_BIT_SS(0)]	= ( RTL_9346CR_EECS | RTL_9346CR_EEM1 ),
};

/**
 * Read input bit
 *
 * @v basher		Bit-bashing interface
 * @v bit_id		Bit number
 * @ret zero		Input is a logic 0
 * @ret non-zero	Input is a logic 1
 */
static int realtek_spi_read_bit ( struct bit_basher *basher,
				  unsigned int bit_id ) {
	struct realtek_nic *rtl = container_of ( basher, struct realtek_nic,
						 spibit.basher );
	uint8_t mask = realtek_eeprom_bits[bit_id];
	uint8_t reg;

	reg = readb ( rtl->regs + RTL_9346CR );
	return ( reg & mask );
}

/**
 * Set/clear output bit
 *
 * @v basher		Bit-bashing interface
 * @v bit_id		Bit number
 * @v data		Value to write
 */
static void realtek_spi_write_bit ( struct bit_basher *basher,
				    unsigned int bit_id, unsigned long data ) {
	struct realtek_nic *rtl = container_of ( basher, struct realtek_nic,
						 spibit.basher );
	uint8_t mask = realtek_eeprom_bits[bit_id];
	uint8_t reg;

	reg = readb ( rtl->regs + RTL_9346CR );
	reg &= ~mask;
	reg |= ( data & mask );
	writeb ( reg, rtl->regs + RTL_9346CR );
}

/** SPI bit-bashing interface */
static struct bit_basher_operations realtek_basher_ops = {
	.read = realtek_spi_read_bit,
	.write = realtek_spi_write_bit,
};

/**
 * Initialise EEPROM
 *
 * @v netdev		Network device
 */
static void realtek_init_eeprom ( struct net_device *netdev ) {
	struct realtek_nic *rtl = netdev->priv;

	/* Initialise SPI bit-bashing interface */
	rtl->spibit.basher.op = &realtek_basher_ops;
	rtl->spibit.bus.mode = SPI_MODE_THREEWIRE;
	init_spi_bit_basher ( &rtl->spibit );

	/* Detect EEPROM type and initialise three-wire device */
	if ( readl ( rtl->regs + RTL_RCR ) & RTL_RCR_9356SEL ) {
		DBGC ( rtl, "REALTEK %p EEPROM is a 93C56\n", rtl );
		init_at93c56 ( &rtl->eeprom, 16 );
	} else {
		DBGC ( rtl, "REALTEK %p EEPROM is a 93C46\n", rtl );
		init_at93c46 ( &rtl->eeprom, 16 );
	}
	rtl->eeprom.bus = &rtl->spibit.bus;

	/* Initialise space for non-volatile options, if available
	 *
	 * We use offset 0x40 (i.e. address 0x20), length 0x40.  This
	 * block is marked as VPD in the Realtek datasheets, so we use
	 * it only if we detect that the card is not supporting VPD.
	 */
	if ( readb ( rtl->regs + RTL_CONFIG1 ) & RTL_CONFIG1_VPD ) {
		DBGC ( rtl, "REALTEK %p EEPROM in use for VPD; cannot use "
		       "for options\n", rtl );
	} else {
		nvo_init ( &rtl->nvo, &rtl->eeprom.nvs, RTL_EEPROM_VPD,
			   RTL_EEPROM_VPD_LEN, NULL, &netdev->refcnt );
	}
}

/******************************************************************************
 *
 * MII interface
 *
 ******************************************************************************
 */

/**
 * Read from MII register
 *
 * @v mii		MII interface
 * @v reg		Register address
 * @ret value		Data read, or negative error
 */
static int realtek_mii_read ( struct mii_interface *mii, unsigned int reg ) {
	struct realtek_nic *rtl = container_of ( mii, struct realtek_nic, mii );
	unsigned int i;
	uint32_t value;

	/* Initiate read */
	writel ( RTL_PHYAR_VALUE ( 0, reg, 0 ), rtl->regs + RTL_PHYAR );

	/* Wait for read to complete */
	for ( i = 0 ; i < RTL_MII_MAX_WAIT_US ; i++ ) {

		/* If read is not complete, delay 1us and retry */
		value = readl ( rtl->regs + RTL_PHYAR );
		if ( ! ( value & RTL_PHYAR_FLAG ) ) {
			udelay ( 1 );
			continue;
		}

		/* Return register value */
		return ( RTL_PHYAR_DATA ( value ) );
	}

	DBGC ( rtl, "REALTEK %p timed out waiting for MII read\n", rtl );
	return -ETIMEDOUT;
}

/**
 * Write to MII register
 *
 * @v mii		MII interface
 * @v reg		Register address
 * @v data		Data to write
 * @ret rc		Return status code
 */
static int realtek_mii_write ( struct mii_interface *mii, unsigned int reg,
			       unsigned int data) {
	struct realtek_nic *rtl = container_of ( mii, struct realtek_nic, mii );
	unsigned int i;

	/* Initiate write */
	writel ( RTL_PHYAR_VALUE ( RTL_PHYAR_FLAG, reg, data ),
		 rtl->regs + RTL_PHYAR );

	/* Wait for write to complete */
	for ( i = 0 ; i < RTL_MII_MAX_WAIT_US ; i++ ) {

		/* If write is not complete, delay 1us and retry */
		if ( readl ( rtl->regs + RTL_PHYAR ) & RTL_PHYAR_FLAG ) {
			udelay ( 1 );
			continue;
		}

		return 0;
	}

	DBGC ( rtl, "REALTEK %p timed out waiting for MII write\n", rtl );
	return -ETIMEDOUT;
}

/** Realtek MII operations */
static struct mii_operations realtek_mii_operations = {
	.read = realtek_mii_read,
	.write = realtek_mii_write,
};

/******************************************************************************
 *
 * Device reset
 *
 ******************************************************************************
 */

/**
 * Reset hardware
 *
 * @v rtl		Realtek device
 * @ret rc		Return status code
 */
static int realtek_reset ( struct realtek_nic *rtl ) {
	unsigned int i;

	/* Issue reset */
	writeb ( RTL_CR_RST, rtl->regs + RTL_CR );

	/* Wait for reset to complete */
	for ( i = 0 ; i < RTL_RESET_MAX_WAIT_MS ; i++ ) {

		/* If reset is not complete, delay 1ms and retry */
		if ( readb ( rtl->regs + RTL_CR ) & RTL_CR_RST ) {
			mdelay ( 1 );
			continue;
		}

		/* Enable PCI Dual Address Cycle (for 64-bit systems) */
		writew ( ( RTL_CPCR_DAC | RTL_CPCR_MULRW ),
			 rtl->regs + RTL_CPCR );

		return 0;
	}

	DBGC ( rtl, "REALTEK %p timed out waiting for reset\n", rtl );
	return -ETIMEDOUT;
}

/******************************************************************************
 *
 * Link state
 *
 ******************************************************************************
 */

/**
 * Check link state
 *
 * @v netdev		Network device
 */
static void realtek_check_link ( struct net_device *netdev ) {
	struct realtek_nic *rtl = netdev->priv;

	if ( readb ( rtl->regs + RTL_PHYSTATUS ) & RTL_PHYSTATUS_LINKSTS ) {
		netdev_link_up ( netdev );
	} else {
		netdev_link_down ( netdev );
	}
}

/******************************************************************************
 *
 * Network device interface
 *
 ******************************************************************************
 */

/**
 * Create descriptor ring
 *
 * @v rtl		Realtek device
 * @v ring		Descriptor ring
 * @ret rc		Return status code
 */
static int realtek_create_ring ( struct realtek_nic *rtl,
				 struct realtek_ring *ring ) {
	physaddr_t address;

	/* Allocate descriptor ring */
	ring->desc = malloc_dma ( ring->len, RTL_RING_ALIGN );
	if ( ! ring->desc )
		return -ENOMEM;

	/* Initialise descriptor ring */
	memset ( ring->desc, 0, ring->len );

	/* Program ring address */
	address = virt_to_bus ( ring->desc );
	writel ( ( address & 0xffffffffUL ), rtl->regs + ring->reg );
	if ( sizeof ( physaddr_t ) > sizeof ( uint32_t ) ) {
		writel ( ( ( ( uint64_t ) address ) >> 32 ),
			 rtl->regs + ring->reg + 4 );
	}
	DBGC ( rtl, "REALTEK %p ring %02x is at [%08llx,%08llx)\n",
	       rtl, ring->reg, ( ( unsigned long long ) address ),
	       ( ( unsigned long long ) address + ring->len ) );

	return 0;
}

/**
 * Destroy descriptor ring
 *
 * @v rtl		Realtek device
 * @v ring		Descriptor ring
 */
static void realtek_destroy_ring ( struct realtek_nic *rtl,
				   struct realtek_ring *ring ) {

	/* Clear ring address */
	writel ( 0, rtl->regs + ring->reg );
	writel ( 0, rtl->regs + ring->reg + 4 );

	/* Free descriptor ring */
	free_dma ( ring->desc, ring->len );
	ring->desc = NULL;
	ring->prod = 0;
	ring->cons = 0;
}

/**
 * Refill receive descriptor ring
 *
 * @v rtl		Realtek device
 */
static void realtek_refill_rx ( struct realtek_nic *rtl ) {
	struct realtek_descriptor *rx;
	struct io_buffer *iobuf;
	unsigned int rx_idx;
	physaddr_t address;
	int is_last;

	while ( ( rtl->rx.prod - rtl->rx.cons ) < RTL_NUM_RX_DESC ) {

		/* Allocate I/O buffer */
		iobuf = alloc_iob ( RTL_RX_MAX_LEN );
		if ( ! iobuf ) {
			/* Wait for next refill */
			return;
		}

		/* Get next receive descriptor */
		rx_idx = ( rtl->rx.prod++ % RTL_NUM_RX_DESC );
		is_last = ( rx_idx == ( RTL_NUM_RX_DESC - 1 ) );
		rx = &rtl->rx.desc[rx_idx];

		/* Populate receive descriptor */
		address = virt_to_bus ( iobuf->data );
		rx->address = cpu_to_le64 ( address );
		rx->length = RTL_RX_MAX_LEN;
		wmb();
		rx->flags = ( cpu_to_le16 ( RTL_DESC_OWN ) |
			      ( is_last ? cpu_to_le16 ( RTL_DESC_EOR ) : 0 ) );
		wmb();

		/* Record I/O buffer */
		assert ( rtl->rx_iobuf[rx_idx] == NULL );
		rtl->rx_iobuf[rx_idx] = iobuf;

		DBGC2 ( rtl, "REALTEK %p RX %d is [%llx,%llx)\n", rtl, rx_idx,
			( ( unsigned long long ) address ),
			( ( unsigned long long ) address + RTL_RX_MAX_LEN ) );
	}
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int realtek_open ( struct net_device *netdev ) {
	struct realtek_nic *rtl = netdev->priv;
	uint32_t rcr;
	int rc;

	/* Create transmit descriptor ring */
	if ( ( rc = realtek_create_ring ( rtl, &rtl->tx ) ) != 0 )
		goto err_create_tx;

	/* Create receive descriptor ring */
	if ( ( rc = realtek_create_ring ( rtl, &rtl->rx ) ) != 0 )
		goto err_create_rx;

	/* Configure MTU */
	writew ( RTL_RX_MAX_LEN, rtl->regs + RTL_RMS );

	/* Accept all packets */
	writel ( 0xffffffffUL, rtl->regs + RTL_MAR0 );
	writel ( 0xffffffffUL, rtl->regs + RTL_MAR4 );
	rcr = readl ( rtl->regs + RTL_RCR );
	writel ( ( rcr | RTL_RCR_AB | RTL_RCR_AM | RTL_RCR_APM | RTL_RCR_AAP ),
		 rtl->regs + RTL_RCR );

	/* Fill receive ring */
	realtek_refill_rx ( rtl );

	/* Enable transmitter and receiver */
	writeb ( ( RTL_CR_TE | RTL_CR_RE ), rtl->regs + RTL_CR );

	/* Update link state */
	realtek_check_link ( netdev );

	return 0;

	realtek_destroy_ring ( rtl, &rtl->rx );
 err_create_rx:
	realtek_destroy_ring ( rtl, &rtl->tx );
 err_create_tx:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void realtek_close ( struct net_device *netdev ) {
	struct realtek_nic *rtl = netdev->priv;
	unsigned int i;

	/* Disable receiver and transmitter */
	writeb ( 0, rtl->regs + RTL_CR );

	/* Destroy receive descriptor ring */
	realtek_destroy_ring ( rtl, &rtl->rx );

	/* Discard any unused receive buffers */
	for ( i = 0 ; i < RTL_NUM_RX_DESC ; i++ ) {
		if ( rtl->rx_iobuf[i] )
			free_iob ( rtl->rx_iobuf[i] );
		rtl->rx_iobuf[i] = NULL;
	}

	/* Destroy transmit descriptor ring */
	realtek_destroy_ring ( rtl, &rtl->tx );
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int realtek_transmit ( struct net_device *netdev,
			      struct io_buffer *iobuf ) {
	struct realtek_nic *rtl = netdev->priv;
	struct realtek_descriptor *tx;
	unsigned int tx_idx;
	physaddr_t address;
	int is_last;

	/* Get next transmit descriptor */
	if ( ( rtl->tx.prod - rtl->tx.cons ) >= RTL_NUM_TX_DESC ) {
		DBGC ( rtl, "REALTEK %p out of transmit descriptors\n", rtl );
		return -ENOBUFS;
	}
	tx_idx = ( rtl->tx.prod++ % RTL_NUM_TX_DESC );
	is_last = ( tx_idx == ( RTL_NUM_TX_DESC - 1 ) );
	tx = &rtl->tx.desc[tx_idx];

	/* Populate transmit descriptor */
	address = virt_to_bus ( iobuf->data );
	tx->address = cpu_to_le64 ( address );
	tx->length = cpu_to_le16 ( iob_len ( iobuf ) );
	wmb();
	tx->flags = ( cpu_to_le16 ( RTL_DESC_OWN | RTL_DESC_FS | RTL_DESC_LS ) |
		      ( is_last ? cpu_to_le16 ( RTL_DESC_EOR ) : 0 ) );
	wmb();

	/* Notify card that there are packets ready to transmit */
	writeb ( RTL_TPPOLL_NPQ, rtl->regs + RTL_TPPOLL );

	DBGC2 ( rtl, "REALTEK %p TX %d is [%llx,%llx)\n", rtl, tx_idx,
		( ( unsigned long long ) address ),
		( ( unsigned long long ) address + iob_len ( iobuf ) ) );

	return 0;
}

/**
 * Poll for completed packets
 *
 * @v netdev		Network device
 */
static void realtek_poll_tx ( struct net_device *netdev ) {
	struct realtek_nic *rtl = netdev->priv;
	struct realtek_descriptor *tx;
	unsigned int tx_idx;

	/* Check for completed packets */
	while ( rtl->tx.cons != rtl->tx.prod ) {

		/* Get next transmit descriptor */
		tx_idx = ( rtl->tx.cons % RTL_NUM_TX_DESC );
		tx = &rtl->tx.desc[tx_idx];

		/* Stop if descriptor is still in use */
		if ( tx->flags & cpu_to_le16 ( RTL_DESC_OWN ) )
			return;

		DBGC2 ( rtl, "REALTEK %p TX %d complete\n", rtl, tx_idx );

		/* Complete TX descriptor */
		netdev_tx_complete_next ( netdev );
		rtl->tx.cons++;
	}
}

/**
 * Poll for received packets
 *
 * @v netdev		Network device
 */
static void realtek_poll_rx ( struct net_device *netdev ) {
	struct realtek_nic *rtl = netdev->priv;
	struct realtek_descriptor *rx;
	struct io_buffer *iobuf;
	unsigned int rx_idx;
	size_t len;

	/* Check for received packets */
	while ( rtl->rx.cons != rtl->rx.prod ) {

		/* Get next receive descriptor */
		rx_idx = ( rtl->rx.cons % RTL_NUM_RX_DESC );
		rx = &rtl->rx.desc[rx_idx];

		/* Stop if descriptor is still in use */
		if ( rx->flags & cpu_to_le16 ( RTL_DESC_OWN ) )
			return;

		/* Populate I/O buffer */
		iobuf = rtl->rx_iobuf[rx_idx];
		rtl->rx_iobuf[rx_idx] = NULL;
		len = ( le16_to_cpu ( rx->length ) & RTL_DESC_SIZE_MASK );
		iob_put ( iobuf, ( len - 4 /* strip CRC */ ) );

		DBGC2 ( rtl, "REALTEK %p RX %d complete (length %zd)\n",
			rtl, rx_idx, len );

		/* Hand off to network stack */
		if ( rx->flags & cpu_to_le16 ( RTL_DESC_RES ) ) {
			netdev_rx_err ( netdev, iobuf, -EIO );
		} else {
			netdev_rx ( netdev, iobuf );
		}
		rtl->rx.cons++;
	}
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void realtek_poll ( struct net_device *netdev ) {
	struct realtek_nic *rtl = netdev->priv;
	uint16_t isr;

	/* Check for and acknowledge interrupts */
	isr = readw ( rtl->regs + RTL_ISR );
	if ( ! isr )
		return;
	writew ( isr, rtl->regs + RTL_ISR );

	/* Poll for TX completions, if applicable */
	if ( isr & ( RTL_IRQ_TER | RTL_IRQ_TOK ) )
		realtek_poll_tx ( netdev );

	/* Poll for RX completionsm, if applicable */
	if ( isr & ( RTL_IRQ_RER | RTL_IRQ_ROK ) )
		realtek_poll_rx ( netdev );

	/* Check link state, if applicable */
	if ( isr & RTL_IRQ_PUN_LINKCHG )
		realtek_check_link ( netdev );

	/* Refill RX ring */
	realtek_refill_rx ( rtl );
}

/**
 * Enable or disable interrupts
 *
 * @v netdev		Network device
 * @v enable		Interrupts should be enabled
 */
static void realtek_irq ( struct net_device *netdev, int enable ) {
	struct realtek_nic *rtl = netdev->priv;
	uint16_t imr;

	/* Set interrupt mask */
	imr = ( enable ? ( RTL_IRQ_PUN_LINKCHG | RTL_IRQ_TER | RTL_IRQ_TOK |
			   RTL_IRQ_RER | RTL_IRQ_ROK ) : 0 );
	writew ( imr, rtl->regs + RTL_IMR );
}

/** Realtek network device operations */
static struct net_device_operations realtek_operations = {
	.open		= realtek_open,
	.close		= realtek_close,
	.transmit	= realtek_transmit,
	.poll		= realtek_poll,
	.irq		= realtek_irq,
};

/******************************************************************************
 *
 * PCI interface
 *
 ******************************************************************************
 */

/**
 * Probe PCI device
 *
 * @v pci		PCI device
 * @ret rc		Return status code
 */
static int realtek_probe ( struct pci_device *pci ) {
	struct net_device *netdev;
	struct realtek_nic *rtl;
	unsigned int i;
	int rc;

	/* Allocate and initialise net device */
	netdev = alloc_etherdev ( sizeof ( *rtl ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &realtek_operations );
	rtl = netdev->priv;
	pci_set_drvdata ( pci, netdev );
	netdev->dev = &pci->dev;
	memset ( rtl, 0, sizeof ( *rtl ) );
	realtek_init_ring ( &rtl->tx, RTL_NUM_TX_DESC, RTL_TNPDS );
	realtek_init_ring ( &rtl->rx, RTL_NUM_RX_DESC, RTL_RDSAR );

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Map registers */
	rtl->regs = ioremap ( pci->membase, RTL_BAR_SIZE );

	/* Reset the NIC */
	if ( ( rc = realtek_reset ( rtl ) ) != 0 )
		goto err_reset;

	/* Initialise EEPROM */
	realtek_init_eeprom ( netdev );

	/* Read MAC address from EEPROM */
	if ( ( rc = nvs_read ( &rtl->eeprom.nvs, RTL_EEPROM_MAC,
			       netdev->hw_addr, ETH_ALEN ) ) != 0 ) {
		DBGC ( rtl, "REALTEK %p could not read MAC address: %s\n",
		       rtl, strerror ( rc ) );
		goto err_nvs_read;
	}

	/* The EEPROM may not be present for onboard NICs.  Fall back
	 * to reading the current ID register value, which will
	 * hopefully have been programmed by the platform firmware.
	 */
	if ( ! is_valid_ether_addr ( netdev->hw_addr ) ) {
		DBGC ( rtl, "REALTEK %p seems to have no EEPROM\n", rtl );
		for ( i = 0 ; i < ETH_ALEN ; i++ )
			netdev->hw_addr[i] = readb ( rtl->regs + RTL_IDR0 + i );
	}

	/* Initialise and reset MII interface */
	mii_init ( &rtl->mii, &realtek_mii_operations );
	if ( ( rc = mii_reset ( &rtl->mii ) ) != 0 ) {
		DBGC ( rtl, "REALTEK %p could not reset MII: %s\n",
		       rtl, strerror ( rc ) );
		goto err_mii_reset;
	}

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	/* Set initial link state */
	realtek_check_link ( netdev );

	/* Register non-volatile options, if applicable */
	if ( rtl->nvo.nvs ) {
		if ( ( rc = register_nvo ( &rtl->nvo,
					   netdev_settings ( netdev ) ) ) != 0)
			goto err_register_nvo;
	}

	return 0;

 err_register_nvo:
	unregister_netdev ( netdev );
 err_register_netdev:
 err_mii_reset:
 err_nvs_read:
	realtek_reset ( rtl );
 err_reset:
	netdev_nullify ( netdev );
	netdev_put ( netdev );
 err_alloc:
	return rc;
}

/**
 * Remove PCI device
 *
 * @v pci		PCI device
 */
static void realtek_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );
	struct realtek_nic *rtl = netdev->priv;

	/* Unregister non-volatile options, if applicable */
	if ( rtl->nvo.nvs )
		unregister_nvo ( &rtl->nvo );

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Reset card */
	realtek_reset ( rtl );

	/* Free network device */
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** Realtek PCI device IDs */
static struct pci_device_id realtek_nics[] = {
	PCI_ROM ( 0x10ec, 0x8129, "r8129",	"RTL-8129", 0 ),
	PCI_ROM ( 0x10ec, 0x8136, "r8136",	"RTL8101E/RTL8102E", 0 ),
	PCI_ROM ( 0x10ec, 0x8167, "r8167",	"RTL-8110SC/8169SC", 0 ),
	PCI_ROM ( 0x10ec, 0x8168, "r8168",	"RTL8111/8168B", 0 ),
	PCI_ROM ( 0x10ec, 0x8169, "r8169",	"RTL-8169", 0 ),
	PCI_ROM ( 0x1186, 0x4300, "dge528t",	"DGE-528T", 0 ),
	PCI_ROM ( 0x1259, 0xc107, "allied8169",	"Allied Telesyn 8169", 0 ),
	PCI_ROM ( 0x16ec, 0x0116, "usr997902",	"USR997902", 0 ),
	PCI_ROM ( 0x1737, 0x1032, "linksys8169","Linksys 8169", 0 ),
	PCI_ROM ( 0x0001, 0x8168, "clone8169",	"Cloned 8169", 0 ),
};

/** Realtek PCI driver */
struct pci_driver realtek_driver __pci_driver = {
	.ids = realtek_nics,
	.id_count = ( sizeof ( realtek_nics ) / sizeof ( realtek_nics[0] ) ),
	.probe = realtek_probe,
	.remove = realtek_remove,
};
