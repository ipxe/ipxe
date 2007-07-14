    /*********************************************************************\
    * Copyright (c) 2005 by Radim Kolar (hsn-sendmail.cz)                 *
    *                                                                     *
    * You may copy or modify this file in any manner you wish, provided   *
    * that this notice is always included, and that you hold the author   *
    * harmless for any loss or damage resulting from the installation or  *
    * use of this software.                                               *
    *                                                                     *
    * This file provides support for FSP v2 protocol written from scratch *
    * by Radim Kolar,   FSP project leader.                               *
    *                                                                     *
    * ABOUT FSP                                                           *
    * FSP is a lightweight file transfer protocol and is being used for   *
    * booting, Internet firmware updates, embedded devices and in         *
    * wireless applications. FSP is very easy to implement; contact Radim *
    * Kolar if you need hand optimized assembler FSP stacks for various   *
    * microcontrollers, CPUs or consultations.                            *
    * http://fsp.sourceforge.net/                                         *
    *                                                                     *
    * REVISION HISTORY                                                    *
    * 1.0 2005-03-17 rkolar   Initial coding                              *
    * 1.1 2005-03-24 rkolar   We really need to send CC_BYE to the server *
    *                         at end of transfer, because next stage boot *
    *                         loader is unable to contact FSP server      *
    *                         until session timeouts.                     *
    * 1.2 2005-03-26 rkolar   We need to query filesize in advance,       *
    *                         because NBI loader do not reads file until  *
    *                         eof is reached.
    * REMARKS                                                             *
    * there is no support for selecting port number of fsp server, maybe  *
    *   we should parse fsp:// URLs in boot image filename.               *
    * this implementation has filename limit 255 chars.                   *
    \*********************************************************************/

#ifdef DOWNLOAD_PROTO_FSP

#define FSP_PORT 21

/* FSP commands */
#define CC_GET_FILE	0x42
#define CC_BYE		0x4A
#define CC_ERR		0x40
#define CC_STAT		0x4D

/* etherboot limits */
#define FSP_MAXFILENAME 255

struct fsp_info {
	in_addr server_ip;
	uint16_t server_port;
	uint16_t local_port;
	const char *filename;
	int (*fnc)(unsigned char *, unsigned int, unsigned int, int);
};

struct fsp_header {
    	uint8_t cmd;
	uint8_t sum;
	uint16_t key;
	uint16_t seq;
	uint16_t len;
	uint32_t pos;
} PACKED;

#define FSP_MAXPAYLOAD (ETH_MAX_MTU - \
  (sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(struct fsp_header)))

static struct fsp_request {
	struct iphdr ip;
	struct udphdr udp;
	struct fsp_header fsp;
	unsigned char data[FSP_MAXFILENAME + 1 + 2];
} request;

struct fsp_reply {
	struct iphdr ip;
	struct udphdr udp;
	struct fsp_header fsp;
	unsigned char data[FSP_MAXPAYLOAD];
} PACKED;


static int await_fsp(int ival, void *ptr, unsigned short ptype __unused,
                      struct iphdr *ip, struct udphdr *udp)
{
	if(!udp)
	    return 0;
	if (ip->dest.s_addr != arptable[ARP_CLIENT].ipaddr.s_addr) 
	    return 0;
        if (ntohs(udp->dest) != ival)
            return 0;
	if (ntohs(udp->len) < 12+sizeof(struct udphdr))
	    return 0;
	return 1;
}

static int proto_fsp(struct fsp_info *info)
{
    uint32_t filepos;
    uint32_t filelength=0;
    int i,retry;
    uint16_t reqlen;
    struct fsp_reply *reply;
    int block=1;
    
    /* prepare FSP request packet */
    filepos=0;
    i=strlen(info->filename);
    if(i>FSP_MAXFILENAME)
    {
	printf("Boot filename is too long.\n");
	return 0;
    }
    strcpy(request.data,info->filename);
    *(uint16_t *)(request.data+i+1)=htons(FSP_MAXPAYLOAD);
    request.fsp.len=htons(i+1);
    reqlen=i+3+12;

    rx_qdrain();
    retry=0;

    /* main loop */
    for(;;) {
	int  sum;
	long timeout;

        /* query filelength if not known */
	if(filelength == 0)
	    request.fsp.cmd=CC_STAT;
		
	/* prepare request packet */
	request.fsp.pos=htonl(filepos);
	request.fsp.seq=random();
	request.fsp.sum=0;
	for(i=0,sum=reqlen;i<reqlen;i++)
	{
	    sum += ((uint8_t *)&request.fsp)[i];
        }
	request.fsp.sum= sum + (sum >> 8);
	/* send request */
        if (!udp_transmit(info->server_ip.s_addr, info->local_port,
	                 info->server_port, sizeof(request.ip) +
			 sizeof(request.udp) + reqlen, &request))
	                    return (0);
	/* wait for retry */		    
#ifdef  CONGESTED
        timeout =
            rfc2131_sleep_interval(filepos ? TFTP_REXMT : TIMEOUT, retry);
#else
	timeout = rfc2131_sleep_interval(TIMEOUT, retry);
#endif
	retry++;
        if (!await_reply(await_fsp, info->local_port, NULL, timeout))
	    continue;
	reply=(struct fsp_reply *) &nic.packet[ETH_HLEN];    
	/* check received packet */
	if (reply->fsp.seq != request.fsp.seq)
	    continue;
	reply->udp.len=ntohs(reply->udp.len)-sizeof(struct udphdr);
	if(reply->udp.len < ntohs(reply->fsp.len) + 12 )
	    continue;
        sum=-reply->fsp.sum;
	for(i=0;i<reply->udp.len;i++)
	{
	    sum += ((uint8_t *)&(reply->fsp))[i];
        }
        sum = (sum + (sum >> 8)) & 0xff;
	if(sum != reply->fsp.sum)
	{
	    printf("FSP checksum failed. computed %d, but packet has %d.\n",sum,reply->fsp.sum);
	    continue;
	}
	if(reply->fsp.cmd == CC_ERR)
	{
	    printf("\nFSP error: %s",info->filename);
	    if(reply->fsp.len)
	        printf(" [%s]",reply->data);
	    printf("\n");
	    return 0;
	}
	if(reply->fsp.cmd == CC_BYE && filelength == 1)
	{
	    info->fnc(request.data,block,1,1);
	    return 1;
	}
	if(reply->fsp.cmd == CC_STAT)
	{
	    if(reply->data[8] == 0)
	    {
		/* file not found, etc. */
		filelength=0xffffffff;
	    } else
	    {
		filelength= ntohl(*((uint32_t *)&reply->data[4]));
	    }
	    request.fsp.cmd = CC_GET_FILE;
	    request.fsp.key = reply->fsp.key;
	    retry=0;
	    continue;
	}

	if(reply->fsp.cmd == CC_GET_FILE)
	{
	    if(ntohl(reply->fsp.pos) != filepos)
		continue;
	    request.fsp.key = reply->fsp.key;
	    retry=0;
	    i=ntohs(reply->fsp.len);
	    if(i == 1)
	    {
		request.fsp.cmd=CC_BYE;
		request.data[0]=reply->data[0];
		continue;
	    }
	    /* let last byte alone */
            if(i >= filelength)
		i = filelength - 1;
	    if(!info->fnc(reply->data,block++,i,0))
		return 0;
	    filepos += i;
	    filelength -= i;
	}
    }

    return 0;
}

int url_fsp(const char *name, int (*fnc)(unsigned char *, unsigned int, unsigned int, int))
{
	struct fsp_info info;
	/* Set the defaults */
	info.server_ip.s_addr    = arptable[ARP_SERVER].ipaddr.s_addr;
	info.server_port         = FSP_PORT;
	info.local_port		 = 1024 + random() & 0xfbff;
	info.fnc                 = fnc;
	
	/* Now parse the url */
	/* printf("fsp-URI: [%s]\n", name); */
        /* quick hack for now */
	info.filename=name;
	return proto_fsp(&info);
}
#endif
