/** @file
 *
 * PXE UDP API
 *
 */

#include <string.h>
#include <byteswap.h>
#include <gpxe/udp.h>
#include <gpxe/uaccess.h>
#include <gpxe/process.h>
#include <pxe.h>

/*
 * Copyright (C) 2004 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/** A PXE UDP connection */
struct pxe_udp_connection {
	/** Etherboot UDP connection */
	struct udp_connection udp;
	/** "Connection is open" flag */
	int open;
	/** Current pxenv_udp_read() operation, if any */
	struct s_PXENV_UDP_READ *pxenv_udp_read;
	/** Current pxenv_udp_write() operation, if any */
	struct s_PXENV_UDP_WRITE *pxenv_udp_write;
};

static inline struct pxe_udp_connection *
udp_to_pxe ( struct udp_connection *conn ) {
	return container_of ( conn, struct pxe_udp_connection, udp );
}

/**
 * Send PXE UDP data
 *
 * @v conn			UDP connection
 * @v data			Temporary data buffer
 * @v len			Size of temporary data buffer
 * @ret rc			Return status code
 *
 * Sends the packet belonging to the current pxenv_udp_write()
 * operation.
 */
static int pxe_udp_senddata ( struct udp_connection *conn, void *data,
			      size_t len ) {
	struct pxe_udp_connection *pxe_udp = udp_to_pxe ( conn );
	struct s_PXENV_UDP_WRITE *pxenv_udp_write = pxe_udp->pxenv_udp_write;
	userptr_t buffer;

	/* Transmit packet */
	buffer = real_to_user ( pxenv_udp_write->buffer.segment,
				pxenv_udp_write->buffer.offset );
	if ( len > pxenv_udp_write->buffer_size )
		len = pxenv_udp_write->buffer_size;
	copy_from_user ( data, buffer, 0, len );
	return udp_send ( conn, data, len );
}

/**
 * Receive PXE UDP data
 *
 * @v conn			UDP connection
 * @v data			Received data
 * @v len			Length of received data
 * @v st_src			Source address
 * @v st_dest			Destination address
 *
 * Receives a packet as part of the current pxenv_udp_read()
 * operation.
 */
static int pxe_udp_newdata ( struct udp_connection *conn, void *data,
			     size_t len, struct sockaddr_tcpip *st_src,
			     struct sockaddr_tcpip *st_dest ) {
	struct pxe_udp_connection *pxe_udp = udp_to_pxe ( conn );
	struct s_PXENV_UDP_READ *pxenv_udp_read = pxe_udp->pxenv_udp_read;
	struct sockaddr_in *sin_src = ( ( struct sockaddr_in * ) st_src );
	struct sockaddr_in *sin_dest = ( ( struct sockaddr_in * ) st_dest );
	userptr_t buffer;

	if ( ! pxenv_udp_read ) {
		DBG ( "PXE discarded UDP packet\n" );
		return -ENOBUFS;
	}

	/* Copy packet to buffer and record length */
	buffer = real_to_user ( pxenv_udp_read->buffer.segment,
				pxenv_udp_read->buffer.offset );
	if ( len > pxenv_udp_read->buffer_size )
		len = pxenv_udp_read->buffer_size;
	copy_to_user ( buffer, 0, data, len );
	pxenv_udp_read->buffer_size = len;

	/* Fill in source/dest information */
	assert ( sin_src->sin_family == AF_INET );
	pxenv_udp_read->src_ip = sin_src->sin_addr.s_addr;
	pxenv_udp_read->s_port = sin_src->sin_port;
	assert ( sin_dest->sin_family == AF_INET );
	pxenv_udp_read->dest_ip = sin_dest->sin_addr.s_addr;
	pxenv_udp_read->d_port = sin_dest->sin_port;

	/* Mark as received */
	pxe_udp->pxenv_udp_read = NULL;

	return 0;
}

/** PXE UDP operations */
static struct udp_operations pxe_udp_operations = {
	.senddata = pxe_udp_senddata,
	.newdata = pxe_udp_newdata,
};

/** The PXE UDP connection */
static struct pxe_udp_connection pxe_udp = {
	.udp.udp_op = &pxe_udp_operations,
};

/**
 * UDP OPEN
 *
 * @v pxenv_udp_open			Pointer to a struct s_PXENV_UDP_OPEN
 * @v s_PXENV_UDP_OPEN::src_ip		IP address of this station, or 0.0.0.0
 * @ret #PXENV_EXIT_SUCCESS		Always
 * @ret s_PXENV_UDP_OPEN::Status	PXE status code
 * @err #PXENV_STATUS_UDP_OPEN		UDP connection already open
 * @err #PXENV_STATUS_OUT_OF_RESOURCES	Could not open connection
 *
 * Prepares the PXE stack for communication using pxenv_udp_write()
 * and pxenv_udp_read().
 *
 * The IP address supplied in s_PXENV_UDP_OPEN::src_ip will be
 * recorded and used as the local station's IP address for all further
 * communication, including communication by means other than
 * pxenv_udp_write() and pxenv_udp_read().  (If
 * s_PXENV_UDP_OPEN::src_ip is 0.0.0.0, the local station's IP address
 * will remain unchanged.)
 *
 * You can only have one open UDP connection at a time.  This is not a
 * meaningful restriction, since pxenv_udp_write() and
 * pxenv_udp_read() allow you to specify arbitrary local and remote
 * ports and an arbitrary remote address for each packet.  According
 * to the PXE specifiation, you cannot have a UDP connection open at
 * the same time as a TFTP connection; this restriction does not apply
 * to Etherboot.
 *
 * On x86, you must set the s_PXE::StatusCallout field to a nonzero
 * value before calling this function in protected mode.  You cannot
 * call this function with a 32-bit stack segment.  (See the relevant
 * @ref pxe_x86_pmode16 "implementation note" for more details.)
 *
 * @note The PXE specification does not make it clear whether the IP
 * address supplied in s_PXENV_UDP_OPEN::src_ip should be used only
 * for this UDP connection, or retained for all future communication.
 * The latter seems more consistent with typical PXE stack behaviour.
 *
 * @note Etherboot currently ignores the s_PXENV_UDP_OPEN::src_ip
 * parameter.
 *
 */
PXENV_EXIT_t pxenv_udp_open ( struct s_PXENV_UDP_OPEN *pxenv_udp_open ) {
	struct in_addr new_ip = { .s_addr = pxenv_udp_open->src_ip };

	DBG ( "PXENV_UDP_OPEN" );

	/* Check connection is not already open */
	if ( pxe_udp.open ) {
		pxenv_udp_open->Status = PXENV_STATUS_UDP_OPEN;
		return PXENV_EXIT_FAILURE;
	}

	/* Set IP address if specified */
	if ( new_ip.s_addr ) {
		/* FIXME: actually do something here */
		DBG ( " with new IP address %s", inet_ntoa ( new_ip ) );
	}

	/* Open UDP connection */
	if ( udp_open ( &pxe_udp.udp, 0 ) != 0 ) {
		pxenv_udp_open->Status = PXENV_STATUS_OUT_OF_RESOURCES;
		return PXENV_EXIT_FAILURE;
	}
	pxe_udp.open = 1;

	pxenv_udp_open->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/**
 * UDP CLOSE
 *
 * @v pxenv_udp_close			Pointer to a struct s_PXENV_UDP_CLOSE
 * @ret #PXENV_EXIT_SUCCESS		Always
 * @ret s_PXENV_UDP_CLOSE::Status	PXE status code
 * @err None				-
 *
 * Closes a UDP connection opened with pxenv_udp_open().
 *
 * You can only have one open UDP connection at a time.  You cannot
 * have a UDP connection open at the same time as a TFTP connection.
 * You cannot use pxenv_udp_close() to close a TFTP connection; use
 * pxenv_tftp_close() instead.
 *
 * On x86, you must set the s_PXE::StatusCallout field to a nonzero
 * value before calling this function in protected mode.  You cannot
 * call this function with a 32-bit stack segment.  (See the relevant
 * @ref pxe_x86_pmode16 "implementation note" for more details.)
 *
 */
PXENV_EXIT_t pxenv_udp_close ( struct s_PXENV_UDP_CLOSE *pxenv_udp_close ) {
	DBG ( "PXENV_UDP_CLOSE" );

	/* Check connection is open */
	if ( ! pxe_udp.open ) {
		pxenv_udp_close->Status = PXENV_STATUS_UDP_CLOSED;
		return PXENV_EXIT_SUCCESS; /* Well, it *is* closed */
	}

	/* Close UDP connection */
	udp_close ( &pxe_udp.udp );
	pxe_udp.open = 0;

	pxenv_udp_close->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/**
 * UDP WRITE
 *
 * @v pxenv_udp_write			Pointer to a struct s_PXENV_UDP_WRITE
 * @v s_PXENV_UDP_WRITE::ip		Destination IP address
 * @v s_PXENV_UDP_WRITE::gw		Relay agent IP address, or 0.0.0.0
 * @v s_PXENV_UDP_WRITE::src_port	Source UDP port, or 0
 * @v s_PXENV_UDP_WRITE::dst_port	Destination UDP port
 * @v s_PXENV_UDP_WRITE::buffer_size	Length of the UDP payload
 * @v s_PXENV_UDP_WRITE::buffer		Address of the UDP payload
 * @ret #PXENV_EXIT_SUCCESS		Packet was transmitted successfully
 * @ret #PXENV_EXIT_FAILURE		Packet could not be transmitted
 * @ret s_PXENV_UDP_WRITE::Status	PXE status code
 * @err #PXENV_STATUS_UDP_CLOSED	UDP connection is not open
 * @err #PXENV_STATUS_UNDI_TRANSMIT_ERROR Could not transmit packet
 *
 * Transmits a single UDP packet.  A valid IP and UDP header will be
 * prepended to the payload in s_PXENV_UDP_WRITE::buffer; the buffer
 * should not contain precomputed IP and UDP headers, nor should it
 * contain space allocated for these headers.  The first byte of the
 * buffer will be transmitted as the first byte following the UDP
 * header.
 *
 * If s_PXENV_UDP_WRITE::gw is 0.0.0.0, normal IP routing will take
 * place.  See the relevant @ref pxe_routing "implementation note" for
 * more details.
 *
 * If s_PXENV_UDP_WRITE::src_port is 0, port 2069 will be used.
 *
 * You must have opened a UDP connection with pxenv_udp_open() before
 * calling pxenv_udp_write().
 *
 * On x86, you must set the s_PXE::StatusCallout field to a nonzero
 * value before calling this function in protected mode.  You cannot
 * call this function with a 32-bit stack segment.  (See the relevant
 * @ref pxe_x86_pmode16 "implementation note" for more details.)
 *
 * @note Etherboot currently ignores the s_PXENV_UDP_WRITE::gw
 * parameter.
 *
 */
PXENV_EXIT_t pxenv_udp_write ( struct s_PXENV_UDP_WRITE *pxenv_udp_write ) {
	union {
		struct sockaddr_in sin;
		struct sockaddr_tcpip st;
	} dest;
	int rc;

	DBG ( "PXENV_UDP_WRITE" );

	/* Check connection is open */
	if ( ! pxe_udp.open ) {
		pxenv_udp_write->Status = PXENV_STATUS_UDP_CLOSED;
		return PXENV_EXIT_FAILURE;
	}

	/* Construct destination socket address */
	memset ( &dest, 0, sizeof ( dest ) );
	dest.sin.sin_family = AF_INET;
	dest.sin.sin_addr.s_addr = pxenv_udp_write->ip;
	dest.sin.sin_port = pxenv_udp_write->dst_port;
	udp_connect ( &pxe_udp.udp, &dest.st );

	/* Set local (source) port.  PXE spec says source port is 2069
	 * if not specified.  Really, this ought to be set at UDP open
	 * time but hey, we didn't design this API.
	 */
	if ( ! pxenv_udp_write->src_port )
		pxenv_udp_write->src_port = htons ( 2069 );
	udp_bind ( &pxe_udp.udp, pxenv_udp_write->src_port );

	/* FIXME: we ignore the gateway specified, since we're
	 * confident of being able to do our own routing.  We should
	 * probably allow for multiple gateways.
	 */

	DBG ( " %04x:%04x+%x %d->%s:%d", pxenv_udp_write->buffer.segment,
	      pxenv_udp_write->buffer.offset, pxenv_udp_write->buffer_size,
	      ntohs ( pxenv_udp_write->src_port ),
	      inet_ntoa ( dest.sin.sin_addr ),
	      ntohs ( pxenv_udp_write->dst_port ) );
	
	/* Transmit packet */
	pxe_udp.pxenv_udp_write = pxenv_udp_write;
	rc = udp_senddata ( &pxe_udp.udp );
	pxe_udp.pxenv_udp_write = NULL;
	if ( rc != 0 ) {
		pxenv_udp_write->Status = PXENV_STATUS_UNDI_TRANSMIT_ERROR;
		return PXENV_EXIT_FAILURE;
	}

	pxenv_udp_write->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/**
 * UDP READ
 *
 * @v pxenv_udp_read			Pointer to a struct s_PXENV_UDP_READ
 * @v s_PXENV_UDP_READ::dest_ip		Destination IP address, or 0.0.0.0
 * @v s_PXENV_UDP_READ::d_port		Destination UDP port, or 0
 * @v s_PXENV_UDP_READ::buffer_size	Size of the UDP payload buffer
 * @v s_PXENV_UDP_READ::buffer		Address of the UDP payload buffer
 * @ret #PXENV_EXIT_SUCCESS		A packet has been received
 * @ret #PXENV_EXIT_FAILURE		No packet has been received
 * @ret s_PXENV_UDP_READ::Status	PXE status code
 * @ret s_PXENV_UDP_READ::src_ip	Source IP address
 * @ret s_PXENV_UDP_READ::dest_ip	Destination IP address
 * @ret s_PXENV_UDP_READ::s_port	Source UDP port
 * @ret s_PXENV_UDP_READ::d_port	Destination UDP port
 * @ret s_PXENV_UDP_READ::buffer_size	Length of UDP payload
 * @err #PXENV_STATUS_UDP_CLOSED	UDP connection is not open
 * @err #PXENV_STATUS_FAILURE		No packet was ready to read
 *
 * Receive a single UDP packet.  This is a non-blocking call; if no
 * packet is ready to read, the call will return instantly with
 * s_PXENV_UDP_READ::Status==PXENV_STATUS_FAILURE.
 *
 * If s_PXENV_UDP_READ::dest_ip is 0.0.0.0, UDP packets addressed to
 * any IP address will be accepted and may be returned to the caller.
 *
 * If s_PXENV_UDP_READ::d_port is 0, UDP packets addressed to any UDP
 * port will be accepted and may be returned to the caller.
 *
 * You must have opened a UDP connection with pxenv_udp_open() before
 * calling pxenv_udp_read().
 *
 * On x86, you must set the s_PXE::StatusCallout field to a nonzero
 * value before calling this function in protected mode.  You cannot
 * call this function with a 32-bit stack segment.  (See the relevant
 * @ref pxe_x86_pmode16 "implementation note" for more details.)
 *
 * @note The PXE specification (version 2.1) does not state that we
 * should fill in s_PXENV_UDP_READ::dest_ip and
 * s_PXENV_UDP_READ::d_port, but Microsoft Windows' NTLDR program
 * expects us to do so, and will fail if we don't.
 *
 */
PXENV_EXIT_t pxenv_udp_read ( struct s_PXENV_UDP_READ *pxenv_udp_read ) {
	struct in_addr dest_ip = { .s_addr = pxenv_udp_read->dest_ip };
	uint16_t d_port = pxenv_udp_read->d_port;

	DBG ( "PXENV_UDP_READ" );

	/* Check connection is open */
	if ( ! pxe_udp.open ) {
		pxenv_udp_read->Status = PXENV_STATUS_UDP_CLOSED;
		return PXENV_EXIT_FAILURE;
	}

	/* Bind promiscuously; we will do our own filtering */
	udp_bind_promisc ( &pxe_udp.udp );

	/* Try receiving a packet */
	pxe_udp.pxenv_udp_read = pxenv_udp_read;
	step();
	if ( pxe_udp.pxenv_udp_read ) {
		/* No packet received */
		pxe_udp.pxenv_udp_read = NULL;
		goto no_packet;
	}

	/* Filter on destination address and/or port */
	if ( dest_ip.s_addr && ( dest_ip.s_addr != pxenv_udp_read->dest_ip ) )
		goto no_packet;
	if ( d_port && ( d_port != pxenv_udp_read->d_port ) )
		goto no_packet;

	DBG ( " %04x:%04x+%x %s:%d<-%s:%d", pxenv_udp_read->buffer.segment,
	      pxenv_udp_read->buffer.offset, pxenv_udp_read->buffer_size,
	      inet_ntoa ( *( ( struct in_addr * ) &pxenv_udp_read->src_ip ) ),
	      ntohs ( pxenv_udp_read->s_port ),
	      inet_ntoa ( *( ( struct in_addr * ) &pxenv_udp_read->dest_ip ) ),
	      ntohs ( pxenv_udp_read->d_port ) );

	pxenv_udp_read->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;

 no_packet:
	pxenv_udp_read->Status = PXENV_STATUS_FAILURE;
	return PXENV_EXIT_FAILURE;
}
