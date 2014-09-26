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

#include "pegasus.h"

static int mii_mode = 0;
static const char driver_name[] = "pegasus";

#undef	PEGASUS_WRITE_EEPROM
#define	BMSR_MEDIA	(BMSR_10HALF | BMSR_10FULL | BMSR_100HALF | \
			BMSR_100FULL | BMSR_ANEGCAPABLE)

static struct usb_eth_dev usb_dev_id[] = {
#define	PEGASUS_DEV(pn, vid, pid, flags)	\
	{.name = pn, .vendor = vid, .device = pid, .private = flags },
#include "pegasus.h"
#undef	PEGASUS_DEV
	{NULL, 0, 0, 0},
	{NULL, 0, 0, 0}
};

static struct usb_device_id pegasus_ids[] = {
USB_ROM(0x0506, 0x4601, "pegasus", "3Com USB Ethernet 3C460B", 0 ), 
USB_ROM(0x0557, 0x2007, "pegasus", "ATEN USB Ethernet UC-110T", 0 ),
USB_ROM(0x07b8, 0x110c, "pegasus", "USB HPNA/Ethernet", 0 ),
USB_ROM(0x07b8, 0x4104, "pegasus", "USB HPNA/Ethernet", 0 ),
USB_ROM(0x07b8, 0x4004, "pegasus", "USB HPNA/Ethernet", 0 ),
USB_ROM(0x07b8, 0x4007, "pegasus", "USB HPNA/Ethernet", 0 ),
USB_ROM(0x07b8, 0x4102, "pegasus", "USB 10/100 Fast Ethernet", 0 ),
USB_ROM(0x07b8, 0x4002, "pegasus", "USB 10/100 Fast Ethernet", 0 ),
USB_ROM(0x07b8, 0x400b, "pegasus", "USB 10/100 Fast Ethernet", 0 ),
USB_ROM(0x07b8, 0x400c, "pegasus", "USB 10/100 Fast Ethernet", 0 ),
USB_ROM(0x07b8, 0xabc1, "pegasus", "USB 10/100 Fast Ethernet", 0 ),
USB_ROM(0x07b8, 0x200c, "pegasus", "USB 10/100 Fast Ethernet", 0 ),
USB_ROM(0x083a, 0x1046, "pegasus", "Accton USB 10/100 Ethernet Adapter", 0 ),
USB_ROM(0x083a, 0x5046, "pegasus", "SpeedStream USB 10/100 Ethernet", 0 ),
USB_ROM(0x083a, 0xb004, "pegasus", "Philips USB 10/100 Ethernet", 0 ),
USB_ROM(0x07a6, 0x8511, "pegasus", "ADMtek ADM8511 USB Ethernet", 0 ),
USB_ROM(0x07a6, 0x8513, "pegasus", "ADMtek ADM8513 USB Ethernet", 0 ),
USB_ROM(0x07a6, 0x8515, "pegasus", "ADMtek ADM8515 USB-2.0 Ethernet", 0 ),
USB_ROM(0x07a6, 0x0986, "pegasus", "ADMtek AN986 USB Ethernet", 0 ),
USB_ROM(0x07a6, 0x07c2, "pegasus", "ADMtek AN986A USB MAC", 0 ),
USB_ROM(0x3334, 0x1701, "pegasus", "AEI USB Fast Ethernet Adapter", 0 ),
USB_ROM(0x07c9, 0xb100, "pegasus", "Allied Telesyn Int. AT-USB100", 0 ),
USB_ROM(0x050d, 0x0121, "pegasus", "Belkin F5D5050 USB Ethernet", 0 ),
USB_ROM(0x08dd, 0x0986, "pegasus", "Billionton USB-100", 0 ),
USB_ROM(0x08dd, 0x0987, "pegasus", "Billionton USBLP-100", 0 ),
USB_ROM(0x049f, 0x8511, "pegasus", "iPAQ Networking 10/100 USB", 0 ),
USB_ROM(0x08dd, 0x0988, "pegasus", "Billionton USBEL-100", 0 ),
USB_ROM(0x08dd, 0x8511, "pegasus", "Billionton USBE-100", 0 ),
USB_ROM(0x07aa, 0x0004, "pegasus", "Corega FEther USB-TX", 0 ),
USB_ROM(0x07aa, 0x000d, "pegasus", "Corega FEther USB-TXS", 0 ),
USB_ROM(0x2001, 0x4001, "pegasus", "D-Link DSB-650TX", 0 ),
USB_ROM(0x2001, 0x4002, "pegasus", "D-Link DSB-650TX", 0 ),
USB_ROM(0x2001, 0x4102, "pegasus", "D-Link DSB-650TX", 0 ),
USB_ROM(0x2001, 0x400b, "pegasus", "D-Link DSB-650TX", 0 ),
USB_ROM(0x2001, 0x200c, "pegasus", "D-Link DSB-650TX", 0 ),
USB_ROM(0x2001, 0x4003, "pegasus", "D-Link DSB-650TX", 0 ),
USB_ROM(0x2001, 0xabc1, "pegasus", "D-Link DSB-650", 0 ),
USB_ROM(0x0db7, 0x0002, "pegasus", "GOLDPFEIL USB Adapter", 0 ),
USB_ROM(0x056e, 0x4010, "pegasus", "ELECOM USB Ethernet LD-USB20", 0 ),
USB_ROM(0x1342, 0x0304, "pegasus", "EasiDock Ethernet", 0 ),
USB_ROM(0x05cc, 0x3000, "pegasus", "Elsa Micolink USB2Ethernet", 0 ),
USB_ROM(0x1044, 0x8002, "pegasus", "GIGABYTE GN-BR402W Wireless Router", 0 ),
USB_ROM(0x0e66, 0x400c, "pegasus", "Hawking UF100 10/100 Ethernet", 0 ),
USB_ROM(0x03f0, 0x811c, "pegasus", "HP hn210c Ethernet USB", 0 ),
USB_ROM(0x04bb, 0x0904, "pegasus", "IO DATA USB ET/TX", 0 ),
USB_ROM(0x04bb, 0x0913, "pegasus", "IO DATA USB ET/TX-S", 0 ),
USB_ROM(0x0951, 0x000a, "pegasus", "Kingston KNU101TX Ethernet", 0 ),
USB_ROM(0x056e, 0x4002, "pegasus", "LANEED USB Ethernet LD-USB/TX", 0 ),
USB_ROM(0x056e, 0x4005, "pegasus", "LANEED USB Ethernet LD-USBL/TX", 0 ),
USB_ROM(0x056e, 0x400b, "pegasus", "LANEED USB Ethernet LD-USB/TX", 0 ),
USB_ROM(0x056e, 0xabc1, "pegasus", "LANEED USB Ethernet LD-USB/T", 0 ),
USB_ROM(0x056e, 0x200c, "pegasus", "LANEED USB Ethernet LD-USB/TX", 0 ),
USB_ROM(0x066b, 0x2202, "pegasus", "Linksys USB10TX", 0 ),
USB_ROM(0x066b, 0x2203, "pegasus", "Linksys USB100TX", 0 ),
USB_ROM(0x066b, 0x2204, "pegasus", "Linksys USB100TX", 0 ),
USB_ROM(0x066b, 0x2206, "pegasus", "Linksys USB10T Ethernet Adapter", 0 ),
USB_ROM(0x077b, 0x08b4, "pegasus", "Linksys USBVPN1", 0 ),
USB_ROM(0x066b, 0x400b, "pegasus", "Linksys USB USB100TX", 0 ),
USB_ROM(0x066b, 0x200c, "pegasus", "Linksys USB10TX", 0 ),
USB_ROM(0x0411, 0x0001, "pegasus", "MELCO/BUFFALO LUA-TX", 0 ),
USB_ROM(0x0411, 0x0005, "pegasus", "MELCO/BUFFALO LUA-TX", 0 ),
USB_ROM(0x0411, 0x0009, "pegasus", "MELCO/BUFFALO LUA2-TX", 0 ),
USB_ROM(0x045e, 0x007a, "pegasus", "Microsoft MN-110", 0 ),
USB_ROM(0x0846, 0x1020, "pegasus", "NETGEAR FA101", 0 ),
USB_ROM(0x0b39, 0x0109, "pegasus", "OCT Inc.", 0 ),
USB_ROM(0x0b39, 0x0901, "pegasus", "OCT USB TO Ethernet", 0 ),
USB_ROM(0x08d1, 0x0003, "pegasus", "smartNIC 2 PnP Adapter", 0 ),
USB_ROM(0x0707, 0x0200, "pegasus", "SMC 202 USB Ethernet", 0 ),
USB_ROM(0x0707, 0x0201, "pegasus", "SMC 2206 USB Ethernet", 0 ),
USB_ROM(0x15e8, 0x9100, "pegasus", "SOHOware NUB100 Ethernet", 0 ),
USB_ROM(0x15e8, 0x9110, "pegasus", "SOHOware NUB110 Ethernet", 0 ),
USB_ROM(0x067c, 0x1001, "pegasus", "SpeedStream USB 10/100 Ethernet", 0 ),
};



static int get_registers(pegasus_t * pegasus, uint16_t indx, uint16_t size,
			 void *data)
{

	return usb_control_msg(pegasus->udev, &pegasus->udev->ep_0_in, PEGASUS_REQ_GET_REGS | USB_DIR_IN,
			PEGASUS_REQT_READ, 0, indx, data, size);
}

static int set_registers(pegasus_t * pegasus, uint16_t indx, uint16_t size,
			 void *data)
{

	return usb_control_msg(pegasus->udev, &pegasus->udev->ep_0_out, PEGASUS_REQ_SET_REGS,
			PEGASUS_REQT_WRITE | USB_DIR_OUT, 0, indx, data, size);
}

static int set_register(pegasus_t * pegasus, uint16_t indx, uint8_t data)
{


	usb_control_msg(pegasus->udev, &pegasus->udev->ep_0_out, PEGASUS_REQ_SET_REG,
			PEGASUS_REQT_WRITE, data, indx, NULL, 1);
	return 0;
}

/* Returns 0 on success, error on failure */
static int read_mii_word(pegasus_t * pegasus, uint8_t phy, uint8_t indx, uint16_t * regd)
{
	int i;
	uint8_t data[4] = { phy, 0, 0, indx };
	uint16_t regdi;
	int ret =  -ETIMEDOUT;

	set_register(pegasus, PhyCtrl, 0);
	set_registers(pegasus, PhyAddr, sizeof (data), data);
	set_register(pegasus, PhyCtrl, (indx | PHY_READ));
	for (i = 0; i < REG_TIMEOUT; i++) {
		ret = get_registers(pegasus, PhyCtrl, 1, data);
		if (data[0] & PHY_DONE)
			break;
	}
	if (i < REG_TIMEOUT) {
		ret = get_registers(pegasus, PhyData, 2, &regdi);
		*regd = le16_to_cpu(regdi);
		return ret;
	}

	DBG("%s failed\n", __FUNCTION__);
	return ret;
}


static int write_mii_word(pegasus_t * pegasus, uint8_t phy, uint8_t indx, uint16_t regd)
{
	int i;
	uint8_t data[4] = { phy, 0, 0, indx };
	int ret;

	data[1] = (u8) regd;
	data[2] = (u8) (regd >> 8);
	set_register(pegasus, PhyCtrl, 0);
	set_registers(pegasus, PhyAddr, sizeof(data), data);
	set_register(pegasus, PhyCtrl, (indx | PHY_WRITE));
	for (i = 0; i < REG_TIMEOUT; i++) {
		ret = get_registers(pegasus, PhyCtrl, 1, data);
		if (data[0] & PHY_DONE)
			break;
	}
	if (i < REG_TIMEOUT)
		return ret;

	DBG("%s failed\n", __FUNCTION__);
	return -ETIMEDOUT;
}

static int enable_net_traffic(struct net_device *dev)
{
	uint16_t linkpart;
	uint8_t data[4];
	pegasus_t *pegasus = netdev_priv(dev);
	int ret;

	read_mii_word(pegasus, pegasus->phy, MII_LPA, &linkpart);
	data[0] = 0xc9;
	data[1] = 0;
	if (linkpart & (ADVERTISE_100FULL | ADVERTISE_10FULL))
		data[1] |= 0x20;	/* set full duplex */
	if (linkpart & (ADVERTISE_100FULL | ADVERTISE_100HALF))
		data[1] |= 0x10;	/* set 100 Mbps */
	if (mii_mode)
		data[1] = 0;
	data[2] = 0x01;
	ret = set_registers(pegasus, EthCtrl0, 3, data);

	if (usb_dev_id[pegasus->dev_index].vendor == VENDOR_LINKSYS ||
	    usb_dev_id[pegasus->dev_index].vendor == VENDOR_LINKSYS2 ||
	    usb_dev_id[pegasus->dev_index].vendor == VENDOR_DLINK) {
		u16 auxmode;
		read_mii_word(pegasus, 0, 0x1b, &auxmode);
		write_mii_word(pegasus, 0, 0x1b, auxmode | 4);
	}

	return ret;
}

static int enqueue_one_rx_urb(pegasus_t *pegasus)
{
	struct urb *urb;
	struct io_buffer *iobuf;
	int ret = -ENOMEM;

	DBG("Enqueing one URB\n");
	
	iobuf = alloc_iob(PEGASUS_MTU);
	if (!iobuf)
		goto err_iobuf_malloc;

	urb = usb_alloc_urb();
	if (!urb)
		goto err_usb_malloc;

	usb_fill_bulk_urb(urb, pegasus->udev, pegasus->in, iob_put(iobuf, PEGASUS_MTU), PEGASUS_MTU);

	ret = usb_submit_urb(urb);
	if (ret < 0)
		goto err_submit_urb;
	
	urb->priv = iobuf;
	list_add_tail(&urb->priv_list, &pegasus->rx_queue);
	
	return 0;

err_submit_urb:
	usb_free_urb(urb);
err_usb_malloc:
	free_iob(iobuf);
err_iobuf_malloc:

	return ret;
}

int pegasus_open ( struct net_device *netdev __unused)
{
	pegasus_t *pegasus = netdev_priv(netdev);
	
	if (enable_net_traffic(netdev) < 0)
		DBG("Error enabling pegasus\n");

	if(set_registers(pegasus, EthID, 6, netdev->ll_addr) < 0)
		return -ENODEV;

	return enqueue_one_rx_urb(pegasus);
}

int pegasus_transmit ( struct net_device *netdev,
			  struct io_buffer *iobuf) {

	pegasus_t *pegasus = netdev_priv(netdev);
	int length;
	struct urb *urb = NULL;
	int status;
	void *buffer;
	int ret = -ENOMEM;

	length = iob_len(iobuf);

	if ((length % pegasus->maxpacket) == 0)
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
	if ((length % pegasus->maxpacket) == 0) {
		((char *)buffer)[length] = 0;
		length++;
	}

	usb_fill_bulk_urb (urb, pegasus->udev, pegasus->out,
			buffer, length);

	ret = usb_submit_urb (urb);
	
	if (ret < 0)
		goto err_submit_urb;

	urb->priv = iobuf;
	list_add_tail(&urb->priv_list, &pegasus->tx_queue);

	/* Report successful transmit completions */		
	list_for_each_entry(urb, &pegasus->tx_queue, priv_list) {
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

void pegasus_poll(struct net_device *netdev) {
	pegasus_t *pegasus;
	struct urb *urb;
	uint8_t *buffer;
	struct io_buffer *iobuf;
	uint8_t status = 0;
	int count, rx_status;
	uint16_t pkt_len;
	
	pegasus = netdev_priv(netdev);
	
	/* Check for RX */
	list_for_each_entry(urb, &pegasus->rx_queue, priv_list) {
	if ((status = usb_urb_status(urb)) == USB_URB_STATUS_COMPLETE) {
		if (enqueue_one_rx_urb(pegasus) < 0)
			DBG("Error enquing packet\n");
		
		buffer = urb->transfer_buffer;
		iobuf = urb->priv;

		count = urb->actual_length;
		rx_status = buffer[count - 2];


		if (rx_status & 0x1e) {
			DBG("%s: RX packet error %x\n",	netdev->name, rx_status);
			usb_unlink_urb(urb);
			return;
		}
		if (pegasus->chip == 0x8513) {
			pkt_len = le32_to_cpu(*(uint32_t *)buffer);
			pkt_len &= 0x0fff;
			iobuf->data += 2;
		} else {
			pkt_len = buffer[count - 3] << 8;
			pkt_len += buffer[count - 4];
			pkt_len &= 0xfff;
			pkt_len -= 8;
		}
		
		iob_unput(iobuf, PEGASUS_MTU - pkt_len);
		DBG("RX Done\n");
		netdev_rx(netdev, iobuf);

		list_del(&urb->priv_list);
		usb_unlink_urb(urb);
		}
	}

}

/* pegasus net device operations */
static struct net_device_operations pegasus_operations = {
	.open		= pegasus_open,
	.transmit	= pegasus_transmit,
	.poll		= pegasus_poll,
};


static int read_eprom_word(pegasus_t * pegasus, uint8_t index, uint16_t * retdata)
{
	int i;
	uint8_t tmp;
	uint16_t retdatai;
	int ret;

	set_register(pegasus, EpromCtrl, 0);
	set_register(pegasus, EpromOffset, index);
	set_register(pegasus, EpromCtrl, EPROM_READ);

	for (i = 0; i < REG_TIMEOUT; i++) {
		ret = get_registers(pegasus, EpromCtrl, 1, &tmp);
		if (tmp & EPROM_DONE)
			break;
	}
	if (i < REG_TIMEOUT) {
		ret = get_registers(pegasus, EpromData, 2, &retdatai);
		*retdata = le16_to_cpu(retdatai);
		return ret;
	}

	return -ETIMEDOUT;
}


static inline void get_node_id(pegasus_t * pegasus, uint8_t * id)
{
	int i;
	uint16_t w16 = 0;

	for (i = 0; i < 3; i++) {
		read_eprom_word(pegasus, i, &w16);
		((uint16_t *) id)[i] = cpu_to_le16(w16);
	}
}

static void set_ethernet_addr(pegasus_t * pegasus)
{
	uint8_t node_id[6];

	if (pegasus->features & PEGASUS_II) {
		get_registers(pegasus, 0x10, sizeof(node_id), node_id);
	} else {
		get_node_id(pegasus, node_id);
		set_registers(pegasus, EthID, sizeof (node_id), node_id);
	}
	memcpy(pegasus->net->ll_addr, node_id, sizeof (node_id));
}

static inline int reset_mac(pegasus_t * pegasus)
{
	uint8_t data = 0x8;
	int i;

	set_register(pegasus, EthCtrl1, data);
	for (i = 0; i < REG_TIMEOUT; i++) {
		get_registers(pegasus, EthCtrl1, 1, &data);
		if (~data & 0x08) {
			if (mii_mode && (pegasus->features & HAS_HOME_PNA))
				set_register(pegasus, Gpio1, 0x34);
			else
				set_register(pegasus, Gpio1, 0x26);
			set_register(pegasus, Gpio0, pegasus->features);
			set_register(pegasus, Gpio0, DEFAULT_GPIO_SET);
			break;
		}
	}
	if (i == REG_TIMEOUT)
		return -ETIMEDOUT;

	if (usb_dev_id[pegasus->dev_index].vendor == VENDOR_LINKSYS ||
	    usb_dev_id[pegasus->dev_index].vendor == VENDOR_DLINK) {
		set_register(pegasus, Gpio0, 0x24);
		set_register(pegasus, Gpio0, 0x26);
	}
	if (usb_dev_id[pegasus->dev_index].vendor == VENDOR_ELCON) {
		uint16_t auxmode;
		read_mii_word(pegasus, 3, 0x1b, &auxmode);
		write_mii_word(pegasus, 3, 0x1b, auxmode | 4);
	}

	return 0;
}

static uint8_t mii_phy_probe(pegasus_t * pegasus)
{
	int i;
	uint16_t tmp;

	return 0xff;

	for (i = 0; i < 32; i++) {
		read_mii_word(pegasus, i, MII_BMSR, &tmp);
		if (tmp == 0 || tmp == 0xffff || (tmp & BMSR_MEDIA) == 0)
			continue;
		else
			return i;
	}

	return 0xff;
}

static inline void setup_pegasus_II(pegasus_t * pegasus)
{
	uint8_t data = 0xa5;
	
	set_register(pegasus, Reg1d, 0);
	set_register(pegasus, Reg7b, 1);
	mdelay(100);
	if ((pegasus->features & HAS_HOME_PNA) && mii_mode)
		set_register(pegasus, Reg7b, 0);
	else
		set_register(pegasus, Reg7b, 2);

	set_register(pegasus, 0x83, data);
	get_registers(pegasus, 0x83, 1, &data);

	if (data == 0xa5) {
		pegasus->chip = 0x8513;
	} else {
		pegasus->chip = 0;
	}

	set_register(pegasus, 0x80, 0xc0);
	set_register(pegasus, 0x83, 0xff);
	set_register(pegasus, 0x84, 0x01);
	
	if (pegasus->features & HAS_HOME_PNA && mii_mode)
		set_register(pegasus, Reg81, 6);
	else
		set_register(pegasus, Reg81, 2);
}

int pegasus_probe(struct usb_device *udev,
			const struct usb_device_id *id)
{
	struct net_device *netdev;
	pegasus_t *pegasus;
	int dev_index = id - pegasus_ids;
	int res = -ENOMEM;
	unsigned int i;

	netdev = alloc_etherdev(sizeof(*pegasus));
	netdev_init(netdev, &pegasus_operations);

	if (!netdev) {
		DBG("can't allocate %s\n", "device");
		goto out;
	}

	pegasus = netdev_priv(netdev);
	INIT_LIST_HEAD(&pegasus->tx_queue);
	INIT_LIST_HEAD(&pegasus->rx_done_queue);
	INIT_LIST_HEAD(&pegasus->rx_queue);
	
	pegasus->dev_index = dev_index;

	pegasus->udev = udev;
	pegasus->net = netdev;

	netdev->dev = &udev->dev;

	for(i = 0;i < udev->num_endpoints; i++) {
		if (usb_ep_xfertype(udev->endpoints[i]) == USB_ENDPOINT_XFER_BULK &&
				usb_ep_dir(udev->endpoints[i]) == USB_DIR_IN)
			pegasus->in = udev->endpoints[i];

		if (usb_ep_xfertype(udev->endpoints[i]) == USB_ENDPOINT_XFER_BULK &&
				usb_ep_dir(udev->endpoints[i]) == USB_DIR_OUT)
			pegasus->out = udev->endpoints[i];
	}

	if (!pegasus->in || !pegasus->out) {
		return -ENODEV;
	}

	pegasus->maxpacket = le16_to_cpu(pegasus->in->desc.wMaxPacketSize);

	pegasus->features = usb_dev_id[dev_index].private;
	
	if (reset_mac(pegasus)) {
		DBG("can't reset MAC\n");
		res = -EIO;
		goto out;
	}
	set_ethernet_addr(pegasus);
	
	if (pegasus->features & PEGASUS_II) {
		DBG("setup Pegasus II specific registers\n");
		setup_pegasus_II(pegasus);
	}
	pegasus->phy = mii_phy_probe(pegasus);
	if (pegasus->phy == 0xff) {
		DBG("can't locate MII phy, using default\n");
		pegasus->phy = 1;
	}
	
	res = register_netdev(netdev);
	if (res)
		goto out;
	
	DBG("%s, %s,", netdev->name, usb_dev_id[dev_index].name);
	
	DBG("pegasus MAC : %02x", netdev->ll_addr[0]);
	for(i=1;i<6;i++)
		DBG(":%02x", netdev->ll_addr[i]);
	DBG("\n");

	netdev_link_up(netdev);	
	return 0;
out:
	return res;
}

struct usb_driver pegasus_usb_driver  __usb_driver = {
	.ids = pegasus_ids,
	.id_count = (sizeof(pegasus_ids) / sizeof(pegasus_ids[0])),
	.probe = pegasus_probe,
};

