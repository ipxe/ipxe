/** @file
 *
 * PXE UDP API
 *
 */

#include "pxe.h"
#include "io.h"
#include "string.h"

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

/**
 * UDP OPEN
 *
 * @v udp_open				Pointer to a struct s_PXENV_UDP_OPEN
 * @v s_PXENV_UDP_OPEN::src_ip		IP address of this station, or 0.0.0.0
 * @ret #PXENV_EXIT_SUCCESS		Always
 * @ret s_PXENV_UDP_OPEN::Status	PXE status code
 * @err #PXENV_STATUS_UNDI_INVALID_STATE NIC could not be initialised
 *
 * Prepares the PXE stack for communication using pxenv_udp_write()
 * and pxenv_udp_read().  The IP address supplied in
 * s_PXENV_UDP_OPEN::src_ip will be recorded and used as the local
 * station's IP address for all further communication, including
 * communication by means other than pxenv_udp_write() and
 * pxenv_udp_read().  (If s_PXENV_UDP_OPEN::src_ip is 0.0.0.0, the
 * local station's IP address will remain unchanged.)
 *
 * You can have multiple UDP connections open simultaneously (and
 * even open concurrently with TFTP connections), provided that
 *
 *   - they all have the same local IP address, and
 *
 *   - you take the multiple connections into account when calling
 *     pxenv_udp_read().
 *
 * On x86, you can call pxenv_udp_open() in real mode, 16-bit
 * protected mode with a 16-bit stack segment, 16-bit protected mode
 * with a 32-bit stack segment, or V86 mode.  The pxe::StatusCallout
 * field may be zero even in protected mode.
 * 
 * @note The PXE specification states that you have only one UDP
 * connection open at a time, and that you cannot have a UDP
 * connection open simultaneously with a TFTP connection.  Etherboot
 * does not enforce this unnecessary restriction.
 *
 */
PXENV_EXIT_t pxenv_udp_open ( struct s_PXENV_UDP_OPEN *udp_open ) {
	DBG ( "PXENV_UDP_OPEN" );
	ENSURE_READY ( udp_open );

	if ( udp_open->src_ip &&
	     udp_open->src_ip != arptable[ARP_CLIENT].ipaddr.s_addr ) {
		/* Overwrite our IP address */
		DBG ( " with new IP %@", udp_open->src_ip );
		arptable[ARP_CLIENT].ipaddr.s_addr = udp_open->src_ip;
	}

	udp_open->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/**
 * UDP CLOSE
 *
 * @v udp_close				Pointer to a struct s_PXENV_UDP_CLOSE
 * @ret #PXENV_EXIT_SUCCESS		Always
 * @ret s_PXENV_UDP_CLOSE::Status	PXE status code
 * @err None				-
 *
 * Closes a UDP "connection" opened with pxenv_udp_open().  Since UDP
 * is a connectionless protocol, this is a no-op.
 *
 * You can call pxenv_udp_close() even if there is another active UDP
 * or TFTP connection, since it has no effect on anything.
 *
 * You can call pxenv_udp_close() in real mode, 16-bit protected mode
 * with a 16-bit stack segment, 16-bit protected mode with a 32-bit
 * stack segment, or V86 mode.  The pxe::StatusCallout field may be
 * zero even in protected mode.
 *
 * @note The PXE specification states that you have only one UDP
 * connection open at a time, and that you cannot have a UDP
 * connection open simultaneously with a TFTP connection.  Etherboot
 * does not enforce this unnecessary restriction.
 *
 */
PXENV_EXIT_t pxenv_udp_close ( struct s_PXENV_UDP_CLOSE *udp_close __unused ) {
	DBG ( "PXENV_UDP_CLOSE" );
	udp_close->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/**
 * UDP WRITE
 *
 * @v udp_write				Pointer to a struct s_PXENV_UDP_WRITE
 * @v s_PXENV_UDP_WRITE::ip		Destination IP address
 * @v s_PXENV_UDP_WRITE::gw		Gateway IP address, or 0.0.0.0
 * @v s_PXENV_UDP_WRITE::src_port	Source UDP port, or 0
 * @v s_PXENV_UDP_WRITE::dst_port	Destination UDP port
 * @v s_PXENV_UDP_WRITE::buffer_size	Length of the UDP payload
 * @v s_PXENV_UDP_WRITE::buffer		Address of the UDP payload
 * @ret #PXENV_EXIT_SUCCESS		Packet was transmitted successfully
 * @ret #PXENV_EXIT_FAILURE		Packet could not be transmitter
 * @ret s_PXENV_UDP_WRITE::Status	PXE status code
 * @err #PXENV_STATUS_UNDI_INVALID_STATE NIC could not be initialised
 * @err #PXENV_STATUS_OUT_OF_RESOURCES	Packet was too large to transmit
 * @err other				Any error from pxenv_undi_transmit()
 *
 * Transmits a single UDP packet.  A valid IP and UDP header will be
 * prepended to the payload in s_PXENV_UDP_WRITE::buffer; the buffer
 * should not contain precomputed IP and UDP headers, nor should it
 * contain space allocated for these headers.  The first byte of the
 * buffer will be transmitted as the first byte following the UDP
 * header.
 *
 * If s_PXENV_UDP_WRITE::gw is 0.0.0.0, normal IP routing will take
 * place (using, for example, the default gateway IP address returned
 * by the DHCP server).
 *
 * If s_PXENV_UDP_WRITE::src_port is 0, port 2069 will be used.
 *
 * It is not necessary to call pxenv_udp_open() before using
 * pxenv_udp_write(), unless you want to change the local station's IP
 * address.  pxenv_udp_write() can be called even if there is another
 * active UDP or TFTP connection,.
 *
 * You can call pxenv_udp_write() in real mode, 16-bit protected mode
 * with a 16-bit stack segment, 16-bit protected mode with a 32-bit
 * stack segment, or V86 mode.  The pxe::StatusCallout field may be
 * zero even in protected mode.
 *
 * @note The PXE specification states that you have only one UDP
 * connection open at a time, and that you cannot have a UDP
 * connection open simultaneously with a TFTP connection.  Etherboot
 * does not enforce this unnecessary restriction.
 *
 * @bug s_PXENV_UDP_WRITE::gw is ignored; the default routing table is
 * always used.
 *
 */
PXENV_EXIT_t pxenv_udp_write ( struct s_PXENV_UDP_WRITE *udp_write ) {
	uint16_t src_port;
	uint16_t dst_port;
	struct udppacket *packet = (struct udppacket *)nic.packet;
	int packet_size;

	DBG ( "PXENV_UDP_WRITE" );
	ENSURE_READY ( udp_write );

	/* PXE spec says source port is 2069 if not specified */
	src_port = ntohs(udp_write->src_port);
	if ( src_port == 0 ) src_port = 2069;
	dst_port = ntohs(udp_write->dst_port);
	DBG ( " %d->%@:%d (%d)", src_port, udp_write->ip, dst_port,
	      udp_write->buffer_size );
	
	/* FIXME: we ignore the gateway specified, since we're
	 * confident of being able to do our own routing.  We should
	 * probably allow for multiple gateways.
	 */
	
	/* Copy payload to packet buffer */
	packet_size = ( (void*)&packet->payload - (void*)packet )
		+ udp_write->buffer_size;
	if ( packet_size > ETH_FRAME_LEN ) {
		udp_write->Status = PXENV_STATUS_OUT_OF_RESOURCES;
		return PXENV_EXIT_FAILURE;
	}
	memcpy ( &packet->payload, SEGOFF16_TO_PTR(udp_write->buffer),
		 udp_write->buffer_size );

	/* Transmit packet */
	if ( ! udp_transmit ( udp_write->ip, src_port, dst_port,
			      packet_size, packet ) ) {
		udp_write->Status = errno;
		return PXENV_EXIT_FAILURE;
	}

	udp_write->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* Utility function for pxenv_udp_read() */
static int await_pxe_udp ( int ival __unused, void *ptr,
			   unsigned short ptype __unused,
			   struct iphdr *ip, struct udphdr *udp,
			   struct tcphdr *tcp __unused ) {
	struct s_PXENV_UDP_READ *udp_read = (struct s_PXENV_UDP_READ*)ptr;
	uint16_t d_port;
	size_t size;

	/* Ignore non-UDP packets */
	if ( !udp ) {
		DBG ( " non-UDP" );
		return 0;
	}
	
	/* Check dest_ip */
	if ( udp_read->dest_ip && ( udp_read->dest_ip != ip->dest.s_addr ) ) {
		DBG ( " wrong dest IP (got %@, wanted %@)",
		      ip->dest.s_addr, udp_read->dest_ip );
		return 0;
	}

	/* Check dest_port */
	d_port = ntohs ( udp_read->d_port );
	if ( d_port && ( d_port != ntohs(udp->dest) ) ) {
		DBG ( " wrong dest port (got %d, wanted %d)",
		      ntohs(udp->dest), d_port );
		return 0;
	}

	/* Copy packet to buffer and fill in information */
	udp_read->src_ip = ip->src.s_addr;
	udp_read->s_port = udp->src; /* Both in network order */
	size = ntohs(udp->len) - sizeof(*udp);
	/* Workaround: NTLDR expects us to fill these in, even though
	 * PXESPEC clearly defines them as input parameters.
	 */
	udp_read->dest_ip = ip->dest.s_addr;
	udp_read->d_port = udp->dest;
	DBG ( " %@:%d->%@:%d (%d)",
	      udp_read->src_ip, ntohs(udp_read->s_port),
	      udp_read->dest_ip, ntohs(udp_read->d_port), size );
	if ( udp_read->buffer_size < size ) {
		/* PXESPEC: what error code should we actually return? */
		DBG ( " buffer too small (%d)", udp_read->buffer_size );
		udp_read->Status = PXENV_STATUS_OUT_OF_RESOURCES;
		return 0;
	}
	memcpy ( SEGOFF16_TO_PTR ( udp_read->buffer ), &udp->payload, size );
	udp_read->buffer_size = size;

	return 1;
}

/**
 * UDP READ
 *
 * @v udp_read				Pointer to a struct s_PXENV_UDP_READ
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
 * @err #PXENV_STATUS_UNDI_INVALID_STATE NIC could not be initialised
 * @err #PXENV_STATUS_OUT_OF_RESOURCES	Buffer was too small for payload
 * @err #PXENV_STATUS_FAILURE		No packet was ready to read
 *
 * Receive a single UDP packet.  This is a non-blocking call; if no
 * packet is ready to read, the call will return instantly with
 * s_PXENV_UDP_READ::Status==PXENV_STATUS_FAILURE.
 *
 * If s_PXENV_UDP_READ::dest_ip is 0.0.0.0, UDP packets addressed to
 * any IP address will be accepted and may be returned.
 *
 * If s_PXENV_UDP_READ::d_port is 0, UDP packets addressed to any UDP
 * port will be accepted and may be returned.
 *
 * It is not necessary to call pxenv_udp_open() before using
 * pxenv_udp_read().  pxenv_udp_read() can be called even if there is
 * another active UDP or TFTP connection, but be aware that you might
 * then receive (or cause to be lost) a packet belonging to another
 * connection.
 *
 * You can call pxenv_udp_read() in real mode, 16-bit protected mode
 * with a 16-bit stack segment, 16-bit protected mode with a 32-bit
 * stack segment, or V86 mode.  The pxe::StatusCallout field may be
 * zero even in protected mode.
 *
 * @note The PXE specification states that you have only one UDP
 * connection open at a time, and that you cannot have a UDP
 * connection open simultaneously with a TFTP connection.  Etherboot
 * does not enforce this unnecessary restriction.
 *
 * @note The PXE specification (version 2.1) does not state that we
 * should fill in s_PXENV_UDP_READ::dest_ip and
 * s_PXENV_UDP_READ::d_port, but Microsoft Windows' NTLDR program
 * expects us to do so, and will fail if we don't.
 *
 */
PXENV_EXIT_t pxenv_udp_read ( struct s_PXENV_UDP_READ *udp_read ) {
	DBG ( "PXENV_UDP_READ" );
	ENSURE_READY ( udp_read );

	/* Use await_reply with a timeout of zero */
	/* Allow await_reply to change Status if necessary */
	udp_read->Status = PXENV_STATUS_FAILURE;
	if ( ! await_reply ( await_pxe_udp, 0, udp_read, 0 ) ) {
		return PXENV_EXIT_FAILURE;
	}

	udp_read->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}
