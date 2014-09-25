/*
 * OHCI HCD (Host Controller Driver) for USB.
 *
 * (C) Copyright 1999 Roman Weissgaerber <weissg@vienna.at>
 * (C) Copyright 2000-2002 David Brownell <dbrownell@users.sourceforge.net>
 *
 * This file is licenced under the GPL.
 */

/*
 * __hc32 and __hc16 are "Host Controller" types, they may be equivalent to
 * __leXX (normally) or __beXX (given OHCI_BIG_ENDIAN), depending on the
 * host controller implementation.
 */

#include <byteswap.h>
#include <little_bswap.h>

typedef uint32_t  __hc32;
typedef uint16_t  __hc16;

/* OHCI CONTROL AND STATUS REGISTER MASKS */

/*
 * HcControl (control) register masks
 */
#define OHCI_CTRL_CBSR	(3 << 0)	/* control/bulk service ratio */
#define OHCI_CTRL_PLE	(1 << 2)	/* periodic list enable */
#define OHCI_CTRL_IE	(1 << 3)	/* isochronous enable */
#define OHCI_CTRL_CLE	(1 << 4)	/* control list enable */
#define OHCI_CTRL_BLE	(1 << 5)	/* bulk list enable */
#define OHCI_CTRL_HCFS	(3 << 6)	/* host controller functional state */
#define OHCI_CTRL_IR	(1 << 8)	/* interrupt routing */
#define OHCI_CTRL_RWC	(1 << 9)	/* remote wakeup connected */
#define OHCI_CTRL_RWE	(1 << 10)	/* remote wakeup enable */

/* pre-shifted values for HCFS */
#	define OHCI_USB_RESET	(0 << 6)
#	define OHCI_USB_RESUME	(1 << 6)
#	define OHCI_USB_OPER	(2 << 6)
#	define OHCI_USB_SUSPEND	(3 << 6)

/*
 * HcCommandStatus (cmdstatus) register masks
 */
#define OHCI_HCR	(1 << 0)	/* host controller reset */
#define OHCI_CLF	(1 << 1)	/* control list filled */
#define OHCI_BLF	(1 << 2)	/* bulk list filled */
#define OHCI_OCR	(1 << 3)	/* ownership change request */
#define OHCI_SOC	(3 << 16)	/* scheduling overrun count */

/*
 * masks used with interrupt registers:
 * HcInterruptStatus (intrstatus)
 * HcInterruptEnable (intrenable)
 * HcInterruptDisable (intrdisable)
 */
#define OHCI_INTR_SO	(1 << 0)	/* scheduling overrun */
#define OHCI_INTR_WDH	(1 << 1)	/* writeback of done_head */
#define OHCI_INTR_SF	(1 << 2)	/* start frame */
#define OHCI_INTR_RD	(1 << 3)	/* resume detect */
#define OHCI_INTR_UE	(1 << 4)	/* unrecoverable error */
#define OHCI_INTR_FNO	(1 << 5)	/* frame number overflow */
#define OHCI_INTR_RHSC	(1 << 6)	/* root hub status change */
#define OHCI_INTR_OC	(1 << 30)	/* ownership change */
#define OHCI_INTR_MIE	(1 << 31)	/* master interrupt enable */


/* OHCI ROOT HUB REGISTER MASKS */

/* roothub.portstatus [i] bits */
#define RH_PS_CCS            0x00000001		/* current connect status */
#define RH_PS_PES            0x00000002		/* port enable status*/
#define RH_PS_PSS            0x00000004		/* port suspend status */
#define RH_PS_POCI           0x00000008		/* port over current indicator */
#define RH_PS_PRS            0x00000010		/* port reset status */
#define RH_PS_PPS            0x00000100		/* port power status */
#define RH_PS_LSDA           0x00000200		/* low speed device attached */
#define RH_PS_CSC            0x00010000		/* connect status change */
#define RH_PS_PESC           0x00020000		/* port enable status change */
#define RH_PS_PSSC           0x00040000		/* port suspend status change */
#define RH_PS_OCIC           0x00080000		/* over current indicator change */
#define RH_PS_PRSC           0x00100000		/* port reset status change */

/* roothub.status bits */
#define RH_HS_LPS	     0x00000001		/* local power status */
#define RH_HS_OCI	     0x00000002		/* over current indicator */
#define RH_HS_DRWE	     0x00008000		/* device remote wakeup enable */
#define RH_HS_LPSC	     0x00010000		/* local power status change */
#define RH_HS_OCIC	     0x00020000		/* over current indicator change */
#define RH_HS_CRWE	     0x80000000		/* clear remote wakeup enable */

/* roothub.b masks */
#define RH_B_DR		0x0000ffff		/* device removable flags */
#define RH_B_PPCM	0xffff0000		/* port power control mask */

/* roothub.a masks */
#define	RH_A_NDP	(0xff << 0)		/* number of downstream ports */
#define	RH_A_PSM	(1 << 8)		/* power switching mode */
#define	RH_A_NPS	(1 << 9)		/* no power switching */
#define	RH_A_DT		(1 << 10)		/* device type (mbz) */
#define	RH_A_OCPM	(1 << 11)		/* over current protection mode */
#define	RH_A_NOCP	(1 << 12)		/* no over current protection */
#define	RH_A_POTPGT	(0xff << 24)		/* power on to power good time */

/*
 * OHCI Endpoint Descriptor (ED) ... holds TD queue
 * See OHCI spec, section 4.2
 *
 * This is a "Queue Head" for those transfers, which is why
 * both EHCI and UHCI call similar structures a "QH".
 */
struct ohci_ed {
	/* first fields are hardware-specified */
	__hc32			hwINFO;      /* endpoint config bitmap */
	/* info bits defined by hcd */
#define ED_DEQUEUE	(1 << 27)
	/* info bits defined by the hardware */
#define ED_ISO		(1 << 15)
#define ED_SKIP		(1 << 14)
#define ED_LOWSPEED	(1 << 13)
#define ED_OUT		(0x01 << 11)
#define ED_IN		(0x02 << 11)
	__hc32			hwTailP;	/* tail of TD list */
	__hc32			hwHeadP;	/* head of TD list (hc r/w) */
#define ED_C		(0x02)			/* toggle carry */
#define ED_H		(0x01)			/* halted */
	__hc32			hwNextED;	/* next ED in list */

	/* rest are purely for the driver's use */
	unsigned long		dma;		/* addr of ED */
	struct ohci_td		*dummy_td;		/* next TD to activate */

	struct list_head	urbp_list;
	
	uint8_t			state;		/* ED_{IDLE,UNLINK,OPER} */
#define ED_IDLE		0x00		/* NOT linked to HC */
#define ED_UNLINK	0x01		/* being unlinked from hc */
#define ED_OPER		0x02		/* IS linked to hc */
} __attribute__ ((aligned(16)));

struct ohci_td {
	/* first fields are hardware-specified */
	__hc32		hwINFO;		/* transfer info bitmask */

	/* hwINFO bits for both general and iso tds: */
#define TD_CC       0xf0000000			/* condition code */
#define TD_CC_GET(td_p) ((td_p >>28) & 0x0f)
//#define TD_CC_SET(td_p, cc) (td_p) = ((td_p) & 0x0fffffff) | (((cc) & 0x0f) << 28)
#define TD_DI       0x00E00000			/* frames before interrupt */
#define TD_DI_SET(X) (((X) & 0x07)<< 21)
	/* these two bits are available for definition/use by HCDs in both
	 * general and iso tds ... others are available for only one type
	 */
#define TD_DONE     0x00020000			/* retired to donelist */
#define TD_ISO      0x00010000			/* copy of ED_ISO */

	/* hwINFO bits for general tds: */
#define TD_EC       0x0C000000			/* error count */
#define TD_T        0x03000000			/* data toggle state */
#define TD_T_DATA0  0x02000000				/* DATA0 */
#define TD_T_DATA1  0x03000000				/* DATA1 */
#define TD_T_TOGGLE 0x00000000				/* uses ED_C */
#define TD_DP       0x00180000			/* direction/pid */
#define TD_DP_SETUP 0x00000000			/* SETUP pid */
#define TD_DP_IN    0x00100000				/* IN pid */
#define TD_DP_OUT   0x00080000				/* OUT pid */
							/* 0x00180000 rsvd */
#define TD_R        0x00040000			/* round: short packets OK? */

	/* (no hwINFO #defines yet for iso tds) */

	__hc32		hwCBP;		/* Current Buffer Pointer (or 0) */
	__hc32		hwNextTD;	/* Next TD Pointer */
	__hc32		hwBE;		/* Memory Buffer End Pointer */

	/* PSW is only for ISO.  Only 1 PSW entry is used, but on
	 * big-endian PPC hardware that's the second entry.
	 */
#define MAXPSW	2
	__hc16		hwPSW [MAXPSW];

	/* For driver's use */
	struct list_head 	list;
	unsigned long		dma;
	struct ohci_ed 		*ed;
	unsigned int 		len;

} __attribute__ ((aligned(32)));	/* c/b/i need 16; only iso needs 32 */

/*
 * The HCCA (Host Controller Communications Area) is a 256 byte
 * structure defined section 4.4.1 of the OHCI spec. The HC is
 * told the base address of it.  It must be 256-byte aligned.
 */
struct ohci_hcca {
#define NUM_INTS 32
	__hc32	int_table [NUM_INTS];	/* periodic schedule */

	/*
	 * OHCI defines u16 frame_no, followed by u16 zero pad.
	 * Since some processors can't do 16 bit bus accesses,
	 * portable access must be a 32 bits wide.
	 */
	__hc32	frame_no;		/* current frame number */
	__hc32	done_head;		/* info returned for an interrupt */
	uint8_t	reserved_for_hc [116];
	uint8_t	what [4];		/* spec only identifies 252 bytes :) */
} __attribute__ ((aligned(256)));


struct ohci_regs {
	/* control and status registers (section 7.1) */
	__hc32	revision;
	__hc32	control;
	__hc32	cmdstatus;
	__hc32	intrstatus;
	__hc32	intrenable;
	__hc32	intrdisable;

	/* memory pointers (section 7.2) */
	__hc32	hcca;
	__hc32	ed_periodcurrent;
	__hc32	ed_controlhead;
	__hc32	ed_controlcurrent;
	__hc32	ed_bulkhead;
	__hc32	ed_bulkcurrent;
	__hc32	donehead;

	/* frame counters (section 7.3) */
	__hc32	fminterval;
	__hc32	fmremaining;
	__hc32	fmnumber;
	__hc32	periodicstart;
	__hc32	lsthresh;

	/* Root hub ports (section 7.4) */
	struct	ohci_roothub_regs {
		__hc32	a;
		__hc32	b;
		__hc32	status;
#define MAX_ROOT_PORTS	15	/* maximum OHCI root hub ports (RH_A_NDP) */
		__hc32	portstatus [MAX_ROOT_PORTS];
	} roothub;

	/* and optional "legacy support" registers (appendix B) at 0x0100 */

} __attribute__ ((aligned(32)));

struct ohci_urb_priv {
	struct list_head	list;
	struct list_head	td_list;

	struct ohci_td	*first_td, *last_td;
};

/*
 * This is the full ohci controller description
 *
 * Note how the "proper" USB information is just
 * a subset of what the full implementation needs. (Linus)
 */

struct ohci_hcd {
	struct ohci_regs 	*regs;

	uint32_t		hc_control;
	uint16_t		num_ports;

	struct ohci_hcca	*hcca;
	uint32_t		fminterval;

	struct ohci_ed		*last_bulk_ed;
	struct ohci_ed		*last_control_ed;
};

static inline struct ohci_hcd *hcd_to_ohci (struct usb_hcd *hcd)
{
	return (struct ohci_hcd *) (hcd->hcpriv);
}

#define	FI			0x2edf		/* 12000 bits per frame (-1) */
#define	FSMP(fi)		(0x7fff & ((6 * ((fi) - 210)) / 7))
#define	FIT			(1 << 31)
#define LSTHRESH		0x628		/* lowspeed bit threshold */

static inline void periodic_reinit (struct ohci_hcd *ohci)
{
	u32	fi = ohci->fminterval & 0x03fff;
	u32	fit = readl(&ohci->regs->fminterval) & FIT;

	writel ((fit ^ FIT) | ohci->fminterval,
						&ohci->regs->fminterval);
	writel (((1 * fi) / 10) & 0x3fff,
						&ohci->regs->periodicstart);
}

#define read_roothub(hc, register, mask) (readl (&hc->regs->roothub.register))


static inline u32 roothub_a (struct ohci_hcd *hc)
	{ return read_roothub (hc, a, 0xfc0fe000); }
static inline u32 roothub_b (struct ohci_hcd *hc)
	{ return readl (&hc->regs->roothub.b); }
static inline u32 roothub_status (struct ohci_hcd *hc)
	{ return readl (&hc->regs->roothub.status); }
static inline u32 roothub_portstatus (struct ohci_hcd *hc, int i)
	{ return read_roothub (hc, portstatus [i], 0xffe0fce0); }

/*-------------------------------------------------------------------------*/

/* We support only little endian HC descs at the moment. */
#define big_endian_desc(ohci) 0

/* cpu to ohci */
static inline __hc16 cpu_to_hc16 (const struct ohci_hcd *ohci __unused, const u16 x)
{
	return big_endian_desc(ohci) ?
		(__hc16)cpu_to_be16(x) :
		(__hc16)cpu_to_le16(x);
}

static inline __hc32 cpu_to_hc32 (const struct ohci_hcd *ohci __unused, const u32 x)
{
	return big_endian_desc(ohci) ?
		(__hc32)cpu_to_be32(x) :
		(__hc32)cpu_to_le32(x);
}

static inline u16 hc16_to_cpu (const struct ohci_hcd *ohci __unused, const __hc16 x)
{
	return big_endian_desc(ohci) ?
		be16_to_cpu(x) :
		le16_to_cpu(x);
}

static inline u32 hc32_to_cpu (const struct ohci_hcd *ohci __unused, const __hc32 x)
{
	return big_endian_desc(ohci) ?
		be32_to_cpu(x) :
		le32_to_cpu(x);
}
