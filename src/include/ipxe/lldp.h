#ifndef _IPXE_LLDP_H
#define _IPXE_LLDP_H

/** @file
 *
 * Link Layer Discovery Protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

/** An LLDP TLV header */
struct lldp_tlv {
	/** Type and length */
	uint16_t type_len;
	/** Data */
	uint8_t data[0];
} __attribute__ (( packed ));

/**
 * Extract LLDP TLV type
 *
 * @v type_len		Type and length
 * @ret type		Type
 */
#define LLDP_TLV_TYPE( type_len ) ( (type_len) >> 9 )

/**
 * Extract LLDP TLV length
 *
 * @v type_len		Type and length
 * @ret len		Length
 */
#define LLDP_TLV_LEN( type_len ) ( (type_len) & 0x01ff )

/** End of LLDP data unit */
#define LLDP_TYPE_END 0x00

/** LLDP settings block name */
#define LLDP_SETTINGS_NAME "lldp"

/**
 * Construct LLDP setting tag
 *
 * LLDP settings are encoded as
 *
 *   ${netX.lldp/<prefix>.<type>.<index>.<offset>.<length>}
 *
 * where
 *
 *   <type> is the TLV type
 *
 *   <offset> is the starting offset within the TLV value
 *
 *   <length> is the length (or zero to read the from <offset> to the end)
 *
 *   <prefix>, if it has a non-zero value, is the subtype byte string
 *   of length <offset> to match at the start of the TLV value, up to
 *   a maximum matched length of 4 bytes
 *
 *   <index> is the index of the entry matching <type> and <prefix> to
 *   be accessed, with zero indicating the first matching entry
 *
 * The <prefix> is designed to accommodate both matching of the OUI
 * within an organization-specific TLV (e.g. 0x0080c2 for IEEE 802.1
 * TLVs) and of a subtype byte as found within many TLVs.
 *
 * This encoding allows most LLDP values to be extracted easily.  For
 * example
 *
 *   System name: ${netX.lldp/5.0.0.0:string}
 *
 *   System description: ${netX.lldp/6.0.0.0:string}
 *
 *   Port description: ${netX.lldp/4.0.0.0:string}
 *
 *   Port interface name: ${netX.lldp/5.2.0.1.0:string}
 *
 *   Chassis MAC address: ${netX.lldp/4.1.0.1.0:hex}
 *
 *   Management IPv4 address: ${netX.lldp/5.1.8.0.2.4:ipv4}
 *
 *   Port VLAN ID: ${netX.lldp/0x0080c2.1.127.0.4.2:int16}
 *
 *   Port VLAN name: ${netX.lldp/0x0080c2.3.127.0.7.0:string}
 *
 *   Maximum frame size: ${netX.lldp/0x00120f.4.127.0.4.2:uint16}
 *
 */
#define LLDP_TAG( prefix, type, index, offset, length )			\
	( ( ( ( uint64_t ) (prefix) ) << 32 ) |				\
	  ( (type) << 24 ) | ( (index) << 16 ) |			\
	  ( (offset) << 8 ) | ( (length) << 0 ) )

#endif /* _IPXE_LLDP_H */
