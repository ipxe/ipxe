#ifndef _GPXE_SBFT_H
#define _GPXE_SBFT_H

/*
 * Copyright (C) 2009 Fen Systems Ltd <mbrown@fensystems.co.uk>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

FILE_LICENCE ( BSD2 );

/** @file
 *
 * SRP boot firmware table
 *
 * The working draft specification for the SRP boot firmware table can
 * be found at
 *
 *   http://etherboot.org/wiki/srp/sbft
 *
 */

#include <stdint.h>
#include <gpxe/acpi.h>
#include <gpxe/scsi.h>
#include <gpxe/srp.h>
#include <gpxe/ib_srp.h>

/** SRP Boot Firmware Table signature */
#define SBFT_SIG "sBFT"

/** An offset from the start of the sBFT */
typedef uint16_t sbft_off_t;

/**
 * SRP Boot Firmware Table
 */
struct sbft_table {
	/** ACPI header */
	struct acpi_description_header acpi;
	/** Offset to SCSI subtable */
	sbft_off_t scsi_offset;
	/** Offset to SRP subtable */
	sbft_off_t srp_offset;
	/** Offset to IB subtable, if present */
	sbft_off_t ib_offset;
	/** Reserved */
	uint8_t reserved[6];
} __attribute__ (( packed ));

/**
 * sBFT SCSI subtable
 */
struct sbft_scsi_subtable {
	/** LUN */
	struct scsi_lun lun;
} __attribute__ (( packed ));

/**
 * sBFT SRP subtable
 */
struct sbft_srp_subtable {
	/** Initiator and target ports */
	struct srp_port_ids port_ids;
} __attribute__ (( packed ));

/**
 * sBFT IB subtable
 */
struct sbft_ib_subtable {
	/** Source GID */
	struct ib_gid sgid;
	/** Destination GID */
	struct ib_gid dgid;
	/** Service ID */
	struct ib_gid_half service_id;
	/** Partition key */
	uint16_t pkey;
	/** Reserved */
	uint8_t reserved[6];
} __attribute__ (( packed ));

/**
 * An sBFT created by gPXE
 */
struct gpxe_sbft {
	/** The table header */
	struct sbft_table table;
	/** The SCSI subtable */
	struct sbft_scsi_subtable scsi;
	/** The SRP subtable */
	struct sbft_srp_subtable srp;
	/** The IB subtable */
	struct sbft_ib_subtable ib;
} __attribute__ (( packed, aligned ( 16 ) ));

struct srp_device;

extern int sbft_fill_data ( struct srp_device *srp );

#endif /* _GPXE_SBFT_H */
