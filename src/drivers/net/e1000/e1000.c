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

#include "e1000.h"

static struct pci_device_id e1000_nics[] = {
	PCI_ROM(0x8086, 0x1000, "e1000-0x1000", "E1000-0x1000"),
	PCI_ROM(0x8086, 0x1001, "e1000-0x1001", "E1000-0x1001"),
	PCI_ROM(0x8086, 0x1004, "e1000-0x1004", "E1000-0x1004"),
	PCI_ROM(0x8086, 0x1008, "e1000-0x1008", "E1000-0x1008"),
	PCI_ROM(0x8086, 0x1009, "e1000-0x1009", "E1000-0x1009"),
	PCI_ROM(0x8086, 0x100C, "e1000-0x100C", "E1000-0x100C"),
	PCI_ROM(0x8086, 0x100D, "e1000-0x100D", "E1000-0x100D"),
	PCI_ROM(0x8086, 0x100E, "e1000-0x100E", "E1000-0x100E"),
	PCI_ROM(0x8086, 0x100F, "e1000-0x100F", "E1000-0x100F"),
	PCI_ROM(0x8086, 0x1010, "e1000-0x1010", "E1000-0x1010"),
	PCI_ROM(0x8086, 0x1011, "e1000-0x1011", "E1000-0x1011"),
	PCI_ROM(0x8086, 0x1012, "e1000-0x1012", "E1000-0x1012"),
	PCI_ROM(0x8086, 0x1013, "e1000-0x1013", "E1000-0x1013"),
	PCI_ROM(0x8086, 0x1014, "e1000-0x1014", "E1000-0x1014"),
	PCI_ROM(0x8086, 0x1015, "e1000-0x1015", "E1000-0x1015"),
	PCI_ROM(0x8086, 0x1016, "e1000-0x1016", "E1000-0x1016"),
	PCI_ROM(0x8086, 0x1017, "e1000-0x1017", "E1000-0x1017"),
	PCI_ROM(0x8086, 0x1018, "e1000-0x1018", "E1000-0x1018"),
	PCI_ROM(0x8086, 0x1019, "e1000-0x1019", "E1000-0x1019"),
	PCI_ROM(0x8086, 0x101A, "e1000-0x101A", "E1000-0x101A"),
	PCI_ROM(0x8086, 0x101D, "e1000-0x101D", "E1000-0x101D"),
	PCI_ROM(0x8086, 0x101E, "e1000-0x101E", "E1000-0x101E"),
	PCI_ROM(0x8086, 0x1026, "e1000-0x1026", "E1000-0x1026"),
	PCI_ROM(0x8086, 0x1027, "e1000-0x1027", "E1000-0x1027"),
	PCI_ROM(0x8086, 0x1028, "e1000-0x1028", "E1000-0x1028"),
	PCI_ROM(0x8086, 0x1049, "e1000-0x1049", "E1000-0x1049"),
	PCI_ROM(0x8086, 0x104A, "e1000-0x104A", "E1000-0x104A"),
	PCI_ROM(0x8086, 0x104B, "e1000-0x104B", "E1000-0x104B"),
	PCI_ROM(0x8086, 0x104C, "e1000-0x104C", "E1000-0x104C"),
	PCI_ROM(0x8086, 0x104D, "e1000-0x104D", "E1000-0x104D"),
	PCI_ROM(0x8086, 0x105E, "e1000-0x105E", "E1000-0x105E"),
	PCI_ROM(0x8086, 0x105F, "e1000-0x105F", "E1000-0x105F"),
	PCI_ROM(0x8086, 0x1060, "e1000-0x1060", "E1000-0x1060"),
	PCI_ROM(0x8086, 0x1075, "e1000-0x1075", "E1000-0x1075"),
	PCI_ROM(0x8086, 0x1076, "e1000-0x1076", "E1000-0x1076"),
	PCI_ROM(0x8086, 0x1077, "e1000-0x1077", "E1000-0x1077"),
	PCI_ROM(0x8086, 0x1078, "e1000-0x1078", "E1000-0x1078"),
	PCI_ROM(0x8086, 0x1079, "e1000-0x1079", "E1000-0x1079"),
	PCI_ROM(0x8086, 0x107A, "e1000-0x107A", "E1000-0x107A"),
	PCI_ROM(0x8086, 0x107B, "e1000-0x107B", "E1000-0x107B"),
	PCI_ROM(0x8086, 0x107C, "e1000-0x107C", "E1000-0x107C"),
	PCI_ROM(0x8086, 0x107D, "e1000-0x107D", "E1000-0x107D"),
	PCI_ROM(0x8086, 0x107E, "e1000-0x107E", "E1000-0x107E"),
	PCI_ROM(0x8086, 0x107F, "e1000-0x107F", "E1000-0x107F"),
	PCI_ROM(0x8086, 0x108A, "e1000-0x108A", "E1000-0x108A"),
	PCI_ROM(0x8086, 0x108B, "e1000-0x108B", "E1000-0x108B"),
	PCI_ROM(0x8086, 0x108C, "e1000-0x108C", "E1000-0x108C"),
	PCI_ROM(0x8086, 0x1096, "e1000-0x1096", "E1000-0x1096"),
	PCI_ROM(0x8086, 0x1098, "e1000-0x1098", "E1000-0x1098"),
	PCI_ROM(0x8086, 0x1099, "e1000-0x1099", "E1000-0x1099"),
	PCI_ROM(0x8086, 0x109A, "e1000-0x109A", "E1000-0x109A"),
	PCI_ROM(0x8086, 0x10A4, "e1000-0x10A4", "E1000-0x10A4"),
	PCI_ROM(0x8086, 0x10A5, "e1000-0x10A5", "E1000-0x10A5"),
	PCI_ROM(0x8086, 0x10B5, "e1000-0x10B5", "E1000-0x10B5"),
	PCI_ROM(0x8086, 0x10B9, "e1000-0x10B9", "E1000-0x10B9"),
	PCI_ROM(0x8086, 0x10BA, "e1000-0x10BA", "E1000-0x10BA"),
	PCI_ROM(0x8086, 0x10BB, "e1000-0x10BB", "E1000-0x10BB"),
	PCI_ROM(0x8086, 0x10BC, "e1000-0x10BC", "E1000-0x10BC"),
	PCI_ROM(0x8086, 0x10C4, "e1000-0x10C4", "E1000-0x10C4"),
	PCI_ROM(0x8086, 0x10C5, "e1000-0x10C5", "E1000-0x10C5"),
	PCI_ROM(0x8086, 0x10D9, "e1000-0x10D9", "E1000-0x10D9"),
	PCI_ROM(0x8086, 0x10DA, "e1000-0x10DA", "E1000-0x10DA"),
};

static void
e1000_init_manageability ( struct e1000_adapter *adapter )
{
	DBG ( "e1000_init_manageability\n" );

	if (adapter->en_mng_pt) {
		uint32_t manc = E1000_READ_REG(&adapter->hw, MANC);

		/* disable hardware interception of ARP */
		manc &= ~(E1000_MANC_ARP_EN);

		/* enable receiving management packets to the host */
		/* this will probably generate destination unreachable messages
		 * from the host OS, but the packets will be handled on SMBUS */
		if (adapter->hw.has_manc2h) {
			uint32_t manc2h = E1000_READ_REG(&adapter->hw, MANC2H);

			manc |= E1000_MANC_EN_MNG2HOST;
			manc2h |= E1000_MNG2HOST_PORT_623;
			manc2h |= E1000_MNG2HOST_PORT_664;
			E1000_WRITE_REG(&adapter->hw, MANC2H, manc2h);
		}

		E1000_WRITE_REG(&adapter->hw, MANC, manc);
	}
}

/**
 * e1000_power_up_phy - restore link in case the phy was powered down
 * @adapter: address of board private structure
 *
 * The phy may be powered down to save power and turn off link when the
 * driver is unloaded and wake on lan is not enabled (among others)
 * *** this routine MUST be followed by a call to e1000_reset ***
 *
 **/
static void
e1000_power_up_phy ( struct e1000_adapter *adapter )
{
	DBG ( "e1000_power_up_phy\n" );

	uint16_t mii_reg = 0;

	/* Just clear the power down bit to wake the phy back up */
	if (adapter->hw.media_type == e1000_media_type_copper) {
		/* according to the manual, the phy will retain its
		 * settings across a power-down/up cycle */
		e1000_read_phy_reg(&adapter->hw, PHY_CTRL, &mii_reg);
		mii_reg &= ~MII_CR_POWER_DOWN;
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL, mii_reg);
	}
}

static void
e1000_power_down_phy ( struct e1000_adapter *adapter )
{
	DBG ( "e1000_power_down_phy\n" );
	
	/* Power down the PHY so no link is implied when interface is down *
	 * The PHY cannot be powered down if any of the following is TRUE *
	 * (a) WoL is enabled
	 * (b) AMT is active
	 * (c) SoL/IDER session is active */
	if (!adapter->wol && adapter->hw.mac_type >= e1000_82540 &&
	   adapter->hw.media_type == e1000_media_type_copper) {
		uint16_t mii_reg = 0;

		switch (adapter->hw.mac_type) {
		case e1000_82540:
		case e1000_82545:
		case e1000_82545_rev_3:
		case e1000_82546:
		case e1000_82546_rev_3:
		case e1000_82541:
		case e1000_82541_rev_2:
		case e1000_82547:
		case e1000_82547_rev_2:
			if (E1000_READ_REG(&adapter->hw, MANC) &
			    E1000_MANC_SMBUS_EN)
				goto out;
			break;
		case e1000_82571:
		case e1000_82572:
		case e1000_82573:
		case e1000_80003es2lan:
		case e1000_ich8lan:
			if (e1000_check_mng_mode(&adapter->hw) ||
			    e1000_check_phy_reset_block(&adapter->hw))
				goto out;
			break;
		default:
			goto out;
		}
		e1000_read_phy_reg(&adapter->hw, PHY_CTRL, &mii_reg);
		mii_reg |= MII_CR_POWER_DOWN;
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL, mii_reg);
		mdelay(1);
	}
out:
	return;
}

/**
 * e1000_sw_init - Initialize general software structures (struct e1000_adapter)
 * @adapter: board private structure to initialize
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
	
	DBG ( "e1000_sw_init\n" );

	/* PCI config space info */
	hw->vendor_id = pdev->vendor;
	hw->device_id = pdev->device;

	pci_read_config_word(pdev, PCI_COMMAND, &hw->pci_cmd_word);

	adapter->rx_buffer_len = 2048;

	/* identify the MAC */

	if ( e1000_set_mac_type ( hw ) ) {
		DBG ( "Unknown MAC Type\n" );
		return -EINVAL;
	}

	switch (hw->mac_type) {
	default:
		break;
	case e1000_82541:
	case e1000_82547:
	case e1000_82541_rev_2:
	case e1000_82547_rev_2:
		hw->phy_init_script = 1;
		break;
	}

	e1000_set_media_type(hw);

	hw->wait_autoneg_complete = TRUE;
	hw->tbi_compatibility_en = TRUE;
	hw->adaptive_ifs = TRUE;

	/* Copper options */

	if (hw->media_type == e1000_media_type_copper) {
		hw->mdix = AUTO_ALL_MODES;
		hw->disable_polarity_correction = FALSE;
		hw->master_slave = E1000_MASTER_SLAVE;
	}

	adapter->num_tx_queues = 1;
	adapter->num_rx_queues = 1;

	return E1000_SUCCESS;
}

/**
 * e1000_setup_tx_resources - allocate Tx resources (Descriptors)
 * @adapter: board private structure
 * @txdr:    tx descriptor ring (for a specific queue) to setup
 *
 * Return 0 on success, negative on failure
 **/
static int
e1000_setup_tx_resources ( struct e1000_adapter *adapter )
{
	int i;
	
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

        adapter->tx_desc_ring = 
        	malloc_dma ( sizeof ( struct e1000_tx_desc ) * NUM_TX_DESC,
        		     sizeof ( struct e1000_tx_desc ) * NUM_TX_DESC );
        		     
       	if ( ! adapter->tx_desc_ring ) {
       		return -ENOMEM;
	}

	memset ( adapter->tx_desc_ring, 0, sizeof ( struct e1000_tx_desc ) *
		 NUM_TX_DESC );

	for ( i = 0; i < NUM_TX_DESC; i++ ) {
		adapter->tx_desc[i] = (void *) adapter->tx_desc_ring + 
			( i * sizeof ( struct e1000_tx_desc ) );
	}

	return 0;
}

static void
e1000_free_tx_resources ( struct e1000_adapter *adapter )
{
	DBG ( "e1000_free_tx_resources\n" );

        free_dma ( adapter->tx_desc_ring, 
                   sizeof ( struct e1000_tx_desc ) * NUM_TX_DESC );
}

/**
 * e1000_setup_rx_resources - allocate Rx resources (Descriptors)
 * @adapter: board private structure
 * @rxdr:    rx descriptor ring (for a specific queue) to setup
 *
 * Returns 0 on success, negative on failure
 **/
static int
e1000_setup_rx_resources ( struct e1000_adapter *adapter )
{
	int i, j;
	
	DBG ( "e1000_setup_rx_resources\n" );
	
	/* Allocate receive descriptor ring memory.
	   It must not cross a 64K boundary because of hardware errata
	 */

        adapter->rx_desc_ring = 
        	malloc_dma ( sizeof ( struct e1000_rx_desc ) * NUM_RX_DESC,
        		     sizeof ( struct e1000_rx_desc ) * NUM_RX_DESC );
        		     
       	if ( ! adapter->rx_desc_ring ) {
       		return -ENOMEM;
	}

	memset ( adapter->rx_desc_ring, 0, sizeof (struct e1000_rx_desc) * NUM_RX_DESC );

	for ( i = 0; i < NUM_RX_DESC; i++ ) {
	
		adapter->rx_iobuf[i] = alloc_iob ( E1000_RXBUFFER_2048 );
		
		/* If unable to allocate all iobufs, free any that
		 * were successfully allocated, and return an error 
		 */
		if ( ! adapter->rx_iobuf[i] ) {
			for ( j = 0; j < i; j++ ) {
				free_iob ( adapter->rx_iobuf[j] );
			}
			return -ENOMEM;
		}
		
		adapter->rx_desc[i] = (void *) adapter->rx_desc_ring + 
			( i * sizeof ( struct e1000_rx_desc ) );
			
		adapter->rx_desc[i]->buffer_addr = virt_to_bus ( adapter->rx_iobuf[i]->data );		
	}	
	return 0;
}

static void
e1000_free_rx_resources ( struct e1000_adapter *adapter )
{
	int i;
	
	DBG ( "e1000_free_rx_resources\n" );

        free_dma ( adapter->rx_desc_ring, 
                   sizeof ( struct e1000_rx_desc ) * NUM_RX_DESC );

	for ( i = 0; i < NUM_RX_DESC; i++ ) {
		free_iob ( adapter->rx_iobuf[i] );
	}
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
	uint32_t tctl, tipg, tarc;
	uint32_t ipgr1, ipgr2;
	
	DBG ( "e1000_configure_tx\n" );

	E1000_WRITE_REG ( hw, TDBAH, 0 );
	E1000_WRITE_REG ( hw, TDBAL, virt_to_bus ( adapter->tx_base ) );
	E1000_WRITE_REG ( hw, TDLEN, sizeof ( struct e1000_tx_desc ) *
			  NUM_TX_DESC );

	/* Setup the HW Tx Head and Tail descriptor pointers */
	E1000_WRITE_REG ( hw, TDH, 0 );
	E1000_WRITE_REG ( hw, TDT, 0 );

	adapter->tx_head = 0;
	adapter->tx_tail = 0;
	adapter->tx_fill_ctr = 0;
	
	/* Set the default values for the Tx Inter Packet Gap timer */
	if (adapter->hw.mac_type <= e1000_82547_rev_2 &&
	    (hw->media_type == e1000_media_type_fiber ||
	     hw->media_type == e1000_media_type_internal_serdes))
		tipg = DEFAULT_82543_TIPG_IPGT_FIBER;
	else
		tipg = DEFAULT_82543_TIPG_IPGT_COPPER;

	switch (hw->mac_type) {
	case e1000_82542_rev2_0:
	case e1000_82542_rev2_1:
		tipg = DEFAULT_82542_TIPG_IPGT;
		ipgr1 = DEFAULT_82542_TIPG_IPGR1;
		ipgr2 = DEFAULT_82542_TIPG_IPGR2;
		break;
	case e1000_80003es2lan:
		ipgr1 = DEFAULT_82543_TIPG_IPGR1;
		ipgr2 = DEFAULT_80003ES2LAN_TIPG_IPGR2;
		break;
	default:
		ipgr1 = DEFAULT_82543_TIPG_IPGR1;
		ipgr2 = DEFAULT_82543_TIPG_IPGR2;
		break;
	}
	tipg |= ipgr1 << E1000_TIPG_IPGR1_SHIFT;
	tipg |= ipgr2 << E1000_TIPG_IPGR2_SHIFT;
	E1000_WRITE_REG ( hw, TIPG, tipg );

	/* Set the Tx Interrupt Delay register */

	E1000_WRITE_REG (hw, TIDV, adapter->tx_int_delay);
	if (hw->mac_type >= e1000_82540)
		E1000_WRITE_REG(hw, TADV, adapter->tx_abs_int_delay);

	/* Program the Transmit Control Register */

	tctl = E1000_READ_REG(hw, TCTL);
	tctl &= ~E1000_TCTL_CT;
	tctl |= E1000_TCTL_PSP | E1000_TCTL_RTLC |
		(E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);

	if (hw->mac_type == e1000_82571 || hw->mac_type == e1000_82572) {
		tarc = E1000_READ_REG(hw, TARC0);
		/* set the speed mode bit, we'll clear it if we're not at
		 * gigabit link later */
		tarc |= (1 << 21);
		E1000_WRITE_REG(hw, TARC0, tarc);
	} else if (hw->mac_type == e1000_80003es2lan) {
		tarc = E1000_READ_REG(hw, TARC0);
		tarc |= 1;
		E1000_WRITE_REG(hw, TARC0, tarc);
		tarc = E1000_READ_REG(hw, TARC1);
		tarc |= 1;
		E1000_WRITE_REG(hw, TARC1, tarc);
	}

	e1000_config_collision_dist(hw);

	/* Setup Transmit Descriptor Settings for eop descriptor */
	adapter->txd_cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS;

	/* only set IDE if we are delaying interrupts using the timers */
	if (adapter->tx_int_delay)
		adapter->txd_cmd |= E1000_TXD_CMD_IDE;

	if (hw->mac_type < e1000_82543)
		adapter->txd_cmd |= E1000_TXD_CMD_RPS;
	else
		adapter->txd_cmd |= E1000_TXD_CMD_RS;

	/* Cache if we're 82544 running in PCI-X because we'll
	 * need this to apply a workaround later in the send path. */
	if (hw->mac_type == e1000_82544 &&
	    hw->bus_type == e1000_bus_type_pcix)
		adapter->pcix_82544 = 1;

	E1000_WRITE_REG ( hw, TCTL, tctl );
}

/**
 * e1000_setup_rctl - configure the receive control registers
 * @adapter: Board private structure
 **/
static void
e1000_setup_rctl ( struct e1000_adapter *adapter )
{
	uint32_t rctl;
	
	DBG ( "e1000_setup_rctl\n" );

	rctl = E1000_READ_REG ( &adapter->hw, RCTL );

	rctl &= ~( 3 << E1000_RCTL_MO_SHIFT );

	rctl |= E1000_RCTL_EN | E1000_RCTL_BAM |
		E1000_RCTL_LBM_NO | E1000_RCTL_RDMTS_HALF |
		( adapter->hw.mc_filter_type << E1000_RCTL_MO_SHIFT );

	if ( adapter->hw.tbi_compatibility_on == 1 )
		rctl |= E1000_RCTL_SBP;
	else
		rctl &= ~E1000_RCTL_SBP;

	rctl &= ~E1000_RCTL_LPE;

	/* Setup buffer sizes */
	rctl |= E1000_RCTL_SZ_2048;
	rctl &= ~E1000_RCTL_BSEX;

	E1000_WRITE_REG ( &adapter->hw, RCTL, rctl );
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
	uint32_t rctl, ctrl_ext;

	DBG ( "e1000_configure_rx\n" );

	/* disable receives while setting up the descriptors */
	rctl = E1000_READ_REG(hw, RCTL);
	E1000_WRITE_REG ( hw, RCTL, rctl & ~E1000_RCTL_EN );

	/* set the Receive Delay Timer Register */
	E1000_WRITE_REG( hw, RDTR, adapter->rx_int_delay );

	if (hw->mac_type >= e1000_82540) {
		E1000_WRITE_REG(hw, RADV, adapter->rx_abs_int_delay);
		if (adapter->itr_setting != 0)
			E1000_WRITE_REG(hw, ITR,
				1000000000 / (adapter->itr * 256));
	}

	if (hw->mac_type >= e1000_82571) {
		ctrl_ext = E1000_READ_REG(hw, CTRL_EXT);
		/* Reset delay timers after every interrupt */
		ctrl_ext |= E1000_CTRL_EXT_INT_TIMER_CLR;
		E1000_WRITE_REG(hw, CTRL_EXT, ctrl_ext);
		E1000_WRITE_FLUSH(hw);
	}

	/* Setup the HW Rx Head and Tail Descriptor Pointers and
	 * the Base and Length of the Rx Descriptor Ring */	 

	adapter->rx_tail = 0;

	E1000_WRITE_REG ( hw, RDBAH, 0 );
	E1000_WRITE_REG ( hw, RDBAL, virt_to_bus ( adapter->rx_base ) );
	E1000_WRITE_REG ( hw, RDLEN, sizeof ( struct e1000_tx_desc ) *
			  NUM_TX_DESC );

	E1000_WRITE_REG ( hw, RDH, 0);
	E1000_WRITE_REG ( hw, RDT, 0);

	/* Enable Receives */

	E1000_WRITE_REG ( hw, RCTL, rctl );
}

/**
 * e1000_configure - configure the hardware for RX and TX
 * @adapter = private board structure
 **/
static void e1000_configure ( struct e1000_adapter *adapter )
{
	DBG ( "e1000_configure\n" );

	e1000_configure_tx ( adapter );

	e1000_setup_rctl ( adapter );

	e1000_configure_rx ( adapter );
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
	case e1000_ich8lan:
		pba = E1000_PBA_8K;
	case e1000_undefined:
	case e1000_num_macs:
		break;
	}

	E1000_WRITE_REG(&adapter->hw, PBA, pba);

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

	adapter->hw.fc_high_water = fc_high_water_mark;
	adapter->hw.fc_low_water = fc_high_water_mark - 8;
	if (adapter->hw.mac_type == e1000_80003es2lan)
		adapter->hw.fc_pause_time = 0xFFFF;
	else
		adapter->hw.fc_pause_time = E1000_FC_PAUSE_TIME;
	adapter->hw.fc_send_xon = 1;
	adapter->hw.fc = adapter->hw.original_fc;

	/* Allow time for pending master requests to run */
	e1000_reset_hw(&adapter->hw);
	if (adapter->hw.mac_type >= e1000_82544)
		E1000_WRITE_REG(&adapter->hw, WUC, 0);

	if (e1000_init_hw(&adapter->hw))
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

	e1000_phy_get_info(&adapter->hw, &adapter->phy_info);

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
	struct e1000_adapter *adapter = netdev_priv( netdev );

	DBG ( "e1000_close\n" );

	e1000_reset ( adapter );
	e1000_power_down_phy ( adapter );

	e1000_free_tx_resources ( adapter );
	e1000_free_rx_resources ( adapter );
}

/** 
 * e1000_transmit - Transmit a packet
 *
 * @v netdev	Network device
 * @v iobuf	I/O buffer
 *
 * @ret rc	Return status code
 */
static int
e1000_transmit ( struct net_device *netdev, struct io_buffer *iobuf )
{
	struct e1000_adapter *adapter = netdev_priv( netdev );
	struct e1000_hw *hw = &adapter->hw;

	DBG ("e1000_transmit\n");
	
	if ( adapter->tx_fill_ctr == NUM_TX_DESC ) {
		DBG ("TX overflow\n");
		return -ENOBUFS;
	}

	/* Save pointer to iobuf we have been given to transmit,
	   netdev_tx_complete() will need it later
	 */
	adapter->tx_iobuf[adapter->tx_tail] = iobuf;

	/* Add the packet to TX ring
	 */
	adapter->tx_desc[adapter->tx_tail]->buffer_addr = 
		virt_to_bus ( iobuf->data );
	adapter->tx_desc[adapter->tx_tail]->lower.data = 
		E1000_TXD_CMD_RPS  | E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP |
		E1000_TXD_CMD_IFCS | iob_len ( iobuf );
	adapter->tx_desc[adapter->tx_tail]->upper.data = 0;
	
	DBG ( "TX fill: %ld tail: %ld addr: %#08lx len: %d\n", adapter->tx_fill_ctr, 
	      adapter->tx_tail, virt_to_bus ( iobuf->data ), iob_len ( iobuf ) );

	/* Point to next free descriptor */
	adapter->tx_tail = ( adapter->tx_tail + 1 ) % NUM_TX_DESC;

	adapter->tx_fill_ctr++;

	/* Write new tail to NIC, making packet available for transmit
	 */
	E1000_WRITE_REG ( hw, TDT, adapter->tx_tail ) ;

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
	struct io_buffer *rx_iob;

	uint32_t i;
	
#if 0
	DBG ( "e1000_poll\n" );
#endif

	/* Acknowledge interrupt. */
	icr = E1000_READ_REG ( hw, ICR );

#if 0
        DBG ( "e1000_poll: intr_status = %#08lx\n", icr );
#endif

	/* Check status of transmitted packets
	 */
	while ( ( i = adapter->tx_head ) != adapter->tx_tail ) {
	
		//tx_status = adapter->tx_desc[i]->upper.fields.status;
		tx_status = adapter->tx_desc[i]->upper.data;

		DBG ( "e1000_poll: tx_status = %#08lx\n", tx_status );

		/* if the packet at tx_head is not owned by hardware */
		if ( ! ( tx_status & E1000_TXD_STAT_DD ) )
			break;
		
		DBG ( "got packet. tx_head: %ld tx_tail: %ld tx_status: %#08lx\n",
	    	      adapter->tx_head, adapter->tx_tail, tx_status );

		if ( tx_status & ( E1000_TXD_STAT_EC | E1000_TXD_STAT_LC | 
				   E1000_TXD_STAT_TU ) ) {
			netdev_tx_complete_err ( netdev, adapter->tx_iobuf[i], -EINVAL );
			DBG ( "Error transmitting packet, tx_status: %#08lx\n",
			      tx_status );
		} else {
			netdev_tx_complete ( netdev, adapter->tx_iobuf[i] );
			DBG ( "Success transmitting packet, tx_status: %#08lx\n",
			      tx_status );
		}

		/* Decrement count of used descriptors, clear this descriptor 
		 */
		adapter->tx_fill_ctr--;
		memset ( &adapter->tx_desc[i], 0, sizeof ( struct e1000_tx_desc ) );
		
		adapter->tx_head = ( adapter->tx_head + 1 ) % NUM_TX_DESC;		
	}
	
	/* Process received packets 
	 */
	while ( ( rx_status = adapter->rx_desc[adapter->rx_tail]->status ) & E1000_RXD_STAT_DD ) {
	
		i = adapter->rx_tail;
		
		rx_len = adapter->rx_desc[i]->length;

                DBG ( "Received packet, rx_tail: %ld rx_status: %#08lx rx_len: %ld\n",
                      i, rx_status, rx_len );
                
                rx_err = adapter->rx_desc[adapter->rx_tail]->errors;
                
		if ( rx_err & E1000_RXD_ERR_FRAME_ERR_MASK ) {
		
			netdev_rx_err ( netdev, NULL, -EINVAL );
			DBG ( "e1000_poll: Corrupted packet received!"
			      " rx_err: %#08lx\n", rx_err );
			      
		} else 	{
		
			/* If unable allocate space for this packet,
			 *  try again next poll
			 */
			rx_iob = alloc_iob ( rx_len );
			if ( ! rx_iob ) 
				break;
				
			memcpy ( iob_put ( rx_iob, rx_len ), 
				adapter->rx_iobuf[i]->data, rx_len );
				
			/* Add this packet to the receive queue. 
			 */
			netdev_rx ( netdev, rx_iob );
		}

		adapter->rx_tail = ( adapter->rx_tail + 1 ) % NUM_RX_DESC;
	}
}				

/**
 * e1000_irq - Enable, Disable, or Force interrupts
 *
 * @v adapter   e1000 adapter
 * @v action    requested interrupt action
 **/
static void 
e1000_irq ( struct net_device *netdev, int enable )
{
	struct e1000_adapter *adapter = netdev_priv(netdev);

	DBG ( "e1000_irq\n" );
	
	switch ( enable ) {
	case 0 :
		E1000_WRITE_REG ( &adapter->hw, IMC, ~0 );
		E1000_WRITE_FLUSH ( &adapter->hw );
		break;
	case 1 :
		E1000_WRITE_REG ( &adapter->hw, IMS,
				E1000_IMS_RXT0 | E1000_IMS_RXSEQ );
		E1000_WRITE_FLUSH ( &adapter->hw );
		break;
	/* FIXME: Etherboot has a "FORCE" action, does gPXE? */
	case 2 :
		E1000_WRITE_REG ( &adapter->hw, ICS, E1000_ICS_RXT0 );
		break;
	}
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

	/* Allocate net device (also allocates memory for netdev->priv
	   and makes netdev-priv point to it 
	 */
	netdev = alloc_etherdev ( sizeof ( struct e1000_adapter ) );
	if ( ! netdev ) 
		goto err_alloc_etherdev;

        pci_set_drvdata ( pdev, netdev );

	adapter = netdev_priv ( netdev );
        memset ( adapter, 0, ( sizeof ( struct e1000_adapter ) ) );

	/* Enable PCI device associated with this NIC device */
	adjust_pci_device ( pdev );

	adapter->ioaddr = pdev->ioaddr;
        adapter->irqno = pdev->irq;
	adapter->netdev = netdev;
	adapter->pdev = pdev;
	adapter->hw.back = adapter;

	mmio_start = pci_bar_start ( pdev, PCI_BASE_ADDRESS_0 );
	mmio_len   = pci_bar_size  ( pdev, PCI_BASE_ADDRESS_0 );

	err = -EIO;

	adapter->hw.hw_addr = ioremap ( mmio_start, mmio_len );
	if ( ! adapter->hw.hw_addr )
		goto err_ioremap;

	for ( i = BAR_1; i <= BAR_5; i++ ) {
		if ( pci_bar_size ( pdev, i ) == 0 )
			continue;
		if ( pci_find_capability ( pdev, i ) & IORESOURCE_IO ) {
			adapter->hw.io_base = pci_bar_start ( pdev, i );
			break;
		}
	}

	/* Associate e1000-specific network operations operations with
	 * generic network device layer 
	 */
	netdev_init ( netdev, &e1000_operations );

	/* setup the private structure */
	if ( ( err = e1000_sw_init ( adapter ) ) )
		goto err_sw_init;

	DBG ( "adapter->hw.mac_type: %#08x\n", adapter->hw.mac_type );

	/* Flash BAR mapping must happen after e1000_sw_init
	 * because it depends on mac_type 
	 */
	if ( ( adapter->hw.mac_type == e1000_ich8lan ) && ( pdev->ioaddr ) ) {
		flash_start = pci_bar_start ( pdev, 1 );
		flash_len = pci_bar_size ( pdev, 1 );
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
	e1000_reset_hw ( &adapter->hw );

	/* make sure the EEPROM is good */
	if ( e1000_validate_eeprom_checksum( &adapter->hw ) < 0 ) {
		DBG ( "The EEPROM Checksum Is Not Valid\n" );
		goto err_eeprom;
	}

	/* copy the MAC address out of the EEPROM */
	if ( e1000_read_mac_addr ( &adapter->hw ) )
		DBG ( "EEPROM Read Error\n" );

        memcpy ( netdev->ll_addr, adapter->hw.mac_addr, ETH_ALEN );

	/* reset the hardware with the new settings */
	e1000_reset ( adapter );

	if ( ( err = register_netdev ( netdev ) ) != 0)
		goto err_register;

	/* No errors, return success */
	return 0;

/* Error return paths */
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

	e1000_reset_hw ( &adapter->hw );
	unregister_netdev ( netdev );
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
	if (err)
		goto err_setup_tx;

	/* allocate receive descriptors */
	err = e1000_setup_rx_resources ( adapter );
	if (err)
		goto err_setup_rx;

	e1000_power_up_phy ( adapter );

	/* disable firmware control */
	e1000_init_manageability ( adapter );

	e1000_configure ( adapter );

	return E1000_SUCCESS;

err_setup_rx:
	e1000_free_tx_resources ( adapter );
err_setup_tx:
	e1000_reset ( adapter );

	return err;
}

struct pci_driver e1000_driver __pci_driver = {
	.ids = e1000_nics,
	.id_count = (sizeof (e1000_nics) / sizeof (e1000_nics[0])),
	.probe = e1000_probe,
	.remove = e1000_remove,
};

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

    return E1000_SUCCESS;
}

void
e1000_pci_clear_mwi ( struct e1000_hw *hw __unused )
{
}

void
e1000_pci_set_mwi ( struct e1000_hw *hw __unused )
{
}

void
e1000_read_pci_cfg(struct e1000_hw *hw, uint32_t reg, uint16_t *value)
{
	struct e1000_adapter *adapter = hw->back;

	pci_read_config_word(adapter->pdev, reg, value);
}

void
e1000_write_pci_cfg(struct e1000_hw *hw, uint32_t reg, uint16_t *value)
{
	struct e1000_adapter *adapter = hw->back;

	pci_write_config_word(adapter->pdev, reg, *value);
}

void
e1000_io_write ( struct e1000_hw *hw  __unused, unsigned long port, uint32_t value )
{
	outl ( value, port );
}

/*
 * Local variables:
 *  c-basic-offset: 8
 *  c-indent-level: 8
 *  tab-width: 8
 * End:
 */
