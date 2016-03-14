#ifndef _EDID_H
#define _EDID_H

/** @file
 *
 * EDID
 *
 */

FILE_LICENCE ( MIT );

/**
 * Get preferred resolution from EDID
 *
 * @v   edid		EDID string
 * @v   x		X resolution of preferred resolution
 * @v   y		Y resolution of preferred resolution
 * @ret rc		Return status code
 */
extern int edid_get_preferred_resolution ( const unsigned char *edid,
					   unsigned int *x, unsigned int *y );

#endif /* _EDID_H */
