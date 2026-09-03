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

#ifndef _XLNX_SCALER_H_
#define _XLNX_SCALER_H_

#include <dlfcn.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xma.h>

#include "xrm_scale_interface.h"

#include "xlnx_app_utils.h"
#include "xlnx_transcoder_constants.h"
#include "xlnx_transcoder_xma_props.h"

#define FLAG_SCAL_FILTER "scaler_ma"
#define FLAG_SCAL_ENABLE_PIPELINE "enable_pipeline"
#define FLAG_SCAL_NUM_OUTPUTS "num-outputs"

#define OUTPUT "out_"
#define WIDTH "_width"
#define HEIGHT "_height"
#define RATE "_rate"
#define FLAG_SCAL_OUTPUT_WIDTH(NUM) OUTPUT NUM WIDTH
#define FLAG_SCAL_OUTPUT_HEIGHT(NUM) OUTPUT NUM HEIGHT
#define FLAG_SCAL_OUTPUT_RATE(NUM) OUTPUT NUM RATE
#define FLAG_SCAL_LATENCY_LOGGING "latency-logging"
#define FLAG_SCAL_MAX "c:v"
#define UNKNOWN_RATE_CTR -2

typedef enum { SCAL_SESSION_ALL_RATE = 0, SCAL_SESSION_FULL_RATE, SCAL_MAX_SESSIONS } XlnxScalSessionType;

typedef struct XlnxScalerCtx {
  XmaFrame*            in_frame;
  XmaFrame*            out_frame[MAX_SCALER_OUTPUTS];
  XmaFrameProperties   output_fprops[MAX_SCALER_OUTPUTS];
  XmaSessionHandle     session[SCAL_MAX_SESSIONS];
  XrmScaleContextV2    scaler_xrm_ctx;
  XlnxScalerProperties scal_props;
  int32_t              session_nb_outputs[SCAL_MAX_ABR_CHANNELS];
  int32_t*             copyOutLink;
  int32_t              scaler_enable;
  int32_t              num_sessions;
  int32_t              session_frame;
  int32_t              flush;
  int32_t              send_status;
  int32_t              frames_out;
  size_t               scal_input_cnt;
  size_t               scal_frame_cnt;
  XmaLogHandle         log;
  XmaHandle            handle;
  bool                 exit_from_threads;
} XlnxScalerCtx;

int32_t xlnx_scal_session(XlnxScalerCtx* scal_ctx, XmaScalerProperties* xma_scal_props, XmaHandle handle);

int32_t xlnx_scal_update_props(XlnxScalerCtx* scal_ctx, XmaScalerProperties* xma_scal_props, XmaHandle handle);
int32_t xlnx_scal_parse_args(int32_t argc, char* argv[], XlnxScalerCtx* scal_ctx, int32_t* param_flag);

void xlnx_scal_context_init(XlnxScalerCtx* scal_ctx);

int32_t xlnx_scal_process_frame(XlnxScalerCtx* scal_ctx, XmaHandle handle);
int32_t xlnx_scal_deinit(XlnxScalerCtx* scal_ctx, XmaScalerProperties* xma_scal_props);

#endif //_XLNX_SCALER_H_
