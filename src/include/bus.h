#ifndef BUS_H
#define BUS_H

#include "stdint.h"

/*
 * When looking at the following data structures, mentally substitute
 * "<bus>_" in place of "bus_" and everything will become clear.
 * "struct bus_location" becomes "struct <bus>_location", which means
 * "the location of a device on a <bus> bus", where <bus> is a
 * particular type of bus such as "pci" or "isapnp".
 *
 */

/*
 * A physical device location.
 *
 */
#define BUS_LOCATION_SIZE 4
struct bus_location {
	char bytes[BUS_LOCATION_SIZE];
};

/* 
 * A structure fully describing a physical device.
 *
 */
#define BUS_DEVICE_SIZE 32
struct bus_device {
	char bytes[BUS_DEVICE_SIZE];
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
 * common symbol with the name "_bus_device" of the correct size; this
 * is easily done using code like
 *	struct pci_device _bus_device;
 * The linker would then use the largest size of the "_bus_device"
 * symbol in any included object, thus giving us a single _bus_device
 * symbol of *exactly* the required size.  However, there's no way to
 * extract the size of this symbol, either directly as a linker symbol
 * ("_bus_device_size = SIZEOF(_bus_device)"; the linker language just
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
 * likes, and can freely cast pointers from struct bus_location to
 * struct <bus>_location (and similarly for bus_device).  To guard
 * against bounding errors, each bus driver *MUST* use the macros
 * BUS_LOCATION_CHECK() and BUS_DEVICE_CHECK(), as in:
 *
 *   BUS_LOCATION_CHECK ( struct pci_location );
 *   BUS_DEVICE_CHECK ( struct pci_device );
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

#define LINKER_ASSERT(test,error_symbol)		\
	if ( ! (test) ) {				\
		extern void error_symbol ( void );	\
		error_symbol();				\
	}

#define BUS_LOCATION_CHECK(datatype) \
	LINKER_ASSERT( ( sizeof (datatype) < sizeof (struct bus_location) ),
		       __BUS_LOCATION_SIZE_is_too_small__see_dev_h )
#define BUS_DEVICE_CHECK(datatype) \
	LINKER_ASSERT( ( sizeof (datatype) < sizeof (struct bus_device) ),
		       __BUS_DEVICE_SIZE_is_too_small__see_dev_h )

/*
 * A description of a device.  This is used to send information about
 * the device to a DHCP server, and to provide a text string to
 * describe the device to the user.
 *
 * Note that "text" is allowed to be NULL, in which case the
 * describe_device() method will print the information directly to the
 * console rather than writing it into a buffer.  (This happens
 * transparently because sprintf(NULL,...) is exactly equivalent to
 * printf(...) in our vsprintf.c).
 *
 */
struct bus_description {
	char *text;
	uint16_t	vendor_id;
	uint16_t	device_id;
	uint8_t		bus_type;
};

/*
 * A driver definition
 *
 */
struct bus_driver;

/*
 * Bus-level operations.
 *
 * int next_location ( struct bus_location * bus_location )
 *
 *	Increment bus_location to point to the next possible device on
 *	the bus (e.g. the next PCI busdevfn, or the next ISAPnP CSN).
 *	If there are no more valid locations, return 0 and leave
 *	struct bus_location zeroed, otherwise return true.
 *
 * int fill_device ( struct bus_location *bus_location,
 *		     struct bus_device *bus_device )
 *
 *	Fill out a bus_device structure with the parameters for the
 *	device at bus_location.  (For example, fill in the PCI vendor
 *	and device IDs).  Return true if there is a device physically
 *	present at this location, otherwise 0.
 *
 * int check_driver ( )
 *
 */
struct bus_operations {
	int ( *next_location ) ( struct bus_location * bus_location );
	int ( *fill_device ) ( struct bus_location * bus_location,
			       struct bus_device * bus_device );
	int ( *check_driver ) ( struct bus_device * bus_device,
				struct bus_driver * bus_driver );
	void ( *describe_device ) ( struct bus_device * bus_device,
				    struct bus_driver * bus_driver,
				    struct bus_description * bus_description );
};



#endif /* BUS_H */
