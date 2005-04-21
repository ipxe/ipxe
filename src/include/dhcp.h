#ifndef DHCP_H
#define DHCP_H

#include "stdint.h"

struct dhcp_dev_id {
	uint8_t		bus_type;
	uint16_t	vendor_id;
	uint16_t	device_id;
} __attribute__ (( packed ));

#endif /* DHCP_H */
