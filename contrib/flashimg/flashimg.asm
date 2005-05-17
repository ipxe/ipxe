; Copyright (C) 1997 Markus Gutschke <gutschk@uni-muenster.de>
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

; Prepend this image file to an arbitrary ROM image. The resulting binary
; can be loaded from any BOOT-Prom that supports the "nbi" file format.
; When started, the image will reprogram the flash EPROM on the FlashCard
; ISA card. The flash EPROM has to be an AMD 29F010, and the programming
; algorithm is the same as that suggested by AMD in the appropriate data
; sheets.


#define SEGLOW		0xC800		/* lower range for EPROM segment     */
#define SEGHIGH		0xE800		/* upper range for EPROM segment     */
#define AMD_ID		0x2001		/* flash EPROM ID, only support AMD  */
#define ERASE1_CMD	0x80		/* first cmd for erasing full chip   */
#define ERASE2_CMD	0x10		/* second cmd for erasing full chip  */
#define READID_CMD	0x90		/* cmd to read chip ID               */
#define PROG_CMD	0xA0		/* cmd to program a byte             */
#define RESET_CMD	0xF0		/* cmd to reset chip state machine   */

;----------------------------------------------------------------------------


	.text
	.org	0

;	.globl	_main
_main:	mov	ax,#0x0FE0
	mov	ds,ax
	mov	ax,magic		; verify that we have been loaded by
	cmp	ax,#0xE4E4		; boot prom
	jnz	lderr
	jmpi	0x200,0x0FE0		; adjust code segment
lderr:	mov	si,#loaderr
	cld
lderrlp:seg	cs
	lodsb				; loop over all characters of
	or	al,al			; string
	jnz	lderrnx
	xor	ah,ah
	int	0x16			; wait for keypress
	jmpi	0x0000,0xFFFF		; reboot!
lderrnx:mov	ah,#0x0E		; print it
	mov	bl,#0x07
	xor	bh,bh
	int	0x10
	jmp	lderrlp

loaderr:.ascii	"The flash EPROM utility has to be loaded from a BOOT-Prom"
	.byte	0xa,0xd
	.ascii	"that knows about the 'nbi' file format!"
	.byte	0xa,0xd
	.ascii	"Reboot to proceed..."
	.byte	0

	.org	510
	.byte	0x55,0xAA

!----------------------------------------------------------------------------

start:	mov	ax,cs
	mov	ds,ax
	mov	ax,romdata		; verify that there is an Prom image
	cmp	ax,#0xAA55		; attached to the utility
	jnz	resmag
	mov	al,romdata+2
	or	al,al			; non-zero size is required
	jnz	magicok
resmag:	mov	si,#badmagic		; print error message
reset:	call	prnstr
	xor	ah,ah
	int	0x16			; wait for keypress
	jmpi	0x0000,0xFFFF		; reboot!
magicok:mov	di,#clrline1
	mov	si,#welcome		; print welcome message
inpnew:	call	prnstr
inprest:xor	bx,bx
	mov	cl,#0xC			; expect 4 nibbles input data
inploop:xor	ah,ah
	int	0x16
	cmp	al,#0x8			; <Backspace>
	jnz	inpnobs
	or	bx,bx			; there has to be at least one input ch
	jz	inperr
	mov	si,#delchar		; wipe out char from screen
	call	prnstr
	add	cl,#4			; compute bitmask for removing input
	mov	ch,cl
	mov	cl,#0xC
	sub	cl,ch
	mov	ax,#0xFFFF
	shr	ax,cl
	not	ax
	and	bx,ax
	mov	cl,ch
inploop1:jmp	inploop
inpnobs:cmp	al,#0x0D		; <Return>
	jnz	inpnocr
	or	bx,bx			; zero input -> autoprobing
	jz	inpdone
	cmp	cl,#-4			; otherwise there have to be 4 nibbles
	jz	inpdone
inperr:	mov	al,#7			; ring the console bell
	jmp	inpecho
inpnocr:cmp	al,#0x15		; <CTRL-U>
	jnz	inpnokl
	mov	si,di
	call	prnstr			; clear entire input and restart
	jmp	inprest
inpnokl:cmp	cl,#-4			; cannot input more than 4 nibbles
	jz	inperr
	cmp	al,#0x30		; '0'
	jb	inperr
	ja	inpdig
	or	bx,bx			; leading '0' is not allowed
	jz	inperr
inpdig:	cmp	al,#0x39		; '9'
	ja	inpnodg
	mov	ch,al
	sub	al,#0x30
inpnum:	xor	ah,ah			; compute new input value
	shl	ax,cl
	add	ax,bx
	test	ax,#0x1FF		; test for 8kB boundary
	jnz	inperr
	cmp	ax,#SEGHIGH		; input has to be below E800
	jae	inperr
	cmp	ax,#SEGLOW		; and above/equal C800
	jae	inpok
	cmp	cl,#0xC			; if there is just one nibble, yet,
	jnz	inperr			;   then the lower limit ix C000
	cmp	ax,#0xC000
	jb	inperr
inpok:	mov	bx,ax			; adjust bitmask
	sub	cl,#4
	mov	al,ch
inpecho:call	prnchr			; output new character
	jmp	inploop1
inpnodg:and	al,#0xDF		; lower case -> upper case
	cmp	al,#0x41		; 'A'
	jb	inperr
	cmp	al,#0x46		; 'F'
	ja	inperr
	mov	ch,al
	sub	al,#0x37
	jmp	inpnum
inpdone:or	bx,bx			; zero -> autoprobing
	jnz	probe
	mov	si,#automsg
	call	prnstr
	mov	cx,#0x10
	mov	bx,#SEGHIGH		; scan from E800 to C800
autoprb:sub	bx,#0x0200		; stepping down in 8kB increments
	mov	di,bx
	call	readid
	cmp	ax,#AMD_ID
	jz	prbfnd
	loop	autoprb
	mov	si,#failmsg
nofnd:	mov	di,#clrline2
	jmp	near inpnew		; failure -> ask user for new input
probe:	mov	di,bx
	test	bx,#0x07FF		; EPROM might have to be aligned to
	jz	noalign			;   32kB boundary
	call	readid
	cmp	ax,#AMD_ID		; check for AMDs id
	jz	prbfnd
	mov	si,#alignmsg
	call	prnstr
	and	bx,#0xF800		; enforce alignment of hardware addr
noalign:call	readid			; check for AMDs id
	cmp	ax,#AMD_ID
	jz	prbfnd
	mov	si,#nofndmsg		; could not find any EPROM at speci-
	call	prnstr			;   fied location --- even tried
	mov	si,#basemsg		;   aligning to 32kB boundary
	jmp	nofnd			; failure -> ask user for new input
prbfnd:	mov	si,#fndmsg
	call	prnstr			; we found a flash EPROM
	mov	ax,bx
	call	prnwrd
	mov	si,#ersmsg
	call	prnstr
	call	erase			; erase old contents
	jnc	ersdone
	mov	si,#failresmsg		; failure -> reboot machine
	jmp	near reset
ersdone:mov	si,#prg1msg		; tell user that we are about
	call	prnstr			;   to program the new data into
	mov	ax,di			;   the specified range
	call	prnwrd
	mov	si,#prg2msg
	call	prnstr
	xor	dh,dh
	mov	dl,romdata+2
	shl	dx,#1
	mov	ah,dh
	mov	cl,#4
	shl	ah,cl
	xor	al,al
	add	ax,di
	call	prnwrd
	mov	al,#0x3A		; ':'
	call	prnchr
	mov	ah,dl
	xor	al,al
	dec	ax
	call	prnwrd
	mov	al,#0x20
	call	prnchr
	mov	dh,romdata+2		; number of 512 byte blocks
	push	ds
	mov	ax,ds
	add	ax,#romdata>>4		; adjust segment descriptor, so that
	mov	ds,ax			;   we can handle images which are
prgloop:mov	cx,#0x200		;   larger than 64kB
	xor	si,si
	xor	bp,bp
	call	program			; program 512 data bytes
	jc	prgerr			; check error condition
	mov	ax,ds
	add	ax,#0x20		; increment segment descriptors
	mov	ds,ax
	add	di,#0x20
	dec	dh			; decrement counter
	jnz	prgloop
	pop	ds
	mov	si,#donemsg		; success -> reboot
prgdone:call	prnstr
	mov	si,#resetmsg
	jmp	near reset
prgerr:	pop	ds			; failure -> reboot
	mov	si,#failresmsg
	jmp	prgdone


;----------------------------------------------------------------------------

; READID -- read EPROM id number, base address is passed in BX
; ======
;
; changes: AX, DL, ES

readid:	mov	dl,#RESET_CMD		; reset chip
	call	sendop
	mov	dl,#READID_CMD
	call	sendop			; send READID command
	mov	es,bx
	seg	es
	mov	ax,0x00			; read manufacturer ID
	mov	dl,#RESET_CMD
	jmp	sendop			; reset chip


;----------------------------------------------------------------------------

; ERASE -- erase entire EPROM, base address is passed in BX
; =====
;
; changes: AL, CX, DL, ES, CF

erase:	mov	dl,#ERASE1_CMD
	call	sendop			; send ERASE1 command
	mov	dl,#ERASE2_CMD
	call	sendop			; send ERASE2 command
	xor	bp,bp
	mov	al,#0xFF
	push	di
	mov	di,bx
	call	waitop			; wait until operation finished
	pop	di
	jnc	erfail
	mov	dl,#RESET_CMD
	call	sendop			; reset chip
	stc
erfail:	ret


;----------------------------------------------------------------------------

; PROGRAM -- write data block at DS:SI of length CX into EPROM at DI:BP
; =======
;
; changes: AX, CX, DL, BP, ES, CF

program:mov	dl,#PROG_CMD
	call	sendop			; send programming command
	lodsb				; get next byte from buffer
	mov	es,di
	seg	es
	mov	byte ptr [bp],al	; write next byte into flash EPROM
	call	waitop			; wait until programming operation is
	jc	progdn			; completed
	inc	bp
	loop	program			; continue with next byte
	clc				; return without error
progdn:	ret


;----------------------------------------------------------------------------

; SENDOP -- send command in DL to EPROM, base address is passed in BX
; ======
;
; changes: ES

sendop:	mov	es,bx
	seg	es
	mov	byte ptr 0x5555,#0xAA	; write magic data bytes into
	jcxz	so1			;   magic locations. This unlocks
so1:	jcxz	so2			;   the flash EPROM. N.B. that the
so2:	seg	es			;   magic locations are mirrored
	mov	byte ptr 0x2AAA,#0x55	;   every 32kB; the hardware address
	jcxz	so3			;   might have to be adjusted to a
so3:	jcxz	so4			;   32kB boundary
so4:	seg	es
	mov	byte ptr 0x5555,dl
	ret


;----------------------------------------------------------------------------

; WAITOP -- wait for command to complete, address is passed in DI:BP
; ======
;
; for details on the programming algorithm, c.f. http://www.amd.com
;
; changes: AX, DL, ES, CF

waitop:	and	al,#0x80		; monitor bit 7
	mov	es,di
wait1:	seg	es			; read contents of EPROM cell that is
	mov	ah,byte ptr [bp]	;   being programmed
	mov	dl,ah
	and	ah,#0x80
	cmp	al,ah			; bit 7 indicates sucess
	je	waitok
	test	dl,#0x20		; bit 5 indicates timeout/error
	jz	wait1			; otherwise wait for cmd to complete
	seg	es
	mov	ah,byte ptr [bp]	; check error condition once again,
	and	ah,#0x80		;   because bits 7 and 5 can change
	cmp	al,ah			;   simultaneously
	je	waitok
	stc
	ret
waitok:	clc
	ret

;----------------------------------------------------------------------------

; PRNSTR -- prints a string in DS:SI onto the console
; ======
;
; changes: AL

prnstr:	push	si
	cld
prns1:	lodsb				; loop over all characters of
	or	al,al			; string
	jz	prns2
	call	prnchr			; print character
	jmp	prns1
prns2:	pop	si
	ret


;----------------------------------------------------------------------------

; PRNWRD, PRNBYT, PRNNIB, PRNCHR -- prints hexadezimal values, or ASCII chars
; ======  ======  ======  ======
;
; changes: AX

prnwrd:	push	ax
	mov	al,ah
	call	prnbyt			; print the upper byte
	pop	ax
prnbyt: push	ax
	shr	al,1			; prepare upper nibble
	shr	al,1
	shr	al,1
	shr	al,1
	call	prnnib			; print it
	pop	ax
prnnib:	and	al,#0x0F		; prepare lower nibble
	add	al,#0x30
	cmp	al,#0x39		; convert it into hex
	jle	prnchr
	add	al,#7
prnchr:	push	bx
	mov	ah,#0x0E		; print it
	mov	bl,#0x07
	xor	bh,bh
	int	0x10
	pop	bx
	ret


;----------------------------------------------------------------------------

magic:	.byte	0xE4,0xE4

badmagic:.byte	0xa,0xd
	.ascii	"There does not appear to be a ROM image attached to the"
	.ascii	"flash EPROM utility;"
	.byte	0xa,0xd
resetmsg:.ascii	"Reboot to proceed..."
	.byte	0
	
welcome:.byte	0xa,0xd
	.ascii	"Flash EPROM programming utility V1.0"
	.byte	0xa,0xd
	.ascii	"Copyright (c) 1997 by M. Gutschke <gutschk@uni-muenster.de>"
	.byte	0xa,0xd
	.ascii	"==========================================================="
	.byte	0xa,0xd
prompt:	.byte	0xa,0xd
	.ascii	"Enter base address for AMD29F010 flash EPROM on FlashCard or"
	.byte	0xa,0xd
	.ascii	"press <RETURN> to start autoprobing; the base address has"
	.byte	0xa
clrline1:.byte	0xd
	.ascii	"to be in the range C800..E600: "
	.ascii	"    "
	.byte	0x8,0x8,0x8,0x8
	.byte	0

delchar:.byte	0x8,0x20,0x8
	.byte	0

automsg:.ascii	"autoprobing... "
	.byte	0

failmsg:.ascii	"failed!"
basemsg:.byte	0xa
clrline2:.byte	0xd
	.ascii	"Enter base address: "
	.ascii	"    "
	.byte	0x8,0x8,0x8,0x8
	.byte	0

fndmsg:	.byte	0xa,0xd
	.ascii	"Found flash EPROM at: "
	.byte	0

alignmsg:.byte	0xa,0xd
	.ascii	"FlashCard requires the hardware address to be aligned to a"
	.byte	0xa,0xd
	.ascii	"32kB boundary; automatically adjusting..."
	.byte	0
	
nofndmsg:.byte	0xa,0xd
	.ascii	"No AMD29F010 flash EPROM found"
	.byte	0

ersmsg:	.byte	0xa,0xd
	.ascii	"Erasing old contents... "
	.byte	0

prg1msg:.ascii	"done"
	.byte	0xa,0xd
	.ascii	"Programming from "
	.byte	0
	
prg2msg:.ascii	":0000 to "
	.byte	0

donemsg:.ascii	"done!"
	.byte	0xa,0xd
	.byte	0
       
failresmsg:
	.ascii	"failed!"
	.byte	0xa,0xd
	.byte	0


;----------------------------------------------------------------------------

	.align	16
	.org	*-1
	.byte	0x00
romdata:
