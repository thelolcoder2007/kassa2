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

#ifndef _XLNX_TRANSCODER_XMA_PROPS_H_
#define _XLNX_TRANSCODER_XMA_PROPS_H_

#include <dlfcn.h>
#include <getopt.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xlnx_app_utils.h"
#include "xlnx_transcoder_constants.h"

typedef struct XlnxDecoderProperties {
  int32_t       device_id; // -1 by default
  uint32_t      width;
  uint32_t      height;
  uint32_t      fps;
  uint32_t      log_level;
  uint32_t      bit_depth;
  uint32_t      codec_type;
  uint32_t      entropy_buf_cnt;
  uint32_t      zero_copy;
  uint32_t      profile_idc;
  uint32_t      level_idc;
  uint32_t      chroma_mode;
  uint32_t      scan_type;
  uint32_t      low_latency;
  uint32_t      latency_logging;
  uint32_t      splitbuff_mode;
  uint32_t      planar;
  XmaFormatType out_pix_fmt;
  uint32_t      resize_width;
  uint32_t      resize_height;
  int32_t       stream_model;
} XlnxDecoderProperties;

typedef struct XlnxScalerProperties {
  uint64_t      p_mixrate_session;
  int32_t       in_width;
  int32_t       in_height;
  int32_t       fr_num;
  int32_t       fr_den;
  int32_t       bits_per_pixel;
  int32_t       nb_outputs;
  int32_t       out_width[MAX_SCALER_OUTPUTS];
  int32_t       out_height[MAX_SCALER_OUTPUTS];
  char          out_rate[MAX_SCALER_OUTPUTS][SCAL_RATE_STRING_LEN];
  uint32_t      enable_pipeline;
  int32_t       log_level;
  int32_t       latency_logging;
  XmaFormatType xma_fmt_type;
  int32_t       threads;
} XlnxScalerProperties;

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
  int32_t       bits_per_pixel;
  char          enc_preset[15];
  int32_t       bufsize;
  int32_t       no_low_latency_b_frames;
  char          cabr_config[1024];
} XlnxEncoderProperties;

void xlnx_dec_get_xma_props(XmaHandle handle, XlnxDecoderProperties* dec_props, XmaDecoderProperties* xma_dec_props);

void xlnx_scal_get_xma_props(XmaHandle handle, XlnxScalerProperties* scal_props, XmaScalerProperties* xma_scal_props);

int32_t xlnx_enc_get_xma_props(XmaHandle handle, XlnxEncoderProperties* enc_props, XmaEncoderProperties* xma_enc_props);

void xlnx_dec_free_xma_props(XmaDecoderProperties* xma_dec_props);

void xlnx_scal_free_xma_props(XmaScalerProperties* xma_scal_props);

void xlnx_enc_free_xma_props(XmaEncoderProperties* xma_enc_props);

void xlnx_la_free_xma_props(XmaFilterProperties* xma_la_props);

#endif // _XLNX_TRANSCODER_XMA_PROPS_H_
