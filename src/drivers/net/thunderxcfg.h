#ifndef _THUNDERXCFG_H
#define _THUNDERXCFG_H

/** @file
 *
 * Cavium ThunderX Board Configuration
 *
 * The definitions in this section are extracted from BSD-licensed
 * (but non-public) portions of ThunderPkg.
 *
 */

FILE_LICENCE ( BSD2 );

#include <ipxe/efi/efi.h>

/******************************************************************************
 *
 * From ThunderxBoardConfig.h
 *
 ******************************************************************************
 *
 *  Header file for Cavium ThunderX Board Configurations
 *  Copyright (c) 2015, Cavium Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 *  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 *
 */

#define MAX_NODES		2
#define CLUSTER_COUNT		3
#define CORE_PER_CLUSTER_COUNT  16
#define CORE_COUNT		(CLUSTER_COUNT*CORE_PER_CLUSTER_COUNT)
#define BGX_PER_NODE_COUNT	2
#define LMAC_PER_BGX_COUNT	4
#define PEM_PER_NODE_COUNT	6
#define LMC_PER_NODE_COUNT	4
#define DIMM_PER_LMC_COUNT	2

#define THUNDERX_CPU_ID(node, cluster, core) (((node) << 16) | ((cluster) << 8) | (core))

/******************************************************************************
 *
 * From ThunderConfigProtocol.h
 *
 ******************************************************************************
 *
 *  Thunder board Configuration Protocol
 *
 *  Copyright (c) 2015, Cavium Inc. All rights reserved.<BR>
 *
 *  This program and the accompanying materials are licensed and made
 *  available under the terms and conditions of the BSD License which
 *  accompanies this distribution.  The full text of the license may
 *  be found at http://opensource.org/licenses/bsd-license.php
 *
 *  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS"
 *  BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED.
 *
 */

#define EFI_THUNDER_CONFIG_PROTOCOL_GUID \
  {0xc12b1873, 0xac17, 0x4176, {0xac, 0x77, 0x7e, 0xcb, 0x4d, 0xef, 0xff, 0xec}}

///
/// Forward declaration
///
typedef struct _EFI_THUNDER_CONFIG_PROTOCOL EFI_THUNDER_CONFIG_PROTOCOL;

typedef enum {
  BGX_ENABLED,
  BGX_MODE,
  LMAC_COUNT,
  BASE_ADDRESS,
  LMAC_TYPE_BGX,
  QLM_MASK,
  QLM_FREQ,
  USE_TRAINING
} BGX_PROPERTY;

typedef enum {
  ENABLED,
  LANE_TO_SDS,
  MAC_ADDRESS
} LMAC_PROPERTY;

///
/// Function prototypes
///
typedef
EFI_STATUS
(EFIAPI *EFI_THUNDER_CONFIG_PROTOCOL_GET_CONFIG)(
  IN EFI_THUNDER_CONFIG_PROTOCOL  *This,
  OUT VOID** cfg
  );

typedef
EFI_STATUS
(EFIAPI *EFI_THUNDER_CONFIG_PROTOCOL_GET_BGX_PROP)(
  IN EFI_THUNDER_CONFIG_PROTOCOL   *This,
  IN UINTN                         NodeId,
  IN UINTN                         BgxId,
  IN BGX_PROPERTY                  BgxProp,
  IN UINT64                        ValueSize,
  OUT UINT64                       *Value
  );

typedef
EFI_STATUS
(EFIAPI *EFI_THUNDER_CONFIG_PROTOCOL_GET_LMAC_PROP)(
  IN EFI_THUNDER_CONFIG_PROTOCOL   *This,
  IN UINTN                         NodeId,
  IN UINTN                         BgxId,
  IN UINTN                         LmacId,
  IN LMAC_PROPERTY                 LmacProp,
  IN UINT64                        ValueSize,
  OUT UINT64                       *Value
  );

///
/// Protocol structure
///
struct _EFI_THUNDER_CONFIG_PROTOCOL {
  EFI_THUNDER_CONFIG_PROTOCOL_GET_CONFIG GetConfig;
  EFI_THUNDER_CONFIG_PROTOCOL_GET_BGX_PROP GetBgxProp;
  EFI_THUNDER_CONFIG_PROTOCOL_GET_LMAC_PROP GetLmacProp;
  VOID* BoardConfig;
};

#endif /* _THUNDERXCFG_H */
