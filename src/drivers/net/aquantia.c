/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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

/** @file
 *
 * aQuantia AQC network card driver
 *
 */

static int aq_download_dwords_(struct aq_nic * nic, uint32_t addr, void* buffer, uint32_t dword_count)
{
    uint32_t i;
    uint32_t* ptr = (uint32_t*)buffer;
    for (i = 0; i < 100; ++i) {
        if (readl(nic->regs + 0x3a8))
            break;
        mdelay(100);
    }

    if (i == 100)
        goto err;

    writel(addr, nic->regs + 0x208);
    for (i = 0; i < dword_count; ++i, ++ptr) {
        uint32_t j = 0U;
        writel(0x8000, nic->regs + 0x200);
        for (j = 1024; (0x100U & readl(nic->regs + 0x200)) && --j;);
        *ptr = readl(nic->regs + 0x20c);
    }

    writel(1, nic->regs + 0x3a8);

    return 0;
err:
    return -1;
}

static int aq_reset(struct aq_nic * nic)
{
    uint32_t i;
    struct aq_hw_stats stats = { 0 };
    uint32_t tid;
    printf("AQUANTIA: aq_reset\n");
    for (i = 0; i < 50 && nic->mbox_addr == 0; i++) {
        nic->mbox_addr = readl(nic->regs + 0x360);
        mdelay(100);
    }
    if (nic->mbox_addr == 0) {
        printf("AQUANTIA: aq_reset wait mbox_addr error\n");
        goto err_wait_fw;
    }
    
    writel(readl(nic->regs + 0x1000) & ~0x20000000, nic->regs + 0x1000); // PCI register reset disable
    writel(readl(nic->regs + 0x5000) & ~0x20000000, nic->regs + 0x5000); // Rx register reset disable
    writel(readl(nic->regs + 0x7000) & ~0x20000000, nic->regs + 0x7000); // Tx register reset disable

    writel(0xC000, nic->regs + 0);
    mdelay(100);

    if (aq_download_dwords_(nic, nic->mbox_addr, &stats, sizeof stats / sizeof(uint32_t)) != 0) {
        printf("AQUANTIA: aq_reset first download error\n");
        goto err_wait_fw;
    }
    tid = stats.tid;

    for (i = 0; i < 50; i++) {
        if (aq_download_dwords_(nic, nic->mbox_addr, &stats, sizeof stats / sizeof(uint32_t)) != 0)
            goto err_wait_fw;
        if (stats.tid != tid)
            break;

        mdelay(100);
    }
    if (stats.tid == tid)
        goto err_wait_fw;
    printf("AQUANTIA: aq_reset success\n");
    return 0;

err_wait_fw:
    printf("AQUANTIA: aq_reset error\n");
    return -1;
}


static int aq_ring_alloc(const struct aq_nic* nic, struct aq_ring* ring, uint32_t desc_size, uint32_t reg_base)
{
    physaddr_t phy_addr;

    // Allocate ring buffer.
    ring->length = AQ_RING_SIZE * desc_size;
    ring->ring = malloc_phys(ring->length, AQ_RING_ALIGN);
    if (!ring->ring)
        goto err_alloc;
    ring->sw_head = ring->sw_tail = 0;

    memset(ring->ring, 0, ring->length);

    // Write ring address (hi & low parts).
    phy_addr = virt_to_bus(ring->ring);
    writel((uint32_t)phy_addr, nic->regs + reg_base + 0);
    writel((uint32_t)(((uint64_t)phy_addr) >> 32), nic->regs + reg_base + 4);
    // Write ring length.
    writel(AQ_RING_SIZE, nic->regs + reg_base + 8);

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

static void aq_ring_free(struct aq_ring* ring)
{
    if (ring->ring)
        free_phys(ring->ring, ring->length);
    ring->ring = NULL;
    ring->length = 0;
}

static void aq_ring_next_dx(unsigned int* val)
{
    ++(*val);
    if (*val == AQ_RING_SIZE)
        *val = 0;
}

int aq_ring_full(const struct aq_ring* ring)
{
    unsigned int tail = ring->sw_tail;
    aq_ring_next_dx(&tail);
    return tail == ring->sw_head;
    /*if (ring->sw_tail >= ring->sw_head)
        return ring->sw_tail - ring->sw_head + 1 == AQ_RING_SIZE;
    else
        return AQ_RING_SIZE - (ring->sw_head - ring->sw_tail + 1) == AQ_RING_SIZE;*/
}


void aq_rx_ring_fill(struct aq_nic* nic)
{
    struct aq_desc_rx *rx;
    struct io_buffer *iobuf;
    physaddr_t address;
    unsigned int refilled = 0;

    /* Refill ring */
    while (!aq_ring_full(&nic->rx_ring)) {

        /* Allocate I/O buffer */
        iobuf = alloc_iob(AQ_RX_MAX_LEN);
        if (!iobuf) {
            /* Wait for next refill */
            break;
        }

        /* Get next receive descriptor */
        rx = (struct aq_desc_rx*)nic->rx_ring.ring + nic->rx_ring.sw_tail;

        /* Populate receive descriptor */
        address = virt_to_bus(iobuf->data);
        rx->data_addr = address;
        rx->hdr_addr = 0; //unused
        
        /* Record I/O buffer */
        assert(nic->iobufs[nic->rx_ring.sw_tail] == NULL);
        nic->iobufs[nic->rx_ring.sw_tail] = iobuf;

        printf("AQUANTIA RX[%d] is [%llx,%llx)\n", nic->rx_ring.sw_tail,
            ((unsigned long long) address),
            ((unsigned long long) address + AQ_RX_MAX_LEN));
        aq_ring_next_dx(&nic->rx_ring.sw_tail);
        refilled++;
    }

    /* Push descriptors to card, if applicable */
    if (refilled) {
        wmb();
        writel(nic->rx_ring.sw_tail, nic->regs + 0x5b10);
    }
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int aq_open (struct net_device *netdev)
{
    struct aq_nic* nic = netdev->priv;
    printf("AQUANTIA: aq_open()\n");
    // Tx ring
    if (aq_ring_alloc(nic, &nic->tx_ring, sizeof(struct aq_desc_tx), 0x7c00) != 0)
        goto err_alloc;

    // Rx ring
    if (aq_ring_alloc(nic, &nic->rx_ring, sizeof(struct aq_desc_rx), 0x5b00) != 0)
        goto err_alloc;

    

    /*RX data path*/
    writel(AQ_IRQ_TX | AQ_IRQ_RX | AQ_IRQ_LINK, nic->regs + 0x2060);//itr mask
    writel((uint32_t)AQ_RX_MAX_LEN/1024U, nic->regs + 0x5b18);
    writel(0x11009, nic->regs + 0x5100);//filter global ctrl
    writel(0x2, nic->regs + 0x5280);//vlan promisc
    writel(0xF0000, nic->regs + 0x5040);//enable rpf2

    writel(0x140, nic->regs + 0x5710);
    writel(0x9C000C00, nic->regs + 0x5714);
    writel(0x111, nic->regs + 0x5700);//rpb global ctrl

    /*TX data path*/
    writel(0x10000, nic->regs + 0x7040);//enable tpo2
    writel(0xA0, nic->regs + 0x7910);//tpb global ctrl
    writel(0xE000600, nic->regs + 0x7914);//tpb global ctrl
    writel(0x105, nic->regs + 0x7900);//tpb global ctrl

    // Enable rings.
    writel(readl(nic->regs + 0x7c08) | 0x80000000, nic->regs + 0x7c08);
    writel(readl(nic->regs + 0x5b08) | 0x80000000, nic->regs + 0x5b08);

    writel(0xFFFF0002, nic->regs + 0x368);

    aq_rx_ring_fill(nic);

    printf("AQUANTIA: code 0()\n");
    return 0;
err_alloc:
    aq_ring_free(&nic->tx_ring);
    aq_ring_free(&nic->rx_ring);
    printf("AQUANTIA: code NOMEM()\n");
    return -ENOMEM;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void aq_close ( struct net_device *netdev ) {
    struct aq_nic* nic = netdev->priv;

    writel(0x0, nic->regs + 0x5700);//rpb global ctrl

    writel(0x0, nic->regs + 0x7900);//tpb global ctrl

    writel(readl(nic->regs + 0x7c08) | (~0x80000000), nic->regs + 0x7c08);
    writel(readl(nic->regs + 0x5b08) | (~0x80000000), nic->regs + 0x5b08);

    writel(0x0, nic->regs + 0x368);

    aq_ring_free(&nic->tx_ring);
    aq_ring_free(&nic->rx_ring);
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
int aq_transmit ( struct net_device *netdev, struct io_buffer *iobuf )
{
    struct aq_nic* nic = netdev->priv;
    struct aq_desc_tx* tx;
    physaddr_t address;
    size_t len;

    /* Get next transmit descriptor */
    if (aq_ring_full(&nic->tx_ring)) {
        printf("AQUANTIA %p out of transmit descriptors\n", nic);
        return -ENOBUFS;
    }
    tx = (struct aq_desc_tx*)nic->tx_ring.ring + nic->tx_ring.sw_tail;

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

    printf("AQUANTIA %p TX[%d] is [%llx,%llx)\n", nic, nic->tx_ring.sw_tail,
        ((unsigned long long) address),
        ((unsigned long long) address + len));

    aq_ring_next_dx(&nic->tx_ring.sw_tail);
    writel(nic->tx_ring.sw_tail, nic->regs + 0x7c10);

    return 0;
}

void aq_check_link(struct net_device* netdev)
{
    struct aq_nic *nic = netdev->priv;
    static int iteration = 0;
    
    uint32_t link_state =  readl(nic->regs + 0x36c);
    //printf("AQUANTIA aq_check_link: state = %X\n", link_state);
    if ((link_state & 0xf) == 2 && (link_state & 0xff0000)) {
	netdev_link_up(netdev);
	//if (iteration % 100 == 0)
	    //printf("AQUANTIA %p link UP!\n", nic);
    } else {
	netdev_link_down(netdev);
	//if (iteration % 100 == 0)
	    //printf("AQUANTIA %p link DOWN!\n", nic);
    }

    ++iteration;
}

/**
 * Poll for completed packets
 *
 * @v netdev		Network device
 */
void aq_poll_tx(struct net_device *netdev)
{
    struct aq_nic *nic = netdev->priv;
    struct aq_desc_tx_wb *tx;

    /* Check for completed packets */
    while (nic->tx_ring.sw_head != nic->tx_ring.sw_tail) {

        /* Get next transmit descriptor */
        tx = (struct aq_desc_tx_wb*)nic->tx_ring.ring + nic->tx_ring.sw_head;

        /* Stop if descriptor is still in use */
        if (!tx->dd)
            return;

        printf("AQUANTIA %p TX[%d] complete\n", nic, nic->tx_ring.sw_head);

        /* Complete TX descriptor */
        netdev_tx_complete_next(netdev);
        aq_ring_next_dx(&nic->tx_ring.sw_head);
    }
}

/**
 * Poll for received packets
 *
 * @v netdev		Network device
 */
void aq_poll_rx ( struct net_device *netdev ) {
    struct aq_nic *nic = netdev->priv;
    struct aq_desc_rx_wb *rx;
    struct io_buffer *iobuf;
    size_t len;

    /* Check for received packets */
    while (nic->rx_ring.sw_head != nic->rx_ring.sw_tail) {

        /* Get next receive descriptor */
        rx = (struct aq_desc_rx_wb*)nic->rx_ring.ring + nic->rx_ring.sw_head;

        /* Stop if descriptor is still in use */
        /*printf("AQUANTIA: rx poll: desc: %llx, %llx\n",*((uint64_t*)rx), *(((uint64_t*)rx) + 1));*/
        if (!rx->dd)
            return;

        /* Populate I/O buffer */
        iobuf = nic->iobufs[nic->rx_ring.sw_head];
        nic->iobufs[nic->rx_ring.sw_head] = NULL;
        len = le16_to_cpu(rx->pkt_len);
        iob_put(iobuf, len);

        /* Hand off to network stack */
        /*to do: process error*/
        printf("AQUANTIA %p RX[%d] complete (length %zd)\n",
            nic, nic->rx_ring.sw_head, len);
        netdev_rx(netdev, iobuf);

        aq_ring_next_dx(&nic->rx_ring.sw_head);
    }
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void aq_poll ( struct net_device *netdev ) {
    struct aq_nic *nic = netdev->priv;
    //uint32_t icr;
    //printf("AQUANTIA: aq_poll()\n");
    /* Check for and acknowledge interrupts */
    //icr = readl(nic->regs + 0x2000);
    //printf("AQUANTIA: aq_poll(): %d\n", icr);
    /*if (!icr)
        return;*/

    /* Poll for TX completions, if applicable */
    //if (icr & AQ_IRQ_TX)
        aq_poll_tx(netdev);

    /* Poll for RX completions, if applicable */
    //if (icr & AQ_IRQ_RX)
        aq_poll_rx(netdev);

    /* Check link state, if applicable */
    //if (icr & AQ_IRQ_LINK)
        aq_check_link(netdev);

    /* Refill RX ring */
    aq_rx_ring_fill(nic);
}

/**
 * Enable or disable interrupts
 *
 * @v netdev		Network device
 * @v enable		Interrupts should be enabled
 */
static void aq_irq ( struct net_device *netdev, int enable ) {
    struct aq_nic *nic = netdev->priv;
    uint32_t mask;

    //printf("AQUANTIA: aq_irq(): %d\n", enable);

    mask = (AQ_IRQ_TX | AQ_IRQ_RX | AQ_IRQ_LINK);
    if (enable) {
        writel(mask, nic->regs + 0x2040);
    }
    else {
        writel(mask, nic->regs + 0x2050);
    }
}

/** Intel network device operations */
static struct net_device_operations aq_operations = {
	.open		= aq_open,
	.close		= aq_close,
	.transmit	= aq_transmit,
	.poll		= aq_poll,
	.irq		= aq_irq,
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
static int aq_probe ( struct pci_device *pci ) {
	struct net_device *netdev;
	struct aq_nic *nic;
	int rc;
    printf("AQUANTIA: aq_probe()\n");
	/* Allocate and initialise net device */
	netdev = alloc_etherdev ( sizeof ( *nic) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &aq_operations );
    nic = netdev->priv;
	pci_set_drvdata ( pci, netdev );
	netdev->dev = &pci->dev;
	memset (nic, 0, sizeof ( *nic) );
    nic->port = PCI_FUNC ( pci->busdevfn );
    nic->flags = pci->id->driver_data;


	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Map registers */
    nic->regs = ioremap ( pci->membase, AQUANITA_BAR_SIZE );
	if ( !nic->regs ) {
		rc = -ENODEV;
		goto err_ioremap;
	}

    printf("AQUANTIA: aq_probe fw ver =  %#x\n", readl(nic->regs + 0x18));
	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

    if (aq_reset(nic) != 0) {
        printf("AQUANTIA: aq_probe reset error 0\n");
        goto err_reset;
    }

	/* Set initial link state */
	aq_check_link(netdev);

    printf("AQUANTIA: aq_probe code 0\n");
	return 0;

	unregister_netdev ( netdev );
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
static void aq_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );
	struct aq_nic *nic = netdev->priv;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Reset the NIC */
	aq_reset (nic);

	/* Free network device */
	iounmap (nic->regs );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** Intel PCI device IDs */
static struct pci_device_id aq_nics[] = {
	PCI_ROM ( 0x1D6A, 0x0001, "AQC07", "Aquantia AQtion 10Gbit Network Adapter", 0 ),
	PCI_ROM (0x1D6A, 0xD107, "AQC07", "Aquantia AQtion 10Gbit Network Adapter", 0 ),
	PCI_ROM (0x1D6A, 0xD108, "AQC07", "Aquantia AQtion 5Gbit Network Adapter", 0 ),
    PCI_ROM(0x1D6A, 0xD109, "AQC07", "Aquantia AQtion 2.5Gbit Network Adapter", 0),
};

/** Intel PCI driver */
struct pci_driver aq_driver __pci_driver = {
	.ids = aq_nics,
	.id_count = ( sizeof ( aq_nics ) / sizeof ( aq_nics[0] ) ),
	.probe = aq_probe,
	.remove = aq_remove,
};
