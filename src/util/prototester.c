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
#include <arpa/inet.h>
#include <getopt.h>
#include <assert.h>

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

static int netdev_poll ( int retrieve, void **data, size_t *len ) {
	int rc = static_nic.nic_op->poll ( &static_nic, retrieve );
	*data = static_nic.packet;
	*len = static_nic.packetlen;
	return rc;
}

static void netdev_transmit ( const void *data, size_t len ) {
	uint16_t type = ntohs ( *( ( uint16_t * ) ( data + 12 ) ) );
	static_nic.nic_op->transmit ( &static_nic, data, type,
				      len - ETH_HLEN,
				      data + ETH_HLEN );
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
 * uIP wrapper layer
 *
 */

#include "../proto/uip/uip.h"
#include "../proto/uip/uip_arp.h"

struct tcp_connection;

struct tcp_operations {
	void ( * aborted ) ( struct tcp_connection *conn );
	void ( * timedout ) ( struct tcp_connection *conn );
	void ( * closed ) ( struct tcp_connection *conn );
	void ( * connected ) ( struct tcp_connection *conn );
	void ( * acked ) ( struct tcp_connection *conn, size_t len );
	void ( * newdata ) ( struct tcp_connection *conn );
	void ( * senddata ) ( struct tcp_connection *conn );
};

struct tcp_connection {
	struct sockaddr_in sin;
	struct tcp_operations *tcp_op;
};

static int tcp_connect ( struct tcp_connection *conn ) {
	struct uip_conn *uip_conn;
	u16_t ipaddr[2];

	assert ( conn->sin.sin_addr.s_addr != 0 );
	assert ( conn->sin.sin_port != 0 );
	assert ( conn->tcp_op != NULL );
	assert ( sizeof ( uip_conn->appstate ) == sizeof ( conn ) );

	* ( ( uint32_t * ) ipaddr ) = conn->sin.sin_addr.s_addr;
	uip_conn = uip_connect ( ipaddr, conn->sin.sin_port );
	if ( ! uip_conn )
		return -1;

	*( ( void ** ) uip_conn->appstate ) = conn;
	return 0;
}

static void tcp_send ( struct tcp_connection *conn, const void *data,
		       size_t len ) {
	assert ( conn = *( ( void ** ) uip_conn->appstate ) );
	uip_send ( ( void * ) data, len );
}

static void tcp_close ( struct tcp_connection *conn ) {
	assert ( conn = *( ( void ** ) uip_conn->appstate ) );
	uip_close();
}

void uip_tcp_appcall ( void ) {
	struct tcp_connection *conn = *( ( void ** ) uip_conn->appstate );
	struct tcp_operations *op = conn->tcp_op;

	assert ( conn->tcp_op->closed != NULL );
	assert ( conn->tcp_op->connected != NULL );
	assert ( conn->tcp_op->acked != NULL );
	assert ( conn->tcp_op->newdata != NULL );
	assert ( conn->tcp_op->senddata != NULL );

	if ( uip_aborted() && op->aborted ) /* optional method */
		op->aborted ( conn );
	if ( uip_timedout() && op->timedout ) /* optional method */
		op->timedout ( conn );
	if ( uip_closed() && op->closed ) /* optional method */
		op->closed ( conn );
	if ( uip_connected() )
		op->connected ( conn );
	if ( uip_acked() )
		op->acked ( conn, uip_conn->len );
	if ( uip_newdata() )
		op->newdata ( conn );
	if ( uip_rexmit() || uip_newdata() || uip_acked() ||
	     uip_connected() || uip_poll() )
		op->senddata ( conn );
}

void uip_udp_appcall ( void ) {
}

static void init_tcpip ( void ) {
	uip_init();
	uip_arp_init();
}

#define UIP_HLEN ( 40 + UIP_LLH_LEN )

static void uip_transmit ( void ) {
	uip_arp_out();
	if ( uip_len > UIP_HLEN ) {
		memcpy ( uip_buf + UIP_HLEN, ( void * ) uip_appdata,
			 uip_len - UIP_HLEN );
	}
	netdev_transmit ( uip_buf, uip_len );
	uip_len = 0;
}

static void run_tcpip ( void ) {
	void *data;
	size_t len;
	uint16_t type;
	int i;
	
	if ( netdev_poll ( 1, &data, &len ) ) {
		/* We have data */
		memcpy ( uip_buf, data, len );
		uip_len = len;
		type = ntohs ( *( ( uint16_t * ) ( uip_buf + 12 ) ) );
		if ( type == ETHERTYPE_ARP ) {
			uip_arp_arpin();
		} else {
			uip_arp_ipin();
			uip_input();
		}
		if ( uip_len > 0 )
			uip_transmit();
	} else {
		for ( i = 0 ; i < UIP_CONNS ; i++ ) {
			uip_periodic ( i );
			if ( uip_len > 0 )
				uip_transmit();
		}
	}
}

/*****************************************************************************
 *
 * "Hello world" protocol tester
 *
 */

#include <stddef.h>
#define container_of(ptr, type, member) ({                      \
	const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
	(type *)( (char *)__mptr - offsetof(type,member) );})

enum hello_state {
	HELLO_SENDING_MESSAGE = 0,
	HELLO_SENDING_ENDL,
};

struct hello_request {
	struct tcp_connection tcp;
	const char *message;
	enum hello_state state;
	int remaining;
	void ( *callback ) ( struct hello_request *hello );
	int complete;
};

static inline struct hello_request *
tcp_to_hello ( struct tcp_connection *conn ) {
	return container_of ( conn, struct hello_request, tcp );
}

static void hello_aborted ( struct tcp_connection *conn ) {
	struct hello_request *hello = tcp_to_hello ( conn );

	printf ( "Connection aborted\n" );
	hello->complete = 1;
}

static void hello_timedout ( struct tcp_connection *conn ) {
	struct hello_request *hello = tcp_to_hello ( conn );

	printf ( "Connection timed out\n" );
	hello->complete = 1;
}

static void hello_closed ( struct tcp_connection *conn ) {
	struct hello_request *hello = tcp_to_hello ( conn );

	hello->complete = 1;
}

static void hello_connected ( struct tcp_connection *conn ) {
	struct hello_request *hello = tcp_to_hello ( conn );
}

static void hello_acked ( struct tcp_connection *conn, size_t len ) {
	struct hello_request *hello = tcp_to_hello ( conn );

	hello->message += len;
	hello->remaining -= len;
	if ( hello->remaining <= 0 ) {
		switch ( hello->state ) {
		case HELLO_SENDING_MESSAGE:
			hello->state = HELLO_SENDING_ENDL;
			hello->message = "\r\n";
			hello->remaining = 2;
			break;
		case HELLO_SENDING_ENDL:
			tcp_close ( conn );
			break;
		default:
			assert ( 0 );
		}
	}
}

static void hello_newdata ( struct tcp_connection *conn ) {
	struct hello_request *hello = tcp_to_hello ( conn );
}

static void hello_senddata ( struct tcp_connection *conn ) {
	struct hello_request *hello = tcp_to_hello ( conn );

	tcp_send ( conn, hello->message, hello->remaining );
}

static struct tcp_operations hello_tcp_operations = {
	.aborted	= hello_aborted,
	.timedout	= hello_timedout,
	.closed		= hello_closed,
	.connected	= hello_connected,
	.acked		= hello_acked,
	.newdata	= hello_newdata,
	.senddata	= hello_senddata,
};

static int hello_connect ( struct hello_request *hello ) {
	hello->tcp.tcp_op = &hello_tcp_operations;
	hello->remaining = strlen ( hello->message );
	return tcp_connect ( &hello->tcp );
}

struct hello_options {
	struct sockaddr_in server;
	const char *message;
};

static void hello_usage ( char **argv ) {
	fprintf ( stderr,
		  "Usage: %s [global options] hello [hello-specific options]\n"
		  "\n"
		  "hello-specific options:\n"
		  "  -h|--host              Host IP address\n"
		  "  -p|--port              Port number\n"
		  "  -m|--message           Message to send\n",
		  argv[0] );
}

static int hello_parse_options ( int argc, char **argv,
				 struct hello_options *options ) {
	static struct option long_options[] = {
		{ "host", 1, NULL, 'h' },
		{ "port", 1, NULL, 'p' },
		{ "message", 1, NULL, 'm' },
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
		
		c = getopt_long ( argc, argv, "h:p:", long_options,
				  &option_index );
		if ( c < 0 )
			break;

		switch ( c ) {
		case 'h':
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

static void test_hello_callback ( struct hello_request *hello ) {
	
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
 * Protocol tester
 *
 */

struct protocol_test {
	const char *name;
	int ( *exec ) ( int argc, char **argv );
};

static struct protocol_test tests[] = {
	{ "hello", test_hello },
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
};

static void usage ( char **argv ) {
	fprintf ( stderr,
		  "Usage: %s [global options] <test> [test-specific options]\n"
		  "\n"
		  "Global options:\n"
		  "  -h|--help              Print this help message\n"
		  "  -i|--interface intf    Use specified network interface\n"
		  "  -l|--list              List available tests\n"
		  "\n"
		  "Use \"%s <test> -h\" to view test-specific options\n",
		  argv[0], argv[0] );
}

static int parse_options ( int argc, char **argv,
			   struct tester_options *options ) {
	static struct option long_options[] = {
		{ "interface", 1, NULL, 'i' },
		{ "list", 0, NULL, 'l' },
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
		
		c = getopt_long ( argc, argv, "+i:hl", long_options,
				  &option_index );
		if ( c < 0 )
			break;

		switch ( c ) {
		case 'i':
			strncpy ( options->interface, optarg,
				  sizeof ( options->interface ) );
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
