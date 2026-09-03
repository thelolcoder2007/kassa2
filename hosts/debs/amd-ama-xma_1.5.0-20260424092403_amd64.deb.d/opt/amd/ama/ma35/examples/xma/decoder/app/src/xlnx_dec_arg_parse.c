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

#include "xlnx_dec_arg_parse.h"

struct option dec_options[] = {{FLAG_HELP, no_argument, 0, HELP_ARG}, {FLAG_LOG_LEVEL, required_argument, 0, LOG_LEVEL_ARG},
    {FLAG_LOG_LOCATION, required_argument, 0, LOG_LOCATION_ARG}, {FLAG_LOG_FILE, required_argument, 0, LOG_FILE_ARG},
    {FLAG_DEVICE_ID, required_argument, 0, DEVICE_ID_ARG}, {FLAG_STREAM_LOOP, required_argument, 0, LOOP_COUNT_ARG},
    {FLAG_INPUT_FILE, required_argument, 0, INPUT_FILE_ARG}, {FLAG_CODEC_TYPE, required_argument, 0, DECODER_ARG},
    {FLAG_LOW_LATENCY, required_argument, 0, LOW_LATENCY_ARG}, {FLAG_LATENCY_LOGGING, required_argument, 0, LATENCY_LOGGING_ARG},
    {FLAG_NUM_FRAMES, required_argument, 0, NUM_FRAMES_ARG}, {FLAG_PIX_FMT, required_argument, 0, PIX_FMT_ARG}, {FLAG_RESIZE_WIDTH, required_argument, 0, WIDTH_ARG},
    {FLAG_RESIZE_HEIGHT, required_argument, 0, HEIGHT_ARG}, {FLAG_OUTPUT_FILE, required_argument, 0, OUTPUT_FILE_ARG}, {0, 0, 0, 0}};

static char* xlnx_dec_get_help(char* program_name) {
  char* help_msg = malloc(4096);
  sprintf(help_msg,
      "This is a standalone xma decoder app. It ingests an h264 or h265\n"
      "encoded file and utilizes hardware acceleration to get the decoded\n"
      "output.\n\n"
      "Usage:\n"
      "\t%s [options] -i <input-file> -c:v <codec-type>\n"
      "\t[codec_options] -o <output-file>\n\n"
      "Arguments:\n"
      "\t--help                     Print this message and exit\n"
      "\t-log_level <value>         Specify the log level\n"
      "\t-log_location <value>      Log location. 0 for none, \n"
      "\t                           1 console (default), 2 syslog, 3 file\n"
      "\t-log_file <file>           Name and path of the log file. Default\n"
      "\t                           ma35_decoder_app.log\n"
      "\t-d <device_id>             Specify a device on which to run.\n"
      "\t                           Default: 0\n\n"
      "Input Arguments:\n\n"
      "\t-stream_loop <loop-count>  Number of times to loop the input\n"
      "\t                           file\n"
      "\t-i <input-file>            Input file to be used\n\n"
      "Codec Arguments:\n\n"
      "\t-c:v <codec>               Specify H264 or H265 decoding.\n"
      "\t                           (h264_ama, hevc_ama)\n"
      "\t-low_latency <0/1>         Low latency decoding (source must not\n"
      "\t                           have B-Frames)\n"
      "\t-latency_logging <0/1>     Log latency information\n"
      "\t-frames <frame-count>      Number of frames to be processed.\n"
      "\t-pix_fmt fmt               The output format (nv12, yuv420p,\n"
      "\t                           yuv420p10le, xv15) Default: nv12 8 bit,\n"
      "\t                           yuv420p10le 10 bit\n"
      "\t-width                     Downscale video to this width\n"
      "\t-height                    Downscale video to this height\n"
      "\t-o <file>                  File to which output is written.\n",
      program_name);
  return help_msg;
}

static void xlnx_dec_set_default_args(XlnxDecoderArguments* dec_args) {
  dec_args->input_file   = "";
  dec_args->loop_count   = 0;
  dec_args->num_frames   = SIZE_MAX;
  dec_args->pix_fmt      = XMA_NV12_FMT_TYPE;
  dec_args->output_file  = "";
  dec_args->log_level    = XMA_WARNING_LOG;
  dec_args->log_location = XMA_LOG_TYPE_CONSOLE;
  dec_args->log_file     = "ma35_decoder_app.log";
  xlnx_dec_set_default_dec_props(&dec_args->dec_props);
}

static void xlnx_dec_validate_arguments(XlnxDecoderArguments* arguments) {
  if (access(arguments->input_file, F_OK) != 0) {
    fprintf(stderr, "INPUT FILE \"%s\" DOES NOT EXIST!\n", arguments->input_file);
    exit(XMA_APP_ERROR);
  }
  if (strcmp(arguments->output_file, "") == 0) {
    fprintf(stderr, "Output file not set!\n");
    exit(XMA_APP_ERROR);
  }
  if (arguments->loop_count < -1) {
    fprintf(stderr,
        "stream_loop value of %d is invalid!"
        " 0 <= stream_loop <= MAX_INT. -1 for infinite loop.\n",
        arguments->loop_count);
    exit(XMA_APP_ERROR);
  }
  /* validates codec arguments in xlnx_dec_validate_dec_props when decoder
    context is created. */
}

static int xlnx_dec_parse_commandline(int argc, char* argv[], XlnxDecoderArguments* arguments) {
  XlnxDecoderProperties* dec_props = &arguments->dec_props;
  int                    flag;
  int                    option_index;
  int                    ret = XMA_APP_SUCCESS;
  int32_t                temp;
  while (1) {
    flag = getopt_long_only(argc, argv, "", dec_options, &option_index);
    if (flag == -1) {
      break;
    }
    switch (flag) {
    case HELP_ARG:; // Need a statement after a label
      char* help_message = xlnx_dec_get_help(argv[0]);
      fprintf(stderr, "%s\n", help_message);
      free(help_message);
      exit(XMA_APP_SUCCESS);
    case LOG_LEVEL_ARG:
      ret                  = xlnx_utils_set_int_arg(&temp, optarg, "");
      arguments->log_level = (XmaLogLevelType) temp;
      if (ret == XMA_APP_ERROR) {
        ret = xlnx_utils_key_from_string(param_log_level_lookup, optarg, PARAM_LOG_LEVEL_KEYS);
        if (ret == XMA_APP_ERROR) {
          if (optarg) {
            fprintf(stderr, "Invalid log level %s\n", optarg);
          } else {
            fprintf(stderr, "Invalid log level");
          }
        } else {
          arguments->log_level = ret;
          ret                  = XMA_APP_SUCCESS;
        }
      }
      break;
    case LOG_LOCATION_ARG:
      ret                     = xlnx_utils_set_int_arg(&temp, optarg, "");
      arguments->log_location = (XmaLogType) temp;
      if (ret == XMA_APP_ERROR) {
        ret = xlnx_utils_key_from_string(param_log_location_lookup, optarg, PARAM_LOG_LOCATION_KEYS);
        if (ret == XMA_APP_ERROR) {
          if (optarg) {
            fprintf(stderr, "Invalid log location value %s\n", optarg);
          } else {
            fprintf(stderr, "Invalid log location");
          }
        } else {
          arguments->log_location = ret;
          ret                     = XMA_APP_SUCCESS;
        }
      }
      break;
    case LOG_FILE_ARG:
      arguments->log_file = optarg;
      ret                 = XMA_APP_SUCCESS;
      break;
    case DEVICE_ID_ARG:
      int cmdline_dev_id = 0;
      ret                = xlnx_utils_set_int_arg(&cmdline_dev_id, optarg, FLAG_DEVICE_ID);
      if (dec_props->device_id != cmdline_dev_id && dec_props->device_id != 0) {
        fprintf(stderr, "Device ID %d specified on commandline, but xrm reserve id evaluated device id %d!\n", cmdline_dev_id, dec_props->device_id);
        ret = XMA_APP_ERROR;
      } else {
        dec_props->device_id = cmdline_dev_id;
      }
      break;
    case LOOP_COUNT_ARG:
      ret = xlnx_utils_set_int_arg(&arguments->loop_count, optarg, FLAG_STREAM_LOOP);
      break;
    case INPUT_FILE_ARG:
      arguments->input_file = optarg;
      break;
    case DECODER_ARG:
      if (strcmp(HEVC_PATTERN_MATCH, optarg) == 0) {
        dec_props->codec_type = HEVC_CODEC_TYPE;
      } else if (strcmp(AVC_PATTERN_MATCH, optarg) == 0) {
        dec_props->codec_type = AVC_CODEC_TYPE;
      } else {
        fprintf(stderr, "ERROR: Unrecognized decoder %s\n", optarg);
        ret = XMA_APP_ERROR;
      }
      break;
    case LOW_LATENCY_ARG:
      ret = xlnx_utils_set_uint_arg(&dec_props->low_latency, optarg, FLAG_LOW_LATENCY);
      break;
    case LATENCY_LOGGING_ARG:
      ret = xlnx_utils_set_uint_arg(&dec_props->latency_logging, optarg, FLAG_LATENCY_LOGGING);
      break;
    case NUM_FRAMES_ARG:
      ret = xlnx_utils_set_size_t_arg(&arguments->num_frames, optarg, FLAG_NUM_FRAMES);
      break;
    case PIX_FMT_ARG:
      ret = xlnx_utils_set_pix_fmt_arg(&arguments->pix_fmt, optarg, FLAG_PIX_FMT);
      break;
    case WIDTH_ARG:
      ret = xlnx_utils_set_uint_arg(&dec_props->resize_width, optarg, FLAG_RESIZE_WIDTH);
      break;
    case HEIGHT_ARG:
      ret = xlnx_utils_set_uint_arg(&dec_props->resize_height, optarg, FLAG_RESIZE_HEIGHT);
      break;
    case OUTPUT_FILE_ARG:
      arguments->output_file = optarg;
      break;
    default:
      fprintf(stderr, "Failed to properly parse commandline. %s \n", argv[optind - 1]);
      ret = XMA_APP_ERROR;
    }
    /* XMA_APP_ERROR should == XMA_APP_ERROR */
    if (ret == XMA_APP_ERROR) {
      exit(ret);
    }
  }
  if (optind < argc) {
    fprintf(stderr, "Failed Unrecognized argument %s.\n", argv[optind]);
    return XMA_APP_ERROR;
  }
  return XMA_APP_SUCCESS;
}

int xlnx_dec_get_arguments(int argc, char* argv[], XlnxDecoderArguments* dec_args) {
  if (argc < 2) {
    char* help_message = xlnx_dec_get_help(argv[0]);
    printf("%s\n", help_message);
    free(help_message);
    exit(XMA_APP_SUCCESS);
  }
  memset(dec_args, 0, sizeof(XlnxDecoderArguments));
  xlnx_dec_set_default_args(dec_args);
  xlnx_dec_parse_commandline(argc, argv, dec_args);
  xlnx_dec_validate_arguments(dec_args);
  return XMA_APP_SUCCESS;
}
