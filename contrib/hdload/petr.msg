From netboot-owner@baghira.han.de Thu Sep 16 12:08:44 1999
Return-Path: <netboot-owner@baghira.han.de>
Received: (from factotum@localhost)
	by baghira.han.de (8.9.3/8.9.3) id NAA23838
	for netboot-outgoing; Wed, 15 Sep 1999 13:12:44 +0200
X-Authentication-Warning: baghira.han.de: factotum set sender to owner-netboot using -f
Received: from hathi.han.de (root@hathi.han.de [192.109.225.1])
	by baghira.han.de (8.9.3/8.9.3) with ESMTP id NAA23785
	for <netboot@baghira.han.de>; Wed, 15 Sep 1999 13:11:02 +0200
Received: from vsb.cz (root@decsys.vsb.cz [158.196.149.9])
	by hathi.han.de (8.9.3/8.9.3) with ESMTP id NAA04707
	for <netboot@baghira.han.de>; Wed, 15 Sep 1999 13:11:00 +0200
Received: from nwfei1.vsb.cz (nwfei1.vsb.cz [158.196.146.13])
	by vsb.cz (8.9.3/8.9.1) with ESMTP id NAA22363
	for <netboot@baghira.han.de>; Wed, 15 Sep 1999 13:10:52 +0200 (MET DST)
Received: from FEI1/SpoolDir by nwfei1.vsb.cz (Mercury 1.44);
    15 Sep 99 13:10:50 +0100
Received: from SpoolDir by FEI1 (Mercury 1.44); 15 Sep 99 13:10:27 +0100
Received: from pcd403z.vsb.cz (158.196.146.9) by nwfei1.vsb.cz (Mercury 1.44) with ESMTP;
    15 Sep 99 13:10:25 +0100
Received: from oli10 by pcd403z.vsb.cz with local-esmtp (Exim 2.05 #1 (Debian))
	id 11RCxI-0000oT-00; Wed, 15 Sep 1999 13:10:28 +0200
Date: Wed, 15 Sep 1999 13:10:28 +0200 (CEST)
From: Petr Olivka <Petr.Olivka@vsb.cz>
To: netboot@baghira.han.de
Subject: netboot image on hard disk - it is easy
In-Reply-To: <37DF4BD4.E8FFF8FC@gsmbox.com>
Message-ID: <Pine.LNX.4.10.9909151247430.2936-100000@pcd403z.vsb.cz>
MIME-Version: 1.0
Content-Type: TEXT/PLAIN; charset=US-ASCII
Sender: owner-netboot@baghira.han.de
Precedence: bulk
Reply-To: netboot@baghira.han.de
X-Moderator: netboot-owner@baghira.han.de

It is good joke, at this moment I have only simple version of MBR to load
image from HD, but only from track 0. HD have to have enough sectors per
track for rom image.
And small program in turbo-pascal to download image to HD.

below is assembler code for MBR. Is writen for tasm and tlink. 
If you have 512 bytes binary file with MBR code, then concat it with
rom-image and download to hda. BUT NOT DIRECTLY !!!! You have to copy 
partition table ( and NT signature ) to MBR and then download. BUT ONLY tO
TRACK 0.

Everything in your own risk.

If I will have some free time, I will write some code directly to netboot.

poli

.model large, pascal

.code
.386
	public 	mbrasm, mbrend

ROMLOAD	equ	5000h

	org	600h

mbrasm	proc

	cli
	xor	ax, ax
	mov	ss, ax
	mov	sp, 7C00h
	mov	si, sp
	mov	es, ax
	mov	ds, ax
	sti
	cld
	mov	di, 600h
	mov	cx, 100h
	rep	movsw
	db	0EAh
	dw	offset @@jump
	dw	0
@@jump:
	mov	si, offset Hlaska
	call	@@print

        ; rozmery prvniho HD
        mov     ah, 8
        mov     dl, 80h
        int     13h
        jc      @@chyba
        ; dh - H, cx - CS

        ; prvi stopa obsahuje bootrom, tak ji natahneme do RAM
        mov     ah, 2
        mov     al, cl
        and     al, 3Fh
        dec     al
        mov     dx, 80h
        mov     cx, 2
        mov     bx, ROMLOAD
        mov     es, bx
        xor     bx, bx
        int     13h
        jc      @@chyba

        ; hromada kodu podle zdrojaku netboot
        xor     di, di
        mov     es, di
        mov     di, 380h
        push    di
        mov     cx, 10
        cld
        rep     stosw
        pop     di
        mov     word ptr [ di ], 5a5ah
        mov     byte ptr [ di + 2 ], 50h
        mov     word ptr [ di + 10h ], 0FFFFh
        mov     word ptr [ di + 12h ], 0FFFFh

        ; navratova adresa, kdyby nezabrala ROM
        mov	OfsErr, offset RomErr
        push    cs
        push    offset @@chyba

        mov     ax, ROMLOAD
        mov     es, ax
        push    es
        ; kouzelny jump....
        mov     si,  es:[ 1ah ]
        push    word ptr es:[ si + 1ah ] ; ...do bootrom v RAM
        retf

@@chyba:
	mov	si, OfsErr
	call	@@print
        mov     si, offset CRLF
	call	@@print
        jmp     @@chyba

@@print:
	lodsb
	cmp	al,0
	je	@@navrat
	push	si
	mov	bx,7
	mov	ah,0Eh
	int	10h
	pop	si
	jmp	@@print

@@navrat:
	retn

Hlaska	db	13, 10, 'HD Net Loader v1.0 (c) poli 1999', 13, 10, 0
CRLF	db	13, 10, 0
OfsErr	dw	offset Error
Error	db	'Error load from HD !', 0
RomErr	db	'ROM Error !', 0

mbrasm	endp

mbrend	proc
	ret
mbrend	endp

        org 800h - 2
        dw  0AA55h

end

===========================================================================
This Mail was sent to netboot mailing list by:
Petr Olivka <Petr.Olivka@vsb.cz>
To get help about this list, send a mail with 'help' as the only string in
it's body to majordomo@baghira.han.de. If you have problems with this list,
send a mail to netboot-owner@baghira.han.de.

