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

#include "xlnx_transcoder_parser.h"
#include "xrm_dec_interface.h"

static struct option transcode_options[] = {{FLAG_TRANSCODE_NO_OF_STREAMS, required_argument, 0, NO_OF_STREAMS},
    {FLAG_TRANSCODE_DEVICE_ID, required_argument, 0, TRANSCODE_DEVICE_ID_ARG}, {FLAG_TRANSCODE_STREAM_LOOP, required_argument, 0, TRANSCODE_STREAM_LOOP_ARG},
    {FLAG_TRANSCODE_NUM_FRAMES, required_argument, 0, TRANSCODE_NUM_FRAMES_ARG}, {FLAG_TRANSCODE_GENERIC_MAX, required_argument, 0, TRANSCODE_GENERIC_MAX_ARG},
    {FLAG_LOG_LEVEL, required_argument, 0, LOG_LEVEL_ARG}, {FLAG_LOG_LOCATION, required_argument, 0, LOG_LOCATION_ARG},
    {FLAG_LOG_FILE, required_argument, 0, LOG_FILE_ARG}, {0, 0, 0, 0}};

/**
 * xlnx_tran_get_help: Prints the list of supported arguments for transcoder
 * application
 *
 * @return List of supported arguments
 */
char* xlnx_tran_get_help() {
  const char* usage =
      " XMA Transcoder App Usage: \n\t"
      "./program [generic options] -c:v <decoder codec> [decoder options] -i input-file -scaler_ma -outputs [num] [Scaler options] -c:v <encoder codec> [encoder "
      "options] -o <output-file> -c:v <encoder codec> [encoder options] -o <output-file> -c:v <encoder codec> [encoder options] -o <output-file>..... \n\n";
  const char* arguments = \
        "Arguments:\n\n"
        "\t--help                     Print this message and exit.\n"
        "\t-streams <no_of_streams>   Number of input video streams. Currently only 1 is supported \n"
        "\t-d <device-id>             Specify a device on which the transcoder to run. Default: 0\n"
        "\t-stream_loop <loop-count>  Number of times to loop the input file \n"
        "\t-frames <frame-count>      Number of input frames to be processed \n\n"
        "Decoder options:\n\n"
        "\t-c:v <codec>               Decoder codec to be used. Supported are hevc_ama, h264_ama \n"
        "\t-low_latency <0/1>         Low latency decoding (source must not have B-Frames) Default disabled\n"
        "\t-width <downscale width>   Width to downscale to.\n"
        "\t-height <downscale height> Height to downscale to.\n"
        "\t-latency_logging <0/1>     Latency logging for decoder. Default disabled \n"
        "\t-push-model <0/1>          Decoder streaming model (pull or push). Default: pull (can be faster than real-time) \n"
        "\t-i <input-file>            Name and path of input H.264/HEVC file \n"
        "\t                           \n"
        "Scaler options:\n\n"
        "\t-scaler_ma                 Name of the ABR scaler filter \n"
        "\t-num-output <value>        Number of output files from scaler \n"
        "\t-out_1_width <width>       Width of the scaler output channel 1 \n"
        "\t-out_1_height <height>     Height of the scaler output channel 1 \n"
        "\t-out_1_rate <full/half>    Full of Half rate for output channel 1 \n"
        "\t-out_2_width <width>       Width of the scaler output channel 2 \n"
        "\t-out_2_height <height>     Height of the scaler output channel 2 \n"
        "\t-out_2_rate <full/half>    Full of Half rate for output channel 2 \n"
        "\t-out_3_width <width>       Width of the scaler output channel 3 \n"
        "\t-out_3_height <height>     Height of the scaler output channel 3 \n"
        "\t-out_3_rate <full/half>    Full of Half rate for output channel 3 \n"
        "\t-out_4_width <width>       Width of the scaler output channel 4 \n"
        "\t-out_4_height <height>     Height of the scaler output channel 4 \n"
        "\t-out_4_rate <full/half>    Full of Half rate for output channel 4 \n"
        "\t-out_5_width <width>       Width of the scaler output channel 5 \n"
        "\t-out_5_height <height>     Height of the scaler output channel 5 \n"
        "\t-out_5_rate <full/half>    Full of Half rate for output channel 5 \n"
        "\t-out_6_width <width>       Width of the scaler output channel 6 \n"
        "\t-out_6_height <height>     Height of the scaler output channel 6 \n"
        "\t-out_6_rate <full/half>    Full of Half rate for output channel 6 \n"
        "\t-out_7_width <width>       Width of the scaler output channel 7 \n"
        "\t-out_7_height <height>     Height of the scaler output channel 7 \n"
        "\t-out_7_rate <full/half>    Full of Half rate for output channel 7 \n"
        "\t-out_8_width <width>       Width of the scaler output channel 8 \n"
        "\t-out_8_height <height>     Height of the scaler output channel 8 \n"
        "\t-out_8_rate <full/half>    Full of Half rate for output channel 8 \n"
        "\t-latency_logging <0/1>     Latency logging for scaler. Default: 0 (disabled) \n"
        "Encoder options:\n\n"
        "\t-c:v <codec>               Encoder codec to be used. Supported are hevc_ama, h264_ama, and av1_ama\n"
        "\t-device_type <1/2>         AV1 encoder type. Default is 1. \n"
        "\t-b:v <bitrate>             Bitrate can be given in Kbps or Mbps or bits i.e., 5000000, 5000K, 5M. Default: " STRINGIFY(ENC_DEFAULT_BITRATE) "kbps \n"
        "\t-fps <fps>                 Input frame rate. Default: " STRINGIFY(ENC_DEFAULT_FRAMERATE) "\n"
        "\t-g <intraperiod>           Intra period. Default is " STRINGIFY(ENC_DEFAULT_GOP_SIZE) "\n"
        "\t-max-bitrate <bitrate>     Maximum bit rate. Supported [" STRINGIFY(ENC_MIN_MAX_BITRATE) ", " STRINGIFY(ENC_MAX_MAX_BITRATE) " Default: " STRINGIFY(ENC_DEFAULT_MAX_BITRATE) "\n"
        "\t-min_qp <qp>               Minimum QP. Supported are " STRINGIFY(ENC_MIN_MIN_QP)" to " STRINGIFY(ENC_MAX_MIN_QP) ". Default: " STRINGIFY(ENC_DEFAULT_MIN_QP) "\n"
        "\t-max_qp <qp>               Maximum QP. Supported values are " STRINGIFY(ENC_MIN_MAX_QP) " to " STRINGIFY(ENC_MAX_MAX_QP) ". Default: " STRINGIFY(ENC_DEFAULT_MAX_QP) "\n"
        "\t-spatial_aq_gain <gain>    Spatial AQ gain. Supported values are [" STRINGIFY(ENC_SUPPORTED_MIN_AQ_GAIN) ", 100] or " STRINGIFY(ENC_SUPPORTED_MAX_AQ_GAIN) ". Default: " STRINGIFY(ENC_SPATIAL_AQ_GAIN_DEFAULT) "\n"
        "\t-temporal_aq_gain <gain>   Temporal AQ gain. Supported values are [" STRINGIFY(ENC_SUPPORTED_MIN_AQ_GAIN) ", " STRINGIFY(ENC_MAX_SPAT_AQ_GAIN) "], or " STRINGIFY(ENC_SUPPORTED_MAX_AQ_GAIN) ". Default: " STRINGIFY(ENC_TEMPORAL_AQ_GAIN_DEFAULT) "\n"
        "\t-bf <frames>               Number of B frames. Supported [" STRINGIFY(ENC_MIN_NUM_B_FRAMES) ", " STRINGIFY(ENC_MAX_NUM_B_FRAMES) "] or [" STRINGIFY(ENC_MIN_NUM_B_FRAMES) ", " STRINGIFY(ENC_MAX_NUM_B_FRAMES_AV1) "] Default: " STRINGIFY(ENC_DEFAULT_NUM_B_FRAMES) ". \n"
        "\t-cores <value>             Cores decide if it's 1 or 2-slice encoding. Supported values are 1 and 2 \n"
        "\t-qp <value>                Quantization parameter [" STRINGIFY(ENC_SUPPORTED_MIN_QP) ", " STRINGIFY(ENC_SUPPORTED_MAX_QP) "]. Default: " STRINGIFY(ENC_DEFAULT_QP) "\n"
        "\t-force_idr <0/1>           Supported values are 0 and 1\n"
        "\t-preset <value>            Encoder preset. Supported: slow, medium and fast. Default: medium\n"
        "\t-profile <value>           Encoder profile\n"
        "\t           For HEVC, supported are -1 (auto), 100 (main), 101 (ENC_HEVC_MAIN_INTRA) 102 (main10), 103 (main10_intra). Default: " STRINGIFY(ENC_PROFILE_DEFAULT) " (auto)\n"
        "\t           For H264, supported are -1 (auto), 0 (baseline), 1 (main), 2 (high), 3 (high10) 4 (high10_intra). Default: " STRINGIFY(ENC_PROFILE_DEFAULT) " (auto)\n"
        "\t           For AV1, supported are -1 (auto), 0 (main). Default: " STRINGIFY(ENC_PROFILE_DEFAULT) "\n"
        "\t-level <value>             Encoder level.\n"
        "\t           For H264, supported are 0 (auto), 10, 11, 12, 13, 20, 21, 22, 30, 31, 32, 40, 41, 42, 50, 51, 52, 60, 61, 62. Default: " STRINGIFY(ENC_DEFAULT_LEVEL) "\n"
        "\t           For HEVC, supported are 0 (auto), 10, 20, 21, 30, 31, 40, 41, 50, 51, 52, 60, 61, 62. Default: " STRINGIFY(ENC_DEFAULT_LEVEL) "\n"
        "\t           For AV1, supported are 0 (auto), 20, 21, 30, 31, 40, 41, 50, 51, 52, 53, 60, 61, 62, 63. Default: " STRINGIFY(ENC_DEFAULT_LEVEL) "\n"
        "\t-qp_mode <qp>              QP Mode. Supported values 0 (auto), 1 (relative load) and 2 (uniform). Default: 0\n"
        "\t-control_rate <rc_mode>    Rate Control. Supported values -1 to 3 -1 (auto), 0 (const_qp), 1 (cbr), 2 (vbr), 3 (cvbr), 4 (cabr : *DEPRECATED*) and 5 (*RESERVED*). Default: -1 (auto)\n"
        "\t-lookahead_depth <value>   Lookahead depth. Supported [" STRINGIFY(ENC_MIN_LOOKAHEAD_DEPTH) ", " STRINGIFY(ENC_MAX_LOOKAHEAD_DEPTH)"]. Default: " STRINGIFY(ENC_DEFAULT_LOOKAHEAD_DEPTH) "\n"
        "\t-latency_ms <value>        Lookahead depth specified in milliseconds. Supported [" STRINGIFY(ENC_MIN_LATENCY_MS) ", " STRINGIFY(ENC_MAX_LATENCY_MS) "]. Default: " STRINGIFY(ENC_DEFAULT_LATENCY_MS) "\n"
        "\t-no_bll <value>            No low latency b-frames. Supported [" STRINGIFY(ENC_NO_LOWLAT_BFRAMES_DEFAULT) ", " STRINGIFY(ENC_NO_LOWLAT_BFRAMES_ENABLE) "]. Default: " STRINGIFY(ENC_NO_LOWLAT_BFRAMES_DEFAULT) "\n"
        "\t-temporal_aq <0/1>         Temporal AQ. Enable/Disable. Default: " STRINGIFY(ENC_DEFAULT_TEMPORAL_AQ) "\n"
        "\t-spatial_aq <0/1>          Spatial AQ. Enable/Disable. Default: " STRINGIFY(ENC_DEFAULT_SPATIAL_AQ) "\n"
        "\t-tune_metrics <value>      Tunes VQ for objective metrics [" STRINGIFY(ENC_MIN_TUNE_METRICS) ", " STRINGIFY(ENC_MAX_TUNE_METRICS) "] 1 (vq) 2 (psnr) 3 (ssim) 4 (vmaf) Default: " STRINGIFY(ENC_DEFAULT_TUNE_METRICS) " (vq)\n"
        "\t-tier <value>              HEVC tier, supported are -1 (auto), 0 (main), 1 (high). Default: -1 (auto)\n"
        "\t-crf <value>               CRF 0 Supported : -1 to 63. Default: " STRINGIFY(ENC_CRF_DEFAULT) "\n"
        "\t-cabr <value>              Content adptive bit rate. parameter impacting Rate Control. Default is auto. Supported values auto, disable, off, enable, on, vq_offset=<value> range from -63 to 63  \n"
        "\t-bufsize <value>           Size of VBV buffer (in bits). Default is -1. Strict ULL = 0, Relaxed ULL > 0 \n"
        "\t-dynamic_gop <value>       Dynamic GOP supported values are " STRINGIFY(ENC_DYNAMIC_GOP_AUTO) " (auto), " STRINGIFY(ENC_DYNAMIC_GOP_DISABLE) " (disable) and " STRINGIFY(ENC_DYNAMIC_GOP_ENABLE) " (enable). Default: " STRINGIFY(ENC_DYNAMIC_GOP_DEFAULT) "\n"
        "\t-expert_options <string>   Expert options\n"
        "\t-latency_logging <0/1>     Enable latency logging\n"
        "\t-o <file>                  File to which output is written.\n";

  int          arg_length      = strlen(arguments);
  int          help_msg_length = strlen(usage) + arg_length + 1;
  const char*  newline_start   = "\t                           ";
  int8_t       newline_length  = strlen(newline_start);
  const int8_t column_length   = 75;
  help_msg_length += (help_msg_length / newline_length + 1) * newline_length; // Account for newlines to be inserted
  char* help_msg = calloc(1, help_msg_length);
  if (help_msg == NULL) {
    printf("ERROR: Unable to allocate %d bytes to get help message!", help_msg_length);
    return NULL;
  }
  strcpy(help_msg, usage);

  int8_t column_index   = 0;
  int    help_msg_index = strlen(usage);
  for (int arg_index = 0; arg_index < arg_length; arg_index++) {
    if (column_index > column_length && arguments[arg_index] == ' ') {
      help_msg[help_msg_index] = '\n';
      strcat(help_msg, newline_start);
      help_msg_index += newline_length;
      column_index = newline_length;
    } else {
      help_msg[help_msg_index] = arguments[arg_index];
      column_index++;
      if (arguments[arg_index] == '\n') {
        column_index = 0;
      }
    }
    help_msg_index++;
  }
  return help_msg;
}

/**
 * xlnx_tran_parse_args: Function to parse command line arguments
 *
 * @param argc: Number of arguments
 * @param argv: Pointer to the arguments
 * @param transcode_ctx: Transcoder context
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
static int32_t xlnx_tran_parse_args(int32_t argc, char* argv[], XlnxTranscoderCtx* transcode_ctx, uint32_t stream_no) {
  int32_t ret  = XMA_APP_SUCCESS;
  int32_t flag = 0;
  int32_t option_index;
  int32_t i;
  optind = 1;
  int32_t temp;
  flag = getopt_long_only(argc, argv, "", transcode_options, &option_index);
  if (flag == -1) {
    return XMA_APP_ERROR;
  }
  while (flag != TRANSCODE_GENERIC_MAX_ARG) {
    switch (flag) {
    case NO_OF_STREAMS:
      ret = xlnx_utils_set_int_arg(&transcode_ctx->no_of_streams, optarg, FLAG_TRANSCODE_STREAM_LOOP);
      break;
    case TRANSCODE_DEVICE_ID_ARG:
      int cmdline_dev_id = 0;
      ret                = xlnx_utils_set_int_arg(&cmdline_dev_id, optarg, FLAG_TRANSCODE_DEVICE_ID);
      if (transcode_ctx->trans_handle.dev_index != cmdline_dev_id && transcode_ctx->trans_handle.dev_index != 0) {
        fprintf(stderr, "Device ID %d specified on commandline, but xrm reserve id evaluated device id %d!\n", cmdline_dev_id, transcode_ctx->trans_handle.dev_index);
        ret = XMA_APP_ERROR;
      } else {
        transcode_ctx->trans_handle.dev_index = cmdline_dev_id;
      }
      if ((transcode_ctx->trans_handle.dev_index < 0) || (transcode_ctx->trans_handle.dev_index > 15)) {
        printf("Unsupported device ID %d\n", transcode_ctx->trans_handle.dev_index);
        return XMA_APP_ERROR;
      }
      break;
    case LOG_LEVEL_ARG:
      ret                                   = xlnx_utils_set_int_arg(&temp, optarg, "");
      transcode_ctx->trans_handle.log_level = (XmaLogLevelType) temp;
      if (ret == XMA_APP_ERROR) {
        ret = xlnx_utils_key_from_string(param_log_level_lookup, optarg, PARAM_LOG_LEVEL_KEYS);
        if (ret == XMA_APP_ERROR) {
          if (optarg) {
            printf("Invalid log level %s\n", optarg);
          } else {
            printf("Invalid log level");
          }
        } else {
          transcode_ctx->trans_handle.log_level = ret;
          ret                                   = XMA_APP_SUCCESS;
        }
      }
      break;
    case LOG_LOCATION_ARG:
      ret                                      = xlnx_utils_set_int_arg(&temp, optarg, "");
      transcode_ctx->trans_handle.log_location = (XmaLogType) temp;
      if (ret == XMA_APP_ERROR) {
        ret = xlnx_utils_key_from_string(param_log_location_lookup, optarg, PARAM_LOG_LOCATION_KEYS);
        if (ret == XMA_APP_ERROR) {
          if (optarg) {
            printf("Invalid log location value %s\n", optarg);
          } else {
            printf("Invalid log location");
          }
        } else {
          transcode_ctx->trans_handle.log_location = ret;
          ret                                      = XMA_APP_SUCCESS;
        }
      }
      break;
    case LOG_FILE_ARG:
      transcode_ctx->trans_handle.log_file = optarg;
      ret                                  = XMA_APP_SUCCESS;
      break;
    case TRANSCODE_STREAM_LOOP_ARG:
      ret = xlnx_utils_set_int_arg(&transcode_ctx->loop_count, optarg, FLAG_TRANSCODE_STREAM_LOOP);
      break;

    case TRANSCODE_NUM_FRAMES_ARG:
      ret = xlnx_utils_set_size_t_arg(&transcode_ctx->num_frames, optarg, FLAG_TRANSCODE_NUM_FRAMES);
      break;

    default:
      printf("Error in parsing generic transcoder arguments %d\n", flag);
      return XMA_APP_ERROR;
    }

    flag = getopt_long_only(argc, argv, "", transcode_options, &option_index);
    if (flag == -1 || ret == XMA_APP_ERROR) {
      return XMA_APP_ERROR;
    }
  }

  if ((ret = xlnx_dec_parse_args(argc, argv, &transcode_ctx->dec_ctx, flag)) != XMA_APP_SUCCESS) {
    return XMA_APP_ERROR;
  }

  flag = 0;
  if ((ret = xlnx_scal_parse_args(argc, argv, &transcode_ctx->scal_ctx, &flag)) != XMA_APP_SUCCESS) {
    return XMA_APP_ERROR;
  }

  transcode_ctx->num_scal_out     = transcode_ctx->scal_ctx.scal_props.nb_outputs;
  transcode_ctx->num_enc_channels = (transcode_ctx->num_scal_out + 1);

  for (i = 0; i < transcode_ctx->num_enc_channels; i++) {
    if ((ret = xlnx_enc_parse_args(argc, argv, &transcode_ctx->enc_ctx[i], flag, stream_no, transcode_ctx->no_of_streams)) <= XMA_APP_ERROR) {
      printf("Error in parsing encoder arguments %d\n", flag);
      return XMA_APP_ERROR;
    } else if (ret & TRANSCODE_PARSING_DONE) {
      break;
    }
    flag = 0;
  }
  transcode_ctx->num_enc_channels  = i;
  transcode_ctx->non_scal_channels = (transcode_ctx->num_enc_channels - transcode_ctx->num_scal_out);

  return ret;
}

/**
 * xlnx_tran_update_props: Function for updating transcoder properties
 *
 * @param transcode_ctx: Transcoder context
 * @param transcode_props: Transcoder properties
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
static int32_t xlnx_tran_update_props(XlnxTranscoderCtx* transcode_ctx, XlnxTranscoderProperties* transcode_props) {
  XlnxScalerProperties* scal_props = &transcode_ctx->scal_ctx.scal_props;
  int32_t               ret        = xlnx_dec_update_props(&transcode_ctx->dec_ctx, &transcode_props->xma_dec_props, transcode_ctx->handle);
  if (ret != XMA_APP_SUCCESS) {
    return XMA_APP_ERROR;
  }

  if (transcode_ctx->num_scal_out) {
    /* Update scaler width, height and fps from decoder context */
    if (transcode_ctx->dec_ctx.dec_props.resize_width && transcode_ctx->dec_ctx.dec_props.resize_height) {
      scal_props->in_width  = transcode_ctx->dec_ctx.dec_props.resize_width;
      scal_props->in_height = transcode_ctx->dec_ctx.dec_props.resize_height;
    } else {
      scal_props->in_width  = transcode_ctx->dec_ctx.frame_data.width;
      scal_props->in_height = transcode_ctx->dec_ctx.frame_data.height;
    }
    scal_props->fr_num         = transcode_ctx->dec_ctx.frame_data.fr_num;
    scal_props->fr_den         = transcode_ctx->dec_ctx.frame_data.fr_den;
    scal_props->bits_per_pixel = transcode_ctx->dec_ctx.frame_data.luma_bit_depth;
    scal_props->xma_fmt_type   = transcode_ctx->dec_ctx.dec_props.out_pix_fmt;

    xlnx_scal_update_props(&transcode_ctx->scal_ctx, &transcode_props->xma_scal_props, transcode_ctx->handle);
  }

  transcode_ctx->num_scal_fullrate = transcode_ctx->scal_ctx.session_nb_outputs[SCAL_SESSION_FULL_RATE];

  for (int32_t i = 0; i < transcode_ctx->num_enc_channels; i++) {

    XlnxEncoderProperties* enc_props = &transcode_ctx->enc_ctx[i].enc_props;

    enc_props->bits_per_pixel = transcode_ctx->dec_ctx.frame_data.luma_bit_depth;
    enc_props->pix_fmt        = transcode_ctx->dec_ctx.dec_props.out_pix_fmt;

    if ((i == 0) && transcode_ctx->non_scal_channels) {
      if (transcode_ctx->dec_ctx.dec_props.resize_width && transcode_ctx->dec_ctx.dec_props.resize_height) {
        enc_props->width  = transcode_ctx->dec_ctx.dec_props.resize_width;
        enc_props->height = transcode_ctx->dec_ctx.dec_props.resize_height;
      } else {
        enc_props->width  = transcode_ctx->dec_ctx.frame_data.width;
        enc_props->height = transcode_ctx->dec_ctx.frame_data.height;
      }
    } else {
      int32_t buf_idx   = i - transcode_ctx->non_scal_channels;
      enc_props->width  = scal_props->out_width[buf_idx];
      enc_props->height = scal_props->out_height[buf_idx];
    }
    if (enc_props->fps == ENC_DEFAULT_FRAMERATE) {
      if (i < (transcode_ctx->num_scal_fullrate + transcode_ctx->non_scal_channels)) {
        enc_props->fps = (transcode_ctx->dec_ctx.frame_data.fr_num / transcode_ctx->dec_ctx.frame_data.fr_den);
      } else {
        enc_props->fps = (scal_props->fr_num / scal_props->fr_den);
      }
    }

    if ((xlnx_enc_update_props(&transcode_ctx->enc_ctx[i], &transcode_props->xma_enc_props[i], transcode_ctx->handle)) == XMA_APP_ERROR) {
      return XMA_APP_ERROR;
    }
  }

  return XMA_APP_SUCCESS;
}

/**
 * xlnx_tran_context_init: Initialize transcoder context params to default
 *
 * @param transcode_ctx: Transcoder context
 */
static void xlnx_tran_context_init(XlnxTranscoderCtx* transcode_ctx, uint32_t stream_no, int no_of_streams) {
  transcode_ctx->loop_count                       = 0;
  transcode_ctx->trans_handle.dev_index           = xrm_interface_get_dev_index(); // Check xrm reserve IDs
  transcode_ctx->trans_handle.log_level           = TRANS_DEFAULT_LOG_LEVEL;
  transcode_ctx->trans_handle.log_location        = TRANS_DEFAULT_LOG_LOCATION;
  transcode_ctx->trans_handle.log_file            = TRANS_DEFAULT_LOG_FILE;
  transcode_ctx->trans_handle.log                 = 0;
  transcode_ctx->enc_chan_idx                     = 0;
  transcode_ctx->num_frames                       = SIZE_MAX;
  transcode_ctx->flush_mode                       = 0;
  transcode_ctx->eos_count                        = 0;
  transcode_ctx->out_frame_cnt                    = 0;
  transcode_ctx->in_frame_cnt                     = 0;
  transcode_ctx->transcoder_state                 = TRANSCODE_DEC_READ_FRAME;
  transcode_ctx->num_scal_fullrate                = 0;
  transcode_ctx->num_scal_out                     = 0;
  transcode_ctx->num_enc_channels                 = 0;
  transcode_ctx->curr_sess_channels               = 0;
  transcode_ctx->non_scal_channels                = 0;
  transcode_ctx->trans_que.dec_que_frame_ptr      = 0;
  transcode_ctx->trans_que.scal_in_que_frame_ptr  = 0;
  transcode_ctx->trans_que.scal_out_que_frame_ptr = 0;
  transcode_ctx->trans_que.enc_que_frame_ptr      = 0;
  transcode_ctx->trans_que.la_in_index            = 0;
  transcode_ctx->trans_que.dec_out_index          = 0;
  transcode_ctx->stream_number                    = stream_no;
  transcode_ctx->no_of_streams                    = no_of_streams;
  transcode_ctx->trans_que.was_q_pressed          = false;
  transcode_ctx->dec_ctx.out_frame                = &transcode_ctx->xma_app_frame[0];
  transcode_ctx->scal_ctx.in_frame                = &transcode_ctx->xma_app_frame[0];
  transcode_ctx->dec_ctx.dec_out_fr_cnt           = 0;
  transcode_ctx->exit_from_threads                = 0;
  transcode_ctx->scal_ctx.exit_from_threads       = 0;
  transcode_ctx->enc_ctx->exit_from_threads       = 0;
  for (int32_t i = 0; i < SCAL_MAX_ABR_CHANNELS; i++) {
    transcode_ctx->scal_ctx.out_frame[i] = &transcode_ctx->xma_app_frame[i + 1];
  }
  xlnx_dec_context_init(&transcode_ctx->dec_ctx);
  xlnx_scal_context_init(&transcode_ctx->scal_ctx);
  for (int32_t i = 0; i < TRANSCODE_MAX_ABR_CHANNELS; i++) {
    xlnx_enc_context_init(&transcode_ctx->enc_ctx[i]);
    transcode_ctx->xma_app_frame[i].data[0].buffer_type = XMA_DEVICE_BUFFER_TYPE;
  }

  return;
}

/**
 * xlnx_tran_parser: Function for argument parsing and initialization
 *
 * @param argc: Number of arguments
 * @param argv: Pointer to the arguments
 * @param transcode_ctx: Transcoder context
 * @param transcode_props: Transcoder properties
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
int32_t xlnx_tran_parser(
    int32_t argc, char* argv[], XlnxTranscoderCtx* transcode_ctx, XlnxTranscoderProperties* transcode_props, uint32_t stream_no, int number_of_streams) {
  /* Initializing the transcoder context with default values */
  xlnx_tran_context_init(transcode_ctx, stream_no, number_of_streams);
  if (xlnx_tran_parse_args(argc, argv, transcode_ctx, stream_no) <= XMA_APP_ERROR) {
    return XMA_APP_ERROR;
  }

  if ((xlnx_dec_parse_frame(&transcode_ctx->dec_ctx)) != XMA_APP_SUCCESS) {
    printf("Error in decoder parser\n");
    return XMA_APP_ERROR;
  }

  if ((xlnx_tran_update_props(transcode_ctx, transcode_props)) == XMA_APP_ERROR) {
    return XMA_APP_ERROR;
  }

  return XMA_APP_SUCCESS;
}
