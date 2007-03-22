#ifndef DHCP_BASEMEM_H
#define DHCP_BASEMEM_H

#include <realmode.h>

/** Maximum length of a DHCP data buffer */
#define DHCP_BASEMEM_LEN 1514

/** DHCP data buffer */
extern char __data16_array ( dhcp_basemem, [DHCP_BASEMEM_LEN] );
#define dhcp_basemem __use_data16 ( dhcp_basemem )

#endif /* DHCP_BASEMEM_H */
