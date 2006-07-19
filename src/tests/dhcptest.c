#include <string.h>
#include <gpxe/dhcp.h>

int test_dhcp ( struct net_device *netdev ) {
	struct dhcp_session dhcp;

	memset ( &dhcp, 0, sizeof ( dhcp ) );
	dhcp.netdev = netdev;
	return async_wait ( start_dhcp ( &dhcp ) );
}
