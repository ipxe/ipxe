	/* Trampoline for calling outside of etherboot */

	.text
	.globl	__call
	.proc	__call
__call:
	alloc	loc0=ar.pfs,8,3,8,0 /* in, local, out, rotating */
	mov	loc1=rp
	mov	loc2=gp
	ld8	r14=[in0],8
	;; 
	ld8	gp=[in0]
	mov	r28=in1		/* So we can use stacked pal calling conventions */
	mov	out0=in1
	mov	out1=in2
	mov	out2=in3
	mov	out3=in4
	mov	out4=in5
	mov	out5=in6
	mov	out6=in7
	mov	out7=0		/* So we can work with sal calling conventions */
	
	mov	b6=r14
	;;
	br.call.sptk.few rp=b6
	;;
	rsm			psr.i	/* disable interrupts */
	;; 
	mov	gp=loc2
	mov	rp=loc1
	;; 
	mov	ar.pfs=loc0
	br.ret.sptk.many rp
	
	.size __call, . - __call
	.endp __call


	.text
	.globl	pal_call
	.proc	pal_call
pal_call:
	alloc	loc0 = ar.pfs,4,3,0,0 /* in, local, out, rotating */
	mov	loc1 = rp
	mov	loc2 = gp
	add	r8 = @gprel(pal_entry),gp
	add	r9 = @gprel(pal_ret),gp
	;;
	ld8	r14 = [r8]
	;; 
	mov	r28 = in0
	mov	r29 = in1
	mov	r30 = in2
	mov	r31 = in3
	mov	b6 = r14
	mov	rp = r9
	rsm	psr.i	/* disable interrupts */
	;; 
	br.sptk.few	b6
	;; 
pal_ret:
	rsm	psr.i	/* disable interrupts */
	;;
	mov	gp=loc2
	mov	rp=loc1
	;;
	mov	ar.pfs=loc0
	br.ret.sptk.many rp
