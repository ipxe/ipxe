#ifdef USB_DISK

#ifndef _OHCI_H
#define _OHCI_H

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

// for OHCI

/* ED States */

#define ED_NEW          0x00
#define ED_UNLINK       0x01
#define ED_OPER         0x02
#define ED_DEL          0x04
#define ED_URB_DEL      0x08

/* usb_ohci_ed */
struct ed {
        u32 hwINFO;       
        u32 hwTailP;
        u32 hwHeadP;
        u32 hwNextED;

        struct ed * ed_prev;  
        u8 int_period;  // No  use just for aligned
        u8 int_branch;  // No use just for aligned
        u8 int_load;    // No uae just for aligned
        u8 int_interval; // No use just for aligned
        u8 state;
        u8 type;
        u16 last_iso; // no use just for aligned
        struct ed * ed_rm_list;  // No use just for aligned

        void * dma;         

        u32 unused[3];        
};
// __attribute((aligned(16)));   
typedef struct ed ed_t;

/* TD info field */
#define TD_CC       0xf0000000
#define TD_CC_GET(td_p) ((td_p >>28) & 0x0f)
#define TD_CC_SET(td_p, cc) (td_p) = ((td_p) & 0x0fffffff) | (((cc) & 0x0f) << 28)
#define TD_EC       0x0C000000
#define TD_T        0x03000000
#define TD_T_DATA0  0x02000000
#define TD_T_DATA1  0x03000000
#define TD_T_TOGGLE 0x00000000
#define TD_R        0x00040000
#define TD_DI       0x00E00000
#define TD_DI_SET(X) (((X) & 0x07)<< 21)
#define TD_DP       0x00180000
#define TD_DP_SETUP 0x00000000
#define TD_DP_IN    0x00100000
#define TD_DP_OUT   0x00080000

#define TD_ISO      0x00010000
#define TD_DEL      0x00020000

/* CC Codes */
#define TD_CC_NOERROR      0x00
#define TD_CC_CRC          0x01
#define TD_CC_BITSTUFFING  0x02
#define TD_CC_DATATOGGLEM  0x03
#define TD_CC_STALL        0x04
#define TD_DEVNOTRESP      0x05
#define TD_PIDCHECKFAIL    0x06
#define TD_UNEXPECTEDPID   0x07
#define TD_DATAOVERRUN     0x08
#define TD_DATAUNDERRUN    0x09
#define TD_BUFFEROVERRUN   0x0C
#define TD_BUFFERUNDERRUN  0x0D
#define TD_NOTACCESSED     0x0F


#define MAXPSW 1

struct ohci_td {
        u32 hwINFO;
        u32 hwCBP;            /* Current Buffer Pointer */
        u32 hwNextTD;         /* Next TD Pointer */
        u32 hwBE;             /* Memory Buffer End Pointer */
        u16 hwPSW[MAXPSW];
        u8 unused;
        u8 index;
        struct ed * ed;
        struct ohci_td * next_dl_td;
        struct urb * urb;  //defined in usb.h
        void * td_dma;
        void * data_dma;
        u32 unused2[2];
}; 
//__attribute((aligned(32)));   /* normally 16, iso needs 32 */
typedef struct ohci_td ohci_td_t;

#define OHCI_ED_SKIP    (1 << 14)

/*
 * The HCCA (Host Controller Communications Area) is a 256 byte
 * structure defined in the OHCI spec. that the host controller is
 * told the base address of.  It must be 256-byte aligned.
 */

#define NUM_INTS 32     /* part of the OHCI standard */
struct ohci_hcca {
        u32   int_table[NUM_INTS];    /* Interrupt ED table */
        u16   frame_no;               /* current frame number */
        u16   pad1;                   /* set to 0 on each frame_no change */
        u32   done_head;              /* info returned for an interrupt */
        u8              reserved_for_hc[116];
} __attribute((aligned(256)));


#define MAX_ROOT_PORTS 15

struct ohci_regs {
        /* control and status registers */
        u32   revision;
        u32   control;
        u32   cmdstatus;
        u32   intrstatus;
        u32   intrenable;
        u32   intrdisable;
        /* memory pointers */
        u32   hcca;
        u32   ed_periodcurrent;
        u32   ed_controlhead;
        u32   ed_controlcurrent;
        u32   ed_bulkhead;
        u32   ed_bulkcurrent;
        u32   donehead;
        /* frame counters */
        u32   fminterval;
        u32   fmremaining;
        u32   fmnumber;
        u32   periodicstart;
        u32   lsthresh;
        /* Root hub ports */
        struct  ohci_roothub_regs {
                u32   a;
                u32   b;
                u32   status;
                u32   portstatus[MAX_ROOT_PORTS];
        } roothub;
} __attribute((aligned(32))); 
typedef struct ohci_regs ohci_regs_t;


/* OHCI CONTROL AND STATUS REGISTER MASKS */

/*
 * HcControl (control) register masks
 */
#define OHCI_CTRL_CBSR  (3 << 0)        /* control/bulk service ratio */
#define OHCI_CTRL_PLE   (1 << 2)        /* periodic list enable */
#define OHCI_CTRL_IE    (1 << 3)        /* isochronous enable */
#define OHCI_CTRL_CLE   (1 << 4)        /* control list enable */
#define OHCI_CTRL_BLE   (1 << 5)        /* bulk list enable */
#define OHCI_CTRL_HCFS  (3 << 6)        /* host controller functional state */
#define OHCI_CTRL_IR    (1 << 8)        /* interrupt routing */
#define OHCI_CTRL_RWC   (1 << 9)        /* remote wakeup connected */
#define OHCI_CTRL_RWE   (1 << 10)       /* remote wakeup enable */

/* pre-shifted values for HCFS */
#       define OHCI_USB_RESET   (0 << 6)
#       define OHCI_USB_RESUME  (1 << 6)
#       define OHCI_USB_OPER    (2 << 6)
#       define OHCI_USB_SUSPEND (3 << 6)

/*
 * HcCommandStatus (cmdstatus) register masks
 */
#define OHCI_HCR        (1 << 0)        /* host controller reset */
#define OHCI_CLF        (1 << 1)        /* control list filled */
#define OHCI_BLF        (1 << 2)        /* bulk list filled */
#define OHCI_OCR        (1 << 3)        /* ownership change request */
#define OHCI_SOC        (3 << 16)       /* scheduling overrun count */

/*      
 * masks used with interrupt registers:
 * HcInterruptStatus (intrstatus)
 * HcInterruptEnable (intrenable)
 * HcInterruptDisable (intrdisable)
 */     
#define OHCI_INTR_SO    (1 << 0)        /* scheduling overrun */
#define OHCI_INTR_WDH   (1 << 1)        /* writeback of done_head */
#define OHCI_INTR_SF    (1 << 2)        /* start frame */
#define OHCI_INTR_RD    (1 << 3)        /* resume detect */
#define OHCI_INTR_UE    (1 << 4)        /* unrecoverable error */
#define OHCI_INTR_FNO   (1 << 5)        /* frame number overflow */
#define OHCI_INTR_RHSC  (1 << 6)        /* root hub status change */
#define OHCI_INTR_OC    (1 << 30)       /* ownership change */
#define OHCI_INTR_MIE   (1 << 31)       /* master interrupt enable */


/* For initializing controller (mask in an HCFS mode too) */
#define OHCI_CONTROL_INIT \
        (OHCI_CTRL_CBSR & 0x3) 
//| OHCI_CTRL_IE | OHCI_CTRL_PLE   

/* OHCI ROOT HUB REGISTER MASKS */

/* roothub.portstatus [i] bits */
#define RH_PS_CCS            0x00000001         /* current connect status */
#define RH_PS_PES            0x00000002         /* port enable status*/
#define RH_PS_PSS            0x00000004         /* port suspend status */
#define RH_PS_POCI           0x00000008         /* port over current indicator */
#define RH_PS_PRS            0x00000010         /* port reset status */
#define RH_PS_PPS            0x00000100         /* port power status */
#define RH_PS_LSDA           0x00000200         /* low speed device attached */
#define RH_PS_CSC            0x00010000         /* connect status change */
#define RH_PS_PESC           0x00020000         /* port enable status change */
#define RH_PS_PSSC           0x00040000         /* port suspend status change */
#define RH_PS_OCIC           0x00080000         /* over current indicator change */
#define RH_PS_PRSC           0x00100000         /* port reset status change */

/* roothub.status bits */
#define RH_HS_LPS            0x00000001         /* local power status */
#define RH_HS_OCI            0x00000002         /* over current indicator */
#define RH_HS_DRWE           0x00008000         /* device remote wakeup enable */
#define RH_HS_LPSC           0x00010000         /* local power status change */
#define RH_HS_OCIC           0x00020000         /* over current indicator change */
#define RH_HS_CRWE           0x80000000         /* clear remote wakeup enable */

/* roothub.b masks */
#define RH_B_DR         0x0000ffff              /* device removable flags */
#define RH_B_PPCM       0xffff0000              /* port power control mask */

/* roothub.a masks */
#define RH_A_NDP        (0xff << 0)             /* number of downstream ports */
#define RH_A_PSM        (1 << 8)                /* power switching mode */
#define RH_A_NPS        (1 << 9)                /* no power switching */
#define RH_A_DT         (1 << 10)               /* device type (mbz) */
#define RH_A_OCPM       (1 << 11)               /* over current protection mode */
#define RH_A_NOCP       (1 << 12)               /* no over current protection */
#define RH_A_POTPGT     (0xff << 24)            /* power on to power good time */

typedef struct
{
        ed_t * ed;
        u16 length;   // number of tds associated with this request
        u16 td_cnt;   // number of tds already serviced
        int   state;
#if 0
        wait_queue_head_t * wait;
#endif
        ohci_td_t * td[0];   // list pointer to all corresponding TDs associated with this request

} urb_priv_t;

#define NUM_EDS 32              /* num of preallocated endpoint descriptors */

typedef struct ohci {
        struct ohci_hcca *hcca;         /* hcca */
        void * hcca_dma;
	
        ohci_regs_t * regs;        /* OHCI controller's memory */
        
	ed_t * ed_bulktail;       /* last endpoint of bulk list */
        ed_t * ed_controltail;    /* last endpoint of control list */

        int intrstatus;
        u32 hc_control;               /* copy of the hc control reg */

	uint32_t ed_cnt;
	ed_t   *ed;	// Allocate that from ed_buffer in ohc_init
	usbdev_t *dev[NUM_EDS];
	urb_t *urb; // one ohci one urb
	urb_priv_t *urb_priv;
	struct usb_ctrlrequest *dr; 
} ohci_t;


extern ohci_t _ohci_x[MAX_CONTROLLERS];

#define usb_to_ohci(usb_dev) (&_ohci_x[(usb_dev)->controller])

extern ohci_regs_t *ohci_regs;

void clear_oport_stat(uint32_t port);
int ohc_init(struct pci_device *dev);
int poll_o_root_hub(uint32_t port, uchar controller);

int ohci_bulk_transfer( uchar devnum, uchar ep, unsigned int data_len, uchar *data);
int ohci_control_msg( uchar devnum, uchar request_type, uchar request, unsigned short wValue, unsigned short wIndex, unsigned short
	wLength, void *data);
void ohci_wait_urb_done(struct urb *urb, int timeout);

void ohci_init(void);
int ohc_init(struct pci_device *dev);
int ohci_submit_urb (struct urb * urb);
#endif

#endif
