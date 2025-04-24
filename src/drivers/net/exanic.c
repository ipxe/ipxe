/*
 * Copyright (C) 2017 Michael Brown <mbrown@fensystems.co.uk>.
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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/iobuf.h>
#include <ipxe/malloc.h>
#include <ipxe/umalloc.h>
#include <ipxe/pci.h>
#include "exanic.h"

/** @file
 *
 * Exablaze ExaNIC driver
 *
 */

/* Disambiguate the various error causes */
#define EIO_ABORTED __einfo_error ( EINFO_EIO_ABORTED )
#define EINFO_EIO_ABORTED \
	__einfo_uniqify ( EINFO_EIO, 0x01, "Frame aborted" )
#define EIO_CORRUPT __einfo_error ( EINFO_EIO_CORRUPT )
#define EINFO_EIO_CORRUPT \
	__einfo_uniqify ( EINFO_EIO, 0x02, "CRC incorrect" )
#define EIO_HWOVFL __einfo_error ( EINFO_EIO_HWOVFL )
#define EINFO_EIO_HWOVFL \
	__einfo_uniqify ( EINFO_EIO, 0x03, "Hardware overflow" )
#define EIO_STATUS( status ) \
	EUNIQ ( EINFO_EIO, ( (status) & EXANIC_STATUS_ERROR_MASK ), \
		EIO_ABORTED, EIO_CORRUPT, EIO_HWOVFL )

/**
 * Write DMA base address register
 *
 * @v addr		DMA base address
 * @v reg		Register
 */
static void exanic_write_base ( physaddr_t addr, void *reg ) {
	uint32_t lo;
	uint32_t hi;

	/* Write high and low registers, setting flags as appropriate */
	lo = addr;
	if ( sizeof ( physaddr_t ) > sizeof ( uint32_t ) ) {
		/* 64-bit build; may be a 32-bit or 64-bit address */
		hi = ( ( ( uint64_t ) addr ) >> 32 );
		if ( ! hi )
			lo |= EXANIC_DMA_32_BIT;
	} else {
		/* 32-bit build; always a 32-bit address */
		hi = 0;
		lo |= EXANIC_DMA_32_BIT;
	}
	writel ( hi, ( reg + 0 ) );
	writel ( lo, ( reg + 4 ) );
}

/**
 * Clear DMA base address register
 *
 * @v reg		Register
 */
static inline void exanic_clear_base ( void *reg ) {

	/* Clear both high and low registers */
	writel ( 0, ( reg + 0 ) );
	writel ( 0, ( reg + 4 ) );
}

/******************************************************************************
 *
 * Device reset
 *
 ******************************************************************************
 */

/**
 * Reset hardware
 *
 * @v exanic		ExaNIC device
 */
static void exanic_reset ( struct exanic *exanic ) {
	void *port_regs;
	unsigned int i;

	/* Disable all possible ports */
	for ( i = 0 ; i < EXANIC_MAX_PORTS ; i++ ) {
		port_regs = ( exanic->regs + EXANIC_PORT_REGS ( i ) );
		writel ( 0, ( port_regs + EXANIC_PORT_ENABLE ) );
		writel ( 0, ( port_regs + EXANIC_PORT_IRQ ) );
		exanic_clear_base ( port_regs + EXANIC_PORT_RX_BASE );
	}

	/* Disable transmit feedback */
	exanic_clear_base ( exanic->regs + EXANIC_TXF_BASE );
}

/******************************************************************************
 *
 * MAC address
 *
 ******************************************************************************
 */

/**
 * Read I2C line status
 *
 * @v basher		Bit-bashing interface
 * @v bit_id		Bit number
 * @ret zero		Input is a logic 0
 * @ret non-zero	Input is a logic 1
 */
static int exanic_i2c_read_bit ( struct bit_basher *basher,
				 unsigned int bit_id ) {
	struct exanic *exanic =
		container_of ( basher, struct exanic, basher.basher );
	unsigned int shift;
	uint32_t i2c;

	/* Identify bit */
	assert ( bit_id == I2C_BIT_SDA );
	shift = exanic->i2cfg.getsda;

	/* Read I2C register */
	DBG_DISABLE ( DBGLVL_IO );
	i2c = readl ( exanic->regs + EXANIC_I2C );
	DBG_ENABLE ( DBGLVL_IO );
	return ( ( i2c >> shift ) & 1 );
}

/**
 * Write I2C line status
 *
 * @v basher		Bit-bashing interface
 * @v bit_id		Bit number
 * @v data		Value to write
 */
static void exanic_i2c_write_bit ( struct bit_basher *basher,
				   unsigned int bit_id, unsigned long data ) {
	struct exanic *exanic =
		container_of ( basher, struct exanic, basher.basher );
	unsigned int shift;
	uint32_t mask;
	uint32_t i2c;

	/* Identify shift */
	assert ( ( bit_id == I2C_BIT_SCL ) || ( bit_id == I2C_BIT_SDA ) );
	shift = ( ( bit_id == I2C_BIT_SCL ) ?
		  exanic->i2cfg.setscl : exanic->i2cfg.setsda );
	mask = ( 1UL << shift );

	/* Modify I2C register */
	DBG_DISABLE ( DBGLVL_IO );
	i2c = readl ( exanic->regs + EXANIC_I2C );
	i2c &= ~mask;
	if ( ! data )
		i2c |= mask;
	writel ( i2c, ( exanic->regs + EXANIC_I2C ) );
	DBG_ENABLE ( DBGLVL_IO );
}

/** I2C bit-bashing interface operations */
static struct bit_basher_operations exanic_i2c_basher_ops = {
	.read = exanic_i2c_read_bit,
	.write = exanic_i2c_write_bit,
};

/** Possible I2C bus configurations */
static struct exanic_i2c_config exanic_i2cfgs[] = {
	/* X2/X10 */
	{ .setscl = 7, .setsda = 4, .getsda = 12 },
	/* X4 */
	{ .setscl = 7, .setsda = 5, .getsda = 13 },
};

/**
 * Initialise EEPROM
 *
 * @v exanic		ExaNIC device
 * @v i2cfg		I2C bus configuration
 * @ret rc		Return status code
 */
static int exanic_try_init_eeprom ( struct exanic *exanic,
				    struct exanic_i2c_config *i2cfg ) {
	int rc;

	/* Configure I2C bus */
	memcpy ( &exanic->i2cfg, i2cfg, sizeof ( exanic->i2cfg ) );

	/* Initialise I2C bus */
	if ( ( rc = init_i2c_bit_basher ( &exanic->basher,
					  &exanic_i2c_basher_ops ) ) != 0 ) {
		DBGC2 ( exanic, "EXANIC %p found no I2C bus via %d/%d/%d\n",
			exanic, exanic->i2cfg.setscl,
			exanic->i2cfg.setsda, exanic->i2cfg.getsda );
		return rc;
	}

	/* Check for EEPROM presence */
	init_i2c_eeprom ( &exanic->eeprom, EXANIC_EEPROM_ADDRESS );
	if ( ( rc = i2c_check_presence ( &exanic->basher.i2c,
					 &exanic->eeprom ) ) != 0 ) {
		DBGC2 ( exanic, "EXANIC %p found no EEPROM via %d/%d/%d\n",
			exanic, exanic->i2cfg.setscl,
			exanic->i2cfg.setsda, exanic->i2cfg.getsda );
		return rc;
	}

	DBGC ( exanic, "EXANIC %p found EEPROM via %d/%d/%d\n",
	       exanic, exanic->i2cfg.setscl,
	       exanic->i2cfg.setsda, exanic->i2cfg.getsda );
	return 0;
}

/**
 * Initialise EEPROM
 *
 * @v exanic		ExaNIC device
 * @ret rc		Return status code
 */
static int exanic_init_eeprom ( struct exanic *exanic ) {
	struct exanic_i2c_config *i2cfg;
	unsigned int i;
	int rc;

	/* Try all possible bus configurations */
	for ( i = 0 ; i < ( sizeof ( exanic_i2cfgs ) /
			    sizeof ( exanic_i2cfgs[0] ) ) ; i++ ) {
		i2cfg = &exanic_i2cfgs[i];
		if ( ( rc = exanic_try_init_eeprom ( exanic, i2cfg ) ) == 0 )
			return 0;
	}

	DBGC ( exanic, "EXANIC %p found no EEPROM\n", exanic );
	return -ENODEV;
}

/**
 * Fetch base MAC address
 *
 * @v exanic		ExaNIC device
 * @ret rc		Return status code
 */
static int exanic_fetch_mac ( struct exanic *exanic ) {
	struct i2c_interface *i2c = &exanic->basher.i2c;
	int rc;

	/* Initialise EEPROM */
	if ( ( rc = exanic_init_eeprom ( exanic ) ) != 0 )
		return rc;

	/* Fetch base MAC address */
	if ( ( rc = i2c->read ( i2c, &exanic->eeprom, 0, exanic->mac,
				sizeof ( exanic->mac ) ) ) != 0 ) {
		DBGC ( exanic, "EXANIC %p could not read MAC address: %s\n",
		       exanic, strerror ( rc ) );
		return rc;
	}

	return 0;
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
static void exanic_check_link ( struct net_device *netdev ) {
	struct exanic_port *port = netdev->priv;
	uint32_t status;
	uint32_t speed;

	/* Report port status changes */
	status = readl ( port->regs + EXANIC_PORT_STATUS );
	speed = readl ( port->regs + EXANIC_PORT_SPEED );
	if ( status != port->status ) {
		DBGC ( port, "EXANIC %s port status %#08x speed %dMbps\n",
		       netdev->name, status, speed );
		if ( status & EXANIC_PORT_STATUS_LINK ) {
			netdev_link_up ( netdev );
		} else {
			netdev_link_down ( netdev );
		}
		port->status = status;
	}
}

/**
 * Check link state periodically
 *
 * @v retry		Link state check timer
 * @v over		Failure indicator
 */
static void exanic_expired ( struct retry_timer *timer, int over __unused ) {
	struct exanic_port *port =
		container_of ( timer, struct exanic_port, timer );
	struct net_device *netdev = port->netdev;
	static const uint32_t speeds[] = {
		100, 1000, 10000, 40000, 100000,
	};
	unsigned int index;

	/* Restart timer */
	start_timer_fixed ( timer, EXANIC_LINK_INTERVAL );

	/* Check link state */
	exanic_check_link ( netdev );

	/* Do nothing further if link is already up */
	if ( netdev_link_ok ( netdev ) )
		return;

	/* Do nothing further unless we have a valid list of supported speeds */
	if ( ! port->speeds )
		return;

	/* Autonegotiation is not supported; try manually selecting
	 * the next supported link speed.
	 */
	do {
		if ( ! port->speed )
			port->speed = ( 8 * sizeof ( port->speeds ) );
		port->speed--;
	} while ( ! ( ( 1UL << port->speed ) & port->speeds ) );
	index = ( port->speed - ( ffs ( EXANIC_CAPS_SPEED_MASK ) - 1 ) );
	assert ( index < ( sizeof ( speeds ) / sizeof ( speeds[0] ) ) );

	/* Attempt the selected speed */
	DBGC ( netdev, "EXANIC %s attempting %dMbps\n",
	       netdev->name, speeds[index] );
	writel ( speeds[index], ( port->regs + EXANIC_PORT_SPEED ) );
}

/******************************************************************************
 *
 * Network device interface
 *
 ******************************************************************************
 */

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int exanic_open ( struct net_device *netdev ) {
	struct exanic_port *port = netdev->priv;
	struct exanic_tx_chunk *tx;
	unsigned int i;

	/* Reset transmit region contents */
	for ( i = 0 ; i < port->tx_count ; i++ ) {
		tx = ( port->tx + ( i * sizeof ( *tx ) ) );
		writew ( port->txf_slot, &tx->desc.txf_slot );
		writeb ( EXANIC_TYPE_RAW, &tx->desc.type );
		writeb ( 0, &tx->desc.flags );
		writew ( 0, &tx->pad );
	}

	/* Reset receive region contents */
	memset ( port->rx, 0xff, EXANIC_RX_LEN );

	/* Reset transmit feedback region */
	*(port->txf) = 0;

	/* Reset counters */
	port->tx_prod = 0;
	port->tx_cons = 0;
	port->rx_cons = 0;

	/* Map receive region */
	exanic_write_base ( phys_to_bus ( virt_to_phys ( port->rx ) ),
			    ( port->regs + EXANIC_PORT_RX_BASE ) );

	/* Enable promiscuous mode */
	writel ( EXANIC_PORT_FLAGS_PROMISC,
		 ( port->regs + EXANIC_PORT_FLAGS ) );

	/* Reset to default speed and clear cached status */
	writel ( port->default_speed, ( port->regs + EXANIC_PORT_SPEED ) );
	port->speed = 0;
	port->status = 0;

	/* Enable port */
	wmb();
	writel ( EXANIC_PORT_ENABLE_ENABLED,
		 ( port->regs + EXANIC_PORT_ENABLE ) );

	/* Start link state timer */
	start_timer_fixed ( &port->timer, EXANIC_LINK_INTERVAL );

	return 0;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void exanic_close ( struct net_device *netdev ) {
	struct exanic_port *port = netdev->priv;

	/* Stop link state timer */
	stop_timer ( &port->timer );

	/* Disable port */
	writel ( 0, ( port->regs + EXANIC_PORT_ENABLE ) );
	wmb();

	/* Clear receive region */
	exanic_clear_base ( port->regs + EXANIC_PORT_RX_BASE );

	/* Discard any in-progress receive */
	if ( port->rx_iobuf ) {
		netdev_rx_err ( netdev, port->rx_iobuf, -ECANCELED );
		port->rx_iobuf = NULL;
	}
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int exanic_transmit ( struct net_device *netdev,
			     struct io_buffer *iobuf ) {
	struct exanic_port *port = netdev->priv;
	struct exanic_tx_chunk *tx;
	unsigned int tx_fill;
	unsigned int tx_index;
	size_t offset;
	size_t len;
	uint8_t *src;
	uint8_t *dst;

	/* Sanity check */
	len = iob_len ( iobuf );
	if ( len > sizeof ( tx->data ) ) {
		DBGC ( port, "EXANIC %s transmit too large\n", netdev->name );
		return -ENOTSUP;
	}

	/* Get next transmit descriptor */
	tx_fill = ( port->tx_prod - port->tx_cons );
	if ( tx_fill >= port->tx_count ) {
		DBGC ( port, "EXANIC %s out of transmit descriptors\n",
		       netdev->name );
		return -ENOBUFS;
	}
	tx_index = ( port->tx_prod & ( port->tx_count - 1 ) );
	offset = ( tx_index * sizeof ( *tx ) );
	tx = ( port->tx + offset );
	DBGC2 ( port, "EXANIC %s TX %04x at [%05zx,%05zx)\n",
		netdev->name, port->tx_prod, ( port->tx_offset + offset ),
		( port->tx_offset + offset +
		  offsetof ( typeof ( *tx ), data ) + len ) );
	port->tx_prod++;

	/* Populate transmit descriptor */
	writew ( port->tx_prod, &tx->desc.txf_id );
	writew ( ( sizeof ( tx->pad ) + len ), &tx->desc.len );

	/* Copy data to transmit region.  There is no DMA on the
	 * transmit data path.
	 */
	src = iobuf->data;
	dst = tx->data;
	while ( len-- )
		writeb ( *(src++), dst++ );

	/* Send transmit command */
	wmb();
	writel ( ( port->tx_offset + offset ),
		 ( port->regs + EXANIC_PORT_TX_COMMAND ) );

	return 0;
}

/**
 * Poll for completed packets
 *
 * @v netdev		Network device
 */
static void exanic_poll_tx ( struct net_device *netdev ) {
	struct exanic_port *port = netdev->priv;

	/* Report any completed packets */
	while ( port->tx_cons != *(port->txf) ) {
		DBGC2 ( port, "EXANIC %s TX %04x complete\n",
			netdev->name, port->tx_cons );
		netdev_tx_complete_next ( netdev );
		port->tx_cons++;
	}
}

/**
 * Poll for received packets
 *
 * @v netdev		Network device
 */
static void exanic_poll_rx ( struct net_device *netdev ) {
	struct exanic_port *port = netdev->priv;
	struct exanic_rx_chunk *rx;
	unsigned int index;
	uint8_t current;
	uint8_t previous;
	size_t len;

	for ( ; ; port->rx_cons++ ) {

		/* Fetch descriptor */
		index = ( port->rx_cons % EXANIC_RX_COUNT );
		rx = &port->rx[index];

		/* Calculate generation */
		current = ( port->rx_cons / EXANIC_RX_COUNT );
		previous = ( current - 1 );

		/* Do nothing if no chunk is ready */
		if ( rx->desc.generation == previous )
			break;

		/* Allocate I/O buffer if needed */
		if ( ! port->rx_iobuf ) {
			port->rx_iobuf = alloc_iob ( EXANIC_MAX_RX_LEN );
			if ( ! port->rx_iobuf ) {
				/* Wait for next poll */
				break;
			}
			port->rx_rc = 0;
		}

		/* Calculate chunk length */
		len = ( rx->desc.len ? rx->desc.len : sizeof ( rx->data ) );

		/* Append data to I/O buffer */
		if ( len <= iob_tailroom ( port->rx_iobuf ) ) {
			memcpy ( iob_put ( port->rx_iobuf, len ),
				 rx->data, len );
		} else {
			DBGC ( port, "EXANIC %s RX too large\n",
			       netdev->name );
			port->rx_rc = -ERANGE;
		}

		/* Check for overrun */
		rmb();
		if ( rx->desc.generation != current ) {
			DBGC ( port, "EXANIC %s RX overrun\n", netdev->name );
			port->rx_rc = -ENOBUFS;
			continue;
		}

		/* Wait for end of packet */
		if ( ! rx->desc.len )
			continue;

		/* Check for receive errors */
		if ( rx->desc.status & EXANIC_STATUS_ERROR_MASK ) {
			port->rx_rc = -EIO_STATUS ( rx->desc.status );
			DBGC ( port, "EXANIC %s RX %04x error: %s\n",
			       netdev->name, port->rx_cons,
			       strerror ( port->rx_rc ) );
		} else {
			DBGC2 ( port, "EXANIC %s RX %04x\n",
				netdev->name, port->rx_cons );
		}

		/* Hand off to network stack */
		if ( port->rx_rc ) {
			netdev_rx_err ( netdev, port->rx_iobuf, port->rx_rc );
		} else {
			iob_unput ( port->rx_iobuf, 4 /* strip CRC */ );
			netdev_rx ( netdev, port->rx_iobuf );
		}
		port->rx_iobuf = NULL;
	}
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void exanic_poll ( struct net_device *netdev ) {

	/* Poll for completed packets */
	exanic_poll_tx ( netdev );

	/* Poll for received packets */
	exanic_poll_rx ( netdev );
}

/** ExaNIC network device operations */
static struct net_device_operations exanic_operations = {
	.open		= exanic_open,
	.close		= exanic_close,
	.transmit	= exanic_transmit,
	.poll		= exanic_poll,
};

/******************************************************************************
 *
 * PCI interface
 *
 ******************************************************************************
 */

/**
 * Probe port
 *
 * @v exanic		ExaNIC device
 * @v dev		Parent device
 * @v index		Port number
 * @ret rc		Return status code
 */
static int exanic_probe_port ( struct exanic *exanic, struct device *dev,
			       unsigned int index ) {
	struct net_device *netdev;
	struct exanic_port *port;
	void *port_regs;
	uint32_t status;
	size_t tx_len;
	int rc;

	/* Do nothing if port is not physically present */
	port_regs = ( exanic->regs + EXANIC_PORT_REGS ( index ) );
	status = readl ( port_regs + EXANIC_PORT_STATUS );
	tx_len = readl ( port_regs + EXANIC_PORT_TX_LEN );
	if ( ( status & EXANIC_PORT_STATUS_ABSENT ) || ( tx_len == 0 ) ) {
		rc = 0;
		goto absent;
	}

	/* Allocate network device */
	netdev = alloc_etherdev ( sizeof ( *port ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc_netdev;
	}
	netdev_init ( netdev, &exanic_operations );
	netdev->dev = dev;
	port = netdev->priv;
	memset ( port, 0, sizeof ( *port ) );
	exanic->port[index] = port;
	port->netdev = netdev;
	port->regs = port_regs;
	timer_init ( &port->timer, exanic_expired, &netdev->refcnt );

	/* Identify transmit region */
	port->tx_offset = readl ( port->regs + EXANIC_PORT_TX_OFFSET );
	if ( tx_len > EXANIC_MAX_TX_LEN )
		tx_len = EXANIC_MAX_TX_LEN;
	assert ( ! ( tx_len & ( tx_len - 1 ) ) );
	port->tx = ( exanic->tx + port->tx_offset );
	port->tx_count = ( tx_len / sizeof ( struct exanic_tx_chunk ) );

	/* Identify transmit feedback region */
	port->txf_slot = EXANIC_TXF_SLOT ( index );
	port->txf = ( exanic->txf +
		      ( port->txf_slot * sizeof ( *(port->txf) ) ) );

	/* Allocate receive region (via umalloc()) */
	port->rx = umalloc ( EXANIC_RX_LEN );
	if ( ! port->rx ) {
		rc = -ENOMEM;
		goto err_alloc_rx;
	}

	/* Set MAC address */
	memcpy ( netdev->hw_addr, exanic->mac, ETH_ALEN );
	netdev->hw_addr[ ETH_ALEN - 1 ] += index;

	/* Record default link speed and supported speeds */
	port->default_speed = readl ( port->regs + EXANIC_PORT_SPEED );
	port->speeds = ( exanic->caps & EXANIC_CAPS_SPEED_MASK );

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;
	DBGC ( port, "EXANIC %s port %d TX [%#05zx,%#05zx) TXF %#02x RX "
	       "[%#lx,%#lx)\n", netdev->name, index, port->tx_offset,
	       ( port->tx_offset + tx_len ), port->txf_slot,
	       virt_to_phys ( port->rx ),
	       ( virt_to_phys ( port->rx ) + EXANIC_RX_LEN ) );

	/* Set initial link state */
	exanic_check_link ( netdev );

	return 0;

	unregister_netdev ( netdev );
 err_register_netdev:
	ufree ( port->rx );
 err_alloc_rx:
	netdev_nullify ( netdev );
	netdev_put ( netdev );
 err_alloc_netdev:
 absent:
	return rc;
}

/**
 * Probe port
 *
 * @v exanic		ExaNIC device
 * @v index		Port number
 */
static void exanic_remove_port ( struct exanic *exanic, unsigned int index ) {
	struct exanic_port *port;

	/* Do nothing if port is not physically present */
	port = exanic->port[index];
	if ( ! port )
		return;

	/* Unregister network device */
	unregister_netdev ( port->netdev );

	/* Free receive region */
	ufree ( port->rx );

	/* Free network device */
	netdev_nullify ( port->netdev );
	netdev_put ( port->netdev );
}

/**
 * Probe PCI device
 *
 * @v pci		PCI device
 * @ret rc		Return status code
 */
static int exanic_probe ( struct pci_device *pci ) {
	struct exanic *exanic;
	unsigned long regs_bar_start;
	unsigned long tx_bar_start;
	size_t tx_bar_len;
	int i;
	int rc;

	/* Allocate and initialise structure */
	exanic = zalloc ( sizeof ( *exanic ) );
	if ( ! exanic ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	pci_set_drvdata ( pci, exanic );

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Map registers */
	regs_bar_start = pci_bar_start ( pci, EXANIC_REGS_BAR );
	exanic->regs = pci_ioremap ( pci, regs_bar_start, EXANIC_REGS_LEN );
	if ( ! exanic->regs ) {
		rc = -ENODEV;
		goto err_ioremap_regs;
	}

	/* Reset device */
	exanic_reset ( exanic );

	/* Read capabilities */
	exanic->caps = readl ( exanic->regs + EXANIC_CAPS );

	/* Power up PHYs */
	writel ( EXANIC_POWER_ON, ( exanic->regs + EXANIC_POWER ) );

	/* Fetch base MAC address */
	if ( ( rc = exanic_fetch_mac ( exanic ) ) != 0 )
		goto err_fetch_mac;
	DBGC ( exanic, "EXANIC %p capabilities %#08x base MAC %s\n",
	       exanic, exanic->caps, eth_ntoa ( exanic->mac ) );

	/* Map transmit region */
	tx_bar_start = pci_bar_start ( pci, EXANIC_TX_BAR );
	tx_bar_len = pci_bar_size ( pci, EXANIC_TX_BAR );
	exanic->tx = pci_ioremap ( pci, tx_bar_start, tx_bar_len );
	if ( ! exanic->tx ) {
		rc = -ENODEV;
		goto err_ioremap_tx;
	}

	/* Allocate transmit feedback region (shared between all ports) */
	exanic->txf = malloc_phys ( EXANIC_TXF_LEN, EXANIC_ALIGN );
	if ( ! exanic->txf ) {
		rc = -ENOMEM;
		goto err_alloc_txf;
	}
	memset ( exanic->txf, 0, EXANIC_TXF_LEN );
	exanic_write_base ( virt_to_bus ( exanic->txf ),
			    ( exanic->regs + EXANIC_TXF_BASE ) );

	/* Allocate and initialise per-port network devices */
	for ( i = 0 ; i < EXANIC_MAX_PORTS ; i++ ) {
		if ( ( rc = exanic_probe_port ( exanic, &pci->dev, i ) ) != 0 )
			goto err_probe_port;
	}

	return 0;

	i = EXANIC_MAX_PORTS;
 err_probe_port:
	for ( i-- ; i >= 0 ; i-- )
		exanic_remove_port ( exanic, i );
	exanic_reset ( exanic );
	free_phys ( exanic->txf, EXANIC_TXF_LEN );
 err_alloc_txf:
	iounmap ( exanic->tx );
 err_ioremap_tx:
	iounmap ( exanic->regs );
 err_fetch_mac:
 err_ioremap_regs:
	free ( exanic );
 err_alloc:
	return rc;
}

/**
 * Remove PCI device
 *
 * @v pci		PCI device
 */
static void exanic_remove ( struct pci_device *pci ) {
	struct exanic *exanic = pci_get_drvdata ( pci );
	unsigned int i;

	/* Remove all ports */
	for ( i = 0 ; i < EXANIC_MAX_PORTS ; i++ )
		exanic_remove_port ( exanic, i );

	/* Reset device */
	exanic_reset ( exanic );

	/* Free transmit feedback region */
	free_phys ( exanic->txf, EXANIC_TXF_LEN );

	/* Unmap transmit region */
	iounmap ( exanic->tx );

	/* Unmap registers */
	iounmap ( exanic->regs );

	/* Free device */
	free ( exanic );
}

/** ExaNIC PCI device IDs */
static struct pci_device_id exanic_ids[] = {
	PCI_ROM ( 0x10ee, 0x2b00, "exanic-old", "ExaNIC (old)", 0 ),
	PCI_ROM ( 0x1ce4, 0x0001, "exanic-x4", "ExaNIC X4", 0 ),
	PCI_ROM ( 0x1ce4, 0x0002, "exanic-x2", "ExaNIC X2", 0 ),
	PCI_ROM ( 0x1ce4, 0x0003, "exanic-x10", "ExaNIC X10", 0 ),
	PCI_ROM ( 0x1ce4, 0x0004, "exanic-x10gm", "ExaNIC X10 GM", 0 ),
	PCI_ROM ( 0x1ce4, 0x0005, "exanic-x40", "ExaNIC X40", 0 ),
	PCI_ROM ( 0x1ce4, 0x0006, "exanic-x10hpt", "ExaNIC X10 HPT", 0 ),
	PCI_ROM ( 0x1ce4, 0x0007, "exanic-x40g", "ExaNIC X40", 0 ),
};

/** ExaNIC PCI driver */
struct pci_driver exanic_driver __pci_driver = {
	.ids = exanic_ids,
	.id_count = ( sizeof ( exanic_ids ) / sizeof ( exanic_ids[0] ) ),
	.probe = exanic_probe,
	.remove = exanic_remove,
};
