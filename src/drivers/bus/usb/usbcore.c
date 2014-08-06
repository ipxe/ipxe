#include <errno.h>
#include <stdio.h>
#include <ipxe/tables.h>
#include <stdlib.h>
#include <ipxe/usb.h>
#include <little_bswap.h>
#include <ipxe/usb/ch9.h>

/* USB Device number. Starts from 2 */
static unsigned int usb_devnum = 2;

/**
 * Probe a USB device
 *
 * @v usb		USB device
 * @ret rc		Return status code
 *
 * Searches for a driver for the USB device.  If a driver is found,
 * its probe() routine is called.
 */
int usb_probe (struct usb_device *udev) {
	struct usb_driver *driver;
	struct usb_device_id *id;
	unsigned int i, vendor, device;
	int rc;

	vendor = udev->descriptor.idVendor;
	device = udev->descriptor.idProduct;

	DBG ("Adding,  USB device %04x:%04x\n", vendor, device);
	
        for_each_table_entry ( driver, USB_DRIVERS ) {
		for (i = 0; i < driver->id_count; i++ ) {
			id = &driver->ids[i];
			if ((id->vendor != 0xffff) &&
			     (id->vendor != vendor))
				continue;
			if (( id->device != 0xffff ) &&
			     ( id->device != device ))
				continue;
			udev->driver = driver;
			udev->driver_name = id->name;
			snprintf(udev->dev.name, sizeof(udev->dev.name), "%s", id->name);
			DBG ("...using driver %s\n", udev->driver_name);
			if ((rc = driver->probe(udev, id )) != 0) {
				DBG ("......probe failed\n");
				continue;
			}
			return 0;
		}
	}

	DBG ("...no driver found\n");
	return -ENOTTY;
}

int usb_set_address(struct usb_device *udev, int devnum)
{
	int ret;

	ret = usb_control_msg(udev, &udev->ep_0_out,
		USB_REQ_SET_ADDRESS, 0, devnum, 0,
		NULL, 0);
	udev->devnum = devnum;

	return ret;
}

struct usb_device *usb_alloc_dev(void)
{
	struct usb_device *udev;

	udev = zalloc(sizeof(*udev));
	if (!udev) {
		DBG("Could not allocate memory for USB device\n");
		goto err_usb_device_malloc;
	}

	/* Default Address */
	udev->devnum = 0;

	/* Approximate a safe low value for endpoint zero's wMaxPacketSize and
	 * also encode its direction.
	 *
	 * This will be updated to the right value after GET DEVICE DESCRIPTOR
	 */
	udev->ep_0_in.desc.wMaxPacketSize = cpu_to_le32(8);
	udev->ep_0_in.desc.bEndpointAddress = (uint8_t) (1 << 7);
	udev->ep_0_in.desc.bLength = USB_DT_ENDPOINT_SIZE;
	udev->ep_0_in.desc.bDescriptorType = USB_DT_ENDPOINT;

	udev->ep_0_out.desc.bLength = USB_DT_ENDPOINT_SIZE;
	udev->ep_0_out.desc.bDescriptorType = USB_DT_ENDPOINT;

	INIT_LIST_HEAD(&udev->list);

	return udev;

	free(udev);
err_usb_device_malloc:

	return NULL;
}

void usb_free_dev(struct usb_device *udev)
{
	int n = udev->num_endpoints;
	/* Go through the list of endpoints and free them */
	for ( ;n != 0; n--)
		free(udev->endpoints[n]);

	free(udev);
}

int usb_dev_init(struct usb_device *udev, int port)
{
	struct usb_hcd *hcd = udev->hcd;
	int ret;

	/* Reset the port for a period of 50 msec. This will
	 * the device into proper speed.
	 */
	if ((ret = hcd->driver->reset_port(hcd, port)) < 0)
		return -1;
		
	if ((ret = usb_get_device_descriptor(udev, USB_DT_DEVICE_SIZE)) < 0) {
		DBG("USB : Error getting device descriptor\n");
		return -1;
	}
	
	if ((ret = usb_set_address(udev, usb_devnum++)) < 0) {
		DBG("USB : Error setting device address\n");
		return -1;
	}
	
	if((ret = usb_get_configuration(udev)) < 0) {
		DBG("USB : Error getting configuration\n");
		return -1;
	}
	
	if ((ret = usb_set_configuration(udev, 1)) < 0) {
		DBG("USB :Error setting configuration number to 1\n");
		return -1;
	}
	
	if ((ret = usb_probe(udev)) >= 0)
		list_add_tail(&udev->hcd->udev_list, &udev->list);

	return ret;
}

void usb_hcd_remove_all_devices(struct usb_hcd *hcd)
{
	struct usb_device *udev;

	list_for_each_entry(udev, &hcd->udev_list, list) {
		if (udev->driver->remove)
			udev->driver->remove(udev);
	}
}
