#include <ipxe/list.h>
#include <ipxe/usb.h>

#include "hcd.h"

/*
 * Universal Host Controller Interface data structures and defines
 */

/* Command register */
#define USBCMD		0
#define   USBCMD_RS		0x0001	/* Run/Stop */
#define   USBCMD_HCRESET	0x0002	/* Host reset */
#define   USBCMD_GRESET		0x0004	/* Global reset */
#define   USBCMD_EGSM		0x0008	/* Global Suspend Mode */
#define   USBCMD_FGR		0x0010	/* Force Global Resume */
#define   USBCMD_SWDBG		0x0020	/* SW Debug mode */
#define   USBCMD_CF		0x0040	/* Config Flag (sw only) */
#define   USBCMD_MAXP		0x0080	/* Max Packet (0 = 32, 1 = 64) */

/* Status register */
#define USBSTS		2
#define   USBSTS_USBINT		0x0001	/* Interrupt due to IOC */
#define   USBSTS_ERROR		0x0002	/* Interrupt due to error */
#define   USBSTS_RD		0x0004	/* Resume Detect */
#define   USBSTS_HSE		0x0008	/* Host System Error: PCI problems */
#define   USBSTS_HCPE		0x0010	/* Host Controller Process Error:
					 * the schedule is buggy */
#define   USBSTS_HCH		0x0020	/* HC Halted */

/* Interrupt enable register */
#define USBINTR		4
#define   USBINTR_TIMEOUT	0x0001	/* Timeout/CRC error enable */
#define   USBINTR_RESUME	0x0002	/* Resume interrupt enable */
#define   USBINTR_IOC		0x0004	/* Interrupt On Complete enable */
#define   USBINTR_SP		0x0008	/* Short packet interrupt enable */

#define USBFRNUM	6
#define USBFLBASEADD	8
#define USBSOF		12
#define USBSOF_DEFAULT	64	/* Frame length is exactly 1 ms */

/* USB port status and control registers */
#define USBPORTSC1	16
#define USBPORTSC2	18
#define   USBPORTSC_CCS		0x0001	/* Current Connect Status
					 * ("device present") */
#define   USBPORTSC_CSC		0x0002	/* Connect Status Change */
#define   USBPORTSC_PE		0x0004	/* Port Enable */
#define   USBPORTSC_PEC		0x0008	/* Port Enable Change */
#define   USBPORTSC_DPLUS	0x0010	/* D+ high (line status) */
#define   USBPORTSC_DMINUS	0x0020	/* D- high (line status) */
#define   USBPORTSC_RD		0x0040	/* Resume Detect */
#define   USBPORTSC_RES1	0x0080	/* reserved, always 1 */
#define   USBPORTSC_LSDA	0x0100	/* Low Speed Device Attached */
#define   USBPORTSC_PR		0x0200	/* Port Reset */
/* OC and OCC from Intel 430TX and later (not UHCI 1.1d spec) */
#define   USBPORTSC_OC		0x0400	/* Over Current condition */
#define   USBPORTSC_OCC		0x0800	/* Over Current Change R/WC */
#define   USBPORTSC_SUSP	0x1000	/* Suspend */
#define   USBPORTSC_RES2	0x2000	/* reserved, write zeroes */
#define   USBPORTSC_RES3	0x4000	/* reserved, write zeroes */
#define   USBPORTSC_RES4	0x8000	/* reserved, write zeroes */

#define UHCI_NUMFRAMES		1024	/* in the frame list [array] */
#define UHCI_MAX_SOF_NUMBER	2047	/* in an SOF packet */

#define LINK_TO_QH(qh)		(UHCI_PTR_QH | cpu_to_le32((qh)->dma_handle))
#define LINK_TO_TD(td)		(UHCI_PTR_DEPTH | cpu_to_le32((td)->dma_handle))

static inline struct uhci_hcd *hcd_to_uhci(struct usb_hcd *hcd)
{
	return (struct uhci_hcd *) hcd->hcpriv;
}

struct uhci_td *uhci_alloc_td(void);
void uhci_free_td(struct uhci_td *td);
void uhci_fill_td(struct uhci_td *td, u32 status,
		u32 token, u32 buffer);

struct uhci_qh *uhci_alloc_qh(void);
void uhci_free_qh(struct uhci_qh *qh);

/*
 *	Transfer Descriptors
 */

/*
 * for TD <status>:
 */
#define TD_CTRL_SPD		(1 << 29)	/* Short Packet Detect */
#define TD_CTRL_C_ERR_MASK	(3 << 27)	/* Error Counter bits */
#define TD_CTRL_C_ERR_SHIFT	27
#define TD_CTRL_LS		(1 << 26)	/* Low Speed Device */
#define TD_CTRL_IOS		(1 << 25)	/* Isochronous Select */
#define TD_CTRL_IOC		(1 << 24)	/* Interrupt on Complete */
#define TD_CTRL_ACTIVE		(1 << 23)	/* TD Active */
#define TD_CTRL_STALLED		(1 << 22)	/* TD Stalled */
#define TD_CTRL_DBUFERR		(1 << 21)	/* Data Buffer Error */
#define TD_CTRL_BABBLE		(1 << 20)	/* Babble Detected */
#define TD_CTRL_NAK		(1 << 19)	/* NAK Received */
#define TD_CTRL_CRCTIMEO	(1 << 18)	/* CRC/Time Out Error */
#define TD_CTRL_BITSTUFF	(1 << 17)	/* Bit Stuff Error */
#define TD_CTRL_ACTLEN_MASK	0x7FF	/* actual length, encoded as n - 1 */

#define TD_CTRL_ANY_ERROR	(TD_CTRL_STALLED | TD_CTRL_DBUFERR | \
				 TD_CTRL_BABBLE | TD_CTRL_CRCTIME | \
				 TD_CTRL_BITSTUFF)

#define uhci_maxerr(err)		((err) << TD_CTRL_C_ERR_SHIFT)
#define uhci_status_bits(ctrl_sts)	((ctrl_sts) & 0xF60000)
#define uhci_actual_length(ctrl_sts)	(((ctrl_sts) + 1) & \
			TD_CTRL_ACTLEN_MASK)	/* 1-based */

/*
 * for TD <info>: (a.k.a. Token)
 */
#define td_token(td)		le32_to_cpu((td)->token)
#define TD_TOKEN_DEVADDR_SHIFT	8
#define TD_TOKEN_TOGGLE_SHIFT	19
#define TD_TOKEN_TOGGLE		(1 << 19)
#define TD_TOKEN_EXPLEN_SHIFT	21
#define TD_TOKEN_EXPLEN_MASK	0x7FF	/* expected length, encoded as n-1 */
#define TD_TOKEN_PID_MASK	0xFF

#define uhci_explen(len)	((((len) - 1) & TD_TOKEN_EXPLEN_MASK) << \
					TD_TOKEN_EXPLEN_SHIFT)

#define uhci_expected_length(token) ((((token) >> TD_TOKEN_EXPLEN_SHIFT) + \
					1) & TD_TOKEN_EXPLEN_MASK)
#define uhci_toggle(token)	(((token) >> TD_TOKEN_TOGGLE_SHIFT) & 1)
#define uhci_endpoint(token)	(((token) >> 15) & 0xf)
#define uhci_devaddr(token)	(((token) >> TD_TOKEN_DEVADDR_SHIFT) & 0x7f)
#define uhci_devep(token)	(((token) >> TD_TOKEN_DEVADDR_SHIFT) & 0x7ff)
#define uhci_packetid(token)	((token) & TD_TOKEN_PID_MASK)
#define uhci_packetout(token)	(uhci_packetid(token) != USB_PID_IN)
#define uhci_packetin(token)	(uhci_packetid(token) == USB_PID_IN)

#define UHCI_PTR_BITS		cpu_to_le32(0x000F)
#define UHCI_PTR_TERM		cpu_to_le32(0x0001)
#define UHCI_PTR_QH		cpu_to_le32(0x0002)
#define UHCI_PTR_DEPTH		cpu_to_le32(0x0004)
#define UHCI_PTR_BREADTH	cpu_to_le32(0x0000)

/*
 * The documentation says "4 words for hardware, 4 words for software".
 *
 * That's silly, the hardware doesn't care. The hardware only cares that
 * the hardware words are 16-byte aligned, and we can have any amount of
 * sw space after the TD entry.
 *
 * td->link points to either another TD (not necessarily for the same urb or
 * even the same endpoint), or nothing (PTR_TERM), or a QH.
 */

struct uhci_td {
	/* Hardware fields */
	uint32_t link;
	uint32_t status;
	uint32_t token;
	uint32_t buffer;

	/* Software fields */
	unsigned long dma_handle;

	struct list_head list;
};

struct uhci_qh {

	/* Hardware fields */
	uint32_t link;			/* Next QH in the schedule */
	uint32_t element;		/* Queue element (TD) pointer */

	/* Software fields */
	unsigned long dma_handle;

	struct list_head urbp_list;
	struct uhci_td *dummy_td;	
};

struct uhci_hcd {
	unsigned int		rh_numports;
	unsigned long		io_addr;

	unsigned int		*frame;
	unsigned int		frame_dma_handle;

	struct uhci_qh		*fs_control_skelqh;
	struct uhci_qh		*bulk_skelqh;
	struct uhci_qh		*last_bulk_qh;

	unsigned int		next_devnum;
};

struct uhci_urb_priv {
	struct list_head td_list;
	struct uhci_td	*first_td;
	struct uhci_td	*last_td;

	/* For qh->urbp_list */
	struct list_head list;
};

/* Debug helper routines */
void uhci_print_td_info(struct uhci_td *td);
void uhci_print_qh_info(struct uhci_qh *qh);

