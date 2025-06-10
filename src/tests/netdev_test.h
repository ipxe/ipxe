#ifndef _NETDEV_TEST_H
#define _NETDEV_TEST_H

/** @file
 *
 * Network device tests
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/device.h>
#include <ipxe/netdevice.h>

/** A test network device setting */
struct testnet_setting {
	/** Setting name (relative to network device's settings) */
	const char *name;
	/** Value */
	const char *value;
};

/** A test network device */
struct testnet {
	/** Network device */
	struct net_device *netdev;
	/** Dummy physical device */
	struct device dev;
	/** Initial settings */
	struct testnet_setting *testset;
	/** Number of initial settings */
	unsigned int count;
};

/**
 * Declare a test network device
 *
 * @v NAME		Network device name
 * @v ...		Initial network device settings
 */
#define TESTNET( NAME, ... )						\
	static struct testnet_setting NAME ## _setting[] = {		\
		__VA_ARGS__						\
	};								\
	static struct testnet NAME = {					\
		.dev = {						\
			.name = #NAME,					\
			.driver_name = "testnet",			\
			.siblings =					\
				LIST_HEAD_INIT ( NAME.dev.siblings ),	\
			.children =					\
				LIST_HEAD_INIT ( NAME.dev.children ),	\
		},							\
		.testset = NAME ## _setting,				\
		.count = ( sizeof ( NAME ## _setting ) /		\
			   sizeof ( NAME ## _setting[0] ) ),		\
	};

/**
 * Report a network device creation test result
 *
 * @v testnet		Test network device
 */
#define testnet_ok( testnet ) testnet_okx ( testnet, __FILE__, __LINE__ )
extern void testnet_okx ( struct testnet *testnet, const char *file,
			  unsigned int line );

/**
 * Report a network device opening test result
 *
 * @v testnet		Test network device
 */
#define testnet_open_ok( testnet ) \
	testnet_open_okx ( testnet, __FILE__, __LINE__ )
extern void testnet_open_okx ( struct testnet *testnet, const char *file,
			       unsigned int line );

/**
 * Report a network device setting test result
 *
 * @v testnet		Test network device
 * @v name		Setting name (relative to network device's settings)
 * @v value		Setting value
 */
#define testnet_set_ok( testnet, name, value ) \
	testnet_set_okx ( testnet, name, value, __FILE__, __LINE__ )
extern void testnet_set_okx ( struct testnet *testnet, const char *name,
			      const char *value, const char *file,
			      unsigned int line );

/**
 * Report a network device closing test result
 *
 * @v testnet		Test network device
 */
#define testnet_close_ok( testnet ) \
	testnet_close_okx ( testnet, __FILE__, __LINE__ )
extern void testnet_close_okx ( struct testnet *testnet, const char *file,
				unsigned int line );

/**
 * Report a network device removal test result
 *
 * @v testnet		Test network device
 */
#define testnet_remove_ok( testnet ) \
	testnet_remove_okx ( testnet, __FILE__, __LINE__ )
extern void testnet_remove_okx ( struct testnet *testnet, const char *file,
				 unsigned int line );

#endif /* _NETDEV_TEST_H */
