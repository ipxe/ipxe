/*******************************************************************************

  Intel 82599 Virtual Function driver for iPXE environment
  Copyright(c) 2009 Intel Corporation.

  Copyright(c) 2010 Eric Keller <ekeller@princeton.edu>
  Copyright(c) 2010 Red Hat Inc.
        Alex Williamson <alex.williamson@redhat.com>

  Copyright(c) 2012 Nokia Siemens Networks
        Hermann Huy <hermann.huy@nsn.com>
        Bernhard Kohl <bernhard.kohl@nsn.com>

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
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#include "ixgbevf.h"

/**
 * ixgbevf_setup_tx_resources - allocate Tx resources (Descriptors)
 *
 * @v adapter   ixgbevf private structure
 *
 * @ret rc       Returns 0 on success, negative on failure
 **/
int ixgbevf_setup_tx_resources ( struct ixgbevf_adapter *adapter )
{
	DBGC (adapter, "IXGBEVF: ixgbevf_setup_tx_resources\n");

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

	DBGC (adapter, "IXGBEVF: adapter->tx_base = %#08lx\n", virt_to_bus ( adapter->tx_base ));

	return 0;
}

/**
 * ixgbevf_free_tx_resources - Free Tx Resources per Queue
 * @adapter: board private structure
 *
 * Free all transmit software resources
 **/
void ixgbevf_free_tx_resources ( struct ixgbevf_adapter *adapter )
{
	DBGC (adapter, "IXGBEVF: ixgbevf_free_tx_resources\n");

	free_dma ( adapter->tx_base, adapter->tx_ring_size );
}

/**
 * ixgbevf_free_rx_resources - Free Rx Resources
 * @adapter: board private structure
 *
 * Free all receive software resources
 **/
void ixgbevf_free_rx_resources ( struct ixgbevf_adapter *adapter )
{
	int i;

	DBGC (adapter, "IXGBEVF: ixgbevf_free_rx_resources\n");

	free_dma ( adapter->rx_base, adapter->rx_ring_size );

	for ( i = 0; i < NUM_RX_DESC; i++ ) {
		free_iob ( adapter->rx_iobuf[i] );
	}
}

/**
 * ixgbevf_refill_rx_ring - allocate Rx io_buffers
 *
 * @v adapter   ixgbevf private structure
 *
 * @ret rc       Returns 0 on success, negative on failure
 **/
static int ixgbevf_refill_rx_ring ( struct ixgbevf_adapter *adapter )
{
	int i, rx_curr;
	int rc = 0;
	union ixgbe_adv_rx_desc *rx_curr_desc;
	struct ixgbe_hw *hw = &adapter->hw;
	struct io_buffer *iob;

	DBGCP (adapter, "IXGBEVF: ixgbevf_refill_rx_ring\n");

	for ( i = 0; i < NUM_RX_DESC; i++ ) {
		rx_curr = ( ( adapter->rx_curr + i ) % NUM_RX_DESC );
		rx_curr_desc = adapter->rx_base + rx_curr;

		if ( rx_curr_desc->wb.upper.status_error & IXGBE_RXD_STAT_DD )
			continue;

		if ( adapter->rx_iobuf[rx_curr] != NULL )
			continue;

		DBGC2 (adapter, "IXGBEVF: Refilling rx desc %d\n", rx_curr);

		iob = alloc_iob ( MAXIMUM_ETHERNET_VLAN_SIZE );
		adapter->rx_iobuf[rx_curr] = iob;

		rx_curr_desc->wb.upper.status_error = 0;

		if ( ! iob ) {
			DBGC (adapter, "IXGBEVF: alloc_iob failed\n");
			rc = -ENOMEM;
			break;
		} else {
			rx_curr_desc->read.pkt_addr = virt_to_bus ( iob->data );
			rx_curr_desc->read.hdr_addr = 0;
			IXGBE_WRITE_REG(hw, IXGBE_VFRDT(0), rx_curr );
			DBGC (adapter, "IXGBEVF: Refilling adapter->rx_iobuf[%d]->data = %#08lx\n",
				rx_curr, virt_to_bus ( adapter->rx_iobuf[rx_curr]->data ));
		}
	}
	return rc;
}

/**
 * ixgbevf_irq_disable - Mask off interrupt generation on the NIC
 * @adapter: board private structure
 **/
static void ixgbevf_irq_disable ( struct ixgbevf_adapter *adapter )
{
	struct ixgbe_hw *hw = &adapter->hw;

	IXGBE_WRITE_REG(hw, IXGBE_VTEIMC, ~0);
	IXGBE_WRITE_FLUSH(hw);
}

/**
 * ixgbevf_irq_enable - Enable default interrupt generation settings
 * @adapter: board private structure
 **/
static void ixgbevf_irq_enable ( struct ixgbevf_adapter *adapter )
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 mask;

	/*
	 * According to 82599 data sheet Rev. 2.75
	 * "The VFEIAC registers are not supported since interrupt
	 * causes are always auto cleared."
	*/

	/* Enable auto clearing and auto setting for MSI-X RX vector */
	mask = (1 << 0);
	IXGBE_WRITE_REG(hw, IXGBE_VTEIAM, mask);

	/* Set mask bit for mailbox and RX MSI-X vectors */
	mask = (1 << 2) | (1 << 0);
	IXGBE_WRITE_REG(hw, IXGBE_VTEIMS, mask);

	IXGBE_WRITE_FLUSH(hw);
}

/**
 * ixgbevf_irq - enable or Disable interrupts
 *
 * @v adapter   ixgbevf adapter
 * @v action    requested interrupt action
 **/
static void ixgbevf_irq ( struct net_device *netdev, int enable )
{
	struct ixgbevf_adapter *adapter = netdev_priv ( netdev );

	DBGC (adapter, "IXGBEVF: ixgbevf_irq\n");

	if ( enable ) {
		ixgbevf_irq_enable ( adapter );
	} else {
		ixgbevf_irq_disable ( adapter );
	}
}

/**
 * ixgbevf_process_tx_packets - process transmitted packets
 *
 * @v netdev    network interface device structure
 **/
static void ixgbevf_process_tx_packets ( struct net_device *netdev )
{
	struct ixgbevf_adapter *adapter = netdev_priv ( netdev );
	uint32_t i;
	uint32_t tx_status;
	union ixgbe_adv_tx_desc *tx_curr_desc;

	/* Check status of transmitted packets
	 */
	DBGCP (adapter, "IXGBEVF: process_tx_packets: tx_head = %d, tx_tail = %d\n",
		adapter->tx_head, adapter->tx_tail);

	while ( ( i = adapter->tx_head ) != adapter->tx_tail ) {

		tx_curr_desc = ( void * )  ( adapter->tx_base ) +
					   ( i * sizeof ( *adapter->tx_base ) );

		tx_status = tx_curr_desc->wb.status;
		DBGC (adapter, "IXGBEVF: tx_curr_desc = %#08lx\n", virt_to_bus ( tx_curr_desc ));
		DBGC (adapter, "IXGBEVF: tx_status = %#08x\n", tx_status);

		/* if the packet at tx_head is not owned by hardware it is for us */
		if ( ! ( tx_status & IXGBE_TXD_STAT_DD ) )
			break;

		DBGC (adapter, "IXGBEVF: Sent packet. tx_head: %d tx_tail: %d tx_status: %#08x\n",
			adapter->tx_head, adapter->tx_tail, tx_status);

		netdev_tx_complete ( netdev, adapter->tx_iobuf[i] );
		DBGC (adapter, "IXGBEVF: Success transmitting packet, tx_status: %#08x\n",
			tx_status);

		/* Decrement count of used descriptors, clear this descriptor
		 */
		adapter->tx_fill_ctr--;
		memset ( tx_curr_desc, 0, sizeof ( *tx_curr_desc ) );

		adapter->tx_head = ( adapter->tx_head + 1 ) % NUM_TX_DESC;
	}
}

/**
 * ixgbevf_process_rx_packets - process received packets
 *
 * @v netdev    network interface device structure
 **/
static void ixgbevf_process_rx_packets ( struct net_device *netdev )
{
	struct ixgbevf_adapter *adapter = netdev_priv ( netdev );
	struct ixgbe_hw *hw = &adapter->hw;
	uint32_t i;
	uint32_t rx_status;
	uint32_t rx_len;
	uint32_t rx_err;
	union ixgbe_adv_rx_desc *rx_curr_desc;

	DBGCP (adapter, "IXGBEVF: ixgbevf_process_rx_packets\n");

	/* Process received packets
	 */
	while ( 1 ) {
		i = adapter->rx_curr;

		rx_curr_desc = ( void * )  ( adapter->rx_base ) +
				  ( i * sizeof ( *adapter->rx_base ) );
		rx_status = rx_curr_desc->wb.upper.status_error;

		DBGC2 (adapter, "IXGBEVF: Before DD Check RX_status: %#08x, rx_curr: %d\n",
			rx_status, i);

		if ( ! ( rx_status & IXGBE_RXD_STAT_DD ) )
			break;

		if ( adapter->rx_iobuf[i] == NULL )
			break;

		/* There is no E1000_RCTL register on 82599, use IXGBE_VFRXDCTL(0) instead */
		DBGC (adapter, "IXGBEVF: IXGBE_VFRXDCTL(0) = %#08x\n",
			IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(0)));

		rx_len = rx_curr_desc->wb.upper.length;

		DBGC (adapter, "IXGBEVF: Received packet, rx_curr: %d  rx_status: %#08x  rx_len: %d\n",
			i, rx_status, rx_len);
		DBGC (adapter, "IXGBEVF: adapter->rx_iobuf[%d]->data = %#08lx\n",
			i, virt_to_bus ( adapter->rx_iobuf[i]->data ));

		rx_err = rx_status;

		iob_put ( adapter->rx_iobuf[i], rx_len );

		if ( rx_err & IXGBE_RXDADV_ERR_FRAME_ERR_MASK ) {

			netdev_rx_err ( netdev, adapter->rx_iobuf[i], -EINVAL );
			DBGC (adapter, "IXGBEVF: ixgbevf_process_rx_packets: Corrupted packet received!"
				" rx_err: %#08x\n", rx_err);
		} else  {
			/* Add this packet to the receive queue. */
			netdev_rx ( netdev, adapter->rx_iobuf[i] );
		}
		adapter->rx_iobuf[i] = NULL;

		memset ( rx_curr_desc, 0, sizeof ( *rx_curr_desc ) );

		adapter->rx_curr = ( adapter->rx_curr + 1 ) % NUM_RX_DESC;
	}
}

/**
 * ixgbevf_poll - Poll for received packets
 *
 * @v netdev    Network device
 */
static void ixgbevf_poll ( struct net_device *netdev )
{
	struct ixgbevf_adapter *adapter = netdev_priv ( netdev );

	DBGCP (adapter, "IXGBEVF: ixgbevf_poll\n");


	ixgbevf_process_tx_packets ( netdev );

	ixgbevf_process_rx_packets ( netdev );

	ixgbevf_refill_rx_ring ( adapter );
}

/**
 * ixgbevf_configure_tx - Configure Transmit Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Tx unit of the MAC after a reset.
 **/
static void ixgbevf_configure_tx ( struct ixgbevf_adapter *adapter )
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 txdctl, txctrl, tdwbal;
	u32 i;

	DBGC (adapter, "IXGBEVF: ixgbevf_configure_tx\n");

	/* disable transmits while setting up the descriptors */
	/* According to document
	 *   Intel 82599 10 Gigabit Ethernet Controller Specification Update
	 *   Revision: 2.86 April 2012
	 *  chapter 1.5.5 Software Clarification,
	 *    5. PF/VF Drivers Should Configure Registers That Are
	 *       Not Reset By VFLR
	 * VFTXDCTL is one of those registers; therefore
	 * set PTHRESH, HTHRESH, WTHRESH, ENABLE and SWFLSH to zero
	 */
	txdctl = 0;
	IXGBE_WRITE_REG(hw, IXGBE_VFTXDCTL(0), txdctl );
	IXGBE_WRITE_FLUSH(hw);
	mdelay (50);

	IXGBE_WRITE_REG(hw, IXGBE_VFTDBAH(0), 0 );
	IXGBE_WRITE_REG(hw, IXGBE_VFTDBAL(0), virt_to_bus ( adapter->tx_base ) );
	IXGBE_WRITE_REG(hw, IXGBE_VFTDLEN(0), adapter->tx_ring_size );

	DBGC (adapter, "IXGBEVF: IXGBE_VFTDBAL(0): %#08x\n", IXGBE_READ_REG(hw, IXGBE_VFTDBAL(0) ));
	DBGC (adapter, "IXGBEVF: IXGBE_VFTDLEN(0): %d\n",    IXGBE_READ_REG(hw, IXGBE_VFTDLEN(0) ));

	/* Setup the HW Tx Head descriptor pointer
	 * HW TX Tail descriptor should be set after enabling
	 * of TX queue, see note in 82599 data sheet chap 4.6.8
	 */
	IXGBE_WRITE_REG(hw, IXGBE_VFTDH(0), 0 );

	adapter->tx_head = 0;
	adapter->tx_tail = 0;
	adapter->tx_fill_ctr = 0;

	/* Enabling transmit queue moved to end of this procedure */

	/* Setup Transmit Descriptor Settings for eop descriptor */
	adapter->txd_cmd  = IXGBE_ADVTXD_DCMD_EOP | IXGBE_ADVTXD_DCMD_IFCS;

	/* Advanced descriptor */
	adapter->txd_cmd |= IXGBE_ADVTXD_DCMD_DEXT;

	/* (not part of cmd, but in same 32 bit word...) */
	adapter->txd_cmd |= IXGBE_ADVTXD_DTYP_DATA;

	/* enable Report Status bit */
	adapter->txd_cmd |= IXGBE_ADVTXD_DCMD_RS;

	/* No collision items to be set on 82599 */

	/* Make sure that header write back is disabled.
	 * According to document
	 *   Intel 82599 10 Gigabit Ethernet Controller Specification Update
	 *   Revision: 2.86 April 2012
	 *  chapter 1.5.5 Software Clarification,
	 *    5. PF/VF Drivers Should Configure Registers That Are
	 *       Not Reset By VFLR
	 * VFTDWBAL: Write zero to whole register to disable Head Write-Back
	 * and to clear head write-back memory location (lowest 32 bits).
	 * VFTDBAH: Clear highest 32 bits of head write-back memory location.
	*/
	tdwbal = 0;
	IXGBE_WRITE_REG(hw, IXGBE_VFTDWBAL(0), tdwbal);
	IXGBE_WRITE_REG(hw, IXGBE_VFTDWBAH(0), tdwbal);

	/* Disable Tx Head Writeback RO bit, since this hoses
	 * bookkeeping if things aren't delivered in order.
	 * According to document
	 *   Intel 82599 10 Gigabit Ethernet Controller Specification Update
	 *   Revision: 2.86 April 2012
	 *  chapter 1.5.5 Software Clarification,
	 *    5. PF/VF Drivers Should Configure Registers That Are
	 *       Not Reset By VFLR
	 * VFDCA_TXCTRL is one of those registers
	 * Write to whole register, implicitly disabling
	 * - IXGBE_DCA_TXCTRL_DESC_DCA_EN
	 * - IXGBE_DCA_TXCTRL_DESC_RRO_EN
	 * - IXGBE_DCA_TXCTRL_DESC_WRO_EN
	 * - IXGBE_DCA_TXCTRL_DATA_RRO_EN
	 * and set CPUID to zero
	 */
	txctrl = 0;
	IXGBE_WRITE_REG(hw, IXGBE_VFDCA_TXCTRL(0), txctrl);

	/* Enable transmits */
	/* No TCTL register on 82599; use VF transmit descriptor control */
	txdctl = IXGBE_READ_REG(hw, IXGBE_VFTXDCTL(0));
	txdctl |= IXGBE_TXDCTL_ENABLE;
	IXGBE_WRITE_REG(hw, IXGBE_VFTXDCTL(0), txdctl );
	IXGBE_WRITE_FLUSH(hw);
	/* Poll the ENABLE bit until it is set before bumping the
	 * transmit descriptor tail */
	for (i = 0; i < IXGBEVF_TX_Q_ENABLE_LIMIT; i++) {
		if(IXGBE_READ_REG(hw,IXGBE_VFTXDCTL(0)) & IXGBE_TXDCTL_ENABLE)
			break;
		else
			udelay(1);
	}
	DBGC (adapter, "IXGBEVF: Microseconds until TX Q was enabled: %d\n", i);

	if( i == IXGBEVF_TX_Q_ENABLE_LIMIT)
		DBGC (adapter, "IXGBEVF: TX queue not enabled before setting VFTDT\n");
	/* HW TX tail descriptor */
	IXGBE_WRITE_REG(hw, IXGBE_VFTDT(0), 0 );
}

/* ixgbevf_reset - bring the hardware into a known good state
 *
 * This function boots the hardware and enables some settings that
 * require a configuration cycle of the hardware - those cannot be
 * set/changed during runtime. After reset the device needs to be
 * properly configured for Rx, Tx etc.
 */
void ixgbevf_reset ( struct ixgbevf_adapter *adapter )
{
	struct ixgbe_mac_info *mac = &adapter->hw.mac;
	struct net_device *netdev = adapter->netdev;
	struct ixgbe_hw *hw = &adapter->hw;

	/* Allow time for pending master requests to run */
	if ( mac->ops.reset_hw(hw) )
		DBGC (adapter, "IXGBEVF: PF still resetting\n");

	mac->ops.init_hw ( hw );

	if ( is_valid_ether_addr(adapter->hw.mac.addr) ) {
		memcpy ( netdev->hw_addr, adapter->hw.mac.addr, ETH_ALEN );
	}
}

extern void ixgbe_init_ops_vf(struct ixgbe_hw *hw);

/**
 * ixgbevf_sw_init - Initialize general software structures (struct ixgbevf_adapter)
 * @adapter: board private structure to initialize
 *
 * ixgbevf_sw_init initializes the Adapter private data structure.
 * Fields are initialized based on PCI device information and
 * OS network device settings (MTU size).
 **/
static int __devinit ixgbevf_sw_init(struct ixgbevf_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct pci_device *pdev = adapter->pdev;

	/* PCI config space info */

	hw->vendor_id = pdev->vendor;
	hw->device_id = pdev->device;

	pci_read_config_byte(pdev, PCI_REVISION_ID, &hw->revision_id);

	/* ixgbe_hw does not contain bus structure */
	/* pci_read_config_word ( pdev, PCI_COMMAND, &hw->bus.pci_cmd_word ); */

	adapter->max_frame_size = MAXIMUM_ETHERNET_VLAN_SIZE + ETH_HLEN + ETH_FCS_LEN;
	adapter->min_frame_size = ETH_ZLEN + ETH_FCS_LEN;

	/* Set various function pointers
	 * including hw->mac.ops.*
	 */
	ixgbe_init_ops_vf (hw);
	/* Set function pointers for mbx */
	adapter->hw.mbx.ops.init_params(hw);
		DBGC (adapter, "IXGBEVF: mac and mbx procedure pointers initialized\n");

	/* Explicitly disable IRQ since the NIC can be in any state. */
	ixgbevf_irq_disable ( adapter );

	return 0;

}

/**
 * ixgbevf_setup_srrctl - configure the receive control registers
 * @adapter: Board private structure
 **/
static void ixgbevf_setup_srrctl ( struct ixgbevf_adapter *adapter )
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 srrctl = 0;

	DBGC (adapter, "IXGBEVF: ixgbevf_setup_srrctl\n");

	srrctl = 0;

	/* Enable queue drop to avoid head of line blocking */
	srrctl |= IXGBE_SRRCTL_DROP_EN;

	/* Setup buffer sizes */
	srrctl |= 2048 >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
	srrctl |= IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;

	IXGBE_WRITE_REG(hw, IXGBE_VFSRRCTL(0), srrctl );
}

/** ixgbevf_rlpml_set_vf - Set the maximum receive packet length
 *  @hw: pointer to the HW structure
 *  @max_size: value to assign to max frame size
 **/
void ixgbevf_rlpml_set_vf(struct ixgbe_hw *hw, u16 max_size)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	u32 msgbuf[2];

	msgbuf[0] = IXGBE_VF_SET_LPE;
	msgbuf[1] = max_size;

	mbx->ops.write_posted(hw, msgbuf, 2, 0);
}

/**
 * ixgbevf_configure_rx - Configure 82599 Receive Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Rx unit of the MAC after a reset.
 **/
static void ixgbevf_configure_rx ( struct ixgbevf_adapter *adapter )
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 rxdctl, rxctrl;
	u32 i;

	DBGC (adapter, "IXGBEVF: ixgbevf_configure_rx\n");

	/* disable receives */
	/* According to document
	 *   Intel 82599 10 Gigabit Ethernet Controller Specification Update
	 *   Revision: 2.86 April 2012
	 *  chapter 1.5.5 Software Clarification,
	 *    5. PF/VF Drivers Should Configure Registers That Are
	 *       Not Reset By VFLR
	 * VFRXDCTL is one of those registers
	 * Handle as unclean -> write to whole register
	 * implicitely zero the ENABLE bit
	 */
	rxdctl = IXGBE_RXDCTL_VME;  /* strip VLAN tags */
	IXGBE_WRITE_REG(hw, IXGBE_VFRXDCTL(0), rxdctl );
	msleep ( 10 );

	/* PSRTYPE must be initialized in 82599 */
	IXGBE_WRITE_REG(hw, IXGBE_VFPSRTYPE, 0);

	/*
	 * Setup the HW Rx Head and Tail Descriptor Pointers and
	 * the Base and Length of the Rx Descriptor Ring
	 */
	IXGBE_WRITE_REG(hw, IXGBE_VFRDBAL(0), virt_to_bus (adapter->rx_base) );
	IXGBE_WRITE_REG(hw, IXGBE_VFRDBAH(0), 0 );
	IXGBE_WRITE_REG(hw, IXGBE_VFRDLEN(0), adapter->rx_ring_size );
	DBGC (adapter, "IXGBEVF: IXGBE_VFRDBAL(0)=  %#08x\n", IXGBE_READ_REG(hw, IXGBE_VFRDBAL(0)));
	DBGC (adapter, "IXGBEVF: IXGBE_VFRDLEN(0)=  %d\n",    IXGBE_READ_REG(hw, IXGBE_VFRDLEN(0)));
	adapter->rx_curr = 0;
	IXGBE_WRITE_REG(hw, IXGBE_VFRDH(0), 0 );
	IXGBE_WRITE_REG(hw, IXGBE_VFRDT(0), 0 );

	ixgbevf_rlpml_set_vf ( hw, adapter->max_frame_size );

	/* No thresholds in RXDCTL registers on Intel 82559EB */

	/* Make sure that order is not relaxed.
	 * According to document
	 *   Intel 82599 10 Gigabit Ethernet Controller Specification Update
	 *   Revision: 2.86 April 2012
	 *  chapter 1.5.5 Software Clarification,
	 *    5. PF/VF Drivers Should Configure Registers That Are
	 *       Not Reset By VFLR
	 * VFDCA_TXCTRL is one of those registers
	 */
	rxctrl = 0;
	IXGBE_WRITE_REG(hw, IXGBE_VFDCA_RXCTRL(0), rxctrl);

	/* enable receives */
	rxdctl |= IXGBE_RXDCTL_ENABLE;
	IXGBE_WRITE_REG(hw, IXGBE_VFRXDCTL(0), rxdctl );
	/* Poll the ENABLE bit until it is set before bumping the
	 * receive descriptor tail */
	for (i = 0; i < IXGBEVF_RX_Q_ENABLE_LIMIT; i++) {
		if(IXGBE_READ_REG(hw,IXGBE_VFRXDCTL(0)) & IXGBE_RXDCTL_ENABLE)
			break;
		else
			udelay(1);
	}
	DBGC (adapter, "IXGBEVF: Microseconds until RX Q was enabled: %d\n", i);
	if( i == IXGBEVF_RX_Q_ENABLE_LIMIT)
		DBGC (adapter, "IXGBEVF: RX queue not enabled before setting VFRDT\n");

	IXGBE_WRITE_REG(hw, IXGBE_VFRDT(0), NUM_RX_DESC );

	DBGC (adapter, "IXGBEVF: IXGBE_VFRXDCTL(0)=  %#08x\n", IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(0)));
	DBGC (adapter, "IXGBEVF: IXGBE_VFRDT(0)=  %d\n",       IXGBE_READ_REG(hw, IXGBE_VFRDT(0)));

}

/**
 * ixgbevf_setup_rx_resources - allocate Rx resources (Descriptors)
 *
 * @v adapter   ixgbevf private structure
 **/
int ixgbevf_setup_rx_resources ( struct ixgbevf_adapter *adapter )
{
	int i;
	union ixgbe_adv_rx_desc *rx_curr_desc;
	struct io_buffer *iob;

	DBGC (adapter, "IXGBEVF: ixgbevf_setup_rx_resources\n");

	/* Allocate receive descriptor ring memory.
	 * It must not cross a 64K boundary because of hardware errata
	 */

	adapter->rx_base =
		malloc_dma ( adapter->rx_ring_size, adapter->rx_ring_size );

	if ( ! adapter->rx_base ) {
		return -ENOMEM;
	}
	memset ( adapter->rx_base, 0, adapter->rx_ring_size );

	for ( i = 0; i < NUM_RX_DESC; i++ ) {
		rx_curr_desc = adapter->rx_base + i;
		iob = alloc_iob ( MAXIMUM_ETHERNET_VLAN_SIZE );
		adapter->rx_iobuf[i] = iob;
		rx_curr_desc->wb.upper.status_error = 0;
		if ( ! iob ) {
			DBGC (adapter, "IXGBEVF: alloc_iob failed\n");
			return -ENOMEM;
		} else {
			rx_curr_desc->read.pkt_addr = virt_to_bus ( iob->data );
			rx_curr_desc->read.hdr_addr = 0;
		}
	}

	return 0;
}

/**
 * ixgbevf_open - Called when a network interface is made active
 * @netdev: network interface device structure
 *
 * Returns 0 on success, negative value on failure
 *
 * The open entry point is called when a network interface is made
 * active by the system (IFF_UP).  At this point all resources needed
 * for transmit and receive operations are allocated, the interrupt
 * handler is registered with the OS, the watchdog timer is started,
 * and the stack is notified that the interface is ready.
 **/
static int ixgbevf_open ( struct net_device *netdev )
{
	struct ixgbevf_adapter *adapter = netdev_priv ( netdev );
	struct ixgbe_hw *hw = &adapter->hw;
	int err;
	u32 ivar;

	DBGC (adapter, "IXGBEVF: ixgbevf_open\n");

	/* Assign MSI-X interrupt vectors to RX,TX and MBX*/
	/* RX <--> vector 0 */
	/* TX <--> vector 1 */
	ivar = ((IXGBE_IVAR_ALLOC_VAL | 1) << 8) | (IXGBE_IVAR_ALLOC_VAL | 0);
	IXGBE_WRITE_REG(hw, IXGBE_VTIVAR(0), ivar);

	/* MBX <--> vector 2 */
	ivar = (IXGBE_IVAR_ALLOC_VAL | 2);
	IXGBE_WRITE_REG(hw, IXGBE_VTIVAR_MISC, ivar);

	/* allocate transmit descriptors */
	err = ixgbevf_setup_tx_resources ( adapter );
	if (err) {
		DBGC (adapter, "IXGBEVF: Error setting up TX resources!\n");
		goto err_setup_tx;
	}

	ixgbevf_configure_tx ( adapter );

	ixgbevf_setup_srrctl( adapter );

	err = ixgbevf_setup_rx_resources( adapter );
	if (err) {
		DBGC (adapter, "IXGBEVF: Error setting up RX resources!\n");
		goto err_setup_rx;
	}

	ixgbevf_configure_rx ( adapter );
	return 0;

err_setup_rx:
	DBGC (adapter, "IXGBEVF: err_setup_rx\n");
	ixgbevf_free_tx_resources ( adapter );
	return err;

err_setup_tx:
	DBGC (adapter, "IXGBEVF: err_setup_tx\n");
	ixgbevf_reset ( adapter );

	return err;
}

/**
 * ixgbevf_close - Disables a network interface
 * @netdev: network interface device structure
 *
 * Returns 0, this is not allowed to fail
 *
 * The close entry point is called when an interface is de-activated
 * by the OS.  The hardware is still under the drivers control, but
 * needs to be disabled.  A global MAC reset is issued to stop the
 * hardware, and all transmit and receive resources are freed.
 **/
static void ixgbevf_close ( struct net_device *netdev )
{
	struct ixgbevf_adapter *adapter = netdev_priv ( netdev );
	struct ixgbe_hw *hw = &adapter->hw;
	uint32_t rxdctl;

	DBGC (adapter, "IXGBEVF: ixgbevf_close\n");

	/* Disable and acknowledge interrupts */
	ixgbevf_irq_disable ( adapter );
	IXGBE_READ_REG(hw, IXGBE_VTEICR);

	/* disable receives */
	rxdctl = IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(0) );
	IXGBE_WRITE_REG(hw, IXGBE_VFRXDCTL(0), rxdctl & ~IXGBE_RXDCTL_ENABLE );
	mdelay ( 10 );

	ixgbevf_reset ( adapter );

	ixgbevf_free_tx_resources( adapter );
	ixgbevf_free_rx_resources( adapter );
}

/**
 * ixgbevf_transmit - Transmit a packet
 *
 * @v netdev    Network device
 * @v iobuf     I/O buffer
 *
 * @ret rc       Returns 0 on success, negative on failure
 */
static int ixgbevf_transmit ( struct net_device *netdev, struct io_buffer *iobuf )
{
	struct ixgbevf_adapter *adapter = netdev_priv ( netdev );
	struct ixgbe_hw *hw = &adapter->hw;
	uint32_t tx_curr = adapter->tx_tail;
	union ixgbe_adv_tx_desc *tx_curr_desc;

	DBGCP (adapter, "IXGBEVF: ixgbevf_transmit\n");

	if ( adapter->tx_fill_ctr == NUM_TX_DESC ) {
		DBGC (adapter, "IXGBEVF: TX overflow\n");
		return -ENOBUFS;
	}

	/* Save pointer to iobuf we have been given to transmit,
	   netdev_tx_complete() will need it later
	 */
	adapter->tx_iobuf[tx_curr] = iobuf;

	tx_curr_desc = ( void * ) ( adapter->tx_base ) +
			( tx_curr * sizeof ( *adapter->tx_base ) );

	DBGC (adapter, "IXGBEVF: tx_curr_desc = %#08lx\n", virt_to_bus ( tx_curr_desc ));
	DBGC (adapter, "IXGBEVF: tx_curr_desc + 16 = %#08lx\n", virt_to_bus ( tx_curr_desc ) + 16);
	DBGC (adapter, "IXGBEVF: iobuf->data = %#08lx\n", virt_to_bus ( iobuf->data ));

	/* Add the packet to TX ring
	 */
	tx_curr_desc->read.buffer_addr = virt_to_bus ( iobuf->data );
	tx_curr_desc->read.cmd_type_len = adapter->txd_cmd |(iob_len ( iobuf )) ;
	/* minus hdr_len ???? */
	tx_curr_desc->read.olinfo_status = ((iob_len ( iobuf )) << IXGBE_ADVTXD_PAYLEN_SHIFT);

	DBGC (adapter, "IXGBEVF: TX fill: %d tx_curr: %d addr: %#08lx len: %zd\n",
		adapter->tx_fill_ctr, tx_curr, virt_to_bus ( iobuf->data ), iob_len ( iobuf ));

	/* Point to next free descriptor */
	adapter->tx_tail = ( adapter->tx_tail + 1 ) % NUM_TX_DESC;
	adapter->tx_fill_ctr++;

	/* Write new tail to NIC, making packet available for transmit
	 */
	IXGBE_WRITE_REG(hw, IXGBE_VFTDT(0), adapter->tx_tail );
	IXGBE_WRITE_FLUSH(hw);

	return 0;
}

/** iPXE specific ixgbevf net device operations */
static struct net_device_operations ixgbevf_operations = {
	.open		= ixgbevf_open,
	.close		= ixgbevf_close,
	.transmit	= ixgbevf_transmit,
	.poll		= ixgbevf_poll,
	.irq		= ixgbevf_irq,
};

/**
 * ixgbevf_probe - Device Initialization Routine
 * @pdev: PCI device information struct
 * @ent: entry in ixgbevf_pci_tbl
 *
 * Returns 0 on success, negative on failure
 *
 * ixgbevf_probe initializes an adapter identified by a pci_dev structure.
 * The OS initialization, configuring of the adapter private structure,
 * and a hardware reset occur.
 **/
int ixgbevf_probe ( struct pci_device *pdev )
{
	int err;
	struct net_device *netdev;
	struct ixgbevf_adapter *adapter;
	unsigned long mmio_start, mmio_len;
	struct ixgbe_hw *hw;

	DBG ("IXGBEVF: ixgbevf_probe\n");

	err = -ENOMEM;

	/* Allocate net device ( also allocates memory for netdev->priv
	 * and makes netdev-priv point to it ) */
	netdev = alloc_etherdev ( sizeof ( struct ixgbevf_adapter ) );
	if ( ! netdev )
		goto err_alloc_etherdev;
	DBG ("IXGBEVF: netdev allocated at %p\n", netdev);
	DBG ("IXGBEVF: netdev physical addr= %#08lx\n", virt_to_bus(netdev));

	/* Associate ixgbevf-specific network operations operations with
	 * generic network device layer */
	netdev_init ( netdev, &ixgbevf_operations );
	DBG ("IXGBEVF: &netdev->op = %p\n", &(netdev->op));
	DBG ("IXGBEVF: &netdev->op physical addr= %#08lx\n", virt_to_bus( &(netdev->op)));

	/* Associate this network device with given PCI device */
	pci_set_drvdata ( pdev, netdev );
	netdev->dev = &pdev->dev;

	/* Initialize driver private storage */
	adapter = netdev_priv ( netdev );
	memset ( adapter, 0, ( sizeof ( *adapter ) ) );
	DBGC (adapter, "IXGBEVF: adapter at %p initialized\n", adapter);
	DBGC (adapter, "IXGBEVF: adapter physical addr= %#08lx\n", virt_to_bus(adapter));

	adapter->pdev = pdev;
	adapter->ioaddr = pdev->ioaddr;
	/* No io_base in ixgbe_hw structure
	 * adapter->hw.io_base = pdev->ioaddr; */

	hw = &adapter->hw;
	DBGC (adapter, "IXGBEVF: &adapter->hw:  %p\n", hw);
	DBGC (adapter, "IXGBEVF: &adapter->hw physical addr= %#08lx\n", virt_to_bus(hw));
	hw->vendor_id = pdev->vendor;
	hw->device_id = pdev->device;

	adapter->irqno = pdev->irq;
	adapter->netdev = netdev;
	adapter->hw.back = adapter;

	adapter->min_frame_size = ETH_ZLEN + ETH_FCS_LEN;
	adapter->max_hw_frame_size = ETH_FRAME_LEN + ETH_FCS_LEN;

	adapter->tx_ring_size = sizeof ( *adapter->tx_base ) * NUM_TX_DESC;
	adapter->rx_ring_size = sizeof ( *adapter->rx_base ) * NUM_RX_DESC;

	/* Fix up PCI device */
	adjust_pci_device ( pdev );

	err = -EIO;

	mmio_start = pci_bar_start ( pdev, PCI_BASE_ADDRESS_0 );
	mmio_len   = pci_bar_size  ( pdev, PCI_BASE_ADDRESS_0 );

	DBGC (adapter, "IXGBEVF: mmio_start: %#08lx\n", mmio_start);
	DBGC (adapter, "IXGBEVF: mmio_len: %#08lx\n", mmio_len);

	adapter->hw.hw_addr = ioremap ( mmio_start, mmio_len );
	DBGC (adapter, "IXGBEVF: adapter->hw.hw_addr: %p\n", adapter->hw.hw_addr);

	if ( ! adapter->hw.hw_addr ) {
		DBGC (adapter, "IXGBEVF: err_ioremap\n");
		goto err_ioremap;
	}

	/* setup adapter struct */
	err = ixgbevf_sw_init ( adapter );
	if (err) {
		DBGC (adapter, "IXGBEVF: err_sw_init\n");
		goto err_sw_init;
	}

	/* reset the controller to put the device in a known good state */
	err = hw->mac.ops.reset_hw ( hw );
	if ( err ) {
		DBGC (adapter, "IXGBEVF: PF still in reset state, assigning new address\n");
		hw->mac.addr[0] = 0x21;
		hw->mac.addr[1] = 0x21;
		hw->mac.addr[2] = 0x21;
		hw->mac.addr[3] = 0x21;
		hw->mac.addr[4] = 0x21;
		hw->mac.addr[5] = 0x21;
	} else {
		err = hw->mac.ops.get_mac_addr(hw,hw->mac.addr);
		if (err) {
			DBGC (adapter, "IXGBEVF: ERROR getting MAC address\n");
			goto err_hw_init;
		}
	}

	memcpy ( netdev->hw_addr, adapter->hw.mac.addr, ETH_ALEN );

	if ( ! is_valid_ether_addr( netdev->hw_addr ) ) {
		DBGC (adapter, "IXGBEVF: Invalid MAC Address: "
			"%02x:%02x:%02x:%02x:%02x:%02x\n",
			netdev->hw_addr[0], netdev->hw_addr[1],
			netdev->hw_addr[2], netdev->hw_addr[3],
			netdev->hw_addr[4], netdev->hw_addr[5]);
		err = -EIO;
		goto err_hw_init;
	}

	/* reset the hardware with the new settings */
	ixgbevf_reset ( adapter );

	/* Removed ixgbevf_get_hw_control call because CTRL_EXT
	 * is not for VF use */

	/* Mark as link up; we don't yet handle link state */
	netdev_link_up ( netdev );

	if ( ( err = register_netdev ( netdev ) ) != 0) {
		DBGC (adapter, "IXGBEVF: err_register\n");
		goto err_register;
	}

	DBGC (adapter, "IXGBEVF: ixgbevf_probe_succeeded\n");

	return 0;

err_register:
err_hw_init:
err_sw_init:
	iounmap ( adapter->hw.hw_addr );
err_ioremap:
	netdev_put ( netdev );
err_alloc_etherdev:
	return err;
}

/**
 * ixgbevf_remove - Device Removal Routine
 * @pdev: PCI device information struct
 *
 * ixgbevf_remove is called by the PCI subsystem to alert the driver
 * that it should release a PCI device.  The could be caused by a
 * Hot-Plug event, or because the driver is going to be removed from
 * memory.
 **/
void ixgbevf_remove ( struct pci_device *pdev )
{
	struct net_device *netdev = pci_get_drvdata ( pdev );
	struct ixgbevf_adapter *adapter = netdev_priv ( netdev );

	DBGC (adapter, "IXGBEVF: ixgbevf_remove\n");

	if ( adapter->hw.hw_addr )
		iounmap ( adapter->hw.hw_addr );

	unregister_netdev ( netdev );
	ixgbevf_reset  ( adapter );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/* ixgbevf_pci_tbl - PCI Device ID Table */
static struct pci_device_id ixgbevf_pci_tbl[] = {
	PCI_ROM(0x8086, 0x10ED, "ixgbevf", "IXGBE_DEV_ID_82599_VF", 0),
	PCI_ROM(0x8086, 0x1515, "board_x540_vf", "IXGBE_DEV_ID_X540_VF", 0),
};

struct pci_driver ixgbevf_driver __pci_driver = {
	.ids		= ixgbevf_pci_tbl,
	.id_count	= (sizeof(ixgbevf_pci_tbl) / sizeof(ixgbevf_pci_tbl[0])),
	.probe		= ixgbevf_probe,
	.remove		= ixgbevf_remove,
};

/* ixgbevf_main.c */
