#ifndef CONFIG_GENERAL_H
#define CONFIG_GENERAL_H

/** @file
 *
 * General configuration
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <config/defaults.h>

/*****************************************************************************
 *
 * Network protocols
 *
 */

/* Protocols supported on all platforms */
#define NET_PROTO_EAPOL		/* EAP over LAN protocol */
//#define NET_PROTO_FCOE	/* Fibre Channel over Ethernet protocol */
#define NET_PROTO_IPV4		/* IPv4 protocol */
#define NET_PROTO_IPV6		/* IPv6 protocol */
#define NET_PROTO_LACP		/* Link Aggregation control protocol */
#define NET_PROTO_LLDP		/* Link Layer Discovery protocol */
#define NET_PROTO_STP		/* Spanning Tree protocol */

/* Disable protocols not historically included in BIOS builds */
#if defined ( PLATFORM_pcbios )
  #undef NET_PROTO_IPV6
  #undef NET_PROTO_LLDP
#endif

/*****************************************************************************
 *
 * Download protocols
 *
 */

/* Protocols supported on all platforms */
#define DOWNLOAD_PROTO_TFTP	/* Trivial File Transfer Protocol */
#define DOWNLOAD_PROTO_HTTP	/* Hypertext Transfer Protocol */
#define DOWNLOAD_PROTO_HTTPS	/* Secure Hypertext Transfer Protocol */
//#define DOWNLOAD_PROTO_FTP	/* File Transfer Protocol */
//#define DOWNLOAD_PROTO_SLAM	/* Scalable Local Area Multicast */
//#define DOWNLOAD_PROTO_NFS	/* Network File System Protocol */

/* Protocols supported only on platforms with filesystem abstractions */
#if defined ( PLATFORM_efi )
  #define DOWNLOAD_PROTO_FILE	/* Local filesystem access */
#endif

/* HTTP(S) protocol extensions */
#define HTTP_AUTH_BASIC		/* Basic authentication */
#define HTTP_AUTH_DIGEST	/* Digest authentication */
#define HTTP_AUTH_NTLM		/* NTLM authentication */
//#define HTTP_ENC_PEERDIST	/* PeerDist content encoding */
//#define HTTP_HACK_GCE		/* Google Compute Engine hacks */

/* Disable protocols not historically included in BIOS builds */
#if defined ( PLATFORM_pcbios )
  #undef DOWNLOAD_PROTO_HTTPS
  #undef HTTP_AUTH_NTLM
#endif

/*****************************************************************************
 *
 * SAN boot protocols
 *
 */

/* Protocols supported on all platforms with SAN boot abstractions */
#if ! defined ( SANBOOT_NULL )
  #define SANBOOT_PROTO_AOE	/* AoE protocol */
  #define SANBOOT_PROTO_FCP	/* Fibre Channel protocol */
  #define SANBOOT_PROTO_HTTP	/* HTTP SAN protocol */
  #define SANBOOT_PROTO_IB_SRP	/* Infiniband SCSI RDMA protocol */
  #define SANBOOT_PROTO_ISCSI	/* iSCSI protocol */
#endif

/*****************************************************************************
 *
 * Command-line and script commands
 *
 */

/* Commands supported on all platforms */
#define AUTOBOOT_CMD		/* Automatic booting */
#define CERT_CMD		/* Certificate management commands */
#define CONFIG_CMD		/* Option configuration console */
#define CONSOLE_CMD		/* Console command */
#define DIGEST_CMD		/* Image crypto digest commands */
#define DHCP_CMD		/* DHCP management commands */
#define FCMGMT_CMD		/* Fibre Channel management commands */
#define FORM_CMD		/* Form commands */
#define IBMGMT_CMD		/* Infiniband management commands */
#define IFMGMT_CMD		/* Interface management commands */
#define IMAGE_CMD		/* Image management commands */
#define IMAGE_ARCHIVE_CMD	/* Archive image management commands */
//#define IMAGE_CRYPT_CMD	/* Image encryption management commands */
//#define IMAGE_MEM_CMD		/* Read memory command */
//#define IMAGE_TRUST_CMD	/* Image trust management commands */
//#define IPSTAT_CMD		/* IP statistics commands */
#define IWMGMT_CMD		/* Wireless interface management commands */
#define LOGIN_CMD		/* Login command */
//#define LOTEST_CMD		/* Loopback testing commands */
#define MENU_CMD		/* Menu commands */
//#define NEIGHBOUR_CMD		/* Neighbour management commands */
//#define NSLOOKUP_CMD		/* DNS resolving command */
#define NTP_CMD			/* NTP commands */
#define NVO_CMD			/* Non-volatile option storage commands */
#define PARAM_CMD		/* Request parameter commands */
#define PCI_CMD			/* PCI commands */
//#define PING_CMD		/* Ping command */
//#define PROFSTAT_CMD		/* Profiling commands */
//#define PXE_CMD		/* PXE commands */
#define ROUTE_CMD		/* Routing table management commands */
#define SANBOOT_CMD		/* SAN boot commands */
#define SHELL_CMD		/* Shell command */
#define SHIM_CMD		/* EFI shim command (or dummy command) */
#define SYNC_CMD		/* Sync command */
//#define TIME_CMD		/* Time commands */
#define USB_CMD			/* USB commands */
#define VLAN_CMD		/* VLAN commands */

/* Commands supported only on systems capable of rebooting */
#if ! defined ( REBOOT_NULL )
  #define POWEROFF_CMD		/* Power off command */
  #define REBOOT_CMD		/* Reboot command */
#endif

/* Commands supported only on systems that may use FDTs */
#if ! defined ( FDT_NULL )
  #define FDT_CMD		/* Flattened Device Tree commands */
#endif

/* Commands supported only on x86 CPUs */
#if defined ( __i386__ ) || defined ( __x86_64__ )
  #define CPUID_CMD		/* x86 CPU feature detection command */
#endif

/* Disable commands not historically included in BIOS builds */
#if defined ( PLATFORM_pcbios )
  #undef CERT_CMD
  #undef CONSOLE_CMD
  #undef DIGEST_CMD
  #undef NTP_CMD
  #undef PARAM_CMD
  #undef PCI_CMD
  #undef USB_CMD
  #undef VLAN_CMD
#endif

/*****************************************************************************
 *
 * Image types
 *
 */

/* Image types supported on all platforms */
#define IMAGE_DER		/* ASN.1 DER-encoded image support */
//#define IMAGE_GZIP		/* GZIP compressed image support */
#define IMAGE_PEM		/* ASN.1 PEM-encoded image support */
//#define IMAGE_PNM		/* PNM graphical image support */
#define IMAGE_PNG		/* PNG graphical image support */
#define IMAGE_SCRIPT		/* iPXE script image support */
//#define IMAGE_ZLIB		/* ZLIB compressed image support */

/* Image types supported only on BIOS platforms */
#if defined ( PLATFORM_pcbios )
  #define IMAGE_BZIMAGE		/* Linux bzImage image support */
  //#define IMAGE_COMBOOT	/* SYSLINUX COMBOOT image support */
  #define IMAGE_ELF		/* ELF image support */
  #define IMAGE_MULTIBOOT	/* MultiBoot image support */
  //#define IMAGE_NBI		/* NBI image support */
  #define IMAGE_PXE		/* PXE image support */
  //#define IMAGE_SDI		/* SDI image support */
#endif

/* Image types supported only on EFI platforms */
#if defined ( PLATFORM_efi )
  #define IMAGE_EFI		/* EFI image support */
  #define IMAGE_EFISIG		/* EFI signature list image support */
#endif

/* Image types supported only on RISC-V SBI platforms */
#if defined ( PLATFORM_sbi )
  #define IMAGE_LKRN		/* Linux kernel image support */
#endif

/* Image types supported only on x86 CPUs */
#if defined ( __i386__ ) || defined ( __x86_64__ )
  //#define IMAGE_UCODE		/* Microcode update image support */
#endif

/* Enable commonly encountered compressed versions of some image types */
#if defined ( IMAGE_EFI ) && defined ( __aarch64__ )
  #define IMAGE_GZIP
#endif
#if defined ( IMAGE_LKRN ) && defined ( __riscv )
  #define IMAGE_GZIP
#endif

/*****************************************************************************
 *
 * Banner timeout configuration
 *
 * This controls the timeout for the "Press Ctrl-B for the iPXE
 * command line" banner displayed when iPXE starts up.  The value is
 * specified in tenths of a second for which the banner should appear.
 * A value of 0 disables the banner.
 *
 * ROM_BANNER_TIMEOUT controls the "Press Ctrl-B to configure iPXE"
 * banner displayed only by ROM builds of iPXE during POST.  This
 * defaults to being twice the length of BANNER_TIMEOUT, to allow for
 * BIOSes that switch video modes immediately before calling the
 * initialisation vector, thus rendering the banner almost invisible
 * to the user.
 */

#define BANNER_TIMEOUT		20
#define ROM_BANNER_TIMEOUT	( 2 * BANNER_TIMEOUT )

/*****************************************************************************
 *
 * ROM-specific options
 *
 */

#define AUTOBOOT_ROM_FILTER	/* Autoboot only devices matching our ROM */
//#define NONPNP_HOOK_INT19	/* Hook INT19 on non-PnP BIOSes */

/*****************************************************************************
 *
 * PXE support
 *
 */

#if defined ( PLATFORM_pcbios )
  #define PXE_MENU		/* PXE menu booting */
  #define PXE_STACK		/* PXE stack in iPXE - you want this! */
#endif

/*****************************************************************************
*
 * Name resolution modules
 *
 */

#define DNS_RESOLVER		/* DNS resolver */

/*****************************************************************************
 *
 * Certificate sources
 *
 */

#if defined ( PLATFORM_efi )
  #define CERTS_EFI		/* EFI certificate sources */
#endif

/*****************************************************************************
 *
 * Virtual network devices
 *
 */

#define VNIC_IPOIB		/* Infiniband IPoIB virtual NICs */
//#define VNIC_XSIGO		/* Infiniband Xsigo virtual NICs */

/*****************************************************************************
 *
 * 802.1x EAP authentication methods
 *
 */

#define EAP_METHOD_MD5		/* MD5-Challenge port authentication */
//#define EAP_METHOD_MSCHAPV2	/* MS-CHAPv2 port authentication */

/*****************************************************************************
 *
 * 802.11 cryptosystems and handshaking protocols
 *
 */

#define CRYPTO_80211_WEP	/* WEP encryption (deprecated and insecure!) */
#define CRYPTO_80211_WPA	/* WPA Personal, with passphrase */
#define CRYPTO_80211_WPA2	/* Add support for stronger WPA cryptography */

/*****************************************************************************
 *
 * Very obscure configuration options
 *
 * You probably don't need to touch these.
 *
 */

//#define NULL_TRAP		/* Attempt to catch NULL function calls */
//#define GDBSERIAL		/* Remote GDB debugging over serial */
//#define GDBUDP		/* Remote GDB debugging over UDP */
//#define EFI_DOWNGRADE_UX	/* Downgrade UEFI user experience */
#define TIVOLI_VMM_WORKAROUND	/* Work around the Tivoli VMM's garbling of SSE
				 * registers when iPXE traps to it due to
				 * privileged instructions */
//#define ERRMSG_80211		/* All 802.11 error descriptions (~3.3kb) */

#include <config/named.h>
#include NAMED_CONFIG(general.h)
#include <config/local/general.h>
#include LOCAL_NAMED_CONFIG(general.h)

#endif /* CONFIG_GENERAL_H */
