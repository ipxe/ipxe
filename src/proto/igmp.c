/*
 * Eric Biederman wrote this code originally.
 *
 */

#if 0

#include <ip.h>
#include <igmp.h>

static unsigned long last_igmpv1 = 0;
static struct igmptable_t igmptable[MAX_IGMP];

static long rfc1112_sleep_interval ( long base, int exp ) {
	unsigned long divisor, tmo;

	if ( exp > BACKOFF_LIMIT )
		exp = BACKOFF_LIMIT;
	divisor = RAND_MAX / ( base << exp );
	tmo = random() / divisor;
	return tmo;
}

static void send_igmp_reports ( unsigned long now ) {
	struct igmp_ip_t igmp;
	int i;

	for ( i = 0 ; i < MAX_IGMP ; i++ ) {
		if ( ! igmptable[i].time )
			continue;
		if ( now < igmptable[i].time )
			continue;

		igmp.router_alert[0] = 0x94;
		igmp.router_alert[1] = 0x04;
		igmp.router_alert[2] = 0;
		igmp.router_alert[3] = 0;
		build_ip_hdr ( igmptable[i].group.s_addr, 1, IP_IGMP,
			       sizeof ( igmp.router_alert ),
			       sizeof ( igmp ), &igmp );
		igmp.igmp.type = IGMPv2_REPORT;
		if ( last_igmpv1 && 
		     ( now < last_igmpv1 + IGMPv1_ROUTER_PRESENT_TIMEOUT ) ) {
			igmp.igmp.type = IGMPv1_REPORT;
		}
		igmp.igmp.response_time = 0;
		igmp.igmp.chksum = 0;
		igmp.igmp.group.s_addr = igmptable[i].group.s_addr;
		igmp.igmp.chksum = ipchksum ( &igmp.igmp,
					      sizeof ( igmp.igmp ) );
		ip_transmit ( sizeof ( igmp ), &igmp );
		DBG ( "IGMP sent report to %s\n", inet_ntoa ( igmp.igmp.group ) );
		/* Don't send another igmp report until asked */
		igmptable[i].time = 0;
	}
}

static void process_igmp ( unsigned long now, unsigned short ptype __unused,
			   struct iphdr *ip ) {
	struct igmp *igmp;
	int i;
	unsigned iplen;

	if ( ( ! ip ) || ( ip->protocol != IP_IGMP ) ||
	     ( nic.packetlen < ( sizeof ( struct iphdr ) +
				 sizeof ( struct igmp ) ) ) ) {
		return;
	}

	iplen = ( ip->verhdrlen & 0xf ) * 4;
	igmp = ( struct igmp * ) &nic.packet[ sizeof( struct iphdr ) ];
	if ( ipchksum ( igmp, ntohs ( ip->len ) - iplen ) != 0 )
		return;

	if ( ( igmp->type == IGMP_QUERY ) && 
	     ( ip->dest.s_addr == htonl ( GROUP_ALL_HOSTS ) ) ) {
		unsigned long interval = IGMP_INTERVAL;

		if ( igmp->response_time == 0 ) {
			last_igmpv1 = now;
		} else {
			interval = ( igmp->response_time * TICKS_PER_SEC ) /10;
		}
		
		DBG ( "IGMP received query for %s\n", inet_ntoa ( igmp->group ) );
		for ( i = 0 ; i < MAX_IGMP ; i++ ) {
			uint32_t group = igmptable[i].group.s_addr;
			if ( ( group == 0 ) ||
			     ( group == igmp->group.s_addr ) ) {
				unsigned long time;
				time = currticks() +
					rfc1112_sleep_interval ( interval, 0 );
				if ( time < igmptable[i].time ) {
					igmptable[i].time = time;
				}
			}
		}
	}
	if ( ( ( igmp->type == IGMPv1_REPORT ) ||
	       ( igmp->type == IGMPv2_REPORT ) ) &&
	     ( ip->dest.s_addr == igmp->group.s_addr ) ) {
	        DBG ( "IGMP received report for %s\n", 
		      inet_ntoa ( igmp->group ) );
		for ( i = 0 ; i < MAX_IGMP ; i++ ) {
			if ( ( igmptable[i].group.s_addr ==
			       igmp->group.s_addr ) &&
			     ( igmptable[i].time != 0 ) ) {
				igmptable[i].time = 0;
			}
		}
	}
}

struct background igmp_background __background = {
	.send = send_igmp_reports,
	.process = process_igmp,
};

void leave_group ( int slot ) {
	/* Be very stupid and always send a leave group message if 
	 * I have subscribed.  Imperfect but it is standards
	 * compliant, easy and reliable to implement.
	 *
	 * The optimal group leave method is to only send leave when,
	 * we were the last host to respond to a query on this group,
	 * and igmpv1 compatibility is not enabled.
	 */
	if ( igmptable[slot].group.s_addr ) {
		struct igmp_ip_t igmp;

		igmp.router_alert[0] = 0x94;
		igmp.router_alert[1] = 0x04;
		igmp.router_alert[2] = 0;
		igmp.router_alert[3] = 0;
		build_ip_hdr ( htonl ( GROUP_ALL_HOSTS ), 1, IP_IGMP,
			       sizeof ( igmp.router_alert ), sizeof ( igmp ),
			       &igmp);
		igmp.igmp.type = IGMP_LEAVE;
		igmp.igmp.response_time = 0;
		igmp.igmp.chksum = 0;
		igmp.igmp.group.s_addr = igmptable[slot].group.s_addr;
		igmp.igmp.chksum = ipchksum ( &igmp.igmp, sizeof ( igmp ) );
		ip_transmit ( sizeof ( igmp ), &igmp );
		DBG ( "IGMP left group %s\n", inet_ntoa ( igmp.igmp.group ) );
	}
	memset ( &igmptable[slot], 0, sizeof ( igmptable[0] ) );
}

void join_group ( int slot, unsigned long group ) {
	/* I have already joined */
	if ( igmptable[slot].group.s_addr == group )
		return;
	if ( igmptable[slot].group.s_addr ) {
		leave_group ( slot );
	}
	/* Only join a group if we are given a multicast ip, this way
	 * code can be given a non-multicast (broadcast or unicast ip)
	 * and still work... 
	 */
	if ( ( group & htonl ( MULTICAST_MASK ) ) ==
	     htonl ( MULTICAST_NETWORK ) ) {
		igmptable[slot].group.s_addr = group;
		igmptable[slot].time = currticks();
	}
}

#endif
