/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/** @file
 *
 * 64-bit division
 *
 * The x86 CPU (386 upwards) has a divl instruction which will perform
 * unsigned division of a 64-bit dividend by a 32-bit divisor.  If the
 * resulting quotient does not fit in 32 bits, then a CPU exception
 * will occur.
 *
 * Unsigned integer division is expressed as solving 
 *
 *   x = d.q + r			0 <= q, 0 <= r < d
 *
 * given the dividend (x) and divisor (d), to find the quotient (q)
 * and remainder (r).
 *
 * The x86 divl instruction will solve
 *
 *   x = d.q + r			0 <= q, 0 <= r < d
 *
 * given x in the range 0 <= x < 2^64 and 1 <= d < 2^32, and causing a
 * hardware exception if the resulting q >= 2^32.
 *
 * We can therefore use divl only if we can prove that the conditions
 *
 *   0 <= x < 2^64
 *   1 <= d < 2^32
 *        q < 2^32
 *
 * are satisfied.
 *
 *
 * Case 1 : 1 <= d < 2^32
 * ======================
 *
 * We express x as
 *
 *   x = xh.2^32 + xl			0 <= xh < 2^32, 0 <= xl < 2^32	(1)
 *
 * i.e. split x into low and high dwords.  We then solve
 *
 *   xh = d.qh + r'			0 <= qh, 0 <= r' < d		(2)
 *
 * which we can do using a divl instruction since
 *
 *   0 <= xh < 2^64			since 0 <= xh < 2^32 from (1)	(3)
 *
 * and
 *
 *   1 <= d < 2^32			by definition of this Case	(4)
 *
 * and
 *
 *   d.qh = xh - r'			from (2)
 *   d.qh <= xh				since r' >= 0 from (2)
 *   qh <= xh				since d >= 1 from (2)
 *   qh < 2^32				since xh < 2^32 from (1)	(5)
 *
 * Having obtained qh and r', we then solve
 *
 *   ( r'.2^32 + xl ) = d.ql + r	0 <= ql, 0 <= r < d		(6)
 *
 * which we can do using another divl instruction since
 *
 *   xl <= 2^32 - 1			from (1), so
 *   r'.2^32 + xl <= ( r' + 1 ).2^32 - 1
 *   r'.2^32 + xl <= d.2^32 - 1		since r' < d from (2)
 *   r'.2^32 + xl < d.2^32						(7)
 *   r'.2^32 + xl < 2^64		since d < 2^32 from (4)		(8)
 *
 * and
 *
 *   1 <= d < 2^32			by definition of this Case	(9)
 *
 * and
 *
 *   d.ql = ( r'.2^32 + xl ) - r	from (6)
 *   d.ql <= r'.2^32 + xl		since r >= 0 from (6)
 *   d.ql < d.2^32			from (7)
 *   ql < 2^32				since d >= 1 from (2)		(10)
 *
 * This then gives us
 *
 *   x = xh.2^32 + xl			from (1)
 *   x = ( d.qh + r' ).2^32 + xl	from (2)
 *   x = d.qh.2^32 + ( r'.2^32 + xl )
 *   x = d.qh.2^32 + d.ql + r		from (3)
 *   x = d.( qh.2^32 + ql ) + r						(11)
 *
 * Letting
 *
 *   q = qh.2^32 + ql							(12)
 *
 * gives
 *
 *   x = d.q + r			from (11) and (12)
 *
 * which is the solution.
 *
 *
 * This therefore gives us a two-step algorithm:
 *
 *   xh = d.qh + r'			0 <= qh, 0 <= r' < d		(2)
 *   ( r'.2^32 + xl ) = d.ql + r	0 <= ql, 0 <= r < d		(6)
 *
 * which translates to
 *
 *   %edx:%eax = 0:xh
 *   divl d
 *   qh = %eax
 *   r' = %edx
 *
 *   %edx:%eax = r':xl
 *   divl d
 *   ql = %eax
 *   r = %edx
 *
 * Note that if
 *
 *   xh < d
 *
 * (which is a fast dword comparison) then the first divl instruction
 * can be omitted, since the answer will be
 *
 *   qh = 0
 *   r = xh
 *
 *
 * Case 2 : 2^32 <= d < 2^64
 * =========================
 *
 * We first express d as
 *
 *   d = dh.2^k + dl			2^31 <= dh < 2^32,
 *					0 <= dl < 2^k, 1 <= k <= 32	(1)
 *
 * i.e. find the highest bit set in d, subtract 32, and split d into
 * dh and dl at that point.
 *
 * We then express x as
 *
 *   x = xh.2^k + xl			0 <= xl < 2^k			(2)
 *
 * giving
 *
 *   xh.2^k = x - xl			from (2)
 *   xh.2^k <= x			since xl >= 0 from (1)
 *   xh.2^k < 2^64			since xh < 2^64 from (1)
 *   xh < 2^(64-k)							(3)
 *
 * We then solve the division
 *
 *   xh = dh.q' + r'	            		0 <= r' < dh		(4)
 *
 * which we can do using a divl instruction since
 *
 *   0 <= xh < 2^64			since x < 2^64 and xh < x
 *
 * and
 *
 *   1 <= dh < 2^32			from (1)
 *
 * and
 *
 *   dh.q' = xh - r'			from (4)
 *   dh.q' <= xh			since r' >= 0 from (4)
 *   dh.q' < 2^(64-k)			from (3)			(5)
 *   q'.2^31 <= dh.q'			since dh >= 2^31 from (1)	(6)
 *   q'.2^31 < 2^(64-k)			from (5) and (6)
 *   q' < 2^(33-k)
 *   q' < 2^32				since k >= 1 from (1)		(7)
 *
 * This gives us
 *
 *   xh.2^k = dh.q'.2^k + r'.2^k	from (4)
 *   x - xl = ( d - dl ).q' + r'.2^k	from (1) and (2)
 *   x = d.q' + ( r'.2^k + xl ) - dl.q'					(8)
 *
 * Now
 *
 *  r'.2^k + xl < r'.2^k + 2^k		since xl < 2^k from (2)
 *  r'.2^k + xl < ( r' + 1 ).2^k
 *  r'.2^k + xl < dh.2^k		since r' < dh from (4)
 *  r'.2^k + xl < ( d - dl )		from (1)			(9)
 *
 *
 * (missing)
 *
 *
 * This gives us two cases to consider:
 *
 * case (a):
 *
 *   dl.q' <= ( r'.2^k + xl )						(15a)
 *
 * in which case
 *
 *   x = d.q' + ( r'.2^k + xl - dl.q' )
 *
 * is a direct solution to the division, since
 *
 *   r'.2^k + xl < d			from (9)
 *   ( r'.2^k + xl - dl.q' ) < d	since dl >= 0 and q' >= 0
 *
 * and
 *
 *   0 <= ( r'.2^k + xl - dl.q' )	from (15a)
 *
 * case (b):
 *
 *   dl.q' > ( r'.2^k + xl )						(15b)
 *   
 * Express
 *
 *  x = d.(q'-1) + ( r'.2^k + xl ) + ( d - dl.q' )
 *  
 *   
 * (missing)
 *
 *
 * special case: k = 32 cannot be handled with shifts
 *
 * (missing)
 * 
 */

#include <stdint.h>
#include <assert.h>

typedef uint64_t UDItype;

struct uint64 {
	uint32_t l;
	uint32_t h;
};

static inline void udivmod64_lo ( const struct uint64 *x,
				  const struct uint64 *d,
				  struct uint64 *q,
				  struct uint64 *r ) {
	uint32_t r_dash;

	q->h = 0;
	r->h = 0;
	r_dash = x->h;

	if ( x->h >= d->l ) {
		__asm__ ( "divl %2"
			  : "=&a" ( q->h ), "=&d" ( r_dash )
			  : "g" ( d->l ), "0" ( x->h ), "1" ( 0 ) );
	}

	__asm__ ( "divl %2"
		  : "=&a" ( q->l ), "=&d" ( r->l )
		  : "g" ( d->l ), "0" ( x->l ), "1" ( r_dash ) );
}

static void udivmod64 ( const struct uint64 *x,
			const struct uint64 *d,
			struct uint64 *q,
			struct uint64 *r ) {

	if ( d->h == 0 ) {
		udivmod64_lo ( x, d, q, r );
	} else {
		assert ( 0 );
		while ( 1 ) {};
	}	
}

/**
 * 64-bit division with remainder
 *
 * @v x			Dividend
 * @v d			Divisor
 * @ret r		Remainder
 * @ret q		Quotient
 */
UDItype __udivmoddi4 ( UDItype x, UDItype d, UDItype *r ) {
	UDItype q;
	UDItype *_x = &x;
	UDItype *_d = &d;
	UDItype *_q = &q;
	UDItype *_r = r;

	udivmod64 ( ( struct uint64 * ) _x, ( struct uint64 * ) _d,
		    ( struct uint64 * ) _q, ( struct uint64 * ) _r );

	assert ( ( x == ( ( d * q ) + (*r) ) ) );

	return q;
}

/**
 * 64-bit division
 *
 * @v x			Dividend
 * @v d			Divisor
 * @ret q		Quotient
 */
UDItype __udivdi3 ( UDItype x, UDItype d ) {
	UDItype r;
	return __udivmoddi4 ( x, d, &r );
}

/**
 * 64-bit modulus
 *
 * @v x			Dividend
 * @v d			Divisor
 * @ret q		Quotient
 */
UDItype __umoddi3 ( UDItype x, UDItype d ) {
	UDItype r;
	__udivmoddi4 ( x, d, &r );
	return r;
}
