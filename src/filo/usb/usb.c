#ifdef USB_DISK

/*******************************************************************************
 *
 *
 *	Copyright 2003 Steven James <pyro@linuxlabs.com> and
 *	LinuxLabs http://www.linuxlabs.com
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ******************************************************************************/

#include <etherboot.h>
#include <pci.h>
#include <timer.h>
#include <lib.h>
        
#define DEBUG_THIS DEBUG_USB
#include <debug.h>

#define DPRINTF debug


#include "usb.h"
#include "uhci.h"
#include "ohci.h"
#include "debug_x.h"


#define ALLOCATE 1

int usec_offset=0;

int num_controllers=0;

uint32_t hc_base[MAX_CONTROLLERS];
uint8_t  hc_type[MAX_CONTROLLERS];


void hci_init(void)
{
	int i;
	struct pci_device *dev;
	uint8_t prog_if;


	for(i=0;i<MAX_CONTROLLERS; i++) {
		hc_type[i] = 0xff;
	}

        /* Find a PCI_SERIAL_USB class device */
	i=0;
	num_controllers = 0;
	while(num_controllers<MAX_CONTROLLERS) {
	        dev = pci_find_device(-1, -1, 0x0c03, -1, i);
		if(!dev) break;
		
		prog_if = ((dev->class>>8) & 0xff);
		if(prog_if == 0x00 ) { // UHCI
			hc_type[num_controllers] = prog_if;
			uhc_init(dev);
		}
		else if(prog_if == 0x10) { // OHCI
			hc_type[num_controllers] = prog_if;
			ohc_init(dev);
		}
#if 0 
		else if(prog_if == 0x20) { // EHCI
			hc_type[num_controllers] = prog_if;
			ehc_init(dev);
		}
#endif
		i++;
	}
	// From now should not change num_controllers any more
	
	uhci_init();
	ohci_init();
}


int next_usb_dev;
usbdev_t usb_device[MAX_USB_DEV];

void init_devices(void) 
{

	memset(usb_device,0,sizeof(usb_device));
	usb_device[0].max_packet[0] = 8;
	next_usb_dev=2; // 0 for all controller root hub, use MAX_CONTROLLERS instead??? 
			// do we need have one for every controller ?? or just use hc_base and hc_type instead
			// For example 0 --> controller 1 root hub
			// 	       1 --> controller 2 root hub
			//	       2 --> controller 3 root hub....
}


inline int set_address( uchar address)
{
	int ret;

	ret = usb_control_msg(0, 0, SET_ADDRESS, address, 0, 0, NULL);

	return(ret);
}

inline int clear_stall(uchar device, uchar endpoint)
{
	int ret;

	ret = usb_control_msg(device, CONTROL_ENDPOINT, CLEAR_FEATURE, FEATURE_HALT, endpoint, 0, NULL);
	if(hc_type[device]==0x00) {
		usb_device[device].toggle[endpoint]=0;
	}
	else if(hc_type[device]==0x10) {
		usb_settoggle(&usb_device[device], endpoint & 0xf, ((endpoint & 0x80)>>7)^1, 0);
	}

	return(ret);
}

inline int device_reset(uchar device) {
	return usb_control_msg(device, 0x21, 0xff, 0, 0, 0, NULL);
}

///////////////////////////////////////////////////////////////////////////////////////
//
//	String Descriptors
//
//////////////////////////////////////////////////////////////////////////////////////

#define STRING_DESCRIPTOR 0x0300

int get_string( uchar addr, uchar string, int len, uchar *buffer)
{
	int ret;
	int i,j;
	int real_len;
	ushort lang;

	if(!string) {
		strcpy(buffer, "unknown");
		return(0);
	}

	ret = usb_control_msg(addr, 0x80, GET_DESCRIPTOR, STRING_DESCRIPTOR | string, 0, 4, buffer);
	real_len = buffer[0];
	if(real_len>len)
		real_len = len;

	lang = buffer[2] | buffer[3]<<8;
	ret = usb_control_msg(addr, 0x80, GET_DESCRIPTOR, STRING_DESCRIPTOR | string, lang, real_len, buffer);

	// de-unicode it!
	for(i=0, j=2; j<real_len; i++, j+=2)
		buffer[i] = buffer[j];

	buffer[i]=0;
	real_len/=2;

	return(real_len);
}

int get_string2( uchar addr, uchar string, ushort lang, int len, uchar *buffer)
{
        int ret;
        int i,j;
        int real_len;


        ret = usb_control_msg(addr, 0x80, GET_DESCRIPTOR, STRING_DESCRIPTOR | string, lang, len, buffer);

        real_len = buffer[0];
        if(real_len>len)
                real_len = len;

	if(real_len<=4)  {
		strcpy(buffer, "USB");
		real_len = 3;
		buffer[real_len] = 0;
	} else {
        	// de-unicode it!
        	for(i=0, j=2; j<real_len; i++, j+=2)
                	buffer[i] = buffer[j];

        	buffer[i]=0;
        	real_len/=2;
	}

        return(real_len);
}
ushort get_lang( uchar addr, uchar string, int len, uchar *buffer)
{
        int ret;
        int i,j;
        int real_len;
	ushort lang;
        
        ret = usb_control_msg(addr, 0x80, GET_DESCRIPTOR, STRING_DESCRIPTOR | string, 0, 4, buffer);
	lang = buffer[2] | buffer[3]<<8;

        return lang;
}       

///////////////////////////////////////////////////////////////////////////////////////
//
// HUB functions. This will be moved to it's own module soonishly
//
///////////////////////////////////////////////////////////////////////////////////////

typedef struct port_charge {
	ushort c_port_connection:1;
	ushort c_port_enable:1;
	ushort c_port_suspend:1;
	ushort c_port_over_current:1;
	ushort c_port_reset:1;
	ushort reserved:11;
} port_change_t;

typedef struct port_status {
	ushort port_connection:1;
	ushort port_enable:1;
	ushort port_suspend:1;
	ushort port_over_current:1;
	ushort port_reset:1;
	ushort reserved:3;
	ushort port_power:1;
	ushort port_lowspeed:1;
	ushort port_highspeed:1;
	ushort port_test:1;
	ushort port_indicator:1;
} __attribute__ ((packed)) portstatus_t;


typedef struct portstat {
	portstatus_t  stat;
	port_change_t change;
} __attribute__ ((packed)) portstat_t;

int hub_port_reset( uchar addr, uchar port)
{
	int ret;
	int tries=100;
	portstat_t status;

	ret = usb_control_msg(addr, 0x23, SET_FEATURE, PORT_RESET, port, 0, NULL);	// reset port

	while(tries--) {
		udelay(10000);
		ret = usb_control_msg(addr, 0xa3, GET_STATUS, 0x0, port, 4, &status);
		if(!status.change.c_port_reset)
			continue;

		ret = usb_control_msg(addr, 0x23, CLEAR_FEATURE, C_PORT_RESET, port, 0, NULL);	// clear status
		return(0);
	}

	DPRINTF("hub_port_reset(%x, %x) failed,\n", addr, port);
	dump_hex((uint8_t *)&status, 4, "status=");

	return(-1);
}

int hub_port_resume( uchar addr, uchar port)
{
	int ret;
	int tries=100;
	portstat_t status;

	ret = usb_control_msg(addr, 0x23, CLEAR_FEATURE, PORT_SUSPEND, port, 0, NULL);	// reset port

	while(tries--) {
		udelay(10000);
		ret = usb_control_msg(addr, 0xa3, GET_STATUS, 0x0, port, 4, &status);
		if(!status.change.c_port_suspend)
			continue;

		ret = usb_control_msg(addr, 0x23, CLEAR_FEATURE, C_PORT_SUSPEND, port, 0, NULL);	// clear status
		return(0);
	}

	return(-1);
}

int poll_hub(uchar addr)
{
	int i;
	int ret;
	uchar devaddr=0;
	hub_descriptor_t *desc;
	portstat_t status;

	DPRINTF("Poll hub (%x)\n", addr);
	desc = usb_device[addr].private;

	for(i=1; i<= desc->bNbrPorts; i++) {
		ret = usb_control_msg(addr, 0xa3, GET_STATUS, 0x0, i, 4, &status);
//		DPRINTF("Get status for port %u returns: %d\n", i, ret);
//		dump_hex(&status, 4, "status=");	

		if(status.change.c_port_connection) {
			ret = usb_control_msg(addr, 0x23, CLEAR_FEATURE, C_PORT_CONNECTION, i, 0, NULL);	// clear status

			if(status.stat.port_connection) {
				udelay(desc->bPwrOn2PwrGood * 20000);

				hub_port_resume(addr, i);

				ret = hub_port_reset(addr,i);
				udelay(10);
				ret = usb_control_msg(addr, 0x23, SET_FEATURE, PORT_ENABLE, i, 0, NULL);	// enable port

//		ret = usb_control_msg(addr, 0xa3, GET_STATUS, 0x0, i, 4, &status);
//		DPRINTF("*****Get status again  for port %u returns: %d\n", i, ret);
//		dump_hex(&status, 4, "status=");	

				devaddr = configure_device(i, usb_device[addr].controller, status.stat.port_lowspeed);

				// configure
			} else {
				ret = usb_control_msg(addr, 0x23, SET_FEATURE, PORT_SUSPEND, i, 0, NULL);	// suspend port
				ret = usb_control_msg(addr, 0x23, CLEAR_FEATURE, PORT_ENABLE, i, 0, NULL);	// disable port
				DPRINTF("Hub %d, Port %04x disconnected\n", addr, i);
				// deconfigure
			}
		}
	}
	return(devaddr);

}

int usb_hub_init( uchar addr) 
{
	int i;
	int ret;
	hub_descriptor_t *desc;

	desc = allot(sizeof(hub_descriptor_t));

	memset(desc, 0 , sizeof(hub_descriptor_t));

	DPRINTF("hub init (%d)\n", addr);

	ret = usb_control_msg(addr, 0xa0, GET_DESCRIPTOR, 0x2900, 0, 8, desc);
	ret = usb_control_msg(addr, 0xa0, GET_DESCRIPTOR, 0x2900, 0, desc->bLength, desc);

	usb_device[addr].private = desc;

	for(i=1; i<=desc->bNbrPorts; i++)
		ret = usb_control_msg(addr, 0x23, SET_FEATURE, PORT_POWER, i, 0, NULL);	// power port


	// register hub to be polled

	devpoll[num_polls] = poll_hub;
	parm[num_polls++] = addr;

	return(0);
}

extern void ohci_dump_x(uchar controller);

// will set up whatever device is answering at address 0.
int configure_device(uint32_t port, uchar controller, unsigned int lowspeed)
{
	device_descriptor_t *desc;
	config_descriptor_t *conf;
	interface_descriptor_t *iface;
	endpoint_descriptor_t *epd;
	int ret;
	int i;
	int addr = next_usb_dev++;
	uchar buffer[512];
	uchar string[255];
	ushort lang;
	uchar x[2];

	desc = (device_descriptor_t *) buffer;

	memset( &usb_device[addr], 0, sizeof(usbdev_t));

	printf("New USB device, setting address %d\n", addr);
	if(lowspeed) {
		usb_device[addr].lowspeed = usb_device[0].lowspeed = 1;
		DPRINTF("LOWSPEED\n");
	} else
		usb_device[addr].lowspeed = usb_device[0].lowspeed = 0;

	usb_device[0].port = usb_device[addr].port = port;
	usb_device[0].controller = usb_device[addr].controller = controller;
	usb_device[addr].toggle2[0]=0;
	usb_device[addr].toggle2[1]=0;

//	hc_clear_stat();

	ret = set_address(addr);
	if(ret<0) {
		DPRINTF("configure_device: set_address failed!\n");
		next_usb_dev--;
		return(-1);
	}

	mdelay(10);   /* Let the SET_ADDRESS settle */
		
	usb_device[addr].max_packet[0] = 8;
	

	DPRINTF("Fetching device descriptor length\n");
	ret = usb_control_msg(addr, 0x80, GET_DESCRIPTOR, 0x100, 0, 8, desc);

	usb_device[addr].max_packet[0] = desc->max_packet;

	DPRINTF("Fetching device descriptor\n");
	ret = usb_control_msg(addr, 0x80, GET_DESCRIPTOR, 0x100, 0, desc->bLength, desc);
	if(ret < desc->bLength)
		return(-1);

	DPRINTF("Fetching config descriptor length\n");
	conf = (config_descriptor_t *) (buffer + sizeof(device_descriptor_t));

	ret = usb_control_msg(addr, 0x80, GET_DESCRIPTOR, 0x200, 0, 8, conf);

	DPRINTF("Fetching config descriptor\n");
	ret = usb_control_msg(addr, 0x80, GET_DESCRIPTOR, 0x200, 0, conf->wTotalLength, conf);
	if(ret < conf->wTotalLength)
		return(-1);

	iface = (interface_descriptor_t *) (buffer + sizeof(device_descriptor_t) + conf->bLength);
	epd = (endpoint_descriptor_t *) (buffer + conf->bLength + iface->bLength + sizeof(device_descriptor_t));

	DPRINTF("device:\n");
	dump_device_descriptor( desc, "");
	DPRINTF("config:\n");
	dump_config_descriptor( (uchar *)conf, "");

	DPRINTF("Selecting Configuration number %x:\n", conf->bConfigurationValue);
	ret = usb_control_msg(addr, 0, SET_CONFIGURATION, conf->bConfigurationValue, 0, 0, NULL);
	
//	mdelay(20);

#if 0
	usb_control_msg(addr, 0x80,    GET_CONFIGURATION, 0, 0, 1 , x);
	DPRINTF("Configuration number = %x\n", x[0]);

        usb_control_msg(addr, 0x80, GET_STATUS, 0, addr, 2, x);
        DPRINTF("status = %x %x\n", x[0], x[1]);

        usb_control_msg(addr, 0x81, GET_STATUS, 0, 0, 2, x);
        DPRINTF("status = %x %x\n", x[0], x[1]);
#endif

	for(i=0; i<iface->bNumEndpoints;i++) {
		if(!epd[i].bEndpointAddress) {
			usb_device[addr].max_packet[ 1 ] = epd[i].wMaxPacketSize & 0x3ff;
		} else {
			usb_device[addr].max_packet[ epd[i].bEndpointAddress & 0x7f ] = epd[i].wMaxPacketSize & 0x3ff;
		}

		if( (epd[i].bmAttributes & 0x03) == 0x01) // interrupt
			usb_device[addr].interrupt = epd[i].bEndpointAddress;

		if( (epd[i].bmAttributes & 0x03) == 0x02) { // bulk
#if 0
			DPRINTF("clear stall on ep=%x\n", epd[i].bEndpointAddress);
			clear_stall(addr, epd[i].bEndpointAddress);	// to reset data toggle
			udelay(10);
#endif

#if 0
			usb_control_msg(addr, 0x82, GET_STATUS, 0, epd[i].bEndpointAddress, 2, x);
			DPRINTF("status = %x %x\n", x[0], x[1]);
#endif

			if(epd[i].bEndpointAddress & 0x80){  //in
				usb_device[addr].bulk_in = epd[i].bEndpointAddress; 
			}
			else { //out
				usb_device[addr].bulk_out = epd[i].bEndpointAddress; 
			}
		}

	}

	// determine device class
	if(desc->Class) {
		usb_device[addr].class = desc->Class;
		usb_device[addr].subclass = desc->SubClass;
		usb_device[addr].protocol = desc->protocol;
	} else {
		usb_device[addr].class = iface->bInterfaceClass;
		usb_device[addr].subclass = iface->bInterfaceSubClass;
		usb_device[addr].protocol = iface->bInterfaceProtocol;
	}

	printf("%02x:%02x:%02x\n", usb_device[addr].class, usb_device[addr].subclass, usb_device[addr].protocol);
#if 0
	get_string(addr, desc->iManufacturor, sizeof(string), string);
	printf("Manufacturor: %s\n", string);

	get_string(addr, desc->iProduct, sizeof(string), string);
	printf("Product: %s\n", string);

	get_string(addr, desc->iSerial, sizeof(string), string);
	printf("Serial: %s\n", string);
#else	
        lang = get_lang(addr, 0, sizeof(string), string);
	
	get_string2(addr, desc->iManufacturor, lang, sizeof(string), string);
        printf("Manufacturor: %s\n", string);

        get_string2(addr, desc->iProduct, lang,sizeof(string), string);
        printf("Product: %s\n", string);

        get_string2(addr, desc->iSerial, lang, sizeof(string), string);
        printf("Serial: %s\n", string);
#endif
	
	switch( usb_device[addr].class) {
		case 0x09:	// hub
			usb_hub_init(addr);
			break;

		default:
			break;

	}
		
	DPRINTF("DEVICE CONFIGURED\n");

	return(addr);
}

int num_polls=0;
int (*devpoll[MAX_POLLDEV])(uchar);
uchar parm[MAX_POLLDEV];

int poll_usb()
{
	int addr;
	int found=0;
	int i;
	int j;

	for(i=0; i<num_controllers; i++) {
		debug("poll_usb1 i=%d\t", i);
		// if addr >0, should probably see what was attached!
		if(hc_type[i]==0x00) {
			addr = poll_u_root_hub(PORTSC1(i), i);
			if(addr && !found)
				found=addr;

			addr = poll_u_root_hub(PORTSC2(i), i);
			if(addr && !found)
				found=addr;
		} 

		else if(hc_type[i]==0x10) {
			int NDP;
			NDP = readl(&ohci_regs->roothub.a) & 0xff;
			ohci_regs = (ohci_regs_t *)hc_base[i];
			for(j=0;j<NDP;j++) { 
	                        addr = poll_o_root_hub((uint32_t)&ohci_regs->roothub.portstatus[j], i);
        	                if(addr && !found)
                	                found=addr;
			}
			
		}
		
	}

	// now poll registered drivers (such as the hub driver
	for(i=0;i<num_polls; i++) {
		debug("poll_usb2 i=%d\t", i);
		addr = devpoll[i](parm[i]);
		if(addr && !found)
			found=addr;
	}

	return(found);	
}


int usb_bulk_transfer( uchar devnum, uchar ep, unsigned int len, uchar *data)
{
	uint8_t hc_num = usb_device[devnum].controller;
	if(ep&0x80) {
		ep = usb_device[devnum].bulk_in;
	} else {
		ep = usb_device[devnum].bulk_out;
	}
	
	if(hc_type[hc_num] == 0x00) { //UHCI
		return uhci_bulk_transfer(devnum, ep, len, data);
	} 
	else if( hc_type[hc_num] == 0x10 ) {  //OHCI
		return ohci_bulk_transfer(devnum, ep, len, data);
	}
#if 0 
	else if (hc_type[hc_num] == 0x20 ) {  //EHCI
		return ehci_bulk_transfer(devnum, ep, len, data);
	}
#endif
	return 0;
}
int usb_control_msg( uchar devnum, uchar request_type, uchar request, unsigned short wValue, unsigned short wIndex, 
	unsigned short wLength, void *data)
{
	
	uint8_t hc_num = usb_device[devnum].controller;

        if(hc_type[hc_num] == 0x00) { //UHCI
                return uhci_control_msg(devnum, request_type, request, wValue, wIndex, wLength, data);
        } 
        else if( hc_type[hc_num] == 0x10 ) {  //OHCI
                return ohci_control_msg(devnum, request_type, request, wValue, wIndex, wLength, data);
        }
#if 0 
        else if (hc_type[hc_num] == 0x20 ) {  //EHCI
                return ehci_control_msg(devnum, request_type, request, wValue, wIndex, wLength, data);
        }
#endif
        return 0;	
}


struct urb *usb_alloc_urb(int controller)
{
        struct urb *urb;
	ohci_t *ohci = NULL;
#if URB_PRE_ALLOCATE!=1
        urb = (struct urb *)allot2(sizeof(struct urb),0xff);
        if (!urb) {
                printf("usb_alloc_urb:  allot2 failed");
                return NULL;
        }
#else 
	if(hc_type[controller] == 0x10) { //OHCI
		ohci = &_ohci_x[controller];
		urb = ohci->urb;
	} else {
		urb = NULL;
	}
#endif

        memset(urb, 0, sizeof(*urb));

        return urb;
}
/**
 *      usb_free_urb - frees the memory used by a urb
 *      @urb: pointer to the urb to free
 *
 *      If an urb is created with a call to usb_create_urb() it should be
 *      cleaned up with a call to usb_free_urb() when the driver is finished
 *      with it.
 */
void usb_free_urb(struct urb* urb)
{
#if URB_PRE_ALLOCATE!=1
        if (urb)
                forget2(urb);
#endif
}

void usb_wait_urb_done(struct urb* urb, int timeout)
{
	usbdev_t *usb_dev = urb->dev;
	if(hc_type[usb_dev->controller]==0x10) {
		ohci_wait_urb_done(urb, timeout);
	}
	
}


int usb_submit_urb(struct urb *urb)
{
        if (urb && urb->dev) {
#if 0
		if(hc_type[urb->dev->controller] == 0x00) {
                        return uhci_submit_urb(urb);
                } else 
#endif
		if(hc_type[urb->dev->controller] == 0x10) {
                	return ohci_submit_urb(urb);
		} 
#if 0
		else if(hc_type[urb->dev->controller] == 0x20) {
                        return ohci_submit_urb(urb);
                }
#endif
		return 0;
	}
        else
                return -ENODEV;
}

// Starts urb and waits for completion or timeout
static int usb_start_wait_urb(struct urb *urb, int timeout, int* actual_length)
{ 
        int status;
        status = usb_submit_urb(urb);

//for OHCI We will check the BLF and CLF, because HC after processing all td list, it will clear the BLF and CLF
	usb_wait_urb_done(urb, timeout);
//Add by LYH to call complete function
	if(urb->complete!=0) urb->complete(urb);

        if (actual_length)
                *actual_length = urb->actual_length;

        usb_free_urb(urb);
        return status;
}
// returns status (negative) or length (positive)
int usb_internal_control_msg(struct usbdev *usb_dev, unsigned int pipe,
                            struct usb_ctrlrequest *cmd,  void *data, int len, int timeout, usb_complete_t complete)
{
        struct urb *urb;
        int retv;
        int length;

        urb = usb_alloc_urb(usb_dev->controller);
        if (!urb)
                return -ENOMEM;

        FILL_CONTROL_URB(urb, usb_dev, pipe, (unsigned char*)cmd, data, len,
                   complete,0);

        retv = usb_start_wait_urb(urb, timeout, &length);
        if (retv < 0)
                return retv;
        else
                return length;
}
int usb_control_msg_x(struct usbdev *dev, unsigned int pipe, u8 request, u8 requesttype,
                         u16 value, u16 index, void *data, u16 size, int timeout, usb_complete_t complete)
{
        struct usb_ctrlrequest *dr;
        int ret;
	int controller = dev->controller;
	ohci_t *ohci;

#if URB_PRE_ALLOCATE!=1
	dr = allot2(sizeof(struct usb_ctrlrequest), 0xf);
        if (!dr) {
		printf("usb_control_msg_x: dr allocate no MEM\n");
                return -ENOMEM;
	}
#else
        if(hc_type[controller] == 0x10) { //OHCI
                ohci = &_ohci_x[controller];
                dr = ohci->dr;
        } else {
		dr = NULL;
	}
	
#endif

        dr->bRequestType = requesttype;
        dr->bRequest = request;
        dr->wValue = cpu_to_le16p(&value);
        dr->wIndex = cpu_to_le16p(&index);
        dr->wLength = cpu_to_le16p(&size);

        ret = usb_internal_control_msg(dev, pipe, dr, data, size, timeout, complete);

#if URB_PRE_ALLOCATE!=1
        forget2(dr);
#endif

        return ret;
}
int usb_bulk_msg_x(struct usbdev *usb_dev, unsigned int pipe,
                        void *data, int len, int *actual_length, int timeout, usb_complete_t complete)
{
        struct urb *urb;

        if (len < 0)
                return -EINVAL;

        urb=usb_alloc_urb(usb_dev->controller);
        if (!urb)
                return -ENOMEM;

        FILL_BULK_URB(urb, usb_dev, pipe, data, len,
                    complete, 0);

        return usb_start_wait_urb(urb,timeout,actual_length);
}

#endif
