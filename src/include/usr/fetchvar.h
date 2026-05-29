#ifndef _USR_FETCHVAR_H
#define _USR_FETCHVAR_H

/** @file
 *
 * Fetch URI to setting
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );
FILE_SECBOOT ( PERMITTED );

extern int fetchvar ( const char *uri_string, const char *setting_name );

#endif /* _USR_FETCHVAR_H */
