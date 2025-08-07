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
#include <string.h>
#include <errno.h>
#include <ipxe/devtree.h>
#include <ipxe/fdt.h>
#include <ipxe/gpio.h>
#include "dwgpio.h"

/** @file
 *
 * Synopsys DesignWare GPIO driver
 *
 */

/******************************************************************************
 *
 * GPIO port group
 *
 ******************************************************************************
 */

/**
 * Probe port group
 *
 * @v dt		Devicetree device
 * @v offset		Starting node offset
 * @ret rc		Return status code
 */
static int dwgpio_group_probe ( struct dt_device *dt, unsigned int offset ) {
	struct dwgpio_group *group;
	int rc;

	/* Allocate and initialise structure */
	group = zalloc ( sizeof ( *group ) );
	if ( ! group ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	dt_set_drvdata ( dt, group );

	/* Map registers */
	group->regs = dt_ioremap ( dt, offset, 0, 0 );
	if ( ! group->regs ) {
		rc = -ENODEV;
		goto err_ioremap;
	}

	/* Probe child ports */
	if ( ( rc = dt_probe_children ( dt, offset ) ) != 0 )
		goto err_children;

	return 0;

	dt_remove_children ( dt );
 err_children:
	iounmap ( group->regs );
 err_ioremap:
	free ( group );
 err_alloc:
	return rc;
}

/**
 * Remove port group
 *
 * @v dt		Devicetree device
 */
static void dwgpio_group_remove ( struct dt_device *dt ) {
	struct dwgpio_group *group = dt_get_drvdata ( dt );

	/* Remove child ports */
	dt_remove_children ( dt );

	/* Unmap registers */
	iounmap ( group->regs );

	/* Free device */
	free ( group );
}

/** DesignWare GPIO port group compatible model identifiers */
static const char * dwgpio_group_ids[] = {
	"snps,dw-apb-gpio",
};

/** DesignWare GPIO port group devicetree driver */
struct dt_driver dwgpio_group_driver __dt_driver = {
	.name = "dwgpio-group",
	.ids = dwgpio_group_ids,
	.id_count = ( sizeof ( dwgpio_group_ids ) /
		      sizeof ( dwgpio_group_ids[0] ) ),
	.probe = dwgpio_group_probe,
	.remove = dwgpio_group_remove,
};

/******************************************************************************
 *
 * GPIO port
 *
 ******************************************************************************
 */

/**
 * Dump GPIO port status
 *
 * @v dwgpio		DesignWare GPIO port
 */
static inline void dwgpio_dump ( struct dwgpio *dwgpio ) {

	DBGC2 ( dwgpio, "DWGPIO %s dr %#08x ddr %#08x ctl %#08x\n",
		dwgpio->name, readl ( dwgpio->swport + DWGPIO_SWPORT_DR ),
		readl ( dwgpio->swport + DWGPIO_SWPORT_DDR ),
		readl ( dwgpio->swport + DWGPIO_SWPORT_CTL ) );
}

/**
 * Get current GPIO input value
 *
 * @v gpios		GPIO controller
 * @v gpio		GPIO pin
 * @ret active		Pin is in the active state
 */
static int dwgpio_in ( struct gpios *gpios, struct gpio *gpio ) {
	struct dwgpio *dwgpio = gpios->priv;
	uint32_t ext;

	/* Read external port status */
	ext = readl ( dwgpio->ext );
	return ( ( ( ext >> gpio->index ) ^ gpio->config ) & 1 );
}

/**
 * Set current GPIO output value
 *
 * @v gpios		GPIO controller
 * @v gpio		GPIO pin
 * @v active		Set pin to active state
 */
static void dwgpio_out ( struct gpios *gpios, struct gpio *gpio, int active ) {
	struct dwgpio *dwgpio = gpios->priv;
	uint32_t mask = ( 1UL << gpio->index );
	uint32_t dr;

	/* Update data register */
	dr = readl ( dwgpio->swport + DWGPIO_SWPORT_DR );
	dr &= ~mask;
	if ( ( ( !! active ) ^ gpio->config ) & 1 )
		dr |= mask;
	writel ( dr, ( dwgpio->swport + DWGPIO_SWPORT_DR ) );
	dwgpio_dump ( dwgpio );
}

/**
 * Configure GPIO pin
 *
 * @v gpios		GPIO controller
 * @v gpio		GPIO pin
 * @v config		Configuration
 * @ret rc		Return status code
 */
static int dwgpio_config ( struct gpios *gpios, struct gpio *gpio,
			   unsigned int config ) {
	struct dwgpio *dwgpio = gpios->priv;
	uint32_t mask = ( 1UL << gpio->index );
	uint32_t ddr;
	uint32_t ctl;

	/* Update data direction and control registers */
	ddr = readl ( dwgpio->swport + DWGPIO_SWPORT_DDR );
	ctl = readl ( dwgpio->swport + DWGPIO_SWPORT_CTL );
	ctl &= ~mask;
	ddr &= ~mask;
	if ( config & GPIO_CFG_OUTPUT )
		ddr |= mask;
	writel ( ctl, ( dwgpio->swport + DWGPIO_SWPORT_CTL ) );
	writel ( ddr, ( dwgpio->swport + DWGPIO_SWPORT_DDR ) );
	dwgpio_dump ( dwgpio );

	return 0;
}

/** GPIO operations */
static struct gpio_operations dwgpio_operations = {
	.in = dwgpio_in,
	.out = dwgpio_out,
	.config = dwgpio_config,
};

/**
 * Probe port
 *
 * @v dt		Devicetree device
 * @v offset		Starting node offset
 * @ret rc		Return status code
 */
static int dwgpio_probe ( struct dt_device *dt, unsigned int offset ) {
	struct dt_device *parent;
	struct dwgpio_group *group;
	struct dwgpio *dwgpio;
	struct gpios *gpios;
	uint32_t count;
	uint64_t port;
	int rc;

	/* Get number of GPIOs */
	if ( ( rc = fdt_u32 ( &sysfdt, offset, "nr-gpios-snps",
			      &count ) ) != 0 ) {
		goto err_count;
	}
	assert ( count <= DWGPIO_MAX_COUNT );

	/* Allocate and initialise device */
	gpios = alloc_gpios ( count, sizeof ( *dwgpio ) );
	if ( ! gpios ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	dt_set_drvdata ( dt, gpios );
	gpios->dev = &dt->dev;
	gpios_init ( gpios, &dwgpio_operations );
	dwgpio = gpios->priv;
	dwgpio->name = dt->name;

	/* Identify group */
	parent = dt_parent ( dt );
	if ( parent->driver != &dwgpio_group_driver ) {
		DBGC ( dwgpio, "DWGPIO %s has invalid parent %s\n",
		       dwgpio->name, parent->name );
		rc = -EINVAL;
		goto err_parent;
	}
	group = dt_get_drvdata ( parent );

	/* Identify port */
	if ( ( rc = fdt_reg ( &sysfdt, offset, &port ) ) != 0 ) {
		DBGC ( dwgpio, "DWGPIO %s could not get port number: %s\n",
		       dwgpio->name, strerror ( rc ) );
		goto err_port;
	}
	dwgpio->port = port;
	DBGC ( dwgpio, "DWGPIO %s is %s port %d (%d GPIOs)\n",
	       dwgpio->name, parent->name, dwgpio->port, gpios->count );

	/* Map registers */
	dwgpio->swport = ( group->regs + DWGPIO_SWPORT ( port ) );
	dwgpio->ext = ( group->regs + DWGPIO_EXT_PORT ( port ) );
	dwgpio_dump ( dwgpio );

	/* Record original register values */
	dwgpio->dr = readl ( dwgpio->swport + DWGPIO_SWPORT_DR );
	dwgpio->ddr = readl ( dwgpio->swport + DWGPIO_SWPORT_DDR );
	dwgpio->ctl = readl ( dwgpio->swport + DWGPIO_SWPORT_CTL );

	/* Register GPIO controller */
	if ( ( rc = gpios_register ( gpios ) ) != 0 ) {
		DBGC ( dwgpio, "DWGPIO %s could not register: %s\n",
		       dwgpio->name, strerror ( rc ) );
		goto err_register;
	}

	return 0;

	gpios_unregister ( gpios );
 err_register:
 err_port:
 err_parent:
	gpios_nullify ( gpios );
	gpios_put ( gpios );
 err_alloc:
 err_count:
	return rc;
}

/**
 * Remove port
 *
 * @v dt		Devicetree device
 */
static void dwgpio_remove ( struct dt_device *dt ) {
	struct gpios *gpios = dt_get_drvdata ( dt );
	struct dwgpio *dwgpio = gpios->priv;

	/* Unregister GPIO controller */
	gpios_unregister ( gpios );

	/* Restore original register values */
	writel ( dwgpio->ctl, ( dwgpio->swport + DWGPIO_SWPORT_CTL ) );
	writel ( dwgpio->ddr, ( dwgpio->swport + DWGPIO_SWPORT_DDR ) );
	writel ( dwgpio->dr, ( dwgpio->swport + DWGPIO_SWPORT_DR ) );

	/* Free GPIO device */
	gpios_nullify ( gpios );
	gpios_put ( gpios );
}

/** DesignWare GPIO port compatible model identifiers */
static const char * dwgpio_ids[] = {
	"snps,dw-apb-gpio-port",
};

/** DesignWare GPIO port devicetree driver */
struct dt_driver dwgpio_driver __dt_driver = {
	.name = "dwgpio",
	.ids = dwgpio_ids,
	.id_count = ( sizeof ( dwgpio_ids ) / sizeof ( dwgpio_ids[0] ) ),
	.probe = dwgpio_probe,
	.remove = dwgpio_remove,
};
