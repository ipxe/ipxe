#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <gpxe/usb.h>
#include <gpxe/pci.h>
#include <gpxe/malloc.h>
#include <io.h>

#include "ohci.h"

static int once_initialized = 0;

#define	OHCI_INTR_INIT \
		(OHCI_INTR_MIE | OHCI_INTR_RHSC | OHCI_INTR_UE \
		| OHCI_INTR_RD | OHCI_INTR_WDH)

static void ohci_usb_reset (struct ohci_hcd *ohci)
{
	ohci->hc_control = readl (&ohci->regs->control);
	ohci->hc_control &= OHCI_CTRL_RWC;
	writel (ohci->hc_control, &ohci->regs->control);
}

static int ohci_init(struct ohci_hcd *ohci)
{
	if (readl (&ohci->regs->control) & OHCI_CTRL_IR) {
		u32 temp;

		DBG("USB HC TakeOver from BIOS/SMM\n");
		/* this timeout is arbitrary.  we make it long, so systems
		 * depending on usb keyboards may be usable even if the
		 * BIOS/SMM code seems pretty broken.
		 */
		temp = 500;	/* arbitrary: five seconds */

		writel (OHCI_INTR_OC, &ohci->regs->intrenable);
		writel (OHCI_OCR, &ohci->regs->cmdstatus);
		while (readl (&ohci->regs->control) & OHCI_CTRL_IR) {
			mdelay (10);
			if (--temp == 0) {
				DBG ("USB HC takeover failed!"
					"  (BIOS/SMM bug)\n");
				return -EBUSY;
			}
		}
	}
	
	ohci_usb_reset (ohci);

	/* Disable HC interrupts */
	writel (OHCI_INTR_MIE, &ohci->regs->intrdisable);

	/* flush the writes, and save key bits like RWC */
	if (readl (&ohci->regs->control) & OHCI_CTRL_RWC)
		ohci->hc_control |= OHCI_CTRL_RWC;

	ohci->num_ports = roothub_a(ohci) & RH_A_NDP;
	DBG("Num ports = %d\n", ohci->num_ports);

	ohci->hcca = malloc_dma(sizeof(*ohci->hcca), 256);
	writel(virt_to_bus(ohci->hcca), &ohci->regs->hcca);
	return 0;
}

/*-------------------------------------------------------------------------*/

#define	PORT_RESET_MSEC		50

/* this timer value might be vendor-specific ... */
#define	PORT_RESET_HW_MSEC	10

/* wrap-aware logic morphed from <linux/jiffies.h> */
#define tick_before(t1,t2) ((int16_t)(((int16_t)(t1))-((int16_t)(t2))) < 0)

/* called from some task, normally khubd */
static inline int reset_port (struct ohci_hcd *ohci, unsigned port)
{
	__hc32 *portstat = &ohci->regs->roothub.portstatus [port];
	uint32_t	temp = 0;
	uint16_t	now = readl(&ohci->regs->fmnumber);
	uint16_t	reset_done = now + PORT_RESET_MSEC;
	int		limit_1 = PORT_RESET_MSEC / PORT_RESET_HW_MSEC;

	/* build a "continuous enough" reset signal, with up to
	 * 3msec gap between pulses.  scheduler HZ==100 must work;
	 * this might need to be deadline-scheduled.
	 */
	do {
		int limit_2;

		/* spin until any current reset finishes */
		limit_2 = PORT_RESET_HW_MSEC * 2;
		while (--limit_2 >= 0) {
			temp = readl(portstat);
			/* handle e.g. CardBus eject */
			if (temp == ~(uint32_t)0)
				return -1;
			if (!(temp & RH_PS_PRS))
				break;
			udelay (500);
		}

		/* timeout (a hardware error) has been observed when
		 * EHCI sets CF while this driver is resetting a port;
		 * presumably other disconnect paths might do it too.
		 */
		if (limit_2 < 0) {
			DBG("port[%d] reset timeout, stat %08lx\n",
				port, temp);
			break;
		}

		if (!(temp & RH_PS_CCS))
			break;
		if (temp & RH_PS_PRSC)
			writel(RH_PS_PRSC, portstat);

		/* start the next reset, sleep till it's probably done */
		writel(RH_PS_PRS, portstat);
		mdelay(PORT_RESET_HW_MSEC);
		now = readl(&ohci->regs->fmnumber);
	} while (tick_before(now, reset_done) && --limit_1 >= 0);

	/* caller synchronizes using PRSC ... and handles PRS
	 * still being set when this returns.
	 */
	mdelay(1);
	return 0;
}

static inline int ohci_reset_port(struct usb_hcd *hcd, int port)
{
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);

	return reset_port(ohci, port);	
}

static struct ohci_td * ohci_alloc_td (struct ohci_hcd *hc, struct ohci_ed *ed)
{
	unsigned long	dma;
	struct ohci_td	*td;

	td = malloc_dma(sizeof(*td), 16);
	if (td) {
		dma = virt_to_bus(td);

		/* in case hc fetches it, make it look dead */
		memset (td, 0, sizeof(*td));
		td->hwNextTD = cpu_to_hc32 (hc, dma);
		td->dma = dma;
		td->ed = ed;
	}
	
	return td;
}

static inline void ohci_fill_td (struct ohci_hcd *ohci, struct ohci_td *td,
				uint32_t info, unsigned long data, unsigned int len)
{
	info |= TD_DI_SET(6);

	td->hwINFO = cpu_to_hc32(ohci, info);
	td->hwCBP = cpu_to_hc32(ohci, data);

	if(data)
		td->hwBE = cpu_to_hc32(ohci, data + len - 1);
	else 
		td->hwBE = 0;

	td->len = len;

	td->ed->hwTailP = cpu_to_hc32(ohci, td->dma);
	wmb();
}

static void ohci_free_td(struct ohci_td *td)
{
	free_dma(td, sizeof(*td));
}

static void ohci_free_ed(struct ohci_ed *ed)
{
	free_dma(ed, sizeof(*ed));
}

static struct ohci_ed * ohci_alloc_ed(struct urb *urb, struct ohci_hcd *ohci)
{
	struct ohci_ed *ed;
	struct ohci_td *td;
	struct usb_host_endpoint *ep = urb->ep;
	uint32_t info;
	int	is_out;

	ed = malloc_dma(sizeof(*ed), 16);
	if (!ed)
		goto err_malloc_ed;

	ed->dma = virt_to_bus(ed);

	td = ohci_alloc_td(ohci, ed);
	if (!td)
		goto err_malloc_td;

	is_out = (usb_ep_dir(ep) == USB_DIR_OUT);
	info = urb->udev->devnum;

	ed->dummy_td = td;
	ed->hwTailP = cpu_to_hc32 (ohci, td->dma);
	ed->hwHeadP = ed->hwTailP;	/* ED_C, ED_H zeroed */

	info |= (usb_ep_num(ep)) << 7;
	info |= le16_to_cpu(ep->desc.wMaxPacketSize) << 16;
	
	ed->hwINFO = cpu_to_hc32(ohci, info);

	INIT_LIST_HEAD(&ed->urbp_list);
	return ed;

	ohci_free_td(td);
err_malloc_td:
	ohci_free_ed(ed);
err_malloc_ed:
	return NULL;	
}

static void ohci_add_urbp_to_ed(struct ohci_urb_priv *urbp, struct ohci_ed *ed)
{
	list_add_tail(&urbp->list, &ed->urbp_list);
}

static void ohci_del_urbp_from_ed(struct ohci_urb_priv *urbp)
{
	list_del(&urbp->list);
}

static void ohci_add_td_to_urbp(struct ohci_td *td, struct ohci_urb_priv *urbp)
{
	list_add_tail(&td->list, &urbp->td_list);
}

static void ohci_del_td_from_urbp(struct ohci_td *td)
{
	list_del(&td->list);
}

static int ohci_submit_bulk(struct urb *urb, struct ohci_ed *ed)
{
	struct ohci_hcd *ohci = hcd_to_ohci(urb->udev->hcd);
	struct ohci_td *td;
	unsigned int len = urb->transfer_buffer_length;
	unsigned long data = urb->transfer_dma;
	uint32_t *plink;
	uint32_t info;
	struct ohci_urb_priv *urbp = urb->hcpriv;
	int is_out;

	is_out = (usb_ep_dir(urb->ep) == USB_DIR_OUT);

	info = is_out ?
		TD_CC | TD_T_TOGGLE | TD_DP_OUT | TD_R:
		TD_CC | TD_T_TOGGLE | TD_DP_IN | TD_R;

	urbp->first_td = td = ed->dummy_td;
	plink = NULL;

	/*
	 * Build the DATA TDs
	 */
	do {
		unsigned int pktsze = 4096;
		
		if (len <= pktsze) {		/* The last data packet */
			pktsze = len;
		}

		if (plink) {
	 		td = ohci_alloc_td(ohci, ed);
			if (!td)
				goto err_td_malloc;
			*plink = cpu_to_hc32(ohci, td->dma);
		}

		ohci_fill_td(ohci, td, info, data, pktsze);
		ohci_add_td_to_urbp(td, urbp);
		plink = &td->hwNextTD;

		data += pktsze;
		len -= pktsze;
	} while (len > 0);

	urbp->last_td = td;
	/*
	 * Build the new dummy TD and activate the old one
	 */
	td = ohci_alloc_td(ohci, ed);
	if (!td)
		goto err_dummytd_malloc;

	*plink = cpu_to_hc32(ohci, td->dma);
	ed->dummy_td = td;
	ed->hwTailP = cpu_to_hc32(ohci, td->dma);
	wmb();

	writel(OHCI_BLF, &ohci->regs->cmdstatus); 
	return 0;

	ohci_free_td(ed->dummy_td);
err_dummytd_malloc:
	list_for_each_entry(td, &urbp->td_list, list) {
		ohci_del_td_from_urbp(td);
		ohci_free_td(td);
	}
err_td_malloc:
	list_for_each_entry(td, &urbp->td_list, list) {
		ohci_del_td_from_urbp(td);
		ohci_free_td(td);
	}

	return -ENOMEM;
}

static int ohci_submit_control(struct urb *urb, struct ohci_ed *ed)
{
	struct ohci_hcd *ohci = hcd_to_ohci(urb->udev->hcd);
	struct ohci_td *td;
	unsigned int len = urb->transfer_buffer_length;
	unsigned long data = urb->transfer_dma;
	uint32_t *plink;
	uint32_t info;
	struct ohci_urb_priv *urbp = urb->hcpriv;
	int is_out;
	unsigned int pktsze;

	is_out = (usb_ep_dir(urb->ep) == USB_DIR_OUT);

	info = TD_CC | TD_DP_SETUP | TD_T_DATA0;
	urbp->first_td = td = ed->dummy_td;

	ohci_fill_td(ohci, td, info, urb->setup_dma, 8);

	ohci_add_td_to_urbp(td, urbp);
	plink = &td->hwNextTD;

	/*
	 * Build the DATA TD
	 */
		
	pktsze = len;

	td = ohci_alloc_td(ohci, ed);
	if (!td)
		goto err_td_malloc;

	*plink = cpu_to_hc32(ohci, td->dma);

	info = TD_CC | TD_R | TD_T_DATA1;
	info |= is_out ? TD_DP_OUT : TD_DP_IN;
	ohci_fill_td(ohci, td, info, data, pktsze);
	ohci_add_td_to_urbp(td, urbp);
	plink = &td->hwNextTD;

	/*
	 * Build the final TD for control status 
	 */
	urbp->last_td = td = ohci_alloc_td(ohci, ed);
	if (!td) 
		goto err_td_malloc;

	*plink = cpu_to_hc32(ohci, td->dma);

	/* Change direction for the status transaction */
	info = (is_out || (urb->transfer_buffer_length == 0)) ?
		TD_CC | TD_DP_IN | TD_T_DATA1:
		TD_CC | TD_DP_OUT | TD_T_DATA1;

	ohci_add_td_to_urbp(td, urbp);
	ohci_fill_td(ohci, td, info, 0, 0);
	plink = &td->hwNextTD;

	/*
	 * Build the new dummy TD and activate the old one
	 */
	td = ohci_alloc_td(ohci, ed);
	if (!td)
		goto err_dummytd_malloc;

	*plink = cpu_to_hc32(ohci, td->dma);
	ed->dummy_td = td;
	ed->hwTailP = cpu_to_hc32(ohci, td->dma);
	writel(OHCI_CLF, &ohci->regs->cmdstatus); 
	wmb();

	return 0;

	ohci_free_td(ed->dummy_td);
err_dummytd_malloc:
	list_for_each_entry(td, &urbp->td_list, list) {
		ohci_del_td_from_urbp(td);
		ohci_free_td(td);
	}
err_td_malloc:
	list_for_each_entry(td, &urbp->td_list, list) {
		ohci_del_td_from_urbp(td);
		ohci_free_td(td);
	}

	return -ENOMEM;
}


static int ohci_enqueue_urb(struct usb_hcd *hcd, struct urb *urb)
{
	struct ohci_hcd *ohci;
	struct ohci_urb_priv *urbp;
	struct ohci_ed *ed;
	int ret = -ENOMEM;
	uint32_t info;

	ohci = hcd_to_ohci(hcd);		
	urbp = malloc(sizeof(*urbp));
	
	if (!urbp)
		goto err_urb_malloc;

	INIT_LIST_HEAD(&urbp->td_list);
	urb->hcpriv = urbp;

	if (!urb->ep->hcpriv) {
		/* Create a new ED for this endpoint */
		ed = urb->ep->hcpriv = ohci_alloc_ed(urb, ohci);
		if (!urb->ep->hcpriv)
			goto err_hcpriv_malloc;	
		if (usb_ep_xfertype(urb->ep) == USB_ENDPOINT_XFER_CONTROL) {
			/* Check to see if this is the first control ed */
			if (!readl(&ohci->regs->ed_controlhead)) {
				writel(0, &ohci->regs->ed_controlcurrent);
				writel(cpu_to_hc32(ohci, ed->dma), &ohci->regs->ed_controlhead);
				ohci->hc_control |= OHCI_CTRL_CLE;
				writel(ohci->hc_control, &ohci->regs->control);
				wmb();
			} else {
				ohci->last_control_ed->hwNextED = cpu_to_hc32(ohci, ed->dma);
			}
			ohci->last_control_ed = ed;
		} else {
			/* Check to see if this is the first ed */
			if (!readl(&ohci->regs->ed_bulkhead)) {
				writel(0, &ohci->regs->ed_bulkcurrent);
				writel(cpu_to_hc32(ohci, ed->dma), &ohci->regs->ed_bulkhead);
				ohci->hc_control |= OHCI_CTRL_BLE;
				writel(ohci->hc_control, &ohci->regs->control);
				wmb();
			} else {
				ohci->last_bulk_ed->hwNextED = cpu_to_hc32(ohci, ed->dma);
			}
			ohci->last_bulk_ed = ed;
		}
	}
	
	/* Update the ed's fa */
	ed = (struct ohci_ed *)urb->ep->hcpriv;
	info = hc32_to_cpu(ohci, ed->hwINFO);
	info &= ~((1 << 7) - 1);
	info |= (urb->udev->devnum);
	ed->hwINFO = cpu_to_hc32(ohci, info);
	wmb();

	/* Add the urbp to the ED's list */
	ohci_add_urbp_to_ed(urbp, ed);
	
	if (usb_ep_xfertype(urb->ep) == USB_ENDPOINT_XFER_CONTROL) {
		if ((ret = ohci_submit_control(urb, ed)) < 0)
			goto err_submit_urb;
	} else {
		if ((ret = ohci_submit_bulk(urb, ed)) < 0)
			goto err_submit_urb;	
	}

	return 0;

	return ret;
err_submit_urb:
	ohci_free_ed(urb->ep->hcpriv);
err_hcpriv_malloc:
	free(urbp);
err_urb_malloc:
	return ret;
}

static void ohci_unlink_urb(struct urb *urb __unused)
{
	struct ohci_td *td;
	struct ohci_urb_priv *urbp;
	struct ohci_hcd *ohci;

	ohci = hcd_to_ohci(urb->udev->hcd);
	urbp = urb->hcpriv;
	list_for_each_entry(td, &urbp->td_list, list) {
		list_del(&td->list);

		/* Wait to make sure this td is unlinked from the schedule by 
		 * the HC. Removing this causes gPXE to hang
		 */
		mdelay(2);
		ohci_free_td(td);
	}

	ohci_del_urbp_from_ed(urbp);
	free(urbp);
}

static int ohci_urb_status(struct urb *urb)
{
	int ret = USB_URB_STATUS_COMPLETE;
	struct ohci_urb_priv *urbp = urb->hcpriv;
	struct ohci_td *td;
	struct list_head *p;
	struct ohci_hcd *ohci = hcd_to_ohci(urb->udev->hcd);
	uint32_t info;
	uint32_t be, cbp;

	/* Check for the status of the first td *///
	info = hc32_to_cpu(ohci, urbp->first_td->hwINFO);
	if (TD_CC_GET(info) != 0) {
		if (TD_CC_GET(info) == 0xf) {
			ret = USB_URB_STATUS_INPROGRESS;
			goto out;
		} else {
			ret = USB_URB_STATUS_ERROR;
			goto out;
		}
	}

	/* Check if URB is complete. Update urb->actual_length */
	/* Optimize by looking at last_td directly. */
	td = urbp->last_td;
	info = hc32_to_cpu(ohci, urbp->last_td->hwINFO);
	if (TD_CC_GET(info) == 0) {
		be = hc32_to_cpu(ohci, td->hwBE);
		cbp = hc32_to_cpu(ohci, td->hwCBP);
		if (cbp == 0) {
				urb->actual_length = urb->transfer_buffer_length;
				return USB_URB_STATUS_COMPLETE;
		}
	}

	urb->actual_length = 0; 
	list_for_each(p, &urbp->td_list) {
		unsigned long cbp, be;

		td = list_entry(p, struct ohci_td, list);
		info = hc32_to_cpu(ohci, td->hwINFO);
		cbp = hc32_to_cpu(ohci, td->hwCBP);
		be = hc32_to_cpu(ohci, td->hwBE);

		if (TD_CC_GET(info) != 0) {
			ret = USB_URB_STATUS_INPROGRESS;
			goto out;
		}

		if (cbp != 0)
			urb->actual_length += td->len - (be - cbp);
		else
			urb->actual_length += td->len;
	}

out:
	return ret;
}

static struct hc_driver ohci_driver = {
	.urb_status = ohci_urb_status,
	.enqueue_urb = ohci_enqueue_urb,
	.reset_port = ohci_reset_port,
	.unlink_urb = ohci_unlink_urb,
};

static int ohci_start(struct ohci_hcd *ohci)
{
	uint32_t temp, mask;

	temp = readl(&ohci->regs->fminterval);
	ohci->fminterval = temp & 0x3fff;

	if (ohci->fminterval != FI)
		DBG("fminterval delta %lx\n",ohci->fminterval - FI);
	ohci->fminterval |= FSMP (ohci->fminterval) << 16;

	switch (ohci->hc_control & OHCI_CTRL_HCFS) {
	case OHCI_USB_OPER:
		temp = 0;
		break;
	case OHCI_USB_SUSPEND:
	case OHCI_USB_RESUME:
		ohci->hc_control &= OHCI_CTRL_RWC;
		ohci->hc_control |= OHCI_USB_RESUME;
		temp = 10 /* msec wait */;
		break;
	// case OHCI_USB_RESET:
	default:
		ohci->hc_control &= OHCI_CTRL_RWC;
		ohci->hc_control |= OHCI_USB_RESET;
		temp = 50 /* msec wait */;
		break;
	}
	writel(ohci->hc_control, &ohci->regs->control);
	// flush the writes
	(void) readl(&ohci->regs->control);
	mdelay(temp);
	
	/* HC Reset requires max 10 us delay */
	writel (OHCI_HCR,  &ohci->regs->cmdstatus);
	temp = 30;	/* ... allow extra time */
	while ((readl(&ohci->regs->cmdstatus) & OHCI_HCR) != 0) {
		if (--temp == 0) {
			DBG("USB HC reset timed out!\n");
			return -1;
		}
		udelay (1);
	}

	/* now we're in the SUSPEND state ... must go OPERATIONAL
	 * within 2msec else HC enters RESUME
	 * Tell the controller where the control and bulk lists are
	 * The lists are empty now. 
	 */

	writel(0, &ohci->regs->ed_controlhead);
	writel(0, &ohci->regs->ed_bulkhead);

	periodic_reinit(ohci);

	ohci->hc_control &= OHCI_CTRL_RWC;
	ohci->hc_control |= OHCI_CTRL_CBSR | OHCI_USB_OPER;
	writel(ohci->hc_control, &ohci->regs->control);

	/* wake on ConnectStatusChange, matching external hubs */
	writel (RH_HS_DRWE, &ohci->regs->roothub.status);

	/* Choose the interrupts we care about now, others later on demand */
	mask = OHCI_INTR_INIT;
	writel (~0, &ohci->regs->intrstatus);
	writel (mask, &ohci->regs->intrenable);

	temp = roothub_a (ohci);
	writel(RH_HS_LPSC, &ohci->regs->roothub.status);
	writel((temp & RH_A_NPS) ? 0 : RH_B_PPCM, &ohci->regs->roothub.b);

	// flush those writes
	(void) readl(&ohci->regs->control);

	return 0;
}
static int ohci_hcd_pci_probe(struct pci_device *pci,
					const struct pci_device_id *id __unused)
{
	struct usb_hcd *hcd;
	struct ohci_hcd *ohci;
	struct usb_device *udev;

	int retval = -ENOMEM;
	unsigned int port;

	if (pci->class != PCI_CLASS_SERIAL_USB_OHCI)
		return -ENOTTY;

	if (once_initialized)
		return -1;

	hcd = (struct usb_hcd *) malloc(sizeof(struct usb_hcd));
	if (!hcd) {
		DBG("Failed allocating memory for usb_hcd\n");
		goto err_hcd_malloc;
	}

	ohci = hcd->hcpriv = malloc(sizeof(struct ohci_hcd));
	if (!hcd->hcpriv) {
		DBG("Failed allocating memory for ohci_hcd\n");
		goto err_ohci_hcd_malloc;
	}

	pci_set_drvdata(pci, hcd);
	hcd->driver = &ohci_driver;

	hcd->res_addr = pci_bar_start(pci, PCI_BASE_ADDRESS_0);
	hcd->res_size = pci_bar_size(pci, PCI_BASE_ADDRESS_0);

	ohci->regs = ioremap(hcd->res_addr, hcd->res_size);
	if (ohci->regs == NULL) {
		DBG("error mapping memory\n");
		retval = -EFAULT;
		goto err_ioremap;
	}

	DBG("OHCI Adapter Found at 0x%lx\n", hcd->res_addr);

	ohci_init(ohci);

	ohci_start(ohci);
	/* Look for devices at ports */
	
	mdelay(100);	
	
	for (port = 0; port < ohci->num_ports; port++) {
		uint32_t status = roothub_portstatus (ohci, port);
		if (!(status & RH_PS_CCS)) {
			DBG("No device on port %d\n", port + 1);
			continue;
		}
		udev = usb_alloc_dev();
		if(!udev)
			goto err_usb_alloc_dev;
		udev->hcd = hcd;
		/* Tell the usb core about the new device */
		if (usb_dev_init(udev, port) < 0) 
			DBG("USB : Error handing off device to usbcore\n");
		
		once_initialized = 1;
	}

	
	return 0;

	usb_free_dev(udev);
err_usb_alloc_dev:
	iounmap(ohci->regs);
err_ioremap:
	free(hcd->hcpriv);
err_ohci_hcd_malloc:
	free(hcd);
err_hcd_malloc:

	return retval;
}

void ohci_hcd_pci_remove(struct pci_device *pci __unused)
{
	struct usb_hcd *hcd;
	struct ohci_hcd *ohci;

	hcd = pci_get_drvdata(pci);

	/* Remove all devices hanging off this HC */
	usb_hcd_remove_all_devices(hcd);

	ohci = hcd_to_ohci(hcd);

	/* Stop the schedule, i.e stop the control and the bulk lists*/
	writel(0, &ohci->regs->control);

	free_dma(ohci->hcca, sizeof(*ohci->hcca));

	free(ohci);
	free(hcd);
}

static struct pci_device_id ohci_hcd_pci_ids[] = {
	PCI_ROM(0xffff, 0xffff, "OHCI HCD", "OHCI USB Controller", 0),
};

struct pci_driver ohci_hcd_pci_driver __pci_driver = {
	.ids = ohci_hcd_pci_ids,
	.id_count = (sizeof(ohci_hcd_pci_ids) / sizeof(ohci_hcd_pci_ids[0])),
	.probe = ohci_hcd_pci_probe,
	.remove = ohci_hcd_pci_remove,
};

