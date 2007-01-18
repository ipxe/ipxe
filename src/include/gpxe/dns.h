#ifndef _GPXE_DNS_H
#define _GPXE_DNS_H

/** @file
 *
 * DNS protocol
 *
 */

#include <stdint.h>
#include <gpxe/in.h>
#include <gpxe/async.h>
#include <gpxe/retry.h>

/*
 * Constants
 *
 */

#define DNS_TYPE_A		1
#define DNS_TYPE_CNAME		5
#define DNS_TYPE_ANY		255

#define DNS_CLASS_IN		1
#define DNS_CLASS_CS		2
#define DNS_CLASS_CH		3
#define DNS_CLASS_HS		4

#define DNS_FLAG_QUERY		( 0x00 << 15 )
#define DNS_FLAG_RESPONSE	( 0x01 << 15 )
#define DNS_FLAG_QR(flags)	( (flags) & ( 0x01 << 15 ) )
#define DNS_FLAG_OPCODE_QUERY	( 0x00 << 11 )
#define DNS_FLAG_OPCODE_IQUERY	( 0x01 << 11 )
#define DNS_FLAG_OPCODE_STATUS	( 0x02 << 11 )
#define DNS_FLAG_OPCODE(flags)	( (flags) & ( 0x0f << 11 ) )
#define DNS_FLAG_RD		( 0x01 << 8 )
#define DNS_FLAG_RA		( 0x01 << 7 )
#define DNS_FLAG_RCODE_OK	( 0x00 << 0 )
#define DNS_FLAG_RCODE_NX	( 0x03 << 0 )
#define DNS_FLAG_RCODE(flags)	( (flags) & ( 0x0f << 0 ) )

#define	DNS_PORT		53
#define	DNS_MAX_RETRIES		3
#define	DNS_MAX_CNAME_RECURSION	0x30

/*
 * DNS protocol structures
 *
 */
struct dns_header {
	uint16_t	id;
	uint16_t	flags;
	uint16_t	qdcount;
	uint16_t	ancount;
	uint16_t	nscount;
	uint16_t	arcount;
} __attribute__ (( packed ));

struct dns_query_info {
	uint16_t	qtype;
	uint16_t	qclass;
} __attribute__ (( packed ));

struct dns_query {
	struct dns_header dns;
	char		payload[ 256 + sizeof ( struct dns_query_info ) ];
} __attribute__ (( packed ));

struct dns_rr_info_common {
	uint16_t	type;
	uint16_t	class;
	uint32_t	ttl;
	uint16_t	rdlength;
} __attribute__ (( packed ));

struct dns_rr_info_a {
	struct dns_rr_info_common common;
	struct in_addr in_addr;
} __attribute__ (( packed ));

struct dns_rr_info_cname {
	struct dns_rr_info_common common;
	char cname[0];
} __attribute__ (( packed ));

union dns_rr_info {
	struct dns_rr_info_common common;
	struct dns_rr_info_a a;
	struct dns_rr_info_cname cname;
};

/** A DNS request */
struct dns_request {
	/** Socket address to fill in with resolved address */
	struct sockaddr *sa;

	/** Current query packet */
	struct dns_query query;
	/** Length of current query packet */
	struct dns_query_info *qinfo;
	/** Recursion counter */
	unsigned int recursion;

	/** Asynchronous operation */
	struct async async;
	/** UDP connection */
	struct udp_connection udp;
	/** Retry timer */
	struct retry_timer timer;
};

extern struct in_addr nameserver;

extern int dns_resolv ( const char *name, struct sockaddr *sa,
			struct async *parent );

#endif /* _GPXE_DNS_H */
