/*
 * Derived from the Hyperstone's library source code.
 * Modefied src in order to apply the -mgnu-param compiler option.
 * Copyright (C) 2002-2003 GDT, Yannis Mitsos <gmitsos@telecom.ntua.gr>
 *                              George Thanos <gthanos@telecom.ntua.gr>
 */    
		.text
	    .align   2
	    .global  _memcpy
;ENTRY(_memcpy)
_memcpy:
	    FRAME    L8, L3
	    MOV      L7, L2                 # Save for return

#*****************************
# Perform byte copy if both
# not on a word alignment
#*****************************
	    CMPBI    L2, 3                  # check word alignment
            BNZ      mem_except
	    CMPBI    L1, 3                  # check word alignment
            BNZ      mem_except

#*****************************
# Copy Double,Word,Halfword,
# then byte
#*****************************
DBL_LOOP:
	    CMPI     L0, 8                  # Copy Doubles
            BLT      DO_WORD
            LDD.P    L1, L5
            ADDI     L0, -8
            DBR      DBL_LOOP
            STD.P    L2, L5

DO_WORD:
	   CMPI      L0, 4                  # Copy leftover word
           BLT       DO_HALF
	   LDW.P     L1, L5
           ADDI      L0, -4
	   DBZ       DONE                   # Done if L0 is 0
           STW.P     L2, L5

DO_HALF:
	   CMPI      L0, 2                  # Copy leftover byte
           BLT       DO_BYTE
           LDHU.N    L1, L5, 2
           ADDI      L0, -2
	   DBZ       DONE                   # Done if L0 is 0
           STHU.N    L2, L5, 2

DO_BYTE:
	   CMPI      L0, 1                  # Copy leftover byte
           BLT       DONE
           LDBU.D    L1, L5, 0
           STBU.D    L2, L5, 0

DONE:                                       # Copy done
	   MOV       L2, L7                 # Return pointer
           RET       PC, L3

#****************************
# Byte memcpy
#****************************
mem_except:
           DBR       L_5
           MOVI      L6,0
L_3:
	   LDBS.D    L1, L5, 0              # Transfer the byte
           ADDI      L6, 1
           STBS.D    L2, L5, 0
           ADDI      L2, 1
           ADDI      L1, 1
L_5:                                        # Loop test
           CMP       L6, L0
	   BST       L_3
	   MOV       L2, L7                 # Return pointer
	   RET       PC, L3
	   .END
