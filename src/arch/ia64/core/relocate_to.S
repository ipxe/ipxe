	/* Temporarily ignore the stack, as I am still using efi's stack */

#define newbase in0
#define base    loc2
#define newgp   loc3
#define len	loc4

	.explicit		
	.globl relocate_to
	.proc relocate_to
relocate_to:
	/* In incoming variable the new base addres of etherboot. */
	alloc	loc0=ar.pfs,1,5,3,0 /* in, local, out, rotating */
	mov	loc1=rp

	/* Compute the current location of _text */
	/* Compute the new gp value */
	add	base=@gprel(_text),gp
	add	len =@gprel(_end),gp
	movl	newgp=@gprel(_text)
	;;
	sub	newgp=newbase,newgp /* gp = _text - @gprel(_text) */
	sub	len=len,base
	;; 

	/* Copy etherboot to the new location */
	mov	out0=newbase
	mov	out1=base
	mov	out2=len
	br.call.sptk.few rp=memcpy
	;;

	/* Jump to my __relocate_to in the new location */
	movl	r14=@gprel(__relocate_to)
	;;
	add	r14=r14,newgp
	;;
	mov	b6=r14
	;;
	br.cond.sptk.few b6
	;; 
__relocate_to:	
	/* I am at the new location set the newgp as the default */
	mov	gp=newgp
	;; 
	/* New apply relocations to the new copy */
	sub	out0=newbase,base
	br.call.sptk.few rp=_relocate
	;;

	/* Lookup restart_etherboot */
	add	out0=@gprel(restart_etherboot),gp
	;;
	
	/* Adjust the gp and return address.
	 * NOTE: This only works when the setjmp can modify it's caller's gp
	 * address.  Essentially this means etherboot must be compiled with
	 * compiled with -mconstant-gp, though an inline version of setjmp might work.
	 */
	add	r14=0x40,out0
	add	r16=0x08,out0
	;; 
	ld8	r15=[r14]
	ld8	r17=[r16]
	;;	
	sub	r15=r15,base
	sub	r17=r17,base
	;;
	add	r15=r15,newbase
	add	r17=r17,newbase
	;;
	st8	[r14]=r15
	st8	[r16]=r17
	;;
	mov	out1=256
	br.call.sptk.few rp=longjmp

	/* And just in case lonjmp returns... */
	
	
	/* Adjust my return address and return */
	sub	loc1=loc1,base
	;;
	add	loc1=loc1,newbase
	;;
	mov	ar.pfs=loc0
	mov	rp=loc1
	;;
	br.ret.sptk.few rp
	
	.size relocate_to, . - relocate_to
	.endp relocate_to
