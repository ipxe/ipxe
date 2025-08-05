#ifndef _IPXE_GPIO_H
#define _IPXE_GPIO_H

/** @file
 *
 * General purpose I/O
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/list.h>
#include <ipxe/refcnt.h>
#include <ipxe/device.h>

/** A GPIO pin */
struct gpio {
	/** GPIO controller */
	struct gpios *gpios;
	/** Pin index */
	unsigned int index;
	/** Configuration */
	unsigned int config;
};

/** GPIO is active low
 *
 * This bit is chosen to match the devicetree standard usage.
 */
#define GPIO_CFG_ACTIVE_LOW 0x01

/** GPIO is an output */
#define GPIO_CFG_OUTPUT 0x0100

/** A GPIO controller */
struct gpios {
	/** Reference count */
	struct refcnt refcnt;
	/** List of GPIO controllers */
	struct list_head list;
	/** Generic device */
	struct device *dev;
	/** Number of GPIOs */
	unsigned int count;

	/** Individual GPIOs */
	struct gpio *gpio;
	/** GPIO operations */
	struct gpio_operations *op;

	/** Driver-private data */
	void *priv;
};

/** GPIO operations */
struct gpio_operations {
	/**
	 * Get current GPIO input value
	 *
	 * @v gpios		GPIO controller
	 * @v gpio		GPIO pin
	 * @ret active		Pin is in the active state
	 */
	int ( * in ) ( struct gpios *gpios, struct gpio *gpio );
	/**
	 * Set current GPIO output value
	 *
	 * @v gpios		GPIO controller
	 * @v gpio		GPIO pin
	 * @v active		Set pin to active state
	 */
	void ( * out ) ( struct gpios *gpios, struct gpio *gpio, int active );
	/**
	 * Configure GPIO pin
	 *
	 * @v gpios		GPIO controller
	 * @v gpio		GPIO pin
	 * @v config		Configuration
	 * @ret rc		Return status code
	 */
	int ( * config ) ( struct gpios *gpios, struct gpio *gpio,
			   unsigned int config );
};

extern struct gpio_operations null_gpio_operations;

/**
 * Get reference to GPIO controller
 *
 * @v gpios		GPIO controller
 * @ret gpios		GPIO controller
 */
static inline __attribute__ (( always_inline )) struct gpios *
gpios_get ( struct gpios *gpios ) {
	ref_get ( &gpios->refcnt );
	return gpios;
}

/**
 * Drop reference to GPIO controller
 *
 * @v gpios		GPIO controller
 */
static inline __attribute__ (( always_inline )) void
gpios_put ( struct gpios *gpios ) {
	ref_put ( &gpios->refcnt );
}

/**
 * Get reference to GPIO pin
 *
 * @v gpio		GPIO pin
 * @ret gpio		GPIO pin
 */
static inline __attribute__ (( always_inline )) struct gpio *
gpio_get ( struct gpio *gpio ) {
	gpios_get ( gpio->gpios );
	return gpio;
}

/**
 * Drop reference to GPIO ping
 *
 * @v gpio		GPIO pin
 */
static inline __attribute__ (( always_inline )) void
gpio_put ( struct gpio *gpio ) {
	gpios_put ( gpio->gpios );
}

/**
 * Initialise a GPIO controller
 *
 * @v gpios		GPIO controller
 * @v op		GPIO operations
 */
static inline __attribute__ (( always_inline )) void
gpios_init ( struct gpios *gpios, struct gpio_operations *op ) {
	gpios->op = op;
}

/**
 * Stop using a GPIO controller
 *
 * @v gpios		GPIO controller
 *
 * Drivers should call this method immediately before the final call
 * to gpios_put().
 */
static inline __attribute__ (( always_inline )) void
gpios_nullify ( struct gpios *gpios ) {
	gpios->op = &null_gpio_operations;
}

/**
 * Get current GPIO input value
 *
 * @v gpio		GPIO pin
 * @ret active		Pin is in the active state
 */
static inline int gpio_in ( struct gpio *gpio ) {
	struct gpios *gpios = gpio->gpios;

	return gpios->op->in ( gpios, gpio );
}

/**
 * Set current GPIO output value
 *
 * @v gpio		GPIO pin
 * @v active		Set pin to active state
 */
static inline void gpio_out ( struct gpio *gpio, int active ) {
	struct gpios *gpios = gpio->gpios;

	gpios->op->out ( gpios, gpio, active );
}

/**
 * Configure GPIO pin
 *
 * @v gpio		GPIO pin
 * @v config		Configuration
 * @ret rc		Return status code
 */
static inline int gpio_config ( struct gpio *gpio, unsigned int config ) {
	struct gpios *gpios = gpio->gpios;

	return gpios->op->config ( gpios, gpio, config );
}

extern struct gpios * alloc_gpios ( unsigned int count, size_t priv_len );
extern int gpios_register ( struct gpios *gpios );
extern void gpios_unregister ( struct gpios *gpios );
extern struct gpios * gpios_find ( unsigned int bus_type,
				   unsigned int location );

#endif /* _IPXE_GPIO_H */
