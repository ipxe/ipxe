/* virtio-net.c - etherboot driver for virtio network interface
 *
 * (c) Copyright 2008 Bull S.A.S.
 *
 *  Author: Laurent Vivier <Laurent.Vivier@bull.net>
 *
 * some parts from Linux Virtio PCI driver
 *
 *  Copyright IBM Corp. 2007
 *  Authors: Anthony Liguori  <aliguori@us.ibm.com>
 *
 *  some parts from Linux Virtio Ring
 *
 *  Copyright Rusty Russell IBM Corporation 2007
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 *
 */

#include "etherboot.h"
#include "nic.h"
#include "gpxe/virtio-ring.h"
#include "gpxe/virtio-pci.h"
#include "virtio-net.h"

#define BUG() do { \
   printf("BUG: failure at %s:%d/%s()!\n", \
          __FILE__, __LINE__, __FUNCTION__); \
   while(1); \
} while (0)
#define BUG_ON(condition) do { if (condition) BUG(); } while (0)

/* Ethernet header */

struct eth_hdr {
   unsigned char dst_addr[ETH_ALEN];
   unsigned char src_addr[ETH_ALEN];
   unsigned short type;
};

struct eth_frame {
   struct eth_hdr hdr;
   unsigned char data[ETH_FRAME_LEN];
};

/* TX: virtio header and eth buffer */

static struct virtio_net_hdr tx_virtio_hdr;
static struct eth_frame tx_eth_frame;

/* RX: virtio headers and buffers */

#define RX_BUF_NB  6
static struct virtio_net_hdr rx_hdr[RX_BUF_NB];
static unsigned char rx_buffer[RX_BUF_NB][ETH_FRAME_LEN];

/* virtio queues and vrings */

enum {
   RX_INDEX = 0,
   TX_INDEX,
   QUEUE_NB
};

static struct vring_virtqueue virtqueue[QUEUE_NB];

/*
 * virtnet_disable
 *
 * Turn off ethernet interface
 *
 */

static void virtnet_disable(struct nic *nic)
{
   int i;

   for (i = 0; i < QUEUE_NB; i++) {
           vring_disable_cb(&virtqueue[i]);
           vp_del_vq(nic->ioaddr, i);
   }
   vp_reset(nic->ioaddr);
}

/*
 * virtnet_poll
 *
 * Wait for a frame
 *
 * return true if there is a packet ready to read
 *
 * nic->packet should contain data on return
 * nic->packetlen should contain length of data
 *
 */
static int virtnet_poll(struct nic *nic, int retrieve)
{
   unsigned int len;
   u16 token;
   struct virtio_net_hdr *hdr;
   struct vring_list list[2];

   if (!vring_more_used(&virtqueue[RX_INDEX]))
           return 0;

   if (!retrieve)
           return 1;

   token = vring_get_buf(&virtqueue[RX_INDEX], &len);

   BUG_ON(len > sizeof(struct virtio_net_hdr) + ETH_FRAME_LEN);

   hdr = &rx_hdr[token];   /* FIXME: check flags */
   len -= sizeof(struct virtio_net_hdr);

   nic->packetlen = len;
   memcpy(nic->packet, (char *)rx_buffer[token], nic->packetlen);

   /* add buffer to desc */

   list[0].addr = (char*)&rx_hdr[token];
   list[0].length = sizeof(struct virtio_net_hdr);
   list[1].addr = (char*)&rx_buffer[token];
   list[1].length = ETH_FRAME_LEN;

   vring_add_buf(&virtqueue[RX_INDEX], list, 0, 2, token, 0);
   vring_kick(nic->ioaddr, &virtqueue[RX_INDEX], 1);

   return 1;
}

/*
 *
 * virtnet_transmit
 *
 * Transmit a frame
 *
 */

static void virtnet_transmit(struct nic *nic, const char *destaddr,
        unsigned int type, unsigned int len, const char *data)
{
   struct vring_list list[2];

   /*
    * from http://www.etherboot.org/wiki/dev/devmanual :
    *     "You do not need more than one transmit buffer."
    */

   /* FIXME: initialize header according to vp_get_features() */

   tx_virtio_hdr.flags = 0;
   tx_virtio_hdr.csum_offset = 0;
   tx_virtio_hdr.csum_start = 0;
   tx_virtio_hdr.gso_type = VIRTIO_NET_HDR_GSO_NONE;
   tx_virtio_hdr.gso_size = 0;
   tx_virtio_hdr.hdr_len = 0;

   /* add ethernet frame into vring */

   BUG_ON(len > sizeof(tx_eth_frame.data));

   memcpy(tx_eth_frame.hdr.dst_addr, destaddr, ETH_ALEN);
   memcpy(tx_eth_frame.hdr.src_addr, nic->node_addr, ETH_ALEN);
   tx_eth_frame.hdr.type = htons(type);
   memcpy(tx_eth_frame.data, data, len);

   list[0].addr = (char*)&tx_virtio_hdr;
   list[0].length = sizeof(struct virtio_net_hdr);
   list[1].addr = (char*)&tx_eth_frame;
   list[1].length = ETH_FRAME_LEN;

   vring_add_buf(&virtqueue[TX_INDEX], list, 2, 0, 0, 0);

   vring_kick(nic->ioaddr, &virtqueue[TX_INDEX], 1);

   /*
    * http://www.etherboot.org/wiki/dev/devmanual
    *
    *   "You should ensure the packet is fully transmitted
    *    before returning from this routine"
    */

   while (!vring_more_used(&virtqueue[TX_INDEX])) {
           mb();
           udelay(10);
   }

   /* free desc */

   (void)vring_get_buf(&virtqueue[TX_INDEX], NULL);
}

static void virtnet_irq(struct nic *nic __unused, irq_action_t action)
{
   switch ( action ) {
   case DISABLE :
           vring_disable_cb(&virtqueue[RX_INDEX]);
           vring_disable_cb(&virtqueue[TX_INDEX]);
           break;
   case ENABLE :
           vring_enable_cb(&virtqueue[RX_INDEX]);
           vring_enable_cb(&virtqueue[TX_INDEX]);
           break;
   case FORCE :
           break;
   }
}

static void provide_buffers(struct nic *nic)
{
   int i;
   struct vring_list list[2];

   for (i = 0; i < RX_BUF_NB; i++) {
           list[0].addr = (char*)&rx_hdr[i];
           list[0].length = sizeof(struct virtio_net_hdr);
           list[1].addr = (char*)&rx_buffer[i];
           list[1].length = ETH_FRAME_LEN;
           vring_add_buf(&virtqueue[RX_INDEX], list, 0, 2, i, i);
   }

   /* nofify */

   vring_kick(nic->ioaddr, &virtqueue[RX_INDEX], i);
}

static struct nic_operations virtnet_operations = {
	.connect = dummy_connect,
	.poll = virtnet_poll,
	.transmit = virtnet_transmit,
	.irq = virtnet_irq,
};

/*
 * virtnet_probe
 *
 * Look for a virtio network adapter
 *
 */

static int virtnet_probe(struct nic *nic, struct pci_device *pci)
{
   u32 features;
   int i;

   /* Mask the bit that says "this is an io addr" */

   nic->ioaddr = pci->ioaddr & ~3;

   /* Copy IRQ from PCI information */

   nic->irqno = pci->irq;

   printf("I/O address 0x%08x, IRQ #%d\n", nic->ioaddr, nic->irqno);

   adjust_pci_device(pci);

   vp_reset(nic->ioaddr);

   features = vp_get_features(nic->ioaddr);
   if (features & (1 << VIRTIO_NET_F_MAC)) {
           vp_get(nic->ioaddr, offsetof(struct virtio_net_config, mac),
                  nic->node_addr, ETH_ALEN);
           printf("MAC address ");
	   for (i = 0; i < ETH_ALEN; i++) {
                   printf("%02x%c", nic->node_addr[i],
                          (i == ETH_ALEN - 1) ? '\n' : ':');
           }
   }

   /* initialize emit/receive queue */

   for (i = 0; i < QUEUE_NB; i++) {
           virtqueue[i].free_head = 0;
           virtqueue[i].last_used_idx = 0;
           memset((char*)&virtqueue[i].queue, 0, sizeof(virtqueue[i].queue));
           if (vp_find_vq(nic->ioaddr, i, &virtqueue[i]) == -1)
                   printf("Cannot register queue #%d\n", i);
   }

   /* provide some receive buffers */

    provide_buffers(nic);

   /* define NIC interface */

    nic->nic_op = &virtnet_operations;

   /* driver is ready */

   vp_set_features(nic->ioaddr, features & (1 << VIRTIO_NET_F_MAC));
   vp_set_status(nic->ioaddr, VIRTIO_CONFIG_S_DRIVER | VIRTIO_CONFIG_S_DRIVER_OK);

   return 1;
}

static struct pci_device_id virtnet_nics[] = {
PCI_ROM(0x1af4, 0x1000, "virtio-net",              "Virtio Network Interface", 0),
};

PCI_DRIVER ( virtnet_driver, virtnet_nics, PCI_NO_CLASS );

DRIVER ( "VIRTIO-NET", nic_driver, pci_driver, virtnet_driver,
	 virtnet_probe, virtnet_disable );
