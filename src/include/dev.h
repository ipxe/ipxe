#ifndef DEV_H
#define DEV_H

#include "stdint.h"
#include "string.h"
#include <gpxe/buffer.h>
#include "dhcp.h" /* for dhcp_dev_id */
#include <gpxe/tables.h>
#include <assert.h>

/*
 * Forward declarations
 *
 */
struct type_dev;
struct type_driver;
struct bus_driver;
struct bus_dev;
struct device_driver;

/*
 * When looking at the following data structures, mentally substitute
 * "<bus>_" in place of "bus_" and everything will become clear.
 * "struct bus_location" becomes "struct <bus>_location", which means
 * "the location of a device on a <bus> bus", where <bus> is a
 * particular type of bus such as "pci" or "isapnp".
 *
 */

/*
 * A physical device location on a bus.
 *
 */
#define BUS_LOC_SIZE 8
struct bus_loc {
	char bytes[BUS_LOC_SIZE];
};

/* 
 * A structure fully describing a physical device on a bus.
 *
 */
#define BUS_DEV_SIZE 32
struct bus_dev {
	char bytes[BUS_DEV_SIZE];
};

/*
 * Individual buses will have different sizes for their <bus>_location
 * and <bus>_device structures.  We need to be able to allocate static
 * storage that's large enough to contain these structures for any
 * bus type that's being used in the current binary.
 *
 * We can't just create a union of all the various types, because some
 * may be architecture-dependent (and some are even embedded in
 * specific drivers, e.g. 3c509), so this would quickly get messy.
 *
 * We could use the magic of common symbols.  Each bus could declare a
 * common symbol with the name "_bus_dev" of the correct size; this
 * is easily done using code like
 *	struct pci_device _bus_dev;
 * The linker would then use the largest size of the "_bus_dev" symbol
 * in any included object, thus giving us a single _bus_dev symbol of
 * *exactly* the required size.  However, there's no way to extract
 * the size of this symbol, either directly as a linker symbol
 * ("_bus_dev_size = SIZEOF(_bus_dev)"; the linker language just
 * doesn't provide this construct) or via any linker trickery I can
 * think of (such as creating a special common symbol section just for
 * this symbol then using SIZE(section) to read the size of the
 * section; ld recognises only a single common symbol section called
 * "COMMON").
 *
 * Since there's no way to get the size of the symbol, this
 * effectively limits us to just one instance of the symbol.  This is
 * all very well for the simple case of "just boot from any single
 * device you can", but becomes limiting when you want to do things
 * like introducing PCMCIA buses (which must instantiate other devices
 * such as PCMCIA controllers).
 *
 * So, we declare the maximum sizes of these constructions to be
 * compile-time constants.  Each individual bus driver should define
 * its own struct <bus>_location and struct <bus>_device however it
 * likes, and can freely cast pointers from struct bus_loc to
 * struct <bus>_location (and similarly for bus_dev).  To guard
 * against bounding errors, each bus driver *MUST* use the macros
 * BUS_LOC_CHECK() and BUS_DEV_CHECK(), as in:
 *
 *   BUS_LOC_CHECK ( struct pci_location );
 *   BUS_DEV_CHECK ( struct pci_device );
 *
 * These macros will generate a link-time error if the size of the
 * <bus> structure exceeds the declared maximum size.
 *
 * The macros will generate no binary object code, but must be placed
 * inside a function (in order to generate syntactically valid C).
 * The easiest wy to do this is to place them in the
 * <bus>_next_location() function.
 *
 * If anyone can think of a better way of doing this that avoids *ALL*
 * of the problems described above, please implement it!
 *
 */

#define BUS_LOC_CHECK(datatype)					      \
	linker_assert( ( sizeof (datatype) <= sizeof (struct bus_loc) ),  \
		       __BUS_LOC_SIZE_is_too_small__see_dev_h )
#define BUS_DEV_CHECK(datatype)					      \
	linker_assert( ( sizeof (datatype) <= sizeof (struct bus_dev) ),    \
		       __BUS_DEV_SIZE_is_too_small__see_dev_h )

/*
 * Bus-level operations.
 *
 * int next_location ( struct bus_loc * bus_loc )
 *
 *	Increment bus_loc to point to the next possible device on
 *	the bus (e.g. the next PCI busdevfn, or the next ISAPnP CSN).
 *	If there are no more valid locations, return 0 and leave
 *	struct bus_loc zeroed, otherwise return true.
 *
 * int fill_device ( struct bus_dev *bus_dev,
 *		     struct bus_loc *bus_loc )
 *
 *	Fill out a bus_dev structure with the parameters for the
 *	device at bus_loc.  (For example, fill in the PCI vendor
 *	and device IDs).  Return true if there is a device physically
 *	present at this location, otherwise 0.
 *
 * int check_driver ( struct bus_dev *bus_dev,
 *		      struct device_driver *device_driver )
 *
 *	Test whether or not the specified driver is capable of driving
 *	the specified device by, for example, comparing the device's
 *	PCI IDs against the list of PCI IDs claimed by the driver.
 *
 * char * describe ( struct bus_dev *bus_dev )
 *
 *	Return a text string describing the bus device bus_dev
 *	(e.g. "PCI 00:01.2")
 *
 * char * name ( struct bus_dev *bus_dev )
 *
 *	Return a text string describing the bus device bus_dev
 *	(e.g. "dfe538")
 *
 */
struct bus_driver {
	const char *name;
	int ( *next_location ) ( struct bus_loc *bus_loc );
	int ( *fill_device ) ( struct bus_dev *bus_dev,
			       struct bus_loc *bus_loc );
	int ( *check_driver ) ( struct bus_dev *bus_dev,
				struct device_driver *device_driver );
	char * ( *describe_device ) ( struct bus_dev *bus_dev );
	const char * ( *name_device ) ( struct bus_dev *bus_dev );
};

#define __bus_driver __table ( struct bus_driver, bus_driver, 01 )

/*
 * A structure fully describing the bus-independent parts of a
 * particular type (e.g. nic or disk) of device.
 *
 * Unlike struct bus_dev, e can limit ourselves to having no more than
 * one instance of this data structure.  We therefore place an
 * instance in each type driver file (e.g. nic.c), and simply use a
 * pointer to the struct type_dev in the struct dev.
 *
 */
struct type_dev;

/*
 * A type driver (e.g. nic, disk)
 *
 */
struct type_driver {
	char *name;
	struct type_dev *type_dev; /* single instance */
	char * ( * describe_device ) ( struct type_dev *type_dev );
	int ( * configure ) ( struct type_dev *type_dev );
	int ( * load ) ( struct type_dev *type_dev, struct buffer *buffer );
};

#define __type_driver __table ( struct type_driver, type_driver, 01 )

/*
 * A driver for a device.
 *
 */
struct device_driver {
	const char *name;
	struct type_driver *type_driver;
	struct bus_driver *bus_driver;
	struct bus_driver_info *bus_driver_info;
	int ( * probe ) ( struct type_dev *type_dev,
			  struct bus_dev *bus_dev );
	void ( * disable ) ( struct type_dev *type_dev,
			     struct bus_dev *bus_dev );
};

#define __device_driver __table ( struct device_driver, device_driver, 01 )

#if 0
#define DRIVER(_name,_type_driver,_bus_driver,_bus_info,	 	      \
	       _probe,_disable) 		 			      \
	struct device_driver device_ ## _bus_info __device_driver = {  \
		.name = _name,						      \
		.type_driver = &_type_driver,				      \
		.bus_driver = &_bus_driver,				      \
		.bus_driver_info = ( struct bus_driver_info * ) &_bus_info,   \
		.probe = ( int (*) () ) _probe,				      \
		.disable = ( void (*) () ) _disable,			      \
	};
#endif

#define DRIVER(a,b,c,d,e,f)

/*
 * A bootable device, comprising a physical device on a bus, a driver
 * for that device, and a type device
 *
 */
struct dev {
	struct bus_driver	*bus_driver;
	struct bus_loc		bus_loc;
	struct bus_dev		bus_dev;
	struct device_driver	*device_driver;
	struct type_driver	*type_driver;
	struct type_dev		*type_dev;
};

/* The current boot device */
extern struct dev dev;

/*
 * Functions in dev.c 
 *
 */
extern void print_drivers ( void );
extern int find_any ( struct bus_driver **bus_driver, struct bus_loc *bus_loc,
		      struct bus_dev *bus_dev, signed int skip );
extern int find_by_device ( struct device_driver **device_driver,
			    struct bus_driver *bus_driver,
			    struct bus_dev *bus_dev,
			    signed int skip );
extern int find_by_driver ( struct bus_loc *bus_loc, struct bus_dev *bus_dev,
			    struct device_driver *device_driver,
			    signed int skip );
extern int find_any_with_driver ( struct dev *dev, signed int skip );

/*
 * Functions inlined to save space
 *
 */

/* Probe a device */
static inline int probe ( struct dev *dev ) {
	return dev->device_driver->probe ( dev->type_dev, &dev->bus_dev );
}
/* Disable a device */
static inline void disable ( struct dev *dev ) {
	dev->device_driver->disable ( dev->type_dev, &dev->bus_dev );
}
/* Set the default boot device */
static inline void select_device ( struct dev *dev,
				   struct bus_driver *bus_driver,
				   struct bus_loc *bus_loc ) {
	dev->bus_driver = bus_driver;
	memcpy ( &dev->bus_loc, bus_loc, sizeof ( dev->bus_loc ) );
}
/* Configure a device */
static inline int configure ( struct dev *dev ) {
	return dev->type_driver->configure ( dev->type_dev );
}
/* Boot from a device */
static inline int load ( struct dev *dev, struct buffer *buffer ) {
	return dev->type_driver->load ( dev->type_dev, buffer );
}

#endif /* DEV_H */
