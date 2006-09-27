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

#include <stdio.h>
#include "pxe.h"

static int pxe_tftp_read_block ( unsigned char *data, unsigned int block,
				 unsigned int len, int eof );

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
 * On x86, you must set the s_PXE::StatusCallout field to a nonzero
 * value before calling this function in protected mode.  You cannot
 * call this function with a 32-bit stack segment.  (See the relevant
 * @ref pxe_x86_pmode16 "implementation note" for more details.)
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
 * @note Despite the existence of the numerous statements within the
 * PXE specification of the form "...if a TFTP/MTFTP or UDP connection
 * is active...", you cannot use pxenv_tftp_open() and
 * pxenv_tftp_read() to read a file via MTFTP; only via plain old
 * TFTP.  If you want to use MTFTP, use pxenv_tftp_read_file()
 * instead.  Astute readers will note that, since
 * pxenv_tftp_read_file() is an atomic operation from the point of
 * view of the PXE API, it is conceptually impossible to issue any
 * other PXE API call "if an MTFTP connection is active".
 */
PXENV_EXIT_t pxenv_tftp_open ( struct s_PXENV_TFTP_OPEN *tftp_open ) {
	DBG ( "PXENV_TFTP_OPEN" );

#if 0
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
	if ( !request.blksize ) request.blksize = TFTP_DEFAULT_BLKSIZE;
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
#endif

	tftp_open->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/**
 * TFTP CLOSE
 *
 * @v tftp_close			Pointer to a struct s_PXENV_TFTP_CLOSE
 * @ret #PXENV_EXIT_SUCCESS		File was closed successfully
 * @ret #PXENV_EXIT_FAILURE		File was not closed
 * @ret s_PXENV_TFTP_CLOSE::Status	PXE status code
 * @err None				-
 *
 * Close a connection previously opened with pxenv_tftp_open().  You
 * must have previously opened a connection with pxenv_tftp_open().
 *
 * On x86, you must set the s_PXE::StatusCallout field to a nonzero
 * value before calling this function in protected mode.  You cannot
 * call this function with a 32-bit stack segment.  (See the relevant
 * @ref pxe_x86_pmode16 "implementation note" for more details.)
 *
 * @note Since TFTP runs over UDP, which is a connectionless protocol,
 * the concept of closing a file is somewhat meaningless.  This call
 * is a no-op for Etherboot.
 */
PXENV_EXIT_t pxenv_tftp_close ( struct s_PXENV_TFTP_CLOSE *tftp_close ) {
	DBG ( "PXENV_TFTP_CLOSE" );

	tftp_close->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/**
 * TFTP READ
 *
 * @v tftp_read				Pointer to a struct s_PXENV_TFTP_READ
 * @v s_PXENV_TFTP_READ::Buffer		Address of data buffer
 * @ret #PXENV_EXIT_SUCCESS		Data was read successfully
 * @ret #PXENV_EXIT_FAILURE		Data was not read
 * @ret s_PXENV_TFTP_READ::Status	PXE status code
 * @ret s_PXENV_TFTP_READ::PacketNumber	TFTP packet number
 * @ret s_PXENV_TFTP_READ::BufferSize	Length of data written into buffer
 *
 * Reads a single packet from a connection previously opened with
 * pxenv_tftp_open() into the data buffer pointed to by
 * s_PXENV_TFTP_READ::Buffer.  You must have previously opened a
 * connection with pxenv_tftp_open().  The data written into
 * s_PXENV_TFTP_READ::Buffer is just the file data; the various
 * network headers have already been removed.
 *
 * The buffer must be large enough to contain a packet of the size
 * negotiated via the s_PXENV_TFTP_OPEN::PacketSize field in the
 * pxenv_tftp_open() call.  It is worth noting that the PXE
 * specification does @b not require the caller to fill in
 * s_PXENV_TFTP_READ::BufferSize before calling pxenv_tftp_read(), so
 * the PXE stack is free to ignore whatever value the caller might
 * place there and just assume that the buffer is large enough.  That
 * said, it may be worth the caller always filling in
 * s_PXENV_TFTP_READ::BufferSize to guard against PXE stacks that
 * mistake it for an input parameter.
 *
 * The length of the TFTP data packet will be returned via
 * s_PXENV_TFTP_READ::BufferSize.  If this length is less than the
 * blksize negotiated via s_PXENV_TFTP_OPEN::PacketSize in the call to
 * pxenv_tftp_open(), this indicates that the block is the last block
 * in the file.  Note that zero is a valid length for
 * s_PXENV_TFTP_READ::BufferSize, and will occur when the length of
 * the file is a multiple of the blksize.
 *
 * The PXE specification doesn't actually state that calls to
 * pxenv_tftp_read() will return the data packets in strict sequential
 * order, though most PXE stacks will probably do so.  The sequence
 * number of the packet will be returned in
 * s_PXENV_TFTP_READ::PacketNumber.  The first packet in the file has
 * a sequence number of one, not zero.
 *
 * To guard against flawed PXE stacks, the caller should probably set
 * s_PXENV_TFTP_READ::PacketNumber to one less than the expected
 * returned value (i.e. set it to zero for the first call to
 * pxenv_tftp_read() and then re-use the returned s_PXENV_TFTP_READ
 * parameter block for subsequent calls without modifying
 * s_PXENV_TFTP_READ::PacketNumber between calls).  The caller should
 * also guard against potential problems caused by flawed
 * implementations returning the occasional duplicate packet, by
 * checking that the value returned in s_PXENV_TFTP_READ::PacketNumber
 * is as expected (i.e. one greater than that returned from the
 * previous call to pxenv_tftp_read()).
 *
 * Nothing in the PXE specification indicates when the TFTP
 * acknowledgement packets will be sent back to the server.  See the
 * relevant @ref pxe_note_tftp "implementation note" for details on
 * when Etherboot chooses to send these packets.
 *
 * On x86, you must set the s_PXE::StatusCallout field to a nonzero
 * value before calling this function in protected mode.  You cannot
 * call this function with a 32-bit stack segment.  (See the relevant
 * @ref pxe_x86_pmode16 "implementation note" for more details.)
 */
PXENV_EXIT_t pxenv_tftp_read ( struct s_PXENV_TFTP_READ *tftp_read ) {
	DBG ( "PXENV_TFTP_READ" );

#if 0
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
#endif
 
	tftp_read->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/**
 * TFTP/MTFTP read file
 *
 * @v tftp_read_file		     Pointer to a struct s_PXENV_TFTP_READ_FILE
 * @v s_PXENV_TFTP_READ_FILE::FileName		File name
 * @v s_PXENV_TFTP_READ_FILE::BufferSize 	Size of the receive buffer
 * @v s_PXENV_TFTP_READ_FILE::Buffer		Address of the receive buffer
 * @v s_PXENV_TFTP_READ_FILE::ServerIPAddress	TFTP server IP address
 * @v s_PXENV_TFTP_READ_FILE::GatewayIPAddress	Relay agent IP address
 * @v s_PXENV_TFTP_READ_FILE::McastIPAddress	File's multicast IP address
 * @v s_PXENV_TFTP_READ_FILE::TFTPClntPort	Client multicast UDP port
 * @v s_PXENV_TFTP_READ_FILE::TFTPSrvPort	Server multicast UDP port
 * @v s_PXENV_TFTP_READ_FILE::TFTPOpenTimeOut	Time to wait for first packet
 * @v s_PXENV_TFTP_READ_FILE::TFTPReopenDelay	MTFTP inactivity timeout
 * @ret #PXENV_EXIT_SUCCESS			File downloaded successfully
 * @ret #PXENV_EXIT_FAILURE			File not downloaded
 * @ret s_PXENV_TFTP_READ_FILE::Status		PXE status code
 * @ret s_PXENV_TFTP_READ_FILE::BufferSize	Length of downloaded file
 *
 * Downloads an entire file via either TFTP or MTFTP into the buffer
 * pointed to by s_PXENV_TFTP_READ_FILE::Buffer.
 *
 * The PXE specification does not make it clear how the caller
 * requests that MTFTP be used rather than TFTP (or vice versa).  One
 * reasonable guess is that setting
 * s_PXENV_TFTP_READ_FILE::McastIPAddress to 0.0.0.0 would cause TFTP
 * to be used instead of MTFTP, though it is conceivable that some PXE
 * stacks would interpret that as "use the DHCP-provided multicast IP
 * address" instead.  Some PXE stacks will not implement MTFTP at all,
 * and will always use TFTP.
 *
 * It is not specified whether or not
 * s_PXENV_TFTP_READ_FILE::TFTPSrvPort will be used as the TFTP server
 * port for TFTP (rather than MTFTP) downloads.  Callers should assume
 * that the only way to access a TFTP server on a non-standard port is
 * to use pxenv_tftp_open() and pxenv_tftp_read().
 *
 * If s_PXENV_TFTP_READ_FILE::GatewayIPAddress is 0.0.0.0, normal IP
 * routing will take place.  See the relevant
 * @ref pxe_routing "implementation note" for more details.
 *
 * It is interesting to note that s_PXENV_TFTP_READ_FILE::Buffer is an
 * #ADDR32_t type, i.e. nominally a flat physical address.  Some PXE
 * NBPs (e.g. NTLDR) are known to call pxenv_tftp_read_file() in real
 * mode with s_PXENV_TFTP_READ_FILE::Buffer set to an address above
 * 1MB.  This means that PXE stacks must be prepared to write to areas
 * outside base memory.  Exactly how this is to be achieved is not
 * specified, though using INT 15,87 is as close to a standard method
 * as any, and should probably be used.  Switching to protected-mode
 * in order to access high memory will fail if pxenv_tftp_read_file()
 * is called in V86 mode; it is reasonably to expect that a V86
 * monitor would intercept the relatively well-defined INT 15,87 if it
 * wants the PXE stack to be able to write to high memory.
 *
 * Things get even more interesting if pxenv_tftp_read_file() is
 * called in protected mode, because there is then absolutely no way
 * for the PXE stack to write to an absolute physical address.  You
 * can't even get around the problem by creating a special "access
 * everything" segment in the s_PXE data structure, because the
 * #SEGDESC_t descriptors are limited to 64kB in size.
 *
 * Previous versions of the PXE specification (e.g. WfM 1.1a) provide
 * a separate API call, %pxenv_tftp_read_file_pmode(), specifically to
 * work around this problem.  The s_PXENV_TFTP_READ_FILE_PMODE
 * parameter block splits s_PXENV_TFTP_READ_FILE::Buffer into
 * s_PXENV_TFTP_READ_FILE_PMODE::BufferSelector and
 * s_PXENV_TFTP_READ_FILE_PMODE::BufferOffset, i.e. it provides a
 * protected-mode segment:offset address for the data buffer.  This
 * API call is no longer present in version 2.1 of the PXE
 * specification.
 *
 * Etherboot makes the assumption that s_PXENV_TFTP_READ_FILE::Buffer
 * is an offset relative to the caller's data segment, when
 * pxenv_tftp_read_file() is called in protected mode.
 *
 * On x86, you must set the s_PXE::StatusCallout field to a nonzero
 * value before calling this function in protected mode.  You cannot
 * call this function with a 32-bit stack segment.  (See the relevant
 * @ref pxe_x86_pmode16 "implementation note" for more details.)
 *
 * @note Microsoft's NTLDR assumes that the filename passed in via
 * s_PXENV_TFTP_READ_FILE::FileName will be stored in the "file" field
 * of the stored DHCPACK packet, whence it will be returned via any
 * subsequent calls to pxenv_get_cached_info().  Though this is
 * essentially a bug in the Intel PXE implementation (not, for once,
 * in the specification!), it is a bug that Microsoft relies upon, and
 * so we implement this bug-for-bug compatibility by overwriting the
 * filename stored DHCPACK packet with the filename passed in
 * s_PXENV_TFTP_READ_FILE::FileName.
 *
 */
PXENV_EXIT_t pxenv_tftp_read_file ( struct s_PXENV_TFTP_READ_FILE
				    *tftp_read_file ) {
	DBG ( "PXENV_TFTP_READ_FILE %s to [%x,%x)", tftp_read_file->FileName,
	      tftp_read_file->Buffer,
	      tftp_read_file->Buffer + tftp_read_file->BufferSize );

#if 0
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
#endif

	tftp_read_file->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

#if 0
static int pxe_tftp_read_block ( unsigned char *data,
				 unsigned int block __unused,
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
#endif

/**
 * TFTP GET FILE SIZE
 *
 * @v tftp_get_fsize		     Pointer to a struct s_PXENV_TFTP_GET_FSIZE
 * @v s_PXENV_TFTP_GET_FSIZE::ServerIPAddress	TFTP server IP address
 * @v s_PXENV_TFTP_GET_FSIZE::GatewayIPAddress	Relay agent IP address
 * @v s_PXENV_TFTP_GET_FSIZE::FileName	File name
 * @ret #PXENV_EXIT_SUCCESS		File size was determined successfully
 * @ret #PXENV_EXIT_FAILURE		File size was not determined
 * @ret s_PXENV_TFTP_GET_FSIZE::Status	PXE status code
 * @ret s_PXENV_TFTP_GET_FSIZE::FileSize	File size
 *
 * Determine the size of a file on a TFTP server.  This uses the
 * "tsize" TFTP option, and so will not work with a TFTP server that
 * does not support TFTP options, or that does not support the "tsize"
 * option.
 *
 * The PXE specification states that this API call will @b not open a
 * TFTP connection for subsequent use with pxenv_tftp_read().  (This
 * is somewhat daft, since the only way to obtain the file size via
 * the "tsize" option involves issuing a TFTP open request, but that's
 * life.)
 *
 * You cannot call pxenv_tftp_get_fsize() while a TFTP or UDP
 * connection is open.
 *
 * If s_PXENV_TFTP_GET_FSIZE::GatewayIPAddress is 0.0.0.0, normal IP
 * routing will take place.  See the relevant
 * @ref pxe_routing "implementation note" for more details.
 *
 * On x86, you must set the s_PXE::StatusCallout field to a nonzero
 * value before calling this function in protected mode.  You cannot
 * call this function with a 32-bit stack segment.  (See the relevant
 * @ref pxe_x86_pmode16 "implementation note" for more details.)
 * 
 * @note There is no way to specify the TFTP server port with this API
 * call.  Though you can open a file using a non-standard TFTP server
 * port (via s_PXENV_TFTP_OPEN::TFTPPort or, potentially,
 * s_PXENV_TFTP_READ_FILE::TFTPSrvPort), you can only get the size of
 * a file from a TFTP server listening on the standard TFTP port.
 * "Consistency" is not a word in Intel's vocabulary.
 */
PXENV_EXIT_t pxenv_tftp_get_fsize ( struct s_PXENV_TFTP_GET_FSIZE
				    *tftp_get_fsize ) {
	int rc;

	DBG ( "PXENV_TFTP_GET_FSIZE" );

#if 0
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
#endif

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
