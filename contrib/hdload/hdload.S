#if	!defined(USE_NASM) && !defined(USE_AS86)
#define	USE_AS86
#endif

#ifdef	USE_AS86
#define	CON(x)		*x
#define	BCON(x)		*x
#define	WCON(x)		*x
#define	LOC(x)		x
#define	BLOC(x)		byte ptr x
#define	WLOC(x)		word ptr x
#define	JMP(x)		jmp x
#define	STRDECL(s)	.ascii	s
#define	SEGCS		seg	cs
#define	SEGES		seg	es
#define	ALIGN(x)	.align	x
#define	SPACE(x)	.space	x
#endif

#ifdef	USE_NASM
#define	CON(x)		x
#define	BCON(x)		byte x
#define	WCON(x)		word x
#define	LOC(x)		[x]
#define	BLOC(x)		byte [x]
#define	WLOC(x)		word [x]
#define	JMP(x)		jmp short x
#define	STRDECL(s)	db	s
#define	SEGCS		cs
#define	SEGES		es
#define	ALIGN(x)	align x, db 0
#define	SPACE(x)	times x db 0
#endif

ROMLOAD	equ	0x5000

start:
	cli
	xor	ax, ax
	mov	ss, ax
	mov	sp, CON(0x7C00)
	mov	si, sp
	mov	es, ax
	mov	ds, ax
	sti
	cld
	mov	di, CON(0x600)
	mov	cx, CON(0x100)
	rep
	movsw
	db	0xEA
	dw	jump
	dw	0
jump:
	mov	si, CON(Hlaska)
	call	print

        ; rozmery prvniho HD
        mov     ah, CON(8)
        mov     dl, CON(0x80)
        int     0x13
        jc      chyba
        ; dh - H, cx - CS

        ; prvi stopa obsahuje bootrom, tak ji natahneme do RAM
        mov     ah, CON(2)
        mov     al, cl
        and     al, CON(0x3F)
        dec     al
        mov     dx, CON(0x80)
        mov     cx, CON(2)
        mov     bx, CON(ROMLOAD)
        mov     es, bx
        xor     bx, bx
        int     0x13
        jc      chyba

        ; hromada kodu podle zdrojaku netboot
        xor     di, di
        mov     es, di
        mov     di, CON(0x380)
        push    di
        mov     cx, CON(10)
        cld
        rep
	stosw
        pop     di
#ifdef	USE_AS86
        mov     word ptr [ di ], CON(0x5a5a)
        mov     byte ptr [ di + 2 ], CON(0x50)
        mov     word ptr [ di + 0x10 ], CON(0xFFFF)
        mov     word ptr [ di + 0x12 ], CON(0xFFFF)
#endif
#ifdef	USE_NASM
        mov     word [ di ], CON(0x5a5a)
        mov     byte [ di + 2 ], CON(0x50)
        mov     word [ di + 10h ], CON(0xFFFF)
        mov     word [ di + 12h ], CON(0xFFFF)
#endif

        ; navratova adresa, kdyby nezabrala ROM
	SEGCS
        mov	WLOC(OfsErr), CON(RomErr)
        push    cs
        push    WCON(chyba)
        mov     ax, CON(ROMLOAD)
        mov     es, ax
        push    es
        ; kouzelny jump....
	SEGES
        mov     si, [ 0x1a ]
	SEGES
#ifdef	USE_AS86
        push    word ptr [ si + 0x1a ] ; ...do bootrom v RAM
#endif
#ifdef	USE_NASM
        push    word [ si + 0x1a ] ; ...do bootrom v RAM
#endif
        retf

chyba:
	SEGCS
	mov	si, LOC(OfsErr)
	call	print
        mov     si, CON(CRLF)
	call	print
        JMP(chyba)

print:
	lodsb
	cmp	al,CON(0)
	je	navrat
	push	si
	mov	bx,CON(7)
	mov	ah,CON(0x0E)
	int	0x10
	pop	si
	JMP(print)

navrat:
	ret

Hlaska:	db	13, 10
	STRDECL('HD Net Loader v1.0 (c) poli 1999')
	db	13, 10, 0
CRLF:	db	13, 10, 0
OfsErr:	dw	Error
Error:	STRDECL('Error load from HD !')
	db	0
RomErr:	STRDECL('ROM Error !')
	db	0

mbrend:
	ret

#ifdef  USE_AS86
	org     510
#endif
#ifdef  USE_NASM
	times   510-($-$$) db 0
#endif
        dw	0xAA55
