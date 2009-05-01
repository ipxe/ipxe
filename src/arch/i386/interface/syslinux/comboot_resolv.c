#include <errno.h>
#include <comboot.h>
#include <gpxe/in.h>
#include <gpxe/list.h>
#include <gpxe/process.h>
#include <gpxe/resolv.h>

FILE_LICENCE ( GPL2_OR_LATER );

static int comboot_resolv_rc;
static struct in_addr comboot_resolv_addr;

static void comboot_resolv_done ( struct resolv_interface *resolv,
				  struct sockaddr *sa, int rc ) {
	struct sockaddr_in *sin;

	resolv_unplug ( resolv );

	if ( rc != 0 ) {
		comboot_resolv_rc = rc;
		return;
	}

	if ( sa->sa_family != AF_INET ) {
		comboot_resolv_rc = -EAFNOSUPPORT;
		return;
	}

	sin = ( ( struct sockaddr_in * ) sa );
	comboot_resolv_addr = sin->sin_addr;

	comboot_resolv_rc = 0;
}

static struct resolv_interface_operations comboot_resolv_ops = {
	.done = comboot_resolv_done,
};

static struct resolv_interface comboot_resolver = {
	.intf = {
		.dest = &null_resolv.intf,
		.refcnt = NULL,
	},
	.op = &comboot_resolv_ops,
};

int comboot_resolv ( const char *name, struct in_addr *address ) {
	int rc;

	comboot_resolv_rc = -EINPROGRESS;

	if ( ( rc = resolv ( &comboot_resolver, name, NULL ) ) != 0 )
		return rc;

	while ( comboot_resolv_rc == -EINPROGRESS )
		step();

	*address = comboot_resolv_addr;
	return comboot_resolv_rc;
}
