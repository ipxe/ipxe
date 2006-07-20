#include <string.h>
#include <vsprintf.h>
#include <byteswap.h>
#include <gpxe/ip.h>
#include <gpxe/dhcp.h>

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
	printf ( "DHCP..." );
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

	/* Proof of concept: check for "aoe:" prefix and if found, do
	 * test AoE boot with AoE options.
	 */
	if ( strncmp ( filename, "aoe:", 4 ) == 0 ) {
		unsigned int drivenum;
		
		drivenum = find_global_dhcp_num_option ( DHCP_EB_BIOS_DRIVE );
		test_aoeboot ( netdev, &filename[4], drivenum );
	} else {
		printf ( "Don't know how to boot %s\n", filename );
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
