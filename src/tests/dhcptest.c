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

	/* Retrieve IP address configuration */
	find_dhcp_ipv4_option ( dhcp.options, DHCP_EB_YIADDR, &address );
	find_dhcp_ipv4_option ( dhcp.options, DHCP_SUBNET_MASK, &netmask );
	find_dhcp_ipv4_option ( dhcp.options, DHCP_ROUTERS, &gateway );

	/* Remove old IP address configuration */
	del_ipv4_address ( netdev );

	/* Set up new IP address configuration */
	if ( ( rc = add_ipv4_address ( netdev, address, netmask,
				       gateway ) ) != 0 )
		goto out_no_del_ipv4;

	printf ( "IP %s", inet_ntoa ( address ) );
	printf ( " netmask %s", inet_ntoa ( netmask ) );
	printf ( " gateway %s\n", inet_ntoa ( gateway ) );

	/* Free DHCP options */
	free_dhcp_options ( dhcp.options );
 out_no_options:
	/* Take down IP interface */
	del_ipv4_address ( netdev );
 out_no_del_ipv4:
	return rc;
}
