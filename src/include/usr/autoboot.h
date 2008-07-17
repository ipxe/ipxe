#ifndef _USR_AUTOBOOT_H
#define _USR_AUTOBOOT_H

/** @file
 *
 * Automatic booting
 *
 */

extern int shutdown_exit_flags;

extern void autoboot ( void );
extern int boot_root_path ( const char *root_path );

#endif /* _USR_AUTOBOOT_H */
