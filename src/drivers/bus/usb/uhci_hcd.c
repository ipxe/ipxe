#include <errno.h>
#include <little_bswap.h>
#include <stdlib.h>
#include <ipxe/malloc.h>
#include <unistd.h>
#include <ipxe/pci.h>
#include <stdio.h>

#include <ipxe/usb/ch9.h>
#include "uhci_hcd.h"
#include "hcd.h"

#define UHCI_RH_MAXCHILD 7

/* must write as zeroes */
#define WZ_BITS		(USBPORTSC_RES2 | USBPORTSC_RES3 | USBPORTSC_RES4)

/* status change bits:  nonzero writes will clear */
#define RWC_BITS	(USBPORTSC_OCC | USBPORTSC_PEC | USBPORTSC_CSC)

#define CLR_RH_PORTSTAT(_x,_port_addr) \
	do {			\
		status = inw(_port_addr); \
		status &= ~(RWC_BITS|WZ_BITS); \
		status &= ~(_x); \
		status |= RWC_BITS & (_x); \
		outw(status, _port_addr); \
	} while(0);

#define SET_RH_PORTSTAT(_x,_port_addr) \
	do {				\
		status = inw(_port_addr); \
		status |= (_x); \
		status &= ~(RWC_BITS|WZ_BITS); \
		outw(status, _port_addr); \
	} while(0);

void uhci_print_td_info(struct uhci_td *td)
{

	DBG("TD INFO\n LP : %zx Vf : %zx Q : %zx T : %zx Status : %zx "
		"ActLen : %zx MaxLen : %zx D : %zx EP : %zx "
		"DEV : %zx PID : %zx BUFFER : %zx\n",
		td->link & 0xFFFFFFF0, td->link & UHCI_PTR_DEPTH,
		td->link & UHCI_PTR_QH,	td->link & UHCI_PTR_TERM,
		uhci_status_bits(td->status) >> 16,
		uhci_actual_length(td->status),
		uhci_expected_length(td->token),
		uhci_toggle(td->token), uhci_endpoint(td->token),
		uhci_devaddr(td->token), uhci_packetid(td->token),
		td->buffer);
}

void uhci_print_qh_info(struct uhci_qh *qh)
{
	DBG("QH INFO\n QHLP : %zx Q : %zx T %zx\n"
		"\tQELP : %zx : Q : %zx T : %zx\n",
		qh->link & 0xFFFFFFF0, (qh->link & UHCI_PTR_QH) >> 1,
		qh->link & UHCI_PTR_TERM, qh->element & 0xFFFFFFF0,
		(qh->element & UHCI_PTR_QH) >> 1 , qh->link & UHCI_PTR_TERM);
		
}

void uhci_print_uhci_info(struct uhci_hcd *uhci)
{
	uint32_t status;
	uint32_t frnum;

	status = inw(uhci->io_addr + USBSTS);
	frnum = inw(uhci->io_addr + USBFRNUM);
	frnum &= 0x3ff;

	DBG("UHCI STATUS\n\t%s %s Frnum %zd\n",
		(status & USBSTS_HCH)?"Halted":"Not Halted",
		(status & USBSTS_HCPE)?"Sched Error":"Schedule Fine",
		frnum);
}

void uhci_print_port_info(struct uhci_hcd *uhci, int port)
{
	uint16_t status;

	status = inw(uhci->io_addr + port);
	DBG("PORT STATUS\n %s %s %s %s %s %s %s %s \n",
		(status & USBPORTSC_CCS)?"Device present":"Device absent ",	
		(status & USBPORTSC_CSC)?"CSC ":"",
		(status & USBPORTSC_PE)?"PE ":"",
		(status & USBPORTSC_PEC)?"PEC ":"",
		(status & USBPORTSC_DPLUS)?"DPLUS ":"",
		(status & USBPORTSC_DMINUS)?"DMINUS ":"",
		(status & USBPORTSC_LSDA)?"LSPD ":"",
		(status & USBPORTSC_PR)?"PR":"");

}

struct uhci_td *uhci_alloc_td(void)
{	
	struct uhci_td *td;

	td = malloc_dma(sizeof(*td), 16);
	if (!td)
		return NULL;
	memset(td, 0, sizeof(*td));

	td->dma_handle = virt_to_bus(td);
	INIT_LIST_HEAD(&td->list);

	return td;
}

void uhci_free_td(struct uhci_td *td)
{
	free_dma(td, sizeof(*td));
}

inline void uhci_fill_td(struct uhci_td *td, uint32_t status,
		uint32_t token, uint32_t buffer)
{
	td->status = cpu_to_le32(status);
	td->token = cpu_to_le32(token);
	td->buffer = cpu_to_le32(buffer);
}

struct uhci_qh *uhci_alloc_qh(void)
{
	struct uhci_qh *qh;

	qh = malloc_dma(sizeof(*qh), 16);
	if (!qh)
		return NULL;

	memset(qh, 0, sizeof(*qh));
	qh->dma_handle = virt_to_bus(qh);

	qh->link = cpu_to_le32(UHCI_PTR_TERM);

	INIT_LIST_HEAD(&qh->urbp_list);

	qh->dummy_td = uhci_alloc_td();
	if (!qh->dummy_td) {
		goto err_dummytd_malloc;
	}

	qh->element = LINK_TO_TD(qh->dummy_td);
	qh->dummy_td->link = cpu_to_le32(UHCI_PTR_TERM);
	return qh;

	free(qh->dummy_td);
err_dummytd_malloc:
	free(qh);
	return NULL;
}

void uhci_free_qh(struct uhci_qh *qh)
{

	if (qh->dummy_td)
			uhci_free_td(qh->dummy_td);
	free_dma(qh, sizeof(*qh));
}


static void uhci_add_urbp_to_qh(struct uhci_urb_priv *urbp, struct uhci_qh *qh)
{
	list_add_tail(&urbp->list, &qh->urbp_list);
}

static void uhci_del_urbp_from_qh(struct uhci_urb_priv *urbp)
{
	list_del(&urbp->list);
}

static void uhci_add_td_to_urbp(struct uhci_td *td, struct uhci_urb_priv *urbp)
{
	list_add_tail(&td->list, &urbp->td_list);
}

static void uhci_del_td_from_urbp(struct uhci_td *td)
{
	list_del(&td->list);
}

static int uhci_submit_control(struct urb *urb, struct uhci_qh *qh)
{
	struct uhci_td *td;
	unsigned int maxsze = le16_to_cpu(urb->ep->desc.wMaxPacketSize);
	unsigned int len = urb->transfer_buffer_length;
	unsigned long data = urb->transfer_dma;
	uint32_t *plink;
	unsigned long destination, status;
	struct uhci_urb_priv *urbp = urb->hcpriv;

	status = uhci_maxerr(3);
	destination = USB_PID_SETUP | (urb->udev->devnum << 8) | 
				(usb_ep_num(urb->ep) << 14);

	urbp->first_td = td = qh->dummy_td;
	uhci_add_td_to_urbp(td, urbp);
	uhci_fill_td(td, status, destination | uhci_explen(8),
			urb->setup_dma);
	plink = &td->link;
	status |= TD_CTRL_ACTIVE;

	/*
	 * If direction is "send", change the packet ID from SETUP (0x2D)
	 * to OUT (0xE1).  Else change it from SETUP to IN (0x69) and
	 * set Short Packet Detect (SPD) for all data packets.
	 *
	 * 0-length transfers always get treated as "send".
	 */

	if (usb_ep_dir(urb->ep) == USB_DIR_OUT || len == 0)
		destination ^= (USB_PID_SETUP ^ USB_PID_OUT);
	else {
		destination ^= (USB_PID_SETUP ^ USB_PID_IN);
		status |= TD_CTRL_SPD;
	}

	/*
	 * Build the DATA TDs
	 */
	while (len > 0) {
		unsigned int pktsze = maxsze;
		
		if (len <= pktsze) {		/* The last data packet */
			pktsze = len;
		}

 		td = uhci_alloc_td();
		if (!td)
			goto err_td_malloc;

		*plink = LINK_TO_TD(td);

		/* Alternate Data0/1 (start with Data1) */
		destination ^= TD_TOKEN_TOGGLE;
		uhci_add_td_to_urbp(td, urbp);

		uhci_fill_td(td, status, destination | uhci_explen(pktsze),
				data);
		plink = &td->link;

		data += pktsze;
		len -= pktsze;
	}

	/*
	 * Build the final TD for control status 
	 */
	urbp->last_td = td = uhci_alloc_td();
	if (!td) 
		goto err_td_malloc;

	*plink = LINK_TO_TD(td);

	/* Change direction for the status transaction */
	destination ^= (USB_PID_IN ^ USB_PID_OUT);
	destination |= TD_TOKEN_TOGGLE;		/* End in Data1 */

	uhci_add_td_to_urbp(td, urbp);
	uhci_fill_td(td, status,
			destination | uhci_explen(0), 0);
	plink = &td->link;

	/*
	 * Build the new dummy TD and activate the old one
	 */
	 td = uhci_alloc_td();
	if (!td)
		goto err_dummytd_malloc;

	*plink = LINK_TO_TD(td);

	uhci_fill_td(td, 0, USB_PID_OUT | uhci_explen(0), 0);
	wmb();

	qh->dummy_td->status |= cpu_to_le32(TD_CTRL_ACTIVE);
	qh->dummy_td = td;
	td->link = cpu_to_le32(UHCI_PTR_TERM);

	return 0;

	uhci_free_td(qh->dummy_td);
err_dummytd_malloc:
	list_for_each_entry(td, &urbp->td_list, list) {
		uhci_del_td_from_urbp(td);
		uhci_free_td(td);
	}
err_td_malloc:
	list_for_each_entry(td, &urbp->td_list, list) {
		uhci_del_td_from_urbp(td);
		uhci_free_td(td);
	}

	return -ENOMEM;
}

static int uhci_submit_bulk(struct urb *urb, struct uhci_qh *qh)
{
	struct uhci_td *td;
	unsigned long destination, status;
	int maxsze = le16_to_cpu(urb->ep->desc.wMaxPacketSize);
	int len = urb->transfer_buffer_length;
	unsigned long data = urb->transfer_dma;
	uint32_t *plink;
	struct uhci_urb_priv *urbp = urb->hcpriv;
	int toggle, pid;

	if (len < 0)
		return -EINVAL;
	
	pid = ((usb_ep_dir(urb->ep) == USB_DIR_OUT)?USB_PID_OUT:USB_PID_IN);
	destination = pid | (urb->udev->devnum << 8) | 
				(usb_ep_num(urb->ep) << 15);

	toggle = (urb->udev->toggle >> usb_ep_num(urb->ep)) & 1;

	/* 3 errors, dummy TD remains inactive */
	status = uhci_maxerr(3);

	if (usb_ep_dir(urb->ep) == USB_DIR_IN)
		status |= TD_CTRL_SPD;

	/*
	 * Build the DATA TDs
	 */
	plink = NULL;
	td = qh->dummy_td;
	do {	/* Allow zero length packets */
		int pktsze = maxsze;

		if (len <= pktsze) {		/* The last packet */
			pktsze = len;
		}
		if (plink) {
			td = uhci_alloc_td();
			if (!td)
				goto err_td_malloc;
			*plink = LINK_TO_TD(td);
		} else
			urbp->first_td = td;

		uhci_add_td_to_urbp(td, urbp);
		uhci_fill_td(td, status,
				destination | uhci_explen(pktsze) |
					(toggle << TD_TOKEN_TOGGLE_SHIFT),
				data);
		plink = &td->link;
		status |= TD_CTRL_ACTIVE;

		data += pktsze;
		len -= maxsze;
		toggle ^= 1;
	} while (len > 0);
	
	urbp->last_td = td;	
	/*
	 * Build the new dummy TD and activate the old one
	 */
	td = uhci_alloc_td();
	if (!td)
		goto err_dummytd_malloc;
	*plink = LINK_TO_TD(td);

	uhci_fill_td(td, 0, USB_PID_OUT | uhci_explen(0), 0);
	wmb();

	qh->dummy_td->status |= cpu_to_le32(TD_CTRL_ACTIVE);
	qh->dummy_td = td;

	/* Set the toggle */
	urb->udev->toggle &= ~(1 << usb_ep_num(urb->ep));
	urb->udev->toggle |= (toggle << usb_ep_num(urb->ep));
	return 0;

	uhci_free_td(qh->dummy_td);
err_dummytd_malloc:
	list_for_each_entry(td, &urbp->td_list, list) {
		uhci_del_td_from_urbp(td);
		uhci_free_td(td);
	}
err_td_malloc:
	list_for_each_entry(td, &urbp->td_list, list)  {
		uhci_del_td_from_urbp(td);
		uhci_free_td(td);
	}

return -ENOMEM;
}

static int uhci_urb_status(struct urb *urb)
{
	int ret = USB_URB_STATUS_COMPLETE;
	struct uhci_urb_priv *urbp = urb->hcpriv, *temp_urbp;
	struct uhci_td *td, *temp_td;
	struct list_head *p;
	struct uhci_qh *qh = ((struct uhci_qh *) urb->ep->hcpriv);

	/* Check for the status of the first td */
	if (uhci_status_bits(urbp->first_td->status) != 0) {
		if (uhci_status_bits(urbp->first_td->status) == TD_CTRL_ACTIVE) {
			ret = USB_URB_STATUS_INPROGRESS;			
			goto out;
		} else {
			ret = USB_URB_STATUS_ERROR;
			goto out;
		}
	}

	/* Check if URB is complete. Update urb->actual_length */
	/* Optimie by looking at last_td directly. */
	td = urbp->last_td;
	if (uhci_status_bits(td->status) == 0) {
		if (uhci_actual_length(td->status) == uhci_expected_length(td->token)) {
				urb->actual_length = urb->transfer_buffer_length;
				return USB_URB_STATUS_COMPLETE;
		}	
	}

	urb->actual_length = 0;
	list_for_each(p, &urbp->td_list) {
		td = list_entry(p, struct uhci_td, list);
		
		if (uhci_status_bits(td->status) != 0) {
			ret = USB_URB_STATUS_INPROGRESS;
			goto out;
		}

		urb->actual_length += uhci_actual_length(td->status);
		if (uhci_actual_length(td->status) != uhci_expected_length(td->token)
				&& (uhci_status_bits(td->status) == 0)) {
			uint16_t toggle = 0;


			/* Fixup toggle */
			toggle = uhci_toggle(td->token);
			toggle ^= 1;

			list_for_each_entry(temp_urbp, &qh->urbp_list, list) {
				/* Don't count ourselves */
				if (urbp == temp_urbp)
					continue;

				list_for_each_entry(temp_td, &temp_urbp->td_list, list) {
					temp_td->token &= ~(1 << TD_TOKEN_TOGGLE_SHIFT);
					temp_td->token |= cpu_to_le32(toggle << TD_TOKEN_TOGGLE_SHIFT);
					toggle ^= 1;
				}
			}

			urb->udev->toggle &= ~(1 << usb_ep_num(urb->ep));
			urb->udev->toggle |= toggle << usb_ep_num(urb->ep);
			
			/* Jump to the next URB's TD */
			qh->element = urbp->last_td->link;
			
			break;
		}
				
	}

out:
	return ret;
}

		
static int uhci_enqueue_urb(struct usb_hcd *hcd, struct urb *urb)
{
	struct uhci_hcd *uhci;
	struct uhci_urb_priv *urbp;
	struct uhci_qh *qh;
	int ret = -ENOMEM;
	
	uhci = hcd_to_uhci(hcd);		
	urbp = malloc(sizeof(*urbp));
	
	if (!urbp)
		goto err_urb_malloc;

	INIT_LIST_HEAD(&urbp->td_list);
	urb->hcpriv = urbp;

	if (usb_ep_xfertype(urb->ep) == USB_ENDPOINT_XFER_BULK) {
		if (!urb->ep->hcpriv) {
			/* Create a new QH for this endpoint */
			qh = urb->ep->hcpriv = uhci_alloc_qh();
			if (!urb->ep->hcpriv) 
				goto err_hcpriv_malloc;

			/* Link it into the schedule */
			uhci->last_bulk_qh->link = LINK_TO_QH(qh);
			uhci->last_bulk_qh = qh;
		} else {
			qh = (struct uhci_qh *)urb->ep->hcpriv;
		}
	} else /* Control QH skel */
		urb->ep->hcpriv = qh = uhci->fs_control_skelqh;

	/* Add the urbp to the QH's list */
	uhci_add_urbp_to_qh(urbp, qh);
	
	if (usb_ep_xfertype(urb->ep) == USB_ENDPOINT_XFER_CONTROL) {
		if ((ret = uhci_submit_control(urb, qh)) < 0)
			goto err_submit_urb;
	} else {
		if ((ret = uhci_submit_bulk(urb, qh)) < 0)
			goto err_submit_urb;	
	}
	return 0;

	return ret;
err_submit_urb:
	uhci_free_qh(urb->ep->hcpriv);
err_hcpriv_malloc:
	free(urbp);
err_urb_malloc:
	return ret;
}

static void uhci_unlink_urb(struct urb *urb)
{
	struct uhci_td *td;
	struct uhci_urb_priv *urbp;

	urbp = urb->hcpriv;
	list_for_each_entry(td, &urbp->td_list, list) {
		list_del(&td->list);
		uhci_free_td(td);
	}

	uhci_del_urbp_from_qh(urbp);
	free(urbp);
}

static int reset_port(struct uhci_hcd *uhci, int port)
{
	unsigned int status;
	int i;

	SET_RH_PORTSTAT(USBPORTSC_PR, uhci->io_addr + (port * 2) + USBPORTSC1);
	mdelay(100);
	CLR_RH_PORTSTAT(USBPORTSC_PR, uhci->io_addr + (port * 2) + USBPORTSC1);
	CLR_RH_PORTSTAT(USBPORTSC_PEC + USBPORTSC_CSC,
			uhci->io_addr + (port * 2) + USBPORTSC1);

	udelay(10);
#define PE_NUM_TRIES 15
	for (i = 0; i < PE_NUM_TRIES; i++) {
		DBG("PE : Trying %d time\nStatus : %x \n", i, status);
		SET_RH_PORTSTAT(USBPORTSC_PE, uhci->io_addr + (port *2) + USBPORTSC1);
		mdelay(10);
		status = inw(uhci->io_addr + (port * 2) + USBPORTSC1);
		if (status & USBPORTSC_PE)
			break;
	}
	if (i == PE_NUM_TRIES) {
		DBG("UHCI : Could not assert PE\n");
		return -1;
	}

	return 0;
}

static inline int uhci_reset_port(struct usb_hcd *hcd, int port)
{
	struct uhci_hcd *uhci = hcd_to_uhci(hcd);

	return reset_port(uhci, port);
}

static struct hc_driver uhci_driver = {
	.urb_status	=	uhci_urb_status,
	.enqueue_urb	=	uhci_enqueue_urb,
	.unlink_urb	=	uhci_unlink_urb,
	.reset_port	=	uhci_reset_port,
};

static void configure_hc(struct uhci_hcd *uhci)
{

	/* Set the frame length to the default: 1 ms exactly */
	outb(USBSOF_DEFAULT, uhci->io_addr + USBSOF);

	/* Store the frame list base address */
	outl(uhci->frame_dma_handle, uhci->io_addr + USBFLBASEADD);

	/* Set the current frame number */
	outw(0 & UHCI_MAX_SOF_NUMBER,
			uhci->io_addr + USBFRNUM);
	mb();
	
}

static void start_rh(struct uhci_hcd *uhci)
{
	/* Mark it configured and running with a 64-byte max packet.
	 * All interrupts are enabled, even though RESUME won't do anything.
	 */
	outw(USBCMD_RS | USBCMD_CF | USBCMD_MAXP, uhci->io_addr + USBCMD);
	outw(USBINTR_TIMEOUT | USBINTR_RESUME | USBINTR_IOC | USBINTR_SP,
			uhci->io_addr + USBINTR);
	mb();
}

/*
 * Probe for devices attached to the root hub. We support only 
 * static detection of devices at the moment.
 *
 * We follow a very simple device addressing mode, since we 
 * dont support external hubs at the moment.
 */

static int uhci_probe_usb_devices(struct usb_hcd *hcd)
{
	struct uhci_hcd *uhci = hcd_to_uhci(hcd);
	struct usb_device *udev;
	unsigned int port;
	int ret = -ENOMEM;

	DBG("UHCI : Probing for USB Devices..\n");
	for (port = 0; port < uhci->rh_numports; port++) {
		int status;
		
		/* Check for device presence */
		status = inw(uhci->io_addr + (port * 2) + USBPORTSC1);
		if (!(status & USBPORTSC_CCS)) {
			DBG("UHCI : No device on port %d\n", port + 1);
			continue;
		}
		
		udev = usb_alloc_dev();
		if(!udev)
			goto err_usb_alloc_dev;
		udev->hcd = hcd;

		/* Tell the usb core about the new device */
		if (usb_dev_init(udev, port) < 0) {
			DBG("Error initializing device\n");
			usb_free_dev(udev);
		}
	}
	return 0;

err_usb_alloc_dev:
	return ret;
}

/*
 * Allocate a frame list, and then setup the skeleton
 *
 */

static int uhci_start(struct usb_hcd *hcd)
{
	struct uhci_hcd *uhci = hcd_to_uhci(hcd);
	int i;
	int ret = -ENOMEM;

	uhci->frame = malloc_dma(
			UHCI_NUMFRAMES * sizeof(*uhci->frame),	1 << 12);
	uhci->frame_dma_handle = virt_to_bus(uhci->frame);

	if (!uhci->frame) {
		goto err_frame_malloc;
	}

	/* Create Skeleton QHs */	
	if (!(uhci->fs_control_skelqh = uhci_alloc_qh()))
		goto err_fs_control_skelqh_malloc;
	if (!(uhci->last_bulk_qh = uhci->bulk_skelqh = uhci_alloc_qh()))
		goto err_bulk_skelqh_malloc;

	/* Link these QHs */
	uhci->fs_control_skelqh->link = LINK_TO_QH(uhci->bulk_skelqh);
	for(i = 0;i < UHCI_NUMFRAMES; i++) {
		uhci->frame[i] = LINK_TO_QH(uhci->fs_control_skelqh);
	}
	
	/*
	 * Some architectures require a full mb() to enforce completion of
	 * the memory writes above before the I/O transfers in configure_hc().
	 */
	mb();

	configure_hc(uhci);

	/* Start the root hub */
	start_rh(uhci);

	/* Now detect the devices and add them */
	if ((ret = uhci_probe_usb_devices(hcd)) < 0)
		goto err_usb_probe_devices;
	
	return 0;

err_usb_probe_devices:
	uhci_free_qh(uhci->fs_control_skelqh);
err_bulk_skelqh_malloc:
	uhci_free_qh(uhci->bulk_skelqh);
err_fs_control_skelqh_malloc:
	free_dma(uhci->frame, UHCI_NUMFRAMES * sizeof(*uhci->frame));
err_frame_malloc:
	return ret;
}

static void uhci_init(struct usb_hcd *hcd)
{
	struct uhci_hcd *uhci;
	unsigned long io_size = hcd->res_size;
	unsigned int port;

	uhci = hcd_to_uhci(hcd);
	uhci->io_addr = hcd->res_addr;

	/* The UHCI spec says devices must have 2 ports, and goes on to say
	 * they may have more but gives no way to determine how many there
	 * are.  However according to the UHCI spec, Bit 7 of the port
	 * status and control register is always set to 1.  So we try to
	 * use this to our advantage.  Another common failure mode when
	 * a nonexistent register is addressed is to return all ones, so
	 * we test for that also.
	 */
	for (port = 0; port < (io_size - USBPORTSC1) / 2; port++) {
		unsigned int portstatus;

		portstatus = inw(uhci->io_addr + USBPORTSC1 + (port * 2));
		if (!(portstatus & 0x0080) || portstatus == 0xffff)
			break;
	}
	
	DBG("Detected %d ports\n", port);

	/* Anything greater than 7 is weird so we'll ignore it. */
	if (port > UHCI_RH_MAXCHILD)
		port = 2;

	uhci->rh_numports = port;
	uhci->next_devnum = 2;
}

static int uhci_hcd_pci_probe(struct pci_device *pci)
{
	struct usb_hcd *hcd;


	if (pci->class != PCI_CLASS_SERIAL_USB_UHCI)
		return -ENOTTY;

	/* Create a new struct usb_hcd for our pci device */
	
	hcd = (struct usb_hcd *) malloc(sizeof(struct usb_hcd));
	if (!hcd) {
		DBG("Failed allocating memory for usb_hcd\n");
		goto err_hcd_malloc;
	}

	hcd->hcpriv = malloc(sizeof(struct uhci_hcd));
	if (!hcd->hcpriv) {
		DBG("Failed allocating memory for uhci_hcd\n");
		goto err_uhci_hcd_malloc;
	}

	pci_set_drvdata(pci, hcd);
	hcd->driver = &uhci_driver;

	/*
	 * Read PCI BAR #4 to determine the io_base of this device
	 * UHCI 1.1 : Section 2 Table 3.
	 *
	 * We explicitly access ioaddr from BAR #4 to retain similarity 
	 * with the code that obtains the size of the io region
	 */

	hcd->res_addr = pci_bar_start(pci, PCI_BASE_ADDRESS_4);
	hcd->res_size = pci_bar_size(pci, PCI_BASE_ADDRESS_4);
	adjust_pci_device(pci);
	
	DBG("UHCI ioaddr @ %x\n", (unsigned int)pci->ioaddr);

	/* Initialize the controller for the first time */
	uhci_init(hcd);

	/* Start the UHCI controller */
	uhci_start(hcd);

	return 0;

	free(hcd->hcpriv);
err_uhci_hcd_malloc:
	free(hcd);
err_hcd_malloc:

	return -ENOMEM;
}

static void uhci_hcd_pci_remove(struct pci_device *pci)
{
	struct usb_hcd *hcd;
	struct uhci_hcd *uhci;

	hcd = pci_get_drvdata(pci);

	/* Remove all devices hanging off this HC */
	usb_hcd_remove_all_devices(hcd);

	uhci = hcd_to_uhci(hcd);

	/* Stop the schedule */
	outb(0, uhci->io_addr + USBCMD);

	free_dma(uhci->frame, UHCI_NUMFRAMES * sizeof(*uhci->frame));

	free(uhci);
	free(hcd);
}

static struct pci_device_id uhci_hcd_pci_ids[] = {
        PCI_ROM(0xffff, 0xffff, "UHCI HCD", "UHCI USB Controller", 0), 
};

struct pci_driver uhci_hcd_pci_driver __pci_driver = {
	.ids = uhci_hcd_pci_ids,
	.id_count = (sizeof(uhci_hcd_pci_ids) / sizeof(uhci_hcd_pci_ids[0])),
	.probe = uhci_hcd_pci_probe,
	.remove = uhci_hcd_pci_remove,
};

