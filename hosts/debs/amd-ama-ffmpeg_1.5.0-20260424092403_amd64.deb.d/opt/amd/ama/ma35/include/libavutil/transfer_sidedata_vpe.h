/*
 * VeriSilicon hardware context
 * Copyright (C) 2020 VeriSilicon Holdings Co., Ltd. <>
 * Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVUTIL_TRANSFER_SIDEDATA_VPE_H
#define AVUTIL_TRANSFER_SIDEDATA_VPE_H

#include "libavutil/mem.h"
#include "vpi_api.h"
#include "vpi_types.h"
#include "frame.h"

/**
 * Transfer sidedata in VpiFrm structure to sidedata in AVFrame structure.
 * @param src  The source frame which contains VpiSidebandHandler.
 * @param side_ch The channel which frame saves sidedata.
 * @param dst  The destination frame which contains AVFrameSideData.
 * @return Return 0 indicating successful and others indicating failed.
 */
int vpe_transfer_side_data_from(VpiFrm *src, AVFrame *dst);


/**
 * Transfer sidedata in AVFrame structure to sidedata in VpiFrm structure.
 * @param mem_handle By which a vpe sideband pool was created.
 * @param src  The source frame which contains AVFrameSideData.
 * @param dst  The destination frame which contains VpiSidebandHandler.
 * @param side_ch The channel which frame saves sidedata.
 * @return Return 0 indicating successful and others indicating failed.
 */
int vpe_transfer_side_data_to(VpiProcHandle handle, AVFrame *src, VpiFrm *dst);

#endif
