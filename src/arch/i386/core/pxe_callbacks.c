/* PXE callback mechanisms.  This file contains only the portions
 * specific to i386: i.e. the low-level mechanisms for calling in from
 * an NBP to the PXE stack and for starting an NBP from the PXE stack.
 */

#ifdef PXE_EXPORT

#include "etherboot.h"
#include "callbacks.h"
#include "realmode.h"
#include "pxe.h"
#include "pxe_callbacks.h"
#include "pxe_export.h"
#include "hidemem.h"
#include <stdarg.h>

#define INSTALLED(x) ( (typeof(&x)) ( (void*)(&x) \
				      - &pxe_callback_interface \
				      + (void*)&pxe_stack->arch_data ) )
#define pxe_intercept_int1a	INSTALLED(_pxe_intercept_int1a)
#define pxe_intercepted_int1a	INSTALLED(_pxe_intercepted_int1a)
#define pxe_pxenv_location	INSTALLED(_pxe_pxenv_location)
#define INT1A_VECTOR ( (segoff_t*) ( phys_to_virt( 4 * 0x1a ) ) )

/* The overall size of the PXE stack is ( sizeof(pxe_stack_t) +
 * pxe_callback_interface_size + rm_callback_interface_size ).
 * Unfortunately, this isn't a compile-time constant, since
 * {pxe,rm}_callback_interface_size depend on the length of the
 * assembly code in these interfaces.
 *
 * We used to have a function pxe_stack_size() which returned this
 * value.  However, it actually needs to be a link-time constant, so
 * that it can appear in the UNDIROMID structure in romprefix.S.  We
 * therefore export the three component sizes as absolute linker
 * symbols, get the linker to add them together and generate a new
 * absolute symbol _pxe_stack_size.  We then import this value into a
 * C variable pxe_stack_size, for access from C code.
 */

/* gcc won't let us use extended asm outside a function (compiler
 * bug), ao we have to put these asm statements inside a dummy
 * function.
 */
static void work_around_gcc_bug ( void ) __attribute__ ((used));
static void work_around_gcc_bug ( void ) {
	/* Export sizeof(pxe_stack_t) as absolute linker symbol */
	__asm__ ( ".globl _pxe_stack_t_size" );
	__asm__ ( ".equ _pxe_stack_t_size, %c0"
		  : : "i" (sizeof(pxe_stack_t)) );
}
/* Import _pxe_stack_size absolute linker symbol into C variable */
extern int pxe_stack_size;
__asm__ ( "pxe_stack_size: .long _pxe_stack_size" );

/* Utility routine: byte checksum
 */
uint8_t byte_checksum ( void *address, size_t size ) {
	unsigned int i, sum = 0;

	for ( i = 0; i < size; i++ ) {
		sum += ((uint8_t*)address)[i];
	}
	return (uint8_t)sum;
}

/* install_pxe_stack(): install PXE stack.
 * 
 * Use base = NULL for auto-allocation of base memory
 *
 * IMPORTANT: no further allocation of base memory should take place
 * before the PXE stack is removed.  This is to work around a small
 * but important deficiency in the PXE specification.
 */
pxe_stack_t * install_pxe_stack ( void *base ) {
	pxe_t *pxe;
	pxenv_t *pxenv;
	void *pxe_callback_code;
	void (*pxe_in_call_far)(void);
	void (*pxenv_in_call_far)(void);
	void *rm_callback_code;
	void *e820mangler_code;
	void *end;

	/* If already installed, just return */
	if ( pxe_stack != NULL ) return pxe_stack;

	/* Allocate base memory if requested to do so
	 */
	if ( base == NULL ) {
		base = allot_base_memory ( pxe_stack_size );
		if ( base == NULL ) return NULL;
	}

	/* Round address up to 16-byte physical alignment */
	pxe_stack = (pxe_stack_t *)
		( phys_to_virt ( ( virt_to_phys(base) + 0xf ) & ~0xf ) );
	/* Zero out allocated stack */
	memset ( pxe_stack, 0, sizeof(*pxe_stack) );
	
	/* Calculate addresses for portions of the stack */
	pxe = &(pxe_stack->pxe);
	pxenv = &(pxe_stack->pxenv);
	pxe_callback_code = &(pxe_stack->arch_data);
	pxe_in_call_far = _pxe_in_call_far +  
		( pxe_callback_code - &pxe_callback_interface );
	pxenv_in_call_far = _pxenv_in_call_far +
		( pxe_callback_code - &pxe_callback_interface );
	rm_callback_code = pxe_callback_code + pxe_callback_interface_size;
	
	e820mangler_code = (void*)(((int)rm_callback_code +
				    rm_callback_interface_size + 0xf ) & ~0xf);
	end = e820mangler_code + e820mangler_size;

	/* Initialise !PXE data structures */
	memcpy ( pxe->Signature, "!PXE", 4 );
	pxe->StructLength = sizeof(*pxe);
	pxe->StructRev = 0;
	pxe->reserved_1 = 0;
	/* We don't yet have an UNDI ROM ID structure */
	pxe->UNDIROMID.segment = 0;
	pxe->UNDIROMID.offset = 0;
	/* or a BC ROM ID structure */
	pxe->BaseROMID.segment = 0;
	pxe->BaseROMID.offset = 0;
	pxe->EntryPointSP.segment = SEGMENT(pxe_stack);
	pxe->EntryPointSP.offset = (void*)pxe_in_call_far - (void*)pxe_stack;
	/* No %esp-compatible entry point yet */
	pxe->EntryPointESP.segment = 0;
	pxe->EntryPointESP.offset = 0;
	pxe->StatusCallout.segment = -1;
	pxe->StatusCallout.offset = -1;
	pxe->reserved_2 = 0;
	pxe->SegDescCn = 7;
	pxe->FirstSelector = 0;
	/* PXE specification doesn't say anything about when the stack
	 * space should get freed.  We work around this by claiming it
	 * as our data segment as well.
	 */
	pxe->Stack.Seg_Addr = pxe->UNDIData.Seg_Addr = real_mode_stack >> 4;
	pxe->Stack.Phy_Addr = pxe->UNDIData.Phy_Addr = real_mode_stack;
	pxe->Stack.Seg_Size = pxe->UNDIData.Seg_Size = real_mode_stack_size;
	/* Code segment has to be the one containing the data structures... */
	pxe->UNDICode.Seg_Addr = SEGMENT(pxe_stack);
	pxe->UNDICode.Phy_Addr = virt_to_phys(pxe_stack);
	pxe->UNDICode.Seg_Size = end - (void*)pxe_stack;
	/* No base code loaded */
	pxe->BC_Data.Seg_Addr = 0;
	pxe->BC_Data.Phy_Addr = 0;
	pxe->BC_Data.Seg_Size = 0;
	pxe->BC_Code.Seg_Addr = 0;
	pxe->BC_Code.Phy_Addr = 0;
	pxe->BC_Code.Seg_Size = 0;
	pxe->BC_CodeWrite.Seg_Addr = 0;
	pxe->BC_CodeWrite.Phy_Addr = 0;
	pxe->BC_CodeWrite.Seg_Size = 0;
	pxe->StructCksum -= byte_checksum ( pxe, sizeof(*pxe) );

	/* Initialise PXENV+ data structures */
	memcpy ( pxenv->Signature, "PXENV+", 6 );
	pxenv->Version = 0x201;
	pxenv->Length = sizeof(*pxenv);
	pxenv->RMEntry.segment = SEGMENT(pxe_stack);
	pxenv->RMEntry.offset = (void*)pxenv_in_call_far - (void*)pxe_stack;
	pxenv->PMOffset = 0; /* "Do not use" says the PXE spec */
	pxenv->PMSelector = 0; /* "Do not use" says the PXE spec */
	pxenv->StackSeg = pxenv->UNDIDataSeg = real_mode_stack >> 4;
	pxenv->StackSize = pxenv->UNDIDataSize = real_mode_stack_size;
	pxenv->BC_CodeSeg = 0;
	pxenv->BC_CodeSize = 0;
	pxenv->BC_DataSeg = 0;
	pxenv->BC_DataSize = 0;
	/* UNDIData{Seg,Size} set above */
	pxenv->UNDICodeSeg = SEGMENT(pxe_stack);
	pxenv->UNDICodeSize = end - (void*)pxe_stack;
	pxenv->PXEPtr.segment = SEGMENT(pxe);
	pxenv->PXEPtr.offset = OFFSET(pxe);
	pxenv->Checksum -= byte_checksum ( pxenv, sizeof(*pxenv) );

	/* Mark stack as inactive */
	pxe_stack->state = CAN_UNLOAD;

	/* Install PXE and RM callback code and E820 mangler */
	memcpy ( pxe_callback_code, &pxe_callback_interface,
		 pxe_callback_interface_size );
	install_rm_callback_interface ( rm_callback_code, 0 );
	install_e820mangler ( e820mangler_code );

	return pxe_stack;
}

/* Use the UNDI data segment as our real-mode stack.  This is for when
 * we have been loaded via the UNDI loader
 */
void use_undi_ds_for_rm_stack ( uint16_t ds ) {
	forget_real_mode_stack();
	real_mode_stack = virt_to_phys ( VIRTUAL ( ds, 0 ) );
	lock_real_mode_stack = 1;
}

/* Activate PXE stack (i.e. hook interrupt vectors).  The PXE stack
 * *can* be used before it is activated, but it really shoudln't.
 */
int hook_pxe_stack ( void ) {
	if ( pxe_stack == NULL ) return 0;
	if ( pxe_stack->state >= MIDWAY ) return 1;

	/* Hook INT15 handler */
	hide_etherboot();

	/* Hook INT1A handler */
	*pxe_intercepted_int1a = *INT1A_VECTOR;
	pxe_pxenv_location->segment = SEGMENT(pxe_stack);
	pxe_pxenv_location->offset = (void*)&pxe_stack->pxenv
		- (void*)pxe_stack;
	INT1A_VECTOR->segment = SEGMENT(&pxe_stack->arch_data);
	INT1A_VECTOR->offset = (void*)pxe_intercept_int1a
		- (void*)&pxe_stack->arch_data;

	/* Mark stack as active */
	pxe_stack->state = MIDWAY;
	return 1;
}

/* Deactivate the PXE stack (i.e. unhook interrupt vectors).
 */
int unhook_pxe_stack ( void ) {
	if ( pxe_stack == NULL ) return 0;
	if ( pxe_stack->state <= CAN_UNLOAD ) return 1;

	/* Restore original INT15 and INT1A handlers */
	*INT1A_VECTOR = *pxe_intercepted_int1a;
	if ( !unhide_etherboot() ) {
		/* Cannot unhook INT15.  We're up the creek without
		 * even a suitable log out of which to fashion a
		 * paddle.  There are some very badly behaved NBPs
		 * that will ignore plaintive pleas such as
		 * PXENV_KEEP_UNDI and just zero out our code anyway.
		 * This means they end up vapourising an active INT15
		 * handler, which is generally not a good thing to do.
		 */
		return 0;
	}

	/* Mark stack as inactive */
	pxe_stack->state = CAN_UNLOAD;
	return 1;
}

/* remove_pxe_stack(): remove PXE stack installed by install_pxe_stack()
 */
void remove_pxe_stack ( void ) {
	/* Ensure stack is deactivated, then free up the memory */
	if ( ensure_pxe_state ( CAN_UNLOAD ) ) {
		forget_base_memory ( pxe_stack, pxe_stack_size );
		pxe_stack = NULL;
	} else {
		printf ( "Cannot remove PXE stack!\n" );
	}
}

/* xstartpxe(): start up a PXE image
 */
int xstartpxe ( void ) {
	int nbp_exit;
	struct {
		reg16_t bx;
		reg16_t es;
		segoff_t pxe;
	} PACKED in_stack;
	
	/* Set up registers and stack parameters to pass to PXE NBP */
	in_stack.es.word = SEGMENT(&(pxe_stack->pxenv));
	in_stack.bx.word = OFFSET(&(pxe_stack->pxenv));
	in_stack.pxe.segment = SEGMENT(&(pxe_stack->pxe));
	in_stack.pxe.offset = OFFSET(&(pxe_stack->pxe));

	/* Real-mode trampoline fragment used to jump to PXE NBP
	 */
	RM_FRAGMENT(jump_to_pxe_nbp, 
		"popw %bx\n\t"
		"popw %es\n\t"
		"lcall $" RM_STR(PXE_LOAD_SEGMENT) ", $" RM_STR(PXE_LOAD_OFFSET) "\n\t"
	);

	/* Call to PXE image */
	gateA20_unset();
	nbp_exit = real_call ( jump_to_pxe_nbp, &in_stack, NULL );
	gateA20_set();

	return nbp_exit;
}

int pxe_in_call ( in_call_data_t *in_call_data, va_list params ) {
	/* i386 calling conventions; the only two defined by Intel's
	 * PXE spec.
	 *
	 * Assembly code must pass a long containing the PXE version
	 * code (i.e. 0x201 for !PXE, 0x200 for PXENV+) as the first
	 * parameter after the in_call opcode.  This is used to decide
	 * whether to take parameters from the stack (!PXE) or from
	 * registers (PXENV+).
	 */
	uint32_t api_version = va_arg ( params, typeof(api_version) );
	uint16_t opcode;
	segoff_t segoff;
	t_PXENV_ANY *structure;
		
	if ( api_version >= 0x201 ) {
		/* !PXE calling convention */
		pxe_call_params_t pxe_params
			= va_arg ( params, typeof(pxe_params) );
		opcode = pxe_params.opcode;
		segoff = pxe_params.segoff;
	} else {
		/* PXENV+ calling convention */
		opcode = in_call_data->pm->regs.bx;
		segoff.segment = in_call_data->rm->seg_regs.es;
		segoff.offset = in_call_data->pm->regs.di;
	}
	structure = VIRTUAL ( segoff.segment, segoff.offset );
	return pxe_api_call ( opcode, structure );
}

#ifdef TEST_EXCLUDE_ALGORITHM
/* This code retained because it's a difficult algorithm to tweak with
 * confidence
 */
int ___test_exclude ( int start, int len, int estart, int elen, int fixbase );
void __test_exclude ( int start, int len, int estart, int elen, int fixbase ) {
	int newrange = ___test_exclude ( start, len, estart, elen, fixbase );
	int newstart = ( newrange >> 16 ) & 0xffff;
	int newlen = ( newrange & 0xffff );

	printf ( "[%x,%x): excluding [%x,%x) %s gives [%x,%x)\n",
		 start, start + len,
		 estart, estart + elen,
		 ( fixbase == 0 ) ? "  " : "fb",
		 newstart, newstart + newlen );
}
void _test_exclude ( int start, int len, int estart, int elen ) {
	__test_exclude ( start, len, estart, elen, 0 );
	__test_exclude ( start, len, estart, elen, 1 );
}
void test_exclude ( void ) {
	_test_exclude ( 0x8000, 0x1000, 0x0400, 0x200 ); /* before */
	_test_exclude ( 0x8000, 0x1000, 0x9000, 0x200 ); /* after */
	_test_exclude ( 0x8000, 0x1000, 0x7f00, 0x200 ); /* before overlap */
	_test_exclude ( 0x8000, 0x1000, 0x8f00, 0x200 ); /* after overlap */
	_test_exclude ( 0x8000, 0x1000, 0x8000, 0x200 ); /* align start */
	_test_exclude ( 0x8000, 0x1000, 0x8e00, 0x200 ); /* align end */
	_test_exclude ( 0x8000, 0x1000, 0x8100, 0x200 ); /* early overlap */
	_test_exclude ( 0x8000, 0x1000, 0x8d00, 0x200 ); /* late overlap */
	_test_exclude ( 0x8000, 0x1000, 0x7000, 0x3000 ); /* total overlap */
	_test_exclude ( 0x8000, 0x1000, 0x8000, 0x1000 ); /* exact overlap */
}
#endif /* TEST_EXCLUDE_ALGORITHM */

#else /* PXE_EXPORT */

/* Define symbols used by the linker scripts, to prevent link errors */
__asm__ ( ".globl _pxe_stack_t_size" );
__asm__ ( ".equ _pxe_stack_t_size, 0" );

#endif /* PXE_EXPORT */
