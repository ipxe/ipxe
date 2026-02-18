#ifndef CONFIG_USB_H
#define CONFIG_USB_H

/** @file
 *
 * USB configuration
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <config/defaults.h>

/* USB host controllers */
#define USB_HCD_EHCI		/* EHCI USB host controller */
#define USB_HCD_UHCI		/* UHCI USB host controller */
#define USB_HCD_XHCI		/* xHCI USB host controller */

/* USB peripherals */
#define USB_BLOCK		/* USB block devices */
#define USB_KEYBOARD		/* USB keyboards */

/* USB quirks on EFI platforms */
#if defined ( PLATFORM_efi )
  #define USB_EFI		/* Provide EFI_USB_IO_PROTOCOL interface */
  //#define USB_HCD_USBIO	/* Very slow EFI USB pseudo-host controller */
  #undef USB_KEYBOARD		/* Use built-in EFI keyboard driver */
#endif

#include <config/named.h>
#include NAMED_CONFIG(usb.h)
#include <config/local/usb.h>
#include LOCAL_NAMED_CONFIG(usb.h)

#endif /* CONFIG_USB_H */
