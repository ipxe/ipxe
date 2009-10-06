#ifndef _USR_AUTOBOOT_H
#define _USR_AUTOBOOT_H

/** @file
 *
 * Automatic booting
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/in.h>
struct net_device;

extern int shutdown_exit_flags;

extern void autoboot ( void );
extern int boot_next_server_and_filename ( struct in_addr next_server,
					   const char *filename );
extern int boot_root_path ( const char *root_path );

extern int pxe_menu_boot ( struct net_device *netdev )
	__attribute__ (( weak ));

#endif /* _USR_AUTOBOOT_H */
