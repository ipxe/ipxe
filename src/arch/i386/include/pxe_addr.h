/*
 * Architecture-specific portion of pxe.h for Etherboot
 *
 * This file has to define the types SEGOFF16_t, SEGDESC_t and
 * SEGSEL_t for use in other PXE structures.  See pxe.h for details.
 */

#ifndef PXE_ADDR_H
#define PXE_ADDR_H

#define IS_NULL_SEGOFF16(x) ( ( (x).segment == 0 ) && ( (x).offset == 0 ) )
#define SEGOFF16_TO_PTR(x) ( VIRTUAL( (x).segment, (x).offset ) )
#define PTR_TO_SEGOFF16(ptr,segoff16) \
	(segoff16).segment = SEGMENT(ptr); \
	(segoff16).offset  = OFFSET(ptr);

#endif /* PXE_ADDR_H */
