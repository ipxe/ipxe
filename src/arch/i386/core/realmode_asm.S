/* Real-mode interface: assembly-language portions.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 */

#include "realmode.h"
#include "callbacks.h"

#if	1	/* CODE16 */
	
#define BOCHSBP xchgw %bx,%bx

#define NUM_PUSHA_REGS (8)
#define NUM_SEG_REGS (6)

	.text
	.arch i386
	.section ".text16.nocompress", "ax", @progbits
	.code16
	
	.equ	CR0_PE,1

#ifdef	GAS291
#define DATA32 data32;
#define ADDR32 addr32;
#define	LJMPI(x)	ljmp	x
#else
#define DATA32 data32
#define ADDR32 addr32
/* newer GAS295 require #define	LJMPI(x)	ljmp	*x */
#define	LJMPI(x)	ljmp	x
#endif

#ifdef PXE_EXPORT
/****************************************************************************
 * PXE CALLBACK INTERFACE
 *
 * Prepend this to rm_callback_interface to create a real-mode PXE
 * callback interface.
 ****************************************************************************
 */
	.section ".text16", "ax", @progbits
	.globl	pxe_callback_interface
	.code16
pxe_callback_interface:

/* Macro to calculate offset of labels within code segment in
 * installed copy of code.
 */
#define INSTALLED(x) ( (x) - pxe_callback_interface )

/****************************************************************************
 * PXE entry points (!PXE and PXENV+ APIs)
 ****************************************************************************
 */
	/* in_call mechanism for !PXE API calls */
	.globl	_pxe_in_call_far
_pxe_in_call_far:
	/* Prepend "PXE API call" and "API version 0x201" to stack */
	pushl	$0x201
	jmp	1f
	/* in_call mechanism for PXENV+ API calls */
	.globl	_pxenv_in_call_far
_pxenv_in_call_far:
	/* Prepend "PXE API call" and "API version 0x200" to stack */
	pushl	$0x200
1:	pushl	$EB_OPCODE_PXE
	/* Perform real-mode in_call */
	call	pxe_rm_in_call
	/* Return */
	addw	$8, %sp
	lret

/****************************************************************************
 * PXE installation check (INT 1A) code
 ****************************************************************************
 */
	.globl	_pxe_intercept_int1a
_pxe_intercept_int1a:
	pushfw
	cmpw	$0x5650, %ax
	jne	2f
1:	/* INT 1A,5650 - Intercept */
	popfw
	/* Set up return values according to PXE spec: */
	movw	$0x564e, %ax		/* AX := 564Eh (VN) */
	pushw	%cs:INSTALLED(_pxe_pxenv_segment)
	popw	%es			/* ES:BX := &(PXENV+ structure) */
	movw	%cs:INSTALLED(_pxe_pxenv_offset), %bx
	clc				/* CF is cleared */
	lret	$2			/* 'iret' without reloading flags */
2:	/* INT 1A,other - Do not intercept */
	popfw
	ljmp	%cs:*INSTALLED(_pxe_intercepted_int1a)

	.globl	_pxe_intercepted_int1a
_pxe_intercepted_int1a:	.word 0,0
	.globl	_pxe_pxenv_location
_pxe_pxenv_location:
_pxe_pxenv_offset:	.word 0
_pxe_pxenv_segment:	.word 0
	
pxe_rm_in_call:	
pxe_attach_rm:
	/* rm_callback_interface must be appended here */

pxe_callback_interface_end:

	.globl	_pxe_callback_interface_size
	.equ	_pxe_callback_interface_size, pxe_callback_interface_end - pxe_callback_interface
	.globl	pxe_callback_interface_size
pxe_callback_interface_size:
	.word	_pxe_callback_interface_size

#else	/* PXE_EXPORT */
	
/* Define symbols used by the linker scripts, to prevent link errors */
	.globl	_pxe_callback_interface_size
	.equ	_pxe_callback_interface_size, 0	
	
#endif	/* PXE_EXPORT */

#else	/* CODE16 */

/* Define symbols used by the linker scripts, to prevent link errors */
	.globl	_rm_callback_interface_size
	.equ	_rm_callback_interface_size, 0	
	.globl	_pxe_callback_interface_size
	.equ	_pxe_callback_interface_size, 0	
		
#endif	/* CODE16 */
