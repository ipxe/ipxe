#ifndef _IPXE_EFI_SHIM_LOCK_PROTOCOL_H
#define _IPXE_EFI_SHIM_LOCK_PROTOCOL_H

/** @file
 *
 * EFI "shim lock" protocol
 *
 */

FILE_LICENCE ( BSD3 );

#define EFI_SHIM_LOCK_PROTOCOL_GUID					\
	{ 0x605dab50, 0xe046, 0x4300,					\
	{ 0xab, 0xb6, 0x3d, 0xd8, 0x10, 0xdd, 0x8b, 0x23 } }

#define SHIMAPI __asmcall

typedef
EFI_STATUS SHIMAPI
(*EFI_SHIM_LOCK_VERIFY) (
  IN VOID *buffer,
  IN UINT32 size
  );

typedef struct _EFI_SHIM_LOCK_PROTOCOL {
  EFI_SHIM_LOCK_VERIFY Verify;
  VOID *Reserved1;
  VOID *Reserved2;
} EFI_SHIM_LOCK_PROTOCOL;

#endif /*_IPXE_EFI_SHIM_LOCK_PROTOCOL_H */
