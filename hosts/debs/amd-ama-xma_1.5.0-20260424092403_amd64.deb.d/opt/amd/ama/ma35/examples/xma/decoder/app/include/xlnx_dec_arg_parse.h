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

#ifndef _XLNX_DEC_ARG_PARSE_H_
#define _XLNX_DEC_ARG_PARSE_H_

#include "xlnx_app_utils.h"
#include "xlnx_dec_common.h"
#include "xlnx_dec_xma_props.h"
#include <ctype.h>
#include <getopt.h>

#define HEVC_PATTERN_MATCH "hevc_ama"
#define AVC_PATTERN_MATCH "h264_ama"

#define FLAG_HELP "help"
#define FLAG_LOG_LEVEL "log_level"
#define FLAG_LOG_LOCATION "log_location"
#define FLAG_LOG_FILE "log_file"
#define FLAG_DEVICE_ID "d"
#define FLAG_STREAM_LOOP "stream_loop"
#define FLAG_INPUT_FILE "i"
#define FLAG_CODEC_TYPE "c:v"
#define FLAG_LOW_LATENCY "low_latency"
#define FLAG_LATENCY_LOGGING "latency-logging"
#define FLAG_NUM_FRAMES "frames"
#define FLAG_PIX_FMT "pix_fmt"
#define FLAG_RESIZE_WIDTH "width"
#define FLAG_RESIZE_HEIGHT "height"
#define FLAG_OUTPUT_FILE "o"

typedef enum {
  HELP_ARG = 0,
  LOG_LEVEL_ARG,
  LOG_LOCATION_ARG,
  LOG_FILE_ARG,
  DEVICE_ID_ARG,
  LOOP_COUNT_ARG,
  INPUT_FILE_ARG,
  DECODER_ARG,
  LOW_LATENCY_ARG,
  LATENCY_LOGGING_ARG,
  NUM_FRAMES_ARG,
  PIX_FMT_ARG,
  WIDTH_ARG,
  HEIGHT_ARG,
  OUTPUT_FILE_ARG
} decoder_argument_identifiers;

typedef struct XlnxDecoderArguments {
  XlnxDecoderProperties dec_props;
  char*                 input_file;
  char*                 output_file;
  int                   loop_count;
  XmaFormatType         pix_fmt;
  size_t                num_frames;
  XmaLogLevelType       log_level;    /* -log */
  XmaLogType            log_location; /* -log-location */
  char*                 log_file;
} XlnxDecoderArguments;

/**
 * Parse the commandline into XlnxDecoderArguments
 * @param argc: The number of commandline arguments
 * @param argv: The arguments themselves
 * @return XMA_APP_SUCCESS on success
 */
int xlnx_dec_get_arguments(int argc, char* argv[], XlnxDecoderArguments* dec_args);

#endif //_XLNX_DEC_ARG_PARSE_H_
