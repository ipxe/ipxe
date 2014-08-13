#include <ipxe/usb/ch9.h>
#include <ipxe/tables.h>
#include <ipxe/device.h>
#include <ipxe/io.h>

/*
 * USB_ROM is used to build up entries in a struct usb_id array.  It
 * is also parsed by parserom.pl to generate Makefile rules and files
 * for rom-o-matic.
 */
#define USB_ID( _vendor, _device, _name, _description, _data ) {        \
        .vendor = _vendor,                                              \
        .device = _device,                                              \
        .name = _name,                                                  \
        .driver_data = _data                                            \
}
#define USB_ROM( _vendor, _device, _name, _description, _data ) \
        USB_ID( _vendor, _device, _name, _description, _data )

/** USB driver table **/
#define USB_DRIVERS __table ( struct usb_driver, "usb_driver" )

/** Declare a USB driver */
#define __usb_driver __table_entry ( USB_DRIVERS, 01 )


#define USB_URB_STATUS_INPROGRESS	1
#define USB_URB_STATUS_COMPLETE		0
#define USB_URB_STATUS_ERROR		-1

struct usb_host_endpoint {
	struct usb_endpoint_descriptor	desc;

	void *hcpriv;
};

static inline unsigned int usb_ep_num(struct usb_host_endpoint *ep)
{
	return ep->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
}

static inline unsigned int  usb_ep_dir(struct usb_host_endpoint *ep)
{
	return ep->desc.bEndpointAddress & USB_ENDPOINT_DIR_MASK;
}

static inline unsigned int usb_ep_xfertype(struct usb_host_endpoint *ep)
{
	return ep->desc.bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
}

struct usb_host_interface {
	struct usb_interface_descriptor	desc;
	struct usb_host_endpoint *endpoint;
};

struct usb_host_config {
	struct usb_config_descriptor desc;
};

struct usb_hcd {
	struct hc_driver	*driver;

	unsigned long 		res_addr; /* Memory/IO resource base */
	unsigned long		res_size; /* Memory/IO resource size */

	void			*hcpriv;
	struct list_head	udev_list;
};


struct usb_device_id {
	/** Name */
	const char *name;
	/** vendor ID */
	uint16_t vendor;
	/** device ID */
	uint16_t device;
        /** Arbitrary driver data */
        unsigned long driver_data;
};

struct usb_device {
	struct device			dev;

	unsigned int 			devnum;		/* Device address */

	enum usb_device_speed		speed;
	struct usb_device_descriptor 	descriptor;

	struct usb_host_endpoint 	ep_0_in;
	struct usb_host_endpoint	ep_0_out;

	/* Arbitrary Limit */
#define USB_MAX_ENDPOINT	8
	struct usb_host_endpoint 	*endpoints[USB_MAX_ENDPOINT];
	unsigned int			num_endpoints;

	struct usb_hcd			*hcd;		
	struct usb_driver		*driver;
	const char 			*driver_name;

	int				toggle;
	void				*priv;
	struct list_head		list;
};

struct usb_driver {
	struct usb_device_id *ids;

	/* Number of entries in the USB Device Table */
	unsigned int id_count;

	int (*probe)(struct usb_device *udev,
			  const struct usb_device_id *id);

	void (*remove)(struct usb_device *udev);
};

struct urb {
	/* private: usb core and host controller only fields in the urb */
	void *hcpriv;			/* private data for host controller */

	/* public: documented fields in the urb that can be used by drivers */
	struct usb_device *udev; 	/* (in) pointer to associated device */
	struct usb_host_endpoint *ep;	/* (internal) pointer to endpoint */

	void *transfer_buffer;		/* (in) associated data buffer */
	unsigned long transfer_dma;	/* (in) dma addr for transfer_buffer */
	unsigned int transfer_buffer_length;	/* (in) data buffer length */
	unsigned int actual_length;		/* (return) actual transfer length */
	unsigned char *setup_packet;	/* (in) setup packet (control only) */
	unsigned long setup_dma;		/* (in) dma addr for setup_packet */		

	int type;

	void *priv;
	struct list_head priv_list;
};

struct hc_driver {
	int	(*enqueue_urb)(struct usb_hcd *hcd, struct urb *urb);
	int	(*urb_status)(struct urb *urb);
	void	(*unlink_urb)(struct urb *urb);

	int	(*reset_port)(struct usb_hcd *hcd, int port);
};

/* 
 * Tell the USB core about the USB device. Returns 0 on success.
 * Returns negative if device could not be bound to a driver
 */
int usb_probe(struct usb_device *udev);
struct usb_device * usb_alloc_dev(void);

/*
 * Initialize a USB device by reading its device descriptor and setting its
 * configuration 
 */
int usb_dev_init(struct usb_device *udev, int port);
void usb_free_dev(struct usb_device *udev);
int usb_set_address(struct usb_device *udev, int devnum);

struct urb * usb_alloc_urb(void);
void usb_free_urb(struct urb *urb);

static inline void usb_fill_control_urb(struct urb *urb,
					struct usb_device *udev,
					struct usb_host_endpoint *ep,
					void *setup_packet,
					void *transfer_buffer,
					int buffer_length)
{
	urb->udev = udev;
	urb->ep = ep;
	urb->setup_packet = setup_packet;
	urb->setup_dma = virt_to_bus(setup_packet);

	urb->transfer_buffer = transfer_buffer;
	urb->transfer_buffer_length = buffer_length;
	urb->transfer_dma = virt_to_bus(transfer_buffer);
}

static inline void usb_fill_bulk_urb(struct urb *urb,
				     struct usb_device *udev,
				     struct usb_host_endpoint *ep,
				     void *transfer_buffer,
				     int buffer_length)
{
	urb->udev = udev;
	urb->ep = ep;
	urb->transfer_buffer = transfer_buffer;
	urb->transfer_buffer_length = buffer_length;
	urb->transfer_dma = virt_to_bus(transfer_buffer);
}

int usb_submit_urb(struct urb *urb);

/* Synchronous control URB. */
int usb_control_msg(struct usb_device *dev, struct usb_host_endpoint *ep, uint8_t request,
		    uint8_t requesttype, uint16_t value, uint16_t index, void *data,
		    uint16_t size);

/* Returns one of USB_URB_STATUS_INPROGRESS, USB_URB_COMPLETE or 
 * USB_URB_ERROR 
 */
int usb_urb_status(struct urb *urb);

/* Remove an URB from the host controller's schedule. */
void usb_unlink_urb(struct urb *urb);


int usb_get_device_descriptor(struct usb_device *dev, unsigned int size);
int usb_set_configuration(struct usb_device *dev, int conf);
int usb_get_configuration(struct usb_device *udev);

/* Remove all devices attached to a HC */
void usb_hcd_remove_all_devices(struct usb_hcd *hcd);

