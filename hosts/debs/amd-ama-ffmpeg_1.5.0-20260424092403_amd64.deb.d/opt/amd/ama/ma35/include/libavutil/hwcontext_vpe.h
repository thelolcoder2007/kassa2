/*
 * VeriSilicon VPI Video codec
 * Copyright (C) 2020 VeriSilicon Holdings Co., Ltd.
 * Copyright (C) 2022 Xilinx Inc - All rights reserved
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef AVUTIL_HWCONTEXT_VPE_H
#define AVUTIL_HWCONTEXT_VPE_H

#include "vpi_api.h"
#include "vpi_types.h"

#include "pixfmt.h"

/**
 * @file
 * An API-specific header for AV_HWDEVICE_TYPE_AMA.
 */

/**
 * This struct is allocated as AVHWDeviceContext.hwctx
 * It will save some device level info
 */

typedef struct VpiMemCtxContainer VpiMemCtxContainer;

typedef struct AVVpeDeviceContext {
#ifndef SUPPORT_OSAL
    VpiHandle vpe_handle;
#endif
    VpiDevHandle vpe_dev_ctx;
    VpiHwCfg *dev_cfg;

    int sw_fmt_cnt;
    int hw_fmt_cnt;
    enum AVPixelFormat *supported_sw_formats;
    enum AVPixelFormat *supported_hw_formats;
    VpiMemCtxContainer *mem_ctx_cont;
} AVVpeDeviceContext;

typedef struct AVVpeFramesContext {
    VpiMemHandle       vpe_mem_ctx;
    VpiProcInfo        *proc_info;
    enum AVPixelFormat *sw_format;
    void               *opaque_up;
    void               *opaque_dn;
} AVVpeFramesContext;

typedef struct AVVpeHWConfig {
    VpiHwId id;
} AVVpeHWConfig;

#endif /* AVUTIL_HWCONTEXT_VPE_H */
