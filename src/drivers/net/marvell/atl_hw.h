/*
 * Copyright(C) 2017-2021 Marvell
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice, 
 *     this list of conditions and the following disclaimer in the documentation 
 *     and/or other materials provided with the distribution.
 *   
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 *   THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 *   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR HOLDER OR 
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 *   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 *   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 *   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 *   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 *   POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef __ATL_HW_H
#define __ATL_HW_H

FILE_LICENCE ( BSD2 );

#define ATL_GLB_STD_CTRL 0x0
#define ATL_GLB_CTRL_RST_DIS 0x4000
#define ATL_FW_VER 0x18

#define ATL_MPI_DAISY_CHAIN_STS 0x704
#define ATL_MPI_BOOT_EXIT_CODE 0x388
#define ATL_SEM_TIMEOUT 0x348
#define ATL_GLB_CTRL2 0x404
#define ATL_GLB_MCP_SEM1 0x3A0
#define ATL_GLB_MCP_SEM4 0x3AC
#define ATL_GLB_MCP_SEM5 0x3B0
#define ATL_GLB_MCP_SP26 0x364
#define ATL_MIF_PWR_GATING_EN_CTRL 0x32A8
#define ATL_GLB_NVR_PROV4 0x53C
#define ATL_GEN_PROV9 0x520

#define ATL_MAC_PHY_CTRL 0x00004000U
#define ATL_MAC_PHY_CTRL_RST_DIS 0x20000000U

#endif
