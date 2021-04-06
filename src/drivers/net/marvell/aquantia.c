/*
* Copyright (C) 2017-2019 Aquantia Corp. <noreply@aquantia.com>
* Copyright (C) 2019-2021 Marvell  <support@marvell.com>
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*  1. Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*  2. Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
*  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
*  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
*  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
*  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
*  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
*  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
*  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
*  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
*  SUCH DAMAGE.
*
*/

FILE_LICENCE(BSD2);

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
#include <compiler.h>

#include "aquantia.h"


/** @file
*
* Marvell AQC network card driver
*
*/

static int atl_ring_alloc(const struct atl_nic *nic, struct atl_ring *ring,
	uint32_t desc_size, uint32_t reg_base)
{
	physaddr_t phy_addr;

	/* Allocate ring buffer.*/
	ring->length = ATL_RING_SIZE * desc_size;
	ring->ring = malloc_phys(ring->length, ATL_RING_ALIGN);
	if (!ring->ring)
		goto err_alloc;
	ring->sw_head = ring->sw_tail = 0;

	memset(ring->ring, 0, ring->length);

	/* Write ring address (hi & low parts).*/
	phy_addr = virt_to_bus(ring->ring);
	ATL_WRITE_REG((uint32_t)phy_addr, reg_base + 0);
	ATL_WRITE_REG((uint32_t)(((uint64_t)phy_addr) >> 32), reg_base + 4);
	/* Write ring length.*/
	ATL_WRITE_REG(ATL_RING_SIZE, reg_base + 8);

	/* #todo: reset head and tail pointers */
	DBG("ATLANTIC %p ring is at [%08llx,%08llx), reg base %#x\n",
	nic, ((unsigned long long)phy_addr),
	((unsigned long long) phy_addr + ring->length), reg_base);

	return 0;

err_alloc:
	if (ring->ring)
		free_phys(ring->ring, ring->length);

	ring->ring = NULL;
	ring->length = 0;

	return -ENOMEM;
}

static void atl_ring_free(struct atl_ring *ring)
{
	if (ring->ring)
		free_phys(ring->ring, ring->length);
	ring->ring = NULL;
	ring->length = 0;
}

static void atl_ring_next_dx(unsigned int *val)
{
	++(*val);
	if (*val == ATL_RING_SIZE)
		*val = 0;
}

int atl_ring_full(const struct atl_ring *ring)
{
	unsigned int tail = ring->sw_tail;
	atl_ring_next_dx(&tail);
	return tail == ring->sw_head;
}

void atl_rx_ring_fill(struct atl_nic *nic)
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
		rx = (struct atl_desc_rx *)nic->rx_ring.ring +
		nic->rx_ring.sw_tail;

		/* Populate receive descriptor */
		address = virt_to_bus(iobuf->data);
		rx->data_addr = address;
		rx->hdr_addr = 0;

		/* Record I/O buffer */
		assert(nic->iobufs[nic->rx_ring.sw_tail] == NULL);
		nic->iobufs[nic->rx_ring.sw_tail] = iobuf;

		DBG("AQUANTIA RX[%d] is [%llx,%llx)\n", nic->rx_ring.sw_tail,
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
	struct atl_nic *nic = netdev->priv;
	uint32_t ctrl = 0;
	DBG("AQUANTIA: atl_open()\n");

	/* Tx ring */
	if (atl_ring_alloc(nic, &nic->tx_ring, sizeof(struct atl_desc_tx),
		ATL_TX_DMA_DESC_ADDR) != 0)
		goto err_alloc;

	/* Rx ring */
	if (atl_ring_alloc(nic, &nic->rx_ring, sizeof(struct atl_desc_rx),
		ATL_RX_DMA_DESC_ADDR) != 0)
		goto err_alloc;

	/* Allocate interrupt vectors */
	ATL_WRITE_REG((ATL_IRQ_CTRL_COR_EN | ATL_IRQ_CTRL_REG_RST_DIS),
	ATL_IRQ_CTRL);

	/*TX & RX Interruprt Mapping*/
	ctrl = ATL_IRQ_MAP_REG1_RX0 | ATL_IRQ_MAP_REG1_RX0_EN |
		   ATL_IRQ_MAP_REG1_TX0 | ATL_IRQ_MAP_REG1_TX0_EN;
	ATL_WRITE_REG(ctrl, ATL_IRQ_MAP_REG1);

	/*TX interrupt ctrl reg*/
	ATL_WRITE_REG(ATL_TX_IRQ_CTRL_WB_EN, ATL_TX_IRQ_CTRL);

	/*RX interrupt ctrl reg*/
	ATL_WRITE_REG(ATL_RX_IRQ_CTRL_WB_EN, ATL_RX_IRQ_CTRL);

	/*RX data path*/
	ctrl = ATL_IRQ_TX | ATL_IRQ_RX;
	/* itr mask */
	ATL_WRITE_REG(ctrl,  ATL_ITR_MSKS);
	ATL_WRITE_REG((uint32_t)ATL_RX_MAX_LEN / 1024U,
	ATL_RX_DMA_DESC_BUF_SIZE);

	/*filter global ctrl */
	ctrl = ATL_RPF_CTRL1_BRC_EN | ATL_RPF_CTRL1_L2_PROMISC |
		ATL_RPF_CTRL1_ACTION | ATL_RPF_CTRL1_BRC_TSH;
	ATL_WRITE_REG(ctrl, ATL_RPF_CTRL1);

	/* vlan promisc */
	ATL_WRITE_REG(ATL_RPF_CTRL2_VLAN_PROMISC, ATL_RPF_CTRL2);
	/* enable rpf2 */
	ATL_WRITE_REG(ATL_RPF2_CTRL_EN, ATL_RPF2_CTRL);

	/* RX Packet Buffer 0 Register 1 */
	ATL_WRITE_REG(ATL_RPB0_CTRL1_SIZE, ATL_RPB0_CTRL1);

	/*RX Packet Buffer 0 Register 2 */
	ctrl = ATL_RPB0_CTRL2_LOW_TSH | ATL_RPB0_CTRL2_HIGH_TSH |
		ATL_RPB0_CTRL2_FC_EN;
	ATL_WRITE_REG(ctrl, ATL_RPB0_CTRL2);

	/*RPB global ctrl*/
	ctrl = ATL_READ_REG(ATL_RPB_CTRL);
	ctrl |= (ATL_RPB_CTRL_EN | ATL_RPB_CTRL_FC);
	ATL_WRITE_REG(ctrl, ATL_RPB_CTRL);

	/*TX data path*/
	/* enable tpo2 */
	ATL_WRITE_REG(ATL_TPO2_EN, ATL_TPO2_CTRL);
	/* tpb global ctrl *** */
	ATL_WRITE_REG(ATL_TPB0_CTRL1_SIZE, ATL_TPB0_CTRL1);

	ctrl = ATL_TPB0_CTRL2_LOW_TSH | ATL_TPB0_CTRL2_HIGH_TSH;
	/* tpb global ctrl *** */
	ATL_WRITE_REG(ctrl, ATL_TPB0_CTRL2);

	ctrl = ATL_READ_REG(ATL_TPB_CTRL);
	ctrl |= (ATL_TPB_CTRL_EN | ATL_TPB_CTRL_PAD_EN);
	/* tpb global ctrl */
	ATL_WRITE_REG(ctrl, ATL_TPB_CTRL);

	/*Enable rings*/
	ATL_WRITE_REG(ATL_READ_REG(ATL_RING_TX_CTRL) | ATL_RING_TX_CTRL_EN,
	ATL_RING_TX_CTRL);
	ATL_WRITE_REG(ATL_READ_REG(ATL_RING_RX_CTRL) | ATL_RING_RX_CTRL_EN,
	ATL_RING_RX_CTRL);

	atl_rx_ring_fill(nic);

	nic->hw_ops->start(nic);
	DBG("AQUANTIA: code 0()\n");

	return 0;
err_alloc:
	atl_ring_free(&nic->tx_ring);
	atl_ring_free(&nic->rx_ring);
	DBG("AQUANTIA: code NOMEM()\n");
	return -ENOMEM;
}

/**
* Close network device
*
* @v netdev		Network device
*/
static void atl_close(struct net_device *netdev)
{
	struct atl_nic *nic = netdev->priv;

	nic->hw_ops->stop(nic);
	/* rpb global ctrl */
	ATL_WRITE_REG(0x0, ATL_RPB_CTRL);

	/* tgb global ctrl */
	ATL_WRITE_REG(0x0, ATL_TPB_CTRL);

	ATL_WRITE_REG(ATL_READ_REG(ATL_RING_TX_CTRL) | (~ATL_RING_TX_CTRL_EN),
	ATL_RING_TX_CTRL);
	ATL_WRITE_REG(ATL_READ_REG(ATL_RING_RX_CTRL) | (~ATL_RING_RX_CTRL_EN),
	ATL_RING_RX_CTRL);

	/* clear itr mask */
	ATL_WRITE_REG(0x0, ATL_ITR_MSKS);

	/* Reset the NIC */
	nic->hw_ops->reset(nic);

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
	struct atl_nic *nic = netdev->priv;
	struct atl_desc_tx *tx;
	physaddr_t address;
	size_t len;

	/* Get next transmit descriptor */
	if (atl_ring_full(&nic->tx_ring)) {
		DBG("AQUANTIA: %p out of transmit descriptors\n", nic);
		return -ENOBUFS;
	}
	tx = (struct atl_desc_tx *)nic->tx_ring.ring + nic->tx_ring.sw_tail;

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

	DBG("AQUANTIA: %p TX[%d] is [%llx, %llx]\n", nic, nic->tx_ring.sw_tail,
		((unsigned long long) address),
		((unsigned long long) address + len));

	atl_ring_next_dx(&nic->tx_ring.sw_tail);
	ATL_WRITE_REG(nic->tx_ring.sw_tail, ATL_RING_TAIL);

	return 0;
}

void atl_check_link(struct net_device *netdev)
{
	struct atl_nic *nic = netdev->priv;

	uint32_t link_state = nic->hw_ops->get_link(nic);

	if (link_state != nic->link_state) {
		if (link_state) {
			DBG("AQUANTIA: link up\n");
			netdev_link_up(netdev);
		} else {
			DBG("AQUANTIA: link lost\n");
			netdev_link_down(netdev);
		}
		nic->link_state = link_state;
	}
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
		tx = (struct atl_desc_tx_wb *)nic->tx_ring.ring +
		nic->tx_ring.sw_head;
		/* Stop if descriptor is still in use */
		if (!tx->dd)
			return;

		DBG("AQUANTIA %p: TX[%d] complete\n",
		nic, nic->tx_ring.sw_head);

		/* Complete TX descriptor */
		atl_ring_next_dx(&nic->tx_ring.sw_head);
		netdev_tx_complete_next(netdev);
	}
}

/**
* Poll for received packets
*
* @v netdev		Network device
*/
void atl_poll_rx(struct net_device *netdev)
{
	struct atl_nic *nic = netdev->priv;
	struct atl_desc_rx_wb *rx;
	struct io_buffer *iobuf;
	size_t len;

	/* Check for received packets */
	while (nic->rx_ring.sw_head != nic->rx_ring.sw_tail) {

		/* Get next receive descriptor */
		rx = (struct atl_desc_rx_wb *)nic->rx_ring.ring +
		nic->rx_ring.sw_head;

		/* Stop if descriptor is still in use */
		DBG("AQUANTIA: rx poll: desc: %llx, %llx\n", *((uint64_t *)rx),
		*(((uint64_t *)rx) + 1));
		if (!rx->dd)
			return;

		/* Populate I/O buffer */
		iobuf = nic->iobufs[nic->rx_ring.sw_head];
		nic->iobufs[nic->rx_ring.sw_head] = NULL;
		len = le16_to_cpu(rx->pkt_len);
		iob_put(iobuf, len);

		/* Hand off to network stack */
		/*to do: process error*/
		DBG("AQUANTIA: %p RX[%d] complete (length %zd)\n",
			nic, nic->rx_ring.sw_head, len);
		netdev_rx(netdev, iobuf);
		DBG("AQUANTIA %p: RX[%d] complete\n",
		nic, nic->rx_ring.sw_head);
		atl_ring_next_dx(&nic->rx_ring.sw_head);
	}
}

/**
* Poll for completed and received packets
*
* @v netdev		Network device
*/
static void atl_poll(struct net_device *netdev)
{
	struct atl_nic *nic = netdev->priv;

	/* Check link state */
	atl_check_link(netdev);

	/* Check for and acknowledge interrupts */
	/* icr = ATL_READ_REG(ATL_IRQ_STAT_REG); */

	/*if(!icr)
		return;
	else
		//DBG("AQUANTIA: %p ICR 0x%X\n", nic, icr);*/

	/* Poll for TX completions, if applicable */
	/* if (icr & ATL_IRQ_TX) */
	atl_poll_tx(netdev);

	/* Poll for RX completions, if applicable */
	/* if (icr & ATL_IRQ_RX) */
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
static void atl_irq(struct net_device *netdev, int enable)
{
	struct atl_nic *nic = netdev->priv;
	uint32_t mask;

	DBG("AQUANTIA: irq: %d\n", enable);

	mask = (ATL_IRQ_TX | ATL_IRQ_RX);
	if (enable)
		ATL_WRITE_REG(mask, ATL_ITR_MSKS);
	else
		ATL_WRITE_REG(mask, ATL_ITR_MSKC);

}

/** Marvell network device operations */
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
static int atl_probe(struct pci_device *pci)
{
	struct net_device *netdev;
	struct atl_nic *nic;
	int rc = ENOERR;

	DBG("\nAQUANTIA: atl_probe()\n");

	/* Allocate and initialize net device */
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

	switch (nic->flags) {
	case ATL_FLAG_A1:
		nic->hw_ops = &atl_hw;
		break;
	case ATL_FLAG_A2:
		/* nic->hw_ops = atl2_hw;*/
		break;
	default:
		goto err_unsupported;
		break;
	}

	if (nic->hw_ops->reset(nic) != 0) {
		DBG("AQUANTIA: atl_probe reset error 0\n");
		goto err_reset;
	}

	if (nic->hw_ops->get_mac(nic, netdev->hw_addr) != 0)
		goto err_mac;

	/* Register network device */
	if (register_netdev(netdev) != 0)
		goto err_register_netdev;

	/* Set initial link state */
	netdev_link_down(netdev);

	DBG("AQUANTIA: atl_probe code 0\n");
	return 0;

	unregister_netdev(netdev);
err_register_netdev:
err_mac:
err_reset:
err_unsupported:
	iounmap(nic->regs);
err_ioremap:
	netdev_nullify(netdev);
	netdev_put(netdev);
err_alloc:
	DBG("AQUANTIA: error %#x()\n", rc);
	return rc;
}

/**
* Remove PCI device
*
* @v pci		PCI device
*/
static void atl_remove(struct pci_device *pci)
{
	struct net_device *netdev = pci_get_drvdata(pci);
	struct atl_nic *nic = netdev->priv;

	/* Unregister network device */
	unregister_netdev(netdev);

	/* Reset the NIC */
	nic->hw_ops->reset(nic);

	/* Free network device */
	iounmap(nic->regs);
	netdev_nullify(netdev);
	netdev_put(netdev);
}

/** Marvell PCI device IDs */
static struct pci_device_id atl_nics[] = {
	/* Atlantic 1 */
	/* 10G */
	PCI_ROM(0x1D6A, 0x0001, "AQC07",
	"Marvell AQtion 10Gbit Network Adapter", ATL_FLAG_A1),
	PCI_ROM(0x1D6A, 0xD107, "AQC07",
	"Marvell AQtion 10Gbit Network Adapter", ATL_FLAG_A1),
	PCI_ROM(0x1D6A, 0x07B1, "AQC07",
	"Marvell AQtion 10Gbit Network Adapter", ATL_FLAG_A1),
	PCI_ROM(0x1D6A, 0x87B1, "AQC07",
	"Marvell AQtion 10Gbit Network Adapter", ATL_FLAG_A1),

	/* SFP */
	PCI_ROM(0x1D6A, 0xD100, "AQC00", "Felicity Network Adapter",
	ATL_FLAG_A1),
	PCI_ROM(0x1D6A, 0x00B1, "AQC00", "Felicity Network Adapter",
	ATL_FLAG_A1),
	PCI_ROM(0x1D6A, 0x80B1, "AQC00", "Felicity Network Adapter",
	ATL_FLAG_A1),

	/* 5G */
	PCI_ROM(0x1D6A, 0xD108, "AQC08", "Marvell AQtion 5Gbit Network Adapter",
	ATL_FLAG_A1),
	PCI_ROM(0x1D6A, 0x08B1, "AQC08", "Marvell AQtion 5Gbit Network Adapter",
	ATL_FLAG_A1),
	PCI_ROM(0x1D6A, 0x88B1, "AQC08", "Marvell AQtion 5Gbit Network Adapter",
	ATL_FLAG_A1),
	PCI_ROM(0x1D6A, 0x11B1, "AQC11", "Marvell AQtion 5Gbit Network Adapter",
	ATL_FLAG_A1),
	PCI_ROM(0x1D6A, 0x91B1, "AQC11", "Marvell AQtion 5Gbit Network Adapter",
	ATL_FLAG_A1),

	/* 2.5G */
	PCI_ROM(0x1D6A, 0xD109, "AQC09",
	"Marvell AQtion 2.5Gbit Network Adapter", ATL_FLAG_A1),
	PCI_ROM(0x1D6A, 0x09B1, "AQC09",
	"Marvell AQtion 2.5Gbit Network Adapter", ATL_FLAG_A1),
	PCI_ROM(0x1D6A, 0x89B1, "AQC09",
	"Marvell AQtion 2.5Gbit Network Adapter", ATL_FLAG_A1),
	PCI_ROM(0x1D6A, 0x12B1, "AQC12",
	"Marvell AQtion 2.5Gbit Network Adapter", ATL_FLAG_A1),
	PCI_ROM(0x1D6A, 0x92B1, "AQC12",
	"Marvell AQtion 2.5Gbit Network Adapter", ATL_FLAG_A1),

	/* Atlantic 2 */
	PCI_ROM(0x1D6A, 0x00C0, "AQC13", "Marvell Antigua Engineering Sample",
	ATL_FLAG_A2),
	PCI_ROM(0x1D6A, 0x94C0, "AQC13", "Marvell Antigua Sample",
	ATL_FLAG_A2),
	PCI_ROM(0x1D6A, 0x93C0, "AQC13", "Marvell Antigua Sample",
	ATL_FLAG_A2),
	PCI_ROM(0x1D6A, 0x04C0, "AQC13", "Marvell Antigua Sample",
	ATL_FLAG_A2),
	PCI_ROM(0x1D6A, 0x14C0, "AQC13", "Marvell Antigua Sample",
	ATL_FLAG_A2),
	PCI_ROM(0x1D6A, 0x12C0, "AQC13", "Marvell Antigua Sample",
	ATL_FLAG_A2),
};

/** Marvell PCI driver */
struct pci_driver atl_driver __pci_driver = {
	.ids = atl_nics,
	.id_count = (sizeof(atl_nics) / sizeof(atl_nics[0])),
	.probe = atl_probe,
	.remove = atl_remove,
};
