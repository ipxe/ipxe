#include <string.h>
#include <stdlib.h>
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

enum {
	RP_LITERAL = 0,
	RP_SERVERNAME,
	RP_PROTOCOL,
	RP_PORT,
	RP_LUN,
	RP_TARGETNAME,
	NUM_RP_COMPONENTS
};

static int iscsi_split_root_path ( char *root_path,
				   char * components[NUM_RP_COMPONENTS] ) {
	int component = 0;
	
	while ( 1 ) {
		components[component++] = root_path;
		if ( component == NUM_RP_COMPONENTS ) {
			return 0;
		}
		for ( ; *root_path != ':' ; root_path++ ) {
			if ( ! *root_path )
				return -EINVAL;
		}
		*(root_path++) = '\0';
	}
}

static int test_dhcp_iscsi_boot ( struct net_device *netdev ) {
	char root_path[256];
	char *rp_components[NUM_RP_COMPONENTS];
	char initiator_iqn_buf[64];
	char *initiator_iqn = initiator_iqn_buf;
	char username[32];
	char password[32];
	union {
		struct sockaddr_in sin;
		struct sockaddr_tcpip st;
	} target;
	unsigned int drivenum;
	unsigned int lun;
	struct dhcp_option *option;
	int rc;

	memset ( &target, 0, sizeof ( target ) );
	target.sin.sin_family = AF_INET;

	dhcp_snprintf ( root_path, sizeof ( root_path ),
			find_global_dhcp_option ( DHCP_ROOT_PATH ) );

	printf ( "Root path \"%s\"\n", root_path );

	if ( ( rc = iscsi_split_root_path ( root_path, rp_components ) ) != 0 )
		goto bad_root_path;

	if ( strcmp ( rp_components[RP_LITERAL], "iscsi" ) != 0 )
		goto bad_root_path;

	if ( inet_aton ( rp_components[RP_SERVERNAME],
			 &target.sin.sin_addr ) == 0 )
		goto bad_root_path;

	target.sin.sin_port = strtoul ( rp_components[RP_PORT], NULL, 0 );
	if ( ! target.st.st_port )
		target.st.st_port = htons ( ISCSI_PORT );

	lun = strtoul ( rp_components[RP_LUN], NULL, 0 );
	
	dhcp_snprintf ( initiator_iqn_buf, sizeof ( initiator_iqn_buf ),
			find_global_dhcp_option ( DHCP_ISCSI_INITIATOR_IQN ) );
	if ( ! initiator_iqn_buf[0] )
		initiator_iqn = "iqn.1900-01.localdomain.localhost:initiator";
	dhcp_snprintf ( username, sizeof ( username ),
			find_global_dhcp_option ( DHCP_EB_USERNAME ) );
	dhcp_snprintf ( password, sizeof ( password ),
			find_global_dhcp_option ( DHCP_EB_PASSWORD ) );

	drivenum = find_global_dhcp_num_option ( DHCP_EB_BIOS_DRIVE );

	return test_iscsiboot ( initiator_iqn, &target.st,
				rp_components[RP_TARGETNAME], lun,
				username, password, netdev, drivenum );

 bad_root_path:
	printf ( "Invalid iSCSI root path\n" );
	return -EINVAL;
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

static int test_dhcp_ftp ( struct net_device *netdev, char *ftpname ) {
	union {
		struct sockaddr_in sin;
		struct sockaddr_tcpip st;
	} target;
	char *filename;

	filename = strchr ( ftpname, ':' );
	if ( ! filename ) {
		printf ( "Invalid FTP path \"%s\"\n", ftpname );
		return -EINVAL;
	}
	*filename++ = '\0';

	memset ( &target, 0, sizeof ( target ) );
	target.sin.sin_family = AF_INET;
	target.sin.sin_port = htons ( 21 );
	inet_aton ( ftpname, &target.sin.sin_addr );

	test_ftp ( &target, filename );
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
	/*
	if ( strncmp ( filename, "aoe:", 4 ) == 0 ) {
		return test_dhcp_aoe_boot ( netdev, &filename[4] );
	} 
	*/
	//	if ( strncmp ( filename, "iscsi:", 6 ) == 0 ) {
	if ( ! filename[0] ) {
		return test_dhcp_iscsi_boot ( netdev );
	}
	/*
	if ( strncmp ( filename, "ftp:", 4 ) == 0 ) {
		return test_dhcp_ftp ( netdev, &filename[4] );
	}
	*/
	/*
	if ( strncmp ( filename, "hello:", 6 ) == 0 ) {
		return test_dhcp_hello ( &filename[6] );
	}
	if ( strncmp ( filename, "http:", 5 ) == 0 ) {
		return test_dhcp_http ( netdev, filename );
	}
	*/
	return test_dhcp_tftp ( netdev, filename );

	return -EPROTONOSUPPORT;
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
	printf ( "DHCP (%s)...", netdev->name );
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
	
	if ( filename[0] )
		printf ( "Bootfile name \"%s\"\n", filename );

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
