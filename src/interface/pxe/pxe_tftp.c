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

#include <stdlib.h>
#include <stdio.h>
#include <byteswap.h>
#include <gpxe/uaccess.h>
#include <gpxe/in.h>
#include <gpxe/tftp.h>
#include <gpxe/posix_io.h>
#include <pxe.h>

/** File descriptor for "single-file-only" PXE TFTP transfer */
static int pxe_single_fd = -1;

/** Block size for "single-file-only" PXE TFTP transfer */
static size_t pxe_single_blksize;

/** Current block index for "single-file-only" PXE TFTP transfer */
static unsigned int pxe_single_blkidx;

/** Length of a PXE-derived URI
 *
 * The "single-file-only" API calls use a filename field of 128 bytes.
 * 256 bytes provides plenty of space for constructing the (temporary)
 * full URI.
 */
#define PXE_URI_LEN 256

/**
 * Build PXE URI string
 *
 * @v uri_string	URI string to fill in
 * @v ipaddress		Server IP address (in network byte order)
 * @v port		Server port (in network byte order)
 * @v filename		File name
 * @v blksize		Requested block size, or 0
 *
 * The URI string buffer must be at least @c PXE_URI_LEN bytes long.
 */
static void pxe_tftp_build_uri ( char *uri_string,
				 uint32_t ipaddress, unsigned int port,
				 const unsigned char *filename,
				 int blksize ) {
	struct in_addr address;

	address.s_addr = ipaddress;
	if ( ! port )
		port = htons ( TFTP_PORT );
	if ( ! blksize )
		blksize = TFTP_MAX_BLKSIZE;
	tftp_set_request_blksize ( blksize );

	snprintf ( uri_string, PXE_URI_LEN, "tftp://%s:%d%s%s",
		   inet_ntoa ( address ), ntohs ( port ),
		   ( ( filename[0] == '/' ) ? "" : "/" ), filename );
}

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
 * Because we support arbitrary protocols, most of which have no
 * notion of "block size" and will return data in arbitrary-sized
 * chunks, we cheat and pretend to the caller that the blocksize is
 * always accepted as-is.
 *
 * On x86, you must set the s_PXE::StatusCallout field to a nonzero
 * value before calling this function in protected mode.  You cannot
 * call this function with a 32-bit stack segment.  (See the relevant
 * @ref pxe_x86_pmode16 "implementation note" for more details.)
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
	char uri_string[PXE_URI_LEN];

	DBG ( "PXENV_TFTP_OPEN" );

	/* Guard against callers that fail to close before re-opening */
	close ( pxe_single_fd );
	pxe_single_fd = -1;

	/* Construct URI */
	pxe_tftp_build_uri ( uri_string, tftp_open->ServerIPAddress,
			     tftp_open->TFTPPort, tftp_open->FileName,
			     tftp_open->PacketSize );
	DBG ( " %s", uri_string );

	/* Open URI */
	pxe_single_fd = open ( uri_string );
	if ( pxe_single_fd < 0 ) {
		tftp_open->Status = PXENV_STATUS ( pxe_single_fd );
		return PXENV_EXIT_FAILURE;
	}

	/* Record parameters for later use */
	pxe_single_blksize = tftp_open->PacketSize;
	pxe_single_blkidx = 0;

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
 */
PXENV_EXIT_t pxenv_tftp_close ( struct s_PXENV_TFTP_CLOSE *tftp_close ) {
	DBG ( "PXENV_TFTP_CLOSE" );

	close ( pxe_single_fd );
	pxe_single_fd = -1;
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
 * On x86, you must set the s_PXE::StatusCallout field to a nonzero
 * value before calling this function in protected mode.  You cannot
 * call this function with a 32-bit stack segment.  (See the relevant
 * @ref pxe_x86_pmode16 "implementation note" for more details.)
 */
PXENV_EXIT_t pxenv_tftp_read ( struct s_PXENV_TFTP_READ *tftp_read ) {
	userptr_t buffer;
	ssize_t len;

	DBG ( "PXENV_TFTP_READ to %04x:%04x",
	      tftp_read->Buffer.segment, tftp_read->Buffer.offset );

	buffer = real_to_user ( tftp_read->Buffer.segment,
				tftp_read->Buffer.offset );
	len = read_user ( pxe_single_fd, buffer, 0, pxe_single_blksize );
	if ( len < 0 ) {
		tftp_read->Status = PXENV_STATUS ( len );
		return PXENV_EXIT_FAILURE;
	}
	tftp_read->BufferSize = len;
	tftp_read->PacketNumber = ++pxe_single_blkidx;

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
	char uri_string[PXE_URI_LEN];
	int fd;
	userptr_t buffer;
	size_t max_len;
	ssize_t frag_len;
	size_t len = 0;
	int rc = -ENOBUFS;

	DBG ( "PXENV_TFTP_READ_FILE" );

	/* Construct URI */
	pxe_tftp_build_uri ( uri_string, tftp_read_file->ServerIPAddress,
			     tftp_read_file->TFTPSrvPort,
			     tftp_read_file->FileName, 0 );
	DBG ( " %s", uri_string );

	DBG ( " to %08lx+%lx", tftp_read_file->Buffer,
	      tftp_read_file->BufferSize );

	/* Open URI */
	fd = open ( uri_string );
	if ( fd < 0 ) {
		tftp_read_file->Status = PXENV_STATUS ( fd );
		return PXENV_EXIT_FAILURE;
	}

	/* Read file */
	buffer = phys_to_user ( tftp_read_file->Buffer );
	max_len = tftp_read_file->BufferSize;
	while ( max_len ) {
		frag_len = read_user ( fd, buffer, len, max_len );
		if ( frag_len <= 0 ) {
			rc = frag_len;
			break;
		}
		len += frag_len;
		max_len -= frag_len;
	}

	close ( fd );
	tftp_read_file->BufferSize = len;
	tftp_read_file->Status = PXENV_STATUS ( rc );
	return ( rc ? PXENV_EXIT_FAILURE : PXENV_EXIT_SUCCESS );	
}

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
	char uri_string[PXE_URI_LEN];
	int fd;
	ssize_t size;

	DBG ( "PXENV_TFTP_GET_FSIZE" );

	/* Construct URI */
	pxe_tftp_build_uri ( uri_string, tftp_get_fsize->ServerIPAddress,
			     0, tftp_get_fsize->FileName, 0 );
	DBG ( " %s", uri_string );

	/* Open URI */
	fd = open ( uri_string );
	if ( fd < 0 ) {
		tftp_get_fsize->Status = PXENV_STATUS ( fd );
		return PXENV_EXIT_FAILURE;
	}

	/* Determine size */
	size = fsize ( fd );
	close ( fd );
	if ( size < 0 ) {
		tftp_get_fsize->Status = PXENV_STATUS ( size );
		return PXENV_EXIT_FAILURE;
	}

	tftp_get_fsize->FileSize = size;
	tftp_get_fsize->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}
