// dns_resolver.h - #define statements for the DNS resolver
 
// We only need A and CNAME queries (later possibly AAAA/A6?)
#define	QUERYTYPE_A	1
#define	QUERYTYPE_CNAME	5

// We only query with INTERNET class (not CHAOS or whatever)
#define	QUERYCLASS_INET	1

// Our first query will have the identifier <1> (arbitrary -
// remember however that (256 - QUERYIDENTIFIER)/2 > MAX_CNAME_RECURSION !!!
#define	QUERYIDENTIFIER	1

// Query flags are standard values here
#define	QUERYFLAGS	0x0100
#define	QUERYFLAGS_MASK	0xf8
#define	QUERYFLAGS_WANT	0x80

// Indices inside the byte array that holds DNS queries/answers
#define	QINDEX_ID	0
#define	QINDEX_FLAGS	2
#define	QINDEX_NUMQUEST	4
#define	QINDEX_NUMANSW	6
#define	QINDEX_NUMAUTH	8
#define	QINDEX_NUMADDIT	10
#define	QINDEX_QUESTION	12
#define	QINDEX_QTYPE	14
#define	QINDEX_QCLASS	16
#define	QINDEX_STORE_A	256

// Constant UDP port number for DNS traffic
#define	UDP_PORT_DNS	53

// Return values that the package parser may give
//	This packet was not for us (broadcast or whatever)
#define	RET_PACK_GARBAG	0
//	Retrieved an address - query finishes
#define	RET_GOT_ADDR	1
//	No A record for that hostname - try running a CNAME query
#define	RET_RUN_CNAME_Q	2
//	The CNAME query returned a valid hostname - run A query on that
#define	RET_RUN_NEXT_A	3
//	The CNAME query failed - stop resolving
#define	RET_CNAME_FAIL	4
//	We have a reliable input that claims that the hostname does not exist
#define	RET_NOSUCHNAME	5
//	The name server response is somehow bogus/can not be parsed -> Abort
#define	RET_DNSERROR	6

// Return values that the query engine may give
//	DNS query succeeded, IP address delivered
#define	RET_DNS_OK	0
//	DNS query failed
#define	RET_DNS_FAIL	1

// Error codes the DNS server can send to us
#define	ERR_NOSUCHNAME	3

