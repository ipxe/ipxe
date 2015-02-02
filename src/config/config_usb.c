/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <config/general.h>

/** @file
 *
 * USB configuration options
 *
 */

/*
 * Drag in USB controllers
 */
#ifdef USB_HCD_XHCI
REQUIRE_OBJECT ( xhci );
#endif
