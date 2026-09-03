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

#ifndef _XLNX_SCAL_ARG_PARSE_H_
#define _XLNX_SCAL_ARG_PARSE_H_

#include <getopt.h>
#include <inttypes.h>
#include <math.h>
#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <xma.h>

#include "xlnx_app_utils.h"
#include "xlnx_scal_common.h"

#define HEVC_CODEC_OPTION "mpsoc_vcu_hevc"
#define AVC_CODEC_OPTION "mpsoc_vcu_avc"

#define FLAG_HELP "help"
#define FLAG_LOG_LEVEL "log_level"
#define FLAG_LOG_LOCATION "log_location"
#define FLAG_LOG_FILE "log_file"
#define FLAG_DEVICE_ID "d"
#define FLAG_STREAM_LOOP "stream_loop"
#define FLAG_WIDTH "w"
#define FLAG_HEIGHT "h"
#define FLAG_PIX_FMT "pix_fmt"
#define FLAG_INPUT_FILE "i"
#define FLAG_MIXRATE "rate"
#define FLAG_LATENCY_LOGGING "latency_logging"
#define FLAG_FRAMES "frames"
#define FLAG_FPS "fps"
#define FLAG_OUTPUT_FILE "o"

typedef enum {
  HELP_ARG = 0,
  LOG_LEVEL_ARG,
  LOG_LOCATION_ARG,
  LOG_FILE_ARG,
  DEVICE_ID_ARG,
  LOOP_COUNT_ARG,
  WIDTH_ARG,
  HEIGHT_ARG,
  PIX_FMT_ARG,
  INPUT_FILE_ARG,
  MIXRATE_ARG,
  LATENCY_LOGGING_ARG,
  IDR_PERIOD_ARG,
  COEF_LOAD_ARG,
  NUM_FRAMES_ARG,
  FPS_ARG,
  OUTPUT_FILE_ARG
} abr_argument_identifiers;

typedef struct XlnxScalOutArgs {
  int           width;       /* -w           */
  int           height;      /* -h           */
  bool          is_halfrate; /* -rate        */
  char*         output_file; /* -o           */
  XmaFormatType pix_fmt;     /* -pix_fmt     */
  int           coeff_load;  /* -coeff_load  */
} XlnxScalOutArgs;

typedef struct XlnxScalArguments {
  XmaLogLevelType log_level;                        /* -log                    */
  XmaLogType      log_location;                     /* -log-location */
  char*           log_file;                         /* -log-file */
  uint            device_id;                        /* -d                      */
  int             loop_count;                       /* -stream_loop            */
  int             input_width;                      /* -w                      */
  int             input_height;                     /* -h                      */
  int             fps_num;                          /* -fps                    */
  int             fps_den;                          /* inferred */
  XmaFormatType   input_pix_fmt;                    /* -pix_fmt                */
  char*           input_file;                       /* -i                      */
  int             latency_logging;                  /* latency_logging         */
  size_t          num_frames;                       /* -frames                 */
  int             num_fullrate_outputs;             /* inferred */
  int             num_halfrate_outputs;             /* inferred */
  int             outputs_used;                     /* inferred */
  XlnxScalOutArgs out_arg_list[MAX_SCALER_OUTPUTS]; /* Various args */
} XlnxScalArguments;

/**
 * xlnx_scal_get_arguments: Parses the commandline arguments into a struct
 * corresponding to scaler arguments
 * @param argc: The number of arguments
 * @param argv: The arguments
 * @return An argument struct containing scaler arguments.
 */
XlnxScalArguments xlnx_scal_get_arguments(int argc, char* argv[]);

#endif // XLNX_ARG_PARSE_H
