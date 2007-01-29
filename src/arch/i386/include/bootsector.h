#ifndef _BOOTSECTOR_H
#define _BOOTSECTOR_H

/** @file
 *
 * x86 bootsector image format
 */

extern int call_bootsector ( unsigned int segment, unsigned int offset,
			     unsigned int drive );

#endif /* _BOOTSECTOR_H */
