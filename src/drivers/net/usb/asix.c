#include <unistd.h>
#include <stdlib.h>
#include <ipxe/if_ether.h>
#include <ipxe/usb.h>
#include <ipxe/pci.h>
#include <ipxe/malloc.h>
#include <ipxe/ethernet.h>
#include <ipxe/iobuf.h>
#include <mii.h>
#include <errno.h>
#include <little_bswap.h>

#include "asix.h"

static const char driver_name[] = "asix";

static int asix_read_cmd(struct asix *asix, uint8_t cmd, uint16_t value, u16 index,
			    uint16_t size, void *data)
{
	void *buf;
	int err = -ENOMEM;

	DBG("asix_read_cmd() cmd=0x%02x value=0x%04x index=0x%04x size=%d\n",
		cmd, value, index, size);

	buf = malloc_dma(size,1);
	if (!buf)
		goto out;

	err = usb_control_msg(asix->udev, &asix->udev->ep_0_in,	cmd,
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE, value,
			index,	buf, size);

	memcpy(data, buf, size);
	free_dma(buf, size);
out:
	return err;
}

static int asix_write_cmd(struct asix *asix, uint8_t cmd, uint16_t value, u16 index,
			     uint16_t size, void *data)
{
	void *buf = NULL;
	int err = -ENOMEM;

	DBG("asix_write_cmd() cmd=0x%02x value=0x%04x index=0x%04x size=%d\n",
		cmd, value, index, size);

	if (data) {
		buf = malloc_dma(size, 1);
		if (!buf)
			goto out;
		memcpy(buf, data, size);
	}

	err = usb_control_msg(asix->udev, &asix->udev->ep_0_out, cmd,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			value, index, buf, size);
	free_dma(buf, size);

out:
	return err;
}

static int asix_write_gpio(struct asix *asix, uint16_t value, int sleep)
{
	int ret;

	DBG("asix_write_gpio() - value = 0x%04x\n", value);
	ret = asix_write_cmd(asix, AX_CMD_WRITE_GPIOS, value, 0, 0, NULL);
	if (ret < 0)
		DBG("Failed to write GPIO value 0x%04x: %02x\n",
			value, ret);

	if (sleep)
		mdelay(sleep);

	return ret;
}

static inline int asix_set_sw_mii(struct asix *asix)
{
	int ret;
	ret = asix_write_cmd(asix, AX_CMD_SET_SW_MII, 0x0000, 0, 0, NULL);
	if (ret < 0)
		DBG("Failed to enable software MII access");
	return ret;
}

static inline int asix_set_hw_mii(struct asix *asix)
{
	int ret;
	ret = asix_write_cmd(asix, AX_CMD_SET_HW_MII, 0x0000, 0, 0, NULL);
	if (ret < 0)
		DBG("Failed to enable hardware MII access");
	return ret;
}

static void
asix_mdio_write(struct net_device *netdev, int phy_id, int loc, int val)
{
	struct asix *asix = netdev_priv(netdev);
	uint16_t res = cpu_to_le16(val);

	DBG("asix_mdio_write() phy_id=0x%02x, loc=0x%02x, val=0x%04x\n", phy_id, loc, val);
	asix_set_sw_mii(asix);
	asix_write_cmd(asix, AX_CMD_WRITE_MII_REG, phy_id, (uint16_t)loc, 2, &res);
	asix_set_hw_mii(asix);
}

static int asix_mdio_read(struct net_device *netdev, int phy_id, int loc)
{
	struct asix *asix = netdev_priv(netdev);
	uint16_t res;

	asix_set_sw_mii(asix);
	asix_read_cmd(asix, AX_CMD_READ_MII_REG, phy_id,
				(uint16_t)loc, 2, &res);
	asix_set_hw_mii(asix);

	DBG("asix_mdio_read() phy_id=0x%02x, loc=0x%02x, returns=0x%04x\n", phy_id, loc, le16_to_cpu(res));

	return le16_to_cpu(res);
}


static int asix_write_medium_mode(struct asix *asix, uint16_t mode)
{
	int ret;

	DBG("asix_write_medium_mode() - mode = 0x%04x\n", mode);
	ret = asix_write_cmd(asix, AX_CMD_WRITE_MEDIUM_MODE, mode, 0, 0, NULL);
	if (ret < 0)
		DBG("Failed to write Medium Mode mode to 0x%04x: %02x\n",
			mode, ret);

	return ret;
}

static inline int asix_get_phy_addr(struct asix *asix)
{
	uint8_t buf[2];
	int ret = asix_read_cmd(asix, AX_CMD_READ_PHY_ID, 0, 0, 2, buf);

	DBG("asix_get_phy_addr()");

	if (ret < 0) {
		DBG("Error reading PHYID register: %02x\n", ret);
		goto out;
	}
	DBG("asix_get_phy_addr() returning 0x%04x\n", *((char *)buf));
	ret = buf[1];

out:
	return ret;
}

static int enqueue_one_rx_urb(struct asix *asix)
{
	struct urb *urb;
	struct io_buffer *iobuf;
	int ret = -ENOMEM;

	DBG("Enqueing one URB\n");
	
	iobuf = alloc_iob(ASIX_MTU);
	if (!iobuf)
		goto err_iobuf_malloc;

	urb = usb_alloc_urb();
	if (!urb)
		goto err_usb_malloc;

	usb_fill_bulk_urb(urb, asix->udev, asix->in, iob_put(iobuf, ASIX_MTU), ASIX_MTU);

	ret = usb_submit_urb(urb);
	if (ret < 0)
		goto err_submit_urb;
	
	urb->priv = iobuf;
	list_add_tail(&urb->priv_list, &asix->rx_queue);
	
	return 0;

err_submit_urb:
	usb_free_urb(urb);
err_usb_malloc:
	free_iob(iobuf);
err_iobuf_malloc:

	return ret;
}

int asix_open ( struct net_device *netdev)
{
	enqueue_one_rx_urb(netdev_priv(netdev));
	return 0;
}

int asix_transmit ( struct net_device *netdev __unused,
			  struct io_buffer *iobuf __unused)
{
	struct asix *asix = netdev_priv(netdev);
	uint32_t length;
	struct urb *urb = NULL;
	int status;
	void *buffer;
	int padlen;
	uint32_t packet_len;
	uint32_t pad_bytes = 0xffff0000;
	int ret = -ENOMEM;

	length = iob_len(iobuf);
	padlen = ((length + 4) % 64) ? 0 : 4;

	buffer = malloc_dma(length + padlen + 4, 1);
	if(!buffer)
		goto err_buffer_malloc;

	packet_len = ((length ^ 0x0000ffff) << 16) + (length);
	
	packet_len = cpu_to_le32(packet_len);


	memcpy(buffer, &packet_len, 4);
	memcpy(buffer + 4, iobuf->data, length);

	DBG("pad len = %d\n", padlen);	

	if (padlen)
		memcpy(buffer + length + 4, &pad_bytes, 4);

	length += (4 + padlen);


	urb = usb_alloc_urb();
	if (!urb)
		goto err_alloc_urb;

	usb_fill_bulk_urb (urb, asix->udev, asix->out,
			buffer, length);


	ret = usb_submit_urb (urb);
	
	if (ret < 0)
		goto err_submit_urb;

	urb->priv = iobuf;
	list_add_tail(&urb->priv_list, &asix->tx_queue);

	/* Report successful transmit completions */		
	list_for_each_entry(urb, &asix->tx_queue, priv_list) {
		if ((status = usb_urb_status(urb)) == USB_URB_STATUS_COMPLETE) {
			netdev_tx_complete(netdev, urb->priv);

			list_del(&urb->priv_list);

			free_dma(urb->transfer_buffer, urb->transfer_buffer_length);
			usb_unlink_urb(urb);
			
			DBG("TX DONE\n");
		} else if (status == USB_URB_STATUS_ERROR)
			DBG("TX Error\n");
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


void asix_poll ( struct net_device *netdev) {
	struct asix *asix = netdev_priv(netdev);
	struct urb *urb;
	uint8_t *buffer;
	struct io_buffer *iobuf;
	uint8_t status = 0;
	unsigned int len;
	uint8_t *head;
	uint32_t header;

	list_for_each_entry(urb, &asix->rx_queue, priv_list) {
		if ((status = usb_urb_status(urb)) == USB_URB_STATUS_COMPLETE) {
			if (enqueue_one_rx_urb(asix) < 0)
				DBG("Error enquing packet\n");
			buffer = urb->transfer_buffer;
			head = (uint8_t *) buffer;
			memcpy(&header, head, sizeof(header));
			header = le32_to_cpu(header);
	
			if ((short)(header & 0x0000ffff) !=
			    ~((short)((header & 0xffff0000) >> 16))) {
				DBG("asix_rx_fixup() Bad Header Length");
			}
			/* get the packet length */
			len = (u16) (header & 0x0000ffff);
			iobuf = urb->priv;
			iob_pull(iobuf, 4);
			iob_unput(iobuf, 2048 - (len + 4));
			DBG("len = %d ioblen = %d\n", len, iob_len(iobuf));
			netdev_rx(netdev, iobuf);
	
			list_del(&urb->priv_list);
			usb_unlink_urb(urb);
	mdelay(2);
		}
	}
}

/* asix net device operations */
static struct net_device_operations asix_operations = {
	.open		= asix_open,
	.transmit	= asix_transmit,
	.poll		= asix_poll,
};

static int asix_sw_reset(struct asix *asix, uint8_t flags)
{
	int ret;

        ret = asix_write_cmd(asix, AX_CMD_SW_RESET, flags, 0, 0, NULL);
	if (ret < 0)
		DBG("Failed to send software reset: %02x\n", ret);

	return ret;
}

static int asix_write_rx_ctl(struct asix *asix, uint16_t mode)
{
	int ret;

	DBG("asix_write_rx_ctl() - mode = 0x%04x\n", mode);
	ret = asix_write_cmd(asix, AX_CMD_WRITE_RX_CTL, mode, 0, 0, NULL);
	if (ret < 0)
		DBG("Failed to write RX_CTL mode to 0x%04x: %02x\n",
		       mode, ret);

	return ret;
}

/* Get the PHY Identifier from the PHYSID1 & PHYSID2 MII registers */
static u32 asix_get_phyid(struct asix *asix)
{
	int phy_reg;
	u32 phy_id;

	phy_reg = asix_mdio_read(asix->net, asix_get_phy_addr(asix), MII_PHYSID1);
	if (phy_reg < 0)
		return 0;

	phy_id = (phy_reg & 0xffff) << 16;

	phy_reg = asix_mdio_read(asix->net, asix_get_phy_addr(asix), MII_PHYSID2);
	if (phy_reg < 0)
		return 0;

	phy_id |= (phy_reg & 0xffff);

	return phy_id;
}

static int marvell_phy_init(struct asix *asix)
{
	uint16_t reg;

	DBG("marvell_phy_init()");

	reg = asix_mdio_read(asix->net, asix_get_phy_addr(asix), MII_MARVELL_STATUS);
	DBG("MII_MARVELL_STATUS = 0x%04x\n", reg);

	asix_mdio_write(asix->net, asix_get_phy_addr(asix), MII_MARVELL_CTRL,
			MARVELL_CTRL_RXDELAY | MARVELL_CTRL_TXDELAY);

	return 0;
}

static int mii_nway_restart (struct asix *asix)
{
	int bmcr;
	int r = -EINVAL;

	/* if autoneg is off, it's an error */
	bmcr = asix_mdio_read(asix->net, 0, MII_BMCR);

	if (bmcr & BMCR_ANENABLE) {
		bmcr |= BMCR_ANRESTART;
		asix_mdio_write(asix->net, 0, MII_BMCR, bmcr);
		r = 0;
	}

	return r;
}

int asix_88178_probe(struct usb_device *udev,
			const struct usb_device_id *id __unused)
{
	struct net_device *netdev;
	struct asix *asix;
	unsigned int i;
	uint8_t buf[ETH_ALEN];
	uint16_t eeprom;
	uint8_t status;
	uint32_t phyid;
	int ret;

	netdev = alloc_etherdev(sizeof(*asix));
	netdev_init(netdev, &asix_operations);

	if (!netdev) {
		DBG("can't allocate %s\n\n", "device");
		goto out;
	}

	asix = netdev_priv(netdev);
	INIT_LIST_HEAD(&asix->tx_queue);
	INIT_LIST_HEAD(&asix->rx_done_queue);
	INIT_LIST_HEAD(&asix->rx_queue);

	asix->udev = udev;
	asix->net = netdev;
	netdev->dev = &udev->dev;

	for(i = 0;i < udev->num_endpoints; i++) {
		if (usb_ep_xfertype(udev->endpoints[i]) == USB_ENDPOINT_XFER_BULK &&
				usb_ep_dir(udev->endpoints[i]) == USB_DIR_IN)
			asix->in = udev->endpoints[i];

		if (usb_ep_xfertype(udev->endpoints[i]) == USB_ENDPOINT_XFER_BULK &&
				usb_ep_dir(udev->endpoints[i]) == USB_DIR_OUT)
			asix->out = udev->endpoints[i];
	}
	asix->maxpacket = le16_to_cpu(asix->in->desc.wMaxPacketSize);

	asix_read_cmd(asix, AX_CMD_READ_GPIOS, 0, 0, 1, &status);
	DBG("GPIO Status: 0x%04x\n", status);

	asix_write_cmd(asix, AX_CMD_WRITE_ENABLE, 0, 0, 0, NULL);
	asix_read_cmd(asix, AX_CMD_READ_EEPROM, 0x0017, 0, 2, &eeprom);
	asix_write_cmd(asix, AX_CMD_WRITE_DISABLE, 0, 0, 0, NULL);

	DBG("EEPROM index 0x17 is 0x%04x\n", eeprom);

	asix_write_gpio(asix, AX_GPIO_RSE | AX_GPIO_GPO_1 | AX_GPIO_GPO1EN, 40);
	if ((le16_to_cpu(eeprom) >> 8) != 1) {
		asix_write_gpio(asix, 0x003c, 30);
		asix_write_gpio(asix, 0x001c, 300);
		asix_write_gpio(asix, 0x003c, 30);
	} else {
		DBG("gpio phymode == 1 path");
		asix_write_gpio(asix, AX_GPIO_GPO1EN, 30);
		asix_write_gpio(asix, AX_GPIO_GPO1EN | AX_GPIO_GPO_1, 30);
	}

	asix_sw_reset(asix, 0);
	mdelay(150);

	asix_sw_reset(asix, AX_SWRESET_PRL | AX_SWRESET_IPPD);
	mdelay(150);

	asix_write_rx_ctl(asix, 0);

	/* Get the MAC address */
	if ((ret = asix_read_cmd(asix, AX_CMD_READ_NODE_ID,
				0, 0, ETH_ALEN, buf)) < 0) {
		DBG("Failed to read MAC address: %d\n", ret);
		goto out;
	}
	memcpy(netdev->ll_addr, buf, ETH_ALEN);
	
	phyid = asix_get_phyid(asix);
	DBG("PHYID=0x%08zx\n", phyid);

	if (asix->phy == PHY_MODE_MARVELL) {
		marvell_phy_init(asix);
		mdelay(60);
	}

	asix_mdio_write(asix->net, asix_get_phy_addr(asix), MII_BMCR,
			BMCR_RESET | BMCR_ANENABLE);
	asix_mdio_write(asix->net, asix_get_phy_addr(asix), MII_ADVERTISE,
			ADVERTISE_ALL | ADVERTISE_CSMA | 0x400);
	asix_mdio_write(asix->net, asix_get_phy_addr(asix), 9,
			0x200);

	mii_nway_restart(asix);

	if ((ret = asix_write_medium_mode(asix, AX88178_MEDIUM_DEFAULT)) < 0)
		goto out;

	if ((ret = asix_write_rx_ctl(asix, AX_DEFAULT_RX_CTL)) < 0)
		goto out;
	/* Register network device */
	if ((ret = register_netdev(netdev)) != 0) {
		return -1;
	}

	netdev_link_up(netdev);
	{
		uint8_t status;
		asix_read_cmd(asix, AX_CMD_READ_MONITOR_MODE, 0, 0, 1, &status);
		DBG("statu s= %x\n", status);
	}
	return 0;

out:
	return ret;
}

static u16 asix_read_rx_ctl(struct asix *asix)
{
	uint16_t v;
	int ret = asix_read_cmd(asix, AX_CMD_READ_RX_CTL, 0, 0, 2, &v);

	if (ret < 0) {
		DBG("Error reading RX_CTL register: %02x", ret);
		goto out;
	}
	ret = le16_to_cpu(v);
out:
	return ret;
}

static u16 asix_read_medium_status(struct asix *asix)
{
	uint16_t v;
	int ret = asix_read_cmd(asix, AX_CMD_READ_MEDIUM_STATUS, 0, 0, 2, &v);

	if (ret < 0) {
		DBG("Error reading Medium Status register: %02x", ret);
		goto out;
	}
	ret = le16_to_cpu(v);
out:
	return ret;
}

int asix_88772_probe(struct usb_device *udev,
			const struct usb_device_id *id __unused)
{
	struct net_device *netdev;
	struct asix *asix;
	unsigned int i;
	uint8_t buf[ETH_ALEN];
	uint32_t phyid;
	int ret, embd_phy;
	uint16_t rx_ctl;

	netdev = alloc_etherdev(sizeof(*asix));
	netdev_init(netdev, &asix_operations);

	if (!netdev) {
		DBG("can't allocate %s\n\n", "device");
		goto out;
	}

	asix = netdev_priv(netdev);
	INIT_LIST_HEAD(&asix->tx_queue);
	INIT_LIST_HEAD(&asix->rx_done_queue);
	INIT_LIST_HEAD(&asix->rx_queue);

	asix->udev = udev;
	asix->net = netdev;
	netdev->dev = &udev->dev;

	for(i = 0;i < udev->num_endpoints; i++) {
		if (usb_ep_xfertype(udev->endpoints[i]) == USB_ENDPOINT_XFER_BULK &&
				usb_ep_dir(udev->endpoints[i]) == USB_DIR_IN)
			asix->in = udev->endpoints[i];

		if (usb_ep_xfertype(udev->endpoints[i]) == USB_ENDPOINT_XFER_BULK &&
				usb_ep_dir(udev->endpoints[i]) == USB_DIR_OUT)
			asix->out = udev->endpoints[i];
	}
	asix->maxpacket = le16_to_cpu(asix->in->desc.wMaxPacketSize);

	if ((ret = asix_write_gpio(asix,
			AX_GPIO_RSE | AX_GPIO_GPO_2 | AX_GPIO_GPO2EN, 5)) < 0)
		goto out;

	/* 0x10 is the phy id of the embedded 10/100 ethernet phy */
	embd_phy = ((asix_get_phy_addr(asix) & 0x1f) == 0x10 ? 1 : 0);
	if ((ret = asix_write_cmd(asix, AX_CMD_SW_PHY_SELECT,
				embd_phy, 0, 0, NULL)) < 0) {
		DBG("Select PHY #1 failed: %d", ret);
		goto out;
	}

	if ((ret = asix_sw_reset(asix, AX_SWRESET_IPPD | AX_SWRESET_PRL)) < 0)
		goto out;

	mdelay(150);
	if ((ret = asix_sw_reset(asix, AX_SWRESET_CLEAR)) < 0)
		goto out;

	mdelay(150);
	if (embd_phy) {
		if ((ret = asix_sw_reset(asix, AX_SWRESET_IPRL)) < 0)
			goto out;
	}
	else {
		if ((ret = asix_sw_reset(asix, AX_SWRESET_PRTE)) < 0)
			goto out;
	}

	mdelay(150);
	rx_ctl = asix_read_rx_ctl(asix);
	DBG("RX_CTL is 0x%04x after software reset", rx_ctl);
	if ((ret = asix_write_rx_ctl(asix, 0x0000)) < 0)
		goto out;

	rx_ctl = asix_read_rx_ctl(asix);
	DBG("RX_CTL is 0x%04x setting to 0x0000", rx_ctl);

	/* Get the MAC address */
	if ((ret = asix_read_cmd(asix, AX_CMD_READ_NODE_ID,
				0, 0, ETH_ALEN, buf)) < 0) {
		DBG("Failed to read MAC address: %d", ret);
		goto out;
	}
	memcpy(netdev->ll_addr, buf, ETH_ALEN);

	phyid = asix_get_phyid(asix);
	DBG("PHYID=0x%08zx", phyid);

	if ((ret = asix_sw_reset(asix, AX_SWRESET_PRL)) < 0)
		goto out;

	mdelay(150);

	if ((ret = asix_sw_reset(asix, AX_SWRESET_IPRL | AX_SWRESET_PRL)) < 0)
		goto out;

	mdelay(150);

	asix_mdio_write(asix->net, asix_get_phyid(asix), MII_BMCR, BMCR_RESET);
	asix_mdio_write(asix->net, asix_get_phyid(asix), MII_ADVERTISE,
			ADVERTISE_ALL | ADVERTISE_CSMA);
	
	mii_nway_restart(asix);

	if ((ret = asix_write_medium_mode(asix, AX88772_MEDIUM_DEFAULT)) < 0)
		goto out;

	if ((ret = asix_write_cmd(asix, AX_CMD_WRITE_IPG0,
				AX88772_IPG0_DEFAULT | AX88772_IPG1_DEFAULT,
				AX88772_IPG2_DEFAULT, 0, NULL)) < 0) {
		DBG("Write IPG,IPG1,IPG2 failed: %d", ret);
		goto out;
	}

	/* Set RX_CTL to default values with 2k buffer, and enable cactus */
	if ((ret = asix_write_rx_ctl(asix, AX_DEFAULT_RX_CTL)) < 0)
		goto out;

	rx_ctl = asix_read_rx_ctl(asix);
	DBG("RX_CTL is 0x%04x after all initializations", rx_ctl);

	rx_ctl = asix_read_medium_status(asix);
	DBG("Medium Status is 0x%04x after all initializations", rx_ctl);

	/* Register network device */
	if ((ret = register_netdev(netdev)) != 0) {
		return -1;
	}

	netdev_link_up(netdev);
	{
		uint8_t status;
		asix_read_cmd(asix, AX_CMD_READ_MONITOR_MODE, 0, 0, 1, &status);
		DBG("statu s= %x\n", status);
	}
	return 0;

out:
	return ret;
}

static struct usb_device_id asix_88178_ids[] = {
	USB_ROM(0x1737, 0x0039, "asix", "Linksys USB1000", 0),
	USB_ROM(0x04bb, 0x0939, "asix", "IO-DATA ETG-US2", 0),
	USB_ROM(0x050d, 0x5055, "asix", "Belkin F5D5055", 0),
};

struct usb_driver asix_88178_usb_driver __usb_driver = {
	.ids = asix_88178_ids,
	.id_count = (sizeof(asix_88178_ids) / sizeof(asix_88178_ids[0])),
	.probe = asix_88178_probe,
};


static struct usb_device_id asix_88772_ids[] = {
	USB_ROM(0x17ef, 0x7203, "asix", "Lenovo U2L 100-Y1", 0),
	USB_ROM(0x2001, 0x3c05, "asix", "DLink DUB-E100", 0),
        USB_ROM(0x0b95, 0x772a, "asix", "ASIX AX88772A", 0),
        USB_ROM(0x05ac, 0x1402, "asix", "Apple Inc.", 0),
};

struct usb_driver asix_88772_usb_driver __usb_driver = {
	.ids = asix_88772_ids,
	.id_count = (sizeof(asix_88772_ids) / sizeof(asix_88772_ids[0])),
	.probe = asix_88772_probe,
};
