#ifndef _GPXE_REFCNT_H
#define _GPXE_REFCNT_H

/** @file
 *
 * Reference counting
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/**
 * A reference counter
 *
 * This data structure is designed to be embedded within a
 * reference-counted object.
 *
 * Reference-counted objects are freed when their reference count
 * drops below zero.  This means that a freshly allocated-and-zeroed
 * reference-counted object will be freed on the first call to
 * ref_put().
 */
struct refcnt {
	/** Current reference count
	 *
	 * When this count is decremented below zero, the free()
	 * method will be called.
	 */
	int refcnt;
	/** Free containing object
	 *
	 * This method is called when the reference count is
	 * decremented below zero.
	 *
	 * If this method is left NULL, the standard library free()
	 * function will be called.  The upshot of this is that you
	 * may omit the free() method if the @c refcnt object is the
	 * first element of your reference-counted struct.
	 */
	void ( * free ) ( struct refcnt *refcnt );
};

extern struct refcnt * ref_get ( struct refcnt *refcnt );
extern void ref_put ( struct refcnt *refcnt );

#endif /* _GPXE_REFCNT_H */
