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
#include "debug_x.h"

#define ALLOCATE 1

extern int usec_offset;

int wait_head( queue_head_t *head, int count)
{
	td_t *td;


	while(!head->depth.terminate) {
		td = MEM_ADDR(head->depth.link);
		if(!td->active)
			return(-1);	// queue failed

		if(count)
			if(! --count)
				return(0);	// still active
				
		udelay(500);	// give it some time
	}

	return(1);	// success
}

queue_head_t *free_qh;
queue_head_t _queue_heads[MAX_QUEUEHEAD];
queue_head_t *queue_heads = _queue_heads;

queue_head_t *new_queue_head(void)
{
	queue_head_t *qh;

	if(!free_qh)
		return(NULL);

	qh = free_qh;
	free_qh = MEM_ADDR(qh->bredth.link);

	memset(qh,0,sizeof(queue_head_t));
	qh->bredth.terminate = qh->depth.terminate=1;

	return(qh);
}

void free_queue_head( queue_head_t *qh)
{

	qh->bredth.link = LINK_ADDR(free_qh);
	if(!free_qh)
		qh->bredth.terminate=1;

	qh->depth.terminate=1;
	free_qh = qh;
}

void init_qh(void)
{
	int i;

	for(i=0; i<MAX_QUEUEHEAD-1; i++) {
		memset(queue_heads+i, 0, sizeof(queue_head_t));
		queue_heads[i].bredth.link = LINK_ADDR( &queue_heads[i+1] );
		queue_heads[i].depth.terminate=1;
	}

	queue_heads[MAX_QUEUEHEAD-1].depth.terminate=1;
	queue_heads[MAX_QUEUEHEAD-1].bredth.terminate=1;

	free_qh = queue_heads;
}

td_t *free_td_list;
td_t _tds[MAX_TD];
td_t *tds = _tds;	// indirection added for kernel testing


void init_td(void)
{
	int i;

	for(i=0; i<MAX_TD-1; i++) {
		memset(tds+i, 0, sizeof(td_t));
		tds[i].link.link = LINK_ADDR( &tds[i+1]);
	}

	memset( &tds[MAX_TD-1], 0, sizeof(td_t));
	tds[MAX_TD-1].link.terminate=1;

	free_td_list = tds;
}

td_t *new_td(void)
{
	td_t *td;

	if(!free_td_list)
		return(NULL);

//	DPRINTF("new_td: free_td = %p\n", free_td_list);
	td = free_td_list;

	free_td_list = MEM_ADDR( td->link.link);
//	DPRINTF("new_td: free_td_list = %p\n", free_td_list);

	memset(td, 0, sizeof(td_t));
	td->link.terminate=1;

//	DPRINTF("new_td: returning %p\n", td);
	return(td);
}

td_t *find_last_td(td_t *td)
{
	td_t *last;

	last = td;

	while(!last->link.terminate)
		last = MEM_ADDR(last->link.link);

	return(last);
}

void free_td( td_t *td)
{
	td_t *last_td;

	last_td = find_last_td(td);

	last_td->link.link = LINK_ADDR(free_td_list);
	if(!free_td_list) 
		last_td->link.terminate=1;
	else
		last_td->link.terminate=0;

	free_td_list = td;
	
}

link_pointer_t *queue_end( queue_head_t *queue)
{
	link_pointer_t *link;

	link = &(queue->depth);

	while(!link->terminate)
		link = MEM_ADDR(link->link);

	return(link);
}

void add_td( queue_head_t *head, td_t *td)
{
	link_pointer_t *link;

	link = queue_end(head);

	link->link = LINK_ADDR(td);
	link->terminate=0;
}

transaction_t transactions[MAX_TRANSACTIONS];
transaction_t *free_transactions;

void init_transactions(void)
{
	int i;

	memset(transactions, 0, sizeof(transactions));

	for(i=0; i<MAX_TRANSACTIONS-1; i++)
		transactions[i].next = &transactions[i+1];

	free_transactions = transactions;
}

void free_transaction( transaction_t *trans )
{
	transaction_t *my_current, *last;

	my_current = trans;

	if(my_current==0) return;
	
	while(my_current) {
		free_td( my_current->td_list );
		free_queue_head( my_current->qh );

		last = my_current;
		my_current = my_current->next;
	}
	
	last->next = free_transactions;
	free_transactions = trans;
}

transaction_t *new_transaction(td_t *td)
{
	transaction_t *trans = free_transactions;
	queue_head_t *qh;

	if(!trans) {
		DPRINTF("new_transaction( td = %x) failed!\n", td);
		return(NULL);
	}

	free_transactions = trans->next;

	memset(trans, 0, sizeof(transaction_t));

	if(td) {
		qh = new_queue_head();
		if(!qh) {
			free_transaction(trans);
			return(NULL);
		}

		trans->qh = qh;
		trans->td_list = td;
		qh->depth.link = LINK_ADDR(td);
		qh->depth.terminate = 0;
		qh->bredth.terminate=1;
	}

	return(trans);
}

transaction_t *add_transaction( transaction_t *trans, td_t *td)
{
	transaction_t *t1;


	t1 = new_transaction(td);
	if(!t1)
		return(NULL);

	trans->next = t1;
	trans->qh->bredth.terminate=0;
	trans->qh->bredth.link = LINK_ADDR(t1->qh);
	trans->qh->bredth.queue=1;

	return(trans);
}

link_pointer_t *frame_list[MAX_CONTROLLERS];
#if 0
uchar fl_buffer[MAX_CONTROLLERS][8192];
#endif

void init_framelist(uchar dev)
{

	int i;
#if 0
	DPRINTF("raw frame_list is at %x\n", fl_buffer[dev]);
	frame_list[dev] = (link_pointer_t *) ((bus_to_virt)(((unsigned int)virt_to_bus(fl_buffer[dev]) & ~0xfff) + 0x1000));
#else 
	frame_list[dev] = (link_pointer_t *) allot2(sizeof(link_pointer_t)*1024, 0xfff);  // 4K alignment
	if(frame_list[dev]==0) {
		printf("init_framelist: no mem\n");
	}
#endif
	memset(frame_list[dev], 0, 1024 * sizeof(link_pointer_t));


	DPRINTF("frame_list is at %x\n", frame_list[dev]);

	for(i=0;i<1024;i++)
		frame_list[dev][i].terminate=1;

}


extern int num_controllers;

extern uint32_t hc_base[MAX_CONTROLLERS];
extern uint8_t  hc_type[MAX_CONTROLLERS];

void uhc_clear_stat()
{
	unsigned short value;

	value = inw(USBSTS(0));
	outw(value, USBSTS(0));
}

void clear_uport_stat(unsigned short port)
{
	unsigned short value;

	value = inw(port);
	outw(value, port);
}

void uport_suspend( unsigned short port)
{
	unsigned short value;

	value = inw(port);
	value |= 0x1000;
	outw( value, port);

}

void uport_wakeup( unsigned short port)
{
	unsigned short value;

	value = inw(port);
	value &= ~0x1000;
	outw( value, port);

}

#if 0
void uport_resume( unsigned short port)
{
	unsigned short value;

	value = inw(port);
	value |= 0x40;
	outw(value, port);
	udelay(20000+usec_offset);
	value &= ~0x40;
	outw(value, port);

	do {
		value = inw(port);
	} while(value & 0x40);
}

#endif
void uport_enable( unsigned short port)
{
	unsigned short value;

	value = inw(port);
	value |= 0x04;
	outw( value, port);

	do {
		value = inw(port);
	} while( !(value & 0x04) && (value & 0x01));

}


void uport_disable( unsigned short port)
{
	unsigned short value;

	value = inw(port);
	value &= ~0x04;
	outw( value, port);
}

void uport_reset(unsigned short port)
{
	unsigned short value;
	int i;

	value = inw(port);
	value |= 0x200;

	outw( value, port);

	for(i=0;i<5;i++)
		udelay(10000+usec_offset);

	value &= ~0x200;
	outw( value, port);

//	DPRINTF("Port %04x reset\n", port);
}

void uport_reset_long(unsigned short port)
{
	unsigned short value;
	int i;

	value = inw(port);
	value |= 0x200;
	outw( value, port);

	for(i=0; i<20; i++)
		udelay(10000);

	value &= ~0x200;
	outw( value, port);

//	DPRINTF("Port %04x reset\n", port);
}

void uhc_reset(uchar controller)
{
	DPRINTF("Resetting UHCI\n");
	outw(0x04, USBCMD(controller));
	udelay(20000);
	outw(0, USBCMD(controller));
}
#if 0
int uhc_stop(uchar dev)
{
	unsigned short tmp;

	tmp = inw(USBCMD(dev));
	tmp &= ~USBCMDRUN;
	outw( tmp, USBCMD(dev));

	while(! (inw(USBSTS(dev)) & USBSTSHALTED) );
	outw( USBSTSHALTED, USBSTS(dev));	// clear the status

	return(0);
}

#endif

int uhc_start(uchar dev) {
	unsigned short tmp;

	DPRINTF("Starting UHCI\n");

	tmp = inw(USBCMD(dev));
	tmp |= USBCMDRUN;

//	tmp |= USBCMD_DEBUG;
	outw( tmp, USBCMD(dev));

	return(0);
}

int uhc_init(struct pci_device *dev)
{
	int16_t word;


	pci_read_config_word(dev, 0x20, &word);
	hc_base[num_controllers] = word;
	hc_base[num_controllers] &= ~1;

	DPRINTF("Found UHCI at %04x\n", hc_base[num_controllers]);
	uhc_reset(num_controllers);

	// set master
	pci_read_config_word(dev, 0x04, &word);
	word |= 0x04;
	pci_write_config_word(dev, 0x04, word);

#if 0
	if( ((unsigned int) virt_to_bus(frame_list[num_controllers])) != ( ( (unsigned int)virt_to_bus(frame_list[num_controllers])) & ~0x7ff) ) {
		DPRINTF("UHCI: grave error, misaligned framelist (%x)\n", frame_list[num_controllers]);
		return(-1);
	}
#endif

	DPRINTF("uhc_init setting framelist to: %08x\n", (unsigned int) virt_to_bus( (frame_list[num_controllers]) ));
	outl( (unsigned int) virt_to_bus(frame_list[num_controllers]), FLBASE(num_controllers));
	outw( 0, FRNUM(num_controllers));
	outw( 0, USBINTR(num_controllers));	// no interrupts!

	outw(0x1000, PORTSC1(num_controllers));
	outw(0x1000, PORTSC2(num_controllers));

	uhc_start(num_controllers);

	dump_uhci(hc_base[num_controllers]);

	num_controllers++;
	return(0);
}

queue_head_t *sched_queue[MAX_CONTROLLERS];
queue_head_t *term_qh[MAX_CONTROLLERS];
//td_t *dummy_td[MAX_CONTROLLERS];
td_t *loop_td[MAX_CONTROLLERS];

void init_sched(uchar dev)
{
	int i;

//	dummy_td[dev] = new_td();
	loop_td[dev] = new_td();
	term_qh[dev] = new_queue_head();

	sched_queue[dev] = new_queue_head();
	sched_queue[dev]->bredth.terminate=0;
	sched_queue[dev]->bredth.queue=1;
	sched_queue[dev]->bredth.link=LINK_ADDR(term_qh[dev]);
	sched_queue[dev]->depth.terminate=1;

	term_qh[dev]->bredth.terminate=1;
	term_qh[dev]->depth.link = LINK_ADDR(loop_td[dev]);
	term_qh[dev]->depth.terminate=0;

//	dummy_td->link.link = LINK_ADDR(sched_queue);
//	dummy_td->link.queue = 1;
//	dummy_td->link.depth=1;
//	dummy_td->link.terminate=0;
//	dummy_td->packet_type = IN_TOKEN;
//	dummy_td->max_transfer = 0x7;
//	dummy_td->isochronous=1;
//	dummy_td->active=1;
//	dummy_td->device_addr = 0x7f;
//	dummy_td->endpoint=0x01;
//	dummy_td->buffer = virt_to_bus(&dummy_td->data[2]);
//	dummy_td->retrys=3;

//dump_hex( (uchar *) dummy_td, sizeof(td_t), "dummy_td ");

	loop_td[dev]->link.link = LINK_ADDR(loop_td[dev]);
	loop_td[dev]->link.terminate=0;
	loop_td[dev]->link.queue=0;
	loop_td[dev]->packet_type = IN_TOKEN;
	loop_td[dev]->max_transfer=7;
	loop_td[dev]->retrys=0;
	loop_td[dev]->device_addr=0x7f;

	for(i=0; i< 1024; i++) {
		frame_list[dev][i].link = LINK_ADDR(sched_queue[dev]);
		frame_list[dev][i].queue=1;
		frame_list[dev][i].terminate=0;
//		frame_list[dev][i].terminate=1;
	}

	dump_link( frame_list[dev], "frame_list_link: ");
//	DPRINTF("dummy_td = %x\n", dummy_td[dev]);

//	dump_frame_list("sched:");

}

void uhci_init(void)
{
	int i;

	init_td();
	init_qh();
	init_transactions();

	for(i=0;i<MAX_CONTROLLERS; i++) {
		if(hc_type[i] == 0x00) {
			init_framelist(i);
			init_sched(i);
		}
	}

	// From now should not change num_controllers any more
}

int poll_queue_head( queue_head_t *qh)
{
	td_t *td;
	int strikes=3;

	if(qh->depth.terminate)
		return(1);

	while(strikes--) {
		if(qh->depth.terminate)
			return(1);

		td = MEM_ADDR(qh->depth.link);

		if(td->active)
			return(0);

		udelay(1000);

//		if(!td->active)
//			return(1);
	}

	return(1);
}

int wait_queue_complete( queue_head_t *qh)
{
	int ret;
	int spins=1000;

	while( --spins && !(ret = poll_queue_head(qh))) {
		udelay(1500);
//		if(!(spins%30))
//			DPRINTF("wait_queue_complete: spin\n");
	}
//	DPRINTF("wait_queue_complete: returning %d\n", ret);

	if(!spins)
		return(-1);

	return(ret);
}

#define BULK_DEPTH 1

transaction_t *_bulk_transfer( uchar devnum, uchar ep, unsigned int len, uchar *data)
{
	uchar dt;
	transaction_t *trans;
	td_t *td, *cur, *last;
	int remaining = len;
	uchar *pos = data;
	int max;
	uchar type = OUT_TOKEN;
	int packet_length;


	if(ep & 0x80)
		type = IN_TOKEN;

	ep &= 0x7f;

	td = cur = last = NULL;
	dt = usb_device[devnum].toggle[ep];
	max = usb_device[devnum].max_packet[ep];

	while(remaining) {
		cur = new_td();
		cur->packet_type = type;
		cur->data_toggle = dt;
		cur->endpoint = ep&0x7f;
		cur->device_addr = devnum;
		cur->detect_short=1;
		cur->active=1;
		dt = dt^0x01;
		
		if(!td){
			td = cur;
		}

		if(last) {
			last->link.terminate=0;
			last->link.link = LINK_ADDR(cur);
		}

		cur->buffer = (void *) virt_to_bus(pos);

		if(remaining>max) {
			packet_length = max;
		}
		else {
			packet_length = remaining;
		}
		
		cur->max_transfer=packet_length-1;
		cur->link.depth = BULK_DEPTH;

		remaining -= packet_length;
		pos+= packet_length;
		last = cur;
	}

//	if( packet_length == max) {	// if final packet wasn't short, add a zero packet
//		cur = new_td();
//		dt = dt^0x01;
//		cur->packet_type = type;
//		cur->max_transfer = 0x7ff;	// zero length code
//		last->link.terminate=0;
//		last->link.link = LINK_ADDR(cur);
//		
//	}

	cur->link.terminate=1;

	trans = new_transaction(td);
	usb_device[devnum].toggle[ep] = dt;

	return(trans);
}

#define DEPTH 0

transaction_t *ctrl_msg(uchar devnum, uchar request_type, uchar request, unsigned short wValue, unsigned short wIndex, unsigned short wLength, uchar *data)
{
	td_t *td;
	td_t *current_td;
	td_t *last_td;
	transaction_t *trans;

	ctrl_msg_t *message;

	unsigned char type;
	int remaining = wLength;
	uchar *pos = data;
	uchar dt=1;

//	DPRINTF("ctrl_msg( %02x, %02x, %02x, %04x, %04x, %04x, %p)\n", devnum, request_type, request, wValue, wIndex, wLength, data);
//	DPRINTF("%d bytes in payload\n", remaining);
//	DPRINTF("lowspeed = %u\n", usb_device[devnum].lowspeed);
	last_td = td = new_td();

	td->packet_type = SETUP_TOKEN;
	td->device_addr = devnum & 0x7f;
	td->max_transfer = 7;		// fixed for setup packets
	td->retrys = CTRL_RETRIES;
	td->active=1;
	td->data_toggle=0;
	td->link.depth=DEPTH;
	td->detect_short=0;
	td->interrupt=1;
	td->lowspeed = usb_device[devnum].lowspeed;

// steal 8 bytes from so-called software area to hole the control message itself
	td->buffer = (void *) virt_to_bus(&(td->data[2]));
	message = bus_to_virt( (unsigned int) td->buffer);

	message->bmRequestType = request_type;
	message->bRequest = request;
	message->wValue = wValue;
	message->wIndex = wIndex;
	message->wLength = wLength;
//dump_hex(td, sizeof(td_t), "ctrl_msg:");
	trans = new_transaction(td);

	if(!trans) {
		DPRINTF("ctrl_msg: couldn't allocate a transaction!\n");
		return(NULL);
	}

	if(request_type & CONTROL_DIR_MASK) 
		type = IN_TOKEN;
	else
		type = OUT_TOKEN;

	while(remaining >0)	{
		int length;

//		DPRINTF("ctrl_msg loop %d remaining, maxpacket = %u\n", remaining, usb_device[devnum].max_packet[0]);
		current_td = new_td();

		last_td->link.link = LINK_ADDR(current_td);
		last_td->link.terminate=0;
		last_td->link.queue=0;
		last_td->link.depth=DEPTH;
		

		current_td->device_addr = devnum & 0x7f;
		current_td->retrys = CTRL_RETRIES;
		current_td->active=1;
		current_td->data_toggle=dt;
		current_td->link.depth=DEPTH;
		current_td->lowspeed = usb_device[devnum].lowspeed;
		current_td->detect_short=1;

		dt = dt^0x01;

		current_td->packet_type = type;
//		if(type == IN_TOKEN)
//			current_td->detect_short=1;

		if(remaining >usb_device[devnum].max_packet[0])
			length = usb_device[devnum].max_packet[0];
		else
			length = remaining;

		current_td->max_transfer = length-1;
		current_td->buffer = (void *) virt_to_bus(pos);
		remaining -= length;
		pos += length;

		last_td = current_td;
	}

	current_td = new_td();

	current_td->device_addr =  devnum & 0x7f;
	current_td->retrys = CONTROL_STS_RETRIES;
	current_td->active=1;
	current_td->lowspeed = usb_device[devnum].lowspeed;

	if(type == IN_TOKEN)
		current_td->packet_type = OUT_TOKEN;
	else
		current_td->packet_type = IN_TOKEN;

	current_td->max_transfer=0x7ff;

	current_td->link.terminate=1;
	current_td->data_toggle=1;
	current_td->link.depth=DEPTH;

	
	last_td->link.link = LINK_ADDR(current_td);
	last_td->link.terminate=0;
	last_td->link.queue=0;
	last_td->link.depth=DEPTH;

	return(trans);
}
	

int schedule_transaction( uchar dev, transaction_t *trans)
{
	unsigned short value;

	if(!sched_queue[dev]->depth.terminate)
		return(-EBUSY);

	sched_queue[dev]->depth.link = LINK_ADDR(trans->qh);
	sched_queue[dev]->depth.terminate = 0;
	sched_queue[dev]->depth.queue=1;

	if(hc_type[dev]==0x00) {
		value = inw(hc_base[dev]);
		value |=1;
		outw( value, hc_base[dev]);
	} 
#if 0
	else if (hc_type[dev]==0x10) {
		uint32_t value;
		ohci_regs_t *ohci_regs = (ohci_regs_t *) hc_base[dev];
                value = readl(&ohci_regs->control);
                value |=OHCI_USB_OPER;
                writel( value, &ohci_regs->control);
	
	}
#endif
	
	return(0);
}

int wait_transaction( transaction_t *trans)
{
	queue_head_t *qh;

	qh = trans->qh;

	while(!qh->bredth.terminate) 
		qh = MEM_ADDR(qh->bredth.link);

	return( wait_queue_complete(qh));
}

void unlink_transaction( uchar dev, transaction_t *trans)
{
	sched_queue[dev]->depth.terminate=1;
	sched_queue[dev]->depth.link = 0;	// just in case
}

int uhci_bulk_transfer( uchar devnum, uchar ep, unsigned int len, uchar *data)
{
	transaction_t *trans;
	td_t *td;
	int data_len;
	int ret;
	uchar *buffer;
	DPRINTF("bulk_transfer: ep = %x len=%d\n", ep, len);
#if ALLOCATE==1
        buffer = allot2(2048, 0x7ff);
	if(buffer==0){
                printf("bulk_transfer: can not allot\n");
        }
	memset(buffer,0,2048);
//	DPRINTF("bulk_transfer: buffer(virt) = %x buffer(phys) = %x len = %d\n", buffer, virt_to_phys(buffer), len);

        if( !(ep & 0x80))
                memcpy(buffer, data, len);
#else
	buffer = data;
#endif
	

	trans = _bulk_transfer(devnum, ep, len, buffer);
#if 0
#ifdef DEBUG
	dump_transaction(trans, "bulk_transfer:");
#endif
#endif
	schedule_transaction( usb_device[devnum].controller, trans);
	ret = wait_transaction(trans);

	if(ret<0) {
#ifdef DEBUG
		dump_uhci(hc_base[usb_device[devnum].controller] );
		dump_td(trans->td_list, "failed_bulk_transaction: ");
#endif
 		unlink_transaction( usb_device[devnum].controller, trans);
		free_transaction(trans);
#if ALLOCATE==1
                forget2(buffer);
#endif
		return(-1);
	}

	unlink_transaction( usb_device[devnum].controller, trans);

	data_len=0;
	td = trans->td_list;
	do {
		if(td->active)
			break;

		if(td->max_transfer == 0x7ff)
			break;

		data_len += td->actual +1;

		if(td->actual < td->max_transfer) // short packet also check for errors here
			break;

		if(!td->link.terminate){
			td = MEM_ADDR(td->link.link);
		}
		else {
			td=NULL;
		}
	} while(td);
#if 0

#ifdef DEBUG
	dump_td(trans->td_list, "bulk_transfer_success:");
#endif
#endif

	if(data_len < len) {
		DPRINTF("bulk_transfer( dev= %d, ep = %d, len = %d, buffer = %x) = %d:short transaction:\n", devnum, ep, len, data, data_len);
		dump_td(trans->td_list, "short_transaction:");
	}

	free_transaction(trans);

#if ALLOCATE==1
	if( (ep & 0x80))
	        memcpy(data, buffer, len);
        forget2(buffer);
#endif


	DPRINTF("bulk_transfer returning %d\n", data_len);
	return(data_len);
}

int uhci_control_msg( uchar devnum, uchar request_type, uchar request, unsigned short wValue, unsigned short wIndex, unsigned short wLength, void *data)
{
	transaction_t *trans;
	td_t *td;
	int data_len=0;
	uchar *buffer;
	int ret;
        DPRINTF("uhci_control_msg: request_type = %x request = %x wLength=%d\n", request_type, request, wLength);
#if ALLOCATE==1
//	if( (wLength!=0) && (data!=NULL) ) {
	        buffer = allot2(2048+wLength,0x7ff);
	        if(buffer==0){
        	        printf("uhci_control_msg: can not allot\n");
        	}
		
		memset(buffer,0,2048+wLength);
	//DPRINTF("uhci_control_msg: buffer(virt) = %x buffer(phys) = %x wLength=%d\n", buffer, virt_to_phys(buffer), wLength);
        	if( !(request_type & 0x80))
                	memcpy(buffer, data, wLength);
//	} else {
//		buffer=NULL;
//	}
	
#else
        buffer = data;
#endif

	trans = ctrl_msg(devnum, request_type, request, wValue, wIndex, wLength, buffer);
	if(!trans) {
		DPRINTF("uhci_control_msg: ctrl_msg failed!\n");
#if ALLOCATE==1
                forget2(buffer);
#endif
		return(-1);
	}
	
	schedule_transaction( usb_device[devnum].controller, trans);
	ret = wait_transaction(trans);

	if(ret<0) {
#ifdef DEBUG
		dump_uhci(hc_base[usb_device[devnum].controller] );
		dump_td(trans->td_list, "failed_transaction: ");
#endif
		unlink_transaction( usb_device[devnum].controller, trans);
		free_transaction(trans);
#if ALLOCATE==1
                forget2(buffer);
#endif
		return(ret);
	}

//#ifdef DEBUG
//	dump_td(trans->td_list, "success: ");
//#endif

	unlink_transaction( usb_device[devnum].controller, trans);

	// now, see what happened

	if(!trans->qh->depth.terminate) {
//		handle setup error

		dump_uhci(hc_base);
		dump_td(trans->td_list, "qh->depth failed_transaction: ");

		free_transaction(trans);
#if ALLOCATE==1
                forget2(buffer);
#endif
		return(-1);
	}

	td = trans->td_list;

	do {
		if(td->packet_type != SETUP_TOKEN)
			data_len += td->actual;

		if(td->actual < td->max_transfer) // short packet also check for errors here
			break;

		if(!td->link.terminate) {
			td = MEM_ADDR(td->link.link);
		}
		else {
			td=NULL;
		}
	} while(td);

	free_transaction(trans);

#if ALLOCATE==1
	if ( (wLength!=0) && (data!=NULL)){
	        if( (request_type & 0x80))
        	        memcpy(data, buffer, wLength);
        	forget2(buffer);
	}
#endif

	DPRINTF("usb_control_message returning %d\n", data_len);

	return(data_len);
}


int poll_u_root_hub(unsigned short port, uchar controller)
{
	ushort value;
	int addr=0;
	int i;
	static int do_over=0;

	value = inw(port);
	
	debug("poll_u_root_hub1 v=%08x\t", value);

	if(value & 0x02 || do_over == port) {
		debug("poll_u_root_hub2 v=%08x\t", value);
		do_over=0;
		if(value & 0x01 ) {	// if port connected
			debug("poll_u_root_hub21 v=%08x\t", value);
			DPRINTF("Connection on port %04x\n", port);

			outw(value, port);
			for(i=0; i<40; i++) {
				udelay(10000+usec_offset);
				value = inw(port);
				if(value & 0x02) {
					outw(value, port);
					i=0;
					DPRINTF("BOUNCE!\n");
				}
			}

			uport_wakeup(port);
//			DPRINTF("Wakup %04x\n", port);

			uport_reset(port);
			udelay(10);
			uport_enable(port);

			if(!value & 0x01) {
				DPRINTF("Device went away!\n");
				return(-1);
			}

			addr = configure_device( port, controller, value & 0x100);

			if(addr<0) {
				uport_disable(port);
				udelay(20000);
//				uport_reset(port);
				uport_reset_long(port);
				uport_suspend(port);
				do_over=port;
				uhc_clear_stat();
//				dump_uhci(0x38c0);
			}
		} else {
			uport_suspend(port);
			uport_disable(port);
			DPRINTF("Port %04x disconnected\n", port);
			// wave hands, deconfigure devices on this port!
		}
	}
			
	
	return(addr);
}

#endif
