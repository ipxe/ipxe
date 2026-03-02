Downloads
=========

Binaries
--------

### ISO images

These ISO9660 images may be burned to a real CD-ROM, or be attached as
virtual media.

  - [`ipxe.iso`][iso] (All CPU architectures, BIOS/UEFI, no UEFI Secure Boot)
  - [`ipxe-x86_64-sb.iso`][isox64] (x86-64 UEFI Secure Boot only)
  - [`ipxe-arm64-sb.iso`][isoaa64] (AArch64 UEFI Secure Boot only)

### USB images

These FAT filesystem images may be burned to a mass storage device
such as a USB stick or SD card, or be attached as virtual media.  You
can edit the included `autoexec.ipxe` script to control the boot
process.

  - [`ipxe.usb`][usb] (All CPU architectures, BIOS/UEFI, no UEFI Secure Boot)
  - [`ipxe-x86_64-sb.usb`][usbx64] (x86-64 UEFI Secure Boot only)
  - [`ipxe-arm64-sb.usb`][usbaa64] (AArch64 UEFI Secure Boot only)

### Network boot server files

This archive image may be extracted to a TFTP or HTTP(S) server.

  - [`ipxeboot.tar.gz`][netboot] (All CPU architectures, BIOS/UEFI)

[iso]: ${BINURL}/ipxe.iso
[isoaa64]: ${BINURL}/ipxe-arm64-sb.iso
[isox64]: ${BINURL}/ipxe-x86_64-sb.iso
[usb]: ${BINURL}/ipxe.usb
[usbaa64]: ${BINURL}/ipxe-arm64-sb.usb
[usbx64]: ${BINURL}/ipxe-x86_64-sb.usb
[netboot]: ${BINURL}/ipxeboot.tar.gz

Source code
-----------

  - [`${SRCNAME}.tar.gz`][tarball]

[tarball]: ${SRCURL}.tar.gz

Changes
-------
