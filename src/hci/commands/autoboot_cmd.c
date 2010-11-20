#include <stdio.h>
#include <ipxe/command.h>
#include <ipxe/netdevice.h>
#include <usr/autoboot.h>

FILE_LICENCE ( GPL2_OR_LATER );

static int autoboot_exec ( int argc, char **argv ) {

	if ( argc != 1 ) {
		printf ( "Usage:\n"
			 "  %s\n"
			 "\n"
			 "Attempts to boot the system\n",
			 argv[0] );
		return 1;
	}

	autoboot();

	/* Can never return success by definition */
	return 1;
}

static int netboot_exec ( int argc, char **argv ) {
	const char *netdev_name;
	struct net_device *netdev;

	if ( argc != 2 ) {
		printf ( "Usage:\n"
			 "  %s <interface>\n"
			 "\n"
			 "Attempts to boot the system from <interface>\n",
			 argv[0] );
		return 1;
	}
	netdev_name = argv[1];

	netdev = find_netdev ( netdev_name );
	if ( ! netdev ) {
		printf ( "%s: no such interface\n", netdev_name );
		return 1;
	}

	netboot ( netdev );

	/* Can never return success by definition */
	return 1;
}

struct command autoboot_commands[] __command = {
	{
		.name = "autoboot",
		.exec = autoboot_exec,
	},
	{
		.name = "netboot",
		.exec = netboot_exec,
	},
};
