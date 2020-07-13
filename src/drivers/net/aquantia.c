/*
* Copyright (C) 2017 Aquantia Corp. <noreply@aquantia.com>.
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

FILE_LICENCE(GPL2_OR_LATER_OR_UBDL);

#include <stdint.h>
#include <stdio.h>
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
#include <ipxe/profile.h>
#include "aquantia.h"

#define ATL_WRITE_REG(VAL, REG)	writel(VAL, nic->regs + (REG)) /*write register*/
#define ATL_READ_REG(REG)	readl(nic->regs + (REG)) /*read register*/

/** @file
*
* aQuantia AQC network card driver
*
*/
static int atl_download_dwords_(struct atl_nic * nic, uint32_t addr, void* buffer, uint32_t dword_count)
{
	uint32_t i;
	uint32_t* ptr = (uint32_t*)buffer;
	printf("AQUANTIA: download_dwords\n");
	for (i = 0; i < 100; ++i) {
		if (ATL_READ_REG(ATL_SEM_RAM))
			break;
		mdelay(100);
	}

	if (i == 100)
		goto err;

	ATL_WRITE_REG(addr, ATL_MBOX_CTRL3);
	for (i = 0; i < dword_count; ++i, ++ptr) {
		uint32_t j = 0U;
		ATL_WRITE_REG(0x8000, ATL_MBOX_CTRL1);
		for (j = 1024; (0x100U & ATL_READ_REG(ATL_MBOX_CTRL1)) && --j;);
		*ptr = ATL_READ_REG(ATL_MBOX_CTRL5);
	}

	ATL_WRITE_REG(1, ATL_SEM_RAM);

	return 0;
err:
	printf("AQUANTIA: download_dwords error\n");
	return -1;
}

static int atl_reset(struct atl_nic * nic)
{
	uint32_t i;
	struct atl_hw_stats stats = { 0 };
	uint32_t tid;
	printf("AQUANTIA: atl_reset\n");
	for (i = 0; i < 50 && nic->mbox_addr == 0; i++) {
		nic->mbox_addr = ATL_READ_REG(ATL_MBOX_ADDR);
		mdelay(100);
	}
	if (nic->mbox_addr == 0) {
		printf("AQUANTIA: atl_reset wait mbox_addr error\n");
		goto err_wait_fw;
	}

	ATL_WRITE_REG(ATL_READ_REG(ATL_PCI_CTRL) & ~ATL_PCI_CTRL_RST_DIS, ATL_PCI_CTRL); // PCI register reset disable
	ATL_WRITE_REG(ATL_READ_REG(ATL_RX_CTRL) & ~ATL_RX_CTRL_RST_DIS, ATL_RX_CTRL); // Rx register reset disable
	ATL_WRITE_REG(ATL_READ_REG(ATL_TX_CTRL) & ~ATL_TX_CTRL_RST_DIS, ATL_TX_CTRL); // Tx register reset disable

	ATL_WRITE_REG(0xC000, 0);
	mdelay(100);

	if (atl_download_dwords_(nic, nic->mbox_addr, &stats, sizeof stats / sizeof(uint32_t)) != 0) {
		printf("AQUANTIA: atl_reset first download error\n");
		goto err_wait_fw;
	}
	tid = stats.tid;

	for (i = 0; i < 50; i++) {
		if (atl_download_dwords_(nic, nic->mbox_addr, &stats, sizeof stats / sizeof(uint32_t)) != 0)
			goto err_wait_fw;
		if (stats.tid != tid)
			break;

		mdelay(100);
	}
	if (stats.tid == tid)
		goto err_wait_fw;
	printf("AQUANTIA: atl_reset success\n");
	return 0;

err_wait_fw:
	printf("AQUANTIA: atl_reset error\n");
	return -1;
}

static int atl_ring_alloc(const struct atl_nic* nic, struct atl_ring* ring, uint32_t desc_size, uint32_t reg_base)
{
	physaddr_t phy_addr;

	// Allocate ring buffer.
	ring->length = ATL_RING_SIZE * desc_size;
	ring->ring = malloc_phys(ring->length, ATL_RING_ALIGN);
	if (!ring->ring)
		goto err_alloc;
	ring->sw_head = ring->sw_tail = 0;

	memset(ring->ring, 0, ring->length);

	// Write ring address (hi & low parts).
	phy_addr = virt_to_bus(ring->ring);
	ATL_WRITE_REG((uint32_t)phy_addr, reg_base + 0);
	ATL_WRITE_REG((uint32_t)(((uint64_t)phy_addr) >> 32), reg_base + 4);
	// Write ring length.
	ATL_WRITE_REG(ATL_RING_SIZE, reg_base + 8);

	// #todo: reset head and tail pointers
	printf("ATLANTIC %p ring is at [%08llx,%08llx), reg base %#x\n", nic, ((unsigned long long)phy_addr),
		((unsigned long long) phy_addr + ring->length), reg_base);

	return 0;

err_alloc:
	if (ring->ring)
		free_phys(ring->ring, ring->length);

	ring->ring = NULL;
	ring->length = 0;

	return -ENOMEM;
}

static void atl_ring_free(struct atl_ring* ring)
{
	if (ring->ring)
		free_phys(ring->ring, ring->length);
	ring->ring = NULL;
	ring->length = 0;
}

static void atl_ring_next_dx(unsigned int* val)
{
	++(*val);
	if (*val == ATL_RING_SIZE)
		*val = 0;
}

int atl_ring_full(const struct atl_ring* ring)
{
	unsigned int tail = ring->sw_tail;
	atl_ring_next_dx(&tail);
	return tail == ring->sw_head;
	/*if (ring->sw_tail >= ring->sw_head)
	return ring->sw_tail - ring->sw_head + 1 == ATL_RING_SIZE;
	else
	return ATL_RING_SIZE - (ring->sw_head - ring->sw_tail + 1) == ATL_RING_SIZE;*/
}

void atl_rx_ring_fill(struct atl_nic* nic)
{
	struct atl_desc_rx *rx;
	struct io_buffer *iobuf;
	physaddr_t address;
	unsigned int refilled = 0;

	/* Refill ring */
	while (!atl_ring_full(&nic->rx_ring)) {

		/* Allocate I/O buffer */
		iobuf = alloc_iob(ATL_RX_MAX_LEN);
		if (!iobuf) {
			/* Wait for next refill */
			break;
		}

		/* Get next receive descriptor */
		rx = (struct atl_desc_rx*)nic->rx_ring.ring + nic->rx_ring.sw_tail;

		/* Populate receive descriptor */
		address = virt_to_bus(iobuf->data);
		rx->data_addr = address;
		rx->hdr_addr = 0; //unused

		/* Record I/O buffer */
		assert(nic->iobufs[nic->rx_ring.sw_tail] == NULL);
		nic->iobufs[nic->rx_ring.sw_tail] = iobuf;

		printf("AQUANTIA RX[%d] is [%llx,%llx)\n", nic->rx_ring.sw_tail,
			((unsigned long long) address),
			((unsigned long long) address + ATL_RX_MAX_LEN));
		atl_ring_next_dx(&nic->rx_ring.sw_tail);
		refilled++;
	}

	/* Push descriptors to card, if applicable */
	if (refilled) {
		wmb();
		ATL_WRITE_REG(nic->rx_ring.sw_tail, ATL_RING_TAIL_PTR);
	}
}

/**
* Open network device
*
* @v netdev		Network device
* @ret rc		Return status code
*/
static int atl_open(struct net_device *netdev)
{
	struct atl_nic* nic = netdev->priv;
	uint32_t ctrl = 0;
	printf("AQUANTIA: atl_open()\n");
	
	// Tx ring
	if (atl_ring_alloc(nic, &nic->tx_ring, sizeof(struct atl_desc_tx), ATL_TPB_CTRL_ADDR) != 0)
		goto err_alloc;

	// Rx ring
	if (atl_ring_alloc(nic, &nic->rx_ring, sizeof(struct atl_desc_rx), ATL_RPB_CTRL_ADDR) != 0)
		goto err_alloc;

	/* Allocate interrupt vectors */
	ATL_WRITE_REG((ATL_IRQ_CTRL_COR_EN | ATL_IRQ_CTRL_REG_RST_DIS), ATL_IRQ_CTRL);

	/*TX & RX Interruprt Mapping*/
	ctrl = ATL_IRQ_MAP_REG1_RX0 | ATL_IRQ_MAP_REG1_RX0_EN |
		   ATL_IRQ_MAP_REG1_TX0 | ATL_IRQ_MAP_REG1_TX0_EN;
	ATL_WRITE_REG(ctrl, ATL_IRQ_MAP_REG1);

	/*TX interrupt ctrl reg*/
	ATL_WRITE_REG(ATL_TX_IRQ_CTRL_WB_EN, ATL_TX_IRQ_CTRL);
	
	/*RX interrupt ctrl reg*/
	ATL_WRITE_REG(ATL_RX_IRQ_CTRL_WB_EN, ATL_RX_IRQ_CTRL);

	
	
	/*RX data path*/
	//ctrl = ATL_IRQ_TX | ATL_IRQ_RX | ATL_IRQ_LINK;
	ctrl = ATL_IRQ_TX | ATL_IRQ_RX;
	ATL_WRITE_REG(ctrl,  ATL_ITR_MSKS);//itr mask
	ATL_WRITE_REG((uint32_t)ATL_RX_MAX_LEN / 1024U, ATL_RPB_CTRL_SIZE);
	
	/*filter global ctrl */
	ctrl = ATL_RPF_CTRL1_BRC_EN | ATL_RPF_CTRL1_L2_PROMISC |
		   ATL_RPF_CTRL1_ACTION | ATL_RPF_CTRL1_BRC_TSH;
	ATL_WRITE_REG(ctrl,ATL_RPF_CTRL1);

	ATL_WRITE_REG(ATL_RPF_CTRL2_VLAN_PROMISC, ATL_RPF_CTRL2);//vlan promisc
	ATL_WRITE_REG(ATL_RPF2_CTRL_EN, ATL_RPF2_CTRL);//enable rpf2

	ATL_WRITE_REG(ATL_RPB0_CTRL1_SIZE, ATL_RPB0_CTRL1);//RX Packet Buffer 0 Register 1

	/*RX Packet Buffer 0 Register 2 */
	ctrl = ATL_RPB0_CTRL2_LOW_TSH | ATL_RPB0_CTRL2_HIGH_TSH | 
		   ATL_RPB0_CTRL2_FC_EN;
	ATL_WRITE_REG(ctrl, ATL_RPB0_CTRL2);

	/*RPB global ctrl*/
	ctrl = ATL_RPB_CTRL_EN | ATL_RPB_CTRL_FC | ATL_RPB_CTRL_TC_MODE;
	ATL_WRITE_REG(ctrl, ATL_RPB_CTRL);

	/*TX data path*/
	ATL_WRITE_REG(ATL_TPO2_EN, ATL_TPO2_CTRL);//enable tpo2
	ATL_WRITE_REG(ATL_TPB0_CTRL1, ATL_TPB0_CTRL1);//tpb global ctrl ***

	ctrl = ATL_TPB0_CTRL2_LOW_TSH | ATL_TPB0_CTRL2_HIGH_TSH;
	ATL_WRITE_REG(ctrl, ATL_TPB0_CTRL2);//tpb global ctrl ***

	ctrl = ATL_TPB_CTRL_EN | ATL_TPB_CTRL_PAD_EN | ATL_TPB_CTRL_TC_MODE;
	ATL_WRITE_REG(ctrl, ATL_TPB_CTRL);//tpb global ctrl ***

	/*Enable rings*/
	ATL_WRITE_REG(ATL_READ_REG(ATL_RING_TX_CTRL) | ATL_RING_TX_CTRL_EN, ATL_RING_TX_CTRL);
	ATL_WRITE_REG(ATL_READ_REG(ATL_RING_RX_CTRL) | ATL_RING_RX_CTRL_EN, ATL_RING_RX_CTRL);

	ATL_WRITE_REG(ATL_LINK_ADV_DOWNSHIFT | ATL_LINK_ADV_CMD | ATL_LINK_ADV_AUTONEG, ATL_LINK_ADV);
	
	atl_rx_ring_fill(nic);

	printf("AQUANTIA: code 0()\n");
	return 0;
err_alloc:
	atl_ring_free(&nic->tx_ring);
	atl_ring_free(&nic->rx_ring);
	printf("AQUANTIA: code NOMEM()\n");
	return -ENOMEM;
}

/**
* Close network device
*
* @v netdev		Network device
*/
static void atl_close(struct net_device *netdev) {
	struct atl_nic* nic = netdev->priv;

	ATL_WRITE_REG(0x0, ATL_RPB_CTRL);//rpb global ctrl

	ATL_WRITE_REG(0x0, ATL_TPB_CTRL);//tpb global ctrl

	ATL_WRITE_REG(ATL_READ_REG(ATL_RING_TX_CTRL) | (~ATL_RING_TX_CTRL_EN), ATL_RING_TX_CTRL);
	ATL_WRITE_REG(ATL_READ_REG(ATL_RING_RX_CTRL) | (~ATL_RING_RX_CTRL_EN), ATL_RING_RX_CTRL);

	ATL_WRITE_REG(0x0, ATL_ITR_MSKS);//clear itr mask

	ATL_WRITE_REG(0x0, ATL_LINK_ADV);

	atl_ring_free(&nic->tx_ring);
	atl_ring_free(&nic->rx_ring);
}

/**
* Transmit packet
*
* @v netdev		Network device
* @v iobuf		I/O buffer
* @ret rc		Return status code
*/
int atl_transmit(struct net_device *netdev, struct io_buffer *iobuf)
{
	struct atl_nic* nic = netdev->priv;
	struct atl_desc_tx* tx;
	physaddr_t address;
	size_t len;

	/* Get next transmit descriptor */
	if (atl_ring_full(&nic->tx_ring)) {
		printf("AQUANTIA: %p out of transmit descriptors\n", nic);
		return -ENOBUFS;
	}
	tx = (struct atl_desc_tx*)nic->tx_ring.ring + nic->tx_ring.sw_tail;

	/* Populate transmit descriptor */
	address = virt_to_bus(iobuf->data);
	tx->address = address;
	len = iob_len(iobuf);

	tx->flags = 0;
	tx->pay_len = tx->buf_len = len;
	tx->dx_type = 0x1;
	tx->eop = 0x1;
	tx->cmd = 0x22;
	wmb();

	printf("AQUANTIA: %p TX[%d] is [%llx, %llx]\n", nic, nic->tx_ring.sw_tail,
		((unsigned long long) address),
		((unsigned long long) address + len));

	atl_ring_next_dx(&nic->tx_ring.sw_tail);
	ATL_WRITE_REG(nic->tx_ring.sw_tail, ATL_RING_TAIL);

	return 0;
}

void atl_check_link(struct net_device* netdev)
{
	struct atl_nic *nic = netdev->priv;
	static int iteration = 0;

	uint32_t link_state = ATL_READ_REG(ATL_LINK_ST);
	//printf("AQUANTIA: atl_check_link: state = %X\n", link_state);
	mdelay(500);
	if ((link_state & 0xf) == 2 && (link_state & 0xff0000)) {
		netdev_link_up(netdev);
	   //if (iteration % 100 == 0)
			//printf("AQUANTIA: %p link UP\n", nic);
	}
	else {
		netdev_link_down(netdev);
		//if (iteration % 100 == 0)
			printf("AQUANTIA: %p link DOWN\n", nic);
	}

	++iteration;
}

/**
* Poll for completed packets
*
* @v netdev		Network device
*/
void atl_poll_tx(struct net_device *netdev)
{
	struct atl_nic *nic = netdev->priv;
	struct atl_desc_tx_wb *tx;

	/* Check for completed packets */
	while (nic->tx_ring.sw_head != nic->tx_ring.sw_tail) {

		/* Get next transmit descriptor */
		tx = (struct atl_desc_tx_wb*)nic->tx_ring.ring + nic->tx_ring.sw_head;
	   
		/* Stop if descriptor is still in use */
		if (!tx->dd)
			return;

		printf("AQUANTIA %p: TX[%d] complete\n", nic, nic->tx_ring.sw_head);

		/* Complete TX descriptor */
		netdev_tx_complete_next(netdev);
		atl_ring_next_dx(&nic->tx_ring.sw_head);
	}
}

/**
* Poll for received packets
*
* @v netdev		Network device
*/
void atl_poll_rx(struct net_device *netdev) {
	struct atl_nic *nic = netdev->priv;
	struct atl_desc_rx_wb *rx;
	struct io_buffer *iobuf;
	size_t len;

	/* Check for received packets */
	while (nic->rx_ring.sw_head != nic->rx_ring.sw_tail) {

		/* Get next receive descriptor */
		rx = (struct atl_desc_rx_wb*)nic->rx_ring.ring + nic->rx_ring.sw_head;
		
		/* Stop if descriptor is still in use */
		printf("AQUANTIA: rx poll: desc: %llx, %llx\n",*((uint64_t*)rx), *(((uint64_t*)rx) + 1));
		if (!rx->dd)
			return;

		/* Populate I/O buffer */
		iobuf = nic->iobufs[nic->rx_ring.sw_head];
		nic->iobufs[nic->rx_ring.sw_head] = NULL;
		len = le16_to_cpu(rx->pkt_len);
		iob_put(iobuf, len);

		/* Hand off to network stack */
		/*to do: process error*/
		printf("AQUANTIA: %p RX[%d] complete (length %zd)\n",
			nic, nic->rx_ring.sw_head, len);
		netdev_rx(netdev, iobuf);
		printf("AQUANTIA %p: RX[%d] complete\n", nic, nic->rx_ring.sw_head);
		atl_ring_next_dx(&nic->rx_ring.sw_head);
		mdelay(250);
	}
}

/**
* Poll for completed and received packets
*
* @v netdev		Network device
*/
static void atl_poll(struct net_device *netdev) {
	struct atl_nic *nic = netdev->priv;
	uint32_t icr;
	
	/* Check link state */
	atl_check_link(netdev);
	
	/* Check for and acknowledge interrupts */
	icr = ATL_READ_REG(ATL_IRQ_STAT_REG);
	
	/*if(!icr)
		return;
	else*/ 
		//printf("AQUANTIA: %p ICR 0x%X\n", nic, icr);
	
	/* Poll for TX completions, if applicable */
	if (icr & ATL_IRQ_TX)
		atl_poll_tx(netdev);

	/* Poll for RX completions, if applicable */
	if (icr & ATL_IRQ_RX)
		atl_poll_rx(netdev);

	

	/* Refill RX ring */
	atl_rx_ring_fill(nic);
}

/**
* Enable or disable interrupts
*
* @v netdev		Network device
* @v enable		Interrupts should be enabled
*/
static void atl_irq(struct net_device *netdev, int enable) {
	struct atl_nic *nic = netdev->priv;
	uint32_t mask;
	
	printf("AQUANTIA: irq: %d\n", enable);

	mask = (ATL_IRQ_TX | ATL_IRQ_RX);
	if (enable) {
		ATL_WRITE_REG(mask, ATL_ITR_MSKS);
	}
	else {
		ATL_WRITE_REG(mask, ATL_ITR_MSKC);
	}
}

/** Intel network device operations */
static struct net_device_operations atl_operations = {
	.open = atl_open,
	.close = atl_close,
	.transmit = atl_transmit,
	.poll = atl_poll,
	.irq = atl_irq,
};

/******************************************************************************
*
* PCI interface
*
*******************************************************************************
*/

/**
* Probe PCI device
*
* @v pci		PCI device
* @ret rc		Return status code
*/
static int atl_probe(struct pci_device *pci) {
	struct net_device *netdev;
	struct atl_nic *nic;
	int rc;
	printf("\nAQUANTIA: atl_probe()\n");
	/* Allocate and initialise net device */
	netdev = alloc_etherdev(sizeof(*nic));
	if (!netdev) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init(netdev, &atl_operations);
	nic = netdev->priv;
	pci_set_drvdata(pci, netdev);
	netdev->dev = &pci->dev;
	memset(nic, 0, sizeof(*nic));
	nic->port = PCI_FUNC(pci->busdevfn);
	nic->flags = pci->id->driver_data;

		/* Fix up PCI device */
	adjust_pci_device(pci);

	/* Map registers */
	nic->regs = ioremap(pci->membase, ATL_BAR_SIZE);
	if (!nic->regs) {
		rc = -ENODEV;
		goto err_ioremap;
	}
	
	printf("AQUANTIA: atl_probe fw ver =  %#x\n", ATL_READ_REG(0x18));
	/* Register network device */
	if ((rc = register_netdev(netdev)) != 0)
		goto err_register_netdev;

	if (atl_reset(nic) != 0) {
		printf("AQUANTIA: atl_probe reset error 0\n");
		goto err_reset;
	}

	/* Set initial link state */
	atl_check_link(netdev);

	printf("AQUANTIA: atl_probe code 0\n");
	return 0;

	unregister_netdev(netdev);
err_register_netdev:
err_reset:
	iounmap(nic->regs);
err_ioremap:
	netdev_nullify(netdev);
	netdev_put(netdev);
err_alloc:
	printf("AQUANTIA: error %#x()\n", rc);
	return rc;
}

/**
* Remove PCI device
*
* @v pci		PCI device
*/
static void atl_remove(struct pci_device *pci) {
	struct net_device *netdev = pci_get_drvdata(pci);
	struct atl_nic *nic = netdev->priv;

	/* Unregister network device */
	unregister_netdev(netdev);

	/* Reset the NIC */
	atl_reset(nic);

	/* Free network device */
	iounmap(nic->regs);
	netdev_nullify(netdev);
	netdev_put(netdev);
}

/** Intel PCI device IDs */
static struct pci_device_id atl_nics[] = {
	PCI_ROM(0x1D6A, 0x0001, "AQC07", "Aquantia AQtion 10Gbit Network Adapter", 0),
	PCI_ROM(0x1D6A, 0xD107, "AQC07", "Aquantia AQtion 10Gbit Network Adapter", 0),
	PCI_ROM(0x1D6A, 0xD108, "AQC07", "Aquantia AQtion 5Gbit Network Adapter", 0),
	PCI_ROM(0x1D6A, 0xD109, "AQC07", "Aquantia AQtion 2.5Gbit Network Adapter", 0),
};

/** Intel PCI driver */
struct pci_driver atl_driver __pci_driver = {
	.ids = atl_nics,
	.id_count = (sizeof(atl_nics) / sizeof(atl_nics[0])),
	.probe = atl_probe,
	.remove = atl_remove,
};
