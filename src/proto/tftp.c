#include "etherboot.h"
#include "in.h"
#include "nic.h"
#include "proto.h"
#include "tftp.h"

/* Utility function for tftp_block() */
static int await_tftp ( int ival, void *ptr __unused,
			unsigned short ptype __unused, struct iphdr *ip,
			struct udphdr *udp, struct tcphdr *tcp __unused ) {
	if ( ! udp ) {
		return 0;
	}
	if ( arptable[ARP_CLIENT].ipaddr.s_addr != ip->dest.s_addr )
		return 0;
	if ( ntohs ( udp->dest ) != ival )
		return 0;
	return 1;
}

/*
 * Download a single block via TFTP.  This function is non-static so
 * that pxe_export.c can call it.
 *
 */
int tftp_block ( struct tftpreq_info_t *request,
		 struct tftpblk_info_t *block ) {
	static struct sockaddr_in server;
	static unsigned short lport = 2000; /* local port */
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
		server = *(request->server);
		if ( !udp_transmit(server.sin_addr.s_addr, lport,
				   server.sin_port, xmitlen, &xmit) )
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
			if ( !udp_transmit(server.sin_addr.s_addr, lport,
					   server.sin_port, xmitlen, &xmit) )
				return (0);
			continue; /* Back to waiting for packet */
		}
		/* Packet has been received */
		rcvd = (struct tftp_t *)&nic.packet[ETH_HLEN];
		recvlen = ntohs(rcvd->udp.len) - sizeof(struct udphdr)
			- sizeof(rcvd->opcode);
		server.sin_port = ntohs(rcvd->udp.src);
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
			if ( blksize < TFTP_DEFAULTSIZE_PACKET ||
			     blksize > request->blksize ) {
				/* Incorrect blksize - error and abort */
				xmit.opcode = htons(TFTP_ERROR);
				xmit.u.err.errcode = 8;
				xmitlen = (void*)&xmit.u.err.errmsg
					- (void*)&xmit
					+ sprintf((char*)xmit.u.err.errmsg,
						  "RFC1782 error")
					+ 1;
				udp_transmit(server.sin_addr.s_addr, lport,
					     server.sin_port, xmitlen, &xmit);
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
		udp_transmit ( server.sin_addr.s_addr, lport, server.sin_port,
			       xmitlen, &xmit );
	}
	return ( block->data ? 1 : 0 );
}

/*
 * Download a file via TFTP
 *
 */
int tftp ( char *url __unused, struct sockaddr_in *server, char *file,
	   struct buffer *buffer ) {
	struct tftpreq_info_t request_data = {
		.server = server,
		.name = file,
		.blksize = TFTP_MAX_PACKET,
	};
	struct tftpreq_info_t *request = &request_data;
	struct tftpblk_info_t block;
	off_t offset = 0;

	do {
		if ( ! tftp_block ( request, &block ) )
			return 0;
		if ( ! fill_buffer ( buffer, block.data, offset, block.len ) )
			return 0;
		twiddle();
		offset += block.len;
		request = NULL; /* Send request only once */
	} while ( ! block.eof );

	return 1;
}

struct protocol tftp_protocol __default_protocol = {
	.name = "tftp",
	.default_port = TFTP_PORT,
	.load = tftp,
};
