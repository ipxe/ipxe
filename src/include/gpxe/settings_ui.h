#ifndef _GPXE_SETTINGS_UI_H
#define _GPXE_SETTINGS_UI_H

/** @file
 *
 * Option configuration console
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

struct settings;

extern int settings_ui ( struct settings *settings ) __nonnull;

#endif /* _GPXE_SETTINGS_UI_H */
