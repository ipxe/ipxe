/*
 * Copyright (C)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Xen console
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <ipxe/io.h>
#include <ipxe/console.h>
#include <ipxe/init.h>
#include <ipxe/xen.h>
#include <ipxe/xenevent.h>
#include <config/console.h>
#include <interface/xen/xencon.h>

/* Set default console usage if applicable */
#if ! ( defined ( CONSOLE_XENCON ) && CONSOLE_EXPLICIT ( CONSOLE_XENCON ) )
#undef CONSOLE_XENCON
#define CONSOLE_XENCON ( CONSOLE_USAGE_ALL & ~CONSOLE_USAGE_DEBUG)
#endif

static struct xen_hypervisor *g_xen = NULL;

static inline void notify_daemon(struct xencons_info *cons)
{
	struct evtchn_send event = { .port = cons->port };
	xenevent_send(g_xen, &event);
}


/**
 * Print a character to debug port console
 *
 * @v character		Character to be printed
 */
static void xencon_putchar ( int character ) {

	XENCONS_RING_IDX cons, prod;
	volatile struct xencons_interface *intf = g_xen->console.intf;


	prod = intf->out_prod;
	do
	{
		cons = intf->out_cons;
	}while(MASK_XENCONS_IDX(prod-cons+1, intf->out) == 0);

	intf->out[MASK_XENCONS_IDX(prod++, intf->out)] = character;

	wmb();			/* write ring before updating pointer */
	intf->out_prod = prod;

	notify_daemon(&g_xen->console);
}

/**
 * Get character from console
 *
 * @ret character	Character read from console
 */
static int xencon_getchar ( void ) {
	XENCONS_RING_IDX cons, prod;
	volatile struct xencons_interface *intf = g_xen->console.intf;
	uint8_t data;

	cons = intf->in_cons;

	/* Wait for data to be ready */
	do {
		prod = intf->in_prod;
	} while(cons == prod);

	/* Receive data */
	data = intf->in[MASK_XENCONS_IDX(cons++, intf->in)];

	intf->in_cons = cons;

	/* convert DEL to backspace */
	if ( data == 0x7f )
		data = 0x08;

	return data;
}

/**
 * Check for character ready to read from console
 *
 * @ret True		Character available to read
 * @ret False		No character available to read
 */
static int xencon_iskey ( void ) {
	volatile struct xencons_interface *intf = g_xen->console.intf;
	return intf->in_cons != intf->in_prod;
}

/** Xen port console driver */
struct console_driver xencon_console __console_driver = {
	.putchar = xencon_putchar,
	.getchar = xencon_getchar,
	.iskey = xencon_iskey,
	.usage = CONSOLE_XENCON,
	.disabled = CONSOLE_DISABLED,
};

/*
 * Initialize xen console
 */
void xencon_late_init(struct xen_hypervisor *xen)
{
	g_xen = xen;
	xencon_console.disabled = 0;
}

/*
 * Shutdown xen console
 */
void xencon_uninit(void)
{
	xencon_console.disabled = CONSOLE_DISABLED;
	g_xen = NULL;
}


