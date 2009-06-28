#include <gpxe/netdevice.h>
#include <gpxe/command.h>
#include <hci/ifmgmt_cmd.h>
#include <pxe_call.h>

FILE_LICENCE ( GPL2_OR_LATER );

static int startpxe_payload ( struct net_device *netdev ) {
	if ( netdev->state & NETDEV_OPEN )
		pxe_activate ( netdev );
	return 0;
}

static int startpxe_exec ( int argc, char **argv ) {
	return ifcommon_exec ( argc, argv, startpxe_payload,
			       "Activate PXE on" );
}

static int stoppxe_exec ( int argc __unused, char **argv __unused ) {
	pxe_deactivate();
	return 0;
}

struct command pxe_commands[] __command = {
	{
		.name = "startpxe",
		.exec = startpxe_exec,
	},
	{
		.name = "stoppxe",
		.exec = stoppxe_exec,
	},
};
