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

/* PXENV_TFTP_OPEN
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
