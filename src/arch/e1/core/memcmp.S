/*
 * Derived from the Hyperstone's library source code.
 * Modefied src in order to apply the -mgnu-param compiler option.
 * Copyright (C) 2002-2003 GDT, Yannis Mitsos <gmitsos@telecom.ntua.gr>
 *                              George Thanos <gthanos@telecom.ntua.gr>
 */
	    .text
	    .align   2
	    .global  _memcmp

;ENTRY (_memcmp)
_memcmp:
	    FRAME    L9, L3                 # get incoming parameters
	    CMPBI    L2,3                   # check word alignment
	    BNZ      byte_compare
	    CMPBI    L1,3                   # check word alignment
	    BNZ      byte_compare

double_compare:
	    ADDI     L0, -8
	    BLT      is_equal
	    LDD.P    L1, L5
	    LDD.P    L2, L7
	    SUB      L5, L7
	    DBNZ     corr_8
	    SUB      L6, L8
	    BZ       double_compare
	    ADDI     L0, 4
	    ADDI     L2, -4
	    ADDI     L1, -4
	    BR       byte_compare

corr_8:     ADDI     L0, 8
	    ADDI     L2, -8
	    ADDI     L1, -8
byte_compare:
	    ADDI     L0, -1
	    BLT      equal
	    LDBU.N   L2, L5, 1              # Load and compare bytes
	    LDBU.N   L1, L6, 1
	    SUB      L5, L6
	    BZ       byte_compare
	    MOV      L2, L5
	    RET      PC, L3

is_equal:   CMPI     L0, -8
	    DBNE     byte_compare
	    ADDI     L0, 8
equal:
	    MOVI     L2, 0
	    RET      PC, L3

	    .END

