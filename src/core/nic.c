/**************************************************************************
Etherboot -  Network Bootstrap Program

Literature dealing with the network protocols:
	ARP - RFC826
	RARP - RFC903
	IP - RFC791
	UDP - RFC768
	BOOTP - RFC951, RFC2132 (vendor extensions)
	DHCP - RFC2131, RFC2132, RFC3004 (options)
	TFTP - RFC1350, RFC2347 (options), RFC2348 (blocksize), RFC2349 (tsize)
	RPC - RFC1831, RFC1832 (XDR), RFC1833 (rpcbind/portmapper)
	NFS - RFC1094, RFC1813 (v3, useful for clarifications, not implemented)
	IGMP - RFC1112, RFC2113, RFC2365, RFC2236, RFC3171

**************************************************************************/
#include "etherboot.h"
#include "nic.h"
#include "elf.h" /* FOR EM_CURRENT */

struct arptable_t	arptable[MAX_ARP];
#if MULTICAST_LEVEL2
unsigned long last_igmpv1 = 0;
struct igmptable_t	igmptable[MAX_IGMP];
#endif
/* Put rom_info in .nocompress section so romprefix.S can write to it */
struct rom_info	rom __attribute__ ((section (".text16.nocompress"))) = {0,0};
static unsigned long	netmask;
/* Used by nfs.c */
char *hostname = "";
int hostnamelen = 0;
static uint32_t xid;
unsigned char *end_of_rfc1533 = NULL;
static int vendorext_isvalid;
static const unsigned char vendorext_magic[] = {0xE4,0x45,0x74,0x68}; /* äEth */
static const unsigned char broadcast[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static const in_addr zeroIP = { 0L };

struct bootpd_t bootp_data;

#ifdef	NO_DHCP_SUPPORT
static unsigned char	rfc1533_cookie[5] = { RFC1533_COOKIE, RFC1533_END };
#else	/* !NO_DHCP_SUPPORT */
static int dhcp_reply;
static in_addr dhcp_server = { 0L };
static in_addr dhcp_addr = { 0L };
static unsigned char	rfc1533_cookie[] = { RFC1533_COOKIE };
#define DHCP_MACHINE_INFO_SIZE (sizeof dhcp_machine_info)
static unsigned char dhcp_machine_info[] = {
	/* Our enclosing DHCP tag */
	RFC1533_VENDOR_ETHERBOOT_ENCAP, 11,
	/* Our boot device */
	RFC1533_VENDOR_NIC_DEV_ID, 5, PCI_BUS_TYPE, 0, 0, 0, 0,
	/* Our current architecture */
	RFC1533_VENDOR_ARCH, 2, EM_CURRENT & 0xff, (EM_CURRENT >> 8) & 0xff,
#ifdef EM_CURRENT_64
	/* The 64bit version of our current architecture */
	RFC1533_VENDOR_ARCH, 2, EM_CURRENT_64 & 0xff, (EM_CURRENT_64 >> 8) & 0xff,
#undef DHCP_MACHINE_INFO_SIZE
#define DHCP_MACHINE_INFO_SIZE (sizeof(dhcp_machine_info) - (EM_CURRENT_64_PRESENT? 0: 4))
#endif /* EM_CURRENT_64 */
};
static const unsigned char dhcpdiscover[] = {
	RFC2132_MSG_TYPE,1,DHCPDISCOVER,
	RFC2132_MAX_SIZE,2,	/* request as much as we can */
	ETH_MAX_MTU / 256, ETH_MAX_MTU % 256,
#ifdef PXE_DHCP_STRICT
	RFC3679_PXE_CLIENT_UUID,RFC3679_PXE_CLIENT_UUID_LENGTH,RFC3679_PXE_CLIENT_UUID_DEFAULT,
	RFC3679_PXE_CLIENT_ARCH,RFC3679_PXE_CLIENT_ARCH_LENGTH,RFC3679_PXE_CLIENT_ARCH_IAX86PC,
	RFC3679_PXE_CLIENT_NDI, RFC3679_PXE_CLIENT_NDI_LENGTH, RFC3679_PXE_CLIENT_NDI_21,
	RFC2132_VENDOR_CLASS_ID,RFC2132_VENDOR_CLASS_ID_PXE_LENGTH,RFC2132_VENDOR_CLASS_ID_PXE,
#else
	RFC2132_VENDOR_CLASS_ID,13,'E','t','h','e','r','b','o','o','t',
	'-',VERSION_MAJOR+'0','.',VERSION_MINOR+'0',
#endif /* PXE_DHCP_STRICT */
#ifdef DHCP_CLIENT_ID
	/* Client ID Option */
	RFC2132_CLIENT_ID, ( DHCP_CLIENT_ID_LEN + 1 ),
	DHCP_CLIENT_ID_TYPE, DHCP_CLIENT_ID,
#endif /* DHCP_CLIENT_ID */
#ifdef DHCP_USER_CLASS
	/* User Class Option */
	RFC3004_USER_CLASS, DHCP_USER_CLASS_LEN, DHCP_USER_CLASS,
#endif /* DHCP_USER_CLASS */
	RFC2132_PARAM_LIST,
#define DHCPDISCOVER_PARAMS_BASE 4
#ifdef  PXE_DHCP_STRICT
#define DHCPDISCOVER_PARAMS_PXE ( 1 + 8 )
#else
#define DHCPDISCOVER_PARAMS_PXE 0
#endif /* PXE_DHCP_STRICT */
#ifdef  DNS_RESOLVER
#define DHCPDISCOVER_PARAMS_DNS  1
#else
#define DHCPDISCOVER_PARAMS_DNS  0
#endif /* DNS_RESOLVER */
	( DHCPDISCOVER_PARAMS_BASE +
	  DHCPDISCOVER_PARAMS_PXE+
	  DHCPDISCOVER_PARAMS_DNS ),
	RFC1533_NETMASK,
	RFC1533_GATEWAY,
	RFC1533_HOSTNAME,
	RFC1533_VENDOR
#ifdef PXE_DHCP_STRICT
	,RFC2132_VENDOR_CLASS_ID,
	RFC1533_VENDOR_PXE_OPT128,
	RFC1533_VENDOR_PXE_OPT129,
	RFC1533_VENDOR_PXE_OPT130,
	RFC1533_VENDOR_PXE_OPT131,
	RFC1533_VENDOR_PXE_OPT132,
	RFC1533_VENDOR_PXE_OPT133,
	RFC1533_VENDOR_PXE_OPT134,
	RFC1533_VENDOR_PXE_OPT135
#endif /* PXE_DHCP_STRICT */
#ifdef	DNS_RESOLVER
	,RFC1533_DNS
#endif
};
static const unsigned char dhcprequest [] = {
	RFC2132_MSG_TYPE,1,DHCPREQUEST,
	RFC2132_SRV_ID,4,0,0,0,0,
	RFC2132_REQ_ADDR,4,0,0,0,0,
	RFC2132_MAX_SIZE,2,	/* request as much as we can */
	ETH_MAX_MTU / 256, ETH_MAX_MTU % 256,
#ifdef PXE_DHCP_STRICT
	RFC3679_PXE_CLIENT_UUID,RFC3679_PXE_CLIENT_UUID_LENGTH,RFC3679_PXE_CLIENT_UUID_DEFAULT,
	RFC3679_PXE_CLIENT_ARCH,RFC3679_PXE_CLIENT_ARCH_LENGTH,RFC3679_PXE_CLIENT_ARCH_IAX86PC,
	RFC3679_PXE_CLIENT_NDI, RFC3679_PXE_CLIENT_NDI_LENGTH, RFC3679_PXE_CLIENT_NDI_21,
	RFC2132_VENDOR_CLASS_ID,RFC2132_VENDOR_CLASS_ID_PXE_LENGTH,RFC2132_VENDOR_CLASS_ID_PXE,
#else
	RFC2132_VENDOR_CLASS_ID,13,'E','t','h','e','r','b','o','o','t',
	'-',VERSION_MAJOR+'0','.',VERSION_MINOR+'0',
#endif /* PXE_DHCP_STRICT */
#ifdef DHCP_CLIENT_ID
	/* Client ID Option */
	RFC2132_CLIENT_ID, ( DHCP_CLIENT_ID_LEN + 1 ),
	DHCP_CLIENT_ID_TYPE, DHCP_CLIENT_ID,
#endif /* DHCP_CLIENT_ID */
#ifdef DHCP_USER_CLASS
	/* User Class Option */
	RFC3004_USER_CLASS, DHCP_USER_CLASS_LEN, DHCP_USER_CLASS,
#endif /* DHCP_USER_CLASS */
	/* request parameters */
	RFC2132_PARAM_LIST,
#define DHCPREQUEST_PARAMS_BASE 5  
#ifdef  PXE_DHCP_STRICT
#define DHCPREQUEST_PARAMS_PXE 1
#define DHCPREQUEST_PARAMS_VENDOR_PXE 8
#define DHCPREQUEST_PARAMS_VENDOR_EB 0
#else
#define DHCPREQUEST_PARAMS_PXE 0
#define DHCPREQUEST_PARAMS_VENDOR_PXE 0
#define DHCPREQUEST_PARAMS_VENDOR_EB 4
#endif /* PXE_DHCP_STRICT */
#ifdef	IMAGE_FREEBSD
#define DHCPREQUEST_PARAMS_FREEBSD 2
#else
#define DHCPREQUEST_PARAMS_FREEBSD 0
#endif /* IMAGE_FREEBSD */
#ifdef  DNS_RESOLVER
#define DHCPREQUEST_PARAMS_DNS     1
#else
#define DHCPREQUEST_PARAMS_DNS     0
#endif /* DNS_RESOLVER */
	( DHCPREQUEST_PARAMS_BASE +
	  DHCPREQUEST_PARAMS_PXE +
	  DHCPREQUEST_PARAMS_VENDOR_PXE +
	  DHCPREQUEST_PARAMS_VENDOR_EB +
	  DHCPREQUEST_PARAMS_DNS +
	  DHCPREQUEST_PARAMS_FREEBSD ),
	/* 5 Standard parameters */
	RFC1533_NETMASK,
	RFC1533_GATEWAY,
	RFC1533_HOSTNAME,
	RFC1533_VENDOR,
	RFC1533_ROOTPATH,	/* only passed to the booted image */
#ifndef PXE_DHCP_STRICT
	/* 4 Etherboot vendortags */
	RFC1533_VENDOR_MAGIC,
	RFC1533_VENDOR_ADDPARM,
	RFC1533_VENDOR_ETHDEV,
	RFC1533_VENDOR_ETHERBOOT_ENCAP,
#endif /* ! PXE_DHCP_STRICT */
#ifdef	IMAGE_FREEBSD
	/* 2 FreeBSD options */
	RFC1533_VENDOR_HOWTO,
	RFC1533_VENDOR_KERNEL_ENV,
#endif
#ifdef	DNS_RESOLVER
	/* 1 DNS option */
	RFC1533_DNS,
#endif
#ifdef  PXE_DHCP_STRICT
	RFC2132_VENDOR_CLASS_ID,
	RFC1533_VENDOR_PXE_OPT128,
	RFC1533_VENDOR_PXE_OPT129,
	RFC1533_VENDOR_PXE_OPT130,
	RFC1533_VENDOR_PXE_OPT131,
	RFC1533_VENDOR_PXE_OPT132,
	RFC1533_VENDOR_PXE_OPT133,
	RFC1533_VENDOR_PXE_OPT134,
	RFC1533_VENDOR_PXE_OPT135,
#endif /* PXE_DHCP_STRICT */
};
#ifdef PXE_EXPORT
static const unsigned char proxydhcprequest [] = {
	RFC2132_MSG_TYPE,1,DHCPREQUEST,
	RFC2132_MAX_SIZE,2,	/* request as much as we can */
	ETH_MAX_MTU / 256, ETH_MAX_MTU % 256,
#ifdef  PXE_DHCP_STRICT
	RFC3679_PXE_CLIENT_UUID,RFC3679_PXE_CLIENT_UUID_LENGTH,RFC3679_PXE_CLIENT_UUID_DEFAULT,
	RFC3679_PXE_CLIENT_ARCH,RFC3679_PXE_CLIENT_ARCH_LENGTH,RFC3679_PXE_CLIENT_ARCH_IAX86PC,
	RFC3679_PXE_CLIENT_NDI, RFC3679_PXE_CLIENT_NDI_LENGTH, RFC3679_PXE_CLIENT_NDI_21,
	RFC2132_VENDOR_CLASS_ID,RFC2132_VENDOR_CLASS_ID_PXE_LENGTH,RFC2132_VENDOR_CLASS_ID_PXE,
#endif /* PXE_DHCP_STRICT */
};
#endif

#ifdef	REQUIRE_VCI_ETHERBOOT
int	vci_etherboot;
#endif
#endif	/* NO_DHCP_SUPPORT */

static int dummy(void *unused __unused)
{
	return (0);
}

/* Careful.  We need an aligned buffer to avoid problems on machines
 * that care about alignment.  To trivally align the ethernet data
 * (the ip hdr and arp requests) we offset the packet by 2 bytes.
 * leaving the ethernet data 16 byte aligned.  Beyond this
 * we use memmove but this makes the common cast simple and fast.
 */
static char	packet[ETH_FRAME_LEN + ETH_DATA_ALIGN] __aligned;

struct nic	nic =
{
	{
		0,				/* dev.disable */
		{
			0,
			0,
			PCI_BUS_TYPE,
		},				/* dev.devid */
		0,				/* index */
		0,				/* type */
		PROBE_FIRST,			/* how_pobe */
		PROBE_NONE,			/* to_probe */
		0,				/* failsafe */
		0,				/* type_index */
		{},				/* state */
	},
	(int (*)(struct nic *, int))dummy,	/* poll */
	(void (*)(struct nic *, const char *,
		unsigned int, unsigned int,
		const char *))dummy,		/* transmit */
	(void (*)(struct nic *,
		 irq_action_t))dummy,		/* irq */
	0,					/* flags */
	&rom,					/* rom_info */
	arptable[ARP_CLIENT].node,		/* node_addr */
	packet + ETH_DATA_ALIGN,		/* packet */
	0,					/* packetlen */
	0,					/* ioaddr */
	0,					/* irqno */
	0,					/* priv_data */
};

#ifdef RARP_NOT_BOOTP
static int rarp(void);
#else
static int bootp(void);
#endif
static unsigned short tcpudpchksum(struct iphdr *ip);


int eth_probe(struct dev *dev)
{
	return probe(dev);
}

int eth_poll(int retrieve)
{
	return ((*nic.poll)(&nic, retrieve));
}

void eth_transmit(const char *d, unsigned int t, unsigned int s, const void *p)
{
	(*nic.transmit)(&nic, d, t, s, p);
	if (t == ETH_P_IP) twiddle();
}

void eth_disable(void)
{
#ifdef MULTICAST_LEVEL2
	int i;
	for(i = 0; i < MAX_IGMP; i++) {
		leave_group(i);
	}
#endif
	disable(&nic.dev);
}

void eth_irq (irq_action_t action)
{
	(*nic.irq)(&nic,action);
}

/*
 * Find out what our boot parameters are
 */
int eth_load_configuration(struct dev *dev __unused)
{
	int server_found;
	/* Find a server to get BOOTP reply from */
#ifdef	RARP_NOT_BOOTP
	printf("Searching for server (RARP)...");
#else
#ifndef	NO_DHCP_SUPPORT
	printf("Searching for server (DHCP)...");
#else
	printf("Searching for server (BOOTP)...");
#endif
#endif

#ifdef	RARP_NOT_BOOTP
	server_found = rarp();
#else
	server_found = bootp();
#endif
	if (!server_found) {
		printf("No Server found\n");
		longjmp(restart_etherboot, -1);
	}
	return 0;
}


/**************************************************************************
LOAD - Try to get booted
**************************************************************************/
int eth_load(struct dev *dev __unused)
{
	const char	*kernel;
	printf("\nMe: %@", arptable[ARP_CLIENT].ipaddr.s_addr );
#ifndef NO_DHCP_SUPPORT
	printf(", DHCP: %@", dhcp_server );
#ifdef PXE_EXPORT       
	if (arptable[ARP_PROXYDHCP].ipaddr.s_addr)
		printf(" (& %@)",
		       arptable[ARP_PROXYDHCP].ipaddr.s_addr);
#endif /* PXE_EXPORT */
#endif /* ! NO_DHCP_SUPPORT */
	printf(", TFTP: %@", arptable[ARP_SERVER].ipaddr.s_addr);
	if (BOOTP_DATA_ADDR->bootp_reply.bp_giaddr.s_addr)
		printf(", Relay: %@",
			BOOTP_DATA_ADDR->bootp_reply.bp_giaddr.s_addr);
	if (arptable[ARP_GATEWAY].ipaddr.s_addr)
		printf(", Gateway %@", arptable[ARP_GATEWAY].ipaddr.s_addr);
#ifdef	DNS_RESOLVER
	if (arptable[ARP_NAMESERVER].ipaddr.s_addr)
		printf(", Nameserver %@", arptable[ARP_NAMESERVER].ipaddr.s_addr);
#endif
	putchar('\n');

#ifdef	MDEBUG
	printf("\n=>>"); getchar();
#endif

	/* Now use TFTP to load file */
#ifdef	DOWNLOAD_PROTO_NFS
	rpc_init();
#endif
	kernel = KERNEL_BUF[0] == '\0' ? 
#ifdef	DEFAULT_BOOTFILE
		DEFAULT_BOOTFILE
#else
		NULL
#endif
		: KERNEL_BUF;
	if ( kernel ) {
		loadkernel(kernel); /* We don't return except on error */
		printf("Unable to load file.\n");
	} else {	
		printf("No filename\n");
	}
	interruptible_sleep(2);		/* lay off the server for a while */
	longjmp(restart_etherboot, -1);
}


/**************************************************************************
DEFAULT_NETMASK - Return default netmask for IP address
**************************************************************************/
static inline unsigned long default_netmask(void)
{
	int net = ntohl(arptable[ARP_CLIENT].ipaddr.s_addr) >> 24;
	if (net <= 127)
		return(htonl(0xff000000));
	else if (net < 192)
		return(htonl(0xffff0000));
	else
		return(htonl(0xffffff00));
}

/**************************************************************************
IP_TRANSMIT - Send an IP datagram
**************************************************************************/
static int await_arp(int ival, void *ptr,
	unsigned short ptype, struct iphdr *ip __unused, struct udphdr *udp __unused,
	struct tcphdr *tcp __unused)
{
	struct	arprequest *arpreply;
	if (ptype != ETH_P_ARP)
		return 0;
	if (nic.packetlen < ETH_HLEN + sizeof(struct arprequest))
		return 0;
	arpreply = (struct arprequest *)&nic.packet[ETH_HLEN];

	if (arpreply->opcode != htons(ARP_REPLY)) 
		return 0;
	if (memcmp(arpreply->sipaddr, ptr, sizeof(in_addr)) != 0)
		return 0;
	memcpy(arptable[ival].node, arpreply->shwaddr, ETH_ALEN);
	return 1;
}

int ip_transmit(int len, const void *buf)
{
	unsigned long destip;
	struct iphdr *ip;
	struct arprequest arpreq;
	int arpentry, i;
	int retry;

	ip = (struct iphdr *)buf;
	destip = ip->dest.s_addr;
	if (destip == IP_BROADCAST) {
		eth_transmit(broadcast, ETH_P_IP, len, buf);
#ifdef MULTICAST_LEVEL1 
	} else if ((destip & htonl(MULTICAST_MASK)) == htonl(MULTICAST_NETWORK)) {
		unsigned char multicast[6];
		unsigned long hdestip;
		hdestip = ntohl(destip);
		multicast[0] = 0x01;
		multicast[1] = 0x00;
		multicast[2] = 0x5e;
		multicast[3] = (hdestip >> 16) & 0x7;
		multicast[4] = (hdestip >> 8) & 0xff;
		multicast[5] = hdestip & 0xff;
		eth_transmit(multicast, ETH_P_IP, len, buf);
#endif
	} else {
		if (((destip & netmask) !=
			(arptable[ARP_CLIENT].ipaddr.s_addr & netmask)) &&
			arptable[ARP_GATEWAY].ipaddr.s_addr)
				destip = arptable[ARP_GATEWAY].ipaddr.s_addr;
		for(arpentry = 0; arpentry<MAX_ARP; arpentry++)
			if (arptable[arpentry].ipaddr.s_addr == destip) break;
		if (arpentry == MAX_ARP) {
			printf("%@ is not in my arp table!\n", destip);
			return(0);
		}
		for (i = 0; i < ETH_ALEN; i++)
			if (arptable[arpentry].node[i])
				break;
		if (i == ETH_ALEN) {	/* Need to do arp request */
			arpreq.hwtype = htons(1);
			arpreq.protocol = htons(IP);
			arpreq.hwlen = ETH_ALEN;
			arpreq.protolen = 4;
			arpreq.opcode = htons(ARP_REQUEST);
			memcpy(arpreq.shwaddr, arptable[ARP_CLIENT].node, ETH_ALEN);
			memcpy(arpreq.sipaddr, &arptable[ARP_CLIENT].ipaddr, sizeof(in_addr));
			memset(arpreq.thwaddr, 0, ETH_ALEN);
			memcpy(arpreq.tipaddr, &destip, sizeof(in_addr));
			for (retry = 1; retry <= MAX_ARP_RETRIES; retry++) {
				long timeout;
				eth_transmit(broadcast, ETH_P_ARP, sizeof(arpreq),
					&arpreq);
				timeout = rfc2131_sleep_interval(TIMEOUT, retry);
				if (await_reply(await_arp, arpentry,
					arpreq.tipaddr, timeout)) goto xmit;
			}
			return(0);
		}
xmit:
		eth_transmit(arptable[arpentry].node, ETH_P_IP, len, buf);
	}
	return 1;
}

void build_ip_hdr(unsigned long destip, int ttl, int protocol, int option_len,
	int len, const void *buf)
{
	struct iphdr *ip;
	ip = (struct iphdr *)buf;
	ip->verhdrlen = 0x45;
	ip->verhdrlen += (option_len/4);
	ip->service = 0;
	ip->len = htons(len);
	ip->ident = 0;
	ip->frags = 0; /* Should we set don't fragment? */
	ip->ttl = ttl;
	ip->protocol = protocol;
	ip->chksum = 0;
	ip->src.s_addr = arptable[ARP_CLIENT].ipaddr.s_addr;
	ip->dest.s_addr = destip;
	ip->chksum = ipchksum(buf, sizeof(struct iphdr) + option_len);
}

void build_udp_hdr(unsigned long destip, 
	unsigned int srcsock, unsigned int destsock, int ttl,
	int len, const void *buf)
{
	struct iphdr *ip;
	struct udphdr *udp;
	ip = (struct iphdr *)buf;
	build_ip_hdr(destip, ttl, IP_UDP, 0, len, buf);
	udp = (struct udphdr *)((char *)buf + sizeof(struct iphdr));
	udp->src = htons(srcsock);
	udp->dest = htons(destsock);
	udp->len = htons(len - sizeof(struct iphdr));
	udp->chksum = 0;
	if ((udp->chksum = tcpudpchksum(ip)) == 0)
		udp->chksum = 0xffff;
}

#ifdef DOWNLOAD_PROTO_HTTP
void build_tcp_hdr(unsigned long destip, unsigned int srcsock,
		  unsigned int destsock, long send_seq, long recv_seq,
		  int window, int flags, int ttl, int len, const void *buf)
{
       struct iphdr *ip;
       struct tcphdr *tcp;
       ip = (struct iphdr *)buf;
       build_ip_hdr(destip, ttl, IP_TCP, 0, len, buf);
       tcp = (struct tcphdr *)(ip + 1);
       tcp->src = htons(srcsock);
       tcp->dst = htons(destsock);
       tcp->seq = htonl(send_seq);
       tcp->ack = htonl(recv_seq);
       tcp->ctrl = htons(flags + (5 << 12)); /* No TCP options */
       tcp->window = htons(window);
       tcp->chksum = 0;
       if ((tcp->chksum = tcpudpchksum(ip)) == 0)
	       tcp->chksum = 0xffff;
}
#endif


/**************************************************************************
UDP_TRANSMIT - Send an UDP datagram
**************************************************************************/
int udp_transmit(unsigned long destip, unsigned int srcsock,
	unsigned int destsock, int len, const void *buf)
{
	build_udp_hdr(destip, srcsock, destsock, 60, len, buf);
	return ip_transmit(len, buf);
}

/**************************************************************************
TCP_TRANSMIT - Send a TCP packet
**************************************************************************/
#ifdef DOWNLOAD_PROTO_HTTP
int tcp_transmit(unsigned long destip, unsigned int srcsock,
		unsigned int destsock, long send_seq, long recv_seq,
		int window, int flags, int len, const void *buf)
{
       build_tcp_hdr(destip, srcsock, destsock, send_seq, recv_seq,
		     window, flags, 60, len, buf);
       return ip_transmit(len, buf);
}

int tcp_reset(struct iphdr *ip) {
       struct tcphdr *tcp = (struct tcphdr *)(ip + 1);
       char buf[sizeof(struct iphdr) + sizeof(struct tcphdr)];

       if (!(tcp->ctrl & htons(RST))) {
	      long seq = ntohl(tcp->seq) + ntohs(ip->len) -
			 sizeof(struct iphdr) -
			 ((ntohs(tcp->ctrl) >> 10) & 0x3C);
	      if (tcp->ctrl & htons(SYN|FIN))
		      seq++;
	      return tcp_transmit(ntohl(ip->src.s_addr),
				  ntohs(tcp->dst), ntohs(tcp->src),
				  tcp->ctrl&htons(ACK) ? ntohl(tcp->ack) : 0,
				  seq, TCP_MAX_WINDOW, RST, sizeof(buf), buf);
       }
       return (1);
}
#endif

/**************************************************************************
QDRAIN - clear the nic's receive queue
**************************************************************************/
static int await_qdrain(int ival __unused, void *ptr __unused,
	unsigned short ptype __unused, 
	struct iphdr *ip __unused, struct udphdr *udp __unused,
	struct tcphdr *tcp __unused)
{
	return 0;
}

void rx_qdrain(void)
{
	/* Clear out the Rx queue first.  It contains nothing of interest,
	 * except possibly ARP requests from the DHCP/TFTP server.  We use
	 * polling throughout Etherboot, so some time may have passed since we
	 * last polled the receive queue, which may now be filled with
	 * broadcast packets.  This will cause the reply to the packets we are
	 * about to send to be lost immediately.  Not very clever.  */
	await_reply(await_qdrain, 0, NULL, 0);
}

#ifdef	DOWNLOAD_PROTO_TFTP
/**************************************************************************
TFTP - Download extended BOOTP data, or kernel image
**************************************************************************/
static int await_tftp(int ival, void *ptr __unused,
	unsigned short ptype __unused, struct iphdr *ip, struct udphdr *udp,
	struct tcphdr *tcp __unused)
{
	if (!udp) {
		return 0;
	}
	if (arptable[ARP_CLIENT].ipaddr.s_addr != ip->dest.s_addr)
		return 0;
	if (ntohs(udp->dest) != ival)
		return 0;
	return 1;
}

int tftp ( const char *name,
	   int (*fnc)(unsigned char *, unsigned int, unsigned int, int) )
{
	struct tftpreq_info_t request_data =
		{ name, TFTP_PORT, TFTP_MAX_PACKET };
	struct tftpreq_info_t *request = &request_data;
	struct tftpblk_info_t block;
	int rc;

	while ( tftp_block ( request, &block ) ) {
		request = NULL; /* Send request only once */
		rc = fnc ( block.data, block.block, block.len, block.eof );
		if ( rc <= 0 ) return (rc);
		if ( block.eof ) {
			/* fnc should not have returned */
			printf ( "TFTP download complete, but\n" );
			return (0);
		}
	}
	return (0);
}

int tftp_block ( struct tftpreq_info_t *request, struct tftpblk_info_t *block )
{
	static unsigned short lport = 2000; /* local port */
	static unsigned short rport = TFTP_PORT; /* remote port */
	struct tftp_t *rcvd = NULL;
	static struct tftpreq_t xmit;
	static unsigned short xmitlen = 0;
	static unsigned short blockidx = 0; /* Last block received */
	static unsigned short retry = 0; /* Retry attempts on last block */
	static int blksize = 0;
	unsigned short recvlen = 0;

	/* If this is a new request (i.e. if name is set), fill in
	 * transmit block with RRQ and send it.
	 */
	if ( request ) {
		rx_qdrain(); /* Flush receive queue */
		xmit.opcode = htons(TFTP_RRQ);
		xmitlen = (void*)&xmit.u.rrq - (void*)&xmit +
			sprintf((char*)xmit.u.rrq, "%s%coctet%cblksize%c%d",
				request->name, 0, 0, 0, request->blksize)
			+ 1; /* null terminator */
		blockidx = 0; /* Reset counters */
		retry = 0;
		blksize = TFTP_DEFAULTSIZE_PACKET;
		lport++; /* Use new local port */
		rport = request->port;
		if ( !udp_transmit(arptable[ARP_SERVER].ipaddr.s_addr, lport,
				   rport, xmitlen, &xmit) )
			return (0);
	}
	/* Exit if no transfer in progress */
	if ( !blksize ) return (0);
	/* Loop to wait until we get a packet we're interested in */
	block->data = NULL; /* Used as flag */
	while ( block->data == NULL ) {
		long timeout = rfc2131_sleep_interval ( blockidx ? TFTP_REXMT :
							TIMEOUT, retry );
		if ( !await_reply(await_tftp, lport, NULL, timeout) ) {
			/* No packet received */
			if ( retry++ > MAX_TFTP_RETRIES ) break;
			/* Retransmit last packet */
			if ( !blockidx ) lport++; /* New lport if new RRQ */
			if ( !udp_transmit(arptable[ARP_SERVER].ipaddr.s_addr,
					   lport, rport, xmitlen, &xmit) )
				return (0);
			continue; /* Back to waiting for packet */
		}
		/* Packet has been received */
		rcvd = (struct tftp_t *)&nic.packet[ETH_HLEN];
		recvlen = ntohs(rcvd->udp.len) - sizeof(struct udphdr)
			- sizeof(rcvd->opcode);
		rport = ntohs(rcvd->udp.src);
		retry = 0; /* Reset retry counter */
		switch ( htons(rcvd->opcode) ) {
		case TFTP_ERROR : {
			printf ( "TFTP error %d (%s)\n",
				 ntohs(rcvd->u.err.errcode),
				 rcvd->u.err.errmsg );
			return (0); /* abort */
		}
		case TFTP_OACK : {
			const char *p = rcvd->u.oack.data;
			const char *e = p + recvlen - 10; /* "blksize\0\d\0" */

			*((char*)(p+recvlen-1)) = '\0'; /* Force final 0 */
			if ( blockidx || !request ) break; /* Too late */
			if ( recvlen <= TFTP_MAX_PACKET ) /* sanity */ {
				/* Check for blksize option honoured */
				while ( p < e ) {
					if ( strcasecmp("blksize",p) == 0 &&
					     p[7] == '\0' ) {
						blksize = strtoul(p+8,&p,10);
						p++; /* skip null */
					}
					while ( *(p++) ) {};
				}
			}
			if ( blksize < TFTP_DEFAULTSIZE_PACKET || blksize > request->blksize ) {
				/* Incorrect blksize - error and abort */
				xmit.opcode = htons(TFTP_ERROR);
				xmit.u.err.errcode = 8;
				xmitlen = (void*)&xmit.u.err.errmsg
					- (void*)&xmit
					+ sprintf((char*)xmit.u.err.errmsg,
						  "RFC1782 error")
					+ 1;
				udp_transmit(
				    arptable[ARP_SERVER].ipaddr.s_addr,
				    lport, rport, xmitlen, &xmit);
				return (0);
			}
		} break;
		case TFTP_DATA :
			if ( ntohs(rcvd->u.data.block) != ( blockidx + 1 ) )
				break; /* Re-ACK last block sent */
			if ( recvlen > ( blksize+sizeof(rcvd->u.data.block) ) )
				break; /* Too large; ignore */
			block->data = rcvd->u.data.download;
			block->block = ++blockidx;
			block->len = recvlen - sizeof(rcvd->u.data.block);
			block->eof = ( (unsigned short)block->len < blksize );
			/* If EOF, zero blksize to indicate transfer done */
			if ( block->eof ) blksize = 0;
			break;
	        default: break;	/* Do nothing */
		}
		/* Send ACK */
		xmit.opcode = htons(TFTP_ACK);
		xmit.u.ack.block = htons(blockidx);
		xmitlen = TFTP_MIN_PACKET;
		udp_transmit ( arptable[ARP_SERVER].ipaddr.s_addr,
			       lport, rport, xmitlen, &xmit );
	}
	return ( block->data ? 1 : 0 );
}
#endif	/* DOWNLOAD_PROTO_TFTP */

#ifdef	RARP_NOT_BOOTP
/**************************************************************************
RARP - Get my IP address and load information
**************************************************************************/
static int await_rarp(int ival, void *ptr,
	unsigned short ptype, struct iphdr *ip, struct udphdr *udp,
	struct tcphdr *tcp __unused)
{
	struct arprequest *arpreply;
	if (ptype != ETH_P_RARP)
		return 0;
	if (nic.packetlen < ETH_HLEN + sizeof(struct arprequest))
		return 0;
	arpreply = (struct arprequest *)&nic.packet[ETH_HLEN];
	if (arpreply->opcode != htons(RARP_REPLY))
		return 0;
	if ((arpreply->opcode == htons(RARP_REPLY)) &&
		(memcmp(arpreply->thwaddr, ptr, ETH_ALEN) == 0)) {
		memcpy(arptable[ARP_SERVER].node, arpreply->shwaddr, ETH_ALEN);
		memcpy(&arptable[ARP_SERVER].ipaddr, arpreply->sipaddr, sizeof(in_addr));
		memcpy(&arptable[ARP_CLIENT].ipaddr, arpreply->tipaddr, sizeof(in_addr));
		return 1;
	}
	return 0;
}

static int rarp(void)
{
	int retry;

	/* arp and rarp requests share the same packet structure. */
	struct arprequest rarpreq;

	memset(&rarpreq, 0, sizeof(rarpreq));

	rarpreq.hwtype = htons(1);
	rarpreq.protocol = htons(IP);
	rarpreq.hwlen = ETH_ALEN;
	rarpreq.protolen = 4;
	rarpreq.opcode = htons(RARP_REQUEST);
	memcpy(&rarpreq.shwaddr, arptable[ARP_CLIENT].node, ETH_ALEN);
	/* sipaddr is already zeroed out */
	memcpy(&rarpreq.thwaddr, arptable[ARP_CLIENT].node, ETH_ALEN);
	/* tipaddr is already zeroed out */

	for (retry = 0; retry < MAX_ARP_RETRIES; ++retry) {
		long timeout;
		eth_transmit(broadcast, ETH_P_RARP, sizeof(rarpreq), &rarpreq);

		timeout = rfc2131_sleep_interval(TIMEOUT, retry);
		if (await_reply(await_rarp, 0, rarpreq.shwaddr, timeout))
			break;
	}

	if (retry < MAX_ARP_RETRIES) {
		(void)sprintf(KERNEL_BUF, DEFAULT_KERNELPATH, arptable[ARP_CLIENT].ipaddr);

		return (1);
	}
	return (0);
}

#else

/**************************************************************************
BOOTP - Get my IP address and load information
**************************************************************************/
static int await_bootp(int ival __unused, void *ptr __unused,
	unsigned short ptype __unused, struct iphdr *ip __unused, 
	struct udphdr *udp, struct tcphdr *tcp __unused)
{
	struct	bootp_t *bootpreply;
	if (!udp) {
		return 0;
	}
	bootpreply = (struct bootp_t *)&nic.packet[ETH_HLEN + 
		sizeof(struct iphdr) + sizeof(struct udphdr)];
	if (nic.packetlen < ETH_HLEN + sizeof(struct iphdr) + 
		sizeof(struct udphdr) + 
#ifdef NO_DHCP_SUPPORT
		sizeof(struct bootp_t)
#else
		sizeof(struct bootp_t) - DHCP_OPT_LEN
#endif	/* NO_DHCP_SUPPORT */
		) {
		return 0;
	}
	if (udp->dest != htons(BOOTP_CLIENT))
		return 0;
	if (bootpreply->bp_op != BOOTP_REPLY)
		return 0;
	if (bootpreply->bp_xid != xid)
		return 0;
	if (memcmp(&bootpreply->bp_siaddr, &zeroIP, sizeof(in_addr)) == 0)
		return 0;
	if ((memcmp(broadcast, bootpreply->bp_hwaddr, ETH_ALEN) != 0) &&
		(memcmp(arptable[ARP_CLIENT].node, bootpreply->bp_hwaddr, ETH_ALEN) != 0)) {
		return 0;
	}
	if ( bootpreply->bp_siaddr.s_addr ) {
		arptable[ARP_SERVER].ipaddr.s_addr = bootpreply->bp_siaddr.s_addr;
		memset(arptable[ARP_SERVER].node, 0, ETH_ALEN);	 /* Kill arp */
	}
	if ( bootpreply->bp_giaddr.s_addr ) {
		arptable[ARP_GATEWAY].ipaddr.s_addr = bootpreply->bp_giaddr.s_addr;
		memset(arptable[ARP_GATEWAY].node, 0, ETH_ALEN);  /* Kill arp */
	}
	if (bootpreply->bp_yiaddr.s_addr) {
		/* Offer with an IP address */
		arptable[ARP_CLIENT].ipaddr.s_addr = bootpreply->bp_yiaddr.s_addr;
#ifndef	NO_DHCP_SUPPORT
		dhcp_addr.s_addr = bootpreply->bp_yiaddr.s_addr;
#endif	/* NO_DHCP_SUPPORT */
		netmask = default_netmask();
		/* bootpreply->bp_file will be copied to KERNEL_BUF in the memcpy */
		memcpy((char *)BOOTP_DATA_ADDR, (char *)bootpreply, sizeof(struct bootpd_t));
		decode_rfc1533(BOOTP_DATA_ADDR->bootp_reply.bp_vend, 0,
#ifdef	NO_DHCP_SUPPORT
			       BOOTP_VENDOR_LEN + MAX_BOOTP_EXTLEN, 
#else
			       DHCP_OPT_LEN + MAX_BOOTP_EXTLEN, 
#endif	/* NO_DHCP_SUPPORT */
			       1);
#ifdef PXE_EXPORT
	} else {
		/* Offer without an IP address - use as ProxyDHCP server */
		arptable[ARP_PROXYDHCP].ipaddr.s_addr = bootpreply->bp_siaddr.s_addr;
		memset(arptable[ARP_PROXYDHCP].node, 0, ETH_ALEN);	 /* Kill arp */
		/* Grab only the bootfile name from a ProxyDHCP packet */
		memcpy(KERNEL_BUF, bootpreply->bp_file, sizeof(KERNEL_BUF));
#endif /* PXE_EXPORT */
	}
#ifdef	REQUIRE_VCI_ETHERBOOT
	if (!vci_etherboot)
		return (0);
#endif
	return(1);
}

static int bootp(void)
{
	int retry;
#ifndef	NO_DHCP_SUPPORT
	int reqretry;
#endif	/* NO_DHCP_SUPPORT */
	struct bootpip_t ip;
	unsigned long  starttime;
	unsigned char *bp_vend;

#ifndef	NO_DHCP_SUPPORT
	dhcp_machine_info[4] = nic.dev.devid.bus_type;
	dhcp_machine_info[5] = nic.dev.devid.vendor_id & 0xff;
	dhcp_machine_info[6] = ((nic.dev.devid.vendor_id) >> 8) & 0xff;
	dhcp_machine_info[7] = nic.dev.devid.device_id & 0xff;
	dhcp_machine_info[8] = ((nic.dev.devid.device_id) >> 8) & 0xff;
#endif	/* NO_DHCP_SUPPORT */
	memset(&ip, 0, sizeof(struct bootpip_t));
	ip.bp.bp_op = BOOTP_REQUEST;
	ip.bp.bp_htype = 1;
	ip.bp.bp_hlen = ETH_ALEN;
	starttime = currticks();
	/* Use lower 32 bits of node address, more likely to be
	   distinct than the time since booting */
	memcpy(&xid, &arptable[ARP_CLIENT].node[2], sizeof(xid));
	ip.bp.bp_xid = xid += htonl(starttime);
	memcpy(ip.bp.bp_hwaddr, arptable[ARP_CLIENT].node, ETH_ALEN);
#ifdef	NO_DHCP_SUPPORT
	memcpy(ip.bp.bp_vend, rfc1533_cookie, 5); /* request RFC-style options */
#else
	memcpy(ip.bp.bp_vend, rfc1533_cookie, sizeof rfc1533_cookie); /* request RFC-style options */
	memcpy(ip.bp.bp_vend + sizeof rfc1533_cookie, dhcpdiscover, sizeof dhcpdiscover);
	/* Append machine_info to end, in encapsulated option */
	bp_vend = ip.bp.bp_vend + sizeof rfc1533_cookie + sizeof dhcpdiscover;
	memcpy(bp_vend, dhcp_machine_info, DHCP_MACHINE_INFO_SIZE);
	bp_vend += DHCP_MACHINE_INFO_SIZE;
	*bp_vend++ = RFC1533_END;
#endif	/* NO_DHCP_SUPPORT */

	for (retry = 0; retry < MAX_BOOTP_RETRIES; ) {
		uint8_t my_hwaddr[ETH_ALEN];
		unsigned long stop_time;
		long remaining_time;

		rx_qdrain();

		/* Kill arptable to avoid keeping stale entries */
		memcpy ( my_hwaddr, arptable[ARP_CLIENT].node, ETH_ALEN );
		memset ( arptable, 0, sizeof(arptable) );
		memcpy ( arptable[ARP_CLIENT].node, my_hwaddr, ETH_ALEN );

		udp_transmit(IP_BROADCAST, BOOTP_CLIENT, BOOTP_SERVER,
			sizeof(struct bootpip_t), &ip);
		remaining_time = rfc2131_sleep_interval(BOOTP_TIMEOUT, retry++);
		stop_time = currticks() + remaining_time;
#ifdef	NO_DHCP_SUPPORT
		if (await_reply(await_bootp, 0, NULL, timeout))
			return(1);
#else
		while ( remaining_time > 0 ) {
			if (await_reply(await_bootp, 0, NULL, remaining_time)){
			}
			remaining_time = stop_time - currticks();
		}
		if ( ! arptable[ARP_CLIENT].ipaddr.s_addr ) {
			printf("No IP address\n");
			continue;
		}
		/* If not a DHCPOFFER then must be just a BOOTP reply,
		 * be backward compatible with BOOTP then */
		if (dhcp_reply != DHCPOFFER)
			return(1);
		dhcp_reply = 0;
		/* Construct the DHCPREQUEST packet */
		memcpy(ip.bp.bp_vend, rfc1533_cookie, sizeof rfc1533_cookie);
		memcpy(ip.bp.bp_vend + sizeof rfc1533_cookie, dhcprequest, sizeof dhcprequest);
		/* Beware: the magic numbers 9 and 15 depend on
		   the layout of dhcprequest */
		memcpy(&ip.bp.bp_vend[9], &dhcp_server, sizeof(in_addr));
		memcpy(&ip.bp.bp_vend[15], &dhcp_addr, sizeof(in_addr));
		bp_vend = ip.bp.bp_vend + sizeof rfc1533_cookie + sizeof dhcprequest;
		/* Append machine_info to end, in encapsulated option */
		memcpy(bp_vend, dhcp_machine_info, DHCP_MACHINE_INFO_SIZE);
		bp_vend += DHCP_MACHINE_INFO_SIZE;
		*bp_vend++ = RFC1533_END;
		for (reqretry = 0; reqretry < MAX_BOOTP_RETRIES; ) {
			unsigned long timeout;

			udp_transmit(IP_BROADCAST, BOOTP_CLIENT, BOOTP_SERVER,
				     sizeof(struct bootpip_t), &ip);
			dhcp_reply=0;
			timeout = rfc2131_sleep_interval(TIMEOUT, reqretry++);
			if (!await_reply(await_bootp, 0, NULL, timeout))
				continue;
			if (dhcp_reply != DHCPACK)
				continue;
			dhcp_reply = 0;
#ifdef PXE_EXPORT			
			if ( arptable[ARP_PROXYDHCP].ipaddr.s_addr ) {
				/* Construct the ProxyDHCPREQUEST packet */
				memcpy(ip.bp.bp_vend, rfc1533_cookie, sizeof rfc1533_cookie);
				memcpy(ip.bp.bp_vend + sizeof rfc1533_cookie, proxydhcprequest, sizeof proxydhcprequest);
				for (reqretry = 0; reqretry < MAX_BOOTP_RETRIES; ) {
					printf ( "\nSending ProxyDHCP request to %@...", arptable[ARP_PROXYDHCP].ipaddr.s_addr);
					udp_transmit(arptable[ARP_PROXYDHCP].ipaddr.s_addr, BOOTP_CLIENT, PROXYDHCP_SERVER,
						     sizeof(struct bootpip_t), &ip);
					timeout = rfc2131_sleep_interval(TIMEOUT, reqretry++);
					if (await_reply(await_bootp, 0, NULL, timeout)) {
						break;
					}
				}
			}
#endif /* PXE_EXPORT */
			return(1);
		}
#endif	/* NO_DHCP_SUPPORT */
		ip.bp.bp_secs = htons((currticks()-starttime)/TICKS_PER_SEC);
	}
	return(0);
}
#endif	/* RARP_NOT_BOOTP */

static uint16_t tcpudpchksum(struct iphdr *ip)
{
	struct udp_pseudo_hdr pseudo;
	uint16_t checksum;

	/* Compute the pseudo header */
	pseudo.src.s_addr  = ip->src.s_addr;
	pseudo.dest.s_addr = ip->dest.s_addr;
	pseudo.unused	   = 0;
	pseudo.protocol	   = ip->protocol;
	pseudo.len	   = htons(ntohs(ip->len) - sizeof(struct iphdr));

	/* Sum the pseudo header */
	checksum = ipchksum(&pseudo, 12);

	/* Sum the rest of the tcp/udp packet */
	checksum = add_ipchksums(12, checksum, ipchksum(ip + 1,
				 ntohs(ip->len) - sizeof(struct iphdr)));
	return checksum;
}

#ifdef MULTICAST_LEVEL2
static void send_igmp_reports(unsigned long now)
{
	int i;
	for(i = 0; i < MAX_IGMP; i++) {
		if (igmptable[i].time && (now >= igmptable[i].time)) {
			struct igmp_ip_t igmp;
			igmp.router_alert[0] = 0x94;
			igmp.router_alert[1] = 0x04;
			igmp.router_alert[2] = 0;
			igmp.router_alert[3] = 0;
			build_ip_hdr(igmptable[i].group.s_addr, 
				1, IP_IGMP, sizeof(igmp.router_alert), sizeof(igmp), &igmp);
			igmp.igmp.type = IGMPv2_REPORT;
			if (last_igmpv1 && 
				(now < last_igmpv1 + IGMPv1_ROUTER_PRESENT_TIMEOUT)) {
				igmp.igmp.type = IGMPv1_REPORT;
			}
			igmp.igmp.response_time = 0;
			igmp.igmp.chksum = 0;
			igmp.igmp.group.s_addr = igmptable[i].group.s_addr;
			igmp.igmp.chksum = ipchksum(&igmp.igmp, sizeof(igmp.igmp));
			ip_transmit(sizeof(igmp), &igmp);
#ifdef	MDEBUG
			printf("Sent IGMP report to: %@\n", igmp.igmp.group.s_addr);
#endif
			/* Don't send another igmp report until asked */
			igmptable[i].time = 0;
		}
	}
}

static void process_igmp(struct iphdr *ip, unsigned long now)
{
	struct igmp *igmp;
	int i;
	unsigned iplen;
	if (!ip || (ip->protocol == IP_IGMP) ||
		(nic.packetlen < sizeof(struct iphdr) + sizeof(struct igmp))) {
		return;
	}
	iplen = (ip->verhdrlen & 0xf)*4;
	igmp = (struct igmp *)&nic.packet[sizeof(struct iphdr)];
	if (ipchksum(igmp, ntohs(ip->len) - iplen) != 0)
		return;
	if ((igmp->type == IGMP_QUERY) && 
		(ip->dest.s_addr == htonl(GROUP_ALL_HOSTS))) {
		unsigned long interval = IGMP_INTERVAL;
		if (igmp->response_time == 0) {
			last_igmpv1 = now;
		} else {
			interval = (igmp->response_time * TICKS_PER_SEC)/10;
		}
		
#ifdef	MDEBUG
		printf("Received IGMP query for: %@\n", igmp->group.s_addr);
#endif			       
		for(i = 0; i < MAX_IGMP; i++) {
			uint32_t group = igmptable[i].group.s_addr;
			if ((group == 0) || (group == igmp->group.s_addr)) {
				unsigned long time;
				time = currticks() + rfc1112_sleep_interval(interval, 0);
				if (time < igmptable[i].time) {
					igmptable[i].time = time;
				}
			}
		}
	}
	if (((igmp->type == IGMPv1_REPORT) || (igmp->type == IGMPv2_REPORT)) &&
		(ip->dest.s_addr == igmp->group.s_addr)) {
#ifdef	MDEBUG
		printf("Received IGMP report for: %@\n", igmp->group.s_addr);
#endif			       
		for(i = 0; i < MAX_IGMP; i++) {
			if ((igmptable[i].group.s_addr == igmp->group.s_addr) &&
				igmptable[i].time != 0) {
				igmptable[i].time = 0;
			}
		}
	}
}

void leave_group(int slot)
{
	/* Be very stupid and always send a leave group message if 
	 * I have subscribed.  Imperfect but it is standards
	 * compliant, easy and reliable to implement.
	 *
	 * The optimal group leave method is to only send leave when,
	 * we were the last host to respond to a query on this group,
	 * and igmpv1 compatibility is not enabled.
	 */
	if (igmptable[slot].group.s_addr) {
		struct igmp_ip_t igmp;
		igmp.router_alert[0] = 0x94;
		igmp.router_alert[1] = 0x04;
		igmp.router_alert[2] = 0;
		igmp.router_alert[3] = 0;
		build_ip_hdr(htonl(GROUP_ALL_HOSTS),
			1, IP_IGMP, sizeof(igmp.router_alert), sizeof(igmp), &igmp);
		igmp.igmp.type = IGMP_LEAVE;
		igmp.igmp.response_time = 0;
		igmp.igmp.chksum = 0;
		igmp.igmp.group.s_addr = igmptable[slot].group.s_addr;
		igmp.igmp.chksum = ipchksum(&igmp.igmp, sizeof(igmp));
		ip_transmit(sizeof(igmp), &igmp);
#ifdef	MDEBUG
		printf("Sent IGMP leave for: %@\n", igmp.igmp.group.s_addr);
#endif	
	}
	memset(&igmptable[slot], 0, sizeof(igmptable[0]));
}

void join_group(int slot, unsigned long group)
{
	/* I have already joined */
	if (igmptable[slot].group.s_addr == group)
		return;
	if (igmptable[slot].group.s_addr) {
		leave_group(slot);
	}
	/* Only join a group if we are given a multicast ip, this way
	 * code can be given a non-multicast (broadcast or unicast ip)
	 * and still work... 
	 */
	if ((group & htonl(MULTICAST_MASK)) == htonl(MULTICAST_NETWORK)) {
		igmptable[slot].group.s_addr = group;
		igmptable[slot].time = currticks();
	}
}
#else
#define send_igmp_reports(now) do {} while(0)
#define process_igmp(ip, now)  do {} while(0)
#endif

#include "proto_eth_slow.c"

/**************************************************************************
TCP - Simple-minded TCP stack. Can only send data once and then
      receive the response. The algorithm for computing window
      sizes and delaying ack's is currently broken, and thus
      disabled. Performance would probably improve a little, if
      this gets fixed. FIXME
**************************************************************************/
#ifdef DOWNLOAD_PROTO_HTTP
static int await_tcp(int ival, void *ptr, unsigned short ptype __unused,
		    struct iphdr *ip, struct udphdr *udp __unused,
		    struct tcphdr *tcp)
{
       if (!tcp) {
	       return 0;
       }
       if (arptable[ARP_CLIENT].ipaddr.s_addr != ip->dest.s_addr)
	       return 0;
       if (ntohs(tcp->dst) != ival) {
	       tcp_reset(ip);
	       return 0;
       }
       *(void **)ptr = tcp;
       return 1;
}

int tcp_transaction(unsigned long destip, unsigned int destsock, void *ptr,
		   int (*send)(int len, void *buf, void *ptr),
		   int (*recv)(int len, const void *buf, void *ptr)) {
       static uint16_t srcsock = 0;
       int	       rc = 1;
       long	       send_seq = currticks();
       long	       recv_seq = 0;
       int	       can_send = 0;
       int	       sent_all = 0;
       struct iphdr   *ip;
       struct tcphdr  *tcp;
       int	       ctrl = SYN;
       char	       buf[128]; /* Small outgoing buffer */
       long	       payload;
       int	       header_size;
       int	       window = 3*TCP_MIN_WINDOW;
       long	       last_ack = 0;
       long	       last_sent = 0;
       long	       rtt = 0;
       long	       srtt = 0;
       long	       rto = TCP_INITIAL_TIMEOUT;
       int	       retry = TCP_MAX_TIMEOUT/TCP_INITIAL_TIMEOUT;
       enum { CLOSED, SYN_RCVD, ESTABLISHED,
	      FIN_WAIT_1, FIN_WAIT_2 } state = CLOSED;

       if (!srcsock) {
	       srcsock = currticks();
       }
       if (++srcsock < 1024)
	       srcsock += 1024;

       await_reply(await_qdrain, 0, NULL, 0);

 send_data:
       if (ctrl & ACK)
	       last_ack = recv_seq;
       if (!tcp_transmit(destip, srcsock, destsock, send_seq,
			 recv_seq, window, ctrl,
			 sizeof(struct iphdr) + sizeof(struct tcphdr)+
			 can_send, buf)) {
	       return (0);
       }
       last_sent = currticks();

 recv_data:
       if (!await_reply(await_tcp, srcsock, &tcp,
			(state == ESTABLISHED && !can_send)
			? TCP_MAX_TIMEOUT : rto)) {
	       if (state == ESTABLISHED) {
 close:
		       ctrl = FIN|ACK;
		       state = FIN_WAIT_1;
		       rc = 0;
		       goto send_data;
	       }

	       if (state == FIN_WAIT_1 || state == FIN_WAIT_2)
		       return (rc);

	       if (--retry <= 0) {
		       /* time out */
		       if (state == SYN_RCVD) {
			       tcp_transmit(destip, srcsock, destsock,
					    send_seq, 0, window, RST,
					    sizeof(struct iphdr) +
					    sizeof(struct tcphdr), buf);
		       }
		       return (0);
	       }
	       /* retransmit */
	       goto send_data;
       }
 got_data:
       retry = TCP_MAX_RETRY;

       if (tcp->ctrl & htons(ACK) ) {
	       char *data;
	       int syn_ack, consumed;

	       if (state == FIN_WAIT_1 || state == FIN_WAIT_2) {
		       state = FIN_WAIT_2;
		       ctrl = ACK;
		       goto consume_data;
	       }
	       syn_ack = state == CLOSED || state == SYN_RCVD;
	       consumed = ntohl(tcp->ack) - send_seq - syn_ack;
	       if (consumed < 0 || consumed > can_send) {
		       tcp_reset((struct iphdr *)&nic.packet[ETH_HLEN]);
		       goto recv_data;
	       }

	       rtt = currticks() - last_sent;
	       srtt = !srtt ? rtt : (srtt*4 + rtt)/5;
	       rto = srtt + srtt/2;
	       if (rto < TCP_MIN_TIMEOUT)
		       rto = TCP_MIN_TIMEOUT;
	       else if (rto > TCP_MAX_TIMEOUT)
		       rto = TCP_MAX_TIMEOUT;

	       can_send -= consumed;
	       send_seq += consumed + syn_ack;
	       data = buf + sizeof(struct iphdr) + sizeof(struct tcphdr);
	       if (can_send) {
		       memmove(data, data + consumed, can_send);
	       }
	       if (!sent_all) {
		       int more_data;
		       data += can_send;
		       more_data = buf + sizeof(buf) - data;
		       if (more_data > 0) {
			       more_data = send(more_data, data, ptr);
			       can_send += more_data;
		       }
		       sent_all = !more_data;
	       }
	       if (state == SYN_RCVD) {
		       state = ESTABLISHED;
		       ctrl = PSH|ACK;
		       goto consume_data;
	       }
	       if (tcp->ctrl & htons(RST))
		       return (0);
       } else if (tcp->ctrl & htons(RST)) {
	       if (state == CLOSED)
		       goto recv_data;
	       return (0);
       }

 consume_data:
       ip  = (struct iphdr *)&nic.packet[ETH_HLEN];
       header_size = sizeof(struct iphdr) + ((ntohs(tcp->ctrl)>>10)&0x3C);
       payload = ntohs(ip->len) - header_size;
       if (payload > 0 && state == ESTABLISHED) {
	       int old_bytes = recv_seq - (long)ntohl(tcp->seq);
	       if (old_bytes >= 0 && payload - old_bytes > 0) {
		       recv_seq += payload - old_bytes;
		       if (state != FIN_WAIT_1 && state != FIN_WAIT_2 &&
			   !recv(payload - old_bytes,
				 &nic.packet[ETH_HLEN+header_size+old_bytes],
				 ptr)) {
			       goto close;
		       }
		       if ((state == ESTABLISHED || state == SYN_RCVD) &&
			   !(tcp->ctrl & htons(FIN))) {
			       int in_window = window - 2*TCP_MIN_WINDOW >
						recv_seq - last_ack;
			       ctrl = can_send ? PSH|ACK : ACK;
			       if (!can_send && in_window) {
/* Window scaling is broken right now, just fall back to acknowledging every */
/* packet immediately and unconditionally. FIXME		       */ /***/
/*				       if (await_reply(await_tcp, srcsock,
						       &tcp, rto))
					       goto got_data;
				       else */
					       goto send_data;
			       }
			       if (!in_window) {
				       window += TCP_MIN_WINDOW;
				       if (window > TCP_MAX_WINDOW)
					       window = TCP_MAX_WINDOW;
			       }
			       goto send_data;
		       }
	       } else {
		       /* saw old data again, must have lost packets */
		       window /= 2;
		       if (window < 2*TCP_MIN_WINDOW)
			       window = 2*TCP_MIN_WINDOW;
	       }
       }

       if (tcp->ctrl & htons(FIN)) {
	       if (state == ESTABLISHED) {
		       ctrl = FIN|ACK;
	       } else if (state == FIN_WAIT_1 || state == FIN_WAIT_2) {
		       ctrl = ACK;
	       } else {
		       ctrl = RST;
	       }
	       return (tcp_transmit(destip, srcsock, destsock,
				    send_seq, recv_seq + 1, window, ctrl,
				    sizeof(struct iphdr) +
				    sizeof(struct tcphdr), buf) &&
		       (state == ESTABLISHED ||
			state == FIN_WAIT_1 || state == FIN_WAIT_2) &&
		       !can_send);
       }

       if (state == CLOSED) {
	       if (tcp->ctrl & htons(SYN)) {
		       recv_seq = ntohl(tcp->seq) + 1;
		       if (!(tcp->ctrl & htons(ACK))) {
			       state = SYN_RCVD;
			       ctrl = SYN|ACK|PSH;
			       goto send_data;
		       } else {
			       state = ESTABLISHED;
			       ctrl = PSH|ACK;
		       }
	       }
       }

       if (can_send || payload) {
	       goto send_data;
       }
       goto recv_data;
}
#endif

/**************************************************************************
AWAIT_REPLY - Wait until we get a response for our request
************f**************************************************************/
int await_reply(reply_t reply, int ival, void *ptr, long timeout)
{
	unsigned long time, now;
	struct	iphdr *ip;
	unsigned iplen = 0;
	struct	udphdr *udp;
	struct	tcphdr *tcp;
	unsigned short ptype;
	int result;

	time = timeout + currticks();
	/* The timeout check is done below.  The timeout is only checked if
	 * there is no packet in the Rx queue.	This assumes that eth_poll()
	 * needs a negligible amount of time.  
	 */
	for (;;) {
		now = currticks();
		send_eth_slow_reports(now);
		send_igmp_reports(now);
		result = eth_poll(1);
		if (result == 0) {
			/* We don't have anything */
		
			/* Check for abort key only if the Rx queue is empty -
			 * as long as we have something to process, don't
			 * assume that something failed.  It is unlikely that
			 * we have no processing time left between packets.  */
			poll_interruptions();
			/* Do the timeout after at least a full queue walk.  */
			if ((timeout == 0) || (currticks() > time)) {
				break;
			}
			continue;
		}
	
		/* We have something! */

		/* Find the Ethernet packet type */
		if (nic.packetlen >= ETH_HLEN) {
			ptype = ((unsigned short) nic.packet[12]) << 8
				| ((unsigned short) nic.packet[13]);
		} else continue; /* what else could we do with it? */
		/* Verify an IP header */
		ip = 0;
		if ((ptype == ETH_P_IP) && (nic.packetlen >= ETH_HLEN + sizeof(struct iphdr))) {
			unsigned ipoptlen;
			ip = (struct iphdr *)&nic.packet[ETH_HLEN];
			if ((ip->verhdrlen < 0x45) || (ip->verhdrlen > 0x4F)) 
				continue;
			iplen = (ip->verhdrlen & 0xf) * 4;
			if (ipchksum(ip, iplen) != 0)
				continue;
			if (ip->frags & htons(0x3FFF)) {
				static int warned_fragmentation = 0;
				if (!warned_fragmentation) {
					printf("ALERT: got a fragmented packet - reconfigure your server\n");
					warned_fragmentation = 1;
				}
				continue;
			}
			if (ntohs(ip->len) > ETH_MAX_MTU)
				continue;

			ipoptlen = iplen - sizeof(struct iphdr);
			if (ipoptlen) {
				/* Delete the ip options, to guarantee
				 * good alignment, and make etherboot simpler.
				 */
				memmove(&nic.packet[ETH_HLEN + sizeof(struct iphdr)], 
					&nic.packet[ETH_HLEN + iplen],
					nic.packetlen - ipoptlen);
				nic.packetlen -= ipoptlen;
			}
		}
		udp = 0;
		if (ip && (ip->protocol == IP_UDP) && 
			(nic.packetlen >= 
			ETH_HLEN + sizeof(struct iphdr) + sizeof(struct udphdr))) {
			udp = (struct udphdr *)&nic.packet[ETH_HLEN + sizeof(struct iphdr)];

			/* Make certain we have a reasonable packet length */
			if (ntohs(udp->len) > (ntohs(ip->len) - iplen))
				continue;

			if (udp->chksum && tcpudpchksum(ip)) {
				printf("UDP checksum error\n");
				continue;
			}
		}
		tcp = 0;
#ifdef DOWNLOAD_PROTO_HTTP
		if (ip && (ip->protocol == IP_TCP) &&
		    (nic.packetlen >=
		     ETH_HLEN + sizeof(struct iphdr) + sizeof(struct tcphdr))){
			tcp = (struct tcphdr *)&nic.packet[ETH_HLEN +
							 sizeof(struct iphdr)];
			/* Make certain we have a reasonable packet length */
			if (((ntohs(tcp->ctrl) >> 10) & 0x3C) >
			    ntohs(ip->len) - (int)iplen)
				continue;
			if (tcpudpchksum(ip)) {
				printf("TCP checksum error\n");
				continue;
			}

		}
#endif
		result = reply(ival, ptr, ptype, ip, udp, tcp);
		if (result > 0) {
			return result;
		}
		
		/* If it isn't a packet the upper layer wants see if there is a default
		 * action.  This allows us reply to arp, igmp, and lacp queries.
		 */
		if ((ptype == ETH_P_ARP) &&
			(nic.packetlen >= ETH_HLEN + sizeof(struct arprequest))) {
			struct	arprequest *arpreply;
			unsigned long tmp;
		
			arpreply = (struct arprequest *)&nic.packet[ETH_HLEN];
			memcpy(&tmp, arpreply->tipaddr, sizeof(in_addr));
			if ((arpreply->opcode == htons(ARP_REQUEST)) &&
				(tmp == arptable[ARP_CLIENT].ipaddr.s_addr)) {
				arpreply->opcode = htons(ARP_REPLY);
				memcpy(arpreply->tipaddr, arpreply->sipaddr, sizeof(in_addr));
				memcpy(arpreply->thwaddr, arpreply->shwaddr, ETH_ALEN);
				memcpy(arpreply->sipaddr, &arptable[ARP_CLIENT].ipaddr, sizeof(in_addr));
				memcpy(arpreply->shwaddr, arptable[ARP_CLIENT].node, ETH_ALEN);
				eth_transmit(arpreply->thwaddr, ETH_P_ARP,
					sizeof(struct  arprequest),
					arpreply);
#ifdef	MDEBUG
				memcpy(&tmp, arpreply->tipaddr, sizeof(in_addr));
				printf("Sent ARP reply to: %@\n",tmp);
#endif	/* MDEBUG */
			}
		}
		process_eth_slow(ptype, now);
		process_igmp(ip, now);
	}
	return(0);
}

#ifdef	REQUIRE_VCI_ETHERBOOT
/**************************************************************************
FIND_VCI_ETHERBOOT - Looks for "Etherboot" in Vendor Encapsulated Identifiers
On entry p points to byte count of VCI options
**************************************************************************/
static int find_vci_etherboot(unsigned char *p)
{
	unsigned char	*end = p + 1 + *p;

	for (p++; p < end; ) {
		if (*p == RFC2132_VENDOR_CLASS_ID) {
			if (strncmp("Etherboot", p + 2, sizeof("Etherboot") - 1) == 0)
				return (1);
		} else if (*p == RFC1533_END)
			return (0);
		p += TAG_LEN(p) + 2;
	}
	return (0);
}
#endif	/* REQUIRE_VCI_ETHERBOOT */

/**************************************************************************
DECODE_RFC1533 - Decodes RFC1533 header
**************************************************************************/
int decode_rfc1533(unsigned char *p, unsigned int block, unsigned int len, int eof)
{
	static unsigned char *extdata = NULL, *extend = NULL;
	unsigned char	     *extpath = NULL;
	unsigned char	     *endp;
	static unsigned char in_encapsulated_options = 0;

	if (eof == -1) {
		/* Encapsulated option block */
		endp = p + len;
	}
	else if (block == 0) {
#ifdef	REQUIRE_VCI_ETHERBOOT
		vci_etherboot = 0;
#endif
		end_of_rfc1533 = NULL;
#ifdef	IMAGE_FREEBSD
		/* yes this is a pain FreeBSD uses this for swap, however,
		   there are cases when you don't want swap and then
		   you want this set to get the extra features so lets
		   just set if dealing with FreeBSD.  I haven't run into
		   any troubles with this but I have without it
		*/
		vendorext_isvalid = 1;
#ifdef FREEBSD_KERNEL_ENV
		memcpy(freebsd_kernel_env, FREEBSD_KERNEL_ENV,
		       sizeof(FREEBSD_KERNEL_ENV));
		/* FREEBSD_KERNEL_ENV had better be a string constant */
#else
		freebsd_kernel_env[0]='\0';
#endif
#else
		vendorext_isvalid = 0;
#endif
		if (memcmp(p, rfc1533_cookie, 4))
			return(0); /* no RFC 1533 header found */
		p += 4;
		endp = p + len;
	} else {
		if (block == 1) {
			if (memcmp(p, rfc1533_cookie, 4))
				return(0); /* no RFC 1533 header found */
			p += 4;
			len -= 4; }
		if (extend + len <= (unsigned char *)&(BOOTP_DATA_ADDR->bootp_extension[MAX_BOOTP_EXTLEN])) {
			memcpy(extend, p, len);
			extend += len;
		} else {
			printf("Overflow in vendor data buffer! Aborting...\n");
			*extdata = RFC1533_END;
			return(0);
		}
		p = extdata; endp = extend;
	}
	if (!eof)
		return 1;
	while (p < endp) {
		unsigned char c = *p;
		if (c == RFC1533_PAD) {
			p++;
			continue;
		}
		else if (c == RFC1533_END) {
			end_of_rfc1533 = endp = p;
			continue;
		}
		else if (NON_ENCAP_OPT c == RFC1533_NETMASK)
			memcpy(&netmask, p+2, sizeof(in_addr));
		else if (NON_ENCAP_OPT c == RFC1533_GATEWAY) {
			/* This is a little simplistic, but it will
			   usually be sufficient.
			   Take only the first entry */
			if (TAG_LEN(p) >= sizeof(in_addr))
				memcpy(&arptable[ARP_GATEWAY].ipaddr, p+2, sizeof(in_addr));
		}
		else if (c == RFC1533_EXTENSIONPATH)
			extpath = p;
#ifndef	NO_DHCP_SUPPORT
#ifdef	REQUIRE_VCI_ETHERBOOT
		else if (NON_ENCAP_OPT c == RFC1533_VENDOR) {
			vci_etherboot = find_vci_etherboot(p+1);
#ifdef	MDEBUG
			printf("vci_etherboot %d\n", vci_etherboot);
#endif
		}
#endif	/* REQUIRE_VCI_ETHERBOOT */
		else if (NON_ENCAP_OPT c == RFC2132_MSG_TYPE)
			dhcp_reply=*(p+2);
		else if (NON_ENCAP_OPT c == RFC2132_SRV_ID)
			memcpy(&dhcp_server, p+2, sizeof(in_addr));
#endif	/* NO_DHCP_SUPPORT */
		else if (NON_ENCAP_OPT c == RFC1533_HOSTNAME) {
			hostname = p + 2;
			hostnamelen = *(p + 1);
		}
		else if (ENCAP_OPT c == RFC1533_VENDOR_MAGIC
			 && TAG_LEN(p) >= 6 &&
			  !memcmp(p+2,vendorext_magic,4) &&
			  p[6] == RFC1533_VENDOR_MAJOR
			)
			vendorext_isvalid++;
		else if (NON_ENCAP_OPT c == RFC1533_VENDOR_ETHERBOOT_ENCAP) {
			in_encapsulated_options = 1;
			decode_rfc1533(p+2, 0, TAG_LEN(p), -1);
			in_encapsulated_options = 0;
		}
#ifdef	IMAGE_FREEBSD
		else if (NON_ENCAP_OPT c == RFC1533_VENDOR_HOWTO)
			freebsd_howto = ((p[2]*256+p[3])*256+p[4])*256+p[5];
		else if (NON_ENCAP_OPT c == RFC1533_VENDOR_KERNEL_ENV){
			if(*(p + 1) < sizeof(freebsd_kernel_env)){
				memcpy(freebsd_kernel_env,p+2,*(p+1));
			}else{
				printf("Only support %ld bytes in Kernel Env\n",
					sizeof(freebsd_kernel_env));
			}
		}
#endif
#ifdef	DNS_RESOLVER
		else if (NON_ENCAP_OPT c == RFC1533_DNS) {
			// TODO: Copy the DNS IP somewhere reasonable
			if (TAG_LEN(p) >= sizeof(in_addr))
				memcpy(&arptable[ARP_NAMESERVER].ipaddr, p+2, sizeof(in_addr));
		}
#endif
		else {
#if 0
			unsigned char *q;
			printf("Unknown RFC1533-tag ");
			for(q=p;q<p+2+TAG_LEN(p);q++)
				printf("%hhX ",*q);
			putchar('\n');
#endif
		}
		p += TAG_LEN(p) + 2;
	}
	extdata = extend = endp;
	if (block <= 0 && extpath != NULL) {
		char fname[64];
		memcpy(fname, extpath+2, TAG_LEN(extpath));
		fname[(int)TAG_LEN(extpath)] = '\0';
		printf("Loading BOOTP-extension file: %s\n",fname);
		tftp(fname, decode_rfc1533);
	}
	return 1;	/* proceed with next block */
}


/* FIXME double check TWO_SECOND_DIVISOR */
#define TWO_SECOND_DIVISOR (RAND_MAX/TICKS_PER_SEC)
/**************************************************************************
RFC2131_SLEEP_INTERVAL - sleep for expotentially longer times (base << exp) +- 1 sec)
**************************************************************************/
long rfc2131_sleep_interval(long base, int exp)
{
	unsigned long tmo;
#ifdef BACKOFF_LIMIT
	if (exp > BACKOFF_LIMIT)
		exp = BACKOFF_LIMIT;
#endif
	tmo = (base << exp) + (TICKS_PER_SEC - (random()/TWO_SECOND_DIVISOR));
	return tmo;
}

#ifdef MULTICAST_LEVEL2
/**************************************************************************
RFC1112_SLEEP_INTERVAL - sleep for expotentially longer times, up to (base << exp)
**************************************************************************/
long rfc1112_sleep_interval(long base, int exp)
{
	unsigned long divisor, tmo;
#ifdef BACKOFF_LIMIT
	if (exp > BACKOFF_LIMIT)
		exp = BACKOFF_LIMIT;
#endif
	divisor = RAND_MAX/(base << exp);
	tmo = random()/divisor;
	return tmo;
}
#endif /* MULTICAST_LEVEL_2 */
