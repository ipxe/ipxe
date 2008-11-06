#ifndef _GPXE_IB_PACKET_H
#define _GPXE_IB_PACKET_H

/** @file
 *
 * Infiniband packet format
 *
 */

/** Half of an Infiniband Global Identifier */
struct ib_gid_half {
	uint8_t bytes[8];
};

/** An Infiniband Global Identifier */
struct ib_gid {
	union {
		uint8_t bytes[16];
		uint16_t words[8];
		uint32_t dwords[4];
		struct ib_gid_half half[2];
	} u;
};

/** An Infiniband Local Route Header */
struct ib_local_route_header {
	/** Virtual lane and link version */
	uint8_t vl__lver;
	/** Service level and next link header */
	uint8_t sl__lnh;
	/** Destination LID */
	uint16_t dlid;
	/** Packet length */
	uint16_t length;
	/** Source LID */
	uint16_t slid;
} __attribute__ (( packed ));

/** Infiniband virtual lanes */
enum ib_vl {
	IB_VL_DEFAULT = 0,
	IB_VL_SMP = 15,
};

/** An Infiniband Link Next Header value */
enum ib_lnh {
	IB_LNH_RAW = 0,
	IB_LNH_IPv6 = 1,
	IB_LNH_BTH = 2,
	IB_LNH_GRH = 3
};

/** Default Infiniband LID */
#define IB_LID_NONE 0xffff

/** An Infiniband Global Route Header */
struct ib_global_route_header {
	/** IP version, traffic class, and flow label
	 *
	 *  4 bits : Version of the GRH
	 *  8 bits : Traffic class
	 * 20 bits : Flow label
	 */
	uint32_t ipver__tclass__flowlabel;
	/** Payload length */
	uint16_t paylen;
	/** Next header */
	uint8_t nxthdr;
	/** Hop limit */
	uint8_t hoplmt;
	/** Source GID */
	struct ib_gid sgid;
	/** Destiniation GID */
	struct ib_gid dgid;
} __attribute__ (( packed ));

#define IB_GRH_IPVER_IPv6 0x06
#define IB_GRH_NXTHDR_IBA 0x1b
#define IB_GRH_HOPLMT_MAX 0xff

/** An Infiniband Base Transport Header */
struct ib_base_transport_header {
	/** Opcode */
	uint8_t opcode;
	/** Transport header version, pad count, migration and solicitation */
	uint8_t se__m__padcnt__tver;
	/** Partition key */
	uint16_t pkey;
	/** Destination queue pair */
	uint32_t dest_qp;
	/** Packet sequence number and acknowledge request */
	uint32_t ack__psn;
} __attribute__ (( packed ));

/** An Infiniband BTH opcode */
enum ib_bth_opcode {
	BTH_OPCODE_UD_SEND = 0x64,
};

/** Default Infiniband partition key */
#define IB_PKEY_NONE 0xffff

/** Subnet management queue pair number */
#define IB_QPN_SMP 0

/** An Infiniband Datagram Extended Transport Header */
struct ib_datagram_extended_transport_header {
	/** Queue key */
	uint32_t qkey;
	/** Source queue pair */
	uint32_t src_qp;
} __attribute__ (( packed ));

#endif /* _GPXE_IB_PACKET_H */
