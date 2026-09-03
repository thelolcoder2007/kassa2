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

#ifndef _XLNX_ENCODER_ARG_PARSE_H_
#define _XLNX_ENCODER_ARG_PARSE_H_

#include <ctype.h>
#include <getopt.h>

#include "xlnx_encoder.h"

#define STR(x) #x
#define STRINGIFY(x) STR(x)

#define FLAG_HELP "help"
#define FLAG_LOG_LEVEL "log_level"
#define FLAG_LOG_LOCATION "log_location"
#define FLAG_LOG_FILE "log_file"
#define FLAG_DEVICE_ID "d"
#define FLAG_STREAM_LOOP "stream_loop"
#define FLAG_INPUT_FILE "i"
#define FLAG_CODEC_TYPE "c:v"
#define FLAG_DEVICE_TYPE "device_type"
#define FLAG_INPUT_WIDTH "w"
#define FLAG_INPUT_HEIGHT "h"
#define FLAG_INPUT_PIX_FMT "pix_fmt"
#define FLAG_BITRATE "b:v"
#define FLAG_FPS "fps"
#define FLAG_INTRA_PERIOD "g"
#define FLAG_MIN_QP "min_qp"
#define FLAG_MAX_QP "max_qp"
#define FLAG_SPAT_AQ_GAIN "spatial_aq_gain"
#define FLAG_TEMP_AQ_GAIN "temporal_aq_gain"
#define FLAG_SPAT_AQ "spatial_aq"
#define FLAG_TEMP_AQ "temporal_aq"
#define FLAG_QP_MODE "qp_mode"
#define FLAG_RC_MODE "control_rate"
#define FLAG_CRF "crf"
#define FLAG_CABR_CONFIG "cabr"
#define FLAG_MAX_BITRATE "max_bitrate"
#define FLAG_FORCE_IDR "forced_idr"
#define FLAG_NUM_SLICES "slice"
#define FLAG_NUM_CORES "cores"
#define FLAG_NUM_BFRAMES "bf"
#define FLAG_DYN_IDR "force_key_frame"
#define FLAG_PRESET "preset"
#define FLAG_PROFILE "profile"
#define FLAG_LEVEL "level"
#define FLAG_TIER "tier"
#define FLAG_LOOKAHEAD_DEPTH "lookahead_depth"
#define FLAG_LATENCY_MS "latency_ms"
#define FLAG_NO_LOWLAT_BFRAMES "no_bll"
#define FLAG_TUNE_METRICS "tune_metrics"
#define FLAG_LATENCY_LOGGING "latency_logging"
#define FLAG_QP "qp"
#define FLAG_DYNAMIC_GOP "dynamic_gop"
#define FLAG_NUM_FRAMES "frames"
#define FLAG_BUFSIZE "bufsize"
#define FLAG_EXPERT_OPTIONS "expert_options"
#define FLAG_OUTPUT_FILE "o"
#define FLAG_STAT_FILE "stats"
#define FLAG_DYN_PARAM_FILE "dynamic_params_file"

typedef enum {
  HELP_ARG = 0,
  LOG_LEVEL_ARG,
  LOG_LOCATION_ARG,
  LOG_FILE_ARG,
  DEVICE_ID_ARG,
  LOOP_COUNT_ARG,
  INPUT_FILE_ARG,
  ENCODER_ARG,
  DEVICE_TYPE_ARG,
  INPUT_WIDTH_ARG,
  INPUT_HEIGHT_ARG,
  INPUT_PIX_FMT_ARG,
  BITRATE_ARG,
  FPS_ARG,
  INTRA_PERIOD_ARG,
  MIN_QP_ARG,
  MAX_QP_ARG,
  SPAT_AQ_GAIN_ARG,
  TEMP_AQ_GAIN_ARG,
  SPAT_AQ_ARG,
  TEMP_AQ_ARG,
  QP_MODE_ARG,
  RC_MODE_ARG,
  CRF_ARG,
  MAX_BITRATE_ARG,
  FORCE_IDR_ARG,
  NUM_SLICES_ARG,
  NUM_CORES_ARG,
  NUM_BFRAMES_ARG,
  DYNAMIC_IDR_ARG,
  PRESET_ARG,
  PROFILE_ARG,
  LEVEL_ARG,
  TIER_ARG,
  LOOKAHEAD_DEPTH_ARG,
  LATENCY_MS_ARG,
  TUNE_METRICS_ARG,
  LATENCY_LOGGING_ARG,
  QP_ARG,
  DYNAMIC_GOP_ARG,
  NUM_FRAMES_ARG,
  BUFSIZE_ARG,
  EXPERT_OPTIONS_ARG,
  OUTPUT_FILE_ARG,
  STAT_FILE_ARG,
  DYN_PARAM_FILE_ARG,
  NO_LOWLAT_BFRAMES_ARG,
  CABR_CONFIG_ARG
} XlnxEncArgIdentifiers;

/* Encoder Context */
typedef struct XlnxEncoderArguments {
  XlnxEncoderProperties enc_props;
  char*                 input_file;
  XmaLogLevelType       log_level;
  XmaLogType            log_location;
  char*                 log_file;
  XmaFormatType         pix_fmt;
  XlnxDynIdrFrames      dynamic_idr;
  char*                 output_file;
  char*                 stat_file;
  char*                 dyn_param_file;
  size_t                num_frames;
  int32_t               loop_count;
} XlnxEncoderArguments;

int32_t xlnx_enc_get_arguments(int32_t argc, char* argv[], XlnxEncoderArguments* arguments);

#endif // _XLNX_ENCODER_H_
