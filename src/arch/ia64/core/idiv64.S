/*
 * Copyright (C) 1999-2000, 2002 Hewlett-Packard Co
 * Copyright (C) 1999-2000 David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * 64-bit integer division.
 *
 * This code is based on the application note entitled "Divide, Square Root
 * and Remainder Algorithms for the IA-64 Architecture".  This document
 * is available as Intel document number 248725-002 or via the web at
 * http://developer.intel.com/software/opensource/numerics/
 *
 * For more details on the theory behind these algorithms, see "IA-64
 * and Elementary Functions" by Peter Markstein; HP Professional Books
 * (http://www.hp.com/go/retailbooks/)
 */

#ifdef MODULO
# define OP	mod
#else
# define OP	div
#endif

#ifdef UNSIGNED
# define SGN	u
# define INT_TO_FP(a,b)	fcvt.xuf.s1 a=b
# define FP_TO_INT(a,b)	fcvt.fxu.trunc.s1 a=b
#else
# define SGN
# define INT_TO_FP(a,b)	fcvt.xf a=b
# define FP_TO_INT(a,b)	fcvt.fx.trunc.s1 a=b
#endif

#define PASTE1(a,b)	a##b
#define PASTE(a,b)	PASTE1(a,b)
#define NAME		PASTE(PASTE(__,SGN),PASTE(OP,di3))

	.text
	.global NAME
	.proc NAME
NAME :	
	.prologue
	.regstk 2,0,0,0
	// Transfer inputs to FP registers.
	setf.sig f8 = in0
	setf.sig f9 = in1
	;;
	.fframe 16
	.save.f 0x20
	stf.spill [sp] = f17,-16

	// Convert the inputs to FP, to avoid FP software-assist faults.
	INT_TO_FP(f8, f8)
	;;

	.save.f 0x10
	stf.spill [sp] = f16
	.body
	INT_TO_FP(f9, f9)
	;;
	frcpa.s1 f17, p6 = f8, f9	// y0 = frcpa(b)
	;;
(p6)	fmpy.s1 f7 = f8, f17		// q0 = a*y0
(p6)	fnma.s1 f6 = f9, f17, f1	// e0 = -b*y0 + 1
	;;
(p6)	fma.s1 f16 = f7, f6, f7		// q1 = q0*e0 + q0
(p6)	fmpy.s1 f7 = f6, f6		// e1 = e0*e0
	;;
#ifdef MODULO
	sub in1 = r0, in1		// in1 = -b
#endif
(p6)	fma.s1 f16 = f16, f7, f16	// q2 = q1*e1 + q1
(p6)	fma.s1 f6 = f17, f6, f17	// y1 = y0*e0 + y0
	;;
(p6)	fma.s1 f6 = f6, f7, f6		// y2 = y1*e1 + y1
(p6)	fnma.s1 f7 = f9, f16, f8	// r = -b*q2 + a
	;;
#ifdef MODULO
	setf.sig f8 = in0		// f8 = a
	setf.sig f9 = in1		// f9 = -b
#endif
(p6)	fma.s1 f17 = f7, f6, f16	// q3 = r*y2 + q2
	;;
	.restore sp
	ldf.fill f16 = [sp], 16
	FP_TO_INT(f17, f17)		// q = trunc(q3)
	;;
#ifdef MODULO
	xma.l f17 = f17, f9, f8		// r = q*(-b) + a
	;;
#endif
	getf.sig r8 = f17		// transfer result to result register
	ldf.fill f17 = [sp]
	br.ret.sptk.many rp
	
	.size NAME, . - NAME
	.endp NAME
