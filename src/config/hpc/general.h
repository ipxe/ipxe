/*
 * Console configuration suitable for use in public cloud
 * environments, or any environment where direct console access is not
 * available.
 *
 */


/* Enable VLAN command: https://ipxe.org/buildcfg/vlan_cmd */
#define VLAN_CMD

/* Enable NTP command: https://ipxe.org/buildcfg/ntp_cmd */
#define NTP_CMD

/* Enable TIME command: https://ipxe.org/buildcfg/time_cmd */
#define TIME_CMD

/* Enable PCI_CMD command: https://ipxe.org/buildcfg/pci_cmd */
#define PCI_CMD

/* Enable REBOOT_CMD command: https://ipxe.org/buildcfg/REBOOT_CMD */
#define REBOOT_CMD

/* Enable NEIGHBOUR command: https://ipxe.org/buildcfg/neighbour_cmd */
#define NEIGHBOUR_CMD

/* Enable CONSOLE command: https://ipxe.org/buildcfg/console_cmd */
#define CONSOLE_CMD

/* Enable IMAGE_TRUST_CMD command: https://ipxe.org/buildcfg/image_trust_cmd
usage: Used for enabling the validation of trusted images.
*/
#define IMAGE_TRUST_CMD

/* Enable NSLOOKUP_CMD command: https://ipxe.org/buildcfg/nslookup_cmd
usage: Used for triaging DNS.
*/
#define NSLOOKUP_CMD

/* Enable PING_CMD command: https://ipxe.org/buildcfg/ping_cmd
usage: Used for triaging TCP/IP routing and general connectivity.
*/
#define PING_CMD

// Iterate through unplugged links faster
#ifdef LINK_WAIT_TIMEOUT
#undef LINK_WAIT_TIMEOUT
#define LINK_WAIT_TIMEOUT ( 5 * TICKS_PER_SEC )
#endif

/* No LACP please */
#ifdef NET_PROTO_LACP
#undef NET_PROTO_LACP
#endif

/* Work around missing EFI_PXE_BASE_CODE_PROTOCOL */
#ifndef EFI_DOWNGRADE_UX
#define EFI_DOWNGRADE_UX
#endif

/* The Tivoli VMM workaround causes a KVM emulation failure on hosts
 * without unrestricted_guest support
 */
#ifdef TIVOLI_VMM_WORKAROUND
#undef TIVOLI_VMM_WORKAROUND
#endif
