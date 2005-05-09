#ifndef IMAGE_H
#define IMAGE_H

#include "stdint.h"
#include "io.h"
#include "tables.h"

#define IMAGE_HEADER_SIZE 512

struct image_header {
	char data[IMAGE_HEADER_SIZE];
};

struct image {
	char *name;
	int ( * probe ) ( struct image_header *header, off_t len );
	int ( * boot ) ( physaddr_t start, off_t len );
};

#define __image_start		__table_start(image)
#define __image			__table(image,01)
#define __default_image		__table(image,02)
#define __image_end		__table_end(image)

#endif /* IMAGE_H */
