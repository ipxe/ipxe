/*
 *  Copyright (C) 1999-2002 Hewlett-Packard Co.
 *	Contributed by Stephane Eranian <eranian@hpl.hp.com>
 *
 * This file is part of the ELILO, the EFI Linux boot loader.
 *
 *  ELILO is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  ELILO is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with ELILO; see the file COPYING.  If not, write to the Free
 *  Software Foundation, 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * Please check out the elilo.txt for complete documentation on how
 * to use this program.
 *
 * This code is derived from the Linux/ia64 source code.
 */

/*
 *
 * Optimized version of the standard memset() function
 *
 * Return: none
 *
 * Inputs:
 *	in0:	address of buffer
 * 	in1:	byte value to use for storing
 *	in2:	length of the buffer
 *
 */

// arguments
//
#define buf		r32
#define val		r33
#define len		r34

//
// local registers
//
#define saved_pfs	r14
#define cnt		r18
#define buf2		r19
#define saved_lc	r20
#define tmp		r21
	.text
	.global memset
	.proc memset
memset:
	.prologue
	.save ar.pfs, saved_pfs
 	alloc saved_pfs=ar.pfs,3,0,0,0	// cnt is sink here
	cmp.eq p8,p0=r0,len	// check for zero length
	.save ar.lc, saved_lc
	mov saved_lc=ar.lc	// preserve ar.lc (slow)
	;;

	.body

	adds tmp=-1,len		// br.ctop is repeat/until
	tbit.nz p6,p0=buf,0	// odd alignment
(p8)	br.ret.spnt.few rp

	cmp.lt p7,p0=16,len	// if len > 16 then long memset
	mux1 val=val,@brcst	// prepare value
(p7)	br.cond.dptk.few long_memset
	;;
	mov ar.lc=tmp		// initialize lc for small count
	;;			// avoid RAW and WAW on ar.lc
1:				// worst case 15 cyles, avg 8 cycles
	st1 [buf]=val,1
	br.cloop.dptk.few 1b
	;;				// avoid RAW on ar.lc
	mov ar.lc=saved_lc
	mov ar.pfs=saved_pfs
	br.ret.sptk.few rp	// end of short memset

	// at this point we know we have more than 16 bytes to copy
	// so we focus on alignment
long_memset:
(p6)	st1 [buf]=val,1		// 1-byte aligned
(p6)	adds len=-1,len;;	// sync because buf is modified
	tbit.nz p6,p0=buf,1
	;;
(p6)	st2 [buf]=val,2		// 2-byte aligned
(p6)	adds len=-2,len;;
	tbit.nz p6,p0=buf,2
	;;
(p6)	st4 [buf]=val,4		// 4-byte aligned
(p6)	adds len=-4,len;;
	tbit.nz p6,p0=buf,3
	;;
(p6)	st8 [buf]=val,8		// 8-byte aligned
(p6)	adds len=-8,len;;
	shr.u cnt=len,4		// number of 128-bit (2x64bit) words
	;;
	cmp.eq p6,p0=r0,cnt
	adds tmp=-1,cnt
(p6)	br.cond.dpnt.few .dotail // we have less than 16 bytes left
	;;
	adds buf2=8,buf		// setup second base pointer
	mov ar.lc=tmp
	;;
2:				// 16bytes/iteration
	st8 [buf]=val,16
	st8 [buf2]=val,16
	br.cloop.dptk.few 2b
	;;
.dotail:			// tail correction based on len only
	tbit.nz p6,p0=len,3
	;;
(p6)	st8 [buf]=val,8		// at least 8 bytes
	tbit.nz p6,p0=len,2
	;;
(p6)	st4 [buf]=val,4		// at least 4 bytes
	tbit.nz p6,p0=len,1
	;;
(p6)	st2 [buf]=val,2		// at least 2 bytes
	tbit.nz p6,p0=len,0
	mov ar.lc=saved_lc
	;;
(p6)	st1 [buf]=val		// only 1 byte left
	br.ret.dptk.few rp
	.endp memset
