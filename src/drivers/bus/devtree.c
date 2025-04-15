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
#include <errno.h>
#include <ipxe/device.h>
#include <ipxe/fdt.h>
#include <ipxe/iomap.h>
#include <ipxe/devtree.h>

static struct dt_driver dt_node_driver __dt_driver;
struct root_device dt_root_device __root_device;

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
	struct dt_device *parent =
		container_of ( dt->dev.parent, struct dt_device, dev );
	uint64_t address;
	uint64_t size;
	unsigned int cell;
	void *io_addr;
	int rc;

	/* Read address */
	cell = ( index * ( parent->address_cells + parent->size_cells ) );
	if ( ( rc = fdt_cells ( &sysfdt, offset, "reg", cell,
				parent->address_cells, &address ) ) != 0 ) {
		DBGC ( dt, "DT %s could not read region %d address: %s\n",
		       dt->path, index, strerror ( rc ) );
		return NULL;
	}
	cell += parent->address_cells;

	/* Read size (or assume sufficient, if tree specifies no sizes) */
	size = len;
	if ( parent->size_cells &&
	     ( rc = fdt_cells ( &sysfdt, offset, "reg", cell,
				parent->size_cells, &size ) ) != 0 ) {
		DBGC ( dt, "DT %s could not read region %d size: %s\n",
		       dt->path, index, strerror ( rc ) );
		return NULL;
	}

	/* Use region size as length if not specified */
	if ( ! len )
		len = size;
	DBGC ( dt, "DT %s region %d at %#08llx+%#04llx\n",
	       dt->path, index, ( ( unsigned long long ) address ),
	       ( ( unsigned long long ) size ) );

	/* Verify size */
	if ( len > size ) {
		DBGC ( dt, "DT %s region %d is too small (%#llx/%#zx bytes)\n",
		       dt->path, index, ( ( unsigned long long ) size ), len );
		return NULL;
	}

	/* Map region */
	io_addr = ioremap ( address, len );
	if ( ! io_addr ) {
		DBGC ( dt, "DT %s could not map region %d\n",
		       dt->path, index );
		return NULL;
	}

	return io_addr;
}

/**
 * Find devicetree driver
 *
 * @v dt		Devicetree device
 * @v offset		Starting node offset
 * @ret driver		Driver, or NULL
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
			dt->path, id );
		for_each_table_entry ( driver, DT_DRIVERS ) {
			for ( i = 0 ; i < driver->id_count ; i++ ) {
				if ( strcmp ( id, driver->ids[i] ) == 0 ) {
					DBGC ( dt, "DT %s has %s driver %s\n",
					       dt->path, id, driver->name );
					return driver;
				}
			}
		}
	}

	return NULL;
}

/**
 * Probe devicetree device
 *
 * @v dt		Devicetree device
 * @v offset		Starting node offset
 * @ret rc		Return status code
 */
static int dt_probe ( struct dt_device *dt, unsigned int offset ) {
	struct dt_driver *driver = NULL;
	int rc;

	/* Identify driver.  Use the generic node driver if no other
	 * driver matches, or if this is the root device (which has no
	 * valid devicetree parent).
	 */
	if ( offset > 0 )
		driver = dt_find_driver ( dt, offset );
	if ( ! driver )
		driver = &dt_node_driver;

	/* Record driver */
	dt->driver = driver;
	dt->dev.driver_name = driver->name;

	/* Probe device */
	if ( ( rc = driver->probe ( dt, offset ) ) != 0 ) {
		if ( driver != &dt_node_driver ) {
			DBGC ( dt, "DT %s could not probe: %s\n",
			       dt->path, strerror ( rc ) );
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

	/* Remove device */
	dt->driver->remove ( dt );
}

/**
 * Probe devicetree node
 *
 * @v parent		Parent device, or NULL for root of tree
 * @v offset		Starting node offset
 * @v name		Node name
 * @ret rc		Return status code
 */
static int dt_probe_node ( struct dt_device *parent, unsigned int offset,
			   const char *name ) {
	struct dt_device *dt;
	const char *ppath;
	size_t path_len;
	void *path;
	int rc;

	/* Allocate and initialise device */
	ppath = ( ( parent && parent->path[1] ) ? parent->path : "" );
	path_len = ( strlen ( ppath ) + 1 /* "/" */ +
		     strlen ( name ) + 1 /* NUL */ );
	dt = zalloc ( sizeof ( *dt ) + path_len );
	if ( ! dt ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	path = ( ( ( void * ) dt ) + sizeof ( *dt ) );
	sprintf ( path, "%s/%s", ppath, name );
	dt->path = path;
	snprintf ( dt->dev.name, sizeof ( dt->dev.name ), "%s", name );
	dt->dev.desc.bus_type = BUS_TYPE_DT;
	dt->dev.parent = ( parent ? &parent->dev : &dt_root_device.dev );
	INIT_LIST_HEAD ( &dt->dev.children );
	list_add_tail ( &dt->dev.siblings, &dt->dev.parent->children );

	/* Read #address-cells and #size-cells, if present */
	if ( ( rc = fdt_u32 ( &sysfdt, offset, "#address-cells",
			      &dt->address_cells ) ) != 0 ) {
		dt->address_cells = DT_DEFAULT_ADDRESS_CELLS;
	}
	if ( ( rc = fdt_u32 ( &sysfdt, offset, "#size-cells",
			      &dt->size_cells ) ) != 0 ) {
		dt->size_cells = DT_DEFAULT_SIZE_CELLS;
	}

	/* Probe device */
	if ( ( rc = dt_probe ( dt, offset ) ) != 0 )
		goto err_probe;

	return 0;

	dt_remove ( dt );
 err_probe:
	list_del ( &dt->dev.siblings );
	free ( dt );
 err_alloc:
	return rc;
}

/**
 * Remove devicetree node
 *
 * @v dt		Devicetree device
 */
static void dt_remove_node ( struct dt_device *dt ) {

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
			       parent->path, strerror ( rc ) );
			goto err_describe;
		}

		/* Terminate when we exit this node */
		if ( ( depth == 0 ) && ( desc.depth < 0 ) )
			break;

		/* Probe child node, if applicable */
		if ( ( depth == 0 ) && desc.name && ( ! desc.data ) )
			dt_probe_node ( parent, desc.offset, desc.name );
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
	struct dt_device *dt;
	struct dt_device *tmp;

	/* Remove all child nodes */
	list_for_each_entry_safe ( dt, tmp, &parent->dev.children,
				   dev.siblings ) {
		dt_remove_node ( dt );
	}
	assert ( list_empty ( &parent->dev.children ) );
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
static int dt_probe_all ( struct root_device *rootdev __unused ) {

	/* Probe root node */
	return dt_probe_node ( NULL, 0, "" );
}

/**
 * Remove devicetree bus
 *
 * @v rootdev		Devicetree root device
 */
static void dt_remove_all ( struct root_device *rootdev ) {
	struct dt_device *dt;

	/* Remove root node */
	dt = list_first_entry ( &rootdev->dev.children, struct dt_device,
				dev.siblings );
	assert ( dt != NULL );
	dt_remove_node ( dt );
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
