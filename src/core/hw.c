#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gpxe/refcnt.h>
#include <gpxe/xfer.h>
#include <gpxe/open.h>

/** @file
 *
 * "Hello World" data source
 *
 */

struct hw {
	struct refcnt refcnt;
	struct xfer_interface xfer;
};

static const char hw_msg[] = "Hello world!\n";

static void hw_finished ( struct hw *hw, int rc ) {
	xfer_nullify ( &hw->xfer );
	xfer_close ( &hw->xfer, rc );
}

static void hw_xfer_close ( struct xfer_interface *xfer, int rc ) {
	struct hw *hw = container_of ( xfer, struct hw, xfer );

	hw_finished ( hw, rc );
}

static void hw_xfer_request ( struct xfer_interface *xfer,
			      off_t start __unused, int whence __unused,
			      size_t len __unused ) {
	struct hw *hw = container_of ( xfer, struct hw, xfer );
	int rc;

	rc = xfer_deliver_raw ( xfer, hw_msg, sizeof ( hw_msg ) );
	hw_finished ( hw, rc );
}

static struct xfer_interface_operations hw_xfer_operations = {
	.close		= hw_xfer_close,
	.vredirect	= ignore_xfer_vredirect,
	.request	= hw_xfer_request,
	.seek		= ignore_xfer_seek,
	.deliver_iob	= xfer_deliver_as_raw,
	.deliver_raw	= ignore_xfer_deliver_raw,
};

static int hw_open ( struct xfer_interface *xfer, struct uri *uri __unused ) {
	struct hw *hw;

	/* Allocate and initialise structure */
	hw = malloc ( sizeof ( *hw ) );
	if ( ! hw )
		return -ENOMEM;
	memset ( hw, 0, sizeof ( *hw ) );
	xfer_init ( &hw->xfer, &hw_xfer_operations, &hw->refcnt );

	/* Attach parent interface, mortalise self, and return */
	xfer_plug_plug ( &hw->xfer, xfer );
	ref_put ( &hw->refcnt );
	return 0;
}

struct uri_opener hw_uri_opener __uri_opener = {
	.scheme = "hw",
	.open = hw_open,
};
