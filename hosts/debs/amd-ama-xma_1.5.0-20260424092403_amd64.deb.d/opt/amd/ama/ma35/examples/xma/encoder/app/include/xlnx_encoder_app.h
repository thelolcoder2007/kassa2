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

#ifndef _XLNX_ENCODER_APP_H_
#define _XLNX_ENCODER_APP_H_

#include "xlnx_enc_arg_parse.h"
#include "xlnx_encoder.h"

typedef struct XlnxEncoderAppCtx {
  XmaFrameProperties frame_props;
  XmaFrame*          enc_input_xframe;
  uint32_t           enc_state;
  XlnxEncoderCtx     enc_ctx;
  FILE*              in_file;
  XmaFormatType      pix_fmt;
  FILE*              out_file;
  size_t             num_frames_to_encode;
  int32_t            loop_count;
  XlnxAppTimeTracker timer;
} XlnxEncoderAppCtx;

#endif // _XLNX_ENCODER_APP_H_
