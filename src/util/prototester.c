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
#include <getopt.h>
#include <assert.h>

#include <gpxe/ip.h>
#include <gpxe/tcp.h>
#include <gpxe/hello.h>
#include <gpxe/iscsi.h>

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
 * Net device layer
 *
 */

#include "../proto/uip/uip_arp.h"

static unsigned char node_addr[ETH_ALEN];
static unsigned char packet[ETH_FRAME_LEN];
struct nic static_nic = {
	.node_addr = node_addr,
	.packet = packet,
};

/* Must be a macro because priv_data[] is of variable size */
#define alloc_netdevice( priv_size ) ( {	\
	static char priv_data[priv_size];	\
	static_nic.priv_data = priv_data;	\
	&static_nic; } )

static int register_netdevice ( struct nic *nic ) {
	struct uip_eth_addr hwaddr;

	memcpy ( &hwaddr, nic->node_addr, sizeof ( hwaddr ) );
	uip_setethaddr ( hwaddr );
	return 0;
}

static inline void unregister_netdevice ( struct nic *nic ) {
	/* Do nothing */
}

static inline void free_netdevice ( struct nic *nic ) {
	/* Do nothing */
}

int netdev_poll ( int retrieve, void **data, size_t *len ) {
	int rc = static_nic.nic_op->poll ( &static_nic, retrieve );
	*data = static_nic.packet;
	*len = static_nic.packetlen;
	return rc;
}

void netdev_transmit ( const void *data, size_t len ) {
	uint16_t type = ntohs ( *( ( uint16_t * ) ( data + 12 ) ) );
	static_nic.nic_op->transmit ( &static_nic, data, type,
				      len - ETH_HLEN,
				      data + ETH_HLEN );
}

/*****************************************************************************
 *
 * Utility functions
 *
 */

static void hex_dump ( const void *data, size_t len ) {
	unsigned int index;
	for ( index = 0; index < len; index++ ) {
		if ( ( index % 16 ) == 0 ) {
			printf ( "\n%08x :", index );
		}
		printf ( " %02x", * ( ( unsigned char * ) ( data + index ) ) );
	}
	printf ( "\n" );
}

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
	void *priv;
};

static inline void hijack_set_drvdata ( struct hijack_device *hijack_dev,
					void *data ) {
	hijack_dev->priv = data;
}

static inline void * hijack_get_drvdata ( struct hijack_device *hijack_dev ) {
	return hijack_dev->priv;
}

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

int hijack_probe ( struct hijack_device *hijack_dev ) {
	struct nic *nic;
	struct hijack *hijack;
	struct sockaddr_un sun;
	int i;

	nic = alloc_netdevice ( sizeof ( *hijack ) );
	if ( ! nic ) {
		fprintf ( stderr, "alloc_netdevice() failed\n" );
		goto err_alloc;
	}
	hijack = nic->priv_data;
	memset ( hijack, 0, sizeof ( *hijack ) );

	/* Create socket */
	hijack->fd = socket ( PF_UNIX, SOCK_SEQPACKET, 0 );
	if ( hijack->fd < 0 ) {
		fprintf ( stderr, "socket() failed: %s\n",
			  strerror ( errno ) );
		goto err;
	}

	/* Connect to hijack daemon */
	sun.sun_family = AF_UNIX;
	snprintf ( sun.sun_path, sizeof ( sun.sun_path ), "/var/run/hijack-%s",
		   hijack_dev->name );
	if ( connect ( hijack->fd, ( struct sockaddr * ) &sun,
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

	nic->nic_op = &hijack_operations;
	if ( register_netdevice ( nic ) < 0 )
		goto err;

	hijack_set_drvdata ( hijack_dev, nic );
	return 1;

 err:
	if ( hijack->fd >= 0 )
		close ( hijack->fd );
	free_netdevice ( nic );
 err_alloc:
	return 0;
}

static void hijack_disable ( struct hijack_device *hijack_dev ) {
	struct nic *nic = hijack_get_drvdata ( hijack_dev );
	struct hijack *hijack = nic->priv_data;
	
	unregister_netdevice ( nic );
	close ( hijack->fd );
}

/*****************************************************************************
 *
 * "Hello world" protocol tester
 *
 */

struct hello_options {
	struct sockaddr_in server;
	const char *message;
};

static void hello_usage ( char **argv ) {
	fprintf ( stderr,
		  "Usage: %s [global options] hello [hello-specific options]\n"
		  "\n"
		  "hello-specific options:\n"
		  "  -h|--help              Print this help message\n"
		  "  -s|--server ip_addr    Server IP address\n"
		  "  -p|--port port         Port number\n"
		  "  -m|--message msg       Message to send\n",
		  argv[0] );
}

static int hello_parse_options ( int argc, char **argv,
				 struct hello_options *options ) {
	static struct option long_options[] = {
		{ "server", 1, NULL, 's' },
		{ "port", 1, NULL, 'p' },
		{ "message", 1, NULL, 'm' },
		{ "help", 0, NULL, 'h' },
		{ },
	};
	int c;
	char *endptr;

	/* Set default options */
	memset ( options, 0, sizeof ( *options ) );
	inet_aton ( "192.168.0.1", &options->server.sin_addr );
	options->server.sin_port = htons ( 80 );
	options->message = "Hello world!";

	/* Parse command-line options */
	while ( 1 ) {
		int option_index = 0;
		
		c = getopt_long ( argc, argv, "s:p:m:h", long_options,
				  &option_index );
		if ( c < 0 )
			break;

		switch ( c ) {
		case 's':
			if ( inet_aton ( optarg,
					 &options->server.sin_addr ) == 0 ) {
				fprintf ( stderr, "Invalid IP address %s\n",
					  optarg );
				return -1;
			}
			break;
		case 'p':
			options->server.sin_port =
				htons ( strtoul ( optarg, &endptr, 0 ) );
			if ( *endptr != '\0' ) {
				fprintf ( stderr, "Invalid port %s\n",
					  optarg );
				return -1;
			}
			break;
		case 'm':
			options->message = optarg;
			break;
		case 'h':
			hello_usage ( argv );
			return -1;
		case '?':
			/* Unrecognised option */
			return -1;
		default:
			fprintf ( stderr, "Unrecognised option '-%c'\n", c );
			return -1;
		}
	}

	/* Check there are no remaining arguments */
	if ( optind != argc ) {
		hello_usage ( argv );
		return -1;
	}
	
	return optind;
}

static void test_hello_callback ( char *data, size_t len ) {
	int i;
	char c;

	for ( i = 0 ; i < len ; i++ ) {
		c = data[i];
		if ( c == '\r' ) {
			/* Print nothing */
		} else if ( ( c == '\n' ) || ( c >= 32 ) || ( c <= 126 ) ) {
			putchar ( c );
		} else {
			putchar ( '.' );
		}
	}	
}

static int test_hello ( int argc, char **argv ) {
	struct hello_options options;
	struct hello_request hello;

	/* Parse hello-specific options */
	if ( hello_parse_options ( argc, argv, &options ) < 0 )
		return -1;

	/* Construct hello request */
	memset ( &hello, 0, sizeof ( hello ) );
	hello.tcp.sin = options.server;
	hello.message = options.message;
	hello.callback = test_hello_callback;
	fprintf ( stderr, "Saying \"%s\" to %s:%d\n", hello.message,
		  inet_ntoa ( hello.tcp.sin.sin_addr ),
		  ntohs ( hello.tcp.sin.sin_port ) );

	/* Issue hello request and run to completion */
	hello_connect ( &hello );
	while ( ! hello.complete ) {
		run_tcpip ();
	}

	return 0;
}

/*****************************************************************************
 *
 * iSCSI protocol tester
 *
 */

struct iscsi_options {
	struct sockaddr_in server;
	const char *initiator;
	const char *target;
};

static void iscsi_usage ( char **argv ) {
	fprintf ( stderr,
		  "Usage: %s [global options] iscsi [iscsi-specific options]\n"
		  "\n"
		  "iscsi-specific options:\n"
		  "  -h|--help              Print this help message\n"
		  "  -s|--server ip_addr    Server IP address\n"
		  "  -p|--port port         Port number\n"
		  "  -i|--initiator iqn     iSCSI initiator name\n"
		  "  -t|--target iqn        iSCSI target name\n",
		  argv[0] );
}

static int iscsi_parse_options ( int argc, char **argv,
				 struct iscsi_options *options ) {
	static struct option long_options[] = {
		{ "server", 1, NULL, 's' },
		{ "port", 1, NULL, 'p' },
		{ "initiator", 1, NULL, 'i' },
		{ "target", 1, NULL, 't' },
		{ "help", 0, NULL, 'h' },
		{ },
	};
	int c;
	char *endptr;

	/* Set default options */
	memset ( options, 0, sizeof ( *options ) );
	inet_aton ( "192.168.0.1", &options->server.sin_addr );
	options->server.sin_port = htons ( 3260 );
	options->initiator = "iqn.1900-01.localdomain.localhost:initiator";
	options->target = "iqn.1900-01.localdomain.localhost:target";

	/* Parse command-line options */
	while ( 1 ) {
		int option_index = 0;
		
		c = getopt_long ( argc, argv, "s:p:i:t:h", long_options,
				  &option_index );
		if ( c < 0 )
			break;

		switch ( c ) {
		case 's':
			if ( inet_aton ( optarg,
					 &options->server.sin_addr ) == 0 ) {
				fprintf ( stderr, "Invalid IP address %s\n",
					  optarg );
				return -1;
			}
			break;
		case 'p':
			options->server.sin_port =
				htons ( strtoul ( optarg, &endptr, 0 ) );
			if ( *endptr != '\0' ) {
				fprintf ( stderr, "Invalid port %s\n",
					  optarg );
				return -1;
			}
			break;
		case 'i':
			options->initiator = optarg;
			break;
		case 't':
			options->target = optarg;
			break;
		case 'h':
			iscsi_usage ( argv );
			return -1;
		case '?':
			/* Unrecognised option */
			return -1;
		default:
			fprintf ( stderr, "Unrecognised option '-%c'\n", c );
			return -1;
		}
	}

	/* Check there are no remaining arguments */
	if ( optind != argc ) {
		iscsi_usage ( argv );
		return -1;
	}
	
	return optind;
}

struct test_iscsi_buffer {
	unsigned char data[512];
};

static void test_iscsi_callback ( void *private, const void *data,
				  unsigned long offset, size_t len ) {
	struct test_iscsi_buffer *buffer = private;

	assert ( ( offset + len ) <= sizeof ( buffer->data ) );
	memcpy ( buffer->data + offset, data, len );
}

static int test_iscsi_block ( struct iscsi_session *iscsi,
			      unsigned int block ) {
	struct test_iscsi_buffer buffer;

	iscsi->block_size = 512;
	iscsi->block_start = block;
	iscsi->block_count = 1;
	iscsi->block_read_callback = test_iscsi_callback;
	iscsi->block_read_private = &buffer;
	memset ( buffer.data, 0x61, sizeof ( buffer.data ) );

	/* Start up iscsi session */
	iscsi_wakeup ( iscsi );
	while ( iscsi_busy ( iscsi ) ) {
		run_tcpip ();
	}

	/* Check for errors */
	if ( iscsi_error ( iscsi ) ) {
		fprintf ( stderr, "iSCSI error on block %d\n", block );
		return -1;
	}

	/* Dump out data */
	hex_dump ( buffer.data, sizeof ( buffer.data ) );

	return 0;
}

static int test_iscsi ( int argc, char **argv ) {
	struct iscsi_options options;
	struct iscsi_session iscsi;
	unsigned int block;

	/* Parse iscsi-specific options */
	if ( iscsi_parse_options ( argc, argv, &options ) < 0 )
		return -1;

	/* Construct iscsi session */
	memset ( &iscsi, 0, sizeof ( iscsi ) );
	iscsi.tcp.sin = options.server;
	iscsi.initiator = options.initiator;
	iscsi.target = options.target;

	/* Read some blocks */
	for ( block = 0 ; block < 4 ; block += 2 ) {
		if ( test_iscsi_block ( &iscsi, block ) < 0 )
			return -1;
	}

	return 0;
}

/*****************************************************************************
 *
 * Protocol tester
 *
 */

struct protocol_test {
	const char *name;
	int ( *exec ) ( int argc, char **argv );
};

static struct protocol_test tests[] = {
	{ "hello", test_hello },
	{ "iscsi", test_iscsi },
};

#define NUM_TESTS ( sizeof ( tests ) / sizeof ( tests[0] ) )

static void list_tests ( void ) {
	int i;

	for ( i = 0 ; i < NUM_TESTS ; i++ ) {
		printf ( "%s\n", tests[i].name );
	}
}

static struct protocol_test * get_test_from_name ( const char *name ) {
	int i;

	for ( i = 0 ; i < NUM_TESTS ; i++ ) {
		if ( strcmp ( name, tests[i].name ) == 0 )
			return &tests[i];
	}

	return NULL;
}

/*****************************************************************************
 *
 * Parse command-line options
 *
 */

struct tester_options {
	char interface[IF_NAMESIZE];
	struct in_addr in_addr;
	struct in_addr netmask;
	struct in_addr gateway;
};

static void usage ( char **argv ) {
	fprintf ( stderr,
		  "Usage: %s [global options] <test> [test-specific options]\n"
		  "\n"
		  "Global options:\n"
		  "  -h|--help              Print this help message\n"
		  "  -i|--interface intf    Use specified network interface\n"
		  "  -f|--from ip-addr      Use specified local IP address\n"
		  "  -n|--netmask mask      Use specified netmask\n"
		  "  -g|--gateway ip-addr   Use specified default gateway\n"
		  "  -l|--list              List available tests\n"
		  "\n"
		  "Use \"%s <test> -h\" to view test-specific options\n",
		  argv[0], argv[0] );
}

static int parse_options ( int argc, char **argv,
			   struct tester_options *options ) {
	static struct option long_options[] = {
		{ "interface", 1, NULL, 'i' },
		{ "from", 1, NULL, 'f' },
		{ "netmask", 1, NULL, 'n' },
		{ "gateway", 1, NULL, 'g' },
		{ "list", 0, NULL, 'l' },
		{ "help", 0, NULL, 'h' },
		{ },
	};
	int c;

	/* Set default options */
	memset ( options, 0, sizeof ( *options ) );
	strncpy ( options->interface, "eth0", sizeof ( options->interface ) );
	inet_aton ( "192.168.0.2", &options->in_addr );

	/* Parse command-line options */
	while ( 1 ) {
		int option_index = 0;
		
		c = getopt_long ( argc, argv, "+i:f:n:g:hl", long_options,
				  &option_index );
		if ( c < 0 )
			break;

		switch ( c ) {
		case 'i':
			strncpy ( options->interface, optarg,
				  sizeof ( options->interface ) );
			break;
		case 'f':
			if ( inet_aton ( optarg, &options->in_addr ) == 0 ) {
				fprintf ( stderr, "Invalid IP address %s\n",
					  optarg );
				return -1;
			}
			break;
		case 'n':
			if ( inet_aton ( optarg, &options->netmask ) == 0 ) {
				fprintf ( stderr, "Invalid IP address %s\n",
					  optarg );
				return -1;
			}
			break;
		case 'g':
			if ( inet_aton ( optarg, &options->gateway ) == 0 ) {
				fprintf ( stderr, "Invalid IP address %s\n",
					  optarg );
				return -1;
			}
			break;
		case 'l':
			list_tests ();
			return -1;
		case 'h':
			usage ( argv );
			return -1;
		case '?':
			/* Unrecognised option */
			return -1;
		default:
			fprintf ( stderr, "Unrecognised option '-%c'\n", c );
			return -1;
		}
	}

	/* Check there is a test specified */
	if ( optind == argc ) {
		usage ( argv );
		return -1;
	}
	
	return optind;
}

/*****************************************************************************
 *
 * Main program
 *
 */

int main ( int argc, char **argv ) {
	struct tester_options options;
	struct protocol_test *test;
	struct hijack_device hijack_dev;

	/* Parse command-line options */
	if ( parse_options ( argc, argv, &options ) < 0 )
		exit ( 1 );

	/* Identify test */
	test = get_test_from_name ( argv[optind] );
	if ( ! test ) {
		fprintf ( stderr, "Unrecognised test \"%s\"\n", argv[optind] );
		exit ( 1 );
	}
	optind++;

	/* Initialise the protocol stack */
	init_tcpip();
	set_ipaddr ( options.in_addr );
	set_netmask ( options.netmask );
	set_gateway ( options.gateway );

	/* Open the hijack device */
	hijack_dev.name = options.interface;
	if ( ! hijack_probe ( &hijack_dev ) )
		exit ( 1 );

	/* Run the test */
	if ( test->exec ( argc, argv ) < 0 )
		exit ( 1 );

	/* Close the hijack device */
	hijack_disable ( &hijack_dev );

	return 0;
}
