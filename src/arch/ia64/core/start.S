	.text
	.align 4
	.proc _start
	.globl _start
_start:
	{
	alloc			loc0 = ar.pfs,1,2,1,0	/* in, local, out, rotating */
	mov			loc1 = rp
	mov			r14 = ip		/* Get the address of _start */
	}
	movl			r15 = @gprel(_start)
	;;
	sub			gp = r14,r15
	;;
	rsm			psr.i	/* disable interrupts */
	;; 
	add			out0 = @gprel(_text),gp
	br.call.sptk.few	rp = _relocate
	;;
	cmp.eq			p6,p7 = r0,r8		/* r8 == SUCCESS? */
	mov			ar.pfs = loc0
	mov			rp = loc1
	;;
(p6)	br.cond.sptk.few	main
(p7)	br.ret.sptk.few		rp

	.size _start, . - _start
	.endp _start
