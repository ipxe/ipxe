#ifndef	IGMP_H
#define	IGMP_H

#include "stdint.h"
#include <gpxe/in.h>

#define IGMP_QUERY	0x11
#define IGMPv1_REPORT	0x12
#define IGMPv2_REPORT	0x16
#define IGMP_LEAVE	0x17
#define GROUP_ALL_HOSTS 0xe0000001 /* 224.0.0.1 Host byte order */

#define MULTICAST_MASK    0xf0000000
#define MULTICAST_NETWORK 0xe0000000

enum {
	IGMP_SERVER,
	MAX_IGMP
};

struct igmp {
	uint8_t		type;
	uint8_t		response_time;
	uint16_t	chksum;
	struct in_addr	group;
} PACKED;

struct igmp_ip_t { /* Format of an igmp ip packet */
	struct iphdr ip;
	uint8_t router_alert[4]; /* Router alert option */
	struct igmp igmp;
} PACKED;

struct igmptable_t {
	struct in_addr group;
	unsigned long time;
} PACKED;

extern void join_group ( int slot, unsigned long group );
extern void leave_group ( int slot );

#endif	/* IGMP_H */
