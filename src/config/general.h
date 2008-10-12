#ifndef CONFIG_GENERAL_H
#define CONFIG_GENERAL_H

/** @file
 *
 * General configuration
 *
 */

/*
 * Timer configuration
 *
 */
#define TIMER_BIOS		/* 18Hz BIOS timer */
#define TIMER_RDTSC		/* CPU TimeStamp Counter timer */
#define BANNER_TIMEOUT	20	/* Tenths of a second for which the shell
				   banner should appear */

/*
 * Network protocols
 *
 */

#define	NET_PROTO_IPV4		/* IPv4 protocol */

/*
 * Download protocols
 *
 */

#define	DOWNLOAD_PROTO_TFTP	/* Trivial File Transfer Protocol */
#undef	DOWNLOAD_PROTO_NFS	/* Network File System */
#define	DOWNLOAD_PROTO_HTTP	/* Hypertext Transfer Protocol */
#undef	DOWNLOAD_PROTO_HTTPS	/* Secure Hypertext Transfer Protocol */
#undef	DOWNLOAD_PROTO_FTP	/* File Transfer Protocol */
#undef	DOWNLOAD_PROTO_TFTM	/* Multicast Trivial File Transfer Protocol */
#undef	DOWNLOAD_PROTO_SLAM	/* Scalable Local Area Multicast */
#undef	DOWNLOAD_PROTO_FSP	/* FSP? */

/*
 * Name resolution modules
 *
 */

#define	DNS_RESOLVER		/* DNS resolver */
#undef	NMB_RESOLVER		/* NMB resolver */

/*
 * Image types
 *
 * Etherboot supports various image formats.  Select whichever ones
 * you want to use.
 *
 */
#undef	IMAGE_NBI		/* NBI image support */
#define	IMAGE_ELF		/* ELF image support */
#undef	IMAGE_FREEBSD		/* FreeBSD kernel image support */
#define	IMAGE_MULTIBOOT		/* MultiBoot image support */
#undef	IMAGE_AOUT		/* a.out image support */
#undef	IMAGE_WINCE		/* WinCE image support */
#define	IMAGE_PXE		/* PXE image support */
#define IMAGE_SCRIPT		/* gPXE script image support */
#define IMAGE_BZIMAGE		/* Linux bzImage image support */
#define IMAGE_COMBOOT		/* SYSLINUX COMBOOT image support */

/*
 * Command-line commands to include
 *
 */
#define	AUTOBOOT_CMD		/* Automatic booting */
#define	NVO_CMD			/* Non-volatile option storage commands */
#define	CONFIG_CMD		/* Option configuration console */
#define	IFMGMT_CMD		/* Interface management commands */
#define	ROUTE_CMD		/* Routing table management commands */
#define IMAGE_CMD		/* Image management commands */
#define DHCP_CMD		/* DHCP management commands */
#define SANBOOT_CMD		/* SAN boot commands */

/*
 * Obscure configuration options
 *
 * You probably don't need to touch these.
 *
 */

#undef	BUILD_SERIAL		/* Include an automatic build serial
				 * number.  Add "bs" to the list of
				 * make targets.  For example:
				 * "make bin/rtl8139.dsk bs" */
#undef	BUILD_ID		/* Include a custom build ID string,
				 * e.g "test-foo" */
#undef	NULL_TRAP		/* Attempt to catch NULL function calls */
#undef	GDBSERIAL		/* Remote GDB debugging over serial */
#undef	GDBUDP			/* Remote GDB debugging over UDP
				 * (both may be set) */

#endif /* CONFIG_GENERAL_H */
