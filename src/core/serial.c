#include "etherboot.h"
#include "timer.h"
#ifdef	CONSOLE_SERIAL

/*
 * The serial port interface routines implement a simple polled i/o
 * interface to a standard serial port.  Due to the space restrictions
 * for the boot blocks, no BIOS support is used (since BIOS requires
 * expensive real/protected mode switches), instead the rudimentary
 * BIOS support is duplicated here.
 *
 * The base address and speed for the i/o port are passed from the
 * Makefile in the COMCONSOLE and CONSPEED preprocessor macros.  The
 * line control parameters are currently hard-coded to 8 bits, no
 * parity, 1 stop bit (8N1).  This can be changed in init_serial().
 */

static int found = 0;

#if defined(COMCONSOLE)
#undef UART_BASE
#define UART_BASE COMCONSOLE
#endif

#ifndef UART_BASE
#error UART_BASE not defined
#endif

#if defined(CONSPEED)
#undef UART_BAUD
#define UART_BAUD CONSPEED
#endif

#ifndef UART_BAUD
#define UART_BAUD 115200
#endif

#if ((115200%UART_BAUD) != 0)
#error Bad ttys0 baud rate
#endif

#define COMBRD (115200/UART_BAUD)

/* Line Control Settings */
#ifndef	COMPARM
/* Set 8bit, 1 stop bit, no parity */
#define	COMPARM	0x03
#endif

#define UART_LCS COMPARM

/* Data */
#define UART_RBR 0x00
#define UART_TBR 0x00

/* Control */
#define UART_IER 0x01
#define UART_IIR 0x02
#define UART_FCR 0x02
#define UART_LCR 0x03
#define UART_MCR 0x04
#define UART_DLL 0x00
#define UART_DLM 0x01

/* Status */
#define UART_LSR 0x05
#define  UART_LSR_TEMPT 0x40	/* Transmitter empty */
#define  UART_LSR_THRE  0x20	/* Transmit-hold-register empty */
#define  UART_LSR_BI	0x10	/* Break interrupt indicator */
#define  UART_LSR_FE	0x08	/* Frame error indicator */
#define  UART_LSR_PE	0x04	/* Parity error indicator */
#define  UART_LSR_OE	0x02	/* Overrun error indicator */
#define  UART_LSR_DR	0x01	/* Receiver data ready */

#define UART_MSR 0x06
#define UART_SCR 0x07

#if defined(UART_MEM)
#define uart_readb(addr) readb((addr))
#define uart_writeb(val,addr) writeb((val),(addr))
#else
#define uart_readb(addr) inb((addr))
#define uart_writeb(val,addr) outb((val),(addr))
#endif

/*
 * void serial_putc(int ch);
 *	Write character `ch' to port UART_BASE.
 */
void serial_putc(int ch)
{
	int i;
	int status;
	if (!found) {
		/* no serial interface */
		return;
	}
	i = 1000; /* timeout */
	while(--i > 0) {
		status = uart_readb(UART_BASE + UART_LSR);
		if (status & UART_LSR_THRE) { 
			/* TX buffer emtpy */
			uart_writeb(ch, UART_BASE + UART_TBR);
			break;
		}
		mdelay(2);
	}
}

/*
 * int serial_getc(void);
 *	Read a character from port UART_BASE.
 */
int serial_getc(void)
{
	int status;
	int ch;
	do {
		status = uart_readb(UART_BASE + UART_LSR);
	} while((status & 1) == 0);
	ch = uart_readb(UART_BASE + UART_RBR);	/* fetch (first) character */
	ch &= 0x7f;				/* remove any parity bits we get */
	if (ch == 0x7f) {			/* Make DEL... look like BS */
		ch = 0x08;
	}
	return ch;
}

/*
 * int serial_ischar(void);
 *       If there is a character in the input buffer of port UART_BASE,
 *       return nonzero; otherwise return 0.
 */
int serial_ischar(void)
{
	int status;
	if (!found)
		return 0;
	status = uart_readb(UART_BASE + UART_LSR);	/* line status reg; */
	return status & 1;		/* rx char available */
}

/*
 * int serial_init(void);
 *	Initialize port UART_BASE to speed CONSPEED, line settings 8N1.
 */
int serial_init(void)
{
	int initialized = 0;
	int status;
	int divisor, lcs;

	if (found)
		return 1;

	divisor = COMBRD;
	lcs = UART_LCS;


#ifdef COMPRESERVE
	lcs = uart_readb(UART_BASE + UART_LCR) & 0x7f;
	uart_writeb(0x80 | lcs, UART_BASE + UART_LCR);
	divisor = (uart_readb(UART_BASE + UART_DLM) << 8) | uart_readb(UART_BASE + UART_DLL);
	uart_writeb(lcs, UART_BASE + UART_LCR);
#endif

	/* Set Baud Rate Divisor to CONSPEED, and test to see if the
	 * serial port appears to be present.
	 */
	uart_writeb(0x80 | lcs, UART_BASE + UART_LCR);
	uart_writeb(0xaa, UART_BASE + UART_DLL);
	if (uart_readb(UART_BASE + UART_DLL) != 0xaa) 
		goto out;
	uart_writeb(0x55, UART_BASE + UART_DLL);
	if (uart_readb(UART_BASE + UART_DLL) != 0x55)
		goto out;
	uart_writeb(divisor & 0xff, UART_BASE + UART_DLL);
	if (uart_readb(UART_BASE + UART_DLL) != (divisor & 0xff))
		goto out;
	uart_writeb(0xaa, UART_BASE + UART_DLM);
	if (uart_readb(UART_BASE + UART_DLM) != 0xaa) 
		goto out;
	uart_writeb(0x55, UART_BASE + UART_DLM);
	if (uart_readb(UART_BASE + UART_DLM) != 0x55)
		goto out;
	uart_writeb((divisor >> 8) & 0xff, UART_BASE + UART_DLM);
	if (uart_readb(UART_BASE + UART_DLM) != ((divisor >> 8) & 0xff))
		goto out;
	uart_writeb(lcs, UART_BASE + UART_LCR);
	
	/* disable interrupts */
	uart_writeb(0x0, UART_BASE + UART_IER);

	/* disable fifo's */
	uart_writeb(0x00, UART_BASE + UART_FCR);

	/* Set clear to send, so flow control works... */
	uart_writeb((1<<1), UART_BASE + UART_MCR);


	/* Flush the input buffer. */
	do {
		/* rx buffer reg
		 * throw away (unconditionally the first time)
		 */
		uart_readb(UART_BASE + UART_RBR);
		/* line status reg */
		status = uart_readb(UART_BASE + UART_LSR);
	} while(status & UART_LSR_DR);
	initialized = 1;
 out:
	found = initialized;
	return initialized;
}

/*
 * void serial_fini(void);
 *	Cleanup our use of the serial port, in particular flush the
 *	output buffer so we don't accidentially loose characters.
 */
void serial_fini(void)
{
	int i, status;
	if (!found) {
		/* no serial interface */
		return;
	}
	/* Flush the output buffer to avoid dropping characters,
	 * if we are reinitializing the serial port.
	 */
	i = 10000; /* timeout */
	do {
		status = uart_readb(UART_BASE + UART_LSR);
	} while((--i > 0) && !(status & UART_LSR_TEMPT));
}
#endif
