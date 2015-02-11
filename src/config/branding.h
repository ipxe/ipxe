#ifndef CONFIG_BRANDING_H
#define CONFIG_BRANDING_H

/** @file
 *
 * Branding configuration
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <config/defaults.h>

/*
 * Branding
 *
 * Vendors may use these strings to add their own branding to iPXE.
 * PRODUCT_NAME is displayed prior to any iPXE branding in startup
 * messages, and PRODUCT_SHORT_NAME is used where a brief product
 * label is required (e.g. in BIOS boot selection menus).
 *
 * To minimise end-user confusion, it's probably a good idea to either
 * make PRODUCT_SHORT_NAME a substring of PRODUCT_NAME or leave it as
 * "iPXE".
 *
 */
#define PRODUCT_NAME ""
#define PRODUCT_SHORT_NAME "iPXE"

#include <config/local/branding.h>

#endif /* CONFIG_BRANDING_H */
