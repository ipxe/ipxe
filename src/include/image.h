#ifndef IMAGE_H
#define IMAGE_H

#include "stdint.h"
#include "io.h"
#include <gpxe/tables.h>
#include "dev.h"

struct image {
	char *name;
	int ( * probe ) ( physaddr_t data, off_t len, void **context );
	int ( * load ) ( physaddr_t data, off_t len, void *context );
	int ( * boot ) ( void *context );
};

#define __image_start		__table_start(image)
#define __image			__table(image,01)
#define __default_image		__table(image,02)
#define __image_end		__table_end(image)

/* Functions in image.c */

extern void print_images ( void );
extern int autoload ( struct dev *dev, struct image **image, void **context );

#endif /* IMAGE_H */
