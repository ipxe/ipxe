/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
.global _start

/* Mode definitions */
#define Mode_USR	0x10
#define Mode_FIQ	0x11
#define Mode_IRQ	0x12
#define Mode_SVC	0x13
#define Mode_ABT	0x17
#define Mode_UNDEF	0x1B
#define Mode_SYS	0x1F				// only available on ARM Arch. v4
#define I_Bit		0x80
#define F_Bit		0x40

/* LPEC register definitions */
#define Adr_SYS_BASE			0x00100000
#define REL_Adr_SDRAM_Ctrl		0x10
#define REL_Adr_ExtMem_Ctrl		0x14
#define REL_Adr_WaitState_Ext		0x18
#define REL_Adr_WaitState_Asic		0x1c
#define Adr_TIMER_BASE			0x00110000
#define REL_Adr_Timer12_PreDiv		0x0c
#define REL_Adr_PLL_12000_config	0x30
#define REL_Adr_PLL_12288_config	0x34
#define REL_Adr_DIV_12288_config	0x38
#define REL_Adr_FSC_CONFIG		0x44
#define Adr_GPIO_BASE			0x00120000
#define REL_Adr_NRES_OUT		0x2c



/* Define entry point */
.arm							// Next instruction will be ARM
_start:

/*
 * Initialize memory system
 */


/*
 * Initialize stack pointer registers
 */

/* Enter SVC mode and set up the SVC stack pointer */
	mov	r0, #(Mode_SVC|I_Bit|F_Bit)
	msr	cpsr_c, r0
	ldr	sp, SP_SVC


/*
 * Initialize critical IO devices
 */

	/* watchdog off */
	mov	r0, #Adr_TIMER_BASE
	ldr	r1, Timer12_PreDiv
	str	r1, [r0, #REL_Adr_Timer12_PreDiv]
	
	/* NRES=1 */
	mov	r0, #Adr_GPIO_BASE
	ldr	r1, NRES_OUT
	str	r1, [r0, #REL_Adr_NRES_OUT]

	/* ExtMem */
	mov	r0, #Adr_SYS_BASE
	ldr	r1, ExtMem_Ctrl
	str	r1, [r0, #REL_Adr_ExtMem_Ctrl]

	/* SDRAM */
	mov	r0, #Adr_SYS_BASE
	ldr	r1, SDRAM_Ctrl
	str	r1, [r0, #REL_Adr_SDRAM_Ctrl]
/*
_wait_sdram_ctrl:
	ldr	r1, [r0]
	tst	r1, #0x20000
	beq	_wait_sdram_ctrl
*/

	/* WaitState_Ext */
	ldr	r1, WaitState_Ext
	str	r1, [r0, #REL_Adr_WaitState_Ext]
	/* WaitState_Asic */
	ldr	r1, WaitState_Asic
	str	r1, [r0, #REL_Adr_WaitState_Asic]

	/* PLL_12288 */
	mov	r0, #Adr_TIMER_BASE
	ldr	r1, PLL_12288_config
	str	r1, [r0, #REL_Adr_PLL_12288_config]
	/* DIV_12288 */
	ldr	r1, DIV_12288_config
	str	r1, [r0, #REL_Adr_DIV_12288_config]
	/* PLL_12200 */
	ldr	r1, PLL_12000_config
	str	r1, [r0, #REL_Adr_PLL_12000_config]

	/* FSC_CONFIG */
	ldr	r1, [r0, #REL_Adr_FSC_CONFIG]
	bic	r1, r1, #0x07
	ldr	r2, FSC_CONFIG
	orr	r1, r1, r2
	str	r1, [r0, #REL_Adr_FSC_CONFIG]


/*
 * Initialize interrupt system variables here
 */


/*
 * Initialize memory required by main C code
 */
	

/* jump to main program */
	mov	r0, #0
	b	main


Timer12_PreDiv:
	.word	0x40bb0000	/* watchdog off */
NRES_OUT:
	.word	0x00000003	/* NRES_OUT_DRV=1, NRES_OUT_DAT=1 */

#if SYSCLK == 73728000
ExtMem_Ctrl:
	.word	0x000000e8	/* fuer FPGA 32 Bit konfiguriert */
SDRAM_Ctrl:
//	.word	0x28fc0037	/* default */
	.word	0xaef40027	/* p2001_bit_compact */
WaitState_Ext:
	.word	0xa0001245	/* fuer 73 MHz */
//	.word	0x0000fff3	/* rom bootloader */
WaitState_Asic:
	.word	0x00ff8a5f	/* fuer 85 MHz */
//	.word	0x00000203	/* rom bootloader */
PLL_12288_config:
	.word	0x00000004	/* fuer 73 MHz */
DIV_12288_config:
	.word	0x00010601	/* fuer 73 MHz */
PLL_12000_config:
	.word	0x10004e75	/* fuer 85 MHz */
FSC_CONFIG:
	.word	0xc0000005	/* fuer 73 MHz */
#else
#error "Please define proper timings and wait states for that sysclk."
#endif

SP_SVC:
	.word	0x40fffffc



#ifndef	NORELOCATE
/**************************************************************************
RELOCATE_TO - relocate etherboot to the specified address
**************************************************************************/
	.global relocate_to

relocate_to:
	ldr	r1, =_start
	ldr	r2, =_end

	/* while (r1 < r2) { *(r0++) = *(r1++) } */
_relocate_loop:
	cmp	r1, r2
	ldrcc	r3, [r1], #4
	strcc	r3, [r0], #4
	bcc	_relocate_loop
	mov	pc, lr
#endif


.global __gccmain
__gccmain:
	mov	pc, lr		/* return from subroutine */
