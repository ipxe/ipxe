/* Real-mode interface
 */

#ifndef ASSEMBLY

#include "etherboot.h"
#include "segoff.h"

typedef union {
	struct {
		union {
			uint8_t l;
			uint8_t byte;
		};
		uint8_t h;
	} PACKED;
	uint16_t word;
} PACKED reg16_t;

typedef union {
	reg16_t w;
	uint32_t dword;
} PACKED reg32_t;

/* Macros to help with defining inline real-mode trampoline fragments.
 */
#define RM_XSTR(x) #x	/* Macro hackery needed to stringify       */
#define RM_STR(x) RM_XSTR(x)
#define	RM_FRAGMENT(name, asm_code_str)		\
	extern void name ( void );		\
	extern void name ## _end (void);	\
	__asm__(				\
		".section \".text16\"\n\t"	\
		".code16\n\t"			\
		".arch i386\n\t"		\
		".globl " #name " \n\t"		\
		#name ":\n\t"			\
		asm_code_str "\n\t"		\
		".globl " #name "_end\n\t"	\
		#name "_end:\n\t"		\
		".code32\n\t"			\
		".previous\n\t"			\
	)

#define FRAGMENT_SIZE(fragment) ( (size_t) ( ( (void*) fragment ## _end )\
					     - ( (void*) (fragment) ) ) )

/* Data structures in _prot_to_real and _real_to_prot.  These
 * structures are accessed by assembly code as well as C code.
 */
typedef struct {
	uint32_t esp;
	uint16_t cs;
	uint16_t ss;
	uint32_t r2p_params;
} PACKED prot_to_real_params_t;

typedef struct {
	uint32_t ret_addr;
	uint32_t esp;
	uint32_t ebx;
	uint32_t esi;
	uint32_t edi;
	uint32_t ebp;
	uint32_t out_stack;
	uint32_t out_stack_len;
} PACKED real_to_prot_params_t;

/* Function prototypes: realmode.c
 */
#define real_call( fragment, in_stack, out_stack ) \
	_real_call ( fragment, FRAGMENT_SIZE(fragment), \
		     (void*)(in_stack), \
		     ( (in_stack) == NULL ? 0 : sizeof(*(in_stack)) ), \
		     (void*)(out_stack), \
		     ( (out_stack) == NULL ? 0 : sizeof(*(out_stack)) ) )
extern uint16_t _real_call ( void *fragment, int fragment_len,
			     void *in_stack, int in_stack_len,
			     void *out_stack, int out_stack_len );
/* Function prototypes: realmode_asm.S
 */
extern void rm_callback_interface;
extern uint16_t rm_callback_interface_size;
extern uint32_t rm_etherboot_location;
extern void _rm_in_call ( void );
extern void _rm_in_call_far ( void );

extern void _prot_to_real_prefix ( void );
extern void _prot_to_real_prefix_end ( void );
extern uint16_t prot_to_real_prefix_size;

extern void _real_to_prot_suffix ( void );
extern void _real_to_prot_suffix_end ( void );
extern uint16_t real_to_prot_suffix_size;

/* PXE assembler bits */
extern void pxe_callback_interface;
extern uint16_t pxe_callback_interface_size;
extern void _pxe_in_call_far ( void );
extern void _pxenv_in_call_far ( void );
extern void _pxe_intercept_int1a ( void );
extern segoff_t _pxe_intercepted_int1a;
extern segoff_t _pxe_pxenv_location;

/* Global variables
 */
extern uint32_t real_mode_stack;
extern size_t real_mode_stack_size;
extern int lock_real_mode_stack;


/* Function prototypes from basemem.c
 */
#ifdef LINUXBIOS
/* A silly hard code that let's the code compile and work. 
 * When this becomes a problem feel free to implement
 * something better.
 */
static inline void allot_real_mode_stack(void) { real_mode_stack = 0x7c00; } 
#else
void allot_real_mode_stack(void);
#endif

#endif /* ASSEMBLY */
