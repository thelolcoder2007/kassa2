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

#ifndef _XLNX_TRANSCODER_PARSER_H_
#define _XLNX_TRANSCODER_PARSER_H_

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xma.h>

#include "xlnx_decoder.h"
#include "xlnx_encoder.h"
#include "xlnx_transcoder.h"
#include "xlnx_transcoder_constants.h"

#define STR(x) #x
#define STRINGIFY(x) STR(x)
#define EXPAND_AND_STRINGIFY(x) STRINGIFY(x)

#define FLAG_TRANSCODE_HELP "help"
#define FLAG_TRANSCODE_DEVICE_ID "d"
#define FLAG_TRANSCODE_STREAM_LOOP "stream_loop"
#define FLAG_TRANSCODE_NUM_FRAMES "frames"
#define FLAG_TRANSCODE_GENERIC_MAX "c:v"
#define FLAG_TRANSCODE_NO_OF_STREAMS "streams"
#define FLAG_LOG_LEVEL "log_level"
#define FLAG_LOG_LOCATION "log_location"
#define FLAG_LOG_FILE "log_file"

char* xlnx_tran_get_help();

int32_t xlnx_tran_parser(int32_t argc, char* argv[], XlnxTranscoderCtx* transcode_ctx, XlnxTranscoderProperties* transcode_props, uint32_t stream_no, int no_of_streams);

#endif // _XLNX_TRANSCODER_PARSER_H_
