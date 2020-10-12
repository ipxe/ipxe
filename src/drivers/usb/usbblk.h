#ifndef _USBBLK_H
#define _USBBLK_H

/** @file
 *
 * USB mass storage driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/usb.h>
#include <ipxe/scsi.h>
#include <ipxe/interface.h>

/** Mass storage class code */
#define USB_CLASS_MSC 0x08

/** SCSI command set subclass code */
#define USB_SUBCLASS_MSC_SCSI 0x06

/** Bulk-only transport protocol */
#define USB_PROTOCOL_MSC_BULK 0x50

/** Mass storage reset command */
#define USBBLK_RESET ( USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE | \
		       USB_REQUEST_TYPE ( 255 ) )

/** Command block wrapper */
struct usbblk_command_wrapper {
	/** Signature */
	uint32_t signature;
	/** Tag */
	uint32_t tag;
	/** Data transfer length */
	uint32_t len;
	/** Flags */
	uint8_t flags;
	/** LUN */
	uint8_t lun;
	/** Command block length */
	uint8_t cblen;
	/** Command block */
	uint8_t cb[16];
} __attribute__ (( packed ));

/** Command block wrapper signature */
#define USBBLK_COMMAND_SIGNATURE 0x43425355UL

/** Command status wrapper */
struct usbblk_status_wrapper {
	/** Signature */
	uint32_t signature;
	/** Tag */
	uint32_t tag;
	/** Data residue */
	uint32_t residue;
	/** Status */
	uint8_t status;
} __attribute__ (( packed ));

/** Command status wrapper signature */
#define USBBLK_STATUS_SIGNATURE 0x53425355UL

/** A USB mass storage command */
struct usbblk_command {
	/** SCSI command */
	struct scsi_cmd scsi;
	/** Command tag (0 for no command in progress) */
	uint32_t tag;
	/** Offset within data buffer */
	size_t offset;
};

/** A USB mass storage device */
struct usbblk_device {
	/** Reference count */
	struct refcnt refcnt;
	/** List of devices */
	struct list_head list;

	/** USB function */
	struct usb_function *func;
	/** Bulk OUT endpoint */
	struct usb_endpoint out;
	/** Bulk IN endpoint */
	struct usb_endpoint in;

	/** SCSI command-issuing interface */
	struct interface scsi;
	/** SCSI data interface */
	struct interface data;
	/** Command process */
	struct process process;
	/** Device opened flag */
	int opened;

	/** Current command (if any) */
	struct usbblk_command cmd;
};

/** Command tag magic
 *
 * This is a policy decision.
 */
#define USBBLK_TAG_MAGIC 0x18ae0000

/** Maximum length of USB data block
 *
 * This is a policy decision.
 */
#define USBBLK_MAX_LEN 2048

/** Maximum endpoint fill level
 *
 * This is a policy decision.
 */
#define USBBLK_MAX_FILL 4

#endif /* _USBBLK_H */
