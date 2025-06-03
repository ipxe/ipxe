/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <config/general.h>

/** @file
 *
 * Ethernet configuration options
 *
 */

PROVIDE_REQUIRING_SYMBOL();

/*
 * Drag in Ethernet-specific protocols
 */
#ifdef SANBOOT_PROTO_AOE
REQUIRE_OBJECT ( aoe );
#endif
#ifdef NET_PROTO_FCOE
REQUIRE_OBJECT ( fcoe );
#endif
#ifdef NET_PROTO_STP
REQUIRE_OBJECT ( stp );
#endif
#ifdef NET_PROTO_LACP
REQUIRE_OBJECT ( eth_slow );
#endif
#ifdef NET_PROTO_EAPOL
REQUIRE_OBJECT ( eapol );
#endif
#ifdef NET_PROTO_LLDP
REQUIRE_OBJECT ( lldp );
#endif
