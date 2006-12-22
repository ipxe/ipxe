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
#include <stdlib.h>
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
 * Finish receiving PDU data into buffer
 *
 * @v iscsi		iSCSI session
 */
static void iscsi_rx_buffered_data_done ( struct iscsi_session *iscsi ) {
	free ( iscsi->rx_buffer );
	iscsi->rx_buffer = NULL;
}

/**
 * Mark iSCSI operation as complete
 *
 * @v iscsi		iSCSI session
 * @v rc		Return status code
 *
 * Note that iscsi_done() will not close the connection, and must
 * therefore be called only when the internal state machines are in an
 * appropriate state, otherwise bad things may happen on the next call
 * to iscsi_issue().  The general rule is to call iscsi_done() only at
 * the end of receiving a PDU; at this point the TX and RX engines
 * should both be idle.
 */
static void iscsi_done ( struct iscsi_session *iscsi, int rc ) {

	/* Clear current SCSI command */
	iscsi->command = NULL;

	/* Free any dynamically allocated memory */
	chap_finish ( &iscsi->chap );
	iscsi_rx_buffered_data_done ( iscsi );

	/* Mark asynchronous operation as complete */
	async_done ( &iscsi->aop, rc );
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
	DBG ( "iSCSI %p start " SCSI_CDB_FORMAT " %s %#x\n",
	      iscsi, SCSI_CDB_DATA ( command->cdb ),
	      ( iscsi->command->data_in ? "in" : "out" ),
	      ( iscsi->command->data_in ?
		iscsi->command->data_in_len : iscsi->command->data_out_len ) );
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
	if ( response->response == ISCSI_RESPONSE_COMMAND_COMPLETE ) {
		iscsi_done ( iscsi, 0 );
	} else {
		iscsi_done ( iscsi, -EIO );
	}
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
	assert ( iscsi->command->data_in );
	assert ( ( offset + len ) <= iscsi->command->data_in_len );
	copy_to_user ( iscsi->command->data_in, offset, data, len );

	/* Record SCSI status, if present */
	if ( data_in->flags & ISCSI_DATA_FLAG_STATUS )
		iscsi->command->status = data_in->status;

	/* If this is the end, flag as complete */
	if ( ( offset + len ) == iscsi->command->data_in_len ) {
		assert ( data_in->flags & ISCSI_FLAG_FINAL );
		assert ( remaining == 0 );
		iscsi_done ( iscsi, 0 );
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
	DBG ( "iSCSI %p start data out DataSN %#x len %#lx\n",
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
	remaining = ( iscsi->tx_len - iscsi->tx_offset );
	assert ( iscsi->command != NULL );
	assert ( iscsi->command->data_out );
	assert ( ( offset + remaining ) <= iscsi->command->data_out_len );
	
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
 * Version of snprintf() that accepts a signed buffer size
 *
 * @v buf		Buffer into which to write the string
 * @v size		Size of buffer
 * @v fmt		Format string
 * @v args		Arguments corresponding to the format string
 * @ret len		Length of formatted string
 *
 * This is a utility function for iscsi_build_login_request_strings().
 */
static int ssnprintf ( char *buf, ssize_t ssize, const char *fmt, ... ) {
	va_list args;
	int len;

	/* Treat negative buffer size as zero buffer size */
	if ( ssize < 0 )
		ssize = 0;

	/* Hand off to vsnprintf */
	va_start ( args, fmt );
	len = vsnprintf ( buf, ssize, fmt, args );
	va_end ( args );
	return len;
}

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
	unsigned int used = 0;
	unsigned int i;

	if ( iscsi->status & ISCSI_STATUS_STRINGS_SECURITY ) {
		used += ssnprintf ( data + used, len - used,
				    "InitiatorName=%s%c"
				    "TargetName=%s%c"
				    "SessionType=Normal%c"
				    "AuthMethod=CHAP,None%c",
				    iscsi->initiator_iqn, 0,
				    iscsi->target_iqn, 0, 0, 0 );
	}

	if ( iscsi->status & ISCSI_STATUS_STRINGS_CHAP_ALGORITHM ) {
		used += ssnprintf ( data + used, len - used, "CHAP_A=5%c", 0 );
	}
	
	if ( ( iscsi->status & ISCSI_STATUS_STRINGS_CHAP_RESPONSE ) &&
	     iscsi->username ) {
		used += ssnprintf ( data + used, len - used,
				    "CHAP_N=%s%cCHAP_R=0x",
				    iscsi->username, 0 );
		for ( i = 0 ; i < iscsi->chap.response_len ; i++ ) {
			used += ssnprintf ( data + used, len - used, "%02x",
					    iscsi->chap.response[i] );
		}
		used += ssnprintf ( data + used, len - used, "%c", 0 );
	}

	if ( iscsi->status & ISCSI_STATUS_STRINGS_OPERATIONAL ) {
		used += ssnprintf ( data + used, len - used,
				    "HeaderDigest=None%c"
				    "DataDigest=None%c"
				    "InitialR2T=Yes%c"
				    "DefaultTime2Wait=0%c"
				    "DefaultTime2Retain=0%c"
				    "MaxOutstandingR2T=1%c"
				    "DataPDUInOrder=Yes%c"
				    "DataSequenceInOrder=Yes%c"
				    "ErrorRecoveryLevel=0%c",
				    0, 0, 0, 0, 0, 0, 0, 0, 0 );
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
 * Handle iSCSI TargetAddress text value
 *
 * @v iscsi		iSCSI session
 * @v value		TargetAddress value
 */
static void iscsi_handle_targetaddress_value ( struct iscsi_session *iscsi,
					       const char *value ) {
	struct in_addr address;
	struct sockaddr_in *sin = ( struct sockaddr_in * ) &iscsi->target;

	if ( inet_aton ( value, &address ) == 0 ) {
		DBG ( "iSCSI %p received invalid TargetAddress \"%s\"\n",
		      iscsi, value );
		return;
	}

	DBG ( "iSCSI %p will redirect to %s\n", iscsi, value );
	sin->sin_addr = address;
}

/**
 * Handle iSCSI AuthMethod text value
 *
 * @v iscsi		iSCSI session
 * @v value		AuthMethod value
 */
static void iscsi_handle_authmethod_value ( struct iscsi_session *iscsi,
					    const char *value ) {

	/* If server requests CHAP, send the CHAP_A string */
	if ( strcmp ( value, "CHAP" ) == 0 ) {
		DBG ( "iSCSI %p initiating CHAP authentication\n", iscsi );
		iscsi->status |= ISCSI_STATUS_STRINGS_CHAP_ALGORITHM;
	}
}

/**
 * Handle iSCSI CHAP_A text value
 *
 * @v iscsi		iSCSI session
 * @v value		CHAP_A value
 */
static void iscsi_handle_chap_a_value ( struct iscsi_session *iscsi,
					const char *value ) {
	int rc;

	/* We only ever offer "5" (i.e. MD5) as an algorithm, so if
	 * the server responds with anything else it is a protocol
	 * violation.
	 */
	if ( strcmp ( value, "5" ) != 0 ) {
		DBG ( "iSCSI %p got invalid CHAP algorithm \"%s\"\n",
		      iscsi, value );
	}

	/* Prepare for CHAP with MD5 */
	if ( ( rc = chap_init ( &iscsi->chap, &md5_algorithm ) ) != 0 ) {
		DBG ( "iSCSI %p could not initialise CHAP\n", iscsi );
		iscsi_done ( iscsi, rc );
	}
}

/**
 * Handle iSCSI CHAP_I text value
 *
 * @v iscsi		iSCSI session
 * @v value		CHAP_I value
 */
static void iscsi_handle_chap_i_value ( struct iscsi_session *iscsi,
					const char *value ) {
	unsigned int identifier;
	char *endp;

	/* The CHAP identifier is an integer value */
	identifier = strtoul ( value, &endp, 0 );
	if ( *endp != '\0' ) {
		DBG ( "iSCSI %p saw invalid CHAP identifier \"%s\"\n",
		      iscsi, value );
	}

	/* Identifier and secret are the first two components of the
	 * challenge.
	 */
	chap_set_identifier ( &iscsi->chap, identifier );
	if ( iscsi->password ) {
		chap_update ( &iscsi->chap, iscsi->password,
			      strlen ( iscsi->password ) );
	}
}

/**
 * Handle iSCSI CHAP_C text value
 *
 * @v iscsi		iSCSI session
 * @v value		CHAP_C value
 */
static void iscsi_handle_chap_c_value ( struct iscsi_session *iscsi,
					const char *value ) {
	char buf[3];
	char *endp;
	uint8_t byte;

	/* Check and strip leading "0x" */
	if ( ( value[0] != '0' ) || ( value[1] != 'x' ) ) {
		DBG ( "iSCSI %p saw invalid CHAP challenge \"%s\"\n",
		      iscsi, value );
	}
	value += 2;

	/* Process challenge an octet at a time */
	for ( ; ( value[0] && value[1] ) ; value += 2 ) {
		memcpy ( buf, value, 2 );
		buf[3] = 0;
		byte = strtoul ( buf, &endp, 16 );
		if ( *endp != '\0' ) {
			DBG ( "iSCSI %p saw invalid CHAP challenge byte "
			      "\"%s\"\n", iscsi, buf );
		}
		chap_update ( &iscsi->chap, &byte, sizeof ( byte ) );
	}

	/* Build CHAP response */
	DBG ( "iSCSI %p sending CHAP response\n", iscsi );
	chap_respond ( &iscsi->chap );
	iscsi->status |= ISCSI_STATUS_STRINGS_CHAP_RESPONSE;
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
	 */
	void ( * handle_value ) ( struct iscsi_session *iscsi,
				  const char *value );
};

/** iSCSI text strings that we want to handle */
struct iscsi_string_type iscsi_string_types[] = {
	{ "TargetAddress=", iscsi_handle_targetaddress_value },
	{ "AuthMethod=", iscsi_handle_authmethod_value },
	{ "CHAP_A=", iscsi_handle_chap_a_value },
	{ "CHAP_I=", iscsi_handle_chap_i_value },
	{ "CHAP_C=", iscsi_handle_chap_c_value },
	{ NULL, NULL }
};

/**
 * Handle iSCSI string
 *
 * @v iscsi		iSCSI session
 * @v string		iSCSI string (in "key=value" format)
 */
static void iscsi_handle_string ( struct iscsi_session *iscsi,
				  const char *string ) {
	struct iscsi_string_type *type;
	size_t key_len;

	for ( type = iscsi_string_types ; type->key ; type++ ) {
		key_len = strlen ( type->key );
		if ( strncmp ( string, type->key, key_len ) == 0 ) {
			DBG ( "iSCSI %p handling %s\n", iscsi, string );
			type->handle_value ( iscsi, ( string + key_len ) );
			return;
		}
	}
	DBG ( "iSCSI %p ignoring %s\n", iscsi, string );
}

/**
 * Handle iSCSI strings
 *
 * @v iscsi		iSCSI session
 * @v string		iSCSI string buffer
 * @v len		Length of string buffer
 */
static void iscsi_handle_strings ( struct iscsi_session *iscsi,
				   const char *strings, size_t len ) {
	size_t string_len;

	/* Handle each string in turn, taking care not to overrun the
	 * data buffer in case of badly-terminated data.
	 */
	while ( 1 ) {
		string_len = ( strnlen ( strings, len ) + 1 );
		if ( string_len > len )
			break;
		iscsi_handle_string ( iscsi, strings );
		strings += string_len;
		len -= string_len;
	}
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
static void iscsi_rx_login_response ( struct iscsi_session *iscsi, void *data,
				      size_t len, size_t remaining ) {
	struct iscsi_bhs_login_response *response
		= &iscsi->rx_bhs.login_response;
	int rc;

	/* Buffer up the PDU data */
	if ( ( rc = iscsi_rx_buffered_data ( iscsi, data, len ) ) != 0 ) {
		DBG ( "iSCSI %p could not buffer login response\n", iscsi );
		iscsi_done ( iscsi, rc );
		return;
	}
	if ( remaining )
		return;

	/* Process string data and discard string buffer */
	iscsi_handle_strings ( iscsi, iscsi->rx_buffer, iscsi->rx_len );
	iscsi_rx_buffered_data_done ( iscsi );

	/* Check for login redirection */
	if ( response->status_class == ISCSI_STATUS_REDIRECT ) {
		DBG ( "iSCSI %p redirecting to new server\n", iscsi );
		/* Close the TCP connection; our TCP closed() method
		 * will take care of the reconnection once this
		 * connection has been cleanly terminated.
		 */
		tcp_close ( &iscsi->tcp );
		return;
	}

	/* Check for fatal errors */
	if ( response->status_class != 0 ) {
		DBG ( "iSCSI login failure: class %02x detail %02x\n",
		      response->status_class, response->status_detail );
		iscsi_done ( iscsi, -EPERM );
		return;
	}

	/* Handle login transitions */
	if ( response->flags & ISCSI_LOGIN_FLAG_TRANSITION ) {
		switch ( response->flags & ISCSI_LOGIN_NSG_MASK ) {
		case ISCSI_LOGIN_NSG_OPERATIONAL_NEGOTIATION:
			iscsi->status =
				( ISCSI_STATUS_OPERATIONAL_NEGOTIATION_PHASE |
				  ISCSI_STATUS_STRINGS_OPERATIONAL );
			break;
		case ISCSI_LOGIN_NSG_FULL_FEATURE_PHASE:
			iscsi->status = ISCSI_STATUS_FULL_FEATURE_PHASE;
			break;
		default:
			DBG ( "iSCSI %p got invalid response flags %02x\n",
			      iscsi, response->flags );
			iscsi_done ( iscsi, -EIO );
			return;
		}
	}

	/* Send next login request PDU if we haven't reached the full
	 * feature phase yet.
	 */
	if ( ( iscsi->status & ISCSI_STATUS_PHASE_MASK ) !=
	     ISCSI_STATUS_FULL_FEATURE_PHASE ) {
		iscsi_start_login ( iscsi );
		return;
	}

	/* Record TSIH for future reference */
	iscsi->tsih = ntohl ( response->tsih );
	
	/* Send the actual SCSI command */
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
	case ISCSI_OPCODE_LOGIN_REQUEST:
		iscsi_login_request_done ( iscsi );
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
	enum iscsi_tx_state next_state;
	
	iscsi->tx_offset += len;
	while ( 1 ) {
		switch ( iscsi->tx_state ) {
		case ISCSI_TX_BHS:
			iscsi->tx_len = sizeof ( iscsi->tx_bhs );
			next_state = ISCSI_TX_AHS;
			break;
		case ISCSI_TX_AHS:
			iscsi->tx_len = 4 * ISCSI_AHS_LEN ( common->lengths );
			next_state = ISCSI_TX_DATA;
			break;
		case ISCSI_TX_DATA:
			iscsi->tx_len = ISCSI_DATA_LEN ( common->lengths );
			next_state = ISCSI_TX_DATA_PADDING;
			break;
		case ISCSI_TX_DATA_PADDING:
			iscsi->tx_len = ISCSI_DATA_PAD_LEN ( common->lengths );
			next_state = ISCSI_TX_IDLE;
			break;
		case ISCSI_TX_IDLE:
			return;
		default:
			assert ( 0 );
			return;
		}
		assert ( iscsi->tx_offset <= iscsi->tx_len );

		/* If the whole of the current portion has not yet
		 * been acked, stay in this state for now.
		 */
		if ( iscsi->tx_offset != iscsi->tx_len )
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
		if ( remaining )
			return;
		printf ( "Unknown iSCSI opcode %02x\n", response->opcode );
		iscsi_done ( iscsi, -EOPNOTSUPP );
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
	if ( ( iscsi->rx_offset + len ) >= sizeof ( iscsi->rx_bhs ) ) {
		DBG ( "iSCSI %p received PDU opcode %#x len %#lx\n",
		      iscsi, iscsi->rx_bhs.common.opcode,
		      ISCSI_DATA_LEN ( iscsi->rx_bhs.common.lengths ) );
	}
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
	enum iscsi_rx_state next_state;
	size_t frag_len;
	size_t remaining;

	while ( 1 ) {
		switch ( iscsi->rx_state ) {
		case ISCSI_RX_BHS:
			process = iscsi_rx_bhs;
			iscsi->rx_len = sizeof ( iscsi->rx_bhs );
			next_state = ISCSI_RX_AHS;			
			break;
		case ISCSI_RX_AHS:
			process = iscsi_rx_discard;
			iscsi->rx_len = 4 * ISCSI_AHS_LEN ( common->lengths );
			next_state = ISCSI_RX_DATA;
			break;
		case ISCSI_RX_DATA:
			process = iscsi_rx_data;
			iscsi->rx_len = ISCSI_DATA_LEN ( common->lengths );
			next_state = ISCSI_RX_DATA_PADDING;
			break;
		case ISCSI_RX_DATA_PADDING:
			process = iscsi_rx_discard;
			iscsi->rx_len = ISCSI_DATA_PAD_LEN ( common->lengths );
			next_state = ISCSI_RX_BHS;
			break;
		default:
			assert ( 0 );
			return;
		}

		frag_len = iscsi->rx_len - iscsi->rx_offset;
		if ( frag_len > len )
			frag_len = len;
		remaining = iscsi->rx_len - iscsi->rx_offset - frag_len;
		process ( iscsi, data, frag_len, remaining );

		iscsi->rx_offset += frag_len;
		data += frag_len;
		len -= frag_len;

		/* If all the data for this state has not yet been
		 * received, stay in this state for now.
		 */
		if ( iscsi->rx_offset != iscsi->rx_len )
			return;

		iscsi->rx_state = next_state;
		iscsi->rx_offset = 0;
	}
}

#warning "Remove me soon"
static struct tcp_operations iscsi_tcp_operations;

/**
 * Handle TCP connection closure
 *
 * @v conn		TCP connection
 * @v status		Error code, if any
 *
 */
static void iscsi_closed ( struct tcp_connection *conn, int status ) {
	struct iscsi_session *iscsi = tcp_to_iscsi ( conn );
	int session_status = iscsi->status;

	/* Clear session status */
	iscsi->status = 0;

	/* If we are deliberately closing down, exit cleanly */
	if ( session_status & ISCSI_STATUS_CLOSING ) {
		iscsi_done ( iscsi, status );
		return;
	}

	/* Retry connection if within the retry limit, otherwise fail */
	if ( ++iscsi->retry_count <= ISCSI_MAX_RETRIES ) {
		DBG ( "iSCSI %p retrying connection\n", iscsi );
		/* Re-copy address to handle redirection */
		memset ( &iscsi->tcp, 0, sizeof ( iscsi->tcp ) );
		iscsi->tcp.tcp_op = &iscsi_tcp_operations;
		memcpy ( &iscsi->tcp.peer, &iscsi->target,
			 sizeof ( iscsi->tcp.peer ) );
		tcp_connect ( conn );
	} else {
		printf ( "iSCSI %p retry count exceeded\n", iscsi );
		iscsi_done ( iscsi, status );
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
	iscsi->status = ( ISCSI_STATUS_SECURITY_NEGOTIATION_PHASE |
			  ISCSI_STATUS_STRINGS_SECURITY );
	iscsi->retry_count = 0;

	/* Prepare to receive PDUs. */
	iscsi->rx_state = ISCSI_RX_BHS;
	iscsi->rx_offset = 0;

	/* Assign fresh initiator task tag */
	iscsi->itt++;

	/* Start logging in */
	iscsi_start_login ( iscsi );
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
 * @ret aop		Asynchronous operation for this SCSI command
 */
struct async_operation * iscsi_issue ( struct iscsi_session *iscsi,
				       struct scsi_command *command ) {
	assert ( iscsi->command == NULL );
	iscsi->command = command;

	if ( iscsi->status ) {
		if ( ( iscsi->status & ISCSI_STATUS_PHASE_MASK ) ==
		     ISCSI_STATUS_FULL_FEATURE_PHASE ) {
			/* Session already open: issue command */
			iscsi_start_command ( iscsi );
			tcp_senddata ( &iscsi->tcp );
		} else {
			/* Session failed to reach full feature phase:
			 * abort immediately rather than retrying the
			 * login.
			 */
			iscsi_done ( iscsi, -EPERM );
		}
	} else {
		/* Session not open: initiate login */
		iscsi->tcp.tcp_op = &iscsi_tcp_operations;
		memcpy ( &iscsi->tcp.peer, &iscsi->target,
			 sizeof ( iscsi->tcp.peer ) );
		tcp_connect ( &iscsi->tcp );
	}

	return &iscsi->aop;
}

/**
 * Close down iSCSI session
 *
 * @v iscsi		iSCSI session
 * @ret aop		Asynchronous operation
 */
struct async_operation * iscsi_shutdown ( struct iscsi_session *iscsi ) {
	if ( iscsi->status ) {
		iscsi->status |= ISCSI_STATUS_CLOSING;
		tcp_close ( &iscsi->tcp );
	}
	return &iscsi->aop;
}
