/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdlib.h>
#include <errno.h>
#include <ipxe/gpio.h>

/** @file
 *
 * General purpose I/O
 *
 */

/** List of GPIO controllers */
static LIST_HEAD ( all_gpios );

/**
 * Allocate GPIO controller
 *
 * @v count		Number of GPIO pins
 * @v priv_len		Size of driver-private data
 * @ret gpios		GPIO controller, or NULL
 */
struct gpios * alloc_gpios ( unsigned int count, size_t priv_len ) {
	struct gpios *gpios;
	struct gpio *gpio;
	size_t len;
	unsigned int i;

	/* Allocate and initialise structure */
	len = ( sizeof ( *gpios ) + ( count * sizeof ( *gpio ) ) + priv_len );
	gpios = zalloc ( len );
	if ( ! gpios )
		return NULL;
	gpios->count = count;
	gpios->gpio = ( ( ( void * ) gpios ) + sizeof ( *gpios ) );
	gpios->priv = ( ( ( void * ) gpios ) + sizeof ( *gpios ) +
			( count * sizeof ( *gpio ) ) );

	/* Initialise GPIO pins */
	for ( i = 0 ; i < count ; i++ ) {
		gpio = &gpios->gpio[i];
		gpio->gpios = gpios;
		gpio->index = i;
	}

	return gpios;
}

/**
 * Register GPIO controller
 *
 * @v gpios		GPIO controller
 * @ret rc		Return status code
 */
int gpios_register ( struct gpios *gpios ) {

	/* Add to list of GPIO controllers */
	gpios_get ( gpios );
	list_add_tail ( &gpios->list, &all_gpios );
	DBGC ( gpios, "GPIO %s registered with %d GPIOs\n",
	       gpios->dev->name, gpios->count );

	return 0;
}

/**
 * Unregister GPIO controller
 *
 * @v gpios		GPIO controller
 */
void gpios_unregister ( struct gpios *gpios ) {

	/* Remove from list of GPIO controllers */
	DBGC ( gpios, "GPIO %s unregistered\n", gpios->dev->name );
	list_del ( &gpios->list );
	gpios_put ( gpios );
}

/**
 * Find GPIO controller
 *
 * @v bus_type		Bus type
 * @v location		Bus location
 * @ret gpios		GPIO controller, or NULL
 */
struct gpios * gpios_find ( unsigned int bus_type, unsigned int location ) {
	struct gpios *gpios;

	/* Scan through list of registered GPIO controllers */
	list_for_each_entry ( gpios, &all_gpios, list ) {
		if ( ( gpios->dev->desc.bus_type == bus_type ) &&
		     ( gpios->dev->desc.location == location ) )
			return gpios;
	}

	return NULL;
}

/**
 * Get null GPIO input value
 *
 * @v gpios		GPIO controller
 * @v gpio		GPIO pin
 * @ret active		Pin is in the active state
 */
static int null_gpio_in ( struct gpios *gpios __unused,
			  struct gpio *gpio __unused ) {
	return 0;
}

/**
 * Set null GPIO output value
 *
 * @v gpios		GPIO controller
 * @v gpio		GPIO pin
 * @v active		Set pin to active state
 */
static void null_gpio_out ( struct gpios *gpios __unused,
			    struct gpio *gpio __unused, int active __unused ) {
	/* Nothing to do */
}

/**
 * Configure null GPIO pin
 *
 * @v gpios		GPIO controller
 * @v gpio		GPIO pin
 * @v config		Configuration
 * @ret rc		Return status code
 */
static int null_gpio_config ( struct gpios *gpios __unused,
			      struct gpio *gpio __unused,
			      unsigned int config __unused ) {
	return -ENODEV;
}

/** Null GPIO operations */
struct gpio_operations null_gpio_operations = {
	.in = null_gpio_in,
	.out = null_gpio_out,
	.config = null_gpio_config,
};
