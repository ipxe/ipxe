#include <stdlib.h>
#include <ipxe/usb.h>
#include <stdio.h>

struct urb *usb_alloc_urb(void)
{
	struct urb *urb;
	urb = malloc(sizeof(struct urb));
	INIT_LIST_HEAD(&urb->priv_list);

	return urb;
}

void usb_free_urb(struct urb *urb)
{
	free(urb);
}

int usb_submit_urb(struct urb *urb) {
	return urb->udev->hcd->driver->enqueue_urb(urb->udev->hcd, urb);
}

int usb_urb_status(struct urb *urb)
{
	return urb->udev->hcd->driver->urb_status(urb);
}

void usb_unlink_urb(struct urb *urb)
{
	urb->udev->hcd->driver->unlink_urb(urb);
	free(urb);
}
