/*
 * Derived from the Hyperstone's library source code.
 * Modefied src in order to apply the -mgnu-param compiler option.
 * Copyright (C) 2002-2003 GDT, Yannis Mitsos <gmitsos@telecom.ntua.gr>
 *                              George Thanos <gthanos@telecom.ntua.gr>
 */      
		.text
	    .align   2
		.global _memset

;ENTRY(_memset)
_memset:    FRAME    L9, L3
	    MASK     L5, L1, 0xFF
	    MOV      L8, L2
	    CMPI     L0, 0                  # if n = 0 then return
            BE       retour

loop0:      CMPBI    L8, 0x3
	    BZ       word_bound
	    ADDI     L0, -1
	    DBNZ     loop0
	    STBU.N   L8, L5, 1
retour:     RET      PC, L3

word_bound:
	    CMPI     L0, 8
	    DBLT     loop2
	    MOV      L7, L5
	    SHLI     L7, 8
            OR       L5, L7
            MOV      L7, L5
            SHLI     L7, 16
            OR       L5, L7
            MOV      L6, L5
loop1:      ADDI     L0, -8
            CMPI     L0, 8
            DBGE     loop1
	    STD.P    L8, L5
            CMPI     L0, 0
            DBNZ     loop2
	    ANDNI    L5, ~ 0xFF
	    RET      PC, L3

loop2:      ADDI     L0, -1
	    DBNZ     loop2
	    STBU.N   L8, L5, 1
	    RET      PC, L3
