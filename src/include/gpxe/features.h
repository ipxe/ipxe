#ifndef _GPXE_FEATURES_H
#define _GPXE_FEATURES_H

#include <stdint.h>
#include <gpxe/tables.h>
#include <gpxe/dhcp.h>

/** @file
 *
 * Feature list
 *
 */

/**
 * @defgroup dhcpfeatures DHCP feature option tags
 *
 * DHCP feature option tags are Etherboot encapsulated options in the
 * range 0x10-0x7f.
 *
 * @{
 */

/** PXE API extensions */
#define DHCP_EB_FEATURE_PXE_EXT 0x10

/** iSCSI */
#define DHCP_EB_FEATURE_ISCSI 0x11

/** AoE */
#define DHCP_EB_FEATURE_AOE 0x12

/** HTTP */
#define DHCP_EB_FEATURE_HTTP 0x13

/** HTTPS */
#define DHCP_EB_FEATURE_HTTPS 0x14

/** @} */

/** Declare a feature code for DHCP */
#define __dhcp_feature __table ( uint8_t, dhcp_features, 01 )

/** Construct a DHCP feature table entry */
#define DHCP_FEATURE( feature_opt, version ) \
	_DHCP_FEATURE ( OBJECT, feature_opt, version )
#define _DHCP_FEATURE( _name, feature_opt, version ) \
	__DHCP_FEATURE ( _name, feature_opt, version )
#define __DHCP_FEATURE( _name, feature_opt, version )		\
	uint8_t __dhcp_feature_ ## _name [] __dhcp_feature = {	\
		feature_opt, DHCP_BYTE ( version )		\
	};

/** Declare a named feature */
#define __feature_name __table ( char *, features, 01 )

/** Construct a named feature */
#define FEATURE_NAME( text ) \
	_FEATURE_NAME ( OBJECT, text )
#define _FEATURE_NAME( _name, text ) \
	__FEATURE_NAME ( _name, text )
#define __FEATURE_NAME( _name, text )				\
	char * __feature_ ## _name __feature_name = text;

/** Declare a feature */
#define FEATURE( text, feature_opt, version )			\
	FEATURE_NAME ( text );					\
	DHCP_FEATURE ( feature_opt, version );

#endif /* _GPXE_FEATURES_H */
