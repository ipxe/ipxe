/**************************************************************************
gPXE -  Network Bootstrap Program

Literature dealing with the network protocols:
	ARP - RFC826
	RARP - RFC903
	UDP - RFC768
	BOOTP - RFC951, RFC2132 (vendor extensions)
	DHCP - RFC2131, RFC2132 (options)
	TFTP - RFC1350, RFC2347 (options), RFC2348 (blocksize), RFC2349 (tsize)
	RPC - RFC1831, RFC1832 (XDR), RFC1833 (rpcbind/portmapper)
	NFS - RFC1094, RFC1813 (v3, useful for clarifications, not implemented)
	IGMP - RFC1112

**************************************************************************/

#include <gpxe/init.h>
#include <gpxe/shell.h>
#include <gpxe/shell_banner.h>
#include <usr/autoboot.h>

/**
 * Main entry point
 *
 * @ret rc		Return status code
 */
__cdecl int main ( void ) {

	initialise();
	startup();

	if ( shell_banner() )
		shell();
	else
		autoboot();
	
	shutdown();

	return 0;
}
