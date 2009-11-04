#include <stdint.h>
#include <stdio.h>
#include <gpxe/settings.h>
#include <gpxe/dhcp.h>
#include <gpxe/init.h>
#include <gpxe/sanboot.h>
#include <usr/autoboot.h>

struct setting keep_san_setting __setting = {
	.name = "keep-san",
	.description = "Preserve SAN connection",
	.tag = DHCP_EB_KEEP_SAN,
	.type = &setting_type_int8,
};

int keep_san ( void ) {
	int keep_san;

	keep_san = fetch_intz_setting ( NULL, &keep_san_setting );
	if ( ! keep_san )
		return 0;

	printf ( "Preserving connection to SAN disk\n" );
	shutdown_exit_flags |= SHUTDOWN_KEEP_DEVICES;
	return 1;
}
