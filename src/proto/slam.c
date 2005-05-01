#ifdef DOWNLOAD_PROTO_SLAM
#include "etherboot.h"
#include "nic.h"

#define SLAM_PORT 10000
#define SLAM_MULTICAST_IP ((239<<24)|(255<<16)|(1<<8)|(1<<0))
#define SLAM_MULTICAST_PORT 10000
#define SLAM_LOCAL_PORT 10000

/* Set the timeout intervals to at least 1 second so
 * on a 100Mbit ethernet can receive 10000 packets
 * in one second.  
 *
 * The only case that is likely to trigger all of the nodes
 * firing a nack packet is a slow server.  The odds of this
 * happening could be reduced being slightly smarter and utilizing 
 * the multicast channels for nacks.   But that only improves the odds
 * it doesn't improve the worst case.  So unless this proves to be
 * a common case having the control data going unicast should increase
 * the odds of the data not being dropped.  
 *
 * When doing exponential backoff we increase just the timeout
 * interval and not the base to optimize for throughput.  This is only
 * expected to happen when the server is down.  So having some nodes
 * pinging immediately should get the transmission restarted quickly after a
 * server restart.  The host nic won't be to baddly swamped because of
 * the random distribution of the nodes.
 *
 */
#define SLAM_INITIAL_MIN_TIMEOUT      (TICKS_PER_SEC/3)
#define SLAM_INITIAL_TIMEOUT_INTERVAL (TICKS_PER_SEC)
#define SLAM_BASE_MIN_TIMEOUT         (2*TICKS_PER_SEC)
#define SLAM_BASE_TIMEOUT_INTERVAL    (4*TICKS_PER_SEC)
#define SLAM_BACKOFF_LIMIT 5
#define SLAM_MAX_RETRIES 20

/*** Packets Formats ***
 * Data Packet:
 *   transaction
 *   total bytes
 *   block size
 *   packet #
 *   data
 *
 * Status Request Packet
 *   transaction
 *   total bytes
 *   block size
 *
 * Status Packet
 *   received packets
 *   requested packets
 *   received packets
 *   requested packets
 *   ...
 *   received packets
 *   requested packtes
 *   0
 */

#define MAX_HDR (7 + 7 + 7) /* transaction, total size, block size */
#define MIN_HDR (1 + 1 + 1) /* transactino, total size, block size */

#define MAX_SLAM_REQUEST MAX_HDR
#define MIN_SLAM_REQUEST MIN_HDR

#define MIN_SLAM_DATA (MIN_HDR + 1)

static struct slam_nack {
	struct iphdr ip;
	struct udphdr udp;
	unsigned char data[ETH_MAX_MTU - 
		(sizeof(struct iphdr) + sizeof(struct udphdr))];
} nack;

struct slam_state {
	unsigned char hdr[MAX_HDR];
	unsigned long hdr_len;
	unsigned long block_size;
	unsigned long total_bytes;
	unsigned long total_packets;

	unsigned long received_packets;

	unsigned char *image;
	unsigned char *bitmap;
} state;


static void init_slam_state(void)
{
	state.hdr_len = sizeof(state.hdr);
	memset(state.hdr, 0, state.hdr_len);
	state.block_size = 0;
	state.total_packets = 0;

	state.received_packets = 0;

	state.image = 0;
	state.bitmap = 0;
}

struct slam_info {
	in_addr server_ip;
	in_addr multicast_ip;
	in_addr local_ip;
	uint16_t server_port;
	uint16_t multicast_port;
	uint16_t local_port;
	int (*fnc)(unsigned char *, unsigned int, unsigned int, int);
	int sent_nack;
};

#define SLAM_TIMEOUT 0
#define SLAM_REQUEST 1
#define SLAM_DATA    2
static int await_slam(int ival __unused, void *ptr,
	unsigned short ptype __unused, struct iphdr *ip, struct udphdr *udp)
{
	struct slam_info *info = ptr;
	if (!udp) {
		return 0;
	}
	/* I can receive two kinds of packets here, a multicast data packet,
	 * or a unicast request for information 
	 */
	/* Check for a data request packet */
	if ((ip->dest.s_addr == arptable[ARP_CLIENT].ipaddr.s_addr) &&
		(ntohs(udp->dest) == info->local_port) && 
		(nic.packetlen >= 
			ETH_HLEN + 
			sizeof(struct iphdr) + 
			sizeof(struct udphdr) +
			MIN_SLAM_REQUEST)) {
		return SLAM_REQUEST;
	}
	/* Check for a multicast data packet */
	if ((ip->dest.s_addr == info->multicast_ip.s_addr) &&
		(ntohs(udp->dest) == info->multicast_port) &&
		(nic.packetlen >= 
			ETH_HLEN + 
			sizeof(struct iphdr) + 
			sizeof(struct udphdr) +
			MIN_SLAM_DATA)) {
		return SLAM_DATA;
	}
#if 0
	printf("#");
	printf("dest: %@ port: %d len: %d\n", 
		ip->dest.s_addr, ntohs(udp->dest), nic.packetlen);
#endif
	return 0;
		
}

static int slam_encode(
	unsigned char **ptr, unsigned char *end, unsigned long value)
{
	unsigned char *data = *ptr;
	int bytes;
	bytes = sizeof(value);
	while ((bytes > 0) && ((0xff & (value >> ((bytes -1)<<3))) == 0)) {
		bytes--;
	}
	if (bytes <= 0) {
		bytes = 1;
	}
	if (data + bytes >= end) {
		return -1;
	}
	if ((0xe0 & (value >> ((bytes -1)<<3))) == 0) {
		/* packed together */
		*data = (bytes << 5) | (value >> ((bytes -1)<<3));
	} else {
		bytes++;
		*data = (bytes << 5);
	}
	bytes--;
	data++;
	while(bytes) {
		*(data++) = 0xff & (value >> ((bytes -1)<<3));
		bytes--;
	}
	*ptr = data;
	return 0;
}

static int slam_skip(unsigned char **ptr, unsigned char *end) 
{
	int bytes;
	if (*ptr >= end) {
		return -1;
	}
	bytes = ((**ptr) >> 5) & 7;
	if (bytes == 0) {
		return -1;
	}
	if (*ptr + bytes >= end) {
		return -1;
	}
	(*ptr) += bytes;
	return 0;
	
}

static unsigned long slam_decode(unsigned char **ptr, unsigned char *end, int *err)
{
	unsigned long value;
	unsigned bytes;
	if (*ptr >= end) {
		*err = -1;
	}
	bytes = ((**ptr) >> 5) & 7;
	if ((bytes == 0) || (bytes > sizeof(unsigned long))) {
		*err = -1;
		return 0;
	}
	if ((*ptr) + bytes >= end) {
		*err =  -1;
	}
	value = (**ptr) & 0x1f;
	bytes--;
	(*ptr)++;
	while(bytes) {
		value <<= 8;
		value |= **ptr;
		(*ptr)++;
		bytes--;
	}
	return value;
}


static long slam_sleep_interval(int exp)
{
	long range;
	long divisor;
	long interval;
	range = SLAM_BASE_TIMEOUT_INTERVAL;
	if (exp < 0) { 
		divisor = RAND_MAX/SLAM_INITIAL_TIMEOUT_INTERVAL;
	} else {
		if (exp > SLAM_BACKOFF_LIMIT) 
			exp = SLAM_BACKOFF_LIMIT;
		divisor = RAND_MAX/(range << exp);
	}
	interval = random()/divisor;
	if (exp < 0) {
		interval += SLAM_INITIAL_MIN_TIMEOUT;
	} else {
		interval += SLAM_BASE_MIN_TIMEOUT;
	}
	return interval;
}


static unsigned char *reinit_slam_state(
	unsigned char *header, unsigned char *end)
{
	unsigned long total_bytes;
	unsigned long block_size;

	unsigned long bitmap_len;
	unsigned long max_packet_len;
	unsigned char *data;
	int err;

#if 0
	printf("reinit\n");
#endif
	data = header;

	state.hdr_len = 0;
	err = slam_skip(&data, end); /* transaction id */
	total_bytes = slam_decode(&data, end, &err);
	block_size  = slam_decode(&data, end, &err);
	if (err) {
		printf("ALERT: slam size out of range\n");
		return 0;
	}
	state.block_size = block_size;
	state.total_bytes = total_bytes;
	state.total_packets = (total_bytes + block_size - 1)/block_size;
	state.hdr_len = data - header;
	state.received_packets = 0;

	data = state.hdr;
	slam_encode(&data, &state.hdr[sizeof(state.hdr)], state.total_packets);
	max_packet_len = data - state.hdr;
	memcpy(state.hdr, header, state.hdr_len);
	
#if 0
	printf("block_size:     %ld\n", block_size);
	printf("total_bytes:    %ld\n", total_bytes);
	printf("total_packets:  %ld\n", state.total_packets);
	printf("hdr_len:        %ld\n", state.hdr_len);
	printf("max_packet_len: %ld\n", max_packet_len);
#endif

	if (state.block_size > ETH_MAX_MTU - (
		sizeof(struct iphdr) + sizeof(struct udphdr) +
		state.hdr_len + max_packet_len)) {
		printf("ALERT: slam blocksize to large\n");
		return 0;
	}
	if (state.bitmap) {
		forget(state.bitmap);
	}
	bitmap_len   = (state.total_packets + 1 + 7)/8;
	state.bitmap = allot(bitmap_len);
	state.image  = allot(total_bytes);
	if ((unsigned long)state.image < 1024*1024) {
		printf("ALERT: slam filesize to large for available memory\n");
		return 0;
	}
	memset(state.bitmap, 0, bitmap_len);

	return header + state.hdr_len;
}

static int slam_recv_data(unsigned char *data)
{
	unsigned long packet;
	unsigned long data_len;
	int err;
	struct udphdr *udp;
	udp = (struct udphdr *)&nic.packet[ETH_HLEN + sizeof(struct iphdr)];
	err = 0;
	packet = slam_decode(&data, &nic.packet[nic.packetlen], &err);
	if (err || (packet > state.total_packets)) {
		printf("ALERT: Invalid packet number\n");
		return 0;
	}
	/* Compute the expected data length */
	if (packet != state.total_packets -1) {
		data_len = state.block_size;
	} else {
		data_len = state.total_bytes % state.block_size;
	}
	/* If the packet size is wrong drop the packet and then continue */
	if (ntohs(udp->len) != (data_len + (data - (unsigned char*)udp))) {
		printf("ALERT: udp packet is not the correct size\n");
		return 1;
	}
	if (nic.packetlen < data_len + (data - nic.packet)) {
		printf("ALERT: Ethernet packet shorter than data_len\n");
		return 1;
	}
	if (data_len > state.block_size) {
		data_len = state.block_size;
	}
	if (((state.bitmap[packet >> 3] >> (packet & 7)) & 1) == 0) {
		/* Non duplicate packet */
		state.bitmap[packet >> 3] |= (1 << (packet & 7));
		memcpy(state.image + (packet*state.block_size), data, data_len);
		state.received_packets++;
	} else {
#ifdef MDEBUG
		printf("<DUP>\n");
#endif
	}
	return 1;
}

static void transmit_nack(unsigned char *ptr, struct slam_info *info)
{
	int nack_len;
	/* Ensure the packet is null terminated */
	*ptr++ = 0;
	nack_len = ptr - (unsigned char *)&nack;
	build_udp_hdr(info->server_ip.s_addr, 
		info->local_port, info->server_port, 1, nack_len, &nack);
	ip_transmit(nack_len, &nack);
#if defined(MDEBUG) && 0
	printf("Sent NACK to %@ bytes: %d have:%ld/%ld\n", 
		info->server_ip, nack_len,
		state.received_packets, state.total_packets);
#endif
}

static void slam_send_nack(struct slam_info *info)
{
	unsigned char *ptr, *end;
	/* Either I timed out or I was explicitly 
	 * asked for a request packet 
	 */
	ptr = &nack.data[0];
	/* Reserve space for the trailling null */
	end = &nack.data[sizeof(nack.data) -1]; 
	if (!state.bitmap) {
		slam_encode(&ptr, end, 0);
		slam_encode(&ptr, end, 1);
	}
	else {
		/* Walk the bitmap */
		unsigned long i;
		unsigned long len;
		unsigned long max;
		int value;
		int last;
		/* Compute the last bit and store an inverted trailer */
		max = state.total_packets;
		value = ((state.bitmap[(max -1) >> 3] >> ((max -1) & 7) ) & 1);
		value = !value;
		state.bitmap[max >> 3] &= ~(1 << (max & 7));
		state.bitmap[max >> 3] |= value << (max & 7);

		len = 0;
		last = 1; /* Start with the received packets */
		for(i = 0; i <= max; i++) {
			value = (state.bitmap[i>>3] >> (i & 7)) & 1;
			if (value == last) {
				len++;
			} else {
				if (slam_encode(&ptr, end, len))
					break;
				last = value;
				len = 1;
			}
		}
	}
	info->sent_nack = 1;
	transmit_nack(ptr, info);
}

static void slam_send_disconnect(struct slam_info *info)
{
	if (info->sent_nack) {
		/* A disconnect is a packet with just the null terminator */
		transmit_nack(&nack.data[0], info);
	}
	info->sent_nack = 0;
}


static int proto_slam(struct slam_info *info)
{
	int retry;
	long timeout;

	init_slam_state();

	retry = -1;
	rx_qdrain();
	/* Arp for my server */
	if (arptable[ARP_SERVER].ipaddr.s_addr != info->server_ip.s_addr) {
		arptable[ARP_SERVER].ipaddr.s_addr = info->server_ip.s_addr;
		memset(arptable[ARP_SERVER].node, 0, ETH_ALEN);
	}
	/* If I'm running over multicast join the multicast group */
	join_group(IGMP_SERVER, info->multicast_ip.s_addr);
	for(;;) {
		unsigned char *header;
		unsigned char *data;
		int type;
		header = data = 0;

		timeout = slam_sleep_interval(retry);
		type = await_reply(await_slam, 0, info, timeout);
		/* Compute the timeout for next time */
		if (type == SLAM_TIMEOUT) {
			/* If I timeouted recompute the next timeout */
			if (retry++ > SLAM_MAX_RETRIES) {
				return 0;
			}
		} else {
			retry = 0;
		}
		if ((type == SLAM_DATA) || (type == SLAM_REQUEST)) {
			/* Check the incomming packet and reinit the data 
			 * structures if necessary.
			 */
			header = &nic.packet[ETH_HLEN + 
				sizeof(struct iphdr) + sizeof(struct udphdr)];
			data = header + state.hdr_len;
			if (memcmp(state.hdr, header, state.hdr_len) != 0) {
				/* Something is fishy reset the transaction */
				data = reinit_slam_state(header, &nic.packet[nic.packetlen]);
				if (!data) {
					return 0;
				}
			}
		}
		if (type == SLAM_DATA) {
			if (!slam_recv_data(data)) {
				return 0;
			}
			if (state.received_packets == state.total_packets) {
				/* We are done get out */
				break;
			}
		}
		if ((type == SLAM_TIMEOUT) || (type == SLAM_REQUEST)) {
			/* Either I timed out or I was explicitly 
			 * asked by a request packet 
			 */
			slam_send_nack(info);
		}
	}
	slam_send_disconnect(info);

	/* Leave the multicast group */
	leave_group(IGMP_SERVER);
	/* FIXME don't overwrite myself */
	/* load file to correct location */
	return info->fnc(state.image, 1, state.total_bytes, 1);
}


int url_slam(const char *name, int (*fnc)(unsigned char *, unsigned int, unsigned int, int))
{
	struct slam_info info;
	/* Set the defaults */
	info.server_ip.s_addr    = arptable[ARP_SERVER].ipaddr.s_addr;
	info.server_port         = SLAM_PORT;
	info.multicast_ip.s_addr = htonl(SLAM_MULTICAST_IP);
	info.multicast_port      = SLAM_MULTICAST_PORT;
	info.local_ip.s_addr     = arptable[ARP_CLIENT].ipaddr.s_addr;
	info.local_port          = SLAM_LOCAL_PORT;
	info.fnc                 = fnc;
	info.sent_nack = 0;
	/* Now parse the url */
	if (url_port != -1) {
		info.server_port = url_port;
	}
	if (name[0]) {
		/* multicast ip */
		name += inet_aton(name, &info.multicast_ip);
		if (name[0] == ':') {
			name++;
			info.multicast_port = strtoul(name, &name, 10);
		}
	}
	if (name[0]) {
		printf("\nBad url\n");
		return 0;
	}
	return proto_slam(&info);
}

#endif /* DOWNLOAD_PROTO_SLAM */
