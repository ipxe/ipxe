/*
 * Copyright (C) 2016 David Decotigny <ddecotig@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <linux_api.h>
#include <ipxe/list.h>
#include <ipxe/linux.h>
#include <ipxe/malloc.h>
#include <ipxe/device.h>
#include <ipxe/netdevice.h>
#include <ipxe/iobuf.h>
#include <ipxe/ethernet.h>
#include <ipxe/settings.h>
#include <ipxe/socket.h>

/* This hack prevents pre-2.6.32 headers from redefining struct sockaddr */
#define __GLIBC__ 2
#include <linux/socket.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#undef __GLIBC__
#include <byteswap.h>

/* linux-specifc syscall params */
#define LINUX_AF_PACKET 17
#define LINUX_SOCK_RAW 3
#define LINUX_SIOCGIFINDEX 0x8933
#define LINUX_SIOCGIFHWADDR 0x8927

#define RX_BUF_SIZE 1536

/** @file
 *
 * The AF_PACKET driver.
 *
 * Bind to an existing linux network interface.
 */

struct af_packet_nic {
	/** Linux network interface name */
	char * ifname;
	/** Packet socket descriptor */
	int fd;
	/** ifindex */
	int ifindex;
};

/** Open the linux interface */
static int af_packet_nic_open ( struct net_device * netdev )
{
	struct af_packet_nic * nic = netdev->priv;
	struct sockaddr_ll socket_address;
	struct ifreq if_data;
	int ret;

	nic->fd = linux_socket(LINUX_AF_PACKET, LINUX_SOCK_RAW,
			       htons(ETH_P_ALL));
	if (nic->fd < 0) {
		DBGC(nic, "af_packet %p socket(AF_PACKET) = %d (%s)\n",
		     nic, nic->fd, linux_strerror(linux_errno));
		return nic->fd;
	}

	/* resolve ifindex of ifname */
	memset(&if_data, 0, sizeof(if_data));
	strncpy(if_data.ifr_name, nic->ifname, sizeof(if_data.ifr_name));
	ret = linux_ioctl(nic->fd, LINUX_SIOCGIFINDEX, &if_data);
	if (ret < 0) {
		DBGC(nic, "af_packet %p ioctl(SIOCGIFINDEX) = %d (%s)\n",
		     nic, ret, linux_strerror(linux_errno));
		linux_close(nic->fd);
		return ret;
	}

	nic->ifindex = if_data.ifr_ifindex;

	/* bind to interface */
	memset(&socket_address, 0, sizeof(socket_address));
	socket_address.sll_family = LINUX_AF_PACKET;
	socket_address.sll_ifindex = nic->ifindex;
	socket_address.sll_protocol = htons(ETH_P_ALL);
	ret = linux_bind(nic->fd, (void *) &socket_address,
			 sizeof(socket_address));
	if (ret == -1) {
		DBGC(nic, "af_packet %p bind() = %d (%s)\n",
		     nic, ret, linux_strerror(linux_errno));
		linux_close(nic->fd);
		return ret;
	}

	/* Set nonblocking mode to make af_packet_nic_poll() easier */
	ret = linux_fcntl(nic->fd, F_SETFL, O_NONBLOCK);
	if (ret != 0) {
		DBGC(nic, "af_packet %p fcntl(%d, ...) = %d (%s)\n",
		     nic, nic->fd, ret, linux_strerror(linux_errno));
		linux_close(nic->fd);
		return ret;
	}

	return 0;
}

/** Close the packet socket */
static void af_packet_nic_close ( struct net_device *netdev )
{
	struct af_packet_nic * nic = netdev->priv;
	linux_close(nic->fd);
}

/**
 * Transmit an ethernet packet.
 *
 * The packet can be written to the socket and marked as complete immediately.
 */
static int af_packet_nic_transmit ( struct net_device *netdev,
				    struct io_buffer *iobuf )
{
	struct af_packet_nic * nic = netdev->priv;
	struct sockaddr_ll socket_address;
	const struct ethhdr * eh;
	int rc;

	memset(&socket_address, 0, sizeof(socket_address));
	socket_address.sll_family = LINUX_AF_PACKET;
	socket_address.sll_ifindex = nic->ifindex;
	socket_address.sll_halen = ETH_ALEN;

	eh = iobuf->data;
	memcpy(socket_address.sll_addr, eh->h_dest, ETH_ALEN);

	rc = linux_sendto(nic->fd, iobuf->data, iobuf->tail - iobuf->data,
			  0, (struct sockaddr *)&socket_address,
			  sizeof(socket_address));

	DBGC2(nic, "af_packet %p wrote %d bytes\n", nic, rc);
	netdev_tx_complete(netdev, iobuf);

	return 0;
}

/** Poll for new packets */
static void af_packet_nic_poll ( struct net_device *netdev )
{
	struct af_packet_nic * nic = netdev->priv;
	struct pollfd pfd;
	struct io_buffer * iobuf;
	int r;

	pfd.fd = nic->fd;
	pfd.events = POLLIN;
	if (linux_poll(&pfd, 1, 0) == -1) {
		DBGC(nic, "af_packet %p poll failed (%s)\n",
		     nic, linux_strerror(linux_errno));
		return;
	}
	if ((pfd.revents & POLLIN) == 0)
		return;

	/* At this point we know there is at least one new packet to be read */

	iobuf = alloc_iob(RX_BUF_SIZE);
	if (! iobuf)
		goto allocfail;

	while ((r = linux_read(nic->fd, iobuf->data, RX_BUF_SIZE)) > 0) {
		DBGC2(nic, "af_packet %p read %d bytes\n", nic, r);

		iob_put(iobuf, r);
		netdev_rx(netdev, iobuf);

		iobuf = alloc_iob(RX_BUF_SIZE);
		if (! iobuf)
			goto allocfail;
	}

	free_iob(iobuf);
	return;

allocfail:
	DBGC(nic, "af_packet %p alloc_iob failed\n", nic);
}

/**
 * Set irq.
 *
 * Not used on linux, provide a dummy implementation.
 */
static void af_packet_nic_irq ( struct net_device *netdev, int enable )
{
	struct af_packet_nic *nic = netdev->priv;

	DBGC(nic, "af_packet %p irq enable = %d\n", nic, enable);
}


static int af_packet_update_properties ( struct net_device *netdev )
{
	struct af_packet_nic *nic = netdev->priv;
	struct ifreq if_data;
	int ret;

	/* retrieve default MAC address */
	int fd = linux_socket(LINUX_AF_PACKET, LINUX_SOCK_RAW, 0);
	if (fd < 0) {
		DBGC(nic, "af_packet %p cannot create raw socket (%s)\n",
		     nic, linux_strerror(linux_errno));
		return fd;
	}

	/* retrieve host's MAC address */
	memset(&if_data, 0, sizeof(if_data));
	strncpy(if_data.ifr_name, nic->ifname, sizeof(if_data.ifr_name));
	ret = linux_ioctl(fd, LINUX_SIOCGIFHWADDR, &if_data);
	if (ret < 0) {
		DBGC(nic, "af_packet %p cannot get mac addr (%s)\n",
		     nic, linux_strerror(linux_errno));
		linux_close(fd);
		return ret;
	}

	linux_close(fd);
	/* struct sockaddr = { u16 family, u8 pad[14] (equiv. sa_data) }; */
	memcpy(netdev->ll_addr, if_data.ifr_hwaddr.pad, ETH_ALEN);
	return 0;
}

/** AF_PACKET operations */
static struct net_device_operations af_packet_nic_operations = {
	.open		= af_packet_nic_open,
	.close		= af_packet_nic_close,
	.transmit	= af_packet_nic_transmit,
	.poll		= af_packet_nic_poll,
	.irq		= af_packet_nic_irq,
};

/** Handle a device request for the af_packet driver */
static int af_packet_nic_probe ( struct linux_device *device,
				 struct linux_device_request *request )
{
	struct linux_setting *if_setting;
	struct net_device *netdev;
	struct af_packet_nic *nic;
	int rc;

	netdev = alloc_etherdev(sizeof(*nic));
	if (! netdev)
		return -ENOMEM;

	netdev_init(netdev, &af_packet_nic_operations);
	nic = netdev->priv;
	linux_set_drvdata(device, netdev);
	netdev->dev = &device->dev;

	memset(nic, 0, sizeof(*nic));

	/* Look for the mandatory if setting */
	if_setting = linux_find_setting("if", &request->settings);

	/* No if setting */
	if (! if_setting) {
		printf("af_packet missing a mandatory if setting\n");
		rc = -EINVAL;
		goto err_settings;
	}

	nic->ifname = if_setting->value;
	snprintf ( device->dev.name, sizeof ( device->dev.name ), "%s",
		   nic->ifname );
	device->dev.desc.bus_type = BUS_TYPE_TAP;
	af_packet_update_properties(netdev);
	if_setting->applied = 1;

	/* Apply rest of the settings */
	linux_apply_settings(&request->settings, &netdev->settings.settings);

	/* Register network device */
	if ((rc = register_netdev(netdev)) != 0)
		goto err_register;

	netdev_link_up(netdev);

	return 0;

err_settings:
	unregister_netdev(netdev);
err_register:
	netdev_nullify(netdev);
	netdev_put(netdev);
	return rc;
}

/** Remove the device */
static void af_packet_nic_remove ( struct linux_device *device )
{
	struct net_device *netdev = linux_get_drvdata(device);
	unregister_netdev(netdev);
	netdev_nullify(netdev);
	netdev_put(netdev);
}

/** AF_PACKET linux_driver */
struct linux_driver af_packet_nic_driver __linux_driver = {
	.name = "af_packet",
	.probe = af_packet_nic_probe,
	.remove = af_packet_nic_remove,
	.can_probe = 1,
};
