// Copyright(C) 2022 - 2024 Advanced Micro Devices, Inc. All rights reserved.
//
// Copyright (C) 2022, Xilinx Inc - All rights reserved
//
// Licensed under the Apache License, Version 2.0 (the "License"). You may
// not use this file except in compliance with the License. A copy of the
// License is located at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.

#ifndef _XLNX_ENC_XMA_PROPS_H_
#define _XLNX_ENC_XMA_PROPS_H_

#include <dlfcn.h>
#include <getopt.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xlnx_app_utils.h"
#include "xlnx_enc_constants.h"

typedef struct {
  int64_t       bitrate;
  int64_t       max_bitrate;
  uint32_t      device_id;
  int32_t       width;
  int32_t       height;
  XmaFormatType pix_fmt;
  int32_t       fps;
  int32_t       gop_size;
  int32_t       qp;
  int32_t       min_qp;
  int32_t       max_qp;
  int32_t       temp_aq_gain;
  int32_t       spat_aq_gain;
  int32_t       spatial_aq;
  int32_t       temporal_aq;
  int32_t       qp_mode;
  int32_t       rc_mode;
  int32_t       crf;
  int32_t       force_idr;
  int32_t       slice;
  int32_t       num_cores;
  int32_t       codec_id;
  int32_t       device_type;
  int32_t       num_bframes;
  int32_t       preset;
  int32_t       profile;
  int32_t       level;
  int32_t       tier;
  int32_t       lookahead_depth;
  int32_t       latency_ms;
  int32_t       tune_metrics;
  int32_t       dynamic_gop;
  char          expert_options[2048];
  int32_t       latency_logging;
  char          enc_preset[15];
  int32_t       bufsize;
  int32_t       no_low_latency_b_frames;
  char          cabr_config[1024];
} XlnxEncoderProperties;

void xlnx_enc_free_xma_props(XmaEncoderProperties* xma_enc_props);

int32_t xlnx_enc_create_xma_props(XmaHandle handle, XlnxEncoderProperties* enc_props, XmaFilterProperties* xma_upload_props, XmaEncoderProperties* xma_enc_props);

#endif // _XLNX_ENC_XMA_PROPS_H_
