/* Copyright 2004 Linux Networx */
#ifdef PROTO_LACP
#if 0
#include "nic.h"
#include "timer.h"
#endif

#define LACP_DEBUG 0

/* Structure definitions originally taken from the linux bond_3ad driver */

#define SLOW_DST_MAC "\x01\x80\xc2\x00\x00\x02"
static const char slow_dest[] = SLOW_DST_MAC;


#define SLOW_SUBTYPE_LACP 1
#define SLOW_SUBTYPE_MARKER 2

struct slow_header {
	uint8_t subtype;
};

struct lacp_info {
	uint16_t system_priority;
	uint8_t  system[ETH_ALEN];
	uint16_t key;
	uint16_t port_priority;
	uint16_t port;
	uint8_t  state;
	uint8_t  reserved[3];
} PACKED;

#define LACP_CMP_LEN (2 + 6 + 2 + 2 + 2)
#define LACP_CP_LEN  (2 + 6 + 2 + 2 + 2 + 1)

/* Link Aggregation Control Protocol(LACP) data unit structure(43.4.2.2 in the 802.3ad standard) */
struct slow_lacp {
	uint8_t  subtype;		       /* = LACP(= 0x01) */
	uint8_t  version_number;
	uint8_t  tlv_type_actor_info;	       /* = actor information(type/length/value) */
#define LACP_TLV_TERMINATOR 0
#define LACP_TLV_ACTOR      1
#define LACP_TLV_PARTNER    2
#define LACP_TLV_COLLECTOR  3
	uint8_t  actor_information_length;     /* = 20 */
	struct lacp_info actor;
	uint8_t  tlv_type_partner_info;        /* = partner information */
	uint8_t  partner_information_length;   /* = 20 */
	struct lacp_info partner;
	uint8_t  tlv_type_collector_info;      /* = collector information */
	uint8_t  collector_information_length; /* = 16 */
	uint16_t collector_max_delay;
	uint8_t  reserved_12[12];
	uint8_t  tlv_type_terminator;	       /* = terminator */
	uint8_t  terminator_length;	       /* = 0 */ 
	uint8_t  reserved_50[50];	       /* = 0 */
} PACKED;

/* Marker Protocol Data Unit(PDU) structure(43.5.3.2 in the 802.3ad standard) */
struct slow_marker {
	uint8_t  subtype;                      /* = 0x02  (marker PDU) */
	uint8_t  version_number;	       /* = 0x01 */
	uint8_t  tlv_type;
#define MARKER_TLV_TERMINATOR 0                /* marker terminator */
#define MARKER_TLV_INFO       1	               /* marker information */
#define MARKER_TLV_RESPONSE   2	               /* marker response information */
	uint8_t  marker_length;                /* = 0x16 */
	uint16_t requester_port;	       /* The number assigned to the port by the requester */
	uint8_t  requester_system[ETH_ALEN];   /* The requester's system id */
	uint32_t requester_transaction_id;     /* The transaction id allocated by the requester, */
	uint16_t pad;		               /* = 0 */
	uint8_t  tlv_type_terminator;	       /* = 0x00 */
	uint8_t  terminator_length;	       /* = 0x00 */
	uint8_t  reserved_90[90];	       /* = 0 */
} PACKED;

union slow_union {
	struct slow_header header;
	struct slow_lacp lacp;
	struct slow_marker marker;
};

#define FAST_PERIODIC_TIME   (1*TICKS_PER_SEC)
#define SLOW_PERIODIC_TIME   (30*TICKS_PER_SEC)
#define SHORT_TIMEOUT_TIME   (3*FAST_PERIODIC_TIME)
#define LONG_TIMEOUT_TIME    (3*SLOW_PERIODIC_TIME)
#define CHURN_DETECTION_TIME (60*TICKS_PER_SEC)
#define AGGREGATE_WAIT_TIME  (2*TICKS_PER_SEC)

#define LACP_ACTIVITY        (1 << 0)
#define LACP_TIMEOUT         (1 << 1)
#define LACP_AGGREGATION     (1 << 2)
#define LACP_SYNCHRONIZATION (1 << 3)
#define LACP_COLLECTING      (1 << 4)
#define LACP_DISTRIBUTING    (1 << 5)
#define LACP_DEFAULTED       (1 << 6)
#define LACP_EXPIRED         (1 << 7)

#define UNSELECTED 0
#define STANDBY    1
#define SELECTED   2


struct lacp_state {
	struct slow_lacp pkt;
	unsigned long current_while_timer; /* Time when the LACP information expires */
	unsigned long periodic_timer; /* Time when I need to send my partner an update */
};

static struct lacp_state lacp;


#if LACP_DEBUG > 0
static void print_lacp_state(uint8_t state)
{
	printf("%hhx", state);
	if (state & LACP_ACTIVITY) {
		printf(" Activity");
	}
	if (state & LACP_TIMEOUT) {
		printf(" Timeout");
	}
	if (state & LACP_AGGREGATION) {
		printf(" Aggregation");
	}
	if (state & LACP_SYNCHRONIZATION) {
		printf(" Syncronization");
	}
	if (state & LACP_COLLECTING) {
		printf(" Collecting");
	}
	if (state & LACP_DISTRIBUTING) {
		printf(" Distributing");
	}
	if (state & LACP_DEFAULTED) {
		printf(" Defaulted");
	}
	if (state & LACP_EXPIRED) {
		printf(" Expired");
	}
	printf("\n");
}

static inline void print_lacpdu(struct slow_lacp *pkt)
{
	printf("subtype version:  %hhx %hhx\n", 
		pkt->subtype, pkt->version_number);
	printf("actor_tlv %hhx", pkt->tlv_type_actor_info);
	printf(" len: %hhx (\n", pkt->actor_information_length);
	printf(" sys_pri: %hx", ntohs(pkt->actor.system_priority));
	printf(" mac: %!", pkt->actor.system);
	printf(" key: %hx", ntohs(pkt->actor.key));
	printf(" port_pri: %hx", ntohs(pkt->actor.port_priority));
	printf(" port: %hx\n", ntohs(pkt->actor.port));
	printf(" state: ");
	print_lacp_state(pkt->actor.state);
#if LACP_DEBUG > 1
	printf(" reserved:     %hhx %hhx %hhx\n",
		pkt->actor.reserved[0], pkt->actor.reserved[1], pkt->actor.reserved[2]);
#endif
	printf(")\n");
	printf("partner_tlv: %hhx", pkt->tlv_type_partner_info);
	printf(" len: %hhx (\n", pkt->partner_information_length);
	printf(" sys_pri: %hx", ntohs(pkt->partner.system_priority));
	printf(" mac: %!", pkt->partner.system);
	printf(" key: %hx", ntohs(pkt->partner.key));
	printf(" port_pri: %hx", ntohs(pkt->partner.port_priority));
	printf(" port: %hx\n", ntohs(pkt->partner.port));
	printf(" state: ");
	print_lacp_state(pkt->partner.state);
#if LACP_DEBUG > 1
	printf(" reserved:     %hhx %hhx %hhx\n",
		pkt->partner.reserved[0], pkt->partner.reserved[1], pkt->partner.reserved[2]);
#endif
	printf(")\n");
	printf("collector_tlv: %hhx ", pkt->tlv_type_collector_info);
	printf(" len: %hhx (", pkt->collector_information_length);
	printf(" max_delay: %hx", ntohs(pkt->collector_max_delay));
#if LACP_DEBUG > 1
	printf("reserved_12:      %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx\n",
		pkt->reserved_12[0], pkt->reserved_12[1], pkt->reserved_12[2], 
		pkt->reserved_12[3], pkt->reserved_12[4], pkt->reserved_12[5], 
		pkt->reserved_12[6], pkt->reserved_12[7], pkt->reserved_12[8], 
		pkt->reserved_12[9], pkt->reserved_12[10], pkt->reserved_12[11]);
#endif
	printf(" )\n");
	printf("terminator_tlv: %hhx", pkt->tlv_type_terminator);
	printf(" len: %hhx ()\n", pkt->terminator_length);
}

static inline unsigned long lacp_timer_val(unsigned long now, unsigned long when)
{
	return when?(when - now)/TICKS_PER_SEC : 0;
}
static void print_lacp(const char *which, struct slow_lacp *pkt, unsigned long now)
{
	printf("%s\n", which);
	print_lacpdu(pkt);
	printf("timers: c %ds p %ds\n",
		lacp_timer_val(now, lacp.current_while_timer),
		lacp_timer_val(now, lacp.periodic_timer)
		);
	printf("\n");
}
#else /* LACP_DEBUG */
#define print_lacp(which, pkt, now) do {} while(0)
#endif /* LACP_DEBUG */

static void lacp_init_state(const uint8_t *mac)
{
	memset(&lacp, 0, sizeof(lacp));

	/* Initialize the packet constants */
	lacp.pkt.subtype               = 1;
	lacp.pkt.version_number        = 1;


	/* The default state of my interface */
	lacp.pkt.tlv_type_actor_info      = LACP_TLV_ACTOR;
	lacp.pkt.actor_information_length = 0x14;
	lacp.pkt.actor.system_priority    = htons(1);
	memcpy(lacp.pkt.actor.system, mac, ETH_ALEN);
	lacp.pkt.actor.key                = htons(1);
	lacp.pkt.actor.port               = htons(1);
	lacp.pkt.actor.port_priority      = htons(1);
	lacp.pkt.actor.state = 
		LACP_SYNCHRONIZATION |
		LACP_COLLECTING      |
		LACP_DISTRIBUTING    |
		LACP_DEFAULTED;

	/* Set my partner defaults */
	lacp.pkt.tlv_type_partner_info      = LACP_TLV_PARTNER;
	lacp.pkt.partner_information_length = 0x14;
	lacp.pkt.partner.system_priority    = htons(1);
	/* memset(lacp.pkt.parnter_system, 0, ETH_ALEN); */
	lacp.pkt.partner.key                = htons(1);
	lacp.pkt.partner.port               = htons(1);
	lacp.pkt.partner.port_priority      = htons(1);
	lacp.pkt.partner.state =
		LACP_ACTIVITY        |
		LACP_SYNCHRONIZATION |
		LACP_COLLECTING      |
		LACP_DISTRIBUTING    |
		LACP_DEFAULTED;

	lacp.pkt.tlv_type_collector_info      = LACP_TLV_COLLECTOR;
	lacp.pkt.collector_information_length = 0x10;
	lacp.pkt.collector_max_delay          = htons(0x8000); /* ???? */

	lacp.pkt.tlv_type_terminator          = LACP_TLV_TERMINATOR;
	lacp.pkt.terminator_length            = 0;
}

#define LACP_NTT_MASK (LACP_ACTIVITY | LACP_TIMEOUT | \
	LACP_SYNCHRONIZATION | LACP_AGGREGATION)

static inline int lacp_update_ntt(struct slow_lacp *pkt)
{
	int ntt = 0;
	if ((memcmp(&pkt->partner, &lacp.pkt.actor, LACP_CMP_LEN) != 0) ||
		((pkt->partner.state & LACP_NTT_MASK) != 
			(lacp.pkt.actor.state & LACP_NTT_MASK)))
	{
		ntt = 1;
	}
	return ntt;
}

static inline void lacp_record_pdu(struct slow_lacp *pkt)
{
	memcpy(&lacp.pkt.partner, &pkt->actor, LACP_CP_LEN);

	lacp.pkt.actor.state &= ~LACP_DEFAULTED;
	lacp.pkt.partner.state &= ~LACP_SYNCHRONIZATION;
	if ((memcmp(&pkt->partner, &lacp.pkt.actor, LACP_CMP_LEN) == 0) &&
		((pkt->partner.state & LACP_AGGREGATION) ==
			(lacp.pkt.actor.state & LACP_AGGREGATION)))
	{
		lacp.pkt.partner.state  |= LACP_SYNCHRONIZATION;
	}
	if (!(pkt->actor.state & LACP_AGGREGATION)) {
		lacp.pkt.partner.state |= LACP_SYNCHRONIZATION;
	}

	/* ACTIVITY? */
}

static inline int lacp_timer_expired(unsigned long now, unsigned long when)
{
	return when && (now > when);
}

static inline void lacp_start_periodic_timer(unsigned long now)
{
	if ((lacp.pkt.partner.state & LACP_ACTIVITY) ||
		(lacp.pkt.actor.state & LACP_ACTIVITY)) {
		lacp.periodic_timer = now +
			(((lacp.pkt.partner.state & LACP_TIMEOUT)?
				FAST_PERIODIC_TIME : SLOW_PERIODIC_TIME));
	}
}

static inline void lacp_start_current_while_timer(unsigned long now)
{
	lacp.current_while_timer = now +
		((lacp.pkt.actor.state & LACP_TIMEOUT) ?
		SHORT_TIMEOUT_TIME : LONG_TIMEOUT_TIME);

	lacp.pkt.actor.state &= ~LACP_EXPIRED;
}

static void send_lacp_reports(unsigned long now, int ntt)
{
	if (memcmp(nic.node_addr, lacp.pkt.actor.system, ETH_ALEN) != 0) {
		lacp_init_state(nic.node_addr);
	}
	/* If the remote information has expired I need to take action */
	if (lacp_timer_expired(now, lacp.current_while_timer)) {
		if (!(lacp.pkt.actor.state & LACP_EXPIRED)) {
			lacp.pkt.partner.state &= ~LACP_SYNCHRONIZATION;
			lacp.pkt.partner.state |= LACP_TIMEOUT;
			lacp.pkt.actor.state |= LACP_EXPIRED;
			lacp.current_while_timer = now + SHORT_TIMEOUT_TIME;
			ntt = 1;
		}
		else {
			lacp_init_state(nic.node_addr);
		}
	}
	/* If the periodic timer has expired I need to transmit */
	if (lacp_timer_expired(now, lacp.periodic_timer)) {
		ntt = 1;
		/* Reset by lacp_start_periodic_timer */
	}
	if (ntt) {
		eth_transmit(slow_dest, ETH_P_SLOW, sizeof(lacp.pkt), &lacp.pkt);

		/* Restart the periodic timer */
		lacp_start_periodic_timer(now);

		print_lacp("Trasmitted", &lacp.pkt, now);
	}
}

static inline void send_eth_slow_reports(unsigned long now)
{
	send_lacp_reports(now, 0);
}

static inline void process_eth_slow(unsigned short ptype, unsigned long now)
{
	union slow_union *pkt;
	if ((ptype != ETH_P_SLOW) || 
		(nic.packetlen < (ETH_HLEN + sizeof(pkt->header)))) {
		return;
	}
	pkt = (union slow_union *)&nic.packet[ETH_HLEN];
	if ((pkt->header.subtype == SLOW_SUBTYPE_LACP) &&
		(nic.packetlen >= ETH_HLEN + sizeof(pkt->lacp))) {
		int ntt;
		if (memcmp(nic.node_addr, lacp.pkt.actor.system, ETH_ALEN) != 0) {
			lacp_init_state(nic.node_addr);
		}
		/* As long as nic.packet is 2 byte aligned all is good */
		print_lacp("Received", &pkt->lacp, now);
		/* I don't actually implement the MUX or SELECT
		 * machines.  
		 *
		 * What logically happens when the client and I
		 * disagree about an aggregator is the current
		 * aggregtator is unselected.  The MUX machine places
		 * me in DETACHED.  The SELECT machine runs and
		 * reslects the same aggregator.  If I go through
		 * these steps fast enough an outside observer can not
		 * notice this.  
		 *
		 * Since the process will not generate any noticeable
		 * effect it does not need an implmenetation.  This
		 * keeps the code simple and the code and binary
		 * size down.
		 */
		/* lacp_update_selected(&pkt->lacp); */
		ntt = lacp_update_ntt(&pkt->lacp);
		lacp_record_pdu(&pkt->lacp);
		lacp_start_current_while_timer(now);
		send_lacp_reports(now, ntt);
	}
	/* If we receive a marker information packet return it */
	else if ((pkt->header.subtype == SLOW_SUBTYPE_MARKER) &&
		(nic.packetlen >= ETH_HLEN + sizeof(pkt->marker)) &&
		(pkt->marker.tlv_type == MARKER_TLV_INFO) &&
		(pkt->marker.marker_length == 0x16)) 
	{
		pkt->marker.tlv_type = MARKER_TLV_RESPONSE;
		eth_transmit(slow_dest, ETH_P_SLOW, 
			sizeof(pkt->marker), &pkt->marker);
	}

 }
#else

#define send_eth_slow_reports(now)    do {} while(0)
#define process_eth_slow(ptype, now)  do {} while(0)

#endif 
