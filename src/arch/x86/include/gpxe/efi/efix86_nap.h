#ifndef _GPXE_EFIX86_NAP_H
#define _GPXE_EFIX86_NAP_H

/** @file
 *
 * EFI CPU sleeping
 *
 */

#ifdef NAP_EFIX86
#define NAP_PREFIX_efix86
#else
#define NAP_PREFIX_efix86 __efix86_
#endif

#endif /* _GPXE_EFIX86_NAP_H */
