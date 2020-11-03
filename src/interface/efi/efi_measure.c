/*
 * Copyright C 2020, Oracle and/or its affiliates.
 * Licensed under the GPL v2 only.
 */

FILE_LICENCE ( GPL2_ONLY );

/** @file
 *
 * EFI measurement support 
 *
 */

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_strings.h>
#include <ipxe/efi/efi_utils.h>
#include <ipxe/efi/IndustryStandard/UefiTcgPlatform.h>
#include <ipxe/efi/Protocol/Tcg2Protocol.h>
#include <ipxe/efi/Protocol/TcgService.h>
#include <ipxe/image.h>
#include <ipxe/measure.h>
#include <ipxe/script.h>
#include <ipxe/uaccess.h>
#include <ipxe/umalloc.h>

/** TCG Protocol */
static EFI_TCG_PROTOCOL *tcg;
EFI_REQUEST_PROTOCOL ( EFI_TCG_PROTOCOL, &tcg );

/** TCG2 Protocol */
static EFI_TCG2_PROTOCOL *tcg2;
EFI_REQUEST_PROTOCOL ( EFI_TCG2_PROTOCOL, &tcg2 );

/**
 * Measure TPM2 data 
 *
 * @v			image to measure
 * @ret rc		Return status code
 */
static int efi_measure_image_tpm2 ( struct image *image ) {
	EFI_TCG2_BOOT_SERVICE_CAPABILITY ProtocolCapability;
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_TCG2_EVENT *Tcg2Event;
	EFI_STATUS efirc;
	int rc;

	/* First validate that the TPM is functional */
	ProtocolCapability.Size = (UINT8) sizeof ( ProtocolCapability );
	efirc = tcg2->GetCapability ( tcg2, &ProtocolCapability );
	if ( efirc || ( ! ProtocolCapability.TPMPresentFlag ) ) {
		DBG ( "EFIMSR TPM not functional\n" );
		return -ENODEV;
	}

	if ( image->type == &script_image_type ) {

		/**
		 * Per the TCG PC Client Platform Firmware Spec,
		 * Level 00 Rev. 1.04, Section 2.3.4.6 PCR[5]:
		 * "UEFI Application code itself will record its
		 * configuration data". "Example: PXE code measuring
		 * partition data it loaded using the EV_EFI_ACTION
		 * event and encoding the data as an ASCII string".
		 * Thus, we will measure script data in the same
		 * manner - passing the ASCII script as the string.
		 * NOTE - per spec (section 9.4.4) - string is not to
		 * include the NULL terminator. So, it's really just
		 * a char buffer.
		 */

		/* Allocate EFI boot mem for EFI_TCG_EVENT + Event[] data */
		Tcg2Event = NULL;
		bs->AllocatePool ( EfiBootServicesData,
				   ( sizeof ( EFI_TCG2_EVENT ) -
				     sizeof ( Tcg2Event->Event ) + image->len ),
				    (void *) &Tcg2Event );
		if ( ! Tcg2Event ) {
			DBG ( "EFIMSR AllocatePool failed\n" );
			return -ENOMEM;
		}

		Tcg2Event->Size = sizeof ( EFI_TCG2_EVENT ) -
				  sizeof ( Tcg2Event->Event ) + image->len;
		Tcg2Event->Header.HeaderSize = sizeof ( EFI_TCG2_EVENT_HEADER );
		Tcg2Event->Header.HeaderVersion = EFI_TCG2_EVENT_HEADER_VERSION;
		Tcg2Event->Header.PCRIndex = 5; /* Per spec */
		Tcg2Event->Header.EventType = EV_EFI_ACTION;
		memcpy( (void *) Tcg2Event->Event, (const void *) image->data,
		        (size_t) image->len );
	} else {
		DBG ( "EFIMSR image type %s, not supported.\n",
		      image->type->name );
		return -ENOTSUP;
	}

	DBG ( "EFIMSR measuring image (type = %s, len = %d)\n",
	      image->type->name, (int) image->len );

	if ( ( efirc = tcg2->HashLogExtendEvent ( tcg2,
						  0,
						  user_to_phys ( image->data, 0 ),
						  image->len,
						  Tcg2Event) ) != 0 ) {
		if ( efirc == EFI_VOLUME_FULL ) {
			/* Event Log is full, but PCR was still extended */
			printf ( "WARNING - TPM Event Log is full\n" );
		} else {
			rc = -EEFI ( efirc );
			DBG ( "EFIMSR HashLogExtendEvent failed: %s\n",
			      strerror ( rc ) );
			bs->FreePool ( Tcg2Event );
			return rc;
		}
	}

	bs->FreePool ( Tcg2Event );

	DBG ( "EFIMSR SUCCESS\n" );

	return 0;
}

/**
 * Measure TPM12 data 
 *
 * @v			image to measure
 * @ret rc		Return status code
 */
static int efi_measure_image_tpm12 ( struct image *image __unused ) {

	/* TBD */
	printf ( "TPM 1.2 not supported\n" );
	return -ENOTSUP;
}

/**
 * Measure data 
 *
 * @v			image to measure
 * @ret rc		Return status code
 */
static int efi_measure_image ( struct image *image ) {

	if ( image == NULL ) {
		DBG ( "EFIMSR Invalid Argument: image = NULL\n" );
		return -EINVAL;
	}

	if ( !image->data || image->len == 0 ) {
		DBG ( "EFIMSR Invalid Argument: image data is empty\n" );
		return -EINVAL;
	}

	/* First check for TPM2 Protocol, fallback to TPM Protocol */
	if ( tcg2 )
		return efi_measure_image_tpm2 ( image );
	else if ( tcg )
		return efi_measure_image_tpm12 ( image );

	DBG ( "EFIMSR TCG/TCG2 Protocol not found\n" );

	return -ENOTSUP;
}

PROVIDE_MEASURE ( efi, measure_image, efi_measure_image );
