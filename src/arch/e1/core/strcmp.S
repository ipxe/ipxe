/*
 * Derived from the Hyperstone's library source code.
 * Modefied src in order to apply the -mgnu-param compiler option.
 * Copyright (C) 2002-2003 GDT, Yannis Mitsos <gmitsos@telecom.ntua.gr>
 *                              George Thanos <gthanos@telecom.ntua.gr>
 */
        .text
	    .align   2
	    .global  _strcmp
;ENTRY(_strcmp)
_strcmp:
	    FRAME    L8,L2
	    CMPBI    L1, 3        # check for word alignment
	    BNZ      str_except
	    CMPBI    L0, 3        # check for word alignment
	    BNZ      str_except

start:
	    LDD.P    L1, L4       # post inc mode
	    LDD.P    L0, L6       # post inc mode
            CMPBI    L4, ANYBZ
            BE       correct1
            CMP      L4, L6
            BNE      correct1
            CMP      L5, L7
            BNE      correct
            CMPBI    L5, ANYBZ
            BE       correct
            CMPBI    L6, ANYBZ
            BE       correct1
            CMPBI    L7, ANYBZ
            BNE      start

correct:    MASK     L4, L5, 0xff000000
	    MASK     L6, L7, 0xff000000
            CMP      L4, L6
            BNE      Exit
            SHLI     L5, 8
            CMPI     L4, 0
            DBNE     correct
            SHLI     L7, 8
            MOV      L1, L4
            RET      PC, L2

Exit:       SUB      L4, L6       # Subtract chars
            SARI     L4, 24
            MOV      L1, L4
            RET      PC, L2

correct1:   MASK     L5, L4, 0xff000000
	    MASK     L7, L6, 0xff000000
            CMP      L5, L7
            BNE      Exit1
            SHLI     L4, 8
            CMPI     L5, 0
            DBNE     correct1
            SHLI     L6, 8
            MOV      L1, L5
            RET      PC, L2
Exit1:      SUB      L5, L7       # Subtract chars
            SARI     L5, 24
            MOV      L1, L5
            RET      PC, L2

testzero:   CMPI     L4, 0
            BE       L_5
str_except:
	    LDBU.N   L1, L4, 1    # Load *s1, compare bytes
	    LDBU.N   L0, L5, 1    # Load *s2, compare bytes
            CMP      L4, L5
	    BE       testzero
	    SUB      L4, L5       # Subtract chars
L_5:        MOV      L1, L4
	    RET      PC, L2
	    .END

