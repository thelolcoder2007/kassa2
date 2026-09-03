/*
 * Copyright(C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef __AMA_DYNAMIC_PARAMS_H__
#define __AMA_DYNAMIC_PARAMS_H__

#include "vpi_types.h"


#define DYNAMIC_PARAM_B_FRAMES_KEY    "NumB"
#define DYNAMIC_PARAM_BITRATE_KEY     "BRkbps"
#define DYNAMIC_PARAM_MIN_BITRATE_KEY "MinBRkbps"
#define DYNAMIC_PARAM_MAX_BITRATE_KEY "MaxBRkbps"
#define DYNAMIC_PARAM_T_AQ_KEY        "tAQ"
#define DYNAMIC_PARAM_T_AQ_GAIN_KEY   "tAQGain"
#define DYNAMIC_PARAM_S_AQ_KEY        "sAQ"
#define DYNAMIC_PARAM_S_AQ_GAIN_KEY   "sAQGain"
#define DYNAMIC_PARAM_QP_KEY          "QP"
#define DYNAMIC_PARAM_MIN_QP_KEY      "MinQP"
#define DYNAMIC_PARAM_MAX_QP_KEY      "MaxQP"
#define DYNAMIC_PARAM_QP_I_OFFSET_KEY "QPOffsetI"
#define DYNAMIC_PARAM_QP_B_OFFSET_KEY "QPOffsetB"

typedef enum {
  DYNAMIC_PARAM_MAX_LENGTH       = 256,
  DYNAMIC_PARAM_MIN_SPAT_AQ_GAIN = 0,
  DYNAMIC_PARAM_MAX_SPAT_AQ_GAIN = 255,
  DYNAMIC_PARAM_MIN_BITRATE      = 0,
  DYNAMIC_PARAM_MAX_BITRATE      = INT_MAX,
  DYNAMIC_PARAM_MIN_BFRAMES      = 0,
  DYNAMIC_PARAM_MAX_BFRAMES      = 4,
  DYNAMIC_PARAM_MIN_TAQ          = 0,
  DYNAMIC_PARAM_MAX_TAQ          = 1,
  DYNAMIC_PARAM_MIN_TAQ_GAIN     = 0,
  DYNAMIC_PARAM_MAX_TAQ_GAIN     = 255,
  DYNAMIC_PARAM_MIN_SAQ          = 0,
  DYNAMIC_PARAM_MAX_SAQ          = 1,
  DYNAMIC_PARAM_MIN_QP           = 0,
  DYNAMIC_PARAM_MAX_QP           = 51
} dynamic_param_defines;

int ama_encoder_get_dyn_params(FILE* dynamic_params_config_fp, size_t current_frame, VpiDynamicParams* dynamic_params);

#endif // __AMA_DYNAMIC_PARAMS_H__
