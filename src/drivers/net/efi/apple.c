/*
 * Copyright (C) 2020 Google.
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

#include <errno.h>
#include <ipxe/efi/efi.h>
#include <string.h>

static void efi_unload_apple_images_for_protocol(EFI_HANDLE handle,
                                                 EFI_GUID *protocol) {
  if (!handle || !protocol) return;

  // Retrieve list of openers
  EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *openers;
  UINTN count;
  EFI_STATUS efirc;
  int rc;
  if ((efirc = bs->OpenProtocolInformation(handle, protocol, &openers,
                                           &count)) != 0) {
    rc = -EEFI(efirc);
    DBGC(handle, "MNP %s retrieve openers failed for %s: %s\n",
         efi_handle_name(handle), efi_guid_ntoa(protocol), strerror(rc));
    return;
  }

  unsigned int i;
  for (i = 0; i < count; ++i) {
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *opener = &openers[i];
    const char *name = efi_handle_name(opener->AgentHandle);
    if (strstr(name, "Apple") != NULL) {
      DBGC(handle, "MNP %s attempting to unload image \"%s\"\n",
           efi_handle_name(handle), name);
      bs->UnloadImage(opener->AgentHandle);
    }
  }

  // Free list
  bs->FreePool(openers);
}

// Unload images bound to this device with "Apple" in the thier name
void efi_unload_apple_images(EFI_HANDLE handle) {
  if (!handle) return;

  // Retrieve list of protocols
  EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
  EFI_GUID **protocols;
  UINTN count;
  EFI_STATUS efirc;
  int rc;
  if ((efirc = bs->ProtocolsPerHandle(handle, &protocols, &count)) != 0) {
    rc = -EEFI(efirc);
    DBGC(handle, "MNP %s retrieve protocols failed: %s\n",
         efi_handle_name(handle), strerror(rc));
    return;
  }

  unsigned int i;
  for (i = 0; i < count; ++i) {
    efi_unload_apple_images_for_protocol(handle, protocols[i]);
  }

  // Free list
  bs->FreePool(protocols);
}
