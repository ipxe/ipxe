/*
 * Copyright (C) 2013 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <unistd.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/iobuf.h>
#include <ipxe/malloc.h>
#include <ipxe/pci.h>
#include "intelx.h"

/** @file
 *
 * Intel 10 Gigabit Ethernet network card driver
 *
 */

/******************************************************************************
 *
 * MAC address
 *
 ******************************************************************************
 */

/**
 * Try to fetch initial MAC address
 *
 * @v intel		Intel device
 * @v ral0		RAL0 register address
 * @v hw_addr		Hardware address to fill in
 * @ret rc		Return status code
 */
static int intelx_try_fetch_mac ( struct intel_nic *intel, unsigned int ral0,
				  uint8_t *hw_addr ) {
	union intel_receive_address mac;

	/* Read current address from RAL0/RAH0 */
	mac.reg.low = cpu_to_le32 ( readl ( intel->regs + ral0 ) );
	mac.reg.high = cpu_to_le32 ( readl ( intel->regs + ral0 +
					     ( INTELX_RAH0 - INTELX_RAL0 ) ) );

	/* Use current address if valid */
	if ( is_valid_ether_addr ( mac.raw ) ) {
		DBGC ( intel, "INTEL %p has autoloaded MAC address %s at "
		       "%#05x\n", intel, eth_ntoa ( mac.raw ), ral0 );
		memcpy ( hw_addr, mac.raw, ETH_ALEN );
		return 0;
	}

	return -ENOENT;
}

/**
 * Fetch initial MAC address
 *
 * @v intel		Intel device
 * @v hw_addr		Hardware address to fill in
 * @ret rc		Return status code
 */
static int intelx_fetch_mac ( struct intel_nic *intel, uint8_t *hw_addr ) {
	int rc;

	/* Try to fetch address from INTELX_RAL0 */
	if ( ( rc = intelx_try_fetch_mac ( intel, INTELX_RAL0,
					   hw_addr ) ) == 0 ) {
		return 0;
	}

	/* Try to fetch address from INTELX_RAL0_ALT */
	if ( ( rc = intelx_try_fetch_mac ( intel, INTELX_RAL0_ALT,
					   hw_addr ) ) == 0 ) {
		return 0;
	}

	DBGC ( intel, "INTEL %p has no MAC address to use\n", intel );
	return -ENOENT;
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
 * @v intel		Intel device
 * @ret rc		Return status code
 */
static int intelx_reset ( struct intel_nic *intel ) {
	uint32_t ctrl;

	/* Perform a global software reset */
	ctrl = readl ( intel->regs + INTELX_CTRL );
	writel ( ( ctrl | INTELX_CTRL_RST | INTELX_CTRL_LRST ),
		 intel->regs + INTELX_CTRL );
	mdelay ( INTELX_RESET_DELAY_MS );

	DBGC ( intel, "INTEL %p reset (ctrl %08x)\n", intel, ctrl );
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
static void intelx_check_link ( struct net_device *netdev ) {
	struct intel_nic *intel = netdev->priv;
	uint32_t links;

	/* Read link status */
	links = readl ( intel->regs + INTELX_LINKS );
	DBGC ( intel, "INTEL %p link status is %08x\n", intel, links );

	/* Update network device */
	if ( links & INTELX_LINKS_UP ) {
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
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int intelx_open ( struct net_device *netdev ) {
	struct intel_nic *intel = netdev->priv;
	union intel_receive_address mac;
	uint32_t ral0;
	uint32_t rah0;
	uint32_t dmatxctl;
	uint32_t fctrl;
	uint32_t srrctl;
	uint32_t hlreg0;
	uint32_t maxfrs;
	uint32_t rdrxctl;
	uint32_t rxctrl;
	uint32_t dca_rxctrl;
	int rc;

	/* Create transmit descriptor ring */
	if ( ( rc = intel_create_ring ( intel, &intel->tx ) ) != 0 )
		goto err_create_tx;

	/* Create receive descriptor ring */
	if ( ( rc = intel_create_ring ( intel, &intel->rx ) ) != 0 )
		goto err_create_rx;

	/* Program MAC address */
	memset ( &mac, 0, sizeof ( mac ) );
	memcpy ( mac.raw, netdev->ll_addr, sizeof ( mac.raw ) );
	ral0 = le32_to_cpu ( mac.reg.low );
	rah0 = ( le32_to_cpu ( mac.reg.high ) | INTELX_RAH0_AV );
	writel ( ral0, intel->regs + INTELX_RAL0 );
	writel ( rah0, intel->regs + INTELX_RAH0 );
	writel ( ral0, intel->regs + INTELX_RAL0_ALT );
	writel ( rah0, intel->regs + INTELX_RAH0_ALT );

	/* Allocate interrupt vectors */
	writel ( ( INTELX_IVAR_RX0_DEFAULT | INTELX_IVAR_RX0_VALID |
		   INTELX_IVAR_TX0_DEFAULT | INTELX_IVAR_TX0_VALID ),
		 intel->regs + INTELX_IVAR );

	/* Enable transmitter  */
	dmatxctl = readl ( intel->regs + INTELX_DMATXCTL );
	dmatxctl |= INTELX_DMATXCTL_TE;
	writel ( dmatxctl, intel->regs + INTELX_DMATXCTL );

	/* Configure receive filter */
	fctrl = readl ( intel->regs + INTELX_FCTRL );
	fctrl |= ( INTELX_FCTRL_BAM | INTELX_FCTRL_UPE | INTELX_FCTRL_MPE );
	writel ( fctrl, intel->regs + INTELX_FCTRL );

	/* Configure receive buffer sizes */
	srrctl = readl ( intel->regs + INTELX_SRRCTL );
	srrctl &= ~INTELX_SRRCTL_BSIZE_MASK;
	srrctl |= INTELX_SRRCTL_BSIZE_DEFAULT;
	writel ( srrctl, intel->regs + INTELX_SRRCTL );

	/* Configure jumbo frames.  Required to allow the extra 4-byte
	 * headroom for VLANs, since we don't use the hardware's
	 * native VLAN offload.
	 */
	hlreg0 = readl ( intel->regs + INTELX_HLREG0 );
	hlreg0 |= INTELX_HLREG0_JUMBOEN;
	writel ( hlreg0, intel->regs + INTELX_HLREG0 );

	/* Configure frame size */
	maxfrs = readl ( intel->regs + INTELX_MAXFRS );
	maxfrs &= ~INTELX_MAXFRS_MFS_MASK;
	maxfrs |= INTELX_MAXFRS_MFS_DEFAULT;
	writel ( maxfrs, intel->regs + INTELX_MAXFRS );

	/* Configure receive DMA */
	rdrxctl = readl ( intel->regs + INTELX_RDRXCTL );
	rdrxctl |= INTELX_RDRXCTL_SECRC;
	writel ( rdrxctl, intel->regs + INTELX_RDRXCTL );

	/* Clear "must-be-zero" bit for direct cache access (DCA).  We
	 * leave DCA disabled anyway, but if we do not clear this bit
	 * then the received packets contain garbage data.
	 */
	dca_rxctrl = readl ( intel->regs + INTELX_DCA_RXCTRL );
	dca_rxctrl &= ~INTELX_DCA_RXCTRL_MUST_BE_ZERO;
	writel ( dca_rxctrl, intel->regs + INTELX_DCA_RXCTRL );

	/* Enable receiver */
	rxctrl = readl ( intel->regs + INTELX_RXCTRL );
	rxctrl |= INTELX_RXCTRL_RXEN;
	writel ( rxctrl, intel->regs + INTELX_RXCTRL );

	/* Fill receive ring */
	intel_refill_rx ( intel );

	/* Update link state */
	intelx_check_link ( netdev );

	return 0;

	intel_destroy_ring ( intel, &intel->rx );
 err_create_rx:
	intel_destroy_ring ( intel, &intel->tx );
 err_create_tx:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void intelx_close ( struct net_device *netdev ) {
	struct intel_nic *intel = netdev->priv;
	uint32_t rxctrl;
	uint32_t dmatxctl;

	/* Disable receiver */
	rxctrl = readl ( intel->regs + INTELX_RXCTRL );
	rxctrl &= ~INTELX_RXCTRL_RXEN;
	writel ( rxctrl, intel->regs + INTELX_RXCTRL );

	/* Disable transmitter  */
	dmatxctl = readl ( intel->regs + INTELX_DMATXCTL );
	dmatxctl &= ~INTELX_DMATXCTL_TE;
	writel ( dmatxctl, intel->regs + INTELX_DMATXCTL );

	/* Destroy receive descriptor ring */
	intel_destroy_ring ( intel, &intel->rx );

	/* Discard any unused receive buffers */
	intel_empty_rx ( intel );

	/* Destroy transmit descriptor ring */
	intel_destroy_ring ( intel, &intel->tx );

	/* Reset the NIC, to flush the transmit and receive FIFOs */
	intelx_reset ( intel );
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void intelx_poll ( struct net_device *netdev ) {
	struct intel_nic *intel = netdev->priv;
	uint32_t eicr;

	/* Check for and acknowledge interrupts */
	eicr = readl ( intel->regs + INTELX_EICR );
	if ( ! eicr )
		return;

	/* Poll for TX completions, if applicable */
	if ( eicr & INTELX_EIRQ_TX0 )
		intel_poll_tx ( netdev );

	/* Poll for RX completions, if applicable */
	if ( eicr & ( INTELX_EIRQ_RX0 | INTELX_EIRQ_RXO ) )
		intel_poll_rx ( netdev );

	/* Report receive overruns */
	if ( eicr & INTELX_EIRQ_RXO )
		netdev_rx_err ( netdev, NULL, -ENOBUFS );

	/* Check link state, if applicable */
	if ( eicr & INTELX_EIRQ_LSC )
		intelx_check_link ( netdev );

	/* Refill RX ring */
	intel_refill_rx ( intel );
}

/**
 * Enable or disable interrupts
 *
 * @v netdev		Network device
 * @v enable		Interrupts should be enabled
 */
static void intelx_irq ( struct net_device *netdev, int enable ) {
	struct intel_nic *intel = netdev->priv;
	uint32_t mask;

	mask = ( INTELX_EIRQ_LSC | INTELX_EIRQ_RXO | INTELX_EIRQ_TX0 |
		 INTELX_EIRQ_RX0 );
	if ( enable ) {
		writel ( mask, intel->regs + INTELX_EIMS );
	} else {
		writel ( mask, intel->regs + INTELX_EIMC );
	}
}

/** Network device operations */
static struct net_device_operations intelx_operations = {
	.open		= intelx_open,
	.close		= intelx_close,
	.transmit	= intel_transmit,
	.poll		= intelx_poll,
	.irq		= intelx_irq,
};

/******************************************************************************
 *
 * PCI interface
 *
 ******************************************************************************
 */

/**
 * Set driver loaded in CTRL_EXT
 *
 * @v intel		Intel device
 */
static void intelx_driver_loaded ( struct intel_nic *intel ) {
	uint32_t ctrl_ext;

	ctrl_ext = readl ( intel->regs + INTELX_CTRL_EXT );
	ctrl_ext |= INTELX_CTRL_EXT_DRV_LOAD;
	writel ( ctrl_ext, intel->regs + INTELX_CTRL_EXT );
}

/**
 * Clear firmware reset bit in FWSTS
 *
 * @v intel		Intel device
 */
static void intelx_clear_firmware_reset ( struct intel_nic *intel ) {
	uint32_t fwsts;

	fwsts = readl ( intel->regs + INTELX_FWSTS );
	fwsts |= INTELX_FWSTS_FWRI;
	writel ( fwsts, intel->regs + INTELX_FWSTS );
}

/**
 * Acquire software driver semaphore (SWSM)
 *
 * @v intel		Intel device
 * @ret rc		Return status code
 */
static int intelx_acquire_software_semaphore ( struct intel_nic *intel ) {
	uint32_t i, swsm;

	for ( i = 0; i < INTELX_SEMAPHORE_ATTEMPTS; i++) {
		swsm = readl ( intel->regs + INTELX_SWSM ( intel->flags ) );
		/* SMBI is 0 if we got the semaphore */
		if ( ! ( swsm & INTELX_SWSM_SMBI ) ) {
			return 0;
		}
		udelay(INTELX_SEMAPHORE_DELAY);
	}
	DBGC ( intel, "INTEL %p acquire SWSM timed out\n", intel );
	return -ETIMEDOUT;
}

/**
 * Release software driver semaphore (SWSM)
 *
 * @v intel		Intel device
 */
static void intelx_release_software_semaphore ( struct intel_nic *intel ) {
	uint32_t swsm;

	swsm = readl ( intel->regs + INTELX_SWSM ( intel->flags ) );
	swsm &= ~INTELX_SWSM_SMBI;
	writel ( swsm, intel->regs + INTELX_SWSM ( intel->flags ) );
}

/**
 * Acquire software/firmware semaphore (SW_FW_SYNC)
 *
 * @v intel		Intel device
 * @ret rc		Return status code
 */
static int intelx_acquire_sync_register_semaphore ( struct intel_nic *intel ) {
	uint32_t i, sw_fw_sync;

	for ( i = 0; i < INTELX_SEMAPHORE_ATTEMPTS; i++) {
		sw_fw_sync = readl ( intel->regs + INTELX_SW_FW_SYNC ( intel->flags ) );
		/* REGSMP is 0 if we got the semaphore */
		if ( ! ( sw_fw_sync & INTELX_SW_FW_SYNC_SW_MNG_SM ) ) {
			return 0;
		}
		udelay(INTELX_SEMAPHORE_DELAY);
	}
	DBGC ( intel, "INTEL %p acquire SW_FW_SYNC timed out\n", intel );
	return -ETIMEDOUT;
}

/**
 * Release software/firmware semaphore (SW_FW_SYNC)
 *
 * @v intel		Intel device
 */
static void intelx_release_sync_register_semaphore ( struct intel_nic * intel ) {
	uint32_t sw_fw_sync;

	sw_fw_sync = readl ( intel->regs + INTELX_SW_FW_SYNC ( intel->flags ) );
	sw_fw_sync &= ~INTELX_SW_FW_SYNC_SW_MNG_SM;
	writel ( sw_fw_sync, intel->regs + INTELX_SW_FW_SYNC ( intel->flags ) );
}

/**
 * Acquire resources that are shared by software and firmware
 *
 * @v intel		Intel device
 * @v res		Resource bitmask to acquire
 * @ret rc		Return status code
 */
static int intelx_acquire_shared_resources ( struct intel_nic *intel,
		uint32_t res ) {
	uint32_t sw_fw_sync;
	int rc;

	/* Gain control of SW_FW_SYNC */
	rc = intelx_acquire_software_semaphore ( intel );
	if ( rc ) {
		return rc;
	}
	rc = intelx_acquire_sync_register_semaphore ( intel );
	if ( rc ) {
		goto err_acquire_sync_register_semaphore;
	}

	/* Make sure firmware doesn't own the requested resource */
	sw_fw_sync = readl ( intel->regs + INTELX_SW_FW_SYNC ( intel->flags ) );
	if ( ( res & INTELX_SW_FW_SYNC_SW_PHY0_SM ) &&
			( sw_fw_sync & INTELX_SW_FW_SYNC_FW_PHY0_SM ) ) {
		rc = -EBUSY;
		goto err_firmware_owns_resource;
	} else if ( ( res & INTELX_SW_FW_SYNC_SW_PHY1_SM ) &&
			( sw_fw_sync & INTELX_SW_FW_SYNC_FW_PHY1_SM ) ) {
		rc = -EBUSY;
		goto err_firmware_owns_resource;
	} else if ( ( res & INTELX_SW_FW_SYNC_SW_MAC_CSR_SM ) &&
			( sw_fw_sync & INTELX_SW_FW_SYNC_FW_MAC_CSR_SM ) ) {
		rc = -EBUSY;
		goto err_firmware_owns_resource;
	}

	/* Mark resources as owned */
	sw_fw_sync |= res;
	writel ( sw_fw_sync, intel->regs + INTELX_SW_FW_SYNC ( intel->flags ) );

	/* Release SW_FW_SYNC */
err_firmware_owns_resource:
	intelx_release_sync_register_semaphore ( intel );
err_acquire_sync_register_semaphore:
	intelx_release_software_semaphore ( intel );
	return rc;
}

/**
 * Release resources that are shared by software and firmware
 *
 * @v intel		Intel device
 * @v res		Resource bitmask to acquire
 */
static int intelx_release_shared_resources ( struct intel_nic *intel,
		uint32_t res ) {
	uint32_t sw_fw_sync;
	int rc;

	/* Gain control of SW_FW_SYNC */
	rc = intelx_acquire_software_semaphore ( intel );
	if ( rc ) {
		return rc;
	}
	rc = intelx_acquire_sync_register_semaphore ( intel );
	if ( rc ) {
		goto err_acquire_sync_register_semaphore;
	}

	/* Clear resource ownership */
	sw_fw_sync = readl ( intel->regs + INTELX_SW_FW_SYNC ( intel->flags ) );
	sw_fw_sync &= ~res;
	writel ( sw_fw_sync, intel->regs + INTELX_SW_FW_SYNC ( intel->flags ) );

	/* Release SW_FW_SYNC */
	intelx_release_sync_register_semaphore ( intel );
err_acquire_sync_register_semaphore:
	intelx_release_software_semaphore ( intel );
	return rc;
}

/**
 * Send a CSR command block to the host manageability interface
 *
 * @v intel		Intel device
 * @v buffer	Buffer containing CSR command block
 * @v num_words	Number of 32-bit words in buffer
 * @ret rc		Return status code
 */
static int intelx_write_host_interface_command ( struct intel_nic *intel,
		uint32_t *buffer, uint32_t num_words ) {
	uint32_t i, hicr;

	/* Clear any previous firmware resets */
	intelx_clear_firmware_reset( intel );

	/* Make sure host interface is enabled */
	hicr = readl ( intel->regs + INTELX_HICR );
	if ( ! ( hicr & INTELX_HICR_EN ) ) {
		return -ENOTSUP;
	}

	/* Write the command words to ARC Data RAM */
	for (i = 0; i < num_words; i++) {
		writel ( buffer[i], intel->regs + INTELX_ARCRAM + 
			sizeof(uint32_t) * i );
	}

	/* Wait for command to complete */
	hicr |= INTELX_HICR_C;
	writel ( hicr, intel->regs + INTELX_HICR );

	for (i = 0; i < INTELX_SEMAPHORE_ATTEMPTS; i++) {
		hicr = readl ( intel->regs + INTELX_HICR );
		if ( ! ( hicr & INTELX_HICR_C ) ) {
			return 0;
		}
		udelay(INTELX_SEMAPHORE_DELAY);
	}
	DBGC ( intel, "INTEL %p host interface command timed out\n", intel );
	return -ETIMEDOUT;
}

/**
 * Read a response to a command from the host manageability interface
 *
 * @v intel		Intel device
 * @v buffer	Buffer to read into
 * @v num_words	Size of buffer in 32-bit words
 * @ret rc		Return status code
 */
static int intelx_read_host_interface_response ( struct intel_nic *intel,
		uint32_t *buffer, uint32_t num_words ) {
	uint32_t i, reg;

	for (i = 0; i < num_words; i++) {
		reg = readl ( intel->regs + INTELX_ARCRAM + sizeof(uint32_t) * i );
		if ( buffer ) {
			buffer[i] = reg;
		}
	}
	return 0;
}

/**
 * Probe PCI device
 *
 * @v pci		PCI device
 * @ret rc		Return status code
 */
static int intelx_probe ( struct pci_device *pci ) {
	struct net_device *netdev;
	struct intel_nic *intel;
	struct intelx_hic_req hic_req;
	uint32_t res = 0;
	int rc;

	/* Allocate and initialise net device */
	netdev = alloc_etherdev ( sizeof ( *intel ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &intelx_operations );
	intel = netdev->priv;
	pci_set_drvdata ( pci, netdev );
	netdev->dev = &pci->dev;
	memset ( intel, 0, sizeof ( *intel ) );
	intel->port = PCI_FUNC ( pci->busdevfn );
	intel->flags = pci->id->driver_data;
	intel_init_ring ( &intel->tx, INTEL_NUM_TX_DESC, INTELX_TD,
			  intel_describe_tx );
	intel_init_ring ( &intel->rx, INTEL_NUM_RX_DESC, INTELX_RD,
			  intel_describe_rx );

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Map registers */
	intel->regs = ioremap ( pci->membase, INTEL_BAR_SIZE );
	if ( ! intel->regs ) {
		rc = -ENODEV;
		goto err_ioremap;
	}

	/* Reset the NIC */
	if ( ( rc = intelx_reset ( intel ) ) != 0 )
		goto err_reset;

	/* 
	 * Some chipsets need the physical interface to be brought up
	 * through the manageability interface.
	 *
	 * This is typically configured to be done by the BMC or ME.
	 * Doing it here ensures that the link always comes up.
	 */
	if ( intel->flags & INTELX_SW_LINK_UP ) {
		DBGC ( intel, "INTEL %p setting up link\n", intel );

		/* Set up driver for firmware interaction */
		intelx_driver_loaded ( intel );

		/* Gain access to the physical interfaces */
		res = INTELX_SW_FW_SYNC_SW_PHY0_SM |
			INTELX_SW_FW_SYNC_SW_PHY1_SM |
			INTELX_SW_FW_SYNC_SW_MAC_CSR_SM |
			INTELX_SW_FW_SYNC_SW_MNG_SM;
		rc = intelx_acquire_shared_resources ( intel, res );
		if ( rc ) {
			goto err_acquire_shared_resources;
		}

		/* Set up command block */
		memset ( &hic_req, 0, sizeof ( struct intelx_hic_req ) );
		hic_req.hdr.cmd = INTELX_HIC_HDR_CMD_REQ;
		hic_req.hdr.buf_len = sizeof ( struct intelx_hic_req ) - 
			sizeof( struct intelx_hic_hdr );
		hic_req.hdr.checksum = 0xFF;

		if ( readl ( intel->regs + INTELX_STATUS ) & INTELX_STATUS_LAN_ID ) {
			hic_req.port_number = 1;
		} else {
			hic_req.port_number = 0;
		}

		/* Probe PHY */
		hic_req.activity_id = INTELX_HIC_REQ_ACT_PHY_GET_INFO;
		rc = intelx_write_host_interface_command ( intel,
			( uint32_t* ) ( &hic_req ), 6 );
		if ( rc ) {
			goto err_csr;
		}
		intelx_read_host_interface_response ( intel, NULL, 5 );

		/* Reset PHY */
		hic_req.activity_id = INTELX_HIC_REQ_ACT_PHY_SW_RESET;
		rc = intelx_write_host_interface_command ( intel,
			( uint32_t* ) ( &hic_req ), 6 );
		if ( rc ) {
			goto err_csr;
		}
		intelx_read_host_interface_response ( intel, NULL, 5 );

		/* Delay after reset for PHY to be responsive */
		mdelay ( 1000 );

		/* Init PHY */
		hic_req.activity_id = INTELX_HIC_REQ_ACT_PHY_INIT;
		rc = intelx_write_host_interface_command ( intel,
			( uint32_t* ) ( &hic_req ), 6 );
		if ( rc ) {
			goto err_csr;
		}
		intelx_read_host_interface_response ( intel, NULL, 5 );

		/* Setup Link */
		hic_req.activity_id = INTELX_HIC_REQ_ACT_PHY_SETUP_LINK;
		hic_req.data[0] = INTELX_HIC_REQ_SETUP_LINK_DATA0;
		rc = intelx_write_host_interface_command ( intel,
			( uint32_t* ) ( &hic_req ), 6 );
		if ( rc ) {
			goto err_csr;
		}
		intelx_read_host_interface_response ( intel, NULL, 5 );

		/* Release access to the physical interfaces */
		intelx_release_shared_resources ( intel, res );
	}

	/* Fetch MAC address */
	if ( ( rc = intelx_fetch_mac ( intel, netdev->hw_addr ) ) != 0 )
		goto err_fetch_mac;

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	/* Set initial link state */
	intelx_check_link ( netdev );

	return 0;

	unregister_netdev ( netdev );
 err_register_netdev:
 err_fetch_mac:
	intelx_reset ( intel );
 err_csr:
 	if ( intel->flags & INTELX_SW_LINK_UP )
 		intelx_release_shared_resources ( intel, res );
 err_acquire_shared_resources:
 err_reset:
	iounmap ( intel->regs );
 err_ioremap:
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
static void intelx_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );
	struct intel_nic *intel = netdev->priv;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Reset the NIC */
	intelx_reset ( intel );

	/* Free network device */
	iounmap ( intel->regs );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** PCI device IDs */
static struct pci_device_id intelx_nics[] = {
	PCI_ROM ( 0x8086, 0x10f7, "82599-kx4", "82599 (KX/KX4)", 0 ),
	PCI_ROM ( 0x8086, 0x10f8, "82599-combo-backplane", "82599 (combined backplane; KR/KX4/KX)", 0 ),
	PCI_ROM ( 0x8086, 0x10f9, "82599-cx4", "82599 (CX4)", 0 ),
	PCI_ROM ( 0x8086, 0x10fb, "82599-sfp", "82599 (SFI/SFP+)", 0 ),
	PCI_ROM ( 0x8086, 0x10fc, "82599-xaui", "82599 (XAUI/BX4)", 0 ),
	PCI_ROM ( 0x8086, 0x1528, "x540t", "X540-AT2/X540-BT2", 0 ),
	PCI_ROM ( 0x8086, 0x154d, "82599-sfp-sf2", "82599 (SFI/SFP+)", 0 ),
	PCI_ROM ( 0x8086, 0x1557, "82599en-sfp", "82599 (Single Port SFI Only)", 0 ),
	PCI_ROM ( 0x8086, 0x1560, "x540t1", "X540-AT2/X540-BT2 (with single port NVM)", 0 ),
	PCI_ROM ( 0x8086, 0x1563, "x550t2", "X550-T2", 0 ),
	PCI_ROM ( 0x8086, 0x15ab, "x552", "X552", 0 ),
	PCI_ROM ( 0x8086, 0x15e5, "x553", "X553", INTELX_X550EM_A | INTELX_SW_LINK_UP ),
};

/** PCI driver */
struct pci_driver intelx_driver __pci_driver = {
	.ids = intelx_nics,
	.id_count = ( sizeof ( intelx_nics ) / sizeof ( intelx_nics[0] ) ),
	.probe = intelx_probe,
	.remove = intelx_remove,
};
