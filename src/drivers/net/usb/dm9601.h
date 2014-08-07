#include <ipxe/usb/ch9.h>
#include <ipxe/netdevice.h>

#define DM9601_MTU			1522
struct dm9601 {
	struct usb_device 		*udev;

	struct list_head		tx_queue;
	struct list_head		rx_queue;
	struct list_head		rx_done_queue;

	struct usb_host_endpoint 	*in;
	struct usb_host_endpoint 	*out;

	struct net_device *net;
	uint16_t			maxpacket;
};

