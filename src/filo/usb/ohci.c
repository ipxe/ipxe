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

#define DEBUG_TD 0
#define DEBUG_ED 0


#include "usb.h"
#include "ohci.h"


extern int usec_offset;

ohci_regs_t *ohci_regs;

// It will clear the enable bit
void ohc_clear_stat(uchar dev)
{
        uint32_t value;
	ohci_regs = (ohci_regs_t *)hc_base[dev];
	
	value = readl(&ohci_regs->cmdstatus);
	writel(value, &ohci_regs->cmdstatus);
}
	
void clear_oport_stat(uint32_t port)
{
        uint32_t value;

        value = readl(port);
        writel(value, port);
}

void oport_suspend( uint32_t port)
{
        writel( RH_PS_PSS, port);

}
void oport_wakeup( uint32_t port)
{
        writel( RH_PS_POCI, port);

}
#if 0
void oport_resume( uint32_t port)
{
        uint32_t value;

        value = readl(port);
        value |= 0x40;
        writel(value, port);
        udelay(20000+usec_offset);
        value &= ~0x40;
        writel(value, port);

        do {
                value = readl(port);
        } while(value & 0x40);
}       
#endif
void oport_enable( uint32_t port)
{
	uint32_t value;
	
	value = readl(port);
	
	if((value & RH_PS_CCS)) {  // if connected
		writel( RH_PS_PES, port);
		udelay(10);
		writel( RH_PS_PESC, port); // Clear Change bit
	}

}



void oport_disable( uint32_t port)
{
        writel( RH_PS_CCS, port);
}

void oport_reset(uint32_t port)
{

        uint32_t value;

        writel( RH_PS_PRS, port);
        do {
               value = readl( port );
        } while (!(value & RH_PS_PRSC) );
	writel(RH_PS_PRSC, port);  //Clear Change bit

}

void oport_reset_long(uint32_t port)
{
	oport_reset(port);
}

#if 0

int ohc_stop(uchar dev)
{
        unsigned short tmp;
	uint32_t ctl;
       
	ohci_regs = hc_base[dev];

	ctl = readl( &ohci_regs->control);
        ctl &= ~(OHCI_CTRL_PLE|OHCI_CTRL_CLE|OHCI_CTRL_BLE|OHCI_CTRL_IE);
        writel (ctl, &ohci_regs->control); 
        
        return(0);
}  
#endif


#define MAX_OHCI_TD 32

ohci_td_t *_ohci_td;
uint8_t	_ohci_td_tag[MAX_OHCI_TD]; //1: used, 0:unused

void init_ohci_td(){
	_ohci_td = allot2(sizeof(ohci_td_t)*MAX_OHCI_TD, 0x1f); // 32 byte aligna
	if(_ohci_td==0) {
		printf("init_ohci_td: NOMEM\n");
	}
	memset(_ohci_td_tag, 0, sizeof(_ohci_td_tag));
}

ohci_td_t *td_alloc(ohci_t *ohci, int memflag){
	int i;
	ohci_td_t *td;
	for(i = 0; i< MAX_OHCI_TD; i++ ) {
		if(_ohci_td_tag[i]==1) continue;
		td = &_ohci_td[i];
		memset(td, 0, sizeof(ohci_td_t));
		td->td_dma = (void *)virt_to_phys(td);
		_ohci_td_tag[i] = 1;
		return td;
	}
	printf("td_alloc: no free slot\n");
	return 0;
}

int td_free(ohci_t *ohci, ohci_td_t *td) {
	int i;
	for(i = 0; i< MAX_OHCI_TD; i++ ) {
                if(_ohci_td_tag[i]==0) continue;
		if(&_ohci_td[i] == td ) {
			_ohci_td_tag[i] = 0;
			return 1;
		}
        }
        return 0;
}


struct ohci_td *
dma_to_td (struct ohci * hc, void *td_dma)
{
        int i;  
        ohci_td_t *td;
        for(i = 0; i< MAX_OHCI_TD; i++ ) {
                if(_ohci_td_tag[i]==0) continue;
		td = &_ohci_td[i];
                if(td->td_dma == td_dma ) {
                        return td;
                }
        }
	printf("dma_to_td: can not find td\n");
        return 0;

}

ohci_t _ohci_x[MAX_CONTROLLERS];

void ohci_init(void)
{       
	init_ohci_td();
}

static int ohci_get_current_frame_number (struct usbdev *usb_dev)
{
        ohci_t * ohci = &_ohci_x[usb_dev->controller]; 
 
        return le16_to_cpu (ohci->hcca->frame_no);
}
 


static u32 roothub_a (struct ohci *hc)
        { return readl (&hc->regs->roothub.a); }
static inline u32 roothub_b (struct ohci *hc)
        { return readl (&hc->regs->roothub.b); }
static inline u32 roothub_status (struct ohci *hc)
        { return readl (&hc->regs->roothub.status); }
static u32 roothub_portstatus (struct ohci *hc, int i)
        { return readl (&hc->regs->roothub.portstatus[i]);}
 
 
#if DEBUG_USB==1

#define OHCI_VERBOSE_DEBUG

# define dbg(...) \
    do { printf(__VA_ARGS__); printf("\n"); } while (0)

static void urb_print (struct urb * urb, char * str, int small)
{
        unsigned int pipe= urb->pipe;
        
        if (!urb->dev ) { 
                dbg("%s URB: no dev", str);
                return;
        }

#ifndef OHCI_VERBOSE_DEBUG
        if (urb->status != 0)
#endif
        dbg("%s URB:[%4x] dev:%2d,ep:%2d-%c,type:%s,flags:%4x,len:%d/%d,stat:%d(%x)",
                        str,
                        ohci_get_current_frame_number (urb->dev),
                        usb_pipedevice (pipe),
                        usb_pipeendpoint (pipe),
                        usb_pipeout (pipe)? 'O': 'I',
                        usb_pipetype (pipe) < 2? (usb_pipeint (pipe)? "INTR": "ISOC"):
                                (usb_pipecontrol (pipe)? "CTRL": "BULK"),
                        urb->transfer_flags,
                        urb->actual_length,
                        urb->transfer_buffer_length,
                        urb->status, urb->status);
#ifdef  OHCI_VERBOSE_DEBUG
//        if (!small) {
                int i, len;

                if (usb_pipecontrol (pipe)) {
                        printf ("ohci.c: cmd(8):");
                        for (i = 0; i < 8 ; i++)
                                printf (" %02x", ((u8 *) urb->setup_packet) [i]);
                        printf ("\n");
                }
                if (urb->transfer_buffer_length > 0 && urb->transfer_buffer) {
                         printf("ohci.c: data(%d/%d):",
                                urb->actual_length,
                                urb->transfer_buffer_length);
                        len = usb_pipeout (pipe)?
                                                urb->transfer_buffer_length: urb->actual_length;
                        for (i = 0; i < 16 && i < len; i++)
                                printf (" %02x", ((u8 *) urb->transfer_buffer) [i]);
                        printf ("%s stat:%d\n", i < len? "...": "", urb->status);
                }
//        }
#endif
}

/* just for debugging; prints non-empty branches of the int ed tree inclusive iso eds*/
void ep_print_int_eds (ohci_t * ohci, char * str) {
        int i, j;
         u32 * ed_p;
        for (i= 0; i < 32; i++) {
                j = 5;
                ed_p = &(ohci->hcca->int_table [i]);
                if (*ed_p == 0)
                    continue;
                printf ("ohci.c: %s branch int %2d(%2x):", str, i, i);
#if 0
                while (*ed_p != 0 && j--) {
                        ed_t *ed = dma_to_ed (ohci, le32_to_cpup(ed_p));
                        printk (" ed: %4x;", ed->hwINFO);
                        ed_p = &ed->hwNextED;
                }
#endif
                printf ("\n");
        }
}
static void ohci_dump_intr_mask (char *label, u32 mask)
{
        dbg ("%s: 0x%08x%s%s%s%s%s%s%s%s%s",
                label,
                mask,
                (mask & OHCI_INTR_MIE) ? " MIE" : "",
                (mask & OHCI_INTR_OC) ? " OC" : "",
                (mask & OHCI_INTR_RHSC) ? " RHSC" : "",
                (mask & OHCI_INTR_FNO) ? " FNO" : "",
                (mask & OHCI_INTR_UE) ? " UE" : "",
                (mask & OHCI_INTR_RD) ? " RD" : "",
                (mask & OHCI_INTR_SF) ? " SF" : "",
                (mask & OHCI_INTR_WDH) ? " WDH" : "",
                (mask & OHCI_INTR_SO) ? " SO" : ""
                );
}
static void maybe_print_eds (char *label, u32 value)
{
        if (value)
                dbg ("%s %08x", label, value);
}
static char *hcfs2string (int state)
{
        switch (state) {
                case OHCI_USB_RESET:    return "reset";
                case OHCI_USB_RESUME:   return "resume";
                case OHCI_USB_OPER:     return "operational";
                case OHCI_USB_SUSPEND:  return "suspend";
        }
        return "?";
}
// dump control and status registers
static void ohci_dump_status (ohci_t *controller)
{
        struct ohci_regs        *regs = controller->regs;
        u32                   temp;

        temp = readl (&regs->revision) & 0xff;
        if (temp != 0x10)
                dbg ("spec %d.%d", (temp >> 4), (temp & 0x0f));

        temp = readl (&regs->control);
        dbg ("control: 0x%08x%s%s%s HCFS=%s%s%s%s%s CBSR=%d", temp,
                (temp & OHCI_CTRL_RWE) ? " RWE" : "",
                (temp & OHCI_CTRL_RWC) ? " RWC" : "",
                (temp & OHCI_CTRL_IR) ? " IR" : "",
                hcfs2string (temp & OHCI_CTRL_HCFS),
                (temp & OHCI_CTRL_BLE) ? " BLE" : "",
                (temp & OHCI_CTRL_CLE) ? " CLE" : "",
                (temp & OHCI_CTRL_IE) ? " IE" : "",
                (temp & OHCI_CTRL_PLE) ? " PLE" : "",
                temp & OHCI_CTRL_CBSR
                );

        temp = readl (&regs->cmdstatus);
        dbg ("cmdstatus: 0x%08x SOC=%d%s%s%s%s", temp,
                (temp & OHCI_SOC) >> 16,
                (temp & OHCI_OCR) ? " OCR" : "",
                (temp & OHCI_BLF) ? " BLF" : "",
                (temp & OHCI_CLF) ? " CLF" : "",
                (temp & OHCI_HCR) ? " HCR" : ""
                );

        ohci_dump_intr_mask ("intrstatus", readl (&regs->intrstatus));
        ohci_dump_intr_mask ("intrenable", readl (&regs->intrenable));
        // intrdisable always same as intrenable
        // ohci_dump_intr_mask ("intrdisable", readl (&regs->intrdisable));

        maybe_print_eds ("ed_periodcurrent", readl (&regs->ed_periodcurrent));

        maybe_print_eds ("ed_controlhead", readl (&regs->ed_controlhead));
        maybe_print_eds ("ed_controlcurrent", readl (&regs->ed_controlcurrent));

        maybe_print_eds ("ed_bulkhead", readl (&regs->ed_bulkhead));
        maybe_print_eds ("ed_bulkcurrent", readl (&regs->ed_bulkcurrent));

        maybe_print_eds ("donehead", readl (&regs->donehead));
}

static void ohci_dump_roothub (ohci_t *controller, int verbose)
{
        u32                   temp, ndp, i;

        temp = roothub_a (controller);
        if (temp == ~(u32)0)
                return;
        ndp = (temp & RH_A_NDP);

        if (verbose) {
                dbg ("roothub.a: %08x POTPGT=%d%s%s%s%s%s NDP=%d", temp,
                        ((temp & RH_A_POTPGT) >> 24) & 0xff,
                        (temp & RH_A_NOCP) ? " NOCP" : "",
                        (temp & RH_A_OCPM) ? " OCPM" : "",
                        (temp & RH_A_DT) ? " DT" : "",
                        (temp & RH_A_NPS) ? " NPS" : "",
                        (temp & RH_A_PSM) ? " PSM" : "",
                        ndp
                        );
                temp = roothub_b (controller);
                dbg ("roothub.b: %08x PPCM=%04x DR=%04x",
                        temp,
                        (temp & RH_B_PPCM) >> 16,
                        (temp & RH_B_DR)
                        );
                temp = roothub_status (controller);
                dbg ("roothub.status: %08x%s%s%s%s%s%s",
                        temp,
                        (temp & RH_HS_CRWE) ? " CRWE" : "",
                        (temp & RH_HS_OCIC) ? " OCIC" : "",
                        (temp & RH_HS_LPSC) ? " LPSC" : "",
                        (temp & RH_HS_DRWE) ? " DRWE" : "",
                        (temp & RH_HS_OCI) ? " OCI" : "",
                        (temp & RH_HS_LPS) ? " LPS" : ""
                        );
        }

        for (i = 0; i < ndp; i++) {
                temp = roothub_portstatus (controller, i);
                dbg ("roothub.portstatus [%d] = 0x%08x%s%s%s%s%s%s%s%s%s%s%s%s",
                        i,
                        temp,
                        (temp & RH_PS_PRSC) ? " PRSC" : "",
                        (temp & RH_PS_OCIC) ? " OCIC" : "",
                        (temp & RH_PS_PSSC) ? " PSSC" : "",
                        (temp & RH_PS_PESC) ? " PESC" : "",
                        (temp & RH_PS_CSC) ? " CSC" : "",

                        (temp & RH_PS_LSDA) ? " LSDA" : "",
                        (temp & RH_PS_PPS) ? " PPS" : "",
                        (temp & RH_PS_PRS) ? " PRS" : "",
                        (temp & RH_PS_POCI) ? " POCI" : "",
                        (temp & RH_PS_PSS) ? " PSS" : "",

                        (temp & RH_PS_PES) ? " PES" : "",
                        (temp & RH_PS_CCS) ? " CCS" : ""
                        );
        }
}

static void ohci_dump (ohci_t *controller, int verbose)
{
        dbg ("OHCI controller usb-%x state", controller->regs);

        // dumps some of the state we know about
        ohci_dump_status (controller);
        if (verbose)
                ep_print_int_eds (controller, "hcca");
        dbg ("hcca frame #%04x", controller->hcca->frame_no);
        ohci_dump_roothub (controller, 1);
}
void ohci_dump_x(uchar controller)
{
	ohci_t *ohci;
        ohci = &_ohci_x[controller];
        ohci_dump (ohci, 1);
}
#endif

/* link an ed into one of the HC chains */

/* ED is only enqueued and dequeued by HCD 
	So ep_link may only to be called two times for every device (function) -- ed, one for controled and one for bulked 
	one ohci may have several controled and bulked.
*/

int ep_link (ohci_t * ohci, ed_t * edi)
{
        volatile ed_t * ed = edi;

        ed->state = ED_OPER;
	
        switch (ed->type) {
        case PIPE_CONTROL:
                ed->hwNextED = 0;
                if (ohci->ed_controltail == NULL) {
//			debug("ep_link control 21 ed->dma = %x\n", (uint32_t)ed->dma);
                        writel ((uint32_t)ed->dma, &ohci->regs->ed_controlhead);
                } else {
//			debug("ep_link control 22 ed->dma = %x\n", (uint32_t)ed->dma);
                        ohci->ed_controltail->hwNextED = cpu_to_le32 ((uint32_t)ed->dma);
                }
                ed->ed_prev = ohci->ed_controltail;
                if (!ohci->ed_controltail) {
			/* enable control ed list */
                        ohci->hc_control |= OHCI_CTRL_CLE;  //5
                        writel (ohci->hc_control, &ohci->regs->control);
                }
                ohci->ed_controltail = edi;
                break;

        case PIPE_BULK:
                ed->hwNextED = 0;
                if (ohci->ed_bulktail == NULL) {
		//	debug("ep_link control 31 ed->dma = %x\n", (uint32_t)ed->dma);
                        writel ((uint32_t)ed->dma, &ohci->regs->ed_bulkhead);
                } else {
		//	debug("ep_link control 32 ed->dma = %x\n", (uint32_t)ed->dma);
                        ohci->ed_bulktail->hwNextED = cpu_to_le32 ((uint32_t)ed->dma);
                }
                ed->ed_prev = ohci->ed_bulktail;
                if (!ohci->ed_bulktail) {
			/* enable bulk ed list */
                        ohci->hc_control |= OHCI_CTRL_BLE;  //5
                        writel (ohci->hc_control, &ohci->regs->control);
                }
                ohci->ed_bulktail = edi;
                break;
        }
        return 0;
}
/* add/reinit an endpoint; this should be done once at the usb_set_configuration command,
 * but the USB stack is a little bit stateless  so we do it at every transaction
 * if the state of the ed is ED_NEW then a dummy td is added and the state is changed to ED_UNLINK
 * in all other cases the state is left unchanged
 * the ed info fields are setted anyway even though most of them should not change */

ed_t * ep_add_ed (
        usbdev_t * usb_dev,
        unsigned int pipe,
        int interval,
        int load,
        int mem_flags
)
{
        ohci_t * ohci = &_ohci_x[usb_dev->controller];
        ohci_td_t * td;
        ed_t * ed;
        unsigned long flags;
	int i;

	/* We use preallocate ed in ohci struct
		numbering rule ???
	*/
	i = (usb_pipeendpoint (pipe) << 1) |(usb_pipecontrol (pipe)? 0: usb_pipeout (pipe));
        ed = (ed_t *)&ohci->ed[i];

//	debug("ep_add_ed: usb_dev port=%x, controller = %d ohci=%x ohci->ed=%x ed=%x ed->dma=%x\n", usb_dev->port,usb_dev->controller, ohci, ohci->ed, ed, ed->dma);

        if (ed->state == ED_NEW) {
                ed->hwINFO = cpu_to_le32 (OHCI_ED_SKIP); /* skip ed */
                /* dummy td; end of td list for ed */
                td = td_alloc (ohci, 0);
                                               
                ed->hwTailP = cpu_to_le32 ((uint32_t)td->td_dma);
                ed->hwHeadP = ed->hwTailP;
                ed->state = ED_UNLINK;
                ed->type = usb_pipetype (pipe);
                ohci->ed_cnt++;  // we will be used to calcaulate next pipe

//	        debug("ep_add_ed 1 td=%x dma=%x ed->dma=%x ed->hwHeadP=%x ed->hwTailP=%x\n", td, td->td_dma, ed->dma, ed->hwHeadP, ed->hwTailP); 
		
        }

        ohci->dev[usb_pipedevice (pipe)] = usb_dev;  // marked the ed to this dev

        ed->hwINFO = cpu_to_le32 (usb_pipedevice (pipe)
                        | usb_pipeendpoint (pipe) << 7
                        | (usb_pipeisoc (pipe)? 0x8000: 0)
                        | (usb_pipecontrol (pipe)? 0: (usb_pipeout (pipe)? 0x800: 0x1000))
                        | usb_pipeslow (pipe) << 13
                        | usb_maxpacket (usb_dev, pipe, usb_pipeout (pipe)) << 16);

//	debug("ep_add_ed:  pipe=%x ed_num=%d ed->dma=%x ed->hwInfo=%x ed->hwHeadP=%x ed->hwTailP=%x\n", pipe, i, ed->dma, ed->hwINFO, ed->hwHeadP, ed->hwTailP);

        return ed;
}

/* enqueue next TD for this URB (OHCI spec 5.2.8.2) */

void
td_fill (ohci_t * ohci, unsigned int info,
        void *data, int len,
        struct urb * urb, int index) // *data should dma address of buffer
{       
        ohci_td_t  * td, * td_pt;
        urb_priv_t * urb_priv = urb->hcpriv;
        
        if (index >= urb_priv->length) {
                printf("internal OHCI error: TD index > length");
                return;
        }       

        /* use this td as the next dummy */
        td_pt = urb_priv->td [index];
        td_pt->hwNextTD = 0;

        /* fill the old dummy TD */
        td = urb_priv->td [index] = dma_to_td (ohci,
                        (void *)(le32_to_cpup (&urb_priv->ed->hwTailP) & ~0xf));
	
//	debug("td_fill 2 td = %x, dma=%x , ed->hwHeadP=%x, ed->hwTailP=%x \n", td, td->td_dma, urb_priv->ed->hwHeadP, urb_priv->ed->hwTailP ); 

        td->ed = urb_priv->ed;
        td->next_dl_td = NULL;
        td->index = index;
        td->urb = urb;
        td->data_dma = data;
        if (!len)
                data = 0;


        td->hwINFO = cpu_to_le32 (info);
        td->hwCBP = cpu_to_le32 ((uint32_t)data);
        if (data)
                td->hwBE = cpu_to_le32 ((uint32_t)data + len - 1);
        else
                td->hwBE = 0;
        td->hwNextTD = cpu_to_le32 ((uint32_t)td_pt->td_dma);

        /* append to queue */
        td->ed->hwTailP = td->hwNextTD;
 //	debug("td_fill 4 td->td_dma=%x, td->hwINFO=%x\n", td->td_dma, td->hwINFO );
 //       debug("td_fill 5 ed->dma=%x, ed->hwHeadP=%x, ed->hwTailP=%x \n", urb_priv->ed->dma, urb_priv->ed->hwHeadP, urb_priv->ed->hwTailP );
}

/* prepare all TDs of a transfer */
        
void td_submit_urb (struct urb * urb)
{
        urb_priv_t * urb_priv = urb->hcpriv;
        ohci_t * ohci = (ohci_t *) &_ohci_x[urb->dev->controller];
        void * data;
        int data_len = urb->transfer_buffer_length;
        int cnt = 0; 
        u32 info = 0;
        unsigned int toggle = 0;
	void *setup_buffer;

        /* OHCI handles the DATA-toggles itself, we just use the USB-toggle bits for reseting */
        if(usb_gettoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe))) {
                toggle = TD_T_TOGGLE;
        } else {
                toggle = TD_T_DATA0;
                usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe), 1);
        }
        
        urb_priv->td_cnt = 0;
        
        if (data_len) {
                data = (void *)virt_to_phys(urb->transfer_buffer);
        } else  
                data = 0;
        switch (usb_pipetype (urb->pipe)) {
                case PIPE_BULK:
                        info = usb_pipeout (urb->pipe)?
                                TD_CC | TD_DP_OUT : TD_CC | TD_DP_IN ;
                        while(data_len > 4096) {
                                td_fill (ohci, info | (cnt? TD_T_TOGGLE:toggle), data, 4096, urb, cnt);
                                data += 4096; data_len -= 4096; cnt++;
                        }
                        info = usb_pipeout (urb->pipe)?
                                TD_CC | TD_DP_OUT : TD_CC | TD_R | TD_DP_IN ;
                        td_fill (ohci, info | (cnt? TD_T_TOGGLE:toggle), data, data_len, urb, cnt);
                        cnt++;
#if 0
                        /* If the transfer size is multiple of the pipe mtu,
                         * we may need an extra TD to create a empty frame
                         * Note : another way to check this condition is
                         * to test if(urb_priv->length > cnt) - Jean II */
                        if ((urb->transfer_flags & USB_ZERO_PACKET) &&
                            usb_pipeout (urb->pipe) &&
                            (urb->transfer_buffer_length != 0) &&
                            ((urb->transfer_buffer_length % maxps) == 0)) {
                                td_fill (ohci, info | (cnt? TD_T_TOGGLE:toggle), 0, 0, urb, cnt);
                                cnt++;
                        }
#endif

//			debug("td_submit_urb 2 -- set OHCI_BLF\n");
                        writel (OHCI_BLF, &ohci->regs->cmdstatus); /* start bulk list */
                        (void)readl (&ohci->regs->intrdisable); /* PCI posting flush */
                        break;


                case PIPE_CONTROL:
                        info = TD_CC | TD_DP_SETUP | TD_T_DATA0;
			setup_buffer = (void *)virt_to_phys(urb->setup_packet);	
//			debug("td_sumbit_urb 11 setup_buffer = %x\n", setup_buffer);
        	        td_fill (ohci, info, setup_buffer , 8, urb, cnt++);
                        if (data_len > 0) {
                                info = usb_pipeout (urb->pipe)?
                                        TD_CC | TD_R | TD_DP_OUT | TD_T_DATA1 : TD_CC | TD_R | TD_DP_IN | TD_T_DATA1;
                                /* NOTE:  mishandles transfers >8K, some >4K */
                                td_fill (ohci, info, data, data_len, urb, cnt++);
                        }
                        info = usb_pipeout (urb->pipe)?
                                TD_CC | TD_DP_IN | TD_T_DATA1: TD_CC | TD_DP_OUT | TD_T_DATA1;
                        td_fill (ohci, info, data, 0, urb, cnt++);
//                        debug("td_sumbit_urb 11 data = %x\n", data);

//			debug("td_submit_urb 2 -- set OHCI_CLF\n");
                        writel (OHCI_CLF, &ohci->regs->cmdstatus); /* start Control list */
                        (void)readl (&ohci->regs->intrdisable); /* PCI posting flush */
                        break;

        }
        if (urb_priv->length != cnt) {
                debug("TD LENGTH %d != CNT %d", urb_priv->length, cnt);
	}
}

/* free HCD-private data associated with this URB */
        
void urb_free_priv (struct ohci *hc, urb_priv_t * urb_priv)
{       
        int             i;
        int             last = urb_priv->length - 1;
        int             len;
        struct ohci_td       *td;
                        
        if (last >= 0) {
#if 0
                /* ISOC, BULK, INTR data buffer starts at td 0 
                 * CTRL setup starts at td 0 */
                td = urb_priv->td [0];
                        
                len = td->urb->transfer_buffer_length;
                                
                /* unmap CTRL URB setup */
                if (usb_pipecontrol (td->urb->pipe)) {
// it should be freed in usb_control_msg_x
//			forget2((void *)phys_to_virt((uint32_t)td->data_dma));  // 8 bytes
                 
                        /* CTRL data buffer starts at td 1 if len > 0 */
                        if (len && last > 0)
                                td = urb_priv->td [1];
                } 
        
                /* unmap data buffer */
                if (len && td->data_dma) {
// Don't need
//				forget2((void *)phys_to_virt((uint32_t)td->data_dma)); 
			}
#endif
 
                for (i = 0; i <= last; i++) {
                        td = urb_priv->td [i];
                        if (td)
                                td_free (hc, td);
                }
        }
#if URB_PRE_ALLOCATE!=1
        forget2((void *)urb_priv);
#endif
}

/* get a transfer request */    
                                
int ohci_submit_urb (struct urb * urb)
{                       
        ohci_t * ohci;
        ed_t * ed;
        urb_priv_t * urb_priv;
        unsigned int pipe = urb->pipe;
        int i, size = 0;        
        int mem_flags = 0;

        if (!urb->dev)
                return -ENODEV;

        if (urb->hcpriv)                        /* urb already in use */
                return -EINVAL;


        ohci = (ohci_t *) &_ohci_x[urb->dev->controller];
//	printf("ohci_submit_urb: urb->dev port=%x, controller = %d ohci=%x ohci->ed=%x ohci->hcca=%x\n", urb->dev->port,urb->dev->controller, ohci, ohci->ed, ohci->hcca);

#if DEBUG_USB==1
//        urb_print (urb, "SUB", usb_pipein (pipe));
#endif


#if 0
        /* handle a request to the virtual root hub */
        if (usb_pipedevice (pipe) == ohci->rh.devnum)
                return rh_submit_urb (urb);

        /* when controller's hung, permit only roothub cleanup attempts
         * such as powering down ports */
        if (ohci->disabled) {
                usb_dec_dev_use (urb->dev);
                return -ESHUTDOWN;
        }
#endif

        /* every endpoint has a ed, locate and fill it */
        if (!(ed = ep_add_ed (urb->dev, pipe, urb->interval, 1, mem_flags))) {
                return -ENOMEM;
        }
//	debug("ohci_submit_usb: ed->dma=%x\n", ed->dma);

                                           /* for the private part of the URB we need the number of TDs (size) */
        switch (usb_pipetype (pipe)) {
                case PIPE_BULK: /* one TD for every 4096 Byte */
                        size = (urb->transfer_buffer_length - 1) / 4096 + 1;
#if 0
                        /* If the transfer size is multiple of the pipe mtu,
                         * we may need an extra TD to create a empty frame
                         * Jean II */
                        if ((urb->transfer_flags & USB_ZERO_PACKET) &&
                            usb_pipeout (pipe) &&
                            (urb->transfer_buffer_length != 0) &&
                            ((urb->transfer_buffer_length % maxps) == 0))
                                size++;
#endif
                        break;
                case PIPE_CONTROL: /* 1 TD for setup, 1 for ACK and 1 for every 4096 B */
                        size = (urb->transfer_buffer_length == 0)? 2:
                                                (urb->transfer_buffer_length - 1) / 4096 + 3;
                        break;
        }

        /* allocate the private part of the URB */
#if URB_PRE_ALLOCATE!=1
        urb_priv = allot2 (sizeof (urb_priv_t) + size * sizeof (ohci_td_t *), 0xff);
        if (urb_priv == 0) {
		printf("ohci_submit_usb: urb_priv allocated no mem\n");
                return -ENOMEM;
        }
#else 
	urb_priv = ohci->urb_priv;
#endif
        memset (urb_priv, 0, sizeof (urb_priv_t) + size * sizeof (ohci_td_t *));

        /* fill the private part of the URB */
        urb_priv->length = size;
        urb_priv->ed = ed;

        /* allocate the TDs (updating hash chains) */
        for (i = 0; i < size; i++) {
                urb_priv->td[i] = td_alloc (ohci, 0);
                if (!urb_priv->td[i]) {
                        urb_priv->length = i;
                        urb_free_priv (ohci, urb_priv);
                        return -ENOMEM;
                }
        }

        if (ed->state == ED_NEW || (ed->state & ED_DEL)) {
                urb_free_priv (ohci, urb_priv);
                return -EINVAL;
        }

        urb->actual_length = 0;
        urb->hcpriv = urb_priv;
        urb->status = USB_ST_URB_PENDING;
        /* link the ed into a chain if is not already */
        if (ed->state != ED_OPER) {
                ep_link (ohci, ed);
	}

        /* fill the TDs and link it to the ed */
        td_submit_urb (urb);

#if 0
        /* drive timeouts by SF (messy, but works) */
        writel (OHCI_INTR_SF, &ohci->regs->intrenable);
        (void)readl (&ohci->regs->intrdisable); /* PCI posting flush */
#endif

        return 0;
}
/* calculate the transfer length and update the urb */

void dl_transfer_length(ohci_td_t * td)
{
        u32 tdINFO, tdBE, tdCBP;
        struct urb * urb = td->urb;
        urb_priv_t * urb_priv = urb->hcpriv;

        tdINFO = le32_to_cpup (&td->hwINFO);
        tdBE   = le32_to_cpup (&td->hwBE);
        tdCBP  = le32_to_cpup (&td->hwCBP);


                if (!(usb_pipetype (urb->pipe) == PIPE_CONTROL &&
                                ((td->index == 0) || (td->index == urb_priv->length - 1)))) {
                        if (tdBE != 0) {
                                if (td->hwCBP == 0)
                                        urb->actual_length += tdBE - (uint32_t)td->data_dma + 1;
                                else
                                        urb->actual_length += tdCBP - (uint32_t)td->data_dma;
                        }
			
                }

//	debug("td->td_dma=%x, urb->actual_length=%d\n", td->td_dma, urb->actual_length);	
}

/*-------------------------------------------------------------------------*/

/* replies to the request have to be on a FIFO basis so
 * we reverse the reversed done-list */

ohci_td_t * dl_reverse_done_list (ohci_t * ohci)
{
        u32 td_list_hc;
        ohci_td_t * td_rev = NULL;
        ohci_td_t * td_list = NULL;
        urb_priv_t * urb_priv = NULL;
	uint32_t value;
	u32 td_list_hc2;
	int timeout = 1000000; //1 second
//        unsigned long flags;
// Here need to process across the frame tds
        td_list_hc = le32_to_cpup (&ohci->hcca->done_head) & 0xfffffff0;
	td_list_hc2 = readl(&ohci->regs->donehead);
//	debug("ohci->hcca->done_head = %x ohci->hcca=%x ohci=%x ohci->regs->donehead=%x\n", td_list_hc, ohci->hcca, ohci, td_list_hc2);

	td_list = dma_to_td (ohci, (void *)td_list_hc);
	urb_priv = (urb_priv_t *) td_list->urb->hcpriv;
	
	while(/*(td_list_hc2!=0) || */(td_list->index < urb_priv->length-1) && (timeout>0)) { // wait another update for donehead
		// To handle 1. ohci->hcca->donehead !=0 and regs->donehead!=0
		//	     2. ohci->hcca->donehead !=0 and regs->donehead ==0 but regs-->donehead will be filled 
	
		ohci->hcca->done_head = 0;

        	value = readl(&ohci->regs->intrstatus);
        	value &= readl(&ohci->regs->intrenable);

	//      We need to clear that the bit, otherwise We will not get next return.
        	if(value & OHCI_INTR_WDH) {
              		writel(value, &ohci->regs->intrstatus);
	      		(void)readl (&ohci->regs->intrdisable); /* PCI posting flush */
//		      debug("OHCI_INTR_WDH cleared intrstatus=%x value=%x \n", readl(&ohci->regs->intrstatus), value);
        	}
		while(timeout>0) {  // wait for next DONEHEAD_WRITEBACK
                	value = readl(&ohci->regs->intrstatus);
                	if(!(value & OHCI_INTR_WDH)) {
                        	udelay(1);
                        	timeout--;
                        	continue;
                	} else {
                        	break;
                	}
		}

		td_list_hc2 = le32_to_cpup (&ohci->hcca->done_head) & 0xfffffff0;
		// merge td_list_hc the tail of td_list_hc2 

		if(td_list_hc2!=0) {

			while (td_list_hc2) {
                		td_list = dma_to_td (ohci, (void *)td_list_hc2);
                		td_list_hc2 = le32_to_cpup (&td_list->hwNextTD) & 0xfffffff0;
			}

			td_list->hwNextTD = td_list_hc;

			td_list_hc = le32_to_cpup (&ohci->hcca->done_head) & 0xfffffff0;

			td_list = dma_to_td (ohci, (void *)td_list_hc);
		} else {
			printf(".");
		}
		
	}

	ohci->hcca->done_head = 0;
	
	value = readl(&ohci->regs->intrstatus);
//        debug("OHCI_INTR_WDH value=%x \n", value);
        value &= readl(&ohci->regs->intrenable);
        
//      We need to clear that the bit, otherwise We will not get next return.
//        if(value & OHCI_INTR_WDH) {
              writel(value, &ohci->regs->intrstatus);
              (void)readl (&ohci->regs->intrdisable); /* PCI posting flush */
//            debug("OHCI_INTR_WDH cleared intrstatus=%x value=%x \n", readl(&ohci->regs->intrstatus), value);
//        }	

#if 0	
        if (value & OHCI_INTR_SO) {
                debug("USB Schedule overrun");
                writel (OHCI_INTR_SO, &ohci->regs->intrenable);
                (void)readl (&ohci->regs->intrdisable); /* PCI posting flush */
        }
#endif
           

	while (td_list_hc) {
//		debug("td_list_hc = %x\n", td_list_hc);
                td_list = dma_to_td (ohci, (void *)td_list_hc);

                if (TD_CC_GET (le32_to_cpup (&td_list->hwINFO))) {
                        urb_priv = (urb_priv_t *) td_list->urb->hcpriv;
                        debug(" USB-error/status: %x : %x\n",
                                        TD_CC_GET (le32_to_cpup (&td_list->hwINFO)), td_list);
                        if (td_list->ed->hwHeadP & cpu_to_le32 (0x1)) {
                                if (urb_priv && ((td_list->index + 1) < urb_priv->length)) {
                                        td_list->ed->hwHeadP =
                                                (urb_priv->td[urb_priv->length - 1]->hwNextTD & cpu_to_le32 (0xfffffff0)) |
                                                                        (td_list->ed->hwHeadP & cpu_to_le32 (0x2));
                                        urb_priv->td_cnt += urb_priv->length - td_list->index - 1;
                                } else
                                        td_list->ed->hwHeadP &= cpu_to_le32 (0xfffffff2);
                        }
                }

                td_list->next_dl_td = td_rev;
                td_rev = td_list;
                td_list_hc = le32_to_cpup (&td_list->hwNextTD) & 0xfffffff0;
        }
        return td_list;
}
/*-------------------------------------------------------------------------*/
/* td done list */

void dl_done_list (ohci_t * ohci, ohci_td_t * td_list)
{
        ohci_td_t * td_list_next = NULL;
        ed_t * ed;
 //       int cc = 0;
        struct urb * urb;
        urb_priv_t * urb_priv;
        u32 tdINFO; //, edHeadP, edTailP;

//        unsigned long flags;

        while (td_list) {
                td_list_next = td_list->next_dl_td;

                urb = td_list->urb;
                urb_priv = urb->hcpriv;
                tdINFO = le32_to_cpup (&td_list->hwINFO);

                ed = td_list->ed;

                dl_transfer_length(td_list);
#if 0
                /* error code of transfer */
                cc = TD_CC_GET (tdINFO);
                if (cc == TD_CC_STALL)
                        usb_endpoint_halt(urb->dev,
                                usb_pipeendpoint(urb->pipe),
                                usb_pipeout(urb->pipe));

                if (!(urb->transfer_flags & USB_DISABLE_SPD)
                                && (cc == TD_DATAUNDERRUN))
                        cc = TD_CC_NOERROR;

                if (++(urb_priv->td_cnt) == urb_priv->length) {
                        if ((ed->state & (ED_OPER | ED_UNLINK))
                                        && (urb_priv->state != URB_DEL)) {
                                urb->status = cc_to_error[cc];
                                ohci_return_urb (ohci, urb);
                        } 
			else {
                                dl_del_urb (urb);
                        }
                }

                if (ed->state != ED_NEW) {
                        edHeadP = le32_to_cpup (&ed->hwHeadP) & 0xfffffff0;
                        edTailP = le32_to_cpup (&ed->hwTailP);

                        /* unlink eds if they are not busy */
                        if ((edHeadP == edTailP) && (ed->state == ED_OPER))
                                ep_unlink (ohci, ed);
                }
#endif

                td_list = td_list_next;
        }
}

void ohci_wait_urb_done(struct urb *urb, int timeout) { // timeout usually ==10000 --> 10milisecond
        //here need to according the urb or ed type judge the BLF and CLF, We may need one time out in it
	// Or need to check intrstatus and see if the hcca->done_head has been filled.
	// We need to clear that the bit, otherwise We will get next return.
	uint32_t pipe = urb->pipe;
	uint32_t value;
	usbdev_t *usb_dev = urb->dev;
	ohci_t *ohci = &_ohci_x[usb_dev->controller];
	uint32_t type;
	while(timeout>0) {
#if 1
		value = readl(&ohci->regs->intrstatus);
		if(!(value & OHCI_INTR_WDH)) {
			udelay(1);
			timeout--;
			continue;
		} else {
			break;
		}
#endif
	}
#if 1
	while (timeout>0) {
                type = usb_pipetype (pipe);
                if(type ==PIPE_BULK) {
                                if( (readl(&ohci->regs->cmdstatus) & OHCI_BLF) == 0) break;
                } else if(type == PIPE_CONTROL) {
                                if( (readl(&ohci->regs->cmdstatus) & OHCI_CLF) == 0) break;
                }
                udelay(1); // 
                timeout--;
	}
#endif

	
}
void ohci_urb_complete(struct urb *urb) {

        ohci_t *ohci = &_ohci_x[urb->dev->controller];
        // it will clear the done list. and urb's actual_length is updated
        dl_done_list (ohci, dl_reverse_done_list (ohci));

#if DEBUG_USB==1
        urb_print (urb, "RET", usb_pipein (urb->pipe));
#endif          
        	
        urb_free_priv(ohci, urb->hcpriv); // free the priv and td list

}
/*-------------------------------------------------------------------*/
// it will 1. call usb_bulk_msg_x 
// 	   2. call dl_list and find the data return
int ohci_bulk_transfer( uchar devnum, uchar ep, unsigned int data_len, uchar *data) {
	int actual_length;
        uint32_t t = devnum;
        uint32_t pipe = ((ep&0x80)? 0x80:0)|(t<<8)|(3<<30);
        t = ep;
        pipe |=(t&0xf)<<15;
	
	usb_bulk_msg_x(&usb_device[devnum], pipe, data, data_len, &actual_length, 10000, ohci_urb_complete);
	
	return actual_length;


}
// it will 1. Call usb_control_msg_x
//	   2. call dl_done_list to get the data returned ----> should be packed in one usb_complete_t function 
//		and assigned that to urb

int ohci_control_msg( uchar devnum, uchar request_type, uchar request, unsigned short wValue, unsigned short wIndex, unsigned short
wLength, void *data){

	uint32_t t = devnum;
        uint32_t pipe = ((request_type&0x80)? 0x80:0)|(t<<8)|(2<<30);
	return usb_control_msg_x(&usb_device[devnum], pipe, request, request_type, wValue, wIndex, data, wLength, 10000, ohci_urb_complete);
	
}               

int ohc_reset(uchar controller)
{

        int timeout = 30;
        int smm_timeout = 50; /* 0,5 sec */
        
        debug("Resetting OHCI\n");
        ohci_regs = (ohci_regs_t *)hc_base[controller];
        ohci_t *ohci = &_ohci_x[controller];

#ifndef __hppa__
        /* PA-RISC doesn't have SMM, but PDC might leave IR set */
        if (readl (&ohci_regs->control) & OHCI_CTRL_IR) { /* SMM owns the HC */
                writel (OHCI_OCR, &ohci_regs->cmdstatus); /* request ownership */
                debug("USB HC TakeOver from SMM");
                while (readl (&ohci_regs->control) & OHCI_CTRL_IR) {
                        mdelay (10);
                        if (--smm_timeout == 0) {
                                printf("USB HC TakeOver failed!");
                                return -1;
                        }
                }
        }
#endif

        debug("USB HC reset_hc usb-%08x: ctrl = 0x%x ;",
                hc_base[controller],
                readl (&ohci_regs->control));

        /* Reset USB (needed by some controllers) */
        writel (0, &ohci_regs->control);

        /* Force a state change from USBRESET to USBOPERATIONAL for ALi */
        (void) readl (&ohci_regs->control);    /* PCI posting */
        writel (ohci->hc_control = OHCI_USB_OPER, &ohci_regs->control);

        /* HC Reset requires max 10 ms delay */
        writel (OHCI_HCR,  &ohci_regs->cmdstatus);
        while ((readl (&ohci_regs->cmdstatus) & OHCI_HCR) != 0) {
                if (--timeout == 0) {
                        printf("USB HC reset timed out!");
                        return -1;
                }
                udelay (1);
        }
        return 0;
}
         

int ohc_start(uchar controller) {
 //       unsigned short tmp;
	u32 mask;
	unsigned int fminterval;
	int delaytime;
	ohci_regs = (ohci_regs_t *)hc_base[controller];
	ohci_t *ohci = &_ohci_x[controller];
	
        debug("Starting OHCI\n");
	
        writel (0, &ohci_regs->ed_controlhead);
        writel (0, &ohci_regs->ed_bulkhead);

        writel ((uint32_t)ohci->hcca_dma, &ohci_regs->hcca); /* a reset clears this */ //3

        fminterval = 0x2edf;	//6
        writel ((fminterval * 9) / 10, &ohci_regs->periodicstart); // Don't worry, we can disable periodic in contol or let the ED list null
        fminterval |= ((((fminterval - 210) * 6) / 7) << 16);
        writel (fminterval, &ohci_regs->fminterval);
        writel (0x628, &ohci_regs->lsthresh);

        /* start controller operations */
	
	ohci->hc_control = OHCI_CONTROL_INIT | OHCI_USB_OPER;
        writel (ohci->hc_control, &ohci_regs->control); // PIE and IE is disabled
							// DO we need to enable that but leave all ISO ED and INT ED list null??? 

        mask = OHCI_INTR_MIE | OHCI_INTR_UE | OHCI_INTR_WDH | OHCI_INTR_SO;
        writel (mask, &ohci->regs->intrenable);
        writel (mask, &ohci->regs->intrstatus);

        /* required for AMD-756 and some Mac platforms */
        writel ((roothub_a (ohci) | RH_A_NPS) & ~RH_A_PSM, &ohci->regs->roothub.a);
        writel (RH_HS_LPSC, &ohci->regs->roothub.status);

	(void)readl (&ohci->regs->intrdisable); /* PCI posting flush */
	
        // POTPGT delay is bits 24-31, in 2 ms units.
	delaytime = ((roothub_a (ohci) >> 23) & 0x1fe)*5/2; // for apacer 256 usb 2.0 + NEC 2.0 chip
//	delaytime = ((roothub_a (ohci) >> 23) & 0x1fe);
	
        mdelay (delaytime);
	
//	printf("delaytime: %d\n", delaytime);
	
        return(0);
}
	

int ohc_init(struct pci_device *dev)
{       
        uint16_t word;
	uint32_t dword;
	ohci_t *ohci;
	ed_t * ed;
	int i,j, NDP;
	int size;
                
        pci_read_config_dword(dev, 0x10, &dword);  // it will be 4k range
        hc_base[num_controllers] =  (uint32_t)phys_to_virt(dword);
	ohci = &_ohci_x[num_controllers];
        debug("ohc_init num_controllers=%d ohci=%x\n", num_controllers, (uint32_t)ohci);	
	memset(ohci, 0, sizeof(ohci_t));
	ohci->regs = (ohci_regs_t *)hc_base[num_controllers];
	ohci_regs  = ohci->regs;

        ohci->hcca = allot2(sizeof (struct ohci_hcca), 0xff); //1
        if (!ohci->hcca) {
		printf("ohc_init: hcca allocated no MEM\n");
                return -ENOMEM;
        }
        memset (ohci->hcca, 0, sizeof (struct ohci_hcca));
	ohci->hcca_dma = (void *)virt_to_phys(ohci->hcca);

	//init ed;
	ohci->ed = allot2(sizeof(ed_t)*NUM_EDS,0xf);
	if(ohci->ed==0) {
		printf("ohci_init: ed allocate no MEM\n");
	}
//	debug("ohci->ed = %x\n", ohci->ed);
	for(i=0; i<NUM_EDS;i++) {
		ed = (ed_t *)&ohci->ed[i];
		ed->dma = (void *)virt_to_phys(ed);
//		debug("i=%d, ed dma = %x\n", i, (uint32_t)ed->dma);
		ed->state = ED_NEW;
	}

//	init urb and urb_priv
        ohci->urb = (struct urb *)allot2(sizeof(struct urb),0xff);
        if (!ohci->urb) {
                printf("ohci_init:  urb allocate failed");
        }
        memset(ohci->urb, 0, sizeof(urb_t));

	
        /* allocate the private part of the URB */
	size = 4;
        ohci->urb_priv = allot2 (sizeof (urb_priv_t) + size * sizeof (ohci_td_t *), 0xff);
        if (ohci->urb_priv == 0) {
                printf("ohci_init: urb_priv allocated no mem\n");
        }
        memset (ohci->urb_priv, 0, sizeof (urb_priv_t) + size * sizeof (ohci_td_t *));

        // set master
        pci_read_config_word(dev, 0x04, &word);
        word |= 0x04;
        pci_write_config_word(dev, 0x04, word);

        
        DPRINTF("Found OHCI at %08x\n", hc_base[num_controllers]);
        ohc_reset(num_controllers);

	/* Here should or move to ohc_start
	1. Init HCCA
	2. Init ED and TD ---> in submit_urb
	3. Assign HCCA to ohci_regs->hcca  ---> in ohc_init
	4. Set Intr to ohci_regs->intrenable  ---> disable that in ohc_init
	5. enable all queue in ohci_regs->control ---> in ep_link and it is called by submit_urb
	6. set peridicstart to 0.9 of frameinterval  ---> ohc_start
	*/
	
//	writel( 0, &ohci_regs->intrenable);   // no interrupts! //4
//	writel( 0xffffffff, &ohci_regs->intrdisable);

        NDP = readl(&ohci->regs->roothub.a) & 0xff;
        for(j=0;j<NDP;j++) {
        	writel(RH_PS_PSS, &ohci->regs->roothub.portstatus[j]);
	}

        /* FIXME this is a second HC reset; why?? */
        writel (ohci->hc_control = OHCI_USB_RESET, &ohci->regs->control);
        (void)readl (&ohci->regs->intrdisable); /* PCI posting flush */
        mdelay (10);	

        ohc_start(num_controllers);

        num_controllers++;

#if  DEBUG_USB==1
//        ohci_dump (ohci, 1);
#endif

//	debug("ohci->ed = %x\n", ohci->ed);
        return(0);
}
int poll_o_root_hub(uint32_t port, uchar controller)
{
        uint32_t value;
        int addr=0;
        int i;
        static uint32_t do_over=0;
	uint8_t what;
	ohci_t *ohci;

        value = readl(port);
	
	debug("poll_o_root_hub1 v=%08x port = %x, controller = %d\n", value, port, controller);

	if(value == 0xffffffff) return addr; // stupid port 

        if((value & RH_PS_CSC) || do_over == port) {
		debug("poll_o_root_hub2 v=%08x\t", value);
                do_over=0;
                if(value & RH_PS_CCS ) {     // if port connected
			debug("poll_o_root_hub21 v=%08x\t", value);
                        DPRINTF("Connection on port %04x\n", port);

                        writel(value, port);
                        for(i=0; i<40; i++) {
                                udelay(10000+usec_offset);
                                value = readl(port);
                                if(value & RH_PS_CSC) {
                                        writel(value, port);  //Clear Change bit
                                        i=0;
                                        DPRINTF("BOUNCE!\n");
                                }
                        }
//	 	 	debug("poll_o_root_hub211 v=%08x\t", value);

                        oport_wakeup(port);
//                      DPRINTF("Wakup %04x\n", port);
                
//   		 	debug("poll_o_root_hub212 v=%08x\t", readl(port)); 
                        oport_reset(port);
//  		     	debug("poll_o_root_hub213 v=%08x\t", readl(port));
                        mdelay(10);
                        oport_enable(port);
// 		        debug("poll_o_root_hub214 v=%08x\t", readl(port));
                
                        if(!(value & RH_PS_CCS)) {
                                DPRINTF("Device went away!\n");
                                return(-1);
                        }
			
                        addr = configure_device( port, controller, value & RH_PS_LSDA);

#if  DEBUG_USB==1     
			// some one clear enable bit??? why??? It costs me one week to find it out. 
                        ohci = &_ohci_x[controller];
                        ohci_dump (ohci, 1);
#endif

#if 1
//			usb_control_msg(addr, 0x21, 0xff, 0, 0, 0, NULL);// reset device
//			mdelay(10);
			usb_control_msg(addr, 0xa1, 0xfe, 0, 0, 1, &what); // get MAX L // get MAX LUN
#endif

//			debug("poll_o_root_hub215 v=%08x  addr = %d\n", readl(port), addr);

                        if(addr<0) {
                                oport_disable(port);
                                udelay(20000);
//                              oport_reset(port);
                                oport_reset_long(port);
                                oport_suspend(port);
                                do_over=port;
                                ohc_clear_stat(controller);

                        }
                } else {
//			debug("poll_o_root_hub22 v=%08x\t", readl(port));
                        oport_suspend(port);
                        oport_disable(port);
                        DPRINTF("Port %04x disconnected\n", port);
                        // wave hands, deconfigure devices on this port!
                }
        }

        return(addr);
}




#endif
