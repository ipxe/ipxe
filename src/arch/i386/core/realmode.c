/* Real-mode interface: C portions.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 */

#include "etherboot.h"
#include "realmode.h"
#include "segoff.h"

#define RM_STACK_SIZE ( 0x1000 )

/* gcc won't let us use extended asm outside a function (compiler
 * bug), ao we have to put these asm statements inside a dummy
 * function.
 */
static void work_around_gcc_bug ( void ) __attribute__ ((used));
static void work_around_gcc_bug ( void ) {
	/* Export _real_mode_stack_size as absolute linker symbol */
	__asm__ ( ".globl _real_mode_stack_size" );
	__asm__ ( ".equ _real_mode_stack_size, %c0" : : "i" (RM_STACK_SIZE) );
}

/* While Etherboot remains in base memory the real-mode stack is
 * placed in the Etherboot main stack.  The first allocation or
 * deallocation of base memory will cause a 'proper' real-mode stack
 * to be allocated.  This will happen before Etherboot is relocated to
 * high memory.
 */
uint32_t real_mode_stack = 0;
size_t real_mode_stack_size = RM_STACK_SIZE;
int lock_real_mode_stack = 0;	/* Set to make stack immobile */

/* Make a call to a real-mode code block.
 */

/* These is the structure that exists on the stack as the paramters
 * passed in to _real_call.  We pass a pointer to this struct to
 * prepare_real_call(), to save stack space.
 */
typedef struct {
	void *fragment;
	int fragment_len;
	void *in_stack;
	int in_stack_len;
	void *out_stack;
	int out_stack_len;
} real_call_params_t;

uint32_t prepare_real_call ( real_call_params_t *p,
			     int local_stack_len, char *local_stack ) {
	char *stack_base;
	char *stack_end;
	char *stack;
	char *s;
	prot_to_real_params_t *p2r_params;
	real_to_prot_params_t *r2p_params;

	/* Work out where we're putting the stack */
	if ( virt_to_phys(local_stack) < 0xa0000 ) {
		/* Current stack is in base memory.  We can therefore
		 * use it directly, with a constraint on the size that
		 * we don't know; assume that we can use up to
		 * real_mode_stack_size.  (Not a valid assumption, but
		 * it will do).
		 */
		stack_end = local_stack + local_stack_len;
		stack_base = stack_end - real_mode_stack_size;
	} else {
		if (!real_mode_stack) {
			allot_real_mode_stack();
		}
		/* Use the allocated real-mode stack in base memory.
		 * This has fixed start and end points.
		 */
		stack_base = phys_to_virt(real_mode_stack);
		stack_end = stack_base + real_mode_stack_size;
	}
	s = stack = stack_end - local_stack_len;

	/* Compile input stack and trampoline code to stack */
	if ( p->in_stack_len ) {
		memcpy ( s, p->in_stack, p->in_stack_len );
		s += p->in_stack_len;
	}
	memcpy ( s, _prot_to_real_prefix, prot_to_real_prefix_size );
	s += prot_to_real_prefix_size;
	p2r_params = (prot_to_real_params_t*) ( s - sizeof(*p2r_params) );
	memcpy ( s, p->fragment, p->fragment_len );
	s += p->fragment_len;
	memcpy ( s, _real_to_prot_suffix, real_to_prot_suffix_size );
	s += real_to_prot_suffix_size;
	r2p_params = (real_to_prot_params_t*) ( s - sizeof(*r2p_params) );

	/* Set parameters within compiled stack */
	p2r_params->ss = p2r_params->cs = SEGMENT ( stack_base );
	p2r_params->esp = virt_to_phys ( stack );
	p2r_params->r2p_params = virt_to_phys ( r2p_params );
	r2p_params->out_stack = ( p->out_stack == NULL ) ?
		0 : virt_to_phys ( p->out_stack );
	r2p_params->out_stack_len = p->out_stack_len;

	return virt_to_phys ( stack + p->in_stack_len );
}


/* Parameters are not genuinely unused; they are passed to
 * prepare_real_call() as part of a real_call_params_t struct.
 */
uint16_t _real_call ( void *fragment, int fragment_len,
		      void *in_stack __unused, int in_stack_len,
		      void *out_stack __unused, int out_stack_len __unused ) {
	uint16_t retval;

	/* This code is basically equivalent to
	 *
	 *	uint32_t trampoline;
	 *	char local_stack[ in_stack_len + prot_to_real_prefix_size +
	 *			  fragment_len + real_to_prot_suffix_size ];
	 *	trampoline = prepare_real_call ( &fragment, local_stack );
	 *	__asm__ ( "call _virt_to_phys\n\t"
	 *		  "call %%eax\n\t"
	 *		  "call _phys_to_virt\n\t"
	 *		  : "=a" (retval) : "0" (trampoline) );
	 *
	 * but implemented in assembly to avoid problems with not
	 * being certain exactly how gcc handles %esp.
	 */

	__asm__ ( "pushl %%ebp\n\t"
		  "movl  %%esp, %%ebp\n\t"	/* %esp preserved via %ebp */
		  "subl  %%ecx, %%esp\n\t"	/* space for inline RM stack */
		  "pushl %%esp\n\t"		/* set up RM stack */
		  "pushl %%ecx\n\t"
		  "pushl %%eax\n\t"
		  "call  prepare_real_call\n\t"	/* %eax = trampoline addr */
		  "addl  $12, %%esp\n\t"
		  "call  _virt_to_phys\n\t"	/* switch to phys addr */
		  "call  *%%eax\n\t"		/* call to trampoline */
		  "call  _phys_to_virt\n\t"	/* switch to virt addr */
		  "movl  %%ebp, %%esp\n\t"	/* restore %esp & %ebp */
		  "popl  %%ebp\n\t"
		  : "=a" ( retval )
		  : "0" ( &fragment )
		  , "c" ( ( ( in_stack_len + prot_to_real_prefix_size +
			      fragment_len + real_to_prot_suffix_size )
			    + 0x3 ) & ~0x3 ) );
	return retval;
}
