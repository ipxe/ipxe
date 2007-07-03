/**************************************************************************
Etherboot -  Network Bootstrap Program

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

#include <gpxe/heap.h>
#include <gpxe/init.h>
#include <gpxe/process.h>
#include <gpxe/device.h>
#include <gpxe/shell.h>
#include <gpxe/shell_banner.h>
#include <gpxe/shutdown.h>
#include <gpxe/hidemem.h>
#include <usr/autoboot.h>

/**
 * Start up Etherboot
 *
 * Call this function only once, before doing (almost) anything else.
 */
static void startup ( void ) {
	init_heap();
	init_processes();

	hide_etherboot();
	call_init_fns();
	probe_devices();
}

/**
 * Shut down Etherboot
 *
 * Call this function only once, before either exiting main() or
 * starting up a non-returnable image.
 */
void shutdown ( void ) {
	remove_devices();
	call_exit_fns();
	unhide_etherboot();
}

/**
 * Main entry point
 *
 * @ret rc		Return status code
 */
int main ( void ) {

	startup();

	/* Try autobooting if we're not going straight to the shell */
	if ( ! shell_banner() ) {
		autoboot();
	}
	
	/* Autobooting failed or the user wanted the shell */
	shell();

	shutdown();

	return 0;
}
