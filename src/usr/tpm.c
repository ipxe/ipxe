/*
 * Copyright (C) 2023 Konst Kolesnichenko <kolesnichenko@gmail.com>.
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

FILE_LICENCE(GPL2_OR_LATER_OR_UBDL);

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ipxe/settings.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/TcgService.h>
#include <ipxe/efi/Protocol/Tcg2Protocol.h>

/** @file
 *
 * TPM Presence information, provides settings variable tpm
 * with 0 in case if TPM is absent or disabled
 * and 1 if TPM is up and running (regardless of version).
 */

static BOOLEAN tpm2_present()
{
	EFI_GUID tpm2_guid = EFI_TCG2_PROTOCOL_GUID;
	EFI_TCG2_PROTOCOL *tcg;
	EFI_STATUS efirc;
	int rc;
	EFI_TCG2_BOOT_SERVICE_CAPABILITY caps;

	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	if ((efirc = bs->LocateProtocol(&tpm2_guid,
									NULL,
									(VOID **)&tcg)) != 0)
	{
		rc = -EEFI(efirc);
		DBGC(efi_systab, "Failed to locate EFI_TCG2_PROTOCOL: %s\n",
			 strerror(rc));
		return FALSE;
	}

	caps.Size = (uint8_t)sizeof(caps);

	if ((efirc = tcg->GetCapability(tcg, &caps)) != 0)
	{

		DBGC(efi_systab, "Failed to query TPM2.0 capability\n");
		return FALSE;
	}

	return caps.TPMPresentFlag;
}

static BOOLEAN tpm1_present()
{
	EFI_TCG_PROTOCOL *tcg_protocol;
	EFI_STATUS efirc;
	int rc;
	TCG_EFI_BOOT_SERVICE_CAPABILITY caps;
	uint32_t tcg_feature_flags;
	EFI_PHYSICAL_ADDRESS event_log_location;
	EFI_PHYSICAL_ADDRESS event_log_last_entry;

	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	if ((efirc = bs->LocateProtocol(&efi_tcg_protocol_guid,
									NULL,
									(VOID **)&tcg_protocol)) != 0)
	{
		rc = -EEFI(efirc);
		DBGC(efi_systab, "Failed to locate EFI_TCG_PROTOCOL: %s\n", strerror(rc));
		return FALSE;
	}

	caps.Size = (uint8_t)sizeof(caps);

	if ((efirc = tcg_protocol->StatusCheck(tcg_protocol, &caps, &tcg_feature_flags,
										   &event_log_location, &event_log_last_entry)) != 0)
	{

		DBGC(efi_systab, "Failed to query TPM status\n");
		return FALSE;
	}

	if (caps.TPMDeactivatedFlag)
	{
		return FALSE;
	}

	return caps.TPMPresentFlag;
}

/**
 * Fetch tpm setting
 *
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int tpm_fetch(void *data, size_t len)
{
	uint8_t content;

	if (tpm2_present())
	{
		content = 1;
	}
	else if (tpm1_present())
	{
		content = 1;
	}
	else
	{
		content = 0;
	}

	if (len > sizeof(content))
		len = sizeof(content);
	memcpy(data, &content, len);
	return sizeof(content);
}

/** TPM setting */
const struct setting tpm_setting __setting(SETTING_MISC, tpm) = {
	.name = "tpm",
	.description = "TPM presence information",
	.type = &setting_type_uint8,
	.scope = &builtin_scope,
};

/** TPM built-in setting */
struct builtin_setting tpm_builtin_setting __builtin_setting = {
	.setting = &tpm_setting,
	.fetch = tpm_fetch,
};
