#include <unistd.h>
#include <stdlib.h>
#include <ipxe/if_ether.h>
#include <ipxe/usb.h>
#include <ipxe/malloc.h>
#include <ipxe/ethernet.h>
#include <ipxe/iobuf.h>
#include <mii.h>
#include <errno.h>
#include <little_bswap.h>

#include "dm9601.h"
/* datasheet:
 http://www.davicom.com.tw/big5/download/Data%20Sheet/DM9601-DS-P01-930914.pdf
*/

/* control requests */
#define DM_READ_REGS	0x00
#define DM_WRITE_REGS	0x01
#define DM_READ_MEMS	0x02
#define DM_WRITE_REG	0x03
#define DM_WRITE_MEMS	0x05
#define DM_WRITE_MEM	0x07

/* registers */
#define DM_NET_CTRL	0x00
#define DM_RX_CTRL	0x05
#define DM_SHARED_CTRL	0x0b
#define DM_SHARED_ADDR	0x0c
#define DM_SHARED_DATA	0x0d	/* low + high */
#define DM_PHY_ADDR	0x10	/* 6 bytes */
#define DM_MCAST_ADDR	0x16	/* 8 bytes */
#define DM_GPR_CTRL	0x1e
#define DM_GPR_DATA	0x1f

#define DM_MAX_MCAST	64
#define DM_MCAST_SIZE	8
#define DM_EEPROM_LEN	256
#define DM_TX_OVERHEAD	2	/* 2 byte header */
#define DM_RX_OVERHEAD	7	/* 3 byte header + 4 byte crc tail */
#define DM_TIMEOUT	1000

static int dm_read(struct dm9601 *dev, uint8_t reg, uint16_t length, void *data)
{
	DBG("dm_read() reg=0x%02x length=%d\n", reg, length);
	return usb_control_msg(dev->udev,
			       &dev->udev->ep_0_in,
			       DM_READ_REGS,
			       USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			       0, reg, data, length);
}

static int dm_read_reg(struct dm9601 *dev, uint8_t reg, uint8_t *value)
{
	return dm_read(dev, reg, 1, value);
}

static int dm_write(struct dm9601 *dev, uint8_t reg, uint16_t length, void *data)
{
	DBG("dm_write() reg=0x%02x, length=%d\n", reg, length);
	return usb_control_msg(dev->udev,
			       &dev->udev->ep_0_out,
			       DM_WRITE_REGS,
			       USB_DIR_OUT | USB_TYPE_VENDOR |USB_RECIP_DEVICE,
			       0, reg, data, length);
}

static int dm_write_reg(struct dm9601 *dev, uint8_t reg, uint8_t value)
{
	DBG("dm_write_reg() reg=0x%02x, value=0x%02x\n", reg, value);
	return usb_control_msg(dev->udev,
			       &dev->udev->ep_0_out,
			       DM_WRITE_REG,
			       USB_DIR_OUT | USB_TYPE_VENDOR |USB_RECIP_DEVICE,
			       value, reg, NULL, 0);
}

static int dm_read_shared_word(struct dm9601 *dev, int phy, u8 reg, uint16_t *value)
{
	int ret, i;

	dm_write_reg(dev, DM_SHARED_ADDR, phy ? (reg | 0x40) : reg);
	dm_write_reg(dev, DM_SHARED_CTRL, phy ? 0xc : 0x4);

	for (i = 0; i < DM_TIMEOUT; i++) {
		u8 tmp;

		mdelay(1);
		ret = dm_read_reg(dev, DM_SHARED_CTRL, &tmp);
		if (ret < 0)
			goto out;

		/* ready */
		if ((tmp & 1) == 0)
			break;
	}

	if (i == DM_TIMEOUT) {
		DBG("%s read timed out!", phy ? "phy" : "eeprom");
		ret = -EIO;
		goto out;
	}

	dm_write_reg(dev, DM_SHARED_CTRL, 0x0);
	ret = dm_read(dev, DM_SHARED_DATA, 2, value);

	DBG( "read shared %d 0x%02x returned 0x%04x, %d\n",
	       phy, reg, *value, ret);
out:
	return ret;
}

static int dm_write_shared_word(struct dm9601 *dev, int phy, u8 reg, uint16_t value)
{
	int ret, i;

	ret = dm_write(dev, DM_SHARED_DATA, 2, &value);
	if (ret < 0)
		goto out;

	dm_write_reg(dev, DM_SHARED_ADDR, phy ? (reg | 0x40) : reg);
	dm_write_reg(dev, DM_SHARED_CTRL, phy ? 0x1c : 0x14);

	for (i = 0; i < DM_TIMEOUT; i++) {
		u8 tmp;

		udelay(1);
		ret = dm_read_reg(dev, DM_SHARED_CTRL, &tmp);
		if (ret < 0)
			goto out;

		/* ready */
		if ((tmp & 1) == 0)
			break;
	}

	if (i == DM_TIMEOUT) {
		DBG("%s write timed out!", phy ? "phy" : "eeprom");
		ret = -EIO;
		goto out;
	}

	dm_write_reg(dev, DM_SHARED_CTRL, 0x0);

out:
	return ret;
}


static int dm9601_mdio_read(struct net_device *netdev, int phy_id, int loc)
{
	struct dm9601 *dev = netdev_priv(netdev);

	uint16_t res;

	if (phy_id) {
		DBG( "Only internal phy supported\n");
		return 0;
	}

	dm_read_shared_word(dev, 1, loc, &res);

	DBG(
	       "dm9601_mdio_read() phy_id=0x%02x, loc=0x%02x, returns=0x%04x\n",
	       phy_id, loc, le16_to_cpu(res));

	return le16_to_cpu(res);
}

static void dm9601_mdio_write(struct net_device *netdev, int phy_id, int loc,
			      int val)
{
	struct dm9601 *dev = netdev_priv(netdev);
	uint16_t res = cpu_to_le16(val);

	if (phy_id) {
		DBG( "Only internal phy supported");
		return;
	}

	DBG("dm9601_mdio_write() phy_id=0x%02x, loc=0x%02x, val=0x%04x",
	       phy_id, loc, val);

	dm_write_shared_word(dev, 1, loc, res);
}

static int mii_nway_restart (struct dm9601 *dm9601)
{
	int bmcr;
	int r = -EINVAL;

	/* if autoneg is off, it's an error */
	bmcr = dm9601_mdio_read(dm9601->net, 0, MII_BMCR);

	if (bmcr & BMCR_ANENABLE) {
		bmcr |= BMCR_ANRESTART;
		dm9601_mdio_write(dm9601->net, 0, MII_BMCR, bmcr);
		r = 0;
	}

	return r;
}

void dm9601_remove(struct usb_device *udev)
{
	struct dm9601 *dm9601;
	struct net_device *netdev;

	dm9601 = (struct dm9601 *)udev->priv;
	netdev = dm9601->net;
	unregister_netdev(netdev);
}

static int enqueue_one_rx_urb(struct dm9601 *dm9601)
{
	struct urb *urb;
	struct io_buffer *iobuf;
	int ret = -ENOMEM;

	DBG("Enqueing one URB\n");
	
	iobuf = alloc_iob(DM9601_MTU);
	if (!iobuf)
		goto err_iobuf_malloc;

	urb = usb_alloc_urb();
	if (!urb)
		goto err_usb_malloc;

	usb_fill_bulk_urb(urb, dm9601->udev, dm9601->in, iob_put(iobuf, DM9601_MTU), DM9601_MTU);

	ret = usb_submit_urb(urb);
	if (ret < 0)
		goto err_submit_urb;
	
	urb->priv = iobuf;
	list_add_tail(&urb->priv_list, &dm9601->rx_queue);
	
	return 0;

err_submit_urb:
	usb_free_urb(urb);
err_usb_malloc:
	free_iob(iobuf);
err_iobuf_malloc:

	return ret;
}

int dm9601_open ( struct net_device *netdev) {

	struct dm9601 *dm9601 = netdev_priv(netdev);

	return enqueue_one_rx_urb(dm9601);
}

void dm9601_close ( struct net_device *netdev  __unused) {

}

int dm9601_transmit ( struct net_device *netdev,
				  struct io_buffer *iobuf ) {
	struct dm9601 *dm9601 = netdev_priv(netdev);
	int length;
	struct urb *urb = NULL;
	uint8_t status;
	void *buffer;
	int ret = -ENOMEM;

	length = iob_len(iobuf);

	if ((length % dm9601->maxpacket) == 0)
		buffer = malloc_dma(length + 2, 1);
	else
		buffer = malloc_dma(length + 3, 1);

	if(!buffer)
		goto err_buffer_malloc;

	((char *)buffer)[0] = length;
	((char *)buffer)[1] = length >> 8;

	memcpy(buffer + 2, iobuf->data, length);
	length += 2;

	urb = usb_alloc_urb();
	if (!urb)
		goto err_alloc_urb;

	/* don't assume the hardware handles USB_ZERO_PACKET
	 * NOTE:  strictly conforming cdc-ether devices should expect
	 * the ZLP here, but ignore the one-byte packet.
	 */
	if ((length % dm9601->maxpacket) == 0) {
		((char *)buffer)[length] = 0;
		length++;
	}

	usb_fill_bulk_urb (urb, dm9601->udev, dm9601->out,
			buffer, length);

	ret = usb_submit_urb (urb);
	if (ret < 0)
		goto err_submit_urb;

	urb->priv = iobuf;
	list_add_tail(&urb->priv_list, &dm9601->tx_queue);

	/* Report successful transmit completions */		
	list_for_each_entry(urb, &dm9601->tx_queue, priv_list) {
		if ((status = usb_urb_status(urb)) == USB_URB_STATUS_COMPLETE) {
			netdev_tx_complete(netdev, urb->priv);

			list_del(&urb->priv_list);

			free_dma(urb->transfer_buffer, urb->transfer_buffer_length);
			usb_unlink_urb(urb);
			
			DBG("TX DONE\n");
		}
	}

	return 0;
	
	/* Nothing to do */
err_submit_urb:
	usb_free_urb(urb);
err_alloc_urb:
	free_dma(buffer, length);
err_buffer_malloc:
	return ret;
}

void dm9601_poll ( struct net_device *netdev) {
	struct dm9601 *dm9601;
	struct urb *urb;
	uint8_t *buffer;
	struct io_buffer *iobuf;
	uint8_t status = 0;
	unsigned int len;
	
	dm9601 = netdev_priv(netdev);
	
	/* Check for RX */
	list_for_each_entry(urb, &dm9601->rx_queue, priv_list) {
	if ((status = usb_urb_status(urb)) == USB_URB_STATUS_COMPLETE) {
		if (enqueue_one_rx_urb(dm9601) < 0)
			DBG("Error enquing packet\n");

		buffer = urb->transfer_buffer;
		len = (buffer[1] | (buffer[2] << 8)) - 4;
		DBG("RX one packet len = %x:%x\n", buffer[1], buffer[2]);
		
		iobuf = urb->priv;
		iob_pull(iobuf, 3);
		iob_unput(iobuf, 1522 - (len + 3));
		DBG("len = %d ioblen = %d\n", len, iob_len(iobuf));
		netdev_rx(netdev, iobuf);

		list_del(&urb->priv_list);
		usb_unlink_urb(urb);
		}
	}
}


/* dm9601 net device operations */
static struct net_device_operations dm9601_operations = {
	.open		= dm9601_open,
	.close		= dm9601_close,
	.transmit	= dm9601_transmit,
	.poll		= dm9601_poll,
};

int dm9601_probe(struct usb_device *udev, 
			const struct usb_device_id *ids __unused)
{
	struct net_device *netdev;
	struct dm9601	*dm9601;
	unsigned int i;
	int ret = -ENOMEM;

	/* 
	 * vendor id and device ids are checked before we're called. So we just
	 * claim the device.
	 */
	netdev = alloc_etherdev(sizeof(*dm9601));
	if (!netdev)
		goto err_alloc_etherdev;

	netdev_init(netdev, &dm9601_operations);
	dm9601 = netdev->priv;
	INIT_LIST_HEAD(&dm9601->tx_queue);
	INIT_LIST_HEAD(&dm9601->rx_done_queue);
	INIT_LIST_HEAD(&dm9601->rx_queue);
	
	dm9601->udev = udev;
	dm9601->net = netdev;
	netdev->dev = &udev->dev;

	udev->priv = dm9601;

	for(i = 0;i < udev->num_endpoints; i++) {
		if (usb_ep_xfertype(udev->endpoints[i]) == USB_ENDPOINT_XFER_BULK &&
				usb_ep_dir(udev->endpoints[i]) == USB_DIR_IN)
			dm9601->in = udev->endpoints[i];

		if (usb_ep_xfertype(udev->endpoints[i]) == USB_ENDPOINT_XFER_BULK &&
				usb_ep_dir(udev->endpoints[i]) == USB_DIR_OUT)
			dm9601->out = udev->endpoints[i];
	}
	dm9601->maxpacket = le16_to_cpu(dm9601->in->desc.wMaxPacketSize);

	if ((ret = dm_write_reg(dm9601, DM_NET_CTRL, 1) < 0)) {
		goto err_reg_write;
	}
	mdelay(2);

	if (dm_read(dm9601, DM_PHY_ADDR, ETH_ALEN, dm9601->net->ll_addr) < 0) {
		DBG("Error reading MAC address\n");
		goto err_reading_mac;
	}

	DBG("DM9601 MAC Address : %02x", dm9601->net->ll_addr[0]);
	for(i=1;i<6;i++)
		DBG(":%02x", dm9601->net->ll_addr[i]);

	/* power up phy */
	if (dm_write_reg(dm9601, DM_GPR_CTRL, 1) < 0)
		goto err_reg_write;

	if (dm_write_reg(dm9601, DM_GPR_DATA, 0) < 0)
		goto err_reg_write;

	if(dm_write_reg(dm9601, DM_RX_CTRL, 0x31) < 0)
		goto err_reg_write;

	dm9601_mdio_write(netdev, 0, MII_BMCR, BMCR_RESET);
	
	dm9601_mdio_write(netdev, 0, MII_ADVERTISE,
			ADVERTISE_ALL | ADVERTISE_CSMA);

	if (mii_nway_restart(dm9601) < 0)
		goto err_restart;
	
	/* Register network device */
	if ((ret = register_netdev(netdev)) != 0) {
		goto err_register_device;
	}
		
	/* Mark as link up*/
	netdev_link_up(netdev);
	return 0;

	netdev_nullify(netdev);

	/* Nothing to do */
err_register_device:	
	/* Nothing to do */
err_restart:
	/* Nothing to do */
err_reg_write:
	/* Nothing to do */
err_reading_mac:
	netdev_nullify(netdev);
	netdev_put(netdev);
err_alloc_etherdev:
	return ret;
}

static struct usb_device_id dm9601_ids[] = {
	USB_ROM(0x0a46, 0x9601, "DM9601", "Davicom 9601", 0),
};

struct usb_driver dm9601_usb_driver  __usb_driver = {
	.ids = dm9601_ids,
	.id_count = (sizeof(dm9601_ids) / sizeof(dm9601_ids[0])),
	.probe = dm9601_probe,
	.remove = dm9601_remove,
};

