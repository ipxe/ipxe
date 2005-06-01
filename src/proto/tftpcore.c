#include "tftp.h"
#include "tcp.h" /* for struct tcphdr */
#include "errno.h"
#include "etherboot.h"
#include "tftpcore.h"

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
 * @v tftp_state::server::sin_addr TFTP server IP address
 * @v tftp_state::client::sin_addr Client multicast IP address, or 0.0.0.0
 * @v tftp_state::client::sin_port Client UDP port
 * @v ip			IP header
 * @v udp			UDP header
 * @ret True			This is our TFTP packet
 * @ret False			This is not one of our TFTP packets
 *
 * Wait for a TFTP packet that is part of the current connection
 * (i.e. comes from the TFTP server, has the correct destination port,
 * and is addressed either to our IP address or to our multicast
 * listening address).
 *
 * Invoke await_tftp() using code such as
 *
 * @code
 *
 * if ( await_reply ( await_tftp, 0, &tftp_state, timeout ) ) {
 *	...
 * }
 *
 * @endcode
 */
int await_tftp ( int ival __unused, void *ptr, unsigned short ptype __unused,
		 struct iphdr *ip, struct udphdr *udp,
		 struct tcphdr *tcp __unused ) {
	struct tftp_state *state = ptr;

	/* Must have valid UDP (and, therefore, also IP) headers */
	if ( ! udp ) {
		DBG2 ( "TFTPCORE: not UDP\n" );
		return 0;
	}
	/* Packet must come from the TFTP server */
	if ( ip->src.s_addr != state->server.sin_addr.s_addr ) {
		DBG2 ( "TFTPCORE: from %@, not from TFTP server %@\n",
		       ip->src.s_addr, state->server.sin_addr.s_addr );
		return 0;
	}
	/* Packet must be addressed to the correct UDP port */
	if ( ntohs ( udp->dest ) != state->client.sin_port ) {
		DBG2 ( "TFTPCORE: to UDP port %d, not to TFTP port %d\n",
		       ntohs ( udp->dest ), state->client.sin_port );
		return 0;
	}
	/* Packet must be addressed to us, or to our multicast
	 * listening address (if we have one).
	 */
	if ( ! ( ( ip->dest.s_addr == arptable[ARP_CLIENT].ipaddr.s_addr ) ||
		 ( ( state->client.sin_addr.s_addr ) && 
		   ( ip->dest.s_addr == state->client.sin_addr.s_addr ) ) ) ) {
		DBG2 ( "TFTPCORE: to %@, not to %@ (or %@)\n",
		       ip->dest.s_addr, arptable[ARP_CLIENT].ipaddr.s_addr,
		       state->client.sin_addr.s_addr );
		return 0;
	}
	return 1;
}


/**
 * Issue a TFTP open request (RRQ)
 *
 * @v state				TFTP transfer state
 * @v tftp_state::server::sin_addr	TFTP server IP address
 * @v tftp_state::server::sin_port	TFTP server UDP port, or 0
 * @v tftp_state::client::sin_addr	Client multicast IP address, or 0.0.0.0
 * @v tftp_state::client::sin_port	Client UDP port, or 0
 * @v tftp_state::blksize		Requested blksize, or 0
 * @v filename				File name
 * @ret True				Received a non-error response
 * @ret False				Received error response / no response
 * @ret tftp_state::server::sin_port	TFTP server UDP port
 * @ret tftp_state::client::sin_port	Client UDP port
 * @ret tftp_state::blksize		Always #TFTP_DEFAULT_BLKSIZE
 * @ret *reply				The server's response, if any
 * @err #PXENV_STATUS_TFTP_OPEN_TIMEOUT	TFTP open timed out
 * @err other				As returned by udp_transmit()
 * @err other				As set by tftp_set_errno()
 *
 * Send a TFTP/TFTM/MTFTP RRQ (read request) to a TFTP server, and
 * return the server's reply (which may be an OACK, DATA or ERROR
 * packet).  The server's reply will not be acknowledged, or processed
 * in any way.
 *
 * If tftp_state::server::sin_port is 0, the standard TFTP server port
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
 * tftp_state::server::sin_port will be set to the UDP port from which
 * the server's response originated.  This may or may not be the port
 * to which the open request was sent.
 *
 * The options "blksize", "tsize" and "multicast" will always be
 * appended to a TFTP open request.  Servers that do not understand
 * any of these options should simply ignore them.
 *
 * tftp_open() will not automatically join or leave multicast groups;
 * the caller is responsible for calling join_group() and
 * leave_group() at appropriate times.
 *
 * If the response from the server is a TFTP ERROR packet, tftp_open()
 * will return False and #errno will be set accordingly.
 */
int tftp_open ( struct tftp_state *state, const char *filename,
		union tftp_any **reply ) {
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
	fixed_lport = state->client.sin_port;

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
	*reply = NULL;

	/* Transmit RRQ until we get a response */
	for ( retry = 0 ; retry < MAX_TFTP_RETRIES ; retry++ ) {
		long timeout = rfc2131_sleep_interval ( TIMEOUT, retry );

		/* Set client UDP port, if not already fixed */
		if ( ! fixed_lport )
			state->client.sin_port = ++lport;
		
		/* Send the RRQ */
		DBG ( "TFTPCORE: requesting %@:%d/%s from port %d\n",
		      state->server.sin_addr.s_addr, state->server.sin_port,
		      rrq.data, state->client.sin_port );
		if ( ! udp_transmit ( state->server.sin_addr.s_addr,
				      state->client.sin_port,
				      state->server.sin_port,
				      rrqlen, &rrq ) )
			return 0;
		
		/* Wait for response */
		if ( await_reply ( await_tftp, 0, state, timeout ) ) {
			*reply = ( union tftp_any * ) &nic.packet[ETH_HLEN];
			state->server.sin_port =
				ntohs ( (*reply)->common.udp.src );
			DBG ( "TFTPCORE: got reply from %@:%d (type %d)\n",
			      state->server.sin_addr.s_addr,
			      state->server.sin_port,
			      ntohs ( (*reply)->common.opcode ) );
			if ( ntohs ( (*reply)->common.opcode ) == TFTP_ERROR ){
				tftp_set_errno ( &(*reply)->error );
				return 0;
			}
			return 1;
		}
	}

	DBG ( "TFTPCORE: open request timed out\n" );
	errno = PXENV_STATUS_TFTP_OPEN_TIMEOUT;
	return 0;
}

/**
 * Process a TFTP OACK packet
 *
 * @v state				TFTP transfer state
 * @v oack				The TFTP OACK packet
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
int tftp_process_opts ( struct tftp_state *state, struct tftp_oack *oack ) {
	const char *p;
	const char *end;

	DBG ( "TFTPCORE: processing OACK\n" );

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
			DBG ( "TFTPCORE: got blksize %d\n", state->blksize );
		} else if ( strcasecmp ( "tsize", p ) == 0 ) {
			p += 6;
			state->tsize = strtoul ( p, &p, 10 );
			if ( *p ) {
				DBG ( "TFTPCORE: garbage \"%s\" "
				      "after tsize\n", p );
				return 0;
			}
			p++;
			DBG ( "TFTPCORE: got tsize %d\n", state->tsize );
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
			DBG ( "TFTPCORE: got multicast %@:%d (%s)\n",
			      state->client.sin_addr.s_addr,
			      state->client.sin_port,
			      ( state->master ? "master" : "not master" ) );
		} else {
			DBG ( "TFTPCORE: unknown option \"%s\"\n", p );
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

/**
 * Acknowledge a TFTP packet
 *
 * @v state				TFTP transfer state
 * @v tftp_state::server::sin_addr	TFTP server IP address
 * @v tftp_state::server::sin_port	TFTP server UDP port
 * @v tftp_state::client::sin_port	Client UDP port
 * @v tftp_state::block			Most recently received block number
 * @ret True				Acknowledgement packet was sent
 * @ret False				Acknowledgement packet was not sent
 * @err other				As returned by udp_transmit()
 * 
 * Send a TFTP ACK packet for the most recently received block.
 *
 * This sends only a single ACK packet; it does not wait for the
 * server's response.
 */
int tftp_ack_nowait ( struct tftp_state *state ) {
	struct tftp_ack ack;

	DBG ( "TFTPCORE: acknowledging data block %d\n", state->block );
	ack.opcode = htons ( TFTP_ACK );
	ack.block = htons ( state->block );
	return udp_transmit ( state->server.sin_addr.s_addr,
			      state->client.sin_port, state->server.sin_port,
			      sizeof ( ack ), &ack );
}

/**
 * Acknowledge a TFTP packet and wait for a response
 *
 * @v state				TFTP transfer state
 * @v tftp_state::server::sin_addr	TFTP server IP address
 * @v tftp_state::server::sin_port	TFTP server UDP port
 * @v tftp_state::client::sin_port	Client UDP port
 * @v tftp_state::block			Most recently received block number
 * @ret True				Received a non-error response
 * @ret False				Received error response / no response
 * @ret *reply				The server's response, if any
 * @err #PXENV_STATUS_TFTP_READ_TIMEOUT	Timed out waiting for a response
 * @err other				As returned by tftp_ack_nowait()
 * @err other				As set by tftp_set_errno()
 *
 * Send a TFTP ACK packet for the most recently received data block,
 * and keep transmitting this ACK until we get a response from the
 * server (e.g. a new data block).
 *
 * If the response is a TFTP DATA packet, no processing is done.
 * Specifically, the block number is not checked to ensure that this
 * is indeed the next data block in the sequence, nor is
 * tftp_state::block updated with the new block number.
 *
 * If the response from the server is a TFTP ERROR packet, tftp_open()
 * will return False and #errno will be set accordingly.
 */
int tftp_ack ( struct tftp_state *state, union tftp_any **reply ) {
	int retry;

	*reply = NULL;
	for ( retry = 0 ; retry < MAX_TFTP_RETRIES ; retry++ ) {
		long timeout = rfc2131_sleep_interval ( TFTP_REXMT, retry );
		/* ACK the last data block */
		if ( ! tftp_ack_nowait ( state ) ) {
			DBG ( "TFTP: could not send ACK: %m\n" );
			return 0;
		}	
		if ( await_reply ( await_tftp, 0, state, timeout ) ) {
			/* We received a reply */
			*reply = ( union tftp_any * ) &nic.packet[ETH_HLEN];
			DBG ( "TFTPCORE: got reply (type %d)\n",
			      ntohs ( (*reply)->common.opcode ) );
			if ( ntohs ( (*reply)->common.opcode ) == TFTP_ERROR ){
				tftp_set_errno ( &(*reply)->error );
				return 0;
			}
			return 1;
		}
	}
	DBG ( "TFTP: timed out during read\n" );
	errno = PXENV_STATUS_TFTP_READ_TIMEOUT;
	return 0;
}

/**
 * Send a TFTP error
 *
 * @v state				TFTP transfer state
 * @v tftp_state::server::sin_addr	TFTP server IP address
 * @v tftp_state::server::sin_port	TFTP server UDP port
 * @v tftp_state::client::sin_port	Client UDP port
 * @v errcode				TFTP error code
 * @v errmsg				Descriptive error string
 * @ret True				Error packet was sent
 * @ret False				Error packet was not sent
 *
 * Send a TFTP ERROR packet back to the server to terminate the
 * transfer.
 */
int tftp_error ( struct tftp_state *state, int errcode, const char *errmsg ) {
	struct tftp_error error;

	DBG ( "TFTPCORE: aborting with error %d (%s)\n", errcode, errmsg );
	error.opcode = htons ( TFTP_ERROR );
	error.errcode = htons ( errcode );
	strncpy ( error.errmsg, errmsg, sizeof ( error.errmsg ) );
	return udp_transmit ( state->server.sin_addr.s_addr,
			      state->client.sin_port, state->server.sin_port,
			      sizeof ( error ), &error );
}

/**
 * Interpret a TFTP error
 *
 * @v error				Pointer to a struct tftp_error
 *
 * Sets #errno based on the error code in a TFTP ERROR packet.
 */
void tftp_set_errno ( struct tftp_error *error ) {
	static int errmap[] = {
		[TFTP_ERR_FILE_NOT_FOUND] = PXENV_STATUS_TFTP_FILE_NOT_FOUND,
		[TFTP_ERR_ACCESS_DENIED] = PXENV_STATUS_TFTP_ACCESS_VIOLATION,
		[TFTP_ERR_ILLEGAL_OP] = PXENV_STATUS_TFTP_UNKNOWN_OPCODE,
	};
	unsigned int errcode = ntohs ( error->errcode );
	
	errno = 0;
	if ( errcode < ( sizeof(errmap) / sizeof(errmap[0]) ) )
		errno = errmap[errcode];
	if ( ! errno )
		errno = PXENV_STATUS_TFTP_ERROR_OPCODE;
}
