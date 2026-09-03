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

#ifndef _XLNX_ENCODER_H_
#define _XLNX_ENCODER_H_

#include <dlfcn.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xma.h>

#include "xlnx_app_utils.h"
#include "xlnx_transcoder_constants.h"

#include "xrm_enc_interface.h"

#include "xlnx_transcoder_xma_props.h"

#define FLAG_ENC_CODEC_TYPE "c:v"
#define FLAG_ENC_DEVICE_TYPE "device_type"
#define FLAG_ENC_BITRATE "b:v"
#define FLAG_ENC_FPS "fps"
#define FLAG_ENC_INTRA_PERIOD "g"
#define FLAG_ENC_MIN_QP "min_qp"
#define FLAG_ENC_MAX_QP "max_qp"
#define FLAG_ENC_SPAT_AQ_GAIN "spatial_aq_gain"
#define FLAG_ENC_TEMP_AQ_GAIN "temporal_aq_gain"
#define FLAG_ENC_SPAT_AQ "spatial_aq"
#define FLAG_ENC_TEMP_AQ "temporal_aq"
#define FLAG_ENC_QP_MODE "qp_mode"
#define FLAG_ENC_RC_MODE "control_rate"
#define FLAG_ENC_CRF "crf"
#define FLAG_CABR_CONFIG "cabr"
#define FLAG_ENC_MAX_BITRATE "max_bitrate"
#define FLAG_ENC_FORCE_IDR "forced_idr"
#define FLAG_ENC_NUM_SLICES "slice"
#define FLAG_ENC_NUM_CORES "cores"
#define FLAG_ENC_NUM_BFRAMES "bf"
#define FLAG_ENC_PRESET "preset"
#define FLAG_ENC_PROFILE "profile"
#define FLAG_ENC_LEVEL "level"
#define FLAG_TIER "tier"
#define FLAG_ENC_LOOKAHEAD_DEPTH "lookahead_depth"
#define FLAG_ENC_LATENCY_MS "latency_ms"
#define FLAG_ENC_NO_LOWLAT_BFRAMES "no_bll"
#define FLAG_ENC_QP "qp"
#define FLAG_ENC_LATENCY_LOGGING "latency_logging"
#define FLAG_ENC_BUFSIZE "bufsize"
#define FLAG_ENC_DYNAMIC_GOP "dynamic_gop"
#define FLAG_ENC_EXPERT_OPTIONS "expert_options"
#define FLAG_ENC_TUNE_METRICS "tune_metrics"
#define FLAG_ENC_OUTPUT_FILE "o"

typedef struct XlnxEncoderCtx {
  XmaEncoderSession*    enc_session;
  XmaFrame*             enc_in_frame;
  XmaFrameProperties    device_frame_props;
  XmaDataBuffer*        xma_out_buffer;
  XrmEncodeContextV2    xrm_enc_ctx;
  XlnxEncoderProperties enc_props;
  int32_t               loop_count;
  uint32_t              num_frames;
  int32_t               la_bypass;
  int32_t               flush_frame_sent;
  int32_t               in_file;
  int32_t               out_file;
  XmaLogHandle          log;
  XmaHandle             handle;
  bool                  exit_from_threads;
} XlnxEncoderCtx;

typedef struct {
  char* key;
  int   value;
} XlnxEncProfileLookup;

void xlnx_enc_context_init(XlnxEncoderCtx* enc_ctx);

int32_t xlnx_enc_update_props(XlnxEncoderCtx* enc_ctx, XmaEncoderProperties* xma_enc_props, XmaHandle handle);

int32_t xlnx_enc_parse_args(int32_t argc, char* argv[], XlnxEncoderCtx* enc_ctx, int32_t param_flag, uint32_t stream_no, uint32_t no_of_streams);

int32_t xlnx_enc_session(XlnxEncoderCtx* enc_ctx, XmaEncoderProperties* xma_enc_props);

int32_t xlnx_enc_process_frame(XlnxEncoderCtx* enc_ctx, int32_t* enc_out_size, XmaHandle handle);

int32_t xlnx_enc_deinit(XlnxEncoderCtx* enc_ctx, XmaEncoderProperties* xma_enc_props);
#endif //_XLNX_ENCODER_H_
