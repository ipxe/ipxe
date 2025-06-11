/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Devicetree bus
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ipxe/device.h>
#include <ipxe/fdt.h>
#include <ipxe/iomap.h>
#include <ipxe/devtree.h>

static struct dt_driver dt_node_driver __dt_driver;

static void dt_remove_children ( struct dt_device *parent );

/**
 * Map devicetree range
 *
 * @v dt		Devicetree device
 * @v offset		Starting node offset
 * @v index		Region index
 * @v len		Length to map, or 0 to map whole region
 * @ret io_addr		I/O address, or NULL on error
 */
void * dt_ioremap ( struct dt_device *dt, unsigned int offset,
		    unsigned int index, size_t len ) {
	struct fdt_reg_cells regs;
	unsigned int parent;
	uint64_t address;
	uint64_t size;
	void *io_addr;
	int rc;

	/* Get parent node */
	if ( ( rc = fdt_parent ( &sysfdt, offset, &parent ) ) != 0 ) {
		DBGC ( dt, "DT %s could not locate parent: %s\n",
		       dt->name, strerror ( rc ) );
		return NULL;
	}

	/* Read #address-cells and #size-cells, if present */
	fdt_reg_cells ( &sysfdt, parent, &regs );

	/* Read address */
	if ( ( rc = fdt_reg_address ( &sysfdt, offset, &regs, index,
				      &address ) ) != 0 ) {
		DBGC ( dt, "DT %s could not read region %d address: %s\n",
		       dt->name, index, strerror ( rc ) );
		return NULL;
	}

	/* Read size (or assume sufficient, if tree specifies no sizes) */
	size = len;
	if ( regs.size_cells &&
	     ( ( rc = fdt_reg_size ( &sysfdt, offset, &regs, index,
				     &size ) ) != 0 ) ) {
		DBGC ( dt, "DT %s could not read region %d size: %s\n",
		       dt->name, index, strerror ( rc ) );
		return NULL;
	}

	/* Use region size as length if not specified */
	if ( ! len )
		len = size;
	DBGC ( dt, "DT %s region %d at %#08llx+%#04llx\n",
	       dt->name, index, ( ( unsigned long long ) address ),
	       ( ( unsigned long long ) size ) );

	/* Verify size */
	if ( len > size ) {
		DBGC ( dt, "DT %s region %d is too small (%#llx/%#zx bytes)\n",
		       dt->name, index, ( ( unsigned long long ) size ), len );
		return NULL;
	}

	/* Map region */
	io_addr = ioremap ( address, len );
	if ( ! io_addr ) {
		DBGC ( dt, "DT %s could not map region %d\n",
		       dt->name, index );
		return NULL;
	}

	return io_addr;
}

/**
 * Find devicetree driver
 *
 * @v dt		Devicetree device
 * @v offset		Starting node offset
 * @ret driver		Driver
 */
static struct dt_driver * dt_find_driver ( struct dt_device *dt,
					   unsigned int offset ) {
	struct dt_driver *driver;
	const char *ids;
	const char *id;
	unsigned int count;
	unsigned int i;

	/* Read compatible programming model identifiers */
	ids = fdt_strings ( &sysfdt, offset, "compatible", &count );

	/* Look for a compatible driver */
	for ( id = ids ; count-- ; id += ( strlen ( id ) + 1 ) ) {
		DBGC2 ( &sysfdt, "DT %s is compatible with %s\n",
			dt->name, id );
		for_each_table_entry ( driver, DT_DRIVERS ) {
			for ( i = 0 ; i < driver->id_count ; i++ ) {
				if ( strcmp ( id, driver->ids[i] ) == 0 ) {
					DBGC ( dt, "DT %s has %s driver %s\n",
					       dt->name, id, driver->name );
					return driver;
				}
			}
		}
	}

	/* Use generic node driver if no other driver matches */
	return &dt_node_driver;
}

/**
 * Probe devicetree device
 *
 * @v dt		Devicetree device
 * @v offset		Starting node offset
 * @ret rc		Return status code
 */
static int dt_probe ( struct dt_device *dt, unsigned int offset ) {
	struct dt_driver *driver;
	int rc;

	/* Identify driver */
	driver = dt_find_driver ( dt, offset );
	dt->driver = driver;
	dt->dev.driver_name = driver->name;

	/* Probe device */
	if ( ( rc = driver->probe ( dt, offset ) ) != 0 ) {
		if ( driver != &dt_node_driver ) {
			DBGC ( dt, "DT %s could not probe: %s\n",
			       dt->name, strerror ( rc ) );
		}
		return rc;
	}

	return 0;
}

/**
 * Remove devicetree device
 *
 * @v dt		Devicetree device
 */
static void dt_remove ( struct dt_device *dt ) {
	struct dt_driver *driver = dt->driver;

	/* Remove device */
	driver->remove ( dt );
	if ( driver != &dt_node_driver )
		DBGC ( dt, "DT %s removed\n", dt->name );
}

/**
 * Probe devicetree node
 *
 * @v parent		Parent generic device
 * @v offset		Starting node offset
 * @ret rc		Return status code
 */
int dt_probe_node ( struct device *parent, unsigned int offset ) {
	struct fdt_descriptor desc;
	struct dt_device *dt;
	const char *name;
	int rc;

	/* Describe token */
	if ( ( rc = fdt_describe ( &sysfdt, offset, &desc ) ) != 0 )
		goto err_describe;

	/* Allocate and initialise device */
	dt = zalloc ( sizeof ( *dt ) );
	if ( ! dt ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	name = ( offset ? desc.name : "root node" );
	dt->name = dt->dev.name;
	snprintf ( dt->dev.name, sizeof ( dt->dev.name ), "%s", name );
	dt->dev.desc.bus_type = BUS_TYPE_DT;
	dt->dev.parent = parent;
	INIT_LIST_HEAD ( &dt->dev.children );
	list_add_tail ( &dt->dev.siblings, &parent->children );

	/* Probe device */
	if ( ( rc = dt_probe ( dt, offset ) ) != 0 )
		goto err_probe;

	return 0;

	dt_remove ( dt );
 err_probe:
	list_del ( &dt->dev.siblings );
	free ( dt );
 err_alloc:
 err_describe:
	return rc;
}

/**
 * Remove devicetree node
 *
 * @v parent		Parent generic device
 */
void dt_remove_node ( struct device *parent ) {
	struct dt_device *dt;

	/* Identify most recently added child */
	dt = list_last_entry ( &parent->children, struct dt_device,
			       dev.siblings );
	assert ( dt != NULL );

	/* Remove driver */
	dt_remove ( dt );

	/* Delete and free device */
	list_del ( &dt->dev.siblings );
	free ( dt );
}

/**
 * Probe devicetree children
 *
 * @v parent		Parent device
 * @v offset		Starting node offset
 * @ret rc		Return status code
 */
static int dt_probe_children ( struct dt_device *parent,
			       unsigned int offset ) {
	struct fdt_descriptor desc;
	int depth;
	int rc;

	/* Probe any immediate child nodes */
	for ( depth = -1 ; ; depth += desc.depth, offset = desc.next ) {

		/* Describe token */
		if ( ( rc = fdt_describe ( &sysfdt, offset, &desc ) ) != 0 ) {
			DBGC ( &sysfdt, "DT %s has malformed node: %s\n",
			       parent->name, strerror ( rc ) );
			goto err_describe;
		}

		/* Terminate when we exit this node */
		if ( ( depth == 0 ) && ( desc.depth < 0 ) )
			break;

		/* Probe child node, if applicable */
		if ( ( depth == 0 ) && desc.name && ( ! desc.data ) ) {
			DBGC2 ( &sysfdt, "DT %s is child of %s\n",
				desc.name, parent->name );
			dt_probe_node ( &parent->dev, desc.offset );
		}
	}

	/* Fail if we have no children (so that this device will be freed) */
	if ( list_empty ( &parent->dev.children ) ) {
		rc = -ENODEV;
		goto err_no_children;
	}

	return 0;

 err_no_children:
 err_describe:
	dt_remove_children ( parent );
	return rc;
}

/**
 * Remove devicetree children
 *
 * @v parent		Parent device
 */
static void dt_remove_children ( struct dt_device *parent ) {

	/* Remove all child nodes */
	while ( ! list_empty ( &parent->dev.children ) )
		dt_remove_node ( &parent->dev );
}

/** Generic node driver */
static struct dt_driver dt_node_driver __dt_driver = {
	.name = "node",
	.probe = dt_probe_children,
	.remove = dt_remove_children,
};

/**
 * Probe devicetree bus
 *
 * @v rootdev		Devicetree root device
 * @ret rc		Return status code
 */
static int dt_probe_all ( struct root_device *rootdev ) {

	/* Probe root node */
	return dt_probe_node ( &rootdev->dev, 0 );
}

/**
 * Remove devicetree bus
 *
 * @v rootdev		Devicetree root device
 */
static void dt_remove_all ( struct root_device *rootdev ) {

	/* Remove root node */
	dt_remove_node ( &rootdev->dev );
}

/** Devicetree bus root device driver */
static struct root_driver dt_root_driver = {
	.probe = dt_probe_all,
	.remove = dt_remove_all,
};

/** Devicetree bus root device */
struct root_device dt_root_device __root_device = {
	.dev = { .name = "DT" },
	.driver = &dt_root_driver,
};
