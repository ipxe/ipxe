/*
 * Copyright (C) 2008 Stefan Hajnoczi <stefanha@gmail.com>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**
 * @file
 *
 * GDB stub for remote debugging
 *
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <gpxe/process.h>
#include <gpxe/serial.h>
#include "gdbmach.h"

enum {
	POSIX_EINVAL = 0x1c /* used to report bad arguments to GDB */
};

struct gdbstub {
	int signo;
	gdbreg_t *regs;
	int exit_handler; /* leave interrupt handler */

	void ( * parse ) ( struct gdbstub *stub, char ch );
	uint8_t cksum1;

	/* Buffer for payload data when parsing a packet.  Once the
	 * packet has been received, this buffer is used to hold
	 * the reply payload. */
	char payload [ 256 ];
	int len;
};

/* Packet parser states */
static void gdbstub_state_new ( struct gdbstub *stub, char ch );
static void gdbstub_state_data ( struct gdbstub *stub, char ch );
static void gdbstub_state_cksum1 ( struct gdbstub *stub, char ch );
static void gdbstub_state_cksum2 ( struct gdbstub *stub, char ch );
static void gdbstub_state_wait_ack ( struct gdbstub *stub, char ch );

static uint8_t gdbstub_from_hex_digit ( char ch ) {
	return ( isdigit ( ch ) ? ch - '0' : tolower ( ch ) - 'a' + 0xa ) & 0xf;
}

static uint8_t gdbstub_to_hex_digit ( uint8_t b ) {
	b &= 0xf;
	return ( b < 0xa ? '0' : 'a' - 0xa ) + b;
}

static void gdbstub_from_hex_buf ( char *dst, char *src, int len ) {
	while ( len-- > 0 ) {
		*dst = gdbstub_from_hex_digit ( *src++ );
		if ( len-- > 0 ) {
			*dst = (*dst << 4) | gdbstub_from_hex_digit ( *src++ );
		}
		dst++;
	}
}

static void gdbstub_to_hex_buf ( char *dst, char *src, int len ) {
	while ( len-- > 0 ) {
		*dst++ = gdbstub_to_hex_digit ( *src >> 4 );
		*dst++ = gdbstub_to_hex_digit ( *src++ );
	}
}

static uint8_t gdbstub_cksum ( char *data, int len ) {
	uint8_t cksum = 0;
	while ( len-- > 0 ) {
		cksum += ( uint8_t ) *data++;
	}
	return cksum;
}

static int gdbstub_getchar ( struct gdbstub *stub ) {
	if ( stub->exit_handler ) {
		return -1;
	}
	return serial_getc();
}

static void gdbstub_putchar ( struct gdbstub * stub __unused, char ch ) {
	serial_putc ( ch );
}

static void gdbstub_tx_packet ( struct gdbstub *stub ) {
	uint8_t cksum = gdbstub_cksum ( stub->payload, stub->len );
	int i;

	gdbstub_putchar ( stub, '$' );
	for ( i = 0; i < stub->len; i++ ) {
		gdbstub_putchar ( stub, stub->payload [ i ] );
	}
	gdbstub_putchar ( stub, '#' );
	gdbstub_putchar ( stub, gdbstub_to_hex_digit ( cksum >> 4 ) );
	gdbstub_putchar ( stub, gdbstub_to_hex_digit ( cksum ) );

	stub->parse = gdbstub_state_wait_ack;
}

/* GDB commands */
static void gdbstub_send_ok ( struct gdbstub *stub ) {
	stub->payload [ 0 ] = 'O';
	stub->payload [ 1 ] = 'K';
	stub->len = 2;
	gdbstub_tx_packet ( stub );
}

static void gdbstub_send_num_packet ( struct gdbstub *stub, char reply, int num ) {
	stub->payload [ 0 ] = reply;
	stub->payload [ 1 ] = gdbstub_to_hex_digit ( ( char ) num >> 4 );
	stub->payload [ 2 ] = gdbstub_to_hex_digit ( ( char ) num );
	stub->len = 3;
	gdbstub_tx_packet ( stub );
}

/* Format is arg1,arg2,...,argn:data where argn are hex integers and data is not an argument */
static int gdbstub_get_packet_args ( struct gdbstub *stub, unsigned long *args, int nargs, int *stop_idx ) {
	int i;
	char ch = 0;
	int argc = 0;
	unsigned long val = 0;
	for ( i = 1; i < stub->len && argc < nargs; i++ ) {
		ch = stub->payload [ i ];
		if ( ch == ':' ) {
			break;
		} else if ( ch == ',' ) {
			args [ argc++ ] = val;
			val = 0;
		} else {
			val = ( val << 4 ) | gdbstub_from_hex_digit ( ch );
		}
	}
	if ( stop_idx ) {
		*stop_idx = i;
	}
	if ( argc < nargs ) {
		args [ argc++ ] = val;
	}
	return ( ( i == stub->len || ch == ':' ) && argc == nargs );
}

static void gdbstub_send_errno ( struct gdbstub *stub, int errno ) {
	gdbstub_send_num_packet ( stub, 'E', errno );
}

static void gdbstub_report_signal ( struct gdbstub *stub ) {
	gdbstub_send_num_packet ( stub, 'S', stub->signo );
}

static void gdbstub_read_regs ( struct gdbstub *stub ) {
	gdbstub_to_hex_buf ( stub->payload, ( char * ) stub->regs, GDBMACH_SIZEOF_REGS );
	stub->len = GDBMACH_SIZEOF_REGS * 2;
	gdbstub_tx_packet ( stub );
}

static void gdbstub_write_regs ( struct gdbstub *stub ) {
	if ( stub->len != 1 + GDBMACH_SIZEOF_REGS * 2 ) {
		gdbstub_send_errno ( stub, POSIX_EINVAL );
		return;
	}
	gdbstub_from_hex_buf ( ( char * ) stub->regs, &stub->payload [ 1 ], stub->len );
	gdbstub_send_ok ( stub );
}

static void gdbstub_read_mem ( struct gdbstub *stub ) {
	unsigned long args [ 2 ];
	if ( !gdbstub_get_packet_args ( stub, args, sizeof args / sizeof args [ 0 ], NULL ) ) {
		gdbstub_send_errno ( stub, POSIX_EINVAL );
		return;
	}
	args [ 1 ] = ( args [ 1 ] < sizeof stub->payload / 2 ) ? args [ 1 ] : sizeof stub->payload / 2;
	gdbstub_to_hex_buf ( stub->payload, ( char * ) args [ 0 ], args [ 1 ] );
	stub->len = args [ 1 ] * 2;
	gdbstub_tx_packet ( stub );
}

static void gdbstub_write_mem ( struct gdbstub *stub ) {
	unsigned long args [ 2 ];
	int colon;
	if ( !gdbstub_get_packet_args ( stub, args, sizeof args / sizeof args [ 0 ], &colon ) ||
			colon >= stub->len || stub->payload [ colon ] != ':' ||
			( stub->len - colon - 1 ) % 2 != 0 ) {
		gdbstub_send_errno ( stub, POSIX_EINVAL );
		return;
	}
	gdbstub_from_hex_buf ( ( char * ) args [ 0 ], &stub->payload [ colon + 1 ], stub->len - colon - 1 );
	gdbstub_send_ok ( stub );
}

static void gdbstub_continue ( struct gdbstub *stub, int single_step ) {
	gdbreg_t pc;
	if ( stub->len > 1 && gdbstub_get_packet_args ( stub, &pc, 1, NULL ) ) {
		gdbmach_set_pc ( stub->regs, pc );
	}
	gdbmach_set_single_step ( stub->regs, single_step );
	stub->exit_handler = 1;
	/* Reply will be sent when we hit the next breakpoint or interrupt */
}

static void gdbstub_rx_packet ( struct gdbstub *stub ) {
	switch ( stub->payload [ 0 ] ) {
		case '?':
			gdbstub_report_signal ( stub );
			break;
		case 'g':
			gdbstub_read_regs ( stub );
			break;
		case 'G':
			gdbstub_write_regs ( stub );
			break;
		case 'm':
			gdbstub_read_mem ( stub );
			break;
		case 'M':
			gdbstub_write_mem ( stub );
			break;
		case 'c': /* Continue */
		case 'k': /* Kill */
		case 's': /* Step */
		case 'D': /* Detach */
			gdbstub_continue ( stub, stub->payload [ 0 ] == 's' );
			if ( stub->payload [ 0 ] == 'D' ) {
				gdbstub_send_ok ( stub );
			}
			break;
		default:
			stub->len = 0;
			gdbstub_tx_packet ( stub );
			break;
	}
}

/* GDB packet parser */
static void gdbstub_state_new ( struct gdbstub *stub, char ch ) {
	if ( ch == '$' ) {
		stub->len = 0;
		stub->parse = gdbstub_state_data;
	}
}

static void gdbstub_state_data ( struct gdbstub *stub, char ch ) {
	if ( ch == '#' ) {
		stub->parse = gdbstub_state_cksum1;
	} else if ( ch == '$' ) {
		stub->len = 0; /* retry new packet */
	} else {
		/* If the length exceeds our buffer, let the checksum fail */
		if ( stub->len < ( int ) sizeof stub->payload ) {
			stub->payload [ stub->len++ ] = ch;
		}
	}
}

static void gdbstub_state_cksum1 ( struct gdbstub *stub, char ch ) {
	stub->cksum1 = gdbstub_from_hex_digit ( ch ) << 4;
	stub->parse = gdbstub_state_cksum2;
}

static void gdbstub_state_cksum2 ( struct gdbstub *stub, char ch ) {
	uint8_t their_cksum;
	uint8_t our_cksum;

	stub->parse = gdbstub_state_new;
	their_cksum = stub->cksum1 + gdbstub_from_hex_digit ( ch );
	our_cksum = gdbstub_cksum ( stub->payload, stub->len );
	if ( their_cksum == our_cksum ) {
		gdbstub_putchar ( stub, '+' );
		if ( stub->len > 0 ) {
			gdbstub_rx_packet ( stub );
		}
	} else {
		gdbstub_putchar ( stub, '-' );
	}
}

static void gdbstub_state_wait_ack ( struct gdbstub *stub, char ch ) {
	if ( ch == '+' ) {
		stub->parse = gdbstub_state_new;
	} else if ( ch == '-' ) {
		gdbstub_tx_packet ( stub ); /* retransmit */
	}
}

static void gdbstub_parse ( struct gdbstub *stub, char ch ) {
	stub->parse ( stub, ch );
}

static struct gdbstub stub = {
	.parse = gdbstub_state_new
};

__cdecl void gdbstub_handler ( int signo, gdbreg_t *regs ) {
	int ch;
	stub.signo = signo;
	stub.regs = regs;
	stub.exit_handler = 0;
	gdbstub_report_signal ( &stub );
	while ( ( ch = gdbstub_getchar( &stub ) ) != -1 ) {
		gdbstub_parse ( &stub, ch );
	}
}

/* Activity monitor to detect packets from GDB when we are not active */
static void gdbstub_activity_step ( struct process *process __unused ) {
	if ( serial_ischar() ) {
		gdbmach_breakpoint();
	}
}

struct process gdbstub_activity_process __permanent_process = {
	.step = gdbstub_activity_step,
};
