#include "tftp.h"
#include "tcp.h" /* for struct tcphdr */
#include "errno.h"
#include "etherboot.h"

/** @file
 *
 * TFTP core functions
 *
 * This file provides functions that are common to the TFTP (rfc1350),
 * TFTM (rfc2090) and MTFTP (PXE) protocols.
 *
 */

/**
 * Wait for a TFTP packet
 *
 * @v ptr			Pointer to a struct tftp_state
 * @v ip			IP header
 * @v udp			UDP header
 * @ret True			This is our TFTP packet
 * @ret False			This is not one of our TFTP packets
 *
 * Wait for a TFTP packet that is part of the current connection
 * (i.e. comes from the TFTP server, has the correct destination port,
 * and is addressed either to our IP address or to our multicast
 * listening address).
 */
static int await_tftp ( int ival __unused, void *ptr,
			unsigned short ptype __unused, struct iphdr *ip,
			struct udphdr *udp, struct tcphdr *tcp __unused ) {
	struct tftp_state *state = ptr;

	/* Must have valid UDP (and, therefore, also IP) headers */
	if ( ! udp ) {
		return 0;
	}
	/* Packet must come from the TFTP server */
	if ( ip->src.s_addr != state->server.sin_addr.s_addr )
		return 0;
	/* Packet must be addressed to the correct UDP port */
	if ( ntohs ( udp->dest ) != state->client.sin_port )
		return 0;
	/* Packet must be addressed to us, or to our multicast
	 * listening address (if we have one).
	 */
	if ( ! ( ( ip->dest.s_addr == arptable[ARP_CLIENT].ipaddr.s_addr ) ||
		 ( ( state->client.sin_addr.s_addr ) && 
		   ( ip->dest.s_addr == state->client.sin_addr.s_addr ) ) ) )
		return 0;
	return 1;
}


/**
 * Issue a TFTP open request (RRQ)
 *
 * @v filename				File name
 * @v state				TFTP transfer state
 * @v tftp_state::server::sin_addr	TFTP server IP address
 * @v tftp_state::server::sin_port	TFTP server UDP port, or 0
 * @v tftp_state::client::sin_addr	Client multicast IP address, or 0.0.0.0
 * @v tftp_state::client::sin_port	Client UDP port, or 0
 * @v tftp_state::blksize		Requested blksize, or 0
 * @ret True				Received a non-error response
 * @ret False				Received error response / no response
 * @ret tftp_state::client::sin_port	Client UDP port
 * @ret tftp_state::client::blksize	Always #TFTP_DEFAULT_BLKSIZE
 * @ret *tftp				The server's response, if any
 *
 * Send a TFTP/TFTM/MTFTP RRQ (read request) to a TFTP server, and
 * return the server's reply (which may be an OACK, DATA or ERROR
 * packet).  The server's reply will not be acknowledged, or processed
 * in any way.
 *
 * If tftp_state::server::sin_port is 0, the standard tftp server port
 * (#TFTP_PORT) will be used.
 *
 * If tftp_state::client::sin_addr is not 0.0.0.0, it will be used as
 * a multicast listening address for replies from the TFTP server.
 *
 * If tftp_state::client::sin_port is 0, the standard mechanism of
 * using a new, unique port number for each TFTP request will be used.
 * 
 * For the various different types of TFTP server, you should treat
 * tftp_state::client as follows:
 *
 *   - Standard TFTP server: set tftp_state::client::sin_addr to
 *     0.0.0.0 and tftp_state::client::sin_port to 0.  tftp_open()
 *     will set tftp_state::client::sin_port to the assigned local UDP
 *     port.
 *
 *   - TFTM server: set tftp_state::client::sin_addr to 0.0.0.0 and
 *     tftp_state::client::sin_port to 0.  tftp_open() will set
 *     tftp_state::client::sin_port to the assigned local UDP port.
 *     (Your call to tftp_process_opts() will then overwrite both
 *     tftp_state::client::sin_addr and tftp_state::client::sin_port
 *     with the values return in the OACK packet.)
 *
 *   - MTFTP server: set tftp_state::client::sin_addr to the client
 *     multicast address and tftp_state::client::sin_port to the
 *     client multicast port (both of which must be previously known,
 *     e.g. provided by a DHCP server).  tftp_open() will not alter
 *     these values.
 *
 * If tftp_state::blksize is 0, the maximum blocksize
 * (#TFTP_MAX_BLKSIZE) will be requested.
 *
 * On exit, tftp_state::blksize will always contain
 * #TFTP_DEFAULT_BLKSIZE, since this is the blocksize value that must
 * be assumed until the OACK packet is processed (by a subsequent call
 * to tftp_process_opts()).
 *
 * The options "blksize", "tsize" and "multicast" will always be
 * appended to a TFTP open request.  Servers that do not understand
 * any of these options should simply ignore them.
 *
 * tftp_open() will not automatically join or leave multicast groups;
 * the caller is responsible for calling join_group() and
 * leave_group() at appropriate times.
 *
 */
int tftp_open ( const char *filename, struct tftp_state *state,
		union tftp_any **tftp ) {
	static unsigned short lport = 2000; /* local port */
	int fixed_lport;
	struct tftp_rrq rrq;
	unsigned int rrqlen;
	int retry;

	/* Flush receive queue */
	rx_qdrain();

	/* Default to blksize of TFTP_MAX_BLKSIZE if none specified */
	if ( ! state->blksize )
		state->blksize = TFTP_MAX_BLKSIZE;

	/* Use default TFTP server port if none specified */
	if ( ! state->server.sin_port )
		state->server.sin_port = TFTP_PORT;

	/* Determine whether or not to use lport */
	fixed_lport = state->server.sin_port;

	/* Set up RRQ */
	rrq.opcode = htons ( TFTP_RRQ );
	rrqlen = ( offsetof ( typeof ( rrq ), data ) +
		    sprintf ( rrq.data,
			      "%s%coctet%cblksize%c%d%ctsize%c0%cmulticast%c",
			      filename, 0, 0, 0, state->blksize, 0, 0, 0, 0 )
		    + 1 );

	/* Set negotiated blksize to default value */
	state->blksize = TFTP_DEFAULT_BLKSIZE;
	
	/* Nullify received packet pointer */
	*tftp = NULL;

	/* Transmit RRQ until we get a response */
	for ( retry = 0 ; retry < MAX_TFTP_RETRIES ; retry++ ) {
		long timeout = rfc2131_sleep_interval ( TIMEOUT, retry );

		/* Set client UDP port, if not already fixed */
		if ( ! fixed_lport )
			state->client.sin_port = ++lport;
		
		/* Send the RRQ */
		if ( ! udp_transmit ( state->server.sin_addr.s_addr,
				      state->client.sin_port,
				      state->server.sin_port,
				      rrqlen, &rrq ) )
			return 0;
		
		/* Wait for response */
		if ( await_reply ( await_tftp, 0, state, timeout ) ) {
			*tftp = ( union tftp_any * ) &nic.packet[ETH_HLEN];
			return 1;
		}
	}

	errno = PXENV_STATUS_TFTP_OPEN_TIMEOUT;
	return 0;
}

/**
 * Process a TFTP OACK packet
 *
 * @v oack				The TFTP OACK packet
 * @v state				TFTP transfer state
 * @ret True				Options were processed successfully
 * @ret False				Options were not processed successfully
 * @ret tftp_state::blksize		Negotiated blksize
 * @ret tftp_state::tsize		File size (if known), or 0
 * @ret tftp_state::client::sin_addr	Client multicast IP address, or 0.0.0.0
 * @ret tftp_state::client::sin_port	Client UDP port
 * @ret tftp_state::master		Client is master
 * @err EINVAL				An invalid option value was encountered
 *
 * Process the options returned by the TFTP server in an rfc2347 OACK
 * packet.  The options "blksize" (rfc2348), "tsize" (rfc2349) and
 * "multicast" (rfc2090) are recognised and processed; any other
 * options are silently ignored.
 *
 * Where an option is not present in the OACK packet, the
 * corresponding field(s) in #state will be left unaltered.
 *
 * Calling tftp_process_opts() does not send an acknowledgement for
 * the OACK packet; this is the responsibility of the caller.
 *
 * @note If the "blksize" option is not present, tftp_state::blksize
 * will @b not be implicitly set to #TFTP_DEFAULT_BLKSIZE.  However,
 * since tftp_open() always sets tftp_state::blksize to
 * #TFTP_DEFAULT_BLKSIZE before returning, you probably don't need to
 * worry about this.
 */
int tftp_process_opts ( struct tftp_oack *oack, struct tftp_state *state ) {
	const char *p;
	const char *end;

	/* End of options */
	end = ( ( char * ) &oack->udp ) + ntohs ( oack->udp.len );

	/* Only possible error */
	errno = EINVAL;

	for ( p = oack->data ; p < end ; ) {
		if ( strcasecmp ( "blksize", p ) == 0 ) {
			p += 8;
			state->blksize = strtoul ( p, &p, 10 );
			if ( *p ) {
				DBG ( "TFTPCORE: garbage \"%s\" "
				      "after blksize\n", p );
				return 0;
			}
			p++;
		} else if ( strcasecmp ( "tsize", p ) == 0 ) {
			p += 6;
			state->tsize = strtoul ( p, &p, 10 );
			if ( *p ) {
				DBG ( "TFTPCORE: garbage \"%s\" "
				      "after tsize\n", p );
				return 0;
			}
			p++;
		} else if ( strcasecmp ( "multicast", p ) == 0 ) {
			char *e = strchr ( p, ',' );
			if ( ( ! e ) || ( e >= end ) ) {
				DBG ( "TFTPCORE: malformed multicast field "
				      "\"%s\"\n", p );
				return 0;
			}
			/* IP address may be missing, in which case we
			 * should leave state->client.sin_addr
			 * unaltered.
			 */
			if ( e != p ) {
				int rc;
				*e = '\0';
				rc = inet_aton ( p, &state->client.sin_addr );
				*e = ',';
				if ( ! rc ) {
					DBG ( "TFTPCORE: malformed multicast "
					      "IP address \"%s\"\n", p );
					return 0;
				}
			}
			p = e + 1;
			/* UDP port may also be missing */
			if ( *p != ',' ) {
				state->client.sin_port = strtoul ( p, &p, 10 );
				if ( *p != ',' ) {
					DBG ( "TFTPCORE: garbage \"%s\" "
					      "after multicast port\n", p );
					return 0;
				}
			} else {
				p++;
			}
			/* "Master Client" must always be present */
			state->master = strtoul ( p, &p, 10 );
			if ( *p ) {
				DBG ( "TFTPCORE: garbage \"%s\" "
				      "after multicast mc\n", p );
				return 0;
			}
			p++;
		} else {
			p += strlen ( p ) + 1; /* skip option name */
			p += strlen ( p ) + 1; /* skip option value */
		}
	}

	if ( p > end ) {
		DBG ( "TFTPCORE: overran options in OACK\n" );
		return 0;
	}

	return 1;
}
