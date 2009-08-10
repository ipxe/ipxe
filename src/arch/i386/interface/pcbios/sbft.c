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
 */

#include <assert.h>
#include <realmode.h>
#include <gpxe/srp.h>
#include <gpxe/ib_srp.h>
#include <gpxe/acpi.h>
#include <gpxe/sbft.h>

#define sbftab __use_data16 ( sbftab )
/** The sBFT used by gPXE */
struct gpxe_sbft __data16 ( sbftab ) = {
	/* Table header */
	.table = {
		/* ACPI header */
		.acpi = {
			.signature = SBFT_SIG,
			.length = sizeof ( sbftab ),
			.revision = 1,
			.oem_id = "FENSYS",
			.oem_table_id = "gPXE",
		},
		.scsi_offset = offsetof ( typeof ( sbftab ), scsi ),
		.srp_offset = offsetof ( typeof ( sbftab ), srp ),
		.ib_offset = offsetof ( typeof ( sbftab ), ib ),
	},
};

/**
 * Fill in all variable portions of sBFT
 *
 * @v srp		SRP device
 * @ret rc		Return status code
 */
int sbft_fill_data ( struct srp_device *srp ) {
	struct sbft_scsi_subtable *sbft_scsi = &sbftab.scsi;
	struct sbft_srp_subtable *sbft_srp = &sbftab.srp;
	struct sbft_ib_subtable *sbft_ib = &sbftab.ib;
	struct ib_srp_parameters *ib_params;
	struct segoff rm_sbftab = {
		.segment = rm_ds,
		.offset = __from_data16 ( &sbftab ),
	};

	/* Fill in the SCSI subtable */
	memcpy ( &sbft_scsi->lun, &srp->lun, sizeof ( sbft_scsi->lun ) );

	/* Fill in the SRP subtable */
	memcpy ( &sbft_srp->port_ids, &srp->port_ids,
		 sizeof ( sbft_srp->port_ids ) );

	/* Fill in the IB subtable */
	assert ( srp->transport == &ib_srp_transport );
	ib_params = ib_srp_params ( srp );
	memcpy ( &sbft_ib->sgid, &ib_params->sgid, sizeof ( sbft_ib->sgid ) );
	memcpy ( &sbft_ib->dgid, &ib_params->dgid, sizeof ( sbft_ib->dgid ) );
	memcpy ( &sbft_ib->service_id, &ib_params->service_id,
		 sizeof ( sbft_ib->service_id ) );
	sbft_ib->pkey = ib_params->pkey;

	/* Update checksum */
	acpi_fix_checksum ( &sbftab.table.acpi );

	DBGC ( &sbftab, "SRP Boot Firmware Table at %04x:%04x:\n",
	       rm_sbftab.segment, rm_sbftab.offset );
	DBGC_HDA ( &sbftab, rm_sbftab, &sbftab, sizeof ( sbftab ) );

	return 0;
}
