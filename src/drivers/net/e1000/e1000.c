/*
 * gPXE driver for Intel eepro1000 ethernet cards
 *
 * Written by Marty Connor
 *
 * Copyright Entity Cyber, Inc. 2007
 *
 * This software may be used and distributed according to the terms of
 * the GNU General Public License (GPL), incorporated herein by
 * reference.  Drivers based on or derived from this code fall under
 * the GPL and must retain the authorship, copyright and license
 * notice.
 *
 */

/*******************************************************************************

  Intel PRO/1000 Linux driver
  Copyright(c) 1999 - 2006 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

FILE_LICENCE ( GPL2_ONLY );

#include "e1000.h"

/**
 * e1000_get_hw_control - get control of the h/w from f/w
 *
 * @v adapter	e1000 private structure
 *
 * e1000_get_hw_control sets {CTRL_EXT|FWSM}:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that
 * the driver is loaded. For AMT version (only with 82573)
 * of the f/w this means that the network i/f is open.
 *
 **/
static void
e1000_get_hw_control ( struct e1000_adapter *adapter )
{
	uint32_t ctrl_ext;
	uint32_t swsm;
	
	DBG ( "e1000_get_hw_control\n" );

	/* Let firmware know the driver has taken over */
	switch (adapter->hw.mac_type) {
	case e1000_82573:
		swsm = E1000_READ_REG(&adapter->hw, SWSM);
		E1000_WRITE_REG(&adapter->hw, SWSM,
				swsm | E1000_SWSM_DRV_LOAD);
		break;
	case e1000_82571:
	case e1000_82572:
	case e1000_82576:
	case e1000_80003es2lan:
	case e1000_ich8lan:
		ctrl_ext = E1000_READ_REG(&adapter->hw, CTRL_EXT);
		E1000_WRITE_REG(&adapter->hw, CTRL_EXT,
				ctrl_ext | E1000_CTRL_EXT_DRV_LOAD);
		break;
	default:
		break;
	}
}

/**
 * e1000_irq_enable - Enable default interrupt generation settings
 *
 * @v adapter	e1000 private structure
 **/
static void
e1000_irq_enable ( struct e1000_adapter *adapter )
{
	E1000_WRITE_REG ( &adapter->hw, IMS, IMS_ENABLE_MASK );
	E1000_WRITE_FLUSH ( &adapter->hw );
}

/**
 * e1000_irq_disable - Mask off interrupt generation on the NIC
 *
 * @v adapter	e1000 private structure
 **/
static void
e1000_irq_disable ( struct e1000_adapter *adapter )
{
	E1000_WRITE_REG ( &adapter->hw, IMC, ~0 );
	E1000_WRITE_FLUSH ( &adapter->hw );
}

/**
 * e1000_sw_init - Initialize general software structures (struct e1000_adapter)
 *
 * @v adapter	e1000 private structure
 *
 * e1000_sw_init initializes the Adapter private data structure.
 * Fields are initialized based on PCI device information and
 * OS network device settings (MTU size).
 **/
static int
e1000_sw_init ( struct e1000_adapter *adapter )
{
	struct e1000_hw *hw = &adapter->hw;
	struct pci_device *pdev = adapter->pdev;

	/* PCI config space info */

	hw->vendor_id = pdev->vendor;
	hw->device_id = pdev->device;

	pci_read_config_word ( pdev, PCI_COMMAND, &hw->pci_cmd_word );

	/* Disable Flow Control */
	hw->fc = E1000_FC_NONE;

	adapter->eeprom_wol = 0;
	adapter->wol = adapter->eeprom_wol;
	adapter->en_mng_pt  = 0;
	adapter->rx_int_delay = 0;
	adapter->rx_abs_int_delay = 0;

        adapter->rx_buffer_len = MAXIMUM_ETHERNET_VLAN_SIZE;
        adapter->rx_ps_bsize0 = E1000_RXBUFFER_128;
        hw->max_frame_size = MAXIMUM_ETHERNET_VLAN_SIZE +
		ENET_HEADER_SIZE + ETHERNET_FCS_SIZE;
        hw->min_frame_size = MINIMUM_ETHERNET_FRAME_SIZE;

	/* identify the MAC */

	if ( e1000_set_mac_type ( hw ) ) {
		DBG ( "Unknown MAC Type\n" );
		return -EIO;
	}

	switch ( hw->mac_type ) {
	default:
		break;
	case e1000_82541:
	case e1000_82547:
	case e1000_82541_rev_2:
	case e1000_82547_rev_2:
		hw->phy_init_script = 1;
		break;
	}

	e1000_set_media_type ( hw );

	hw->autoneg = TRUE;
	hw->autoneg_advertised = AUTONEG_ADVERTISE_SPEED_DEFAULT;
	hw->wait_autoneg_complete = TRUE;

	hw->tbi_compatibility_en = TRUE;
	hw->adaptive_ifs = TRUE;

	/* Copper options */

	if ( hw->media_type == e1000_media_type_copper ) {
		hw->mdix = AUTO_ALL_MODES;
		hw->disable_polarity_correction = FALSE;
		hw->master_slave = E1000_MASTER_SLAVE;
	}

	e1000_irq_disable ( adapter );

	return 0;
}

/**
 * e1000_setup_tx_resources - allocate Tx resources (Descriptors)
 *
 * @v adapter	e1000 private structure
 *
 * @ret rc       Returns 0 on success, negative on failure
 **/
static int
e1000_setup_tx_resources ( struct e1000_adapter *adapter )
{
	DBG ( "e1000_setup_tx_resources\n" );
	
	/* Allocate transmit descriptor ring memory.
	   It must not cross a 64K boundary because of hardware errata #23
	   so we use malloc_dma() requesting a 128 byte block that is
	   128 byte aligned. This should guarantee that the memory
	   allocated will not cross a 64K boundary, because 128 is an
	   even multiple of 65536 ( 65536 / 128 == 512 ), so all possible
	   allocations of 128 bytes on a 128 byte boundary will not
	   cross 64K bytes.
	 */

        adapter->tx_base = 
        	malloc_dma ( adapter->tx_ring_size, adapter->tx_ring_size );
        		             		     
       	if ( ! adapter->tx_base ) {
       		return -ENOMEM;
	}
	
	memset ( adapter->tx_base, 0, adapter->tx_ring_size );
	
	DBG ( "adapter->tx_base = %#08lx\n", virt_to_bus ( adapter->tx_base ) );

	return 0;
}

static void
e1000_free_tx_resources ( struct e1000_adapter *adapter )
{
	DBG ( "e1000_free_tx_resources\n" );

        free_dma ( adapter->tx_base, adapter->tx_ring_size );
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Tx unit of the MAC after a reset.
 **/
static void
e1000_configure_tx ( struct e1000_adapter *adapter )
{
	struct e1000_hw *hw = &adapter->hw;
	uint32_t tctl;
	uint32_t txdctl;

	DBG ( "e1000_configure_tx\n" );

	E1000_WRITE_REG ( hw, TDBAH, 0 );
	E1000_WRITE_REG ( hw, TDBAL, virt_to_bus ( adapter->tx_base ) );
	E1000_WRITE_REG ( hw, TDLEN, adapter->tx_ring_size );
			  
        DBG ( "TDBAL: %#08x\n",  E1000_READ_REG ( hw, TDBAL ) );
        DBG ( "TDLEN: %d\n",     E1000_READ_REG ( hw, TDLEN ) );

	/* Setup the HW Tx Head and Tail descriptor pointers */
	E1000_WRITE_REG ( hw, TDH, 0 );
	E1000_WRITE_REG ( hw, TDT, 0 );

	adapter->tx_head = 0;
	adapter->tx_tail = 0;
	adapter->tx_fill_ctr = 0;

	if (hw->mac_type == e1000_82576) {
		txdctl = E1000_READ_REG ( hw, TXDCTL );
		txdctl |= E1000_TXDCTL_QUEUE_ENABLE;
		E1000_WRITE_REG ( hw, TXDCTL, txdctl );
	}

	/* Setup Transmit Descriptor Settings for eop descriptor */
	tctl = E1000_TCTL_PSP | E1000_TCTL_EN |
		(E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT) | 
		(E1000_HDX_COLLISION_DISTANCE << E1000_COLD_SHIFT);

	e1000_config_collision_dist ( hw );

	E1000_WRITE_REG ( hw, TCTL, tctl );
        E1000_WRITE_FLUSH ( hw );
}

static void
e1000_free_rx_resources ( struct e1000_adapter *adapter )
{
	int i;

	DBG ( "e1000_free_rx_resources\n" );

	free_dma ( adapter->rx_base, adapter->rx_ring_size );

	for ( i = 0; i < NUM_RX_DESC; i++ ) {
		free_iob ( adapter->rx_iobuf[i] );
	}
}

/**
 * e1000_refill_rx_ring - allocate Rx io_buffers
 *
 * @v adapter	e1000 private structure
 *
 * @ret rc       Returns 0 on success, negative on failure
 **/
int e1000_refill_rx_ring ( struct e1000_adapter *adapter )
{
	int i, rx_curr;
	int rc = 0;
	struct e1000_rx_desc *rx_curr_desc;
	struct e1000_hw *hw = &adapter->hw;
	struct io_buffer *iob;

	DBG ("e1000_refill_rx_ring\n");

	for ( i = 0; i < NUM_RX_DESC; i++ ) {
		rx_curr = ( ( adapter->rx_curr + i ) % NUM_RX_DESC );
		rx_curr_desc = adapter->rx_base + rx_curr;

		if ( rx_curr_desc->status & E1000_RXD_STAT_DD )
			continue;

		if ( adapter->rx_iobuf[rx_curr] != NULL )
			continue;

		DBG2 ( "Refilling rx desc %d\n", rx_curr );

		iob = alloc_iob ( MAXIMUM_ETHERNET_VLAN_SIZE );
		adapter->rx_iobuf[rx_curr] = iob;

		if ( ! iob ) {
			DBG ( "alloc_iob failed\n" );
			rc = -ENOMEM;
			break;
		} else {
			rx_curr_desc->buffer_addr = virt_to_bus ( iob->data );

			E1000_WRITE_REG ( hw, RDT, rx_curr );
		}
	}
	return rc;
}

/**
 * e1000_setup_rx_resources - allocate Rx resources (Descriptors)
 *
 * @v adapter	e1000 private structure
 *
 * @ret rc       Returns 0 on success, negative on failure
 **/
static int
e1000_setup_rx_resources ( struct e1000_adapter *adapter )
{
	int i, rc = 0;
	
	DBG ( "e1000_setup_rx_resources\n" );
	
	/* Allocate receive descriptor ring memory.
	   It must not cross a 64K boundary because of hardware errata
	 */

        adapter->rx_base = 
        	malloc_dma ( adapter->rx_ring_size, adapter->rx_ring_size );

       	if ( ! adapter->rx_base ) {
       		return -ENOMEM;
	}
	memset ( adapter->rx_base, 0, adapter->rx_ring_size );

	for ( i = 0; i < NUM_RX_DESC; i++ ) {
		/* let e1000_refill_rx_ring() io_buffer allocations */
		adapter->rx_iobuf[i] = NULL;
	}

	/* allocate io_buffers */
	rc = e1000_refill_rx_ring ( adapter );
	if ( rc < 0 )
		e1000_free_rx_resources ( adapter );

	return rc;
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Rx unit of the MAC after a reset.
 **/
static void
e1000_configure_rx ( struct e1000_adapter *adapter )
{
	struct e1000_hw *hw = &adapter->hw;
	uint32_t rctl, rxdctl, mrqc, rxcsum;

	DBG ( "e1000_configure_rx\n" );

	/* disable receives while setting up the descriptors */
	rctl = E1000_READ_REG ( hw, RCTL );
	E1000_WRITE_REG ( hw, RCTL, rctl & ~E1000_RCTL_EN );
	E1000_WRITE_FLUSH ( hw );
	mdelay(10);

	adapter->rx_curr = 0;

	/* Setup the HW Rx Head and Tail Descriptor Pointers and
	 * the Base and Length of the Rx Descriptor Ring */	 

	E1000_WRITE_REG ( hw, RDBAL, virt_to_bus ( adapter->rx_base ) );
	E1000_WRITE_REG ( hw, RDBAH, 0 );
	E1000_WRITE_REG ( hw, RDLEN, adapter->rx_ring_size );

	E1000_WRITE_REG ( hw, RDH, 0 );
	if (hw->mac_type == e1000_82576)
		E1000_WRITE_REG ( hw, RDT, 0 );
	else
		E1000_WRITE_REG ( hw, RDT, NUM_RX_DESC - 1 );

	/* This doesn't seem to  be necessary for correct operation,
	 * but it seems as well to be implicit
	 */
	if (hw->mac_type == e1000_82576) {
		rxdctl = E1000_READ_REG ( hw, RXDCTL );
		rxdctl |= E1000_RXDCTL_QUEUE_ENABLE;
		rxdctl &= 0xFFF00000;
		rxdctl |= IGB_RX_PTHRESH;
		rxdctl |= IGB_RX_HTHRESH << 8;
		rxdctl |= IGB_RX_WTHRESH << 16;
		E1000_WRITE_REG ( hw, RXDCTL, rxdctl );
		E1000_WRITE_FLUSH ( hw );

		rxcsum = E1000_READ_REG(hw, RXCSUM);
		rxcsum &= ~( E1000_RXCSUM_TUOFL | E1000_RXCSUM_IPPCSE );
		E1000_WRITE_REG ( hw, RXCSUM, 0 );

		/* The initial value for MRQC disables multiple receive
		 * queues, however this setting is not recommended.
		 * - Intel® 82576 Gigabit Ethernet Controller Datasheet r2.41
	         *   Section 8.10.9 Multiple Queues Command Register - MRQC
		 */
		mrqc = E1000_MRQC_ENABLE_VMDQ;
		E1000_WRITE_REG ( hw, MRQC, mrqc );
	}

	/* Enable Receives */
	rctl |=  E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SZ_2048 |
		 E1000_RCTL_MPE;
	E1000_WRITE_REG ( hw, RCTL, rctl );
	E1000_WRITE_FLUSH ( hw );

	/* On the 82576, RDT([0]) must not be "bumped" before
	 * the enable bit of RXDCTL([0]) is set.
	 * - Intel® 82576 Gigabit Ethernet Controller Datasheet r2.41
	 *   Section 4.5.9 receive Initialization
	 *
	 * By observation I have found to occur when the enable bit of
	 * RCTL is set. The datasheet recommends polling for this bit,
	 * however as I see no evidence of this in the Linux igb driver
	 * I have omitted that step.
	 * - Simon Horman, May 2009
	 */
	if (hw->mac_type == e1000_82576)
		E1000_WRITE_REG ( hw, RDT, NUM_RX_DESC - 1 );

        DBG ( "RDBAL: %#08x\n",  E1000_READ_REG ( hw, RDBAL ) );
        DBG ( "RDLEN: %d\n",     E1000_READ_REG ( hw, RDLEN ) );
        DBG ( "RCTL:  %#08x\n",  E1000_READ_REG ( hw, RCTL ) );
}

/**
 * e1000_reset - Put e1000 NIC in known initial state
 *
 * @v adapter	e1000 private structure
 **/
static void
e1000_reset ( struct e1000_adapter *adapter )
{
	uint32_t pba = 0;
	uint16_t fc_high_water_mark = E1000_FC_HIGH_DIFF;

	DBG ( "e1000_reset\n" );

	switch (adapter->hw.mac_type) {
	case e1000_82542_rev2_0:
	case e1000_82542_rev2_1:
	case e1000_82543:
	case e1000_82544:
	case e1000_82540:
	case e1000_82541:
	case e1000_82541_rev_2:
		pba = E1000_PBA_48K;
		break;
	case e1000_82545:
	case e1000_82545_rev_3:
	case e1000_82546:
	case e1000_82546_rev_3:
		pba = E1000_PBA_48K;
		break;
	case e1000_82547:
	case e1000_82547_rev_2:
		pba = E1000_PBA_30K;
		break;
	case e1000_82571:
	case e1000_82572:
	case e1000_80003es2lan:
		pba = E1000_PBA_38K;
		break;
	case e1000_82573:
		pba = E1000_PBA_20K;
		break;
	case e1000_82576:
		pba = E1000_PBA_64K;
		break;
	case e1000_ich8lan:
		pba = E1000_PBA_8K;
	case e1000_undefined:
	case e1000_num_macs:
		break;
	}

	E1000_WRITE_REG ( &adapter->hw, PBA, pba );
	
	/* flow control settings */
	/* Set the FC high water mark to 90% of the FIFO size.
	 * Required to clear last 3 LSB */
	fc_high_water_mark = ((pba * 9216)/10) & 0xFFF8;

	/* We can't use 90% on small FIFOs because the remainder
	 * would be less than 1 full frame.  In this case, we size
	 * it to allow at least a full frame above the high water
	 *  mark. */
	if (pba < E1000_PBA_16K)
		fc_high_water_mark = (pba * 1024) - 1600;

	/* This actually applies to < e1000_82575, one revision less than
	 * e1000_82576, but e1000_82575 isn't currently defined in the code */
	if (adapter->hw.mac_type < e1000_82576) {
		/* 8-byte granularity */
		adapter->hw.fc_high_water = fc_high_water_mark & 0xFFF8;
		adapter->hw.fc_low_water = adapter->hw.fc_high_water - 8;
	} else {
		/* 16-byte granularity */
		adapter->hw.fc_high_water = fc_high_water_mark & 0xFFF0;
		adapter->hw.fc_low_water = adapter->hw.fc_high_water - 16;
	}

	if (adapter->hw.mac_type == e1000_80003es2lan ||
	    adapter->hw.mac_type == e1000_82576)
		adapter->hw.fc_pause_time = 0xFFFF;
	else
		adapter->hw.fc_pause_time = E1000_FC_PAUSE_TIME;
	adapter->hw.fc_send_xon = 1;
	adapter->hw.fc = adapter->hw.original_fc;
	/* Allow time for pending master requests to run */

	e1000_reset_hw ( &adapter->hw );

	if ( adapter->hw.mac_type >= e1000_82544 )
		E1000_WRITE_REG ( &adapter->hw, WUC, 0 );

	if ( e1000_init_hw ( &adapter->hw ) )
		DBG ( "Hardware Error\n" );

	/* if (adapter->hwflags & HWFLAGS_PHY_PWR_BIT) { */
	if (adapter->hw.mac_type >= e1000_82544 &&
	    adapter->hw.mac_type <= e1000_82547_rev_2 &&
	    adapter->hw.autoneg == 1 &&
	    adapter->hw.autoneg_advertised == ADVERTISE_1000_FULL) {
		uint32_t ctrl = E1000_READ_REG(&adapter->hw, CTRL);
		/* clear phy power management bit if we are in gig only mode,
		 * which if enabled will attempt negotiation to 100Mb, which
		 * can cause a loss of link at power off or driver unload */
		ctrl &= ~E1000_CTRL_SWDPIN3;
		E1000_WRITE_REG(&adapter->hw, CTRL, ctrl);
	}

	e1000_phy_get_info ( &adapter->hw, &adapter->phy_info );

	if (!adapter->smart_power_down &&
	    (adapter->hw.mac_type == e1000_82571 ||
	     adapter->hw.mac_type == e1000_82572)) {
		uint16_t phy_data = 0;
		/* speed up time to link by disabling smart power down, ignore
		 * the return value of this function because there is nothing
		 * different we would do if it failed */
		e1000_read_phy_reg(&adapter->hw, IGP02E1000_PHY_POWER_MGMT,
		                   &phy_data);
		phy_data &= ~IGP02E1000_PM_SPD;
		e1000_write_phy_reg(&adapter->hw, IGP02E1000_PHY_POWER_MGMT,
		                    phy_data);
	}
}

/** Functions that implement the gPXE driver API **/

/**
 * e1000_close - Disables a network interface
 *
 * @v netdev	network interface device structure
 *
 **/
static void
e1000_close ( struct net_device *netdev )
{
	struct e1000_adapter *adapter = netdev_priv ( netdev );
	struct e1000_hw *hw = &adapter->hw;
	uint32_t rctl;
	uint32_t icr;

	DBG ( "e1000_close\n" );
	
	/* Acknowledge interrupts */
	icr = E1000_READ_REG ( hw, ICR );

	e1000_irq_disable ( adapter );

	/* disable receives */
	rctl = E1000_READ_REG ( hw, RCTL );
	E1000_WRITE_REG ( hw, RCTL, rctl & ~E1000_RCTL_EN );
	E1000_WRITE_FLUSH ( hw );

	e1000_reset_hw ( hw );

	e1000_free_tx_resources ( adapter );
	e1000_free_rx_resources ( adapter );
}

/** 
 * e1000_transmit - Transmit a packet
 *
 * @v netdev	Network device
 * @v iobuf	I/O buffer
 *
 * @ret rc       Returns 0 on success, negative on failure
 */
static int
e1000_transmit ( struct net_device *netdev, struct io_buffer *iobuf )
{
	struct e1000_adapter *adapter = netdev_priv( netdev );
	struct e1000_hw *hw = &adapter->hw;
	uint32_t tx_curr = adapter->tx_tail;
	struct e1000_tx_desc *tx_curr_desc;

	DBG ("e1000_transmit\n");
	
	if ( adapter->tx_fill_ctr == NUM_TX_DESC ) {
		DBG ("TX overflow\n");
		return -ENOBUFS;
	}

	/* Save pointer to iobuf we have been given to transmit,
	   netdev_tx_complete() will need it later
	 */
	adapter->tx_iobuf[tx_curr] = iobuf;

	tx_curr_desc = ( void * ) ( adapter->tx_base ) + 
		       ( tx_curr * sizeof ( *adapter->tx_base ) ); 

	DBG ( "tx_curr_desc = %#08lx\n", virt_to_bus ( tx_curr_desc ) );
	DBG ( "tx_curr_desc + 16 = %#08lx\n", virt_to_bus ( tx_curr_desc ) + 16 );
	DBG ( "iobuf->data = %#08lx\n", virt_to_bus ( iobuf->data ) );

	/* Add the packet to TX ring
	 */
 	tx_curr_desc->buffer_addr = 
		virt_to_bus ( iobuf->data );
	tx_curr_desc->lower.data = 
		E1000_TXD_CMD_RPS  | E1000_TXD_CMD_EOP |
		E1000_TXD_CMD_IFCS | iob_len ( iobuf );
	tx_curr_desc->upper.data = 0;
	
	DBG ( "TX fill: %d tx_curr: %d addr: %#08lx len: %zd\n", adapter->tx_fill_ctr, 
	      tx_curr, virt_to_bus ( iobuf->data ), iob_len ( iobuf ) );
	      
	/* Point to next free descriptor */
	adapter->tx_tail = ( adapter->tx_tail + 1 ) % NUM_TX_DESC;
	adapter->tx_fill_ctr++;

	/* Write new tail to NIC, making packet available for transmit
	 */
	wmb();
	E1000_WRITE_REG ( hw, TDT, adapter->tx_tail );

	return 0;
}

/** 
 * e1000_poll - Poll for received packets
 *
 * @v netdev	Network device
 */
static void
e1000_poll ( struct net_device *netdev )
{
	struct e1000_adapter *adapter = netdev_priv( netdev );
	struct e1000_hw *hw = &adapter->hw;

	uint32_t icr;
	uint32_t tx_status;
	uint32_t rx_status;
	uint32_t rx_len;
	uint32_t rx_err;
	struct e1000_tx_desc *tx_curr_desc;
	struct e1000_rx_desc *rx_curr_desc;
	uint32_t i;

	DBGP ( "e1000_poll\n" );

	/* Acknowledge interrupts */
	icr = E1000_READ_REG ( hw, ICR );
	if ( ! icr )
		return;
		
        DBG ( "e1000_poll: intr_status = %#08x\n", icr );

	/* Check status of transmitted packets
	 */
	while ( ( i = adapter->tx_head ) != adapter->tx_tail ) {
			
		tx_curr_desc = ( void * )  ( adapter->tx_base ) + 
					   ( i * sizeof ( *adapter->tx_base ) ); 
					    		
		tx_status = tx_curr_desc->upper.data;

		/* if the packet at tx_head is not owned by hardware it is for us */
		if ( ! ( tx_status & E1000_TXD_STAT_DD ) )
			break;
		
		DBG ( "Sent packet. tx_head: %d tx_tail: %d tx_status: %#08x\n",
	    	      adapter->tx_head, adapter->tx_tail, tx_status );

		if ( tx_status & ( E1000_TXD_STAT_EC | E1000_TXD_STAT_LC | 
				   E1000_TXD_STAT_TU ) ) {
			netdev_tx_complete_err ( netdev, adapter->tx_iobuf[i], -EINVAL );
			DBG ( "Error transmitting packet, tx_status: %#08x\n",
			      tx_status );
		} else {
			netdev_tx_complete ( netdev, adapter->tx_iobuf[i] );
			DBG ( "Success transmitting packet, tx_status: %#08x\n",
			      tx_status );
		}

		/* Decrement count of used descriptors, clear this descriptor 
		 */
		adapter->tx_fill_ctr--;
		memset ( tx_curr_desc, 0, sizeof ( *tx_curr_desc ) );
		
		adapter->tx_head = ( adapter->tx_head + 1 ) % NUM_TX_DESC;		
	}
	
	/* Process received packets 
	 */
	while ( 1 ) {
	
		i = adapter->rx_curr;
		
		rx_curr_desc = ( void * )  ( adapter->rx_base ) + 
			          ( i * sizeof ( *adapter->rx_base ) ); 
		rx_status = rx_curr_desc->status;
		
		DBG2 ( "Before DD Check RX_status: %#08x\n", rx_status );
	
		if ( ! ( rx_status & E1000_RXD_STAT_DD ) )
			break;

		if ( adapter->rx_iobuf[i] == NULL )
			break;

		DBG ( "RCTL = %#08x\n", E1000_READ_REG ( &adapter->hw, RCTL ) );
	
		rx_len = rx_curr_desc->length;

                DBG ( "Received packet, rx_curr: %d  rx_status: %#08x  rx_len: %d\n",
                      i, rx_status, rx_len );

                rx_err = rx_curr_desc->errors;

		iob_put ( adapter->rx_iobuf[i], rx_len );

		if ( rx_err & E1000_RXD_ERR_FRAME_ERR_MASK ) {
		
			netdev_rx_err ( netdev, adapter->rx_iobuf[i], -EINVAL );
			DBG ( "e1000_poll: Corrupted packet received!"
			      " rx_err: %#08x\n", rx_err );
		} else 	{
			/* Add this packet to the receive queue. */
			netdev_rx ( netdev, adapter->rx_iobuf[i] );
		}
		adapter->rx_iobuf[i] = NULL;

		memset ( rx_curr_desc, 0, sizeof ( *rx_curr_desc ) );

		adapter->rx_curr = ( adapter->rx_curr + 1 ) % NUM_RX_DESC;
	}
	e1000_refill_rx_ring(adapter);
}

/**
 * e1000_irq - enable or Disable interrupts
 *
 * @v adapter   e1000 adapter
 * @v action    requested interrupt action
 **/
static void 
e1000_irq ( struct net_device *netdev, int enable )
{
	struct e1000_adapter *adapter = netdev_priv(netdev);

	DBG ( "e1000_irq\n" );

	if ( enable )
		e1000_irq_enable ( adapter );
	else
		e1000_irq_disable ( adapter );
}

static struct net_device_operations e1000_operations;

/**
 * e1000_probe - Initial configuration of e1000 NIC
 *
 * @v pci	PCI device
 * @v id	PCI IDs
 *
 * @ret rc	Return status code
 **/
static int 
e1000_probe ( struct pci_device *pdev,
	      const struct pci_device_id *id __unused )
{
	int i, err;
	struct net_device *netdev;
	struct e1000_adapter *adapter;
	unsigned long mmio_start, mmio_len;
	unsigned long flash_start, flash_len;

	DBG ( "e1000_probe\n" );
	
	err = -ENOMEM;

	/* Allocate net device ( also allocates memory for netdev->priv
	   and makes netdev-priv point to it ) */
	netdev = alloc_etherdev ( sizeof ( struct e1000_adapter ) );
	if ( ! netdev ) 
		goto err_alloc_etherdev;
		
	/* Associate e1000-specific network operations operations with
	 * generic network device layer */
	netdev_init ( netdev, &e1000_operations );
	
	/* Associate this network device with given PCI device */
	pci_set_drvdata ( pdev, netdev );
	netdev->dev = &pdev->dev;
	
	/* Initialize driver private storage */		
	adapter = netdev_priv ( netdev );
        memset ( adapter, 0, ( sizeof ( *adapter ) ) );
	
        adapter->hw.io_base = pdev->ioaddr;
	adapter->ioaddr     = pdev->ioaddr;
        adapter->irqno      = pdev->irq;
	adapter->netdev     = netdev;
	adapter->pdev       = pdev;
	adapter->hw.back    = adapter;

	adapter->tx_ring_size = sizeof ( *adapter->tx_base ) * NUM_TX_DESC;
	adapter->rx_ring_size = sizeof ( *adapter->rx_base ) * NUM_RX_DESC;

	mmio_start = pci_bar_start ( pdev, PCI_BASE_ADDRESS_0 );
	mmio_len   = pci_bar_size  ( pdev, PCI_BASE_ADDRESS_0 );

	DBG ( "mmio_start: %#08lx\n", mmio_start );
	DBG ( "mmio_len: %#08lx\n", mmio_len );
	
	/* Fix up PCI device */
	adjust_pci_device ( pdev );

	err = -EIO;

	adapter->hw.hw_addr = ioremap ( mmio_start, mmio_len );
	DBG ( "adapter->hw.hw_addr: %p\n", adapter->hw.hw_addr );
	
	if ( ! adapter->hw.hw_addr )
		goto err_ioremap;

	/* setup the private structure */
	if ( ( err = e1000_sw_init ( adapter ) ) )
		goto err_sw_init;

	DBG ( "adapter->hw.mac_type: %#08x\n", adapter->hw.mac_type );

	/* Flash BAR mapping must happen after e1000_sw_init
	 * because it depends on mac_type 
	 */
	if ( ( adapter->hw.mac_type == e1000_ich8lan ) && ( pdev->ioaddr ) ) {
		flash_start = pci_bar_start ( pdev, PCI_BASE_ADDRESS_1 );
		flash_len = pci_bar_size ( pdev, PCI_BASE_ADDRESS_1 );
		adapter->hw.flash_address = ioremap ( flash_start, flash_len );
		if ( ! adapter->hw.flash_address )
			goto err_flashmap;
	}

	/* initialize eeprom parameters */
	if ( e1000_init_eeprom_params ( &adapter->hw ) ) {
		DBG ( "EEPROM initialization failed\n" );
		goto err_eeprom;
	}

	/* before reading the EEPROM, reset the controller to
	 * put the device in a known good starting state 
	 */
	err = e1000_reset_hw ( &adapter->hw );
	if ( err < 0 ) {
		DBG ( "Hardware Initialization Failed\n" );
		goto err_reset;
	}

	/* make sure the EEPROM is good */
	if ( e1000_validate_eeprom_checksum( &adapter->hw ) < 0 ) {
		DBG ( "The EEPROM Checksum Is Not Valid\n" );
		goto err_eeprom;
	}

	/* copy the MAC address out of the EEPROM */
	if ( e1000_read_mac_addr ( &adapter->hw ) )
		DBG ( "EEPROM Read Error\n" );

        memcpy ( netdev->hw_addr, adapter->hw.mac_addr, ETH_ALEN );

	/* print bus type/speed/width info */
	{
	struct e1000_hw *hw = &adapter->hw;
	DBG ( "(PCI%s:%s:%s) ",
	      ((hw->bus_type == e1000_bus_type_pcix) ? "-X" :
	       (hw->bus_type == e1000_bus_type_pci_express ? " Express":"")),
	      ((hw->bus_speed == e1000_bus_speed_2500) ? "2.5Gb/s" :
	       (hw->bus_speed == e1000_bus_speed_133) ? "133MHz" :
	       (hw->bus_speed == e1000_bus_speed_120) ? "120MHz" :
	       (hw->bus_speed == e1000_bus_speed_100) ? "100MHz" :
	       (hw->bus_speed == e1000_bus_speed_66) ? "66MHz" : "33MHz"),
  	      ((hw->bus_width == e1000_bus_width_64) ? "64-bit" :
  	       (hw->bus_width == e1000_bus_width_pciex_4) ? "Width x4" :
	       (hw->bus_width == e1000_bus_width_pciex_1) ? "Width x1" :
	       "32-bit"));
	}
	for (i = 0; i < 6; i++)
		DBG ("%02x%s", netdev->ll_addr[i], i == 5 ? "\n" : ":");
        
	/* reset the hardware with the new settings */
	e1000_reset ( adapter );
	
	e1000_get_hw_control ( adapter );

	/* Mark as link up; we don't yet handle link state */
	netdev_link_up ( netdev );

	if ( ( err = register_netdev ( netdev ) ) != 0)
		goto err_register;
		
	DBG ( "e1000_probe succeeded!\n" );	

	/* No errors, return success */
	return 0;

/* Error return paths */
err_reset:
err_register:
err_eeprom:
	if ( ! e1000_check_phy_reset_block ( &adapter->hw ) )
		e1000_phy_hw_reset ( &adapter->hw );
	if ( adapter->hw.flash_address )
		iounmap ( adapter->hw.flash_address );
err_flashmap:
err_sw_init:
	iounmap ( adapter->hw.hw_addr );
err_ioremap:
	netdev_put ( netdev );
err_alloc_etherdev:
	return err;
}

/**
 * e1000_remove - Device Removal Routine
 *
 * @v pdev PCI device information struct
 *
 **/
static void
e1000_remove ( struct pci_device *pdev )
{
	struct net_device *netdev = pci_get_drvdata ( pdev );
	struct e1000_adapter *adapter = netdev_priv ( netdev );
	
	DBG ( "e1000_remove\n" );

	if ( adapter->hw.flash_address )
		iounmap ( adapter->hw.flash_address );
	if  ( adapter->hw.hw_addr )
		iounmap ( adapter->hw.hw_addr );

	unregister_netdev ( netdev );
	e1000_reset_hw ( &adapter->hw );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/**
 * e1000_open - Called when a network interface is made active
 *
 * @v netdev	network interface device structure
 * @ret rc	Return status code, 0 on success, negative value on failure
 *
 **/
static int
e1000_open ( struct net_device *netdev )
{
	struct e1000_adapter *adapter = netdev_priv(netdev);
	int err;
	
	DBG ( "e1000_open\n" );	

	/* allocate transmit descriptors */
	err = e1000_setup_tx_resources ( adapter );
	if ( err ) {
		DBG ( "Error setting up TX resources!\n" );
		goto err_setup_tx;
	}

	/* allocate receive descriptors */
	err = e1000_setup_rx_resources ( adapter );
	if ( err ) {
		DBG ( "Error setting up RX resources!\n" );
		goto err_setup_rx;
	}

	e1000_configure_tx ( adapter );

	e1000_configure_rx ( adapter );
	
        DBG ( "RXDCTL: %#08x\n",  E1000_READ_REG ( &adapter->hw, RXDCTL ) );

	return 0;

err_setup_rx:
	e1000_free_tx_resources ( adapter );
err_setup_tx:
	e1000_reset ( adapter );

	return err;
}

/** e1000 net device operations */
static struct net_device_operations e1000_operations = {
        .open           = e1000_open,
        .close          = e1000_close,
        .transmit       = e1000_transmit,
        .poll           = e1000_poll,
        .irq            = e1000_irq,
};

int32_t
e1000_read_pcie_cap_reg(struct e1000_hw *hw, uint32_t reg, uint16_t *value)
{
    struct e1000_adapter *adapter = hw->back;
    uint16_t cap_offset;

#define  PCI_CAP_ID_EXP        0x10    /* PCI Express */
    cap_offset = pci_find_capability(adapter->pdev, PCI_CAP_ID_EXP);
    if (!cap_offset)
        return -E1000_ERR_CONFIG;

    pci_read_config_word(adapter->pdev, cap_offset + reg, value);

    return 0;
}

void
e1000_pci_clear_mwi ( struct e1000_hw *hw )
{
	struct e1000_adapter *adapter = hw->back;

	pci_write_config_word ( adapter->pdev, PCI_COMMAND,
			        hw->pci_cmd_word & ~PCI_COMMAND_INVALIDATE );
}

void
e1000_pci_set_mwi ( struct e1000_hw *hw )
{
	struct e1000_adapter *adapter = hw->back;

	pci_write_config_word ( adapter->pdev, PCI_COMMAND, hw->pci_cmd_word );
}

void
e1000_read_pci_cfg ( struct e1000_hw *hw, uint32_t reg, uint16_t *value )
{
	struct e1000_adapter *adapter = hw->back;

	pci_read_config_word ( adapter->pdev, reg, value );
}

void
e1000_write_pci_cfg ( struct e1000_hw *hw, uint32_t reg, uint16_t *value )
{
	struct e1000_adapter *adapter = hw->back;

	pci_write_config_word ( adapter->pdev, reg, *value );
}

void
e1000_io_write ( struct e1000_hw *hw  __unused, unsigned long port, uint32_t value )
{
	outl ( value, port );
}

static struct pci_device_id e1000_nics[] = {
	PCI_ROM(0x8086, 0x1000, "e1000-0x1000", "e1000-0x1000", 0),
	PCI_ROM(0x8086, 0x1001, "e1000-0x1001", "e1000-0x1001", 0),
	PCI_ROM(0x8086, 0x1004, "e1000-0x1004", "e1000-0x1004", 0),
	PCI_ROM(0x8086, 0x1008, "e1000-0x1008", "e1000-0x1008", 0),
	PCI_ROM(0x8086, 0x1009, "e1000-0x1009", "e1000-0x1009", 0),
	PCI_ROM(0x8086, 0x100c, "e1000-0x100c", "e1000-0x100c", 0),
	PCI_ROM(0x8086, 0x100d, "e1000-0x100d", "e1000-0x100d", 0),
	PCI_ROM(0x8086, 0x100e, "e1000-0x100e", "e1000-0x100e", 0),
	PCI_ROM(0x8086, 0x100f, "e1000-0x100f", "e1000-0x100f", 0),
	PCI_ROM(0x8086, 0x1010, "e1000-0x1010", "e1000-0x1010", 0),
	PCI_ROM(0x8086, 0x1011, "e1000-0x1011", "e1000-0x1011", 0),
	PCI_ROM(0x8086, 0x1012, "e1000-0x1012", "e1000-0x1012", 0),
	PCI_ROM(0x8086, 0x1013, "e1000-0x1013", "e1000-0x1013", 0),
	PCI_ROM(0x8086, 0x1014, "e1000-0x1014", "e1000-0x1014", 0),
	PCI_ROM(0x8086, 0x1015, "e1000-0x1015", "e1000-0x1015", 0),
	PCI_ROM(0x8086, 0x1016, "e1000-0x1016", "e1000-0x1016", 0),
	PCI_ROM(0x8086, 0x1017, "e1000-0x1017", "e1000-0x1017", 0),
	PCI_ROM(0x8086, 0x1018, "e1000-0x1018", "e1000-0x1018", 0),
	PCI_ROM(0x8086, 0x1019, "e1000-0x1019", "e1000-0x1019", 0),
	PCI_ROM(0x8086, 0x101a, "e1000-0x101a", "e1000-0x101a", 0),
	PCI_ROM(0x8086, 0x101d, "e1000-0x101d", "e1000-0x101d", 0),
	PCI_ROM(0x8086, 0x101e, "e1000-0x101e", "e1000-0x101e", 0),
	PCI_ROM(0x8086, 0x1026, "e1000-0x1026", "e1000-0x1026", 0),
	PCI_ROM(0x8086, 0x1027, "e1000-0x1027", "e1000-0x1027", 0),
	PCI_ROM(0x8086, 0x1028, "e1000-0x1028", "e1000-0x1028", 0),
	PCI_ROM(0x8086, 0x1049, "e1000-0x1049", "e1000-0x1049", 0),
	PCI_ROM(0x8086, 0x104a, "e1000-0x104a", "e1000-0x104a", 0),
	PCI_ROM(0x8086, 0x104b, "e1000-0x104b", "e1000-0x104b", 0),
	PCI_ROM(0x8086, 0x104c, "e1000-0x104c", "e1000-0x104c", 0),
	PCI_ROM(0x8086, 0x104d, "e1000-0x104d", "e1000-0x104d", 0),
	PCI_ROM(0x8086, 0x105e, "e1000-0x105e", "e1000-0x105e", 0),
	PCI_ROM(0x8086, 0x105f, "e1000-0x105f", "e1000-0x105f", 0),
	PCI_ROM(0x8086, 0x1060, "e1000-0x1060", "e1000-0x1060", 0),
	PCI_ROM(0x8086, 0x1075, "e1000-0x1075", "e1000-0x1075", 0),
	PCI_ROM(0x8086, 0x1076, "e1000-0x1076", "e1000-0x1076", 0),
	PCI_ROM(0x8086, 0x1077, "e1000-0x1077", "e1000-0x1077", 0),
	PCI_ROM(0x8086, 0x1078, "e1000-0x1078", "e1000-0x1078", 0),
	PCI_ROM(0x8086, 0x1079, "e1000-0x1079", "e1000-0x1079", 0),
	PCI_ROM(0x8086, 0x107a, "e1000-0x107a", "e1000-0x107a", 0),
	PCI_ROM(0x8086, 0x107b, "e1000-0x107b", "e1000-0x107b", 0),
	PCI_ROM(0x8086, 0x107c, "e1000-0x107c", "e1000-0x107c", 0),
	PCI_ROM(0x8086, 0x107d, "e1000-0x107d", "e1000-0x107d", 0),
	PCI_ROM(0x8086, 0x107e, "e1000-0x107e", "e1000-0x107e", 0),
	PCI_ROM(0x8086, 0x107f, "e1000-0x107f", "e1000-0x107f", 0),
	PCI_ROM(0x8086, 0x108a, "e1000-0x108a", "e1000-0x108a", 0),
	PCI_ROM(0x8086, 0x108b, "e1000-0x108b", "e1000-0x108b", 0),
	PCI_ROM(0x8086, 0x108c, "e1000-0x108c", "e1000-0x108c", 0),
	PCI_ROM(0x8086, 0x1096, "e1000-0x1096", "e1000-0x1096", 0),
	PCI_ROM(0x8086, 0x1098, "e1000-0x1098", "e1000-0x1098", 0),
	PCI_ROM(0x8086, 0x1099, "e1000-0x1099", "e1000-0x1099", 0),
	PCI_ROM(0x8086, 0x109a, "e1000-0x109a", "e1000-0x109a", 0),
	PCI_ROM(0x8086, 0x10a4, "e1000-0x10a4", "e1000-0x10a4", 0),
	PCI_ROM(0x8086, 0x10a5, "e1000-0x10a5", "e1000-0x10a5", 0),
	PCI_ROM(0x8086, 0x10b5, "e1000-0x10b5", "e1000-0x10b5", 0),
	PCI_ROM(0x8086, 0x10b9, "e1000-0x10b9", "e1000-0x10b9", 0),
	PCI_ROM(0x8086, 0x10ba, "e1000-0x10ba", "e1000-0x10ba", 0),
	PCI_ROM(0x8086, 0x10bb, "e1000-0x10bb", "e1000-0x10bb", 0),
	PCI_ROM(0x8086, 0x10bc, "e1000-0x10bc", "e1000-0x10bc", 0),
	PCI_ROM(0x8086, 0x10c4, "e1000-0x10c4", "e1000-0x10c4", 0),
	PCI_ROM(0x8086, 0x10c5, "e1000-0x10c5", "e1000-0x10c5", 0),
	PCI_ROM(0x8086, 0x10c9, "e1000-0x10c9", "e1000-0x10c9", 0),
	PCI_ROM(0x8086, 0x10d9, "e1000-0x10d9", "e1000-0x10d9", 0),
	PCI_ROM(0x8086, 0x10da, "e1000-0x10da", "e1000-0x10da", 0),
};

struct pci_driver e1000_driver __pci_driver = {
	.ids = e1000_nics,
	.id_count = (sizeof (e1000_nics) / sizeof (e1000_nics[0])),
	.probe = e1000_probe,
	.remove = e1000_remove,
};

/*
 * Local variables:
 *  c-basic-offset: 8
 *  c-indent-level: 8
 *  tab-width: 8
 * End:
 */
