/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
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

#include <ipxe/io.h>
#include <ipxe/riscv_io.h>

/** @file
 *
 * iPXE I/O API for RISC-V
 *
 */

PROVIDE_IOAPI_INLINE ( riscv, phys_to_bus );
PROVIDE_IOAPI_INLINE ( riscv, bus_to_phys );
PROVIDE_IOAPI_INLINE ( riscv, readb );
PROVIDE_IOAPI_INLINE ( riscv, readw );
PROVIDE_IOAPI_INLINE ( riscv, readl );
PROVIDE_IOAPI_INLINE ( riscv, writeb );
PROVIDE_IOAPI_INLINE ( riscv, writew );
PROVIDE_IOAPI_INLINE ( riscv, writel );
PROVIDE_IOAPI_INLINE ( riscv, readq );
PROVIDE_IOAPI_INLINE ( riscv, writeq );
PROVIDE_IOAPI_INLINE ( riscv, mb );
PROVIDE_DUMMY_PIO ( riscv );
