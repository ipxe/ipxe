/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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

FILE_LICENCE ( GPL2_OR_LATER );

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <gpxe/vsprintf.h>
#include <gpxe/socket.h>
#include <gpxe/xfer.h>
#include <gpxe/open.h>
#include <gpxe/scsi.h>
#include <gpxe/process.h>
#include <gpxe/uaccess.h>
#include <gpxe/tcpip.h>
#include <gpxe/settings.h>
#include <gpxe/features.h>
#include <gpxe/iscsi.h>

/** @file
 *
 * iSCSI protocol
 *
 */

FEATURE ( FEATURE_PROTOCOL, "iSCSI", DHCP_EB_FEATURE_ISCSI, 1 );

/** iSCSI initiator name (explicitly specified) */
static char *iscsi_explicit_initiator_iqn;

/** Default iSCSI initiator name (constructed from hostname) */
static char *iscsi_default_initiator_iqn;

/** iSCSI initiator username */
static char *iscsi_initiator_username;

/** iSCSI initiator password */
static char *iscsi_initiator_password;

/** iSCSI target username */
static char *iscsi_target_username;

/** iSCSI target password */
static char *iscsi_target_password;

static void iscsi_start_tx ( struct iscsi_session *iscsi );
static void iscsi_start_login ( struct iscsi_session *iscsi );
static void iscsi_start_data_out ( struct iscsi_session *iscsi,
				   unsigned int datasn );

/**
 * Finish receiving PDU data into buffer
 *
 * @v iscsi		iSCSI session
 */
static void iscsi_rx_buffered_data_done ( struct iscsi_session *iscsi ) {
	free ( iscsi->rx_buffer );
	iscsi->rx_buffer = NULL;
}

/**
 * Free iSCSI session
 *
 * @v refcnt		Reference counter
 */
static void iscsi_free ( struct refcnt *refcnt ) {
	struct iscsi_session *iscsi =
		container_of ( refcnt, struct iscsi_session, refcnt );

	free ( iscsi->target_address );
	free ( iscsi->target_iqn );
	free ( iscsi->initiator_username );
	free ( iscsi->initiator_password );
	free ( iscsi->target_username );
	free ( iscsi->target_password );
	chap_finish ( &iscsi->chap );
	iscsi_rx_buffered_data_done ( iscsi );
	free ( iscsi );
}

/**
 * Open iSCSI transport-layer connection
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
static int iscsi_open_connection ( struct iscsi_session *iscsi ) {
	struct sockaddr_tcpip target;
	int rc;

	assert ( iscsi->tx_state == ISCSI_TX_IDLE );
	assert ( iscsi->rx_state == ISCSI_RX_BHS );
	assert ( iscsi->rx_offset == 0 );

	/* Open socket */
	memset ( &target, 0, sizeof ( target ) );
	target.st_port = htons ( iscsi->target_port );
	if ( ( rc = xfer_open_named_socket ( &iscsi->socket, SOCK_STREAM,
					     ( struct sockaddr * ) &target,
					     iscsi->target_address,
					     NULL ) ) != 0 ) {
		DBGC ( iscsi, "iSCSI %p could not open socket: %s\n",
		       iscsi, strerror ( rc ) );
		return rc;
	}

	/* Enter security negotiation phase */
	iscsi->status = ( ISCSI_STATUS_SECURITY_NEGOTIATION_PHASE |
			  ISCSI_STATUS_STRINGS_SECURITY );
	if ( iscsi->target_username )
		iscsi->status |= ISCSI_STATUS_AUTH_REVERSE_REQUIRED;

	/* Assign fresh initiator task tag */
	iscsi->itt++;

	/* Initiate login */
	iscsi_start_login ( iscsi );

	return 0;
}

/**
 * Close iSCSI transport-layer connection
 *
 * @v iscsi		iSCSI session
 * @v rc		Reason for close
 *
 * Closes the transport-layer connection and resets the session state
 * ready to attempt a fresh login.
 */
static void iscsi_close_connection ( struct iscsi_session *iscsi, int rc ) {

	/* Close all data transfer interfaces */
	xfer_close ( &iscsi->socket, rc );

	/* Clear connection status */
	iscsi->status = 0;

	/* Reset TX and RX state machines */
	iscsi->tx_state = ISCSI_TX_IDLE;
	iscsi->rx_state = ISCSI_RX_BHS;
	iscsi->rx_offset = 0;

	/* Free any temporary dynamically allocated memory */
	chap_finish ( &iscsi->chap );
	iscsi_rx_buffered_data_done ( iscsi );
}

/**
 * Mark iSCSI SCSI operation as complete
 *
 * @v iscsi		iSCSI session
 * @v rc		Return status code
 *
 * Note that iscsi_scsi_done() will not close the connection, and must
 * therefore be called only when the internal state machines are in an
 * appropriate state, otherwise bad things may happen on the next call
 * to iscsi_issue().  The general rule is to call iscsi_scsi_done()
 * only at the end of receiving a PDU; at this point the TX and RX
 * engines should both be idle.
 */
static void iscsi_scsi_done ( struct iscsi_session *iscsi, int rc ) {

	assert ( iscsi->tx_state == ISCSI_TX_IDLE );
	assert ( iscsi->command != NULL );

	iscsi->command->rc = rc;
	iscsi->command = NULL;
}

/****************************************************************************
 *
 * iSCSI SCSI command issuing
 *
 */

/**
 * Build iSCSI SCSI command BHS
 *
 * @v iscsi		iSCSI session
 *
 * We don't currently support bidirectional commands (i.e. with both
 * Data-In and Data-Out segments); these would require providing code
 * to generate an AHS, and there doesn't seem to be any need for it at
 * the moment.
 */
static void iscsi_start_command ( struct iscsi_session *iscsi ) {
	struct iscsi_bhs_scsi_command *command = &iscsi->tx_bhs.scsi_command;

	assert ( ! ( iscsi->command->data_in && iscsi->command->data_out ) );

	/* Construct BHS and initiate transmission */
	iscsi_start_tx ( iscsi );
	command->opcode = ISCSI_OPCODE_SCSI_COMMAND;
	command->flags = ( ISCSI_FLAG_FINAL |
			   ISCSI_COMMAND_ATTR_SIMPLE );
	if ( iscsi->command->data_in )
		command->flags |= ISCSI_COMMAND_FLAG_READ;
	if ( iscsi->command->data_out )
		command->flags |= ISCSI_COMMAND_FLAG_WRITE;
	/* lengths left as zero */
	command->lun = iscsi->lun;
	command->itt = htonl ( ++iscsi->itt );
	command->exp_len = htonl ( iscsi->command->data_in_len |
				   iscsi->command->data_out_len );
	command->cmdsn = htonl ( iscsi->cmdsn );
	command->expstatsn = htonl ( iscsi->statsn + 1 );
	memcpy ( &command->cdb, &iscsi->command->cdb, sizeof ( command->cdb ));
	DBGC2 ( iscsi, "iSCSI %p start " SCSI_CDB_FORMAT " %s %#zx\n",
		iscsi, SCSI_CDB_DATA ( command->cdb ),
		( iscsi->command->data_in ? "in" : "out" ),
		( iscsi->command->data_in ?
		  iscsi->command->data_in_len :
		  iscsi->command->data_out_len ) );
}

/**
 * Receive data segment of an iSCSI SCSI response PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * @ret rc		Return status code
 */
static int iscsi_rx_scsi_response ( struct iscsi_session *iscsi,
				    const void *data, size_t len,
				    size_t remaining ) {
	struct iscsi_bhs_scsi_response *response
		= &iscsi->rx_bhs.scsi_response;
	int sense_offset;

	/* Capture the sense response code as it floats past, if present */
	sense_offset = ISCSI_SENSE_RESPONSE_CODE_OFFSET - iscsi->rx_offset;
	if ( ( sense_offset >= 0 ) && len ) {
		iscsi->command->sense_response =
			* ( ( char * ) data + sense_offset );
	}

	/* Wait for whole SCSI response to arrive */
	if ( remaining )
		return 0;
	
	/* Record SCSI status code */
	iscsi->command->status = response->status;

	/* Check for errors */
	if ( response->response != ISCSI_RESPONSE_COMMAND_COMPLETE )
		return -EIO;

	/* Mark as completed */
	iscsi_scsi_done ( iscsi, 0 );
	return 0;
}

/**
 * Receive data segment of an iSCSI data-in PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * @ret rc		Return status code
 */
static int iscsi_rx_data_in ( struct iscsi_session *iscsi,
			      const void *data, size_t len,
			      size_t remaining ) {
	struct iscsi_bhs_data_in *data_in = &iscsi->rx_bhs.data_in;
	unsigned long offset;

	/* Copy data to data-in buffer */
	offset = ntohl ( data_in->offset ) + iscsi->rx_offset;
	assert ( iscsi->command != NULL );
	assert ( iscsi->command->data_in );
	assert ( ( offset + len ) <= iscsi->command->data_in_len );
	copy_to_user ( iscsi->command->data_in, offset, data, len );

	/* Wait for whole SCSI response to arrive */
	if ( remaining )
		return 0;

	/* Mark as completed if status is present */
	if ( data_in->flags & ISCSI_DATA_FLAG_STATUS ) {
		assert ( ( offset + len ) == iscsi->command->data_in_len );
		assert ( data_in->flags & ISCSI_FLAG_FINAL );
		iscsi->command->status = data_in->status;
		/* iSCSI cannot return an error status via a data-in */
		iscsi_scsi_done ( iscsi, 0 );
	}

	return 0;
}

/**
 * Receive data segment of an iSCSI R2T PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * @ret rc		Return status code
 */
static int iscsi_rx_r2t ( struct iscsi_session *iscsi,
			  const void *data __unused, size_t len __unused,
			  size_t remaining __unused ) {
	struct iscsi_bhs_r2t *r2t = &iscsi->rx_bhs.r2t;

	/* Record transfer parameters and trigger first data-out */
	iscsi->ttt = ntohl ( r2t->ttt );
	iscsi->transfer_offset = ntohl ( r2t->offset );
	iscsi->transfer_len = ntohl ( r2t->len );
	iscsi_start_data_out ( iscsi, 0 );

	return 0;
}

/**
 * Build iSCSI data-out BHS
 *
 * @v iscsi		iSCSI session
 * @v datasn		Data sequence number within the transfer
 *
 */
static void iscsi_start_data_out ( struct iscsi_session *iscsi,
				   unsigned int datasn ) {
	struct iscsi_bhs_data_out *data_out = &iscsi->tx_bhs.data_out;
	unsigned long offset;
	unsigned long remaining;
	unsigned long len;

	/* We always send 512-byte Data-Out PDUs; this removes the
	 * need to worry about the target's MaxRecvDataSegmentLength.
	 */
	offset = datasn * 512;
	remaining = iscsi->transfer_len - offset;
	len = remaining;
	if ( len > 512 )
		len = 512;

	/* Construct BHS and initiate transmission */
	iscsi_start_tx ( iscsi );
	data_out->opcode = ISCSI_OPCODE_DATA_OUT;
	if ( len == remaining )
		data_out->flags = ( ISCSI_FLAG_FINAL );
	ISCSI_SET_LENGTHS ( data_out->lengths, 0, len );
	data_out->lun = iscsi->lun;
	data_out->itt = htonl ( iscsi->itt );
	data_out->ttt = htonl ( iscsi->ttt );
	data_out->expstatsn = htonl ( iscsi->statsn + 1 );
	data_out->datasn = htonl ( datasn );
	data_out->offset = htonl ( iscsi->transfer_offset + offset );
	DBGC ( iscsi, "iSCSI %p start data out DataSN %#x len %#lx\n",
	       iscsi, datasn, len );
}

/**
 * Complete iSCSI data-out PDU transmission
 *
 * @v iscsi		iSCSI session
 *
 */
static void iscsi_data_out_done ( struct iscsi_session *iscsi ) {
	struct iscsi_bhs_data_out *data_out = &iscsi->tx_bhs.data_out;

	/* If we haven't reached the end of the sequence, start
	 * sending the next data-out PDU.
	 */
	if ( ! ( data_out->flags & ISCSI_FLAG_FINAL ) )
		iscsi_start_data_out ( iscsi, ntohl ( data_out->datasn ) + 1 );
}

/**
 * Send iSCSI data-out data segment
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
static int iscsi_tx_data_out ( struct iscsi_session *iscsi ) {
	struct iscsi_bhs_data_out *data_out = &iscsi->tx_bhs.data_out;
	struct io_buffer *iobuf;
	unsigned long offset;
	size_t len;

	offset = ntohl ( data_out->offset );
	len = ISCSI_DATA_LEN ( data_out->lengths );

	assert ( iscsi->command != NULL );
	assert ( iscsi->command->data_out );
	assert ( ( offset + len ) <= iscsi->command->data_out_len );

	iobuf = xfer_alloc_iob ( &iscsi->socket, len );
	if ( ! iobuf )
		return -ENOMEM;
	
	copy_from_user ( iob_put ( iobuf, len ),
			 iscsi->command->data_out, offset, len );

	return xfer_deliver_iob ( &iscsi->socket, iobuf );
}

/****************************************************************************
 *
 * iSCSI login
 *
 */

/**
 * Build iSCSI login request strings
 *
 * @v iscsi		iSCSI session
 *
 * These are the initial set of strings sent in the first login
 * request PDU.  We want the following settings:
 *
 *     HeaderDigest=None
 *     DataDigest=None
 *     MaxConnections is irrelevant; we make only one connection anyway
 *     InitialR2T=Yes [1]
 *     ImmediateData is irrelevant; we never send immediate data
 *     MaxRecvDataSegmentLength=8192 (default; we don't care) [3]
 *     MaxBurstLength=262144 (default; we don't care) [3]
 *     FirstBurstLength=262144 (default; we don't care)
 *     DefaultTime2Wait=0 [2]
 *     DefaultTime2Retain=0 [2]
 *     MaxOutstandingR2T=1
 *     DataPDUInOrder=Yes
 *     DataSequenceInOrder=Yes
 *     ErrorRecoveryLevel=0
 *
 * [1] InitialR2T has an OR resolution function, so the target may
 * force us to use it.  We therefore simplify our logic by always
 * using it.
 *
 * [2] These ensure that we can safely start a new task once we have
 * reconnected after a failure, without having to manually tidy up
 * after the old one.
 *
 * [3] We are quite happy to use the RFC-defined default values for
 * these parameters, but some targets (notably OpenSolaris)
 * incorrectly assume a default value of zero, so we explicitly
 * specify the default values.
 */
static int iscsi_build_login_request_strings ( struct iscsi_session *iscsi,
					       void *data, size_t len ) {
	unsigned int used = 0;
	unsigned int i;
	const char *auth_method;

	if ( iscsi->status & ISCSI_STATUS_STRINGS_SECURITY ) {
		/* Default to allowing no authentication */
		auth_method = "None";
		/* If we have a credential to supply, permit CHAP */
		if ( iscsi->initiator_username )
			auth_method = "CHAP,None";
		/* If we have a credential to check, force CHAP */
		if ( iscsi->target_username )
			auth_method = "CHAP";
		used += ssnprintf ( data + used, len - used,
				    "InitiatorName=%s%c"
				    "TargetName=%s%c"
				    "SessionType=Normal%c"
				    "AuthMethod=%s%c",
				    iscsi_initiator_iqn(), 0,
				    iscsi->target_iqn, 0, 0,
				    auth_method, 0 );
	}

	if ( iscsi->status & ISCSI_STATUS_STRINGS_CHAP_ALGORITHM ) {
		used += ssnprintf ( data + used, len - used, "CHAP_A=5%c", 0 );
	}
	
	if ( ( iscsi->status & ISCSI_STATUS_STRINGS_CHAP_RESPONSE ) ) {
		assert ( iscsi->initiator_username != NULL );
		used += ssnprintf ( data + used, len - used,
				    "CHAP_N=%s%cCHAP_R=0x",
				    iscsi->initiator_username, 0 );
		for ( i = 0 ; i < iscsi->chap.response_len ; i++ ) {
			used += ssnprintf ( data + used, len - used, "%02x",
					    iscsi->chap.response[i] );
		}
		used += ssnprintf ( data + used, len - used, "%c", 0 );
	}

	if ( ( iscsi->status & ISCSI_STATUS_STRINGS_CHAP_CHALLENGE ) ) {
		used += ssnprintf ( data + used, len - used,
				    "CHAP_I=%d%cCHAP_C=0x",
				    iscsi->chap_challenge[0], 0 );
		for ( i = 1 ; i < sizeof ( iscsi->chap_challenge ) ; i++ ) {
			used += ssnprintf ( data + used, len - used, "%02x",
					    iscsi->chap_challenge[i] );
		}
		used += ssnprintf ( data + used, len - used, "%c", 0 );
	}

	if ( iscsi->status & ISCSI_STATUS_STRINGS_OPERATIONAL ) {
		used += ssnprintf ( data + used, len - used,
				    "HeaderDigest=None%c"
				    "DataDigest=None%c"
				    "InitialR2T=Yes%c"
				    "MaxRecvDataSegmentLength=8192%c"
				    "MaxBurstLength=262144%c"
				    "DefaultTime2Wait=0%c"
				    "DefaultTime2Retain=0%c"
				    "MaxOutstandingR2T=1%c"
				    "DataPDUInOrder=Yes%c"
				    "DataSequenceInOrder=Yes%c"
				    "ErrorRecoveryLevel=0%c",
				    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	}

	return used;
}

/**
 * Build iSCSI login request BHS
 *
 * @v iscsi		iSCSI session
 */
static void iscsi_start_login ( struct iscsi_session *iscsi ) {
	struct iscsi_bhs_login_request *request = &iscsi->tx_bhs.login_request;
	int len;

	/* Construct BHS and initiate transmission */
	iscsi_start_tx ( iscsi );
	request->opcode = ( ISCSI_OPCODE_LOGIN_REQUEST |
			    ISCSI_FLAG_IMMEDIATE );
	request->flags = ( ( iscsi->status & ISCSI_STATUS_PHASE_MASK ) |
			   ISCSI_LOGIN_FLAG_TRANSITION );
	/* version_max and version_min left as zero */
	len = iscsi_build_login_request_strings ( iscsi, NULL, 0 );
	ISCSI_SET_LENGTHS ( request->lengths, 0, len );
	request->isid_iana_en = htonl ( ISCSI_ISID_IANA |
					IANA_EN_FEN_SYSTEMS );
	/* isid_iana_qual left as zero */
	request->tsih = htons ( iscsi->tsih );
	request->itt = htonl ( iscsi->itt );
	/* cid left as zero */
	request->cmdsn = htonl ( iscsi->cmdsn );
	request->expstatsn = htonl ( iscsi->statsn + 1 );
}

/**
 * Complete iSCSI login request PDU transmission
 *
 * @v iscsi		iSCSI session
 *
 */
static void iscsi_login_request_done ( struct iscsi_session *iscsi ) {

	/* Clear any "strings to send" flags */
	iscsi->status &= ~ISCSI_STATUS_STRINGS_MASK;

	/* Free any dynamically allocated storage used for login */
	chap_finish ( &iscsi->chap );
}

/**
 * Transmit data segment of an iSCSI login request PDU
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 *
 * For login requests, the data segment consists of the login strings.
 */
static int iscsi_tx_login_request ( struct iscsi_session *iscsi ) {
	struct iscsi_bhs_login_request *request = &iscsi->tx_bhs.login_request;
	struct io_buffer *iobuf;
	size_t len;

	len = ISCSI_DATA_LEN ( request->lengths );
	iobuf = xfer_alloc_iob ( &iscsi->socket, len );
	if ( ! iobuf )
		return -ENOMEM;
	iob_put ( iobuf, len );
	iscsi_build_login_request_strings ( iscsi, iobuf->data, len );
	return xfer_deliver_iob ( &iscsi->socket, iobuf );
}

/**
 * Handle iSCSI TargetAddress text value
 *
 * @v iscsi		iSCSI session
 * @v value		TargetAddress value
 * @ret rc		Return status code
 */
static int iscsi_handle_targetaddress_value ( struct iscsi_session *iscsi,
					      const char *value ) {
	char *separator;

	DBGC ( iscsi, "iSCSI %p will redirect to %s\n", iscsi, value );

	/* Replace target address */
	free ( iscsi->target_address );
	iscsi->target_address = strdup ( value );
	if ( ! iscsi->target_address )
		return -ENOMEM;

	/* Replace target port */
	iscsi->target_port = htons ( ISCSI_PORT );
	separator = strchr ( iscsi->target_address, ':' );
	if ( separator ) {
		*separator = '\0';
		iscsi->target_port = strtoul ( ( separator + 1 ), NULL, 0 );
	}

	return 0;
}

/**
 * Handle iSCSI AuthMethod text value
 *
 * @v iscsi		iSCSI session
 * @v value		AuthMethod value
 * @ret rc		Return status code
 */
static int iscsi_handle_authmethod_value ( struct iscsi_session *iscsi,
					   const char *value ) {

	/* If server requests CHAP, send the CHAP_A string */
	if ( strcmp ( value, "CHAP" ) == 0 ) {
		DBGC ( iscsi, "iSCSI %p initiating CHAP authentication\n",
		       iscsi );
		iscsi->status |= ( ISCSI_STATUS_STRINGS_CHAP_ALGORITHM |
				   ISCSI_STATUS_AUTH_FORWARD_REQUIRED );
	}

	return 0;
}

/**
 * Handle iSCSI CHAP_A text value
 *
 * @v iscsi		iSCSI session
 * @v value		CHAP_A value
 * @ret rc		Return status code
 */
static int iscsi_handle_chap_a_value ( struct iscsi_session *iscsi,
				       const char *value ) {

	/* We only ever offer "5" (i.e. MD5) as an algorithm, so if
	 * the server responds with anything else it is a protocol
	 * violation.
	 */
	if ( strcmp ( value, "5" ) != 0 ) {
		DBGC ( iscsi, "iSCSI %p got invalid CHAP algorithm \"%s\"\n",
		       iscsi, value );
		return -EPROTO;
	}

	return 0;
}

/**
 * Handle iSCSI CHAP_I text value
 *
 * @v iscsi		iSCSI session
 * @v value		CHAP_I value
 * @ret rc		Return status code
 */
static int iscsi_handle_chap_i_value ( struct iscsi_session *iscsi,
				       const char *value ) {
	unsigned int identifier;
	char *endp;
	int rc;

	/* The CHAP identifier is an integer value */
	identifier = strtoul ( value, &endp, 0 );
	if ( *endp != '\0' ) {
		DBGC ( iscsi, "iSCSI %p saw invalid CHAP identifier \"%s\"\n",
		       iscsi, value );
		return -EPROTO;
	}

	/* Prepare for CHAP with MD5 */
	chap_finish ( &iscsi->chap );
	if ( ( rc = chap_init ( &iscsi->chap, &md5_algorithm ) ) != 0 ) {
		DBGC ( iscsi, "iSCSI %p could not initialise CHAP: %s\n",
		       iscsi, strerror ( rc ) );
		return rc;
	}

	/* Identifier and secret are the first two components of the
	 * challenge.
	 */
	chap_set_identifier ( &iscsi->chap, identifier );
	if ( iscsi->initiator_password ) {
		chap_update ( &iscsi->chap, iscsi->initiator_password,
			      strlen ( iscsi->initiator_password ) );
	}

	return 0;
}

/**
 * Handle iSCSI CHAP_C text value
 *
 * @v iscsi		iSCSI session
 * @v value		CHAP_C value
 * @ret rc		Return status code
 */
static int iscsi_handle_chap_c_value ( struct iscsi_session *iscsi,
				       const char *value ) {
	char buf[3];
	char *endp;
	uint8_t byte;
	unsigned int i;

	/* Check and strip leading "0x" */
	if ( ( value[0] != '0' ) || ( value[1] != 'x' ) ) {
		DBGC ( iscsi, "iSCSI %p saw invalid CHAP challenge \"%s\"\n",
		       iscsi, value );
		return -EPROTO;
	}
	value += 2;

	/* Process challenge an octet at a time */
	for ( ; ( value[0] && value[1] ) ; value += 2 ) {
		memcpy ( buf, value, 2 );
		buf[2] = 0;
		byte = strtoul ( buf, &endp, 16 );
		if ( *endp != '\0' ) {
			DBGC ( iscsi, "iSCSI %p saw invalid CHAP challenge "
			       "byte \"%s\"\n", iscsi, buf );
			return -EPROTO;
		}
		chap_update ( &iscsi->chap, &byte, sizeof ( byte ) );
	}

	/* Build CHAP response */
	DBGC ( iscsi, "iSCSI %p sending CHAP response\n", iscsi );
	chap_respond ( &iscsi->chap );
	iscsi->status |= ISCSI_STATUS_STRINGS_CHAP_RESPONSE;

	/* Send CHAP challenge, if applicable */
	if ( iscsi->target_username ) {
		iscsi->status |= ISCSI_STATUS_STRINGS_CHAP_CHALLENGE;
		/* Generate CHAP challenge data */
		for ( i = 0 ; i < sizeof ( iscsi->chap_challenge ) ; i++ ) {
			iscsi->chap_challenge[i] = random();
		}
	}

	return 0;
}

/**
 * Handle iSCSI CHAP_N text value
 *
 * @v iscsi		iSCSI session
 * @v value		CHAP_N value
 * @ret rc		Return status code
 */
static int iscsi_handle_chap_n_value ( struct iscsi_session *iscsi,
				       const char *value ) {

	/* The target username isn't actually involved at any point in
	 * the authentication process; it merely serves to identify
	 * which password the target is using to generate the CHAP
	 * response.  We unnecessarily verify that the username is as
	 * expected, in order to provide mildly helpful diagnostics if
	 * the target is supplying the wrong username/password
	 * combination.
	 */
	if ( iscsi->target_username &&
	     ( strcmp ( iscsi->target_username, value ) != 0 ) ) {
		DBGC ( iscsi, "iSCSI %p target username \"%s\" incorrect "
		       "(wanted \"%s\")\n",
		       iscsi, value, iscsi->target_username );
		return -EACCES;
	}

	return 0;
}

/**
 * Handle iSCSI CHAP_R text value
 *
 * @v iscsi		iSCSI session
 * @v value		CHAP_R value
 * @ret rc		Return status code
 */
static int iscsi_handle_chap_r_value ( struct iscsi_session *iscsi,
				       const char *value ) {
	char buf[3];
	char *endp;
	uint8_t byte;
	unsigned int i;
	int rc;

	/* Generate CHAP response for verification */
	chap_finish ( &iscsi->chap );
	if ( ( rc = chap_init ( &iscsi->chap, &md5_algorithm ) ) != 0 ) {
		DBGC ( iscsi, "iSCSI %p could not initialise CHAP: %s\n",
		       iscsi, strerror ( rc ) );
		return rc;
	}
	chap_set_identifier ( &iscsi->chap, iscsi->chap_challenge[0] );
	if ( iscsi->target_password ) {
		chap_update ( &iscsi->chap, iscsi->target_password,
			      strlen ( iscsi->target_password ) );
	}
	chap_update ( &iscsi->chap, &iscsi->chap_challenge[1],
		      ( sizeof ( iscsi->chap_challenge ) - 1 ) );
	chap_respond ( &iscsi->chap );

	/* Check and strip leading "0x" */
	if ( ( value[0] != '0' ) || ( value[1] != 'x' ) ) {
		DBGC ( iscsi, "iSCSI %p saw invalid CHAP response \"%s\"\n",
		       iscsi, value );
		return -EPROTO;
	}
	value += 2;

	/* Check CHAP response length */
	if ( strlen ( value ) != ( 2 * iscsi->chap.response_len ) ) {
		DBGC ( iscsi, "iSCSI %p invalid CHAP response length\n",
		       iscsi );
		return -EPROTO;
	}

	/* Process response an octet at a time */
	for ( i = 0 ; ( value[0] && value[1] ) ; value += 2, i++ ) {
		memcpy ( buf, value, 2 );
		buf[2] = 0;
		byte = strtoul ( buf, &endp, 16 );
		if ( *endp != '\0' ) {
			DBGC ( iscsi, "iSCSI %p saw invalid CHAP response "
			       "byte \"%s\"\n", iscsi, buf );
			return -EPROTO;
		}
		if ( byte != iscsi->chap.response[i] ) {
			DBGC ( iscsi, "iSCSI %p saw incorrect CHAP "
			       "response\n", iscsi );
			return -EACCES;
		}
	}
	assert ( i == iscsi->chap.response_len );

	/* Mark session as authenticated */
	iscsi->status |= ISCSI_STATUS_AUTH_REVERSE_OK;

	return 0;
}

/** An iSCSI text string that we want to handle */
struct iscsi_string_type {
	/** String key
	 *
	 * This is the portion up to and including the "=" sign,
	 * e.g. "InitiatorName=", "CHAP_A=", etc.
	 */
	const char *key;
	/** Handle iSCSI string value
	 *
	 * @v iscsi		iSCSI session
	 * @v value		iSCSI string value
	 * @ret rc		Return status code
	 */
	int ( * handle ) ( struct iscsi_session *iscsi, const char *value );
};

/** iSCSI text strings that we want to handle */
static struct iscsi_string_type iscsi_string_types[] = {
	{ "TargetAddress=", iscsi_handle_targetaddress_value },
	{ "AuthMethod=", iscsi_handle_authmethod_value },
	{ "CHAP_A=", iscsi_handle_chap_a_value },
	{ "CHAP_I=", iscsi_handle_chap_i_value },
	{ "CHAP_C=", iscsi_handle_chap_c_value },
	{ "CHAP_N=", iscsi_handle_chap_n_value },
	{ "CHAP_R=", iscsi_handle_chap_r_value },
	{ NULL, NULL }
};

/**
 * Handle iSCSI string
 *
 * @v iscsi		iSCSI session
 * @v string		iSCSI string (in "key=value" format)
 * @ret rc		Return status code
 */
static int iscsi_handle_string ( struct iscsi_session *iscsi,
				 const char *string ) {
	struct iscsi_string_type *type;
	size_t key_len;
	int rc;

	for ( type = iscsi_string_types ; type->key ; type++ ) {
		key_len = strlen ( type->key );
		if ( strncmp ( string, type->key, key_len ) != 0 )
			continue;
		DBGC ( iscsi, "iSCSI %p handling %s\n", iscsi, string );
		if ( ( rc = type->handle ( iscsi,
					   ( string + key_len ) ) ) != 0 ) {
			DBGC ( iscsi, "iSCSI %p could not handle %s: %s\n",
			       iscsi, string, strerror ( rc ) );
			return rc;
		}
		return 0;
	}
	DBGC ( iscsi, "iSCSI %p ignoring %s\n", iscsi, string );
	return 0;
}

/**
 * Handle iSCSI strings
 *
 * @v iscsi		iSCSI session
 * @v string		iSCSI string buffer
 * @v len		Length of string buffer
 * @ret rc		Return status code
 */
static int iscsi_handle_strings ( struct iscsi_session *iscsi,
				  const char *strings, size_t len ) {
	size_t string_len;
	int rc;

	/* Handle each string in turn, taking care not to overrun the
	 * data buffer in case of badly-terminated data.
	 */
	while ( 1 ) {
		string_len = ( strnlen ( strings, len ) + 1 );
		if ( string_len > len )
			break;
		if ( ( rc = iscsi_handle_string ( iscsi, strings ) ) != 0 )
			return rc;
		strings += string_len;
		len -= string_len;
	}
	return 0;
}

/**
 * Receive PDU data into buffer
 *
 * @v iscsi		iSCSI session
 * @v data		Data to receive
 * @v len		Length of data
 * @ret rc		Return status code
 *
 * This can be used when the RX PDU type handler wishes to buffer up
 * all received data and process the PDU as a single unit.  The caller
 * is repsonsible for calling iscsi_rx_buffered_data_done() after
 * processing the data.
 */
static int iscsi_rx_buffered_data ( struct iscsi_session *iscsi,
				    const void *data, size_t len ) {

	/* Allocate buffer on first call */
	if ( ! iscsi->rx_buffer ) {
		iscsi->rx_buffer = malloc ( iscsi->rx_len );
		if ( ! iscsi->rx_buffer )
			return -ENOMEM;
	}

	/* Copy data to buffer */
	assert ( ( iscsi->rx_offset + len ) <= iscsi->rx_len );
	memcpy ( ( iscsi->rx_buffer + iscsi->rx_offset ), data, len );

	return 0;
}

/**
 * Convert iSCSI response status to return status code
 *
 * @v status_class	iSCSI status class
 * @v status_detail	iSCSI status detail
 * @ret rc		Return status code
 */
static int iscsi_status_to_rc ( unsigned int status_class,
				unsigned int status_detail ) {
	switch ( status_class ) {
	case ISCSI_STATUS_INITIATOR_ERROR :
		switch ( status_detail ) {
		case ISCSI_STATUS_INITIATOR_ERROR_AUTHENTICATION :
			return -EACCES;
		case ISCSI_STATUS_INITIATOR_ERROR_AUTHORISATION :
			return -EPERM;
		case ISCSI_STATUS_INITIATOR_ERROR_NOT_FOUND :
		case ISCSI_STATUS_INITIATOR_ERROR_REMOVED :
			return -ENODEV;
		default :
			return -ENOTSUP;
		}
	case ISCSI_STATUS_TARGET_ERROR :
		return -EIO;
	default :
		return -EINVAL;
	}
}

/**
 * Receive data segment of an iSCSI login response PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * @ret rc		Return status code
 */
static int iscsi_rx_login_response ( struct iscsi_session *iscsi,
				     const void *data, size_t len,
				     size_t remaining ) {
	struct iscsi_bhs_login_response *response
		= &iscsi->rx_bhs.login_response;
	int rc;

	/* Buffer up the PDU data */
	if ( ( rc = iscsi_rx_buffered_data ( iscsi, data, len ) ) != 0 ) {
		DBGC ( iscsi, "iSCSI %p could not buffer login response: %s\n",
		       iscsi, strerror ( rc ) );
		return rc;
	}
	if ( remaining )
		return 0;

	/* Process string data and discard string buffer */
	if ( ( rc = iscsi_handle_strings ( iscsi, iscsi->rx_buffer,
					   iscsi->rx_len ) ) != 0 )
		return rc;
	iscsi_rx_buffered_data_done ( iscsi );

	/* Check for login redirection */
	if ( response->status_class == ISCSI_STATUS_REDIRECT ) {
		DBGC ( iscsi, "iSCSI %p redirecting to new server\n", iscsi );
		iscsi_close_connection ( iscsi, 0 );
		if ( ( rc = iscsi_open_connection ( iscsi ) ) != 0 ) {
			DBGC ( iscsi, "iSCSI %p could not redirect: %s\n ",
			       iscsi, strerror ( rc ) );
			return rc;
		}
		return 0;
	}

	/* Check for fatal errors */
	if ( response->status_class != 0 ) {
		DBGC ( iscsi, "iSCSI login failure: class %02x detail %02x\n",
		       response->status_class, response->status_detail );
		rc = iscsi_status_to_rc ( response->status_class,
					  response->status_detail );
		iscsi->instant_rc = rc;
		return rc;
	}

	/* Handle login transitions */
	if ( response->flags & ISCSI_LOGIN_FLAG_TRANSITION ) {
		iscsi->status &= ~( ISCSI_STATUS_PHASE_MASK |
				    ISCSI_STATUS_STRINGS_MASK );
		switch ( response->flags & ISCSI_LOGIN_NSG_MASK ) {
		case ISCSI_LOGIN_NSG_OPERATIONAL_NEGOTIATION:
			iscsi->status |=
				( ISCSI_STATUS_OPERATIONAL_NEGOTIATION_PHASE |
				  ISCSI_STATUS_STRINGS_OPERATIONAL );
			break;
		case ISCSI_LOGIN_NSG_FULL_FEATURE_PHASE:
			iscsi->status |= ISCSI_STATUS_FULL_FEATURE_PHASE;
			break;
		default:
			DBGC ( iscsi, "iSCSI %p got invalid response flags "
			       "%02x\n", iscsi, response->flags );
			return -EIO;
		}
	}

	/* Send next login request PDU if we haven't reached the full
	 * feature phase yet.
	 */
	if ( ( iscsi->status & ISCSI_STATUS_PHASE_MASK ) !=
	     ISCSI_STATUS_FULL_FEATURE_PHASE ) {
		iscsi_start_login ( iscsi );
		return 0;
	}

	/* Check that target authentication was successful (if required) */
	if ( ( iscsi->status & ISCSI_STATUS_AUTH_REVERSE_REQUIRED ) &&
	     ! ( iscsi->status & ISCSI_STATUS_AUTH_REVERSE_OK ) ) {
		DBGC ( iscsi, "iSCSI %p nefarious target tried to bypass "
		       "authentication\n", iscsi );
		return -EPROTO;
	}

	/* Reset retry count */
	iscsi->retry_count = 0;

	/* Record TSIH for future reference */
	iscsi->tsih = ntohl ( response->tsih );
	
	/* Send the actual SCSI command */
	iscsi_start_command ( iscsi );

	return 0;
}

/****************************************************************************
 *
 * iSCSI to socket interface
 *
 */

/**
 * Start up a new TX PDU
 *
 * @v iscsi		iSCSI session
 *
 * This initiates the process of sending a new PDU.  Only one PDU may
 * be in transit at any one time.
 */
static void iscsi_start_tx ( struct iscsi_session *iscsi ) {
	assert ( iscsi->tx_state == ISCSI_TX_IDLE );
	
	/* Initialise TX BHS */
	memset ( &iscsi->tx_bhs, 0, sizeof ( iscsi->tx_bhs ) );

	/* Flag TX engine to start transmitting */
	iscsi->tx_state = ISCSI_TX_BHS;
}

/**
 * Transmit nothing
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
static int iscsi_tx_nothing ( struct iscsi_session *iscsi __unused ) {
	return 0;
}

/**
 * Transmit basic header segment of an iSCSI PDU
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
static int iscsi_tx_bhs ( struct iscsi_session *iscsi ) {
	return xfer_deliver_raw ( &iscsi->socket,  &iscsi->tx_bhs,
				  sizeof ( iscsi->tx_bhs ) );
}

/**
 * Transmit data segment of an iSCSI PDU
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 * 
 * Handle transmission of part of a PDU data segment.  iscsi::tx_bhs
 * will be valid when this is called.
 */
static int iscsi_tx_data ( struct iscsi_session *iscsi ) {
	struct iscsi_bhs_common *common = &iscsi->tx_bhs.common;

	switch ( common->opcode & ISCSI_OPCODE_MASK ) {
	case ISCSI_OPCODE_DATA_OUT:
		return iscsi_tx_data_out ( iscsi );
	case ISCSI_OPCODE_LOGIN_REQUEST:
		return iscsi_tx_login_request ( iscsi );
	default:
		/* Nothing to send in other states */
		return 0;
	}
}

/**
 * Transmit data padding of an iSCSI PDU
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 * 
 * Handle transmission of any data padding in a PDU data segment.
 * iscsi::tx_bhs will be valid when this is called.
 */
static int iscsi_tx_data_padding ( struct iscsi_session *iscsi ) {
	static const char pad[] = { '\0', '\0', '\0' };
	struct iscsi_bhs_common *common = &iscsi->tx_bhs.common;
	size_t pad_len;
	
	pad_len = ISCSI_DATA_PAD_LEN ( common->lengths );
	if ( ! pad_len )
		return 0;

	return xfer_deliver_raw ( &iscsi->socket, pad, pad_len );
}

/**
 * Complete iSCSI PDU transmission
 *
 * @v iscsi		iSCSI session
 *
 * Called when a PDU has been completely transmitted and the TX state
 * machine is about to enter the idle state.  iscsi::tx_bhs will be
 * valid for the just-completed PDU when this is called.
 */
static void iscsi_tx_done ( struct iscsi_session *iscsi ) {
	struct iscsi_bhs_common *common = &iscsi->tx_bhs.common;

	switch ( common->opcode & ISCSI_OPCODE_MASK ) {
	case ISCSI_OPCODE_DATA_OUT:
		iscsi_data_out_done ( iscsi );
	case ISCSI_OPCODE_LOGIN_REQUEST:
		iscsi_login_request_done ( iscsi );
	default:
		/* No action */
		break;
	}
}

/**
 * Transmit iSCSI PDU
 *
 * @v iscsi		iSCSI session
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 * 
 * Constructs data to be sent for the current TX state
 */
static void iscsi_tx_step ( struct process *process ) {
	struct iscsi_session *iscsi =
		container_of ( process, struct iscsi_session, process );
	struct iscsi_bhs_common *common = &iscsi->tx_bhs.common;
	int ( * tx ) ( struct iscsi_session *iscsi );
	enum iscsi_tx_state next_state;
	size_t tx_len;
	int rc;

	/* Select fragment to transmit */
	while ( 1 ) {
		switch ( iscsi->tx_state ) {
		case ISCSI_TX_IDLE:
			/* Stop processing */
			return;
		case ISCSI_TX_BHS:
			tx = iscsi_tx_bhs;
			tx_len = sizeof ( iscsi->tx_bhs );
			next_state = ISCSI_TX_AHS;
			break;
		case ISCSI_TX_AHS:
			tx = iscsi_tx_nothing;
			tx_len = 0;
			next_state = ISCSI_TX_DATA;
			break;
		case ISCSI_TX_DATA:
			tx = iscsi_tx_data;
			tx_len = ISCSI_DATA_LEN ( common->lengths );
			next_state = ISCSI_TX_DATA_PADDING;
			break;
		case ISCSI_TX_DATA_PADDING:
			tx = iscsi_tx_data_padding;
			tx_len = ISCSI_DATA_PAD_LEN ( common->lengths );
			next_state = ISCSI_TX_IDLE;
			break;
		default:
			assert ( 0 );
			return;
		}

		/* Check for window availability, if needed */
		if ( tx_len && ( xfer_window ( &iscsi->socket ) == 0 ) ) {
			/* Cannot transmit at this point; stop processing */
			return;
		}

		/* Transmit data */
		if ( ( rc = tx ( iscsi ) ) != 0 ) {
			DBGC ( iscsi, "iSCSI %p could not transmit: %s\n",
			       iscsi, strerror ( rc ) );
			return;
		}

		/* Move to next state */
		iscsi->tx_state = next_state;
		if ( next_state == ISCSI_TX_IDLE )
			iscsi_tx_done ( iscsi );
	}
}

/**
 * Receive basic header segment of an iSCSI PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * @ret rc		Return status code
 *
 * This fills in iscsi::rx_bhs with the data from the BHS portion of
 * the received PDU.
 */
static int iscsi_rx_bhs ( struct iscsi_session *iscsi, const void *data,
			  size_t len, size_t remaining __unused ) {
	memcpy ( &iscsi->rx_bhs.bytes[iscsi->rx_offset], data, len );
	if ( ( iscsi->rx_offset + len ) >= sizeof ( iscsi->rx_bhs ) ) {
		DBGC2 ( iscsi, "iSCSI %p received PDU opcode %#x len %#x\n",
			iscsi, iscsi->rx_bhs.common.opcode,
			ISCSI_DATA_LEN ( iscsi->rx_bhs.common.lengths ) );
	}
	return 0;
}

/**
 * Discard portion of an iSCSI PDU.
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * @ret rc		Return status code
 *
 * This discards data from a portion of a received PDU.
 */
static int iscsi_rx_discard ( struct iscsi_session *iscsi __unused,
			      const void *data __unused, size_t len __unused,
			      size_t remaining __unused ) {
	/* Do nothing */
	return 0;
}

/**
 * Receive data segment of an iSCSI PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * @ret rc		Return status code
 *
 * Handle processing of part of a PDU data segment.  iscsi::rx_bhs
 * will be valid when this is called.
 */
static int iscsi_rx_data ( struct iscsi_session *iscsi, const void *data,
			   size_t len, size_t remaining ) {
	struct iscsi_bhs_common_response *response
		= &iscsi->rx_bhs.common_response;

	/* Update cmdsn and statsn */
	iscsi->cmdsn = ntohl ( response->expcmdsn );
	iscsi->statsn = ntohl ( response->statsn );

	switch ( response->opcode & ISCSI_OPCODE_MASK ) {
	case ISCSI_OPCODE_LOGIN_RESPONSE:
		return iscsi_rx_login_response ( iscsi, data, len, remaining );
	case ISCSI_OPCODE_SCSI_RESPONSE:
		return iscsi_rx_scsi_response ( iscsi, data, len, remaining );
	case ISCSI_OPCODE_DATA_IN:
		return iscsi_rx_data_in ( iscsi, data, len, remaining );
	case ISCSI_OPCODE_R2T:
		return iscsi_rx_r2t ( iscsi, data, len, remaining );
	default:
		if ( remaining )
			return 0;
		DBGC ( iscsi, "iSCSI %p unknown opcode %02x\n", iscsi,
		       response->opcode );
		return -ENOTSUP;
	}
}

/**
 * Receive new data
 *
 * @v socket		Transport layer interface
 * @v data		Received data
 * @v len		Length of received data
 * @ret rc		Return status code
 *
 * This handles received PDUs.  The receive strategy is to fill in
 * iscsi::rx_bhs with the contents of the BHS portion of the PDU,
 * throw away any AHS portion, and then process each part of the data
 * portion as it arrives.  The data processing routine therefore
 * always has a full copy of the BHS available, even for portions of
 * the data in different packets to the BHS.
 */
static int iscsi_socket_deliver_raw ( struct xfer_interface *socket,
				      const void *data, size_t len ) {
	struct iscsi_session *iscsi =
		container_of ( socket, struct iscsi_session, socket );
	struct iscsi_bhs_common *common = &iscsi->rx_bhs.common;
	int ( * rx ) ( struct iscsi_session *iscsi, const void *data,
		       size_t len, size_t remaining );
	enum iscsi_rx_state next_state;
	size_t frag_len;
	size_t remaining;
	int rc;

	while ( 1 ) {
		switch ( iscsi->rx_state ) {
		case ISCSI_RX_BHS:
			rx = iscsi_rx_bhs;
			iscsi->rx_len = sizeof ( iscsi->rx_bhs );
			next_state = ISCSI_RX_AHS;			
			break;
		case ISCSI_RX_AHS:
			rx = iscsi_rx_discard;
			iscsi->rx_len = 4 * ISCSI_AHS_LEN ( common->lengths );
			next_state = ISCSI_RX_DATA;
			break;
		case ISCSI_RX_DATA:
			rx = iscsi_rx_data;
			iscsi->rx_len = ISCSI_DATA_LEN ( common->lengths );
			next_state = ISCSI_RX_DATA_PADDING;
			break;
		case ISCSI_RX_DATA_PADDING:
			rx = iscsi_rx_discard;
			iscsi->rx_len = ISCSI_DATA_PAD_LEN ( common->lengths );
			next_state = ISCSI_RX_BHS;
			break;
		default:
			assert ( 0 );
			return -EINVAL;
		}

		frag_len = iscsi->rx_len - iscsi->rx_offset;
		if ( frag_len > len )
			frag_len = len;
		remaining = iscsi->rx_len - iscsi->rx_offset - frag_len;
		if ( ( rc = rx ( iscsi, data, frag_len, remaining ) ) != 0 ) {
			DBGC ( iscsi, "iSCSI %p could not process received "
			       "data: %s\n", iscsi, strerror ( rc ) );
			iscsi_close_connection ( iscsi, rc );
			iscsi_scsi_done ( iscsi, rc );
			return rc;
		}

		iscsi->rx_offset += frag_len;
		data += frag_len;
		len -= frag_len;

		/* If all the data for this state has not yet been
		 * received, stay in this state for now.
		 */
		if ( iscsi->rx_offset != iscsi->rx_len )
			return 0;

		iscsi->rx_state = next_state;
		iscsi->rx_offset = 0;
	}

	return 0;
}

/**
 * Handle stream connection closure
 *
 * @v socket		Transport layer interface
 * @v rc		Reason for close
 *
 */
static void iscsi_socket_close ( struct xfer_interface *socket, int rc ) {
	struct iscsi_session *iscsi =
		container_of ( socket, struct iscsi_session, socket );

	/* Even a graceful close counts as an error for iSCSI */
	if ( ! rc )
		rc = -ECONNRESET;

	/* Close session cleanly */
	iscsi_close_connection ( iscsi, rc );

	/* Retry connection if within the retry limit, otherwise fail */
	if ( ++iscsi->retry_count <= ISCSI_MAX_RETRIES ) {
		DBGC ( iscsi, "iSCSI %p retrying connection (retry #%d)\n",
		       iscsi, iscsi->retry_count );
		if ( ( rc = iscsi_open_connection ( iscsi ) ) != 0 ) {
			DBGC ( iscsi, "iSCSI %p could not reconnect: %s\n",
			       iscsi, strerror ( rc ) );
			iscsi_scsi_done ( iscsi, rc );
		}
	} else {
		DBGC ( iscsi, "iSCSI %p retry count exceeded\n", iscsi );
		iscsi->instant_rc = rc;
		iscsi_scsi_done ( iscsi, rc );
	}
}

/**
 * Handle redirection event
 *
 * @v socket		Transport layer interface
 * @v type		Location type
 * @v args		Remaining arguments depend upon location type
 * @ret rc		Return status code
 */
static int iscsi_vredirect ( struct xfer_interface *socket, int type,
			     va_list args ) {
	struct iscsi_session *iscsi =
		container_of ( socket, struct iscsi_session, socket );
	va_list tmp;
	struct sockaddr *peer;

	/* Intercept redirects to a LOCATION_SOCKET and record the IP
	 * address for the iBFT.  This is a bit of a hack, but avoids
	 * inventing an ioctl()-style call to retrieve the socket
	 * address from a data-xfer interface.
	 */
	if ( type == LOCATION_SOCKET ) {
		va_copy ( tmp, args );
		( void ) va_arg ( tmp, int ); /* Discard "semantics" */
		peer = va_arg ( tmp, struct sockaddr * );
		memcpy ( &iscsi->target_sockaddr, peer,
			 sizeof ( iscsi->target_sockaddr ) );
		va_end ( tmp );
	}

	return xfer_vreopen ( socket, type, args );
}
			     

/** iSCSI socket operations */
static struct xfer_interface_operations iscsi_socket_operations = {
	.close		= iscsi_socket_close,
	.vredirect	= iscsi_vredirect,
	.window		= unlimited_xfer_window,
	.alloc_iob	= default_xfer_alloc_iob,
	.deliver_iob	= xfer_deliver_as_raw,
	.deliver_raw	= iscsi_socket_deliver_raw,
};


/****************************************************************************
 *
 * iSCSI command issuing
 *
 */

/**
 * Issue SCSI command
 *
 * @v scsi		SCSI device
 * @v command		SCSI command
 * @ret rc		Return status code
 */
static int iscsi_command ( struct scsi_device *scsi,
			   struct scsi_command *command ) {
	struct iscsi_session *iscsi =
		container_of ( scsi->backend, struct iscsi_session, refcnt );
	int rc;

	/* Abort immediately if we have a recorded permanent failure */
	if ( iscsi->instant_rc )
		return iscsi->instant_rc;

	/* Record SCSI command */
	iscsi->command = command;

	/* Issue command or open connection as appropriate */
	if ( iscsi->status ) {
		iscsi_start_command ( iscsi );
	} else {
		if ( ( rc = iscsi_open_connection ( iscsi ) ) != 0 ) {
			iscsi->command = NULL;
			return rc;
		}
	}

	return 0;
}

/**
 * Shut down iSCSI interface
 *
 * @v scsi		SCSI device
 */
void iscsi_detach ( struct scsi_device *scsi ) {
	struct iscsi_session *iscsi =
		container_of ( scsi->backend, struct iscsi_session, refcnt );

	xfer_nullify ( &iscsi->socket );
	iscsi_close_connection ( iscsi, 0 );
	process_del ( &iscsi->process );
	scsi->command = scsi_detached_command;
	ref_put ( scsi->backend );
	scsi->backend = NULL;
}

/****************************************************************************
 *
 * Instantiator
 *
 */

/** iSCSI root path components (as per RFC4173) */
enum iscsi_root_path_component {
	RP_LITERAL = 0,
	RP_SERVERNAME,
	RP_PROTOCOL,
	RP_PORT,
	RP_LUN,
	RP_TARGETNAME,
	NUM_RP_COMPONENTS
};

/**
 * Parse iSCSI root path
 *
 * @v iscsi		iSCSI session
 * @v root_path		iSCSI root path (as per RFC4173)
 * @ret rc		Return status code
 */
static int iscsi_parse_root_path ( struct iscsi_session *iscsi,
				   const char *root_path ) {
	char rp_copy[ strlen ( root_path ) + 1 ];
	char *rp_comp[NUM_RP_COMPONENTS];
	char *rp = rp_copy;
	int i = 0;
	int rc;

	/* Split root path into component parts */
	strcpy ( rp_copy, root_path );
	while ( 1 ) {
		rp_comp[i++] = rp;
		if ( i == NUM_RP_COMPONENTS )
			break;
		for ( ; *rp != ':' ; rp++ ) {
			if ( ! *rp ) {
				DBGC ( iscsi, "iSCSI %p root path \"%s\" "
				       "too short\n", iscsi, root_path );
				return -EINVAL;
			}
		}
		*(rp++) = '\0';
	}

	/* Use root path components to configure iSCSI session */
	iscsi->target_address = strdup ( rp_comp[RP_SERVERNAME] );
	if ( ! iscsi->target_address )
		return -ENOMEM;
	iscsi->target_port = strtoul ( rp_comp[RP_PORT], NULL, 10 );
	if ( ! iscsi->target_port )
		iscsi->target_port = ISCSI_PORT;
	if ( ( rc = scsi_parse_lun ( rp_comp[RP_LUN], &iscsi->lun ) ) != 0 ) {
		DBGC ( iscsi, "iSCSI %p invalid LUN \"%s\"\n",
		       iscsi, rp_comp[RP_LUN] );
		return rc;
	}
	iscsi->target_iqn = strdup ( rp_comp[RP_TARGETNAME] );
	if ( ! iscsi->target_iqn )
		return -ENOMEM;

	return 0;
}

/**
 * Set iSCSI authentication details
 *
 * @v iscsi		iSCSI session
 * @v initiator_username Initiator username, if any
 * @v initiator_password Initiator password, if any
 * @v target_username	Target username, if any
 * @v target_password	Target password, if any
 * @ret rc		Return status code
 */
static int iscsi_set_auth ( struct iscsi_session *iscsi,
			    const char *initiator_username,
			    const char *initiator_password,
			    const char *target_username,
			    const char *target_password ) {

	/* Check for initiator or target credentials */
	if ( initiator_username || initiator_password ||
	     target_username || target_password ) {

		/* We must have at least an initiator username+password */
		if ( ! ( initiator_username && initiator_password ) )
			goto invalid_auth;

		/* Store initiator credentials */
		iscsi->initiator_username = strdup ( initiator_username );
		if ( ! iscsi->initiator_username )
			return -ENOMEM;
		iscsi->initiator_password = strdup ( initiator_password );
		if ( ! iscsi->initiator_password )
			return -ENOMEM;

		/* Check for target credentials */
		if ( target_username || target_password ) {

			/* We must have target username+password */
			if ( ! ( target_username && target_password ) )
				goto invalid_auth;

			/* Store target credentials */
			iscsi->target_username = strdup ( target_username );
			if ( ! iscsi->target_username )
				return -ENOMEM;
			iscsi->target_password = strdup ( target_password );
			if ( ! iscsi->target_password )
				return -ENOMEM;
		}
	}

	return 0;

 invalid_auth:
	DBGC ( iscsi, "iSCSI %p invalid credentials: initiator "
	       "%sname,%spw, target %sname,%spw\n", iscsi,
	       ( initiator_username ? "" : "no " ),
	       ( initiator_password ? "" : "no " ),
	       ( target_username ? "" : "no " ),
	       ( target_password ? "" : "no " ) );
	return -EINVAL;
}

/**
 * Attach iSCSI interface
 *
 * @v scsi		SCSI device
 * @v root_path		iSCSI root path (as per RFC4173)
 * @ret rc		Return status code
 */
int iscsi_attach ( struct scsi_device *scsi, const char *root_path ) {
	struct iscsi_session *iscsi;
	int rc;

	/* Allocate and initialise structure */
	iscsi = zalloc ( sizeof ( *iscsi ) );
	if ( ! iscsi )
		return -ENOMEM;
	iscsi->refcnt.free = iscsi_free;
	xfer_init ( &iscsi->socket, &iscsi_socket_operations, &iscsi->refcnt );
	process_init ( &iscsi->process, iscsi_tx_step, &iscsi->refcnt );

	/* Parse root path */
	if ( ( rc = iscsi_parse_root_path ( iscsi, root_path ) ) != 0 )
		goto err;
	/* Set fields not specified by root path */
	if ( ( rc = iscsi_set_auth ( iscsi,
				     iscsi_initiator_username,
				     iscsi_initiator_password,
				     iscsi_target_username,
				     iscsi_target_password ) ) != 0 )
		goto err;

	/* Sanity checks */
	if ( ! iscsi->target_address ) {
		DBGC ( iscsi, "iSCSI %p does not yet support discovery\n",
		       iscsi );
		rc = -ENOTSUP;
		goto err;
	}
	if ( ! iscsi->target_iqn ) {
		DBGC ( iscsi, "iSCSI %p no target address supplied in %s\n",
		       iscsi, root_path );
		rc = -EINVAL;
		goto err;
	}

	/* Attach parent interface, mortalise self, and return */
	scsi->backend = ref_get ( &iscsi->refcnt );
	scsi->command = iscsi_command;
	ref_put ( &iscsi->refcnt );
	return 0;
	
 err:
	ref_put ( &iscsi->refcnt );
	return rc;
}

/****************************************************************************
 *
 * Settings
 *
 */

/** iSCSI initiator IQN setting */
struct setting initiator_iqn_setting __setting = {
	.name = "initiator-iqn",
	.description = "iSCSI initiator name",
	.tag = DHCP_ISCSI_INITIATOR_IQN,
	.type = &setting_type_string,
};

/** iSCSI reverse username setting */
struct setting reverse_username_setting __setting = {
	.name = "reverse-username",
	.description = "Reverse user name",
	.tag = DHCP_EB_REVERSE_USERNAME,
	.type = &setting_type_string,
};

/** iSCSI reverse password setting */
struct setting reverse_password_setting __setting = {
	.name = "reverse-password",
	.description = "Reverse password",
	.tag = DHCP_EB_REVERSE_PASSWORD,
	.type = &setting_type_string,
};

/** An iSCSI string setting */
struct iscsi_string_setting {
	/** Setting */
	struct setting *setting;
	/** String to update */
	char **string;
	/** String prefix */
	const char *prefix;
};

/** iSCSI string settings */
static struct iscsi_string_setting iscsi_string_settings[] = {
	{
		.setting = &initiator_iqn_setting,
		.string = &iscsi_explicit_initiator_iqn,
		.prefix = "",
	},
	{
		.setting = &username_setting,
		.string = &iscsi_initiator_username,
		.prefix = "",
	},
	{
		.setting = &password_setting,
		.string = &iscsi_initiator_password,
		.prefix = "",
	},
	{
		.setting = &reverse_username_setting,
		.string = &iscsi_target_username,
		.prefix = "",
	},
	{
		.setting = &reverse_password_setting,
		.string = &iscsi_target_password,
		.prefix = "",
	},
	{
		.setting = &hostname_setting,
		.string = &iscsi_default_initiator_iqn,
		.prefix = "iqn.2000-01.org.etherboot:",
	},
};

/**
 * Apply iSCSI setting
 *
 * @v setting		iSCSI string setting
 * @ret rc		Return status code
 */
static int apply_iscsi_string_setting ( struct iscsi_string_setting *setting ){
	size_t prefix_len;
	int setting_len;
	size_t len;
	int check_len;
	char *p;

	/* Free old string */
	free ( *setting->string );
	*setting->string = NULL;

	/* Allocate new string */
	prefix_len = strlen ( setting->prefix );
	setting_len = fetch_setting_len ( NULL, setting->setting );
	if ( setting_len < 0 ) {
		/* Missing settings are not errors; leave strings as NULL */
		return 0;
	}
	len = ( prefix_len + setting_len + 1 );
	p = *setting->string = malloc ( len );
	if ( ! p )
		return -ENOMEM;

	/* Fill new string */
	strcpy ( p, setting->prefix );
	check_len = fetch_string_setting ( NULL, setting->setting,
					   ( p + prefix_len ),
					   ( len - prefix_len ) );
	assert ( check_len == setting_len );

	return 0;
}

/**
 * Apply iSCSI settings
 *
 * @ret rc		Return status code
 */
static int apply_iscsi_settings ( void ) {
	struct iscsi_string_setting *setting;
	unsigned int i;
	int rc;

	for ( i = 0 ; i < ( sizeof ( iscsi_string_settings ) /
			    sizeof ( iscsi_string_settings[0] ) ) ; i++ ) {
		setting = &iscsi_string_settings[i];
		if ( ( rc = apply_iscsi_string_setting ( setting ) ) != 0 ) {
			DBG ( "iSCSI could not apply setting %s\n",
			      setting->setting->name );
			return rc;
		}
	}

	return 0;
}

/** iSCSI settings applicator */
struct settings_applicator iscsi_settings_applicator __settings_applicator = {
	.apply = apply_iscsi_settings,
};

/****************************************************************************
 *
 * Initiator name
 *
 */

/**
 * Get iSCSI initiator IQN
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
const char * iscsi_initiator_iqn ( void ) {

	if ( iscsi_explicit_initiator_iqn )
		return iscsi_explicit_initiator_iqn;
	if ( iscsi_default_initiator_iqn )
		return iscsi_default_initiator_iqn;
	return "iqn.2000-09.org.etherboot:UNKNOWN";
}
