/* reloc.S - position independent IA-64 ELF shared object relocator
   Copyright (C) 1999 Hewlett-Packard Co.
	Contributed by David Mosberger <davidm@hpl.hp.com>.
   Copyright (C) 2002 Eric Biederman sponsored by Linux Networx

   This file is part of etherboot.
   This file was derived from reloc_ia64.S from GNU-EFI, the GNU EFI development environment.

   GNU EFI is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GNU EFI is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU EFI; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA. */

/*
 * This is written in assembly because the entire code needs to be position
 * independent.  Note that the compiler does not generate code that's position
 * independent by itself because it relies on the global offset table being
 * relocated.
 *
 * This code assumes the code was compiled with -mconstant-gp -mauto-pic -static -shared
 * Which generates position independent code, but not position indepedent data.
 * This code assumes in the linker script the rela entries are bracked with:
 * _rela, _erela and _rela_size gives the total size of the rela entries.
 * This gives a much smaller binary than when compiled as a true shared object.
 *
 * This code assumes the original shared object was initially relocated,
 * So that it only needs to apply changes for the new address the code is linked
 * at.
 */
	.text
	.psr abi64
	.psr lsb
	.lsb


#define ST_VALUE_OFF	8		/* offset of st_value in elf sym */

#define RET_SUCCESS		0
#define RET_LOAD_ERROR		1

#define R_IA64_NONE		0
#define R_IA64_REL64MSB		0x6e
#define R_IA64_REL64LSB		0x6f
#define R_IA64_DIR64MSB		0x26
#define R_IA64_DIR64LSB		0x27
#define R_IA64_FPTR64MSB	0x46
#define R_IA64_FPTR64LSB	0x47

#define	delta	in0	/* Chaing in load address (address of .text) */

#define ldbase	r15
#define target  r16
#define val	r17
#define rela	r18
#define relasz  r19
#define relaent	r20
#define addr	r21
#define r_info	r22
#define r_offset r23
#define r_type	r25

#define Pmore		p6

#define Pnone		p6
#define Prel		p7
#define Pdir		p8


	.global _relocate
_relocate:
	alloc		r2=ar.pfs,1,0,0,0
	add		rela=@gprel(_rela),gp
	add		ldbase=@gprel(_text),gp
	add		r3=@ltoff(_rela_size),gp
	;;
	ld8		relasz = [r3]
	mov		relaent=24
	br.sptk.few	apply_relocs

apply_loop:
	ld8		r_offset = [rela]
	add		addr = 8,rela
	sub		relasz = relasz,relaent
	
	;;
	ld8		r_info = [addr], 8
	;; 
	add		target = ldbase, r_offset
	add		rela = rela,relaent
	extr.u		r_type = r_info, 0, 32
	;;
	cmp.eq		Pnone,p0 = R_IA64_NONE, r_type
	cmp.eq		Prel,p0  = R_IA64_REL64LSB, r_type
	cmp.eq		Pdir,p0  = R_IA64_DIR64LSB, r_type /* Needed? */
	;;
(Pnone) br.cond.sptk.few apply_relocs
(Prel)	br.cond.sptk.few apply_REL64
(Pdir)  br.cond.sptk.few apply_DIR64 /* Needed? */
	;; 

apply_error:
	mov		r8 = RET_LOAD_ERROR
	br.ret.sptk.few	rp

apply_REL64:
apply_DIR64:
	ld8		val = [target]
	;;
	add		val = val, delta
	;;
	st8		[target] = val
	;; 
	/* fall through to apply_relocs */
apply_relocs:
	cmp.ltu		Pmore,p0=0,relasz
(Pmore) br.cond.sptk.few	apply_loop
	;; 

	mov		r8 = RET_SUCCESS
	br.ret.sptk.few	rp
	
	.size	_relocate, . - _relocate
	.endp _relocate
