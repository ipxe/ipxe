/*
 * Copyright (C) 2009 Joshua Oreman <oremanj@rwcr.net>.
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

#include <errno.h>
#include <gpxe/errortab.h>

/* Record errors as though they come from the 802.11 stack */
#undef ERRFILE
#define ERRFILE ERRFILE_net80211

/** All 802.11 errors
 *
 * These follow the 802.11 standard as much as is feasible, but most
 * have been abbreviated to fit the 50-character limit imposed by
 * strerror.
 */
struct errortab wireless_errors[] __errortab = {
	/* gPXE 802.11 stack errors */
	{ EINVAL | EUNIQ_01, "Packet too short" },
	{ EINVAL | EUNIQ_02, "Packet 802.11 version not supported" },
	{ EINVAL | EUNIQ_03, "Packet not a data packet" },
	{ EINVAL | EUNIQ_04, "Packet not from an Access Point" },
	{ EINVAL | EUNIQ_05, "Packet has invalid LLC header" },
	{ EINVAL | EUNIQ_06, "Packet decryption error", },
	{ EINVAL | EUNIQ_07, "Invalid active scan requested" },

	/* 802.11 status codes (IEEE Std 802.11-2007, Table 7-23) */
	/* Maximum error length: 50 chars                                            | */
	{ ECONNREFUSED | EUNIQ_01, "Unspecified failure" },
	{ ECONNREFUSED | EUNIQ_0A, "Cannot support all requested capabilities" },
	{ ECONNREFUSED | EUNIQ_0B, "Reassociation denied due to lack of association" },
	{ ECONNREFUSED | EUNIQ_0C, "Association denied for another reason" },
	{ ECONNREFUSED | EUNIQ_0D, "Authentication algorithm unsupported" },
	{ ECONNREFUSED | EUNIQ_0E, "Authentication sequence number unexpected" },
	{ ECONNREFUSED | EUNIQ_0F, "Authentication rejected due to challenge failure" },
	{ ECONNREFUSED | EUNIQ_10, "Authentication rejected due to timeout" },
	{ ECONNREFUSED | EUNIQ_11, "Association denied because AP is out of resources" },
	{ ECONNREFUSED | EUNIQ_12, "Association denied; basic rate support required" },
	{ ECONNREFUSED | EUNIQ_13, "Association denied; short preamble support req'd" },
	{ ECONNREFUSED | EUNIQ_14, "Association denied; PBCC modulation support req'd" },
	{ ECONNREFUSED | EUNIQ_15, "Association denied; Channel Agility support req'd" },
	{ ECONNREFUSED | EUNIQ_16, "Association denied; Spectrum Management required" },
	{ ECONNREFUSED | EUNIQ_17, "Association denied; Power Capability unacceptable" },
	{ ECONNREFUSED | EUNIQ_18, "Association denied; Supported Channels unacceptable" },
	{ ECONNREFUSED | EUNIQ_19, "Association denied; Short Slot Tume support req'd" },
	{ ECONNREFUSED | EUNIQ_1A, "Association denied; DSSS-OFDM support required" },
	{ EHOSTUNREACH,            "Unspecified, QoS-related failure" },
	{ EHOSTUNREACH | EUNIQ_01, "Association denied; QoS AP out of QoS resources" },
	{ EHOSTUNREACH | EUNIQ_02, "Association denied due to excessively poor link" },
	{ EHOSTUNREACH | EUNIQ_03, "Association denied; QoS support required" },
	{ EHOSTUNREACH | EUNIQ_05, "The request has been declined" },
	{ EHOSTUNREACH | EUNIQ_06, "Request unsuccessful due to invalid parameters" },
	{ EHOSTUNREACH | EUNIQ_07, "TS not created due to bad specification" },
	{ EHOSTUNREACH | EUNIQ_08, "Invalid information element" },
	{ EHOSTUNREACH | EUNIQ_09, "Invalid group cipher" },
	{ EHOSTUNREACH | EUNIQ_0A, "Invalid pairwise cipher" },
	{ EHOSTUNREACH | EUNIQ_0B, "Invalid AKMP" },
	{ EHOSTUNREACH | EUNIQ_0C, "Unsupported RSN information element version" },
	{ EHOSTUNREACH | EUNIQ_0D, "Invalid RSN information element capabilities" },
	{ EHOSTUNREACH | EUNIQ_0E, "Cipher suite rejected because of security policy" },
	{ EHOSTUNREACH | EUNIQ_0F, "TS not created due to insufficient delay" },
	{ EHOSTUNREACH | EUNIQ_10, "Direct link is not allowed in the BSS by policy" },
	{ EHOSTUNREACH | EUNIQ_11, "The Destination STA is not present within the BSS" },
	{ EHOSTUNREACH | EUNIQ_12, "The Destination STA is not a QoS STA" },
	{ EHOSTUNREACH | EUNIQ_13, "Association denied; Listen Interval is too large" },

	/* 802.11 reason codes (IEEE Std 802.11-2007, Table 7-22) */
	/* Maximum error length: 50 chars                                          | */
	{ ECONNRESET | EUNIQ_01, "Unspecified reason" },
	{ ECONNRESET | EUNIQ_02, "Previous authentication no longer valid" },
	{ ECONNRESET | EUNIQ_03, "Deauthenticated due to leaving network" },
	{ ECONNRESET | EUNIQ_04, "Disassociated due to inactivity" },
	{ ECONNRESET | EUNIQ_05, "Disassociated because AP is out of resources" },
	{ ECONNRESET | EUNIQ_06, "Class 2 frame received from nonauthenticated STA" },
	{ ECONNRESET | EUNIQ_07, "Class 3 frame received from nonassociated STA" },
	{ ECONNRESET | EUNIQ_08, "Disassociated due to roaming" },
	{ ECONNRESET | EUNIQ_09, "STA requesting (re)association not authenticated" },
	{ ECONNRESET | EUNIQ_0A, "Disassociated; Power Capability unacceptable" },
	{ ECONNRESET | EUNIQ_0B, "Disassociated; Supported Channels unacceptable" },
	{ ECONNRESET | EUNIQ_0D, "Invalid information element" },
	{ ECONNRESET | EUNIQ_0E, "Message integrity code (MIC) failure" },
	{ ECONNRESET | EUNIQ_0F, "4-Way Handshake timeout" },
	{ ECONNRESET | EUNIQ_10, "Group Key Handshake timeout" },
	{ ECONNRESET | EUNIQ_11, "4-Way Handshake information element changed unduly" },
	{ ECONNRESET | EUNIQ_12, "Invalid group cipher" },
	{ ECONNRESET | EUNIQ_13, "Invalid pairwise cipher" },
	{ ECONNRESET | EUNIQ_14, "Invalid AKMP" },
	{ ECONNRESET | EUNIQ_15, "Unsupported RSN information element version" },
	{ ECONNRESET | EUNIQ_16, "Invalid RSN information element capabilities" },
	{ ECONNRESET | EUNIQ_17, "IEEE 802.1X authentication failed" },
	{ ECONNRESET | EUNIQ_18, "Cipher suite rejected because of security policy" },
	{ ENETRESET,            "Disassociated for unspecified, QoS-related reason" },
	{ ENETRESET | EUNIQ_01, "Disassociated; QoS AP is out of QoS resources" },
	{ ENETRESET | EUNIQ_02, "Disassociated due to excessively poor link" },
	{ ENETRESET | EUNIQ_03, "Disassociated due to TXOP limit violation" },
	{ ENETRESET | EUNIQ_04, "Requested; STA is leaving the BSS (or resetting)" },
	{ ENETRESET | EUNIQ_05, "Requested; does not want to use the mechanism" },
	{ ENETRESET | EUNIQ_06, "Requested; setup is required" },
	{ ENETRESET | EUNIQ_07, "Requested from peer STA due to timeout" },
	{ ENETRESET | EUNIQ_0D, "Peer STA does not support requested cipher suite" },
};
