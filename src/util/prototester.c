#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <getopt.h>

typedef int irq_action_t;

struct nic {
	struct nic_operations	*nic_op;
	unsigned char		*node_addr;
	unsigned char		*packet;
	unsigned int		packetlen;
	void			*priv_data;	/* driver private data */
};

struct nic_operations {
	int ( *connect ) ( struct nic * );
	int ( *poll ) ( struct nic *, int retrieve );
	void ( *transmit ) ( struct nic *, const char *,
			     unsigned int, unsigned int, const char * );
	void ( *irq ) ( struct nic *, irq_action_t );
};

/*****************************************************************************
 *
 * Hijack device interface
 *
 * This requires a hijack daemon to be running
 *
 */

struct hijack {
	int fd;
};

struct hijack_device {
	char *name;
};

static int hijack_poll ( struct nic *nic, int retrieve ) {
	struct hijack *hijack = nic->priv_data;
	fd_set fdset;
	struct timeval tv;
	int ready;
	ssize_t len;

	/* Poll for data */
	FD_ZERO ( &fdset );
	FD_SET ( hijack->fd, &fdset );
	tv.tv_sec = 0;
	tv.tv_usec = 500; /* 500us to avoid hogging CPU */
	ready = select ( ( hijack->fd + 1 ), &fdset, NULL, NULL, &tv );
	if ( ready < 0 ) {
		fprintf ( stderr, "select() failed: %s\n",
			  strerror ( errno ) );
		return 0;
	}
	if ( ready == 0 )
		return 0;

	/* Return if we're not retrieving data yet */
	if ( ! retrieve )
		return 1;

	/* Fetch data */
	len = read ( hijack->fd, nic->packet, ETH_FRAME_LEN );
	if ( len < 0 ) {
		fprintf ( stderr, "read() failed: %s\n",
			  strerror ( errno ) );
		return 0;
	}
	nic->packetlen = len;

	return 1;
}

static void hijack_transmit ( struct nic *nic, const char *dest,
			      unsigned int type, unsigned int size,
			      const char *packet ) {
	struct hijack *hijack = nic->priv_data;
	unsigned int nstype = htons ( type );
	unsigned int total_size = ETH_HLEN + size;
	char txbuf[ total_size ];

	/* Build packet header */
	memcpy ( txbuf, dest, ETH_ALEN );
	memcpy ( txbuf + ETH_ALEN, nic->node_addr, ETH_ALEN );
	memcpy ( txbuf + 2 * ETH_ALEN, &nstype, 2 );
	memcpy ( txbuf + ETH_HLEN, packet, size );

	/* Transmit data */
	if ( write ( hijack->fd, txbuf, total_size ) != total_size ) {
		fprintf ( stderr, "write() failed: %s\n",
			  strerror ( errno ) );
	}
}

static int hijack_connect ( struct nic *nic ) {
	return 1;
}

static void hijack_irq ( struct nic *nic, irq_action_t action ) {
	/* Do nothing */
}

static struct nic_operations hijack_operations = {
	.connect	= hijack_connect,
	.transmit	= hijack_transmit,
	.poll		= hijack_poll,
	.irq		= hijack_irq,
};

static int hijack_probe ( struct nic *nic, struct hijack_device *hijack_dev ) {
	static struct hijack hijack;
	struct sockaddr_un sun;
	int i;

	memset ( &hijack, 0, sizeof ( hijack ) );

	/* Create socket */
	hijack.fd = socket ( PF_UNIX, SOCK_SEQPACKET, 0 );
	if ( hijack.fd < 0 ) {
		fprintf ( stderr, "socket() failed: %s\n",
			  strerror ( errno ) );
		goto err;
	}

	/* Connect to hijack daemon */
	sun.sun_family = AF_UNIX;
	snprintf ( sun.sun_path, sizeof ( sun.sun_path ), "/var/run/hijack-%s",
		   hijack_dev->name );
	if ( connect ( hijack.fd, ( struct sockaddr * ) &sun,
		       sizeof ( sun ) ) < 0 ) {
		fprintf ( stderr, "could not connect to %s: %s\n",
			  sun.sun_path, strerror ( errno ) );
		goto err;
	}

	/* Generate MAC address */
	srand ( time ( NULL ) );
	for ( i = 0 ; i < ETH_ALEN ; i++ ) {
		nic->node_addr[i] = ( rand() & 0xff );
	}
	nic->node_addr[0] &= 0xfe; /* clear multicast bit */
	nic->node_addr[0] |= 0x02; /* set "locally-assigned" bit */

	nic->priv_data = &hijack;
	nic->nic_op = &hijack_operations;
	return 1;

 err:
	if ( hijack.fd >= 0 )
		close ( hijack.fd );
	return 0;
}

static void hijack_disable ( struct nic *nic,
			     struct hijack_device *hijack_dev ) {
	struct hijack *hijack = nic->priv_data;
	
	close ( hijack->fd );
}

/*****************************************************************************
 *
 * Parse command-line options
 *
 */

struct tester_options {
	char interface[IF_NAMESIZE];
};

static void usage ( char **argv ) {
	fprintf ( stderr,
		  "Usage: %s [options]\n"
		  "\n"
		  "Options:\n"
		  "  -h|--help              Print this help message\n"
		  "  -i|--interface intf    Use specified network interface\n",
		  argv[0] );
}

static int parse_options ( int argc, char **argv,
			   struct tester_options *options ) {
	static struct option long_options[] = {
		{ "interface", 1, NULL, 'i' },
		{ "help", 0, NULL, 'h' },
		{ },
	};
	int c;

	/* Set default options */
	memset ( options, 0, sizeof ( *options ) );
	strncpy ( options->interface, "eth0", sizeof ( options->interface ) );

	/* Parse command-line options */
	while ( 1 ) {
		int option_index = 0;
		
		c = getopt_long ( argc, argv, "i:h", long_options,
				  &option_index );
		if ( c < 0 )
			break;

		switch ( c ) {
		case 'i':
			strncpy ( options->interface, optarg,
				  sizeof ( options->interface ) );
			break;
		case 'h':
			usage( argv );
			return -1;
		case '?':
			/* Unrecognised option */
			return -1;
		default:
			fprintf ( stderr, "Unrecognised option '-%c'\n", c );
			return -1;
		}
	}

	/* Check there's nothing left over on the command line */
	if ( optind != argc ) {
		usage ( argv );
		return -1;
	}

	return 0;
}

/*****************************************************************************
 *
 * uIP wrapper layer
 *
 */

#include "../proto/uip/uip.h"
#include "../proto/uip/uip_arp.h"

static int done;

void UIP_APPCALL ( void ) {
	printf ( "appcall\n" );
}

void udp_appcall ( void ) {
}

static void uip_transmit ( struct nic *nic ) {
	uip_arp_out();
	nic->nic_op->transmit ( nic, ( char * ) uip_buf,
				ntohs ( *( ( uint16_t * ) ( uip_buf + 12 ) ) ),
				uip_len - ETH_HLEN,
				( char * ) uip_buf + ETH_HLEN );
	uip_len = 0;
}

static void run_stack ( struct nic *nic ) {
	struct uip_eth_addr hwaddr;
	u16_t ipaddr[2];
	uint16_t type;
	int i;

	uip_init();
	uip_arp_init();
	memcpy ( &hwaddr, nic->node_addr, sizeof ( hwaddr ) );
	uip_setethaddr ( hwaddr );

	uip_ipaddr(ipaddr, 192,168,0,1);
	uip_connect(ipaddr, HTONS(80));

	done = 0;
	while ( ! done ) {
		if ( nic->nic_op->poll ( nic, 1 ) ) {
			/* We have data */
			memcpy ( uip_buf, nic->packet, nic->packetlen );
			uip_len = nic->packetlen;
			type = ntohs ( *( ( uint16_t * ) ( uip_buf + 12 ) ) );
			if ( type == ETHERTYPE_ARP ) {
				uip_arp_arpin();
			} else {
				uip_arp_ipin();
				uip_input();
			}
			if ( uip_len > 0 )
				uip_transmit ( nic );
		} else {
			for ( i = 0 ; i < UIP_CONNS ; i++ ) {
				uip_periodic ( i );
				if ( uip_len > 0 )
					uip_transmit ( nic );
			}
		}
	}
}

/*****************************************************************************
 *
 * Main program
 *
 */

int main ( int argc, char **argv ) {
	struct tester_options options;
	struct hijack_device hijack_dev;
	static unsigned char node_addr[ETH_ALEN];
	static unsigned char packet[ETH_FRAME_LEN];
	struct nic nic = {
		.node_addr = node_addr,
		.packet = packet,
	};

	/* Parse command-line options */
	if ( parse_options ( argc, argv, &options ) < 0 )
		exit ( 1 );

	/* Open the hijack device */
	hijack_dev.name = options.interface;
	if ( ! hijack_probe ( &nic, &hijack_dev ) )
		exit ( 1 );

	/* Run the stack to completion */
	run_stack ( &nic );

	/* Close the hijack device */
	hijack_disable ( &nic, &hijack_dev );

	return 0;
}
