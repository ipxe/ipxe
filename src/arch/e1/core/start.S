/*
 * Derived from the Hyperstone's library source code.
 * Copyright (C) 2002-2003 GDT, Yannis Mitsos <gmitsos@telecom.ntua.gr>
 *                              George Thanos <gthanos@telecom.ntua.gr>
 */    
	.global	Priority	; Task-Priority
	.global	_Stack1Size	; Size of hardware stack
	.global	_Stack2Size	; Size of aggregate stack
	.global	_Stack1Base	; Base of hardware stack
	.global	_Stack2Base	; Base of aggregate stack
	.global	_Mem0HeapBase	; Base of Heap in Mem0
	.global	_Mem1HeapBase	; Base of Heap in Mem1

	.global _init_peripherals
	.global	_main
	.global	Main

	.global	__exit
	.global	__fmode
	.global	__MaxArgCount

	.text
BasePtrs:
	.weak	G6Base,G7Base,G8Base,G9Base,G10Base,G11Base,G12Base,G13Base
	.long	G6Base,G7Base,G8Base,G9Base,G10Base,G11Base,G12Base,G13Base
BasePtrsEnd:
HeapPtrs:
	.long	_Mem0HeapBase
	.long	_Mem1HeapBase
HeapPtrsEnd:

__MaxArgCount:
        .long	32
__StandAloneMode:
	.long	0		; 0 indicate stand alone mode

;============================================================================;
; Startup-Code                                                               ;
;============================================================================;
	.data

; do not change the order of: __argc,..
__argc:	
	.long  0
__argv:	
	.long  0
__IsShell:
	.long  1
ErrorLevel:
	.long  0
__lab:	
	.long  0
__fmode:
	.long	0x4000		; O_TEXT    attribute

_isa_drivers:
	.long	0

_isa_drivers_end:
	.long	0


	.text
Main:
StartUp:
	FRAME	L5, L0
	MOVI	L2, __bss_start__	; clear the .bss segment
0:	CMPI	L2, __bss_end__
	BHE	0f
	DBR	0b
	STW.P	L2, 0
0:	SUM     L2, PC, BasePtrs-$	; Load BasePtrs G6-G13
	LDD.P   L2, G6
	LDD.P   L2, G8
;	LDD.P   L2, G10
	LDD.P   L2, G12
	MOVI    L2, 1
	SUM     L3, PC, __StandAloneMode-$
	STW.R   L3, L2

	;----------------------------------------------------------------;
	; Call main C function                                           ;
	;----------------------------------------------------------------;
2:	LDW.D	PC, L2, __argc - $	; pass count of arguments to main
	LDW.D   PC, L3, __argv - $	; pass pointer array to main
	CALL	L4, PC, _init_peripherals - $ 
	CALL    L4, PC, _main - $	; --> Call Main-Program
	CHK     PC, PC			; trap if execution arrives here

__exit:
	FRAME   L5, L1
	STW.D   PC, L0, ErrorLevel - $	; Store ERRORLEVEL

	CHK     PC, PC
	RET     PC, L1

	.global ___main
___main:
	FRAME   L4, L1
	MOVI    L3, 2
	STW.D   PC, L3, __StandAloneMode-$
	RET     PC, L1		; does not return

	.section _GOT_
	.long   Main+4		; OnCreate
	.long   Main+8		; OnError
	.long   BasePtrs	; G6
	.long   BasePtrs+4	; G7
	.long   BasePtrs+8	; G8
	
	.END
