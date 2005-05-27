/** @file
 *
 * PXE TFTP API
 *
 */

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

#include "pxe.h"

/**
 * TFTP OPEN
 *
 * @v tftp_open				Pointer to a struct s_PXENV_TFTP_OPEN
 * @v s_PXENV_TFTP_OPEN::ServerIPAddress TFTP server IP address
 * @v s_PXENV_TFTP_OPEN::GatewayIPAddress Relay agent IP address, or 0.0.0.0
 * @v s_PXENV_TFTP_OPEN::FileName	Name of file to open
 * @v s_PXENV_TFTP_OPEN::TFTPPort	TFTP server UDP port
 * @v s_PXENV_TFTP_OPEN::PacketSize	TFTP blksize option to request
 * @ret #PXENV_EXIT_SUCCESS		File was opened
 * @ret #PXENV_EXIT_FAILURE		File was not opened
 * @ret s_PXENV_TFTP_OPEN::Status	PXE status code
 * @ret s_PXENV_TFTP_OPEN::PacketSize	Negotiated blksize
 * @err #PXENV_STATUS_TFTP_INVALID_PACKET_SIZE Requested blksize too small
 *
 * Opens a TFTP connection for downloading a file a block at a time
 * using pxenv_tftp_read().
 *
 * If s_PXENV_TFTP_OPEN::GatewayIPAddress is 0.0.0.0, normal IP
 * routing will take place.  See the relevant
 * @ref pxe_routing "implementation note" for more details.
 *
 * The blksize negotiated with the TFTP server will be returned in
 * s_PXENV_TFTP_OPEN::PacketSize, and will be the size of data blocks
 * returned by subsequent calls to pxenv_tftp_read().  The TFTP server
 * may negotiate a smaller blksize than the caller requested.
 *
 * Some TFTP servers do not support TFTP options, and will therefore
 * not be able to use anything other than a fixed 512-byte blksize.
 * The PXE specification version 2.1 requires that the caller must
 * pass in s_PXENV_TFTP_OPEN::PacketSize with a value of 512 or
 * greater.
 *
 * You can only have one TFTP connection open at a time, because the
 * PXE API requires the PXE stack to keep state (e.g. local and remote
 * port numbers, data block index) about the open TFTP connection,
 * rather than letting the caller do so.
 *
 * It is unclear precisely what constitutes a "TFTP open" operation.
 * Clearly, we must send the TFTP open request to the server.  Since
 * we must know whether or not the open succeeded, we must wait for
 * the first reply packet from the TFTP server.  If the TFTP server
 * supports options, the first reply packet will be an OACK; otherwise
 * it will be a DATA packet.  In other words, we may only get to
 * discover whether or not the open succeeded when we receive the
 * first block of data.  However, the pxenv_tftp_open() API provides
 * no way for us to return this block of data at this time.  See the
 * relevant @ref pxe_note_tftp "implementation note" for Etherboot's
 * solution to this problem.
 *
 * 
 * @note If you pass in a value less than 512 for
 * s_PXENV_TFTP_OPEN::PacketSize, Etherboot will attempt to negotiate
 * this blksize with the TFTP server, even though such a value is not
 * permitted according to the PXE specification.  If the TFTP server
 * ends up dictating a blksize larger than the value requested by the
 * caller (which is very probable in the case of a requested blksize
 * less than 512), then Etherboot will return the error
 * #PXENV_STATUS_TFTP_INVALID_PACKET_SIZE.
 *
 * @note According to the PXE specification version 2.1, this call
 * "opens a file for reading/writing", though how writing is to be
 * achieved without the existence of an API call %pxenv_tftp_write()
 * is not made clear.
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_tftp_open ( struct s_PXENV_TFTP_OPEN *tftp_open ) {
	struct sockaddr_in tftp_server;
	struct tftpreq_info_t request;
	struct tftpblk_info_t block;

	DBG ( "PXENV_TFTP_OPEN" );
	ENSURE_READY ( tftp_open );

	/* Set server address and port */
	tftp_server.sin_addr.s_addr = tftp_open->ServerIPAddress
		? tftp_open->ServerIPAddress
		: arptable[ARP_SERVER].ipaddr.s_addr;
	tftp_server.sin_port = ntohs ( tftp_open->TFTPPort );
#ifdef WORK_AROUND_BPBATCH_BUG        
	/* Force use of port 69; BpBatch tries to use port 4 for some         
	* bizarre reason.         */        
	tftp_server.sin_port = TFTP_PORT;
#endif
	/* Ignore gateway address; we can route properly */
	/* Fill in request structure */
	request.server = &tftp_server;
	request.name = tftp_open->FileName;
	request.blksize = tftp_open->PacketSize;
	DBG ( " %@:%d/%s (%d)", tftp_open->ServerIPAddress,
	      tftp_open->TFTPPort, request.name, request.blksize );
	if ( !request.blksize ) request.blksize = TFTP_DEFAULTSIZE_PACKET;
	/* Make request and get first packet */
	if ( !tftp_block ( &request, &block ) ) {
		tftp_open->Status = PXENV_STATUS_TFTP_FILE_NOT_FOUND;
		return PXENV_EXIT_FAILURE;
	}
	/* Fill in PacketSize */
	tftp_open->PacketSize = request.blksize;
	/* Store first block for later retrieval by TFTP_READ */
	pxe_stack->tftpdata.magic_cookie = PXE_TFTP_MAGIC_COOKIE;
	pxe_stack->tftpdata.len = block.len;
	pxe_stack->tftpdata.eof = block.eof;
	memcpy ( pxe_stack->tftpdata.data, block.data, block.len );

	tftp_open->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_TFTP_CLOSE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_tftp_close ( struct s_PXENV_TFTP_CLOSE *tftp_close ) {
	DBG ( "PXENV_TFTP_CLOSE" );
	ENSURE_READY ( tftp_close );
	tftp_close->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_TFTP_READ
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_tftp_read ( struct s_PXENV_TFTP_READ *tftp_read ) {
	struct tftpblk_info_t block;

	DBG ( "PXENV_TFTP_READ" );
	ENSURE_READY ( tftp_read );

	/* Do we have a block pending */
	if ( pxe_stack->tftpdata.magic_cookie == PXE_TFTP_MAGIC_COOKIE ) {
		block.data = pxe_stack->tftpdata.data;
		block.len = pxe_stack->tftpdata.len;
		block.eof = pxe_stack->tftpdata.eof;
		block.block = 1; /* Will be the first block */
		pxe_stack->tftpdata.magic_cookie = 0;
	} else {
		if ( !tftp_block ( NULL, &block ) ) {
			tftp_read->Status = PXENV_STATUS_TFTP_FILE_NOT_FOUND;
			return PXENV_EXIT_FAILURE;
		}
	}

	/* Return data */
	tftp_read->PacketNumber = block.block;
	tftp_read->BufferSize = block.len;
	memcpy ( SEGOFF16_TO_PTR(tftp_read->Buffer), block.data, block.len );
	DBG ( " %d to %hx:%hx", block.len, tftp_read->Buffer.segment,
	      tftp_read->Buffer.offset );
 
	tftp_read->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_TFTP_READ_FILE
 *
 * Status: working
 */

int pxe_tftp_read_block ( unsigned char *data, unsigned int block __unused,
			  unsigned int len, int eof ) {
	if ( pxe_stack->readfile.buffer ) {
		if ( pxe_stack->readfile.offset + len >=
		     pxe_stack->readfile.bufferlen ) return -1;
		memcpy ( pxe_stack->readfile.buffer +
			 pxe_stack->readfile.offset, data, len );
	}
	pxe_stack->readfile.offset += len;
	return eof ? 0 : 1;
}

PXENV_EXIT_t pxenv_tftp_read_file ( struct s_PXENV_TFTP_READ_FILE
				    *tftp_read_file ) {
	struct sockaddr_in tftp_server;
	int rc;

	DBG ( "PXENV_TFTP_READ_FILE %s to [%x,%x)", tftp_read_file->FileName,
	      tftp_read_file->Buffer,
	      tftp_read_file->Buffer + tftp_read_file->BufferSize );
	ENSURE_READY ( tftp_read_file );

	/* inserted by Klaus Wittemeier */
	/* KERNEL_BUF stores the name of the last required file */
	/* This is a fix to make Microsoft Remote Install Services work (RIS) */
	memcpy(KERNEL_BUF, tftp_read_file->FileName, sizeof(KERNEL_BUF));
	/* end of insertion */

	/* Set server address and port */
	tftp_server.sin_addr.s_addr = tftp_read_file->ServerIPAddress
		? tftp_read_file->ServerIPAddress
		: arptable[ARP_SERVER].ipaddr.s_addr;
	tftp_server.sin_port = ntohs ( tftp_read_file->TFTPSrvPort );

	pxe_stack->readfile.buffer = phys_to_virt ( tftp_read_file->Buffer );
	pxe_stack->readfile.bufferlen = tftp_read_file->BufferSize;
	pxe_stack->readfile.offset = 0;

	rc = tftp ( NULL, &tftp_server, tftp_read_file->FileName,
		    pxe_tftp_read_block );
	if ( rc ) {
		tftp_read_file->Status = PXENV_STATUS_FAILURE;
		return PXENV_EXIT_FAILURE;
	}
	tftp_read_file->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_TFTP_GET_FSIZE
 *
 * Status: working, though ugly (we actually read the whole file,
 * because it's too ugly to make Etherboot request the tsize option
 * and hand it to us).
 */
PXENV_EXIT_t pxenv_tftp_get_fsize ( struct s_PXENV_TFTP_GET_FSIZE
				    *tftp_get_fsize ) {
	int rc;

	DBG ( "PXENV_TFTP_GET_FSIZE" );
	ENSURE_READY ( tftp_get_fsize );

	pxe_stack->readfile.buffer = NULL;
	pxe_stack->readfile.bufferlen = 0;
	pxe_stack->readfile.offset = 0;

#warning "Rewrite pxenv_tftp_get_fsize, please"
	if ( rc ) {
		tftp_get_fsize->FileSize = 0;
		tftp_get_fsize->Status = PXENV_STATUS_FAILURE;
		return PXENV_EXIT_FAILURE;
	}
	tftp_get_fsize->FileSize = pxe_stack->readfile.offset;
	tftp_get_fsize->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/** @page pxe_notes Etherboot PXE implementation notes

@section pxe_note_tftp Welding together the TFTP protocol and the PXE TFTP API

The PXE TFTP API is fundamentally poorly designed; the TFTP protocol
simply does not map well into "open file", "read file block", "close
file" operations.  The problem is the unreliable nature of UDP
transmissions and the lock-step mechanism employed by TFTP to
guarantee file transfer.  The lock-step mechanism requires that if we
time out waiting for a packet to arrive, we must trigger its
retransmission by retransmitting our own previously transmitted
packet.

For example, suppose that pxenv_tftp_read() is called to read the
first data block of a file from a server that does not support TFTP
options, and that no data block is received within the timeout period.
In order to trigger the retransmission of this data block,
pxenv_tftp_read() must retransmit the TFTP open request.  However, the
information used to build the TFTP open request is not available at
this time; it was provided only to the pxenv_tftp_open() call.  Even
if we were able to retransmit a TFTP open request, we would have to
allocate a new local port number (and be prepared for data to arrive
from a new remote port number) in order to avoid violating the TFTP
protocol specification.

The question of when to transmit the ACK packets is also awkward.  At
a first glance, it would seem to be fairly simple: acknowledge a
packet immediately after receiving it.  However, since the ACK packet
may itself be lost, the next call to pxenv_tftp_read() must be
prepared to retransmit the acknowledgement.

Another problem to consider is that the pxenv_tftp_open() API call
must return an indication of whether or not the TFTP open request
succeeded.  In the case of a TFTP server that doesn't support TFTP
options, the only indication of a successful open is the reception of
the first data block.  However, the pxenv_tftp_open() API provides no
way to return this data block at this time.

At least some PXE stacks (e.g. NILO) solve this problem by violating
the TFTP protocol and never bothering with retransmissions, relying on
the TFTP server to retransmit when it times out waiting for an ACK.
This approach is dubious at best; if, for example, the initial TFTP
open request is lost then NILO will believe that it has opened the
file and will eventually time out and give up while waiting for the
first packet to arrive.

The only viable solution seems to be to allocate a buffer for the
storage of the first data packet returned by the TFTP server, since we
may receive this packet during the pxenv_tftp_open() call but have to
return it from the subsequent pxenv_tftp_read() call.  This buffer
must be statically allocated and must be dedicated to providing a
temporary home for TFTP packets.  There is nothing in the PXE
specification that prevents a caller from calling
e.g. pxenv_undi_transmit() between calls to the TFTP API, so we cannot
use the normal transmit/receive buffer for this purpose.

Having paid the storage penalty for this buffer, we can then gain some
simplicity by exploiting it in full.  There is at least one
circumstance (pxenv_tftp_open() called to open a file on a server that
does not support TFTP options) in which we will have to enter
pxenv_tftp_read() knowing that our previous transmission (the open
request, in this situation) has already been acknowledged.
Implementation of pxenv_tftp_read() can be made simpler by making this
condition an invariant.  Specifically, on each call to
pxenv_tftp_read(), we shall ensure that the following are true:

  - Our previous transmission has already been acknowledged.  We
    therefore do not need to keep state about our previous
    transmission.

  - The next packet to read is already in a buffer in memory.

In order to maintain these two conditions, pxenv_tftp_read() must do
the following:

  - Copy the data packet from our buffer to the caller's buffer.

  - Acknowledge the data packet that we have just copied.  This will
    trigger transmission of the next packet from the server.

  - Retransmit this acknowledgement packet until the next packet
    arrives.

  - Copy the packet into our internal buffer, ready for the next call
    to pxenv_tftp_read().

It can be verified that this preserves the invariant condition, and it
is clear that the resulting implementation of pxenv_tftp_read() can be
relatively simple.  (For the special case of the last data packet,
pxenv_tftp_read() should return immediately after sending a single
acknowledgement packet.)

In order to set up this invariant condition for the first call to
pxenv_tftp_read(), pxenv_tftp_open() must do the following:

  - Construct and transmit the TFTP open request.

  - Retransmit the TFTP open request (using a new local port number as
    necessary) until a response (DATA, OACK, or ERROR) is received.

  - If the response is an OACK, acknowledge the OACK and retransmit
    the acknowledgement until the first DATA packet arrives.

  - If we have a DATA packet, store it in a buffer ready for the first
    call to pxenv_tftp_read().

This approach has the advantage of being fully compliant with both
RFC1350 (TFTP) and RFC2347 (TFTP options).  It avoids unnecessary
retransmissions.  The cost is approximately 1500 bytes of
uninitialised storage.  Since there is demonstrably no way to avoid
paying this cost without either violating the protocol specifications
or introducing unnecessary retransmissions, we deem this to be a cost
worth paying.

A small performance gain may be obtained by adding a single extra
"send ACK" in both pxenv_tftp_open() and pxenv_tftp_read() immediately
after receiving the DATA packet and copying it into the internal
buffer.   The sequence of events for pxenv_tftp_read() then becomes:

  - Copy the data packet from our buffer to the caller's buffer.

  - If this was the last data packet, return immediately.

  - Check to see if a TFTP data packet is waiting.  If not, send an
    ACK for the data packet that we have just copied, and retransmit
    this ACK until the next data packet arrives.

  - Copy the packet into our internal buffer, ready for the next call
    to pxenv_tftp_read().

  - Send a single ACK for this data packet.

Sending the ACK at this point allows the server to transmit the next
data block while our caller is processing the current packet.  If this
ACK is lost, or the DATA packet it triggers is lost or is consumed by
something other than pxenv_tftp_read() (e.g. by calls to
pxenv_undi_isr()), then the next call to pxenv_tftp_read() will not
find a TFTP data packet waiting and will retransmit the ACK anyway.

Note to future API designers at Intel: try to understand the
underlying network protocol first!

*/
