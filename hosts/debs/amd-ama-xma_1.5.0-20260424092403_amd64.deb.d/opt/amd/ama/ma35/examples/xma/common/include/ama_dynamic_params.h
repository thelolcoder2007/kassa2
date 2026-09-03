/* SPDX-License-Identifier: LGPL-3.0-or-later OR Apache-2.0 */

/*
 * Copyright (C) 2022 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * Copyright (C) 2022, Xilinx Inc - All rights reserved
 *
 * AMA dynamic params interface
 *
 * This file is dual-licensed; you may select either the GNU
 * Lesser General Public License version 3 or
 * Apache License, Version 2.0.
 *
 */

#ifndef __AMA_DYNAMIC_PARAMS_H__
#define __AMA_DYNAMIC_PARAMS_H__

#include "xmasidedata.h"

#define DYNAMIC_PARAM_B_FRAMES_KEY "NumB"
#define DYNAMIC_PARAM_BITRATE_KEY "BRkbps"
#define DYNAMIC_PARAM_MIN_BITRATE_KEY "MinBRkbps"
#define DYNAMIC_PARAM_MAX_BITRATE_KEY "MaxBRkbps"
#define DYNAMIC_PARAM_T_AQ_KEY "tAQ"
#define DYNAMIC_PARAM_T_AQ_GAIN_KEY "tAQGain"
#define DYNAMIC_PARAM_S_AQ_KEY "sAQ"
#define DYNAMIC_PARAM_S_AQ_GAIN_KEY "sAQGain"
#define DYNAMIC_PARAM_QP_KEY "QP"
#define DYNAMIC_PARAM_MIN_QP_KEY "MinQP"
#define DYNAMIC_PARAM_MAX_QP_KEY "MaxQP"
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

int ama_encoder_get_dyn_params(XmaLogHandle log, FILE* dynamic_params_config_fp, size_t current_frame, XmaDynamicEncParams* dynamic_params);

//Defining the v2 version to avoid breaking the backward compatibility
int ama_encoder_get_dyn_params_v2(XmaLogHandle log, FILE* dynamic_params_config_fp, size_t current_frame, XmaDynamicEncParams_v2* dynamic_params);

#endif // __AMA_DYNAMIC_PARAMS_H__
