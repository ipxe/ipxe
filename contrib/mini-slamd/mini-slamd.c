/*
 * mini-slamd
 * (c) 2002 Eric Biederman
 */

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

/*
 * To specify the default interface for multicast packets use:
 * route add -net 224.0.0.0 netmask 240.0.0.0 dev eth1
 * This server is stupid and does not override the default.
 */

/* Sever states.
 *
 * Waiting for clients.
 * Sending data to clients.
 * Pinging clients for data.
 *
 */
#define SLAM_PORT 10000
#define SLAM_MULTICAST_IP ((239<<24)|(255<<16)|(1<<8)|(1<<0))
#define SLAM_MULTICAST_PORT 10000
#define SLAM_MULTICAST_TTL 1
#define SLAM_MULTICAST_LOOPBACK 1
#define SLAM_MAX_CLIENTS 10

#define SLAM_PING_TIMEOUT	100 /* ms */

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
 *   block packets
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
#define MIN_HDR (1 + 1 + 1) /* transaction, total size, block size */

#define MAX_DATA_HDR (MAX_HDR + 7) /* header, packet # */
#define MIN_DATA_HDR (MAX_HDR + 1) /* header, packet # */

/* ETH_MAX_MTU 1500 - sizeof(iphdr) 20  - sizeof(udphdr) 8 = 1472 */
#define SLAM_MAX_NACK		(1500 - (20 + 8))
/* ETH_MAX_MTU 1500 - sizeof(iphdr) 20  - sizeof(udphdr) 8 - MAX_HDR = 1451 */
#define SLAM_BLOCK_SIZE		(1500 - (20 + 8 + MAX_HDR))


/* Define how many debug messages you want 
 * 1 - sparse but useful
 * 2 - everything
 */
#ifndef DEBUG
#define DEBUG 0
#endif

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


static struct sockaddr_in client[SLAM_MAX_CLIENTS];
static int clients;


void del_client(struct sockaddr_in *old)
{
	int i;
	for(i = 0; i < clients; i++) {
		if ((client[i].sin_family == old->sin_family) &&
			(client[i].sin_addr.s_addr == old->sin_addr.s_addr) &&
			(client[i].sin_port == old->sin_port)) {
			memmove(&client[i], &client[i+1],
				(clients - (i+1))*sizeof(client[0]));
			clients--;
		}
	}
}

void add_client(struct sockaddr_in *new)
{
	del_client(new);
	if (clients >= SLAM_MAX_CLIENTS)
		return;
	memcpy(&client[clients], new, sizeof(*new));
	clients++;
}

void push_client(struct sockaddr_in *new)
{
	del_client(new);
	if (clients >= SLAM_MAX_CLIENTS) {
		clients--;
	}
	memmove(&client[1], &client[0], clients*sizeof(*new));
	memcpy(&client[0], new, sizeof(*new));
	clients++;
}


void next_client(struct sockaddr_in *next)
{
	/* Find the next client we want to ping next */
	if (!clients) {
		next->sin_family = AF_UNSPEC;
		return;
	}
	/* Return the first client */
	memcpy(next, &client[0], sizeof(*next));
}

int main(int argc, char **argv)
{
	char *filename;
	uint8_t nack_packet[SLAM_MAX_NACK];
	int nack_len;
	uint8_t request_packet[MAX_HDR];
	int request_len;
	uint8_t data_packet[MAX_DATA_HDR +  SLAM_BLOCK_SIZE];
	int data_len;
	uint8_t *ptr, *end;
	struct sockaddr_in master_client;
	struct sockaddr_in sa_src;
	struct sockaddr_in sa_mcast;
	uint8_t mcast_ttl;
	uint8_t mcast_loop;
	int sockfd, filefd;
	int result;
	struct pollfd fds[1];
	int state;
#define STATE_PINGING      1
#define STATE_WAITING      2
#define STATE_RECEIVING    3
#define STATE_TRANSMITTING 4
	off_t size;
	struct stat st;
	uint64_t transaction;
	unsigned long packet;
	unsigned long packet_count;
	unsigned slam_port, slam_multicast_port;
	struct in_addr slam_multicast_ip;

	slam_port = SLAM_PORT;
	slam_multicast_port = SLAM_MULTICAST_PORT;
	slam_multicast_ip.s_addr = htonl(SLAM_MULTICAST_IP);
	
	if (argc != 2) {
		fprintf(stderr, "Bad argument count\n");
		fprintf(stderr, "Usage: mini-slamd filename\n");
		exit(EXIT_FAILURE);
	}
	filename = argv[1];
	filefd = -1;
	size = 0;
	transaction = 0;

	/* Setup the udp socket */
	sockfd = socket(PF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Cannot create socket\n");
		exit(EXIT_FAILURE);
	}
	memset(&sa_src, 0, sizeof(sa_src));
	sa_src.sin_family = AF_INET;
	sa_src.sin_port = htons(slam_port);
	sa_src.sin_addr.s_addr = INADDR_ANY;

	result = bind(sockfd, &sa_src, sizeof(sa_src));
	if (result < 0) { 
		fprintf(stderr, "Cannot bind socket to port %d\n", 
			ntohs(sa_src.sin_port));
		exit(EXIT_FAILURE);
	}

	/* Setup the multicast transmission address */
	memset(&sa_mcast, 0, sizeof(sa_mcast));
	sa_mcast.sin_family = AF_INET;
	sa_mcast.sin_port = htons(slam_multicast_port);
	sa_mcast.sin_addr.s_addr = slam_multicast_ip.s_addr;
	if (!IN_MULTICAST(ntohl(sa_mcast.sin_addr.s_addr))) {
		fprintf(stderr, "Not a multicast ip\n");
		exit(EXIT_FAILURE);
	}

	/* Set the multicast ttl */
	mcast_ttl = SLAM_MULTICAST_TTL;
	setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL,
		&mcast_ttl, sizeof(mcast_ttl));

	/* Set the multicast loopback status */
	mcast_loop = SLAM_MULTICAST_LOOPBACK;
	setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, &mcast_loop, sizeof(mcast_loop));


	state = STATE_WAITING;
	packet = 0;
	packet_count = 0;
	fds[0].fd = sockfd;
	fds[0].events = POLLIN;
	fds[0].revents = 0;
	for(;;) {
		switch(state) {
		case STATE_PINGING:
			state = STATE_WAITING;
			next_client(&master_client);
			if (master_client.sin_family == AF_UNSPEC) {
				break;
			}
#if DEBUG
			printf("Pinging %s:%d\n", 
				inet_ntoa(master_client.sin_addr),
				ntohs(master_client.sin_port));
			fflush(stdout);
#endif

			/* Prepare the request packet, it is all header */
			ptr = request_packet;
			end = &request_packet[sizeof(request_packet) -1];
			slam_encode(&ptr, end, transaction);
			slam_encode(&ptr, end, size);
			slam_encode(&ptr, end, SLAM_BLOCK_SIZE);
			request_len = ptr - request_packet;

			result = sendto(sockfd, request_packet, request_len, 0,
				&master_client, sizeof(master_client));
			/* Forget the client I just asked, when the reply
			 * comes in we will remember it again.
			 */
			del_client(&master_client);
			break;
		case STATE_WAITING:
		{
			int timeout;
			int from_len;
			timeout = -1;
			if (master_client.sin_family != AF_UNSPEC) {
				timeout = SLAM_PING_TIMEOUT;
			}
			result = poll(fds, sizeof(fds)/sizeof(fds[0]), timeout);
			if (result == 0) {
				/* On a timeout try the next client */
				state = STATE_PINGING;
				break;
			}
			if (result > 0) {
				from_len = sizeof(master_client);
				result = recvfrom(sockfd, 
					nack_packet, 	sizeof(nack_packet), 0,
					&master_client, &from_len);
				if (result < 0)
					break;
				nack_len = result;
#if DEBUG
				printf("Received Nack from %s:%d\n",
					inet_ntoa(master_client.sin_addr),
					ntohs(master_client.sin_port));
				fflush(stdout);
#endif
#if DEBUG
				{
					ptr = nack_packet;
					end = ptr + result;
					packet = 0;
					result = 0;
					while(ptr < end) {
						packet += slam_decode(&ptr, end, &result);
						if (result < 0) break;
						packet_count = slam_decode(&ptr, end, &result);
						if (result < 0) break;
						printf("%d-%d ",
							packet, packet + packet_count -1);
					}
					printf("\n");
					fflush(stdout);
				}
#endif
				/* Forget this client temporarily.
				 * If the packet appears good they will be
				 * readded.
				 */
				del_client(&master_client);
				ptr = nack_packet;
				end = ptr + nack_len;
				result = 0;
				packet = slam_decode(&ptr, end, &result);
				if (result < 0)
					break;
				packet_count = slam_decode(&ptr, end, &result);
				if (result < 0)
					break;
				/* We appear to have a good packet, keep
				 * this client.
				 */
				push_client(&master_client);

				/* Reopen the file to transmit */
				if (filefd != -1) {
					close(filefd);
				}
				filefd = open(filename, O_RDONLY);
				if (filefd < 0) {
					fprintf(stderr, "Cannot open %s: %s\n",
						filename, strerror(errno));
					break;
				}
				size = lseek(filefd, 0, SEEK_END);
				if (size < 0) {
					fprintf(stderr, "Seek failed on %s: %s\n",
						filename, strerror(errno));
					break;
				}
				result = fstat(filefd, &st);
				if (result < 0) {
					fprintf(stderr, "Stat failed on %s: %s\n",
						filename, strerror(errno));
					break;
				}
				transaction = st.st_mtime;
				
				state = STATE_TRANSMITTING;
				break;
			}
			break;
		}
		case STATE_RECEIVING:
			/* Now clear the queue of received packets */
		{
			struct sockaddr_in from;
			int from_len;
			uint8_t dummy_packet[SLAM_MAX_NACK];
			state = STATE_TRANSMITTING;
			result = poll(fds, sizeof(fds)/sizeof(fds[0]), 0);
			if (result < 1)
				break;
			from_len = sizeof(from);
			result = recvfrom(sockfd, 
				dummy_packet, sizeof(dummy_packet), 0,
				&from, &from_len);
			if (result <= 0)
				break;
#if DEBUG				
			printf("Received Nack from %s:%d\n",
				inet_ntoa(from.sin_addr),
				ntohs(from.sin_port));
			fflush(stdout);
#endif
			/* Receive packets until I don't get any more */
			state = STATE_RECEIVING;
			/* Process a  packet */
			if (dummy_packet[0] == '\0') {
				/* If the first byte is null it is a disconnect
				 * packet.  
				 */
				del_client(&from);
			}
			else {
				/* Otherwise attempt to add the client. */
				add_client(&from);
			}
			break;
		}
		case STATE_TRANSMITTING:
		{
			off_t off;
			off_t offset;
			ssize_t bytes;
			uint8_t *ptr2, *end2;

			/* After I transmit a packet check for packets to receive. */
			state = STATE_RECEIVING;

			/* Find the packet to transmit */
			offset = packet * SLAM_BLOCK_SIZE;

			/* Seek to the desired packet */
			off = lseek(filefd, offset, SEEK_SET);
			if ((off < 0) || (off != offset)) {
		 		fprintf(stderr, "Seek failed on %s:%s\n",
					filename, strerror(errno));
				break;
			}
			/* Encode the packet header */
			ptr2 = data_packet;
			end2 = data_packet + sizeof(data_packet);
			slam_encode(&ptr2, end2, transaction);
			slam_encode(&ptr2, end2, size);
			slam_encode(&ptr2, end2, SLAM_BLOCK_SIZE);
			slam_encode(&ptr2, end2, packet);
			data_len = ptr2 - data_packet;
			
			/* Read in the data */
			bytes = read(filefd, &data_packet[data_len], 
				SLAM_BLOCK_SIZE);
			if (bytes <= 0) {
				fprintf(stderr, "Read failed on %s:%s\n",
					filename, strerror(errno));
				break;
			}
			data_len += bytes;
			/* Write out the data */
			result = sendto(sockfd, data_packet, data_len, 0,
				&sa_mcast, sizeof(sa_mcast));
			if (result != data_len) {
				fprintf(stderr, "Send failed %s\n",
					strerror(errno));
				break;
			}
#if DEBUG > 1
			printf("Transmitted: %d\n", packet);
			fflush(stdout);
#endif
			/* Compute the next packet */
			packet++;
			packet_count--;
			if (packet_count == 0) {
				packet += slam_decode(&ptr, end, &result);
				if (result >= 0)
					packet_count = slam_decode(&ptr, end, &result);
				if (result < 0) {
					/* When a transmission is done close the file,
					 * so it may be updated.  And then ping then start
					 * pinging clients to get the transmission started
					 * again.
					 */
					state = STATE_PINGING;
					close(filefd);
					filefd = -1;
					break;
				}
			}
			break;
		}
		}
	}
	return EXIT_SUCCESS;
}
