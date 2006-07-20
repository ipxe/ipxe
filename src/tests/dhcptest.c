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
	int rc;

	/* Bring IP interface up with address 0.0.0.0 */
	if ( ( rc = add_ipv4_address ( netdev, address, netmask,
				       gateway ) ) != 0 )
		goto out_no_del_ipv4;

	/* Issue DHCP request */
	memset ( &dhcp, 0, sizeof ( dhcp ) );
	dhcp.netdev = netdev;
	if ( ( rc = async_wait ( start_dhcp ( &dhcp ) ) ) != 0 )
		goto out_no_options;

	/* Register options received via DHCP */
	register_dhcp_options ( dhcp.options );

	/* Retrieve IP address configuration */
	find_global_dhcp_ipv4_option ( DHCP_EB_YIADDR, &address );
	find_global_dhcp_ipv4_option ( DHCP_SUBNET_MASK, &netmask );
	find_global_dhcp_ipv4_option ( DHCP_ROUTERS, &gateway );

	printf ( "IP %s", inet_ntoa ( address ) );
	printf ( " netmask %s", inet_ntoa ( netmask ) );
	printf ( " gateway %s\n", inet_ntoa ( gateway ) );

	printf ( "Lease time is %ld seconds\n",
		 find_global_dhcp_num_option ( DHCP_LEASE_TIME ) );

	/* Remove old IP address configuration */
	del_ipv4_address ( netdev );

	/* Set up new IP address configuration */
	if ( ( rc = add_ipv4_address ( netdev, address, netmask,
				       gateway ) ) != 0 )
		goto out_no_del_ipv4;

	/* Unregister and free DHCP options */
	unregister_dhcp_options ( dhcp.options );
	free_dhcp_options ( dhcp.options );
 out_no_options:
	/* Take down IP interface */
	del_ipv4_address ( netdev );
 out_no_del_ipv4:
	return rc;
}
