#include <errno.h>
#include <ipxe/usb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ipxe/malloc.h>
#include <stdio.h>
#include <little_bswap.h>

#define URB_WAIT_TIMEOUT 150
static int usb_start_wait_urb(struct urb *urb)
{
	int retval;
	int timeout = URB_WAIT_TIMEOUT;
	retval = usb_submit_urb(urb);
	if (retval < 0)
		goto out;

	do {
		mdelay(10);
	} while (((retval = usb_urb_status(urb)) == USB_URB_STATUS_INPROGRESS) && (timeout--));
	if (timeout < 0)
		retval = -1;
out:	
	usb_free_urb(urb);
	return retval;
}

int usb_control_msg(struct usb_device *udev, struct usb_host_endpoint *ep, uint8_t request,
		    uint8_t requesttype, uint16_t value, uint16_t index, void * data,
		    uint16_t size)
{
	struct usb_ctrlrequest *dr;
	struct urb *urb;
	int ret = -ENOMEM;

	dr = malloc_dma(sizeof(*dr), 16);
	if (!dr)
		goto err_dr_malloc;

	dr->bRequestType = requesttype;
	dr->bRequest = request;
	dr->wValue = cpu_to_le16(value);
	dr->wIndex = cpu_to_le16(index);
	dr->wLength = cpu_to_le16(size);

	urb = usb_alloc_urb();
	if (!urb)
		goto err_urb_malloc;

	usb_fill_control_urb(urb, udev, ep, dr, data,
			     size);
	ret = usb_start_wait_urb(urb);
	return ret;

	usb_free_urb(urb);
err_urb_malloc:
	free_dma(dr, sizeof(*dr));
err_dr_malloc:
	return ret;
}

int usb_get_descriptor(struct usb_device *udev, unsigned char type,
		       unsigned char index, void *buf, int size)
{
	memset(buf, 0, size);
	return usb_control_msg(udev, &udev->ep_0_in,USB_REQ_GET_DESCRIPTOR , USB_DIR_IN,
					 (type << 8) + index, 0, buf, size);
}

int usb_get_device_descriptor(struct usb_device *udev, unsigned int size)
{
	int ret = -ENOMEM;

	if (size > sizeof(udev->descriptor))
		return -EINVAL;

	ret = usb_get_descriptor(udev, USB_DT_DEVICE, 0, &udev->descriptor, size);
	if (ret >= 0) {
		udev->ep_0_in.desc.wMaxPacketSize = udev->descriptor.bMaxPacketSize0;
		udev->ep_0_out.desc.wMaxPacketSize = udev->descriptor.bMaxPacketSize0;
	}

	return ret;
}

int usb_set_configuration(struct usb_device *udev, int conf)
{
	return usb_control_msg(udev, &udev->ep_0_out, USB_REQ_SET_CONFIGURATION,
			USB_DIR_OUT,conf, 0, NULL, 0);
}

int usb_get_configuration(struct usb_device *udev)
{
	char buffer[255];
	char *buf1;
	struct usb_config_descriptor *cdesc;
	struct usb_interface_descriptor *idesc;
	int ep_count, i;
	struct usb_host_endpoint *ep;
	int ret;

	buf1 = buffer;
	ret = usb_get_descriptor(udev, USB_DT_CONFIG, 0, buffer, sizeof(buffer));
	if (ret < 0)
		return ret;

	cdesc = (struct usb_config_descriptor *)buf1;
	buf1+= cdesc->bLength;

	idesc = (struct usb_interface_descriptor *)buf1;
	ep_count = idesc->bNumEndpoints;
	buf1+= idesc->bLength;

	for (i = 0; i < ep_count; i++) {

		ep = zalloc(sizeof(*ep));
		if (!ep)
			return -ENOMEM;
		memcpy(&ep->desc, buf1, sizeof(ep->desc));
		buf1+= ep->desc.bLength;
		if (usb_ep_xfertype(ep) != USB_ENDPOINT_XFER_CONTROL &&
				usb_ep_xfertype(ep) != USB_ENDPOINT_XFER_BULK )
			continue;

		DBG("Detected EP bEndPointAddress = %x wMaxPacketSize = %x type = %s\n",
				ep->desc.bEndpointAddress, ep->desc.wMaxPacketSize,
				 (usb_ep_xfertype(ep) == USB_ENDPOINT_XFER_BULK) ? "Bulk":"Int");
		udev->endpoints[udev->num_endpoints++] = ep;
	}

	return 0;
}
