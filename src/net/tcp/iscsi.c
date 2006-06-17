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

#include <stddef.h>
#include <string.h>
#include <vsprintf.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <gpxe/scsi.h>
#include <gpxe/process.h>
#include <gpxe/uaccess.h>
#include <gpxe/iscsi.h>

/** @file
 *
 * iSCSI protocol
 *
 */

static void iscsi_start_tx ( struct iscsi_session *iscsi );
static void iscsi_start_data_out ( struct iscsi_session *iscsi,
				   unsigned int datasn );

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
}

/**
 * Receive data segment of an iSCSI SCSI response PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * 
 */
static void iscsi_rx_scsi_response ( struct iscsi_session *iscsi, void *data,
				     size_t len, size_t remaining ) {
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
		return;
	
	/* Record SCSI status code */
	iscsi->command->status = response->status;

	/* Mark as completed, with error if applicable */
	iscsi->status |= ISCSI_STATUS_DONE;
	if ( response->response != ISCSI_RESPONSE_COMMAND_COMPLETE )
		iscsi->status |= ISCSI_STATUS_ERR;
}

/**
 * Receive data segment of an iSCSI data-in PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * 
 */
static void iscsi_rx_data_in ( struct iscsi_session *iscsi, void *data,
			       size_t len, size_t remaining __unused ) {
	struct iscsi_bhs_data_in *data_in = &iscsi->rx_bhs.data_in;
	unsigned long offset;

	/* Copy data to data-in buffer */
	offset = ntohl ( data_in->offset ) + iscsi->rx_offset;
	assert ( iscsi->command != NULL );
	assert ( iscsi->command->data_in != NULL );
	assert ( ( offset + len ) <= iscsi->command->data_in_len );
	copy_to_user ( iscsi->command->data_in, offset, data, len );

	/* Record SCSI status, if present */
	if ( data_in->flags & ISCSI_DATA_FLAG_STATUS )
		iscsi->command->status = data_in->status;

	/* If this is the end, flag as complete */
	if ( ( offset + len ) == iscsi->command->data_in_len ) {
		assert ( data_in->flags & ISCSI_FLAG_FINAL );
		assert ( remaining == 0 );
		iscsi->status |= ISCSI_STATUS_DONE;
	}
}

/**
 * Receive data segment of an iSCSI R2T PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * 
 */
static void iscsi_rx_r2t ( struct iscsi_session *iscsi, void *data __unused,
			   size_t len __unused, size_t remaining __unused ) {
	struct iscsi_bhs_r2t *r2t = &iscsi->rx_bhs.r2t;

	/* Record transfer parameters and trigger first data-out */
	iscsi->ttt = ntohl ( r2t->ttt );
	iscsi->transfer_offset = ntohl ( r2t->offset );
	iscsi->transfer_len = ntohl ( r2t->len );
	iscsi_start_data_out ( iscsi, 0 );
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
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 */
static void iscsi_tx_data_out ( struct iscsi_session *iscsi,
				void *buf, size_t len ) {
	struct iscsi_bhs_data_out *data_out = &iscsi->tx_bhs.data_out;
	unsigned long offset;
	unsigned long remaining;

	offset = ( iscsi->transfer_offset + ntohl ( data_out->offset ) +
		   iscsi->tx_offset );
	remaining = ( ISCSI_DATA_LEN ( data_out->lengths ) - iscsi->tx_offset);
	assert ( iscsi->command != NULL );
	assert ( iscsi->command->data_out != NULL );
	assert ( ( offset + len ) <= iscsi->command->data_out_len );
	
	if ( remaining < len )
		len = remaining;
	copy_from_user ( buf, iscsi->command->data_out, offset, len );

	tcp_send ( &iscsi->tcp, buf, len );
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
 *     MaxRecvDataSegmentLength=8192 (default; we don't care)
 *     MaxBurstLength=262144 (default; we don't care)
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
 */
static int iscsi_build_login_request_strings ( struct iscsi_session *iscsi,
					       void *data, size_t len ) {
	return snprintf ( data, len,
			  "InitiatorName=%s%c"
			  "TargetName=%s%c"
			  "SessionType=Normal%c"
			  "HeaderDigest=None%c"
			  "DataDigest=None%c"
			  "InitialR2T=Yes%c"
			  "DefaultTime2Wait=0%c"
			  "DefaultTime2Retain=0%c"
			  "MaxOutstandingR2T=1%c"
			  "DataPDUInOrder=Yes%c"
			  "DataSequenceInOrder=Yes%c"
			  "ErrorRecoveryLevel=0%c",
			  iscsi->initiator, 0, iscsi->target, 0,
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
}

/**
 * Build iSCSI login request BHS
 *
 * @v iscsi		iSCSI session
 * @v first		Login request is the first in a sequence
 */
static void iscsi_start_login ( struct iscsi_session *iscsi, int first ) {
	struct iscsi_bhs_login_request *request = &iscsi->tx_bhs.login_request;
	int len;

	/* Construct BHS and initiate transmission */
	iscsi_start_tx ( iscsi );
	request->opcode = ( ISCSI_OPCODE_LOGIN_REQUEST |
			    ISCSI_FLAG_IMMEDIATE );
	request->flags = ( ISCSI_LOGIN_FLAG_TRANSITION |
			   ISCSI_LOGIN_CSG_OPERATIONAL_NEGOTIATION |
			   ISCSI_LOGIN_NSG_FULL_FEATURE_PHASE );
	/* version_max and version_min left as zero */
	if ( first ) {
		len = iscsi_build_login_request_strings ( iscsi, NULL, 0 );
		ISCSI_SET_LENGTHS ( request->lengths, 0, len );
	}
	request->isid_iana_en = htonl ( ISCSI_ISID_IANA |
					IANA_EN_FEN_SYSTEMS );
	/* isid_iana_qual left as zero */
	request->tsih = htons ( iscsi->tsih );
	if ( first )
		iscsi->itt++;
	request->itt = htonl ( iscsi->itt );
	/* cid left as zero */
	request->cmdsn = htonl ( iscsi->cmdsn );
	request->expstatsn = htonl ( iscsi->statsn + 1 );
}

/**
 * Transmit data segment of an iSCSI login request PDU
 *
 * @v iscsi		iSCSI session
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 *
 * For login requests, the data segment consists of the login strings.
 */
static void iscsi_tx_login_request ( struct iscsi_session *iscsi,
				     void *buf, size_t len ) {
	len = iscsi_build_login_request_strings ( iscsi, buf, len );
	tcp_send ( &iscsi->tcp, buf + iscsi->tx_offset,
		   len - iscsi->tx_offset );
}

/**
 * Receive data segment of an iSCSI login response PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * 
 */
static void iscsi_rx_login_response ( struct iscsi_session *iscsi,
				      void *data __unused,
				      size_t len __unused,
				      size_t remaining __unused ) {
	struct iscsi_bhs_login_response *response
		= &iscsi->rx_bhs.login_response;

	/* Check for fatal errors */
	if ( response->status_class != 0 ) {
		printf ( "iSCSI login failure: class %02x detail %02x\n",
			 response->status_class, response->status_detail );
		iscsi->status |= ( ISCSI_STATUS_DONE | ISCSI_STATUS_ERR );
		tcp_close ( &iscsi->tcp );
		return;
	}

	/* If server did not transition, send back another login
	 * request without any login strings.
	 */
	if ( ! ( response->flags & ISCSI_LOGIN_FLAG_TRANSITION ) ) {
		iscsi_start_login ( iscsi, 0 );
		return;
	}

	/* Record TSIH for future reference */
	iscsi->tsih = ntohl ( response->tsih );
	
	/* Send the SCSI command */
	iscsi_start_command ( iscsi );
}

/****************************************************************************
 *
 * iSCSI to TCP interface
 *
 */

static inline struct iscsi_session *
tcp_to_iscsi ( struct tcp_connection *conn ) {
	return container_of ( conn, struct iscsi_session, tcp );
}

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
	iscsi->tx_offset = 0;
}

/**
 * Transmit data segment of an iSCSI PDU
 *
 * @v iscsi		iSCSI session
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 * 
 * Handle transmission of part of a PDU data segment.  iscsi::tx_bhs
 * will be valid when this is called.
 */
static void iscsi_tx_data ( struct iscsi_session *iscsi,
			    void *buf, size_t len ) {
	struct iscsi_bhs_common *common = &iscsi->tx_bhs.common;

	switch ( common->opcode & ISCSI_OPCODE_MASK ) {
	case ISCSI_OPCODE_DATA_OUT:
		iscsi_tx_data_out ( iscsi, buf, len );
		break;
	case ISCSI_OPCODE_LOGIN_REQUEST:
		iscsi_tx_login_request ( iscsi, buf, len );
		break;
	default:
		assert ( 0 );
		break;
	}
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
	default:
		/* No action */
		break;
	}
}

/**
 * Handle TCP ACKs
 *
 * @v iscsi		iSCSI session
 * 
 * Updates iscsi->tx_offset and, if applicable, transitions to the
 * next TX state.
 */
static void iscsi_acked ( struct tcp_connection *conn, size_t len ) {
	struct iscsi_session *iscsi = tcp_to_iscsi ( conn );
	struct iscsi_bhs_common *common = &iscsi->tx_bhs.common;
	size_t max_tx_offset;
	enum iscsi_tx_state next_state;
	
	iscsi->tx_offset += len;
	while ( 1 ) {
		switch ( iscsi->tx_state ) {
		case ISCSI_TX_BHS:
			max_tx_offset = sizeof ( iscsi->tx_bhs );
			next_state = ISCSI_TX_AHS;
			break;
		case ISCSI_TX_AHS:
			max_tx_offset = 4 * ISCSI_AHS_LEN ( common->lengths );
			next_state = ISCSI_TX_DATA;
			break;
		case ISCSI_TX_DATA:
			max_tx_offset = ISCSI_DATA_LEN ( common->lengths );
			next_state = ISCSI_TX_DATA_PADDING;
			break;
		case ISCSI_TX_DATA_PADDING:
			max_tx_offset = ISCSI_DATA_PAD_LEN ( common->lengths );
			next_state = ISCSI_TX_IDLE;
			break;
		case ISCSI_TX_IDLE:
			return;
		default:
			assert ( 0 );
			return;
		}
		assert ( iscsi->tx_offset <= max_tx_offset );

		/* If the whole of the current portion has not yet
		 * been acked, stay in this state for now.
		 */
		if ( iscsi->tx_offset != max_tx_offset )
			return;

		/* Move to next state.  Call iscsi_tx_done() when PDU
		 * transmission is complete.
		 */
		iscsi->tx_state = next_state;
		iscsi->tx_offset = 0;
		if ( next_state == ISCSI_TX_IDLE )
			iscsi_tx_done ( iscsi );
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
static void iscsi_senddata ( struct tcp_connection *conn,
			     void *buf, size_t len ) {
	struct iscsi_session *iscsi = tcp_to_iscsi ( conn );
	struct iscsi_bhs_common *common = &iscsi->tx_bhs.common;
	static const char pad[] = { '\0', '\0', '\0' };

	switch ( iscsi->tx_state ) {
	case ISCSI_TX_IDLE:
		/* Nothing to send */
		break;
	case ISCSI_TX_BHS:
		tcp_send ( conn, &iscsi->tx_bhs.bytes[iscsi->tx_offset],
			   ( sizeof ( iscsi->tx_bhs ) - iscsi->tx_offset ) );
		break;
	case ISCSI_TX_AHS:
		/* We don't yet have an AHS transmission mechanism */
		assert ( 0 );
		break;
	case ISCSI_TX_DATA:
		iscsi_tx_data ( iscsi, buf, len );
		break;
	case ISCSI_TX_DATA_PADDING:
		tcp_send ( conn, pad, ( ISCSI_DATA_PAD_LEN ( common->lengths )
					- iscsi->tx_offset ) );
		break;
	default:
		assert ( 0 );
		break;
	}
}

/**
 * Receive data segment of an iSCSI PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 *
 * Handle processing of part of a PDU data segment.  iscsi::rx_bhs
 * will be valid when this is called.
 */
static void iscsi_rx_data ( struct iscsi_session *iscsi, void *data,
			    size_t len, size_t remaining ) {
	struct iscsi_bhs_common_response *response
		= &iscsi->rx_bhs.common_response;

	/* Update cmdsn and statsn */
	iscsi->cmdsn = ntohl ( response->expcmdsn );
	iscsi->statsn = ntohl ( response->statsn );

	switch ( response->opcode & ISCSI_OPCODE_MASK ) {
	case ISCSI_OPCODE_LOGIN_RESPONSE:
		iscsi_rx_login_response ( iscsi, data, len, remaining );
		break;
	case ISCSI_OPCODE_SCSI_RESPONSE:
		iscsi_rx_scsi_response ( iscsi, data, len, remaining );
		break;
	case ISCSI_OPCODE_DATA_IN:
		iscsi_rx_data_in ( iscsi, data, len, remaining );
		break;
	case ISCSI_OPCODE_R2T:
		iscsi_rx_r2t ( iscsi, data, len, remaining );
		break;
	default:
		printf ( "Unknown iSCSI opcode %02x\n", response->opcode );
		iscsi->status |= ( ISCSI_STATUS_DONE | ISCSI_STATUS_ERR );
		break;
	}
}

/**
 * Discard portion of an iSCSI PDU.
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 *
 * This discards data from a portion of a received PDU.
 */
static void iscsi_rx_discard ( struct iscsi_session *iscsi __unused,
			       void *data __unused, size_t len __unused,
			       size_t remaining __unused ) {
	/* Do nothing */
}

/**
 * Receive basic header segment of an iSCSI PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 *
 * This fills in iscsi::rx_bhs with the data from the BHS portion of
 * the received PDU.
 */
static void iscsi_rx_bhs ( struct iscsi_session *iscsi, void *data,
			   size_t len, size_t remaining __unused ) {
	memcpy ( &iscsi->rx_bhs.bytes[iscsi->rx_offset], data, len );
}

/**
 * Receive new data
 *
 * @v tcp		TCP connection
 * @v data		Received data
 * @v len		Length of received data
 *
 * This handles received PDUs.  The receive strategy is to fill in
 * iscsi::rx_bhs with the contents of the BHS portion of the PDU,
 * throw away any AHS portion, and then process each part of the data
 * portion as it arrives.  The data processing routine therefore
 * always has a full copy of the BHS available, even for portions of
 * the data in different packets to the BHS.
 */
static void iscsi_newdata ( struct tcp_connection *conn, void *data,
			    size_t len ) {
	struct iscsi_session *iscsi = tcp_to_iscsi ( conn );
	struct iscsi_bhs_common *common = &iscsi->rx_bhs.common;
	void ( *process ) ( struct iscsi_session *iscsi, void *data,
			    size_t len, size_t remaining );
	size_t max_rx_offset;
	enum iscsi_rx_state next_state;
	size_t frag_len;
	size_t remaining;

	while ( 1 ) {
		switch ( iscsi->rx_state ) {
		case ISCSI_RX_BHS:
			process = iscsi_rx_bhs;
			max_rx_offset = sizeof ( iscsi->rx_bhs );
			next_state = ISCSI_RX_AHS;			
			break;
		case ISCSI_RX_AHS:
			process = iscsi_rx_discard;
			max_rx_offset = 4 * ISCSI_AHS_LEN ( common->lengths );
			next_state = ISCSI_RX_DATA;
			break;
		case ISCSI_RX_DATA:
			process = iscsi_rx_data;
			max_rx_offset = ISCSI_DATA_LEN ( common->lengths );
			next_state = ISCSI_RX_DATA_PADDING;
			break;
		case ISCSI_RX_DATA_PADDING:
			process = iscsi_rx_discard;
			max_rx_offset = ISCSI_DATA_PAD_LEN ( common->lengths );
			next_state = ISCSI_RX_BHS;
			break;
		default:
			assert ( 0 );
			return;
		}

		frag_len = max_rx_offset - iscsi->rx_offset;
		if ( frag_len > len )
			frag_len = len;
		remaining = max_rx_offset - iscsi->rx_offset - frag_len;
		process ( iscsi, data, frag_len, remaining );

		iscsi->rx_offset += frag_len;
		data += frag_len;
		len -= frag_len;

		/* If all the data for this state has not yet been
		 * received, stay in this state for now.
		 */
		if ( iscsi->rx_offset != max_rx_offset )
			return;

		iscsi->rx_state = next_state;
		iscsi->rx_offset = 0;
	}
}

/**
 * Handle TCP connection closure
 *
 * @v conn		TCP connection
 * @v status		Error code, if any
 *
 */
static void iscsi_closed ( struct tcp_connection *conn, int status __unused ) {
	struct iscsi_session *iscsi = tcp_to_iscsi ( conn );

	/* Clear connected flag */
	iscsi->status &= ~ISCSI_STATUS_CONNECTED;

	/* Retry connection if within the retry limit, otherwise fail */
	if ( ++iscsi->retry_count <= ISCSI_MAX_RETRIES ) {
		tcp_connect ( conn );
	} else {
		printf ( "iSCSI retry count exceeded\n" );
		iscsi->status |= ( ISCSI_STATUS_DONE | ISCSI_STATUS_ERR );
	}
}

/**
 * Handle TCP connection opening
 *
 * @v conn		TCP connection
 *
 */
static void iscsi_connected ( struct tcp_connection *conn ) {
	struct iscsi_session *iscsi = tcp_to_iscsi ( conn );

	/* Set connected flag and reset retry count */
	iscsi->status |= ISCSI_STATUS_CONNECTED;
	iscsi->retry_count = 0;

	/* Prepare to receive PDUs. */
	iscsi->rx_state = ISCSI_RX_BHS;
	iscsi->rx_offset = 0;

	/* Start logging in */
	iscsi_start_login ( iscsi, 1 );
}

/** iSCSI TCP operations */
static struct tcp_operations iscsi_tcp_operations = {
	.closed		= iscsi_closed,
	.connected	= iscsi_connected,
	.acked		= iscsi_acked,
	.newdata	= iscsi_newdata,
	.senddata	= iscsi_senddata,
};

/**
 * Issue SCSI command via iSCSI session
 *
 * @v iscsi		iSCSI session
 * @v command		SCSI command
 * @ret rc		Return status code
 */
int iscsi_issue ( struct iscsi_session *iscsi,
		  struct scsi_command *command ) {
	iscsi->command = command;
	iscsi->status &= ~( ISCSI_STATUS_DONE | ISCSI_STATUS_ERR );

	if ( iscsi->status & ISCSI_STATUS_CONNECTED ) {
		iscsi_start_command ( iscsi );
		tcp_kick ( &iscsi->tcp );
	} else {
		iscsi->tcp.tcp_op = &iscsi_tcp_operations;
		tcp_connect ( &iscsi->tcp );
	}

	while ( ! ( iscsi->status & ISCSI_STATUS_DONE ) ) {
		step();
	}

	iscsi->command = NULL;

	return ( ( iscsi->status & ISCSI_STATUS_ERR ) ? -EIO : 0 );	
}
