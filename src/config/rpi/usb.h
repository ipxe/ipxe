/*
 * Use EFI_USB_IO_PROTOCOL
 *
 * The Raspberry Pi uses an embedded DesignWare USB controller for
 * which we do not have a native driver.  Use via the
 * EFI_USB_IO_PROTOCOL driver instead.
 *
 */
#undef USB_HCD_XHCI
#undef USB_HCD_EHCI
#undef USB_HCD_UHCI
#define USB_HCD_USBIO
#undef USB_EFI
