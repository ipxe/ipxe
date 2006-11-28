#include <string.h>
#include <vsprintf.h>
#include <byteswap.h>
#include <gpxe/ip.h>
#include <gpxe/dhcp.h>
#include <gpxe/iscsi.h>
#include <gpxe/netdevice.h>

static int test_dhcp_aoe_boot ( struct net_device *netdev,
				char *aoename ) {
	unsigned int drivenum;
	
	drivenum = find_global_dhcp_num_option ( DHCP_EB_BIOS_DRIVE );
	return test_aoeboot ( netdev, aoename, drivenum );
}

static int test_dhcp_iscsi_boot ( struct net_device *netdev, char *iscsiname ) {
	char *initiator_iqn = "iqn.1900-01.localdomain.localhost:initiator";
	char username[32];
	char password[32];
	char *target_iqn;
	union {
		struct sockaddr_in sin;
		struct sockaddr_tcpip st;
	} target;

	memset ( &target, 0, sizeof ( target ) );
	target.sin.sin_family = AF_INET;
	target.sin.sin_port = htons ( ISCSI_PORT );
	target_iqn = strchr ( iscsiname, ':' );
	*target_iqn++ = '\0';
	if ( ! target_iqn ) {
		printf ( "Invalid iSCSI DHCP path\n" );
		return -EINVAL;
	}
	inet_aton ( iscsiname, &target.sin.sin_addr );

	dhcp_snprintf ( username, sizeof ( username ),
			find_global_dhcp_option ( DHCP_EB_USERNAME ) );
	dhcp_snprintf ( password, sizeof ( password ),
			find_global_dhcp_option ( DHCP_EB_PASSWORD ) );

	return test_iscsiboot ( initiator_iqn, &target.st, target_iqn,
				username, password, netdev );
}

static int test_dhcp_hello ( char *helloname ) {
	char *message;
	union {
		struct sockaddr_in sin;
		struct sockaddr_tcpip st;
	} target;

	memset ( &target, 0, sizeof ( target ) );
	target.sin.sin_family = AF_INET;
	target.sin.sin_port = htons ( 80 );
	message = strchr ( helloname, ':' );
	*message++ = '\0';
	if ( ! message ) {
		printf ( "Invalid hello path\n" );
		return -EINVAL;
	}
	inet_aton ( helloname, &target.sin.sin_addr );	

	test_hello ( &target.st, message );
	return 0;
}

static int test_dhcp_http ( struct net_device *netdev, char *url ) {
	union {
		struct sockaddr_in sin;
		struct sockaddr_tcpip st;
	} target;

	memset ( &target, 0, sizeof ( target ) );
	target.sin.sin_family = AF_INET;
	target.sin.sin_port = htons ( 80 );

	char *addr = url + 7; // http://
        char *file = strchr(addr, '/');
	*file = '\0'; // for printf and inet_aton to work
	printf("connecting to %s\n", addr);
	inet_aton ( addr, &target.sin.sin_addr );
	*file = '/';
	test_http ( netdev, &target.st, file );
	return 0;
}

static int test_dhcp_tftp ( struct net_device *netdev, char *tftpname ) {
	union {
		struct sockaddr_in sin;
		struct sockaddr_tcpip st;
	} target;

	memset ( &target, 0, sizeof ( target ) );
	target.sin.sin_family = AF_INET;
	target.sin.sin_port = htons ( 69 );
	find_global_dhcp_ipv4_option ( DHCP_EB_SIADDR,
				       &target.sin.sin_addr );

	return test_tftp ( netdev, &target.st, tftpname );
}

static int test_dhcp_boot ( struct net_device *netdev, char *filename ) {
	if ( strncmp ( filename, "aoe:", 4 ) == 0 ) {
		return test_dhcp_aoe_boot ( netdev, &filename[4] );
	} else if ( strncmp ( filename, "iscsi:", 6 ) == 0 ) {
		return test_dhcp_iscsi_boot ( netdev, &filename[6] );
	} else if ( strncmp ( filename, "hello:", 6 ) == 0 ) {
		return test_dhcp_hello ( &filename[6] );
	} else if ( strncmp ( filename, "http:", 5 ) == 0 ) {
		return test_dhcp_http ( netdev, filename );
	} else {
		return test_dhcp_tftp ( netdev, filename );
	}
}

int test_dhcp ( struct net_device *netdev ) {
	struct dhcp_session dhcp;
	struct in_addr address = { htonl ( 0 ) };
	struct in_addr netmask = { htonl ( 0 ) };
	struct in_addr gateway = { INADDR_NONE };
	char filename[256];
	int rc;

	/* Bring IP interface up with address 0.0.0.0 */
	if ( ( rc = add_ipv4_address ( netdev, address, netmask,
				       gateway ) ) != 0 )
		goto out_no_del_ipv4;

	/* Issue DHCP request */
	printf ( "DHCP (%s)...", netdev_name ( netdev ) );
	memset ( &dhcp, 0, sizeof ( dhcp ) );
	dhcp.netdev = netdev;
	if ( ( rc = async_wait ( start_dhcp ( &dhcp ) ) ) != 0 ) {
		printf ( "failed\n" );
		goto out_no_options;
	}
	printf ( "done\n" );

	/* Register options received via DHCP */
	register_dhcp_options ( dhcp.options );

	/* Retrieve IP address configuration */
	find_global_dhcp_ipv4_option ( DHCP_EB_YIADDR, &address );
	find_global_dhcp_ipv4_option ( DHCP_SUBNET_MASK, &netmask );
	find_global_dhcp_ipv4_option ( DHCP_ROUTERS, &gateway );

	printf ( "IP %s", inet_ntoa ( address ) );
	printf ( " netmask %s", inet_ntoa ( netmask ) );
	printf ( " gateway %s\n", inet_ntoa ( gateway ) );

	dhcp_snprintf ( filename, sizeof ( filename ),
			find_global_dhcp_option ( DHCP_BOOTFILE_NAME ) );
	if ( ! filename[0] ) {
		printf ( "No filename specified!\n" );
		goto out;
	}
	
	printf ( "Bootfile name %s\n", filename );

	/* Remove old IP address configuration */
	del_ipv4_address ( netdev );

	/* Set up new IP address configuration */
	if ( ( rc = add_ipv4_address ( netdev, address, netmask,
				       gateway ) ) != 0 )
		goto out_no_del_ipv4;

	/* Test boot */
	if ( ( rc = test_dhcp_boot ( netdev, filename ) ) != 0 ) {
		printf ( "Boot failed\n" );
		goto out;
	}
	
 out:
	/* Unregister and free DHCP options */
	unregister_dhcp_options ( dhcp.options );
	free_dhcp_options ( dhcp.options );
 out_no_options:
	/* Take down IP interface */
	del_ipv4_address ( netdev );
 out_no_del_ipv4:
	return rc;
}
