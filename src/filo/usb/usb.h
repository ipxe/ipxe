#ifndef _USB_H
#define _USB_H

#define URB_PRE_ALLOCATE 1

#define u32 uint32_t
#define u16 uint16_t
#define u8  uint8_t

#define uchar uint8_t
#define ushort uint16_t
#define EBUSY  1
#define ENOMEM 12
#define ENODEV 19
#define EINVAL 22
#define EINPROGRESS 115

#define LINK_ADDR(x) ( virt_to_bus(x) >> 4)
#define MEM_ADDR(x) (void *) (  bus_to_virt( ((unsigned int) (x)) <<4) )

#define MAX_CONTROLLERS 4

extern int num_controllers;

extern uint32_t hc_base[];
extern uint8_t hc_type[];

// Some control message bmRequestType defines
#define CTRL_DEVICE 0
#define CONTROL_INTERFACE 1
#define CONTROL_ENDPOINT 2
#define CONTROL_OTHER 3
#define CONTROL_RECIPIENT_MASK 0x1f

#define CONTROL_TYPE_STD 0
#define CONTROL_TYPE_CLASS 0x20
#define CONTROL_CLASS_VENDOR 0x40
#define CONTROL_CLASS_MASK 0x60

#define CONTROL_OUT 0
#define CONTROL_IN 0x80
#define CONTROL_DIR_MASK 0x80

// bRequest values
#define GET_STATUS 0
#define CLEAR_FEATURE 1
#define SET_FEATURE 3
#define SET_ADDRESS 5

#define GET_DESCRIPTOR 6
#define SET_DESCRIPTOR 7

#define GET_CONFIGURATION 8
#define SET_CONFIGURATION 9

#define GET_INTERFACE 10
#define SET_INTERFACE 11

#define SYNC_FRAME 12

// descriptor types
#define DEVICE_DESC 1
#define CONFIGURATION_DESC 2
#define STRING_DESC 3
#define INTERFACE_DESC 4
#define ENDPOINT_DESC 5
#define OTHERSPEED_DESC 7
#define POWER_DESC 8


typedef struct device_descriptor {
        uchar bLength;
        uchar type;
        
        uchar bcdVersion[2];
        uchar Class;
        uchar SubClass;
        uchar protocol;
        uchar max_packet;
        
        unsigned short idVendor;
        unsigned short idProduct;
        
        uchar bcdDevice[2];
        uchar iManufacturor;
        uchar iProduct;
        uchar iSerial;
        uchar bNumConfig;
} __attribute__((packed)) device_descriptor_t;

#define GET_DESCRIPTOR 6

typedef struct config_descriptor {
        uchar bLength;
        uchar type;

        unsigned short wTotalLength;
        uchar bNumInterfaces;
        uchar bConfigurationValue;
        uchar iConfiguration;

        uchar bmAttributes;
        uchar bMaxPower;
} __attribute__((packed)) config_descriptor_t;

typedef struct interface_descriptor {
        uchar bLength;
        uchar type;

        uchar bInterfaceNumber;
        uchar bAlternateSetting;

        uchar bNumEndpoints;
        uchar bInterfaceClass;
        uchar bInterfaceSubClass;
        uchar bInterfaceProtocol;
        uchar iInterface;
} __attribute__((packed)) interface_descriptor_t;

typedef struct endpoint_descriptor {
        uchar bLength;
        uchar type;

        uchar bEndpointAddress;
        uchar bmAttributes;
        unsigned short wMaxPacketSize;
        uchar bInterval;
} __attribute__((packed)) endpoint_descriptor_t;

typedef struct ctrl_msg {
        uchar bmRequestType;
        uchar bRequest;
        unsigned short wValue;
        unsigned short wIndex;
        unsigned short wLength;
} __attribute__((packed)) ctrl_msg_t;

// Some descriptors for hubs, will be moved later
typedef struct hub_descriptor {
        uchar bLength;
        uchar type;

        uchar bNbrPorts;
        ushort wHubCharacteristics;
        uchar bPwrOn2PwrGood;
        uchar bHubCntrCurrent;

        uchar DeviceRemovable;  // assume bNbrPorts <=8
        uchar PortPwrCntrMask;
} __attribute__((packed)) hub_descriptor_t;

#define MAX_USB_DEV 127
#define MAX_EP 8
        
typedef struct usbdev {
        uint32_t        port;
        uchar           address;
        uchar           controller;
        uchar           class;
        uchar           subclass;
        uchar           protocol;
        uchar           bulk_in;
        uchar           bulk_out;
        uchar           interrupt;
        uchar           lowspeed;
        uint32_t        toggle2[2]; //For OHCI
	uint32_t	halted[2];
	uchar		toggle[MAX_EP]; //for UHCI
        unsigned short  max_packet[MAX_EP];
        void            *private;
} usbdev_t;

// I will use urb as transaction for OHCI to remember the td and ed

struct urb;
typedef void (*usb_complete_t)(struct urb *);

struct urb
{
#if 0
        spinlock_t lock;                // lock for the URB
#endif
        void *hcpriv;                   // private data for host controller
#if 0
        struct list_head urb_list;      // list pointer to all active urbs 
        struct urb *next;               // pointer to next URB  
#endif
        struct usbdev *dev;         // pointer to associated USB device
        unsigned int pipe;              // pipe information
        int status;                     // returned status
        unsigned int transfer_flags;    // USB_DISABLE_SPD | USB_ISO_ASAP | etc.
        void *transfer_buffer;          // associated data buffer
        void *transfer_dma;        // dma addr for transfer_buffer
        int transfer_buffer_length;     // data buffer length
        int actual_length;              // actual data buffer length    
        int bandwidth;                  // bandwidth for this transfer request (INT or ISO)
        unsigned char *setup_packet;    // setup packet (control only)
        void * setup_dma;           // dma addr for setup_packet
        //
        int start_frame;                // start frame (iso/irq only)
        int number_of_packets;          // number of packets in this request (iso)
        int interval;                   // polling interval (irq only)
        int error_count;                // number of errors in this transfer (iso only)
        int timeout;                    // timeout (in jiffies)
        //
        void *context;                  // context for completion routine
        usb_complete_t complete;        // pointer to completion routine
        //
#if 0
        struct iso_packet_descriptor iso_frame_desc[0];
#endif
};

typedef struct urb urb_t;

/*
 * urb->transfer_flags:
 */
#define USB_DISABLE_SPD         0x0001
#define URB_SHORT_NOT_OK        USB_DISABLE_SPD
#define USB_ISO_ASAP            0x0002
#define USB_ASYNC_UNLINK        0x0008
#define USB_QUEUE_BULK          0x0010
#define USB_NO_FSBR             0x0020
#define USB_ZERO_PACKET         0x0040  // Finish bulk OUTs always with zero length packet
#define URB_NO_INTERRUPT        0x0080  /* HINT: no non-error interrupt needed */
                                        /* ... less overhead for QUEUE_BULK */
#define USB_TIMEOUT_KILLED      0x1000  // only set by HCD!


struct usb_ctrlrequest {
        u8 bRequestType;
        u8 bRequest;
        u16 wValue;
        u16 wIndex;
        u16 wLength;
} __attribute__ ((packed));

/*
 * USB-status codes:
 * USB_ST* maps to -E* and should go away in the future
 */

#define USB_ST_NOERROR          0
#define USB_ST_CRC              (-EILSEQ)
#define USB_ST_BITSTUFF         (-EPROTO)
#define USB_ST_NORESPONSE       (-ETIMEDOUT)                    /* device not responding/handshaking */
#define USB_ST_DATAOVERRUN      (-EOVERFLOW)
#define USB_ST_DATAUNDERRUN     (-EREMOTEIO)
#define USB_ST_BUFFEROVERRUN    (-ECOMM)
#define USB_ST_BUFFERUNDERRUN   (-ENOSR)
#define USB_ST_INTERNALERROR    (-EPROTO)                       /* unknown error */
#define USB_ST_SHORT_PACKET     (-EREMOTEIO)
#define USB_ST_PARTIAL_ERROR    (-EXDEV)                        /* ISO transfer only partially completed */
#define USB_ST_URB_KILLED       (-ENOENT)                       /* URB canceled by user */
#define USB_ST_URB_PENDING      (-EINPROGRESS)
#define USB_ST_REMOVED          (-ENODEV)                       /* device not existing or removed */
#define USB_ST_TIMEOUT          (-ETIMEDOUT)                    /* communication timed out, also in urb->status**/
#define USB_ST_NOTSUPPORTED     (-ENOSYS)                       
#define USB_ST_BANDWIDTH_ERROR  (-ENOSPC)                       /* too much bandwidth used */
#define USB_ST_URB_INVALID_ERROR  (-EINVAL)                     /* invalid value/transfer type */
#define USB_ST_URB_REQUEST_ERROR  (-ENXIO)                      /* invalid endpoint */
#define USB_ST_STALL            (-EPIPE)                        /* pipe stalled, also in urb->status*/

/**
 * FILL_CONTROL_URB - macro to help initialize a control urb
 * @URB: pointer to the urb to initialize.
 * @DEV: pointer to the struct usb_device for this urb.
 * @PIPE: the endpoint pipe
 * @SETUP_PACKET: pointer to the setup_packet buffer
 * @TRANSFER_BUFFER: pointer to the transfer buffer
 * @BUFFER_LENGTH: length of the transfer buffer
 * @COMPLETE: pointer to the usb_complete_t function
 * @CONTEXT: what to set the urb context to.
 *
 * Initializes a control urb with the proper information needed to submit
 * it to a device.  This macro is depreciated, the usb_fill_control_urb()
 * function should be used instead.
 */
#define FILL_CONTROL_URB(URB,DEV,PIPE,SETUP_PACKET,TRANSFER_BUFFER,BUFFER_LENGTH,COMPLETE,CONTEXT) \
    do {\
        (URB)->dev=DEV;\
        (URB)->pipe=PIPE;\
        (URB)->setup_packet=SETUP_PACKET;\
        (URB)->transfer_buffer=TRANSFER_BUFFER;\
        (URB)->transfer_buffer_length=BUFFER_LENGTH;\
        (URB)->complete=COMPLETE;\
        (URB)->context=CONTEXT;\
    } while (0)


/**
 * FILL_BULK_URB - macro to help initialize a bulk urb
 * @URB: pointer to the urb to initialize.
 * @DEV: pointer to the struct usb_device for this urb.
 * @PIPE: the endpoint pipe
 * @TRANSFER_BUFFER: pointer to the transfer buffer
 * @BUFFER_LENGTH: length of the transfer buffer
 * @COMPLETE: pointer to the usb_complete_t function
 * @CONTEXT: what to set the urb context to.
 *
 * Initializes a bulk urb with the proper information needed to submit it
 * to a device.  This macro is depreciated, the usb_fill_bulk_urb()
 * function should be used instead.
 */
#define FILL_BULK_URB(URB,DEV,PIPE,TRANSFER_BUFFER,BUFFER_LENGTH,COMPLETE,CONTEXT) \
    do {\
        (URB)->dev=DEV;\
        (URB)->pipe=PIPE;\
        (URB)->transfer_buffer=TRANSFER_BUFFER;\
        (URB)->transfer_buffer_length=BUFFER_LENGTH;\
        (URB)->complete=COMPLETE;\
        (URB)->context=CONTEXT;\
    } while (0)


/*
 * USB directions
 */
#define USB_DIR_OUT                     0               /* to device */
#define USB_DIR_IN                      0x80            /* to host */

/*
 * USB Packet IDs (PIDs)
 */
#define USB_PID_UNDEF_0                 0xf0
#define USB_PID_OUT                     0xe1
#define USB_PID_ACK                     0xd2
#define USB_PID_DATA0                   0xc3
#define USB_PID_PING                    0xb4    /* USB 2.0 */
#define USB_PID_SOF                     0xa5
#define USB_PID_NYET                    0x96    /* USB 2.0 */
#define USB_PID_DATA2                   0x87    /* USB 2.0 */
#define USB_PID_SPLIT                   0x78    /* USB 2.0 */
#define USB_PID_IN                      0x69
#define USB_PID_NAK                     0x5a
#define USB_PID_DATA1                   0x4b
#define USB_PID_PREAMBLE                0x3c    /* Token mode */
#define USB_PID_ERR                     0x3c    /* USB 2.0: handshake mode */
#define USB_PID_SETUP                   0x2d
#define USB_PID_STALL                   0x1e
#define USB_PID_MDATA                   0x0f    /* USB 2.0 */

#define PIPE_ISOCHRONOUS                0
#define PIPE_INTERRUPT                  1
#define PIPE_CONTROL                    2
#define PIPE_BULK                       3

#define usb_maxpacket(dev, pipe, out)   ((dev)->max_packet[usb_pipeendpoint(pipe)])
#define usb_packetid(pipe)      (((pipe) & USB_DIR_IN) ? USB_PID_IN : USB_PID_OUT)

#define usb_pipeout(pipe)       ((((pipe) >> 7) & 1) ^ 1)
#define usb_pipein(pipe)        (((pipe) >> 7) & 1)
#define usb_pipedevice(pipe)    (((pipe) >> 8) & 0x7f)
#define usb_pipe_endpdev(pipe)  (((pipe) >> 8) & 0x7ff)
#define usb_pipeendpoint(pipe)  (((pipe) >> 15) & 0xf)
#define usb_pipedata(pipe)      (((pipe) >> 19) & 1)
#define usb_pipeslow(pipe)      (((pipe) >> 26) & 1)
#define usb_pipetype(pipe)      (((pipe) >> 30) & 3)
#define usb_pipeisoc(pipe)      (usb_pipetype((pipe)) == PIPE_ISOCHRONOUS)
#define usb_pipeint(pipe)       (usb_pipetype((pipe)) == PIPE_INTERRUPT)
#define usb_pipecontrol(pipe)   (usb_pipetype((pipe)) == PIPE_CONTROL)
#define usb_pipebulk(pipe)      (usb_pipetype((pipe)) == PIPE_BULK)

#define PIPE_DEVEP_MASK         0x0007ff00


/* The D0/D1 toggle bits */
#define usb_gettoggle(dev, ep, out) (((dev)->toggle2[out] >> (ep)) & 1)
#define usb_dotoggle(dev, ep, out)  ((dev)->toggle2[out] ^= (1 << (ep)))
static inline void usb_settoggle(struct usbdev *dev,
                                 unsigned int ep,
                                 unsigned int out,
                                 int bit)
{
        dev->toggle2[out] &= ~(1 << ep);
        dev->toggle2[out] |= bit << ep;
}


/* Endpoint halt control/status */
#define usb_endpoint_out(ep_dir)        (((ep_dir >> 7) & 1) ^ 1)
#define usb_endpoint_halt(dev, ep, out) ((dev)->halted[out] |= (1 << (ep)))
#define usb_endpoint_running(dev, ep, out) ((dev)->halted[out] &= ~(1 << (ep)))
#define usb_endpoint_halted(dev, ep, out) ((dev)->halted[out] & (1 << (ep)))


static inline unsigned int __create_pipe(usbdev_t *dev, unsigned int endpoint)
{
        return (dev->address << 8) | (endpoint << 15) |
                ((dev->lowspeed == 1) << 26);
}

static inline unsigned int __default_pipe(struct usbdev *dev)
{
        return ((dev->lowspeed == 1) << 26);
}

/* Create various pipes... */
#define usb_sndctrlpipe(dev,endpoint)   ((PIPE_CONTROL << 30) | __create_pipe(dev,endpoint))
#define usb_rcvctrlpipe(dev,endpoint)   ((PIPE_CONTROL << 30) | __create_pipe(dev,endpoint) | USB_DIR_IN)
#if 0
#define usb_sndisocpipe(dev,endpoint)   ((PIPE_ISOCHRONOUS << 30) | __create_pipe(dev,endpoint))
#define usb_rcvisocpipe(dev,endpoint)   ((PIPE_ISOCHRONOUS << 30) | __create_pipe(dev,endpoint) | USB_DIR_IN)
#endif
#define usb_sndbulkpipe(dev,endpoint)   ((PIPE_BULK << 30) | __create_pipe(dev,endpoint))
#define usb_rcvbulkpipe(dev,endpoint)   ((PIPE_BULK << 30) | __create_pipe(dev,endpoint) | USB_DIR_IN)
#if 0
#define usb_sndintpipe(dev,endpoint)    ((PIPE_INTERRUPT << 30) | __create_pipe(dev,endpoint))
#define usb_rcvintpipe(dev,endpoint)    ((PIPE_INTERRUPT << 30) | __create_pipe(dev,endpoint) | USB_DIR_IN)
#endif
#define usb_snddefctrl(dev)             ((PIPE_CONTROL << 30) | __default_pipe(dev))
#define usb_rcvdefctrl(dev)             ((PIPE_CONTROL << 30) | __default_pipe(dev) | USB_DIR_IN)


extern int next_usb_dev;
usbdev_t usb_device[MAX_USB_DEV];

void init_devices(void);
void hci_init(void);
int hc_init(struct pci_device *dev);
inline int set_address(uchar address);
inline int clear_stall(uchar device, uchar endpoint);
int poll_usb();
int configure_device(uint32_t  port, uchar controller, unsigned int lowspeed);
int usb_bulk_transfer( uchar devnum, uchar ep, unsigned int len, uchar *data);
int usb_control_msg( uchar devnum, uchar request_type, uchar request, unsigned short wValue, unsigned short wIndex,
        unsigned short wLength, void *data);

int usb_control_msg_x(struct usbdev *dev, unsigned int pipe, u8 request, u8 requesttype,
                         u16 value, u16 index, void *data, u16 size, int timeout, usb_complete_t complete);
int usb_bulk_msg_x(struct usbdev *usb_dev, unsigned int pipe,
                        void *data, int len, int *actual_length, int timeout, usb_complete_t complete);

#endif
