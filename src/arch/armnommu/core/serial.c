/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "etherboot.h"
#include "hardware.h"
#ifdef	CONSOLE_SERIAL

/*
 * void serial_putc(int ch);
 *	Write character `ch' to port UART_BASE.
 */
void serial_putc(int ch)
{
	/* wait for room in the 32 byte tx FIFO */
	while ((P2001_UART->r.STATUS & 0x3f) > /* 30 */ 0) ;
	P2001_UART->w.TX[0] = ch & 0xff;
}

/*
 * int serial_getc(void);
 *	Read a character from port UART_BASE.
 */
int serial_getc(void)
{
	while (((P2001_UART->r.STATUS >> 6) & 0x3f) == 0) ;
	return P2001_UART->r.RX[0] & 0xff;
}

/*
 * int serial_ischar(void);
 *       If there is a character in the input buffer of port UART_BASE,
 *       return nonzero; otherwise return 0.
 */
int serial_ischar(void)
{
	return (P2001_UART->r.STATUS >> 6) & 0x3f;
}

/*
 * int serial_init(void);
 *	Initialize port to speed 57.600, line settings 8N1.
 */
int serial_init(void)
{
	static unsigned int N;
	// const M=3
	P2001_UART->w.Clear = 0;		// clear
	N = ((SYSCLK/8)*3)/CONSPEED;
	P2001_UART->w.Baudrate = (N<<16)+3;	// set 57.600 BAUD
	P2001_UART->w.Config = 0xcc100;		// set 8N1, *water = 12
	return 1;
}

/*
 * void serial_fini(void);
 *	Cleanup our use of the serial port, in particular flush the
 *	output buffer so we don't accidentially loose characters.
 */
void serial_fini(void)
{
}
#endif
