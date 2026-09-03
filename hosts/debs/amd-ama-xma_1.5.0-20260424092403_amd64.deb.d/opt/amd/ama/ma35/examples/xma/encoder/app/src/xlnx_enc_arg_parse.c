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

#include "xlnx_enc_arg_parse.h"
#include "xrm_dec_interface.h"

static struct option enc_options[] = {{FLAG_HELP, no_argument, 0, HELP_ARG}, {FLAG_LOG_LEVEL, required_argument, 0, LOG_LEVEL_ARG},
    {FLAG_LOG_LOCATION, required_argument, 0, LOG_LOCATION_ARG}, {FLAG_LOG_FILE, required_argument, 0, LOG_FILE_ARG},
    {FLAG_DEVICE_ID, required_argument, 0, DEVICE_ID_ARG}, {FLAG_STREAM_LOOP, required_argument, 0, LOOP_COUNT_ARG},
    {FLAG_INPUT_FILE, required_argument, 0, INPUT_FILE_ARG}, {FLAG_CODEC_TYPE, required_argument, 0, ENCODER_ARG},
    {FLAG_DEVICE_TYPE, required_argument, 0, DEVICE_TYPE_ARG}, {FLAG_INPUT_WIDTH, required_argument, 0, INPUT_WIDTH_ARG},
    {FLAG_INPUT_HEIGHT, required_argument, 0, INPUT_HEIGHT_ARG}, {FLAG_INPUT_PIX_FMT, required_argument, 0, INPUT_PIX_FMT_ARG},
    {FLAG_BITRATE, required_argument, 0, BITRATE_ARG}, {FLAG_FPS, required_argument, 0, FPS_ARG}, {FLAG_INTRA_PERIOD, required_argument, 0, INTRA_PERIOD_ARG},
    {FLAG_MIN_QP, required_argument, 0, MIN_QP_ARG}, {FLAG_MAX_QP, required_argument, 0, MAX_QP_ARG}, {FLAG_SPAT_AQ_GAIN, required_argument, 0, SPAT_AQ_GAIN_ARG},
    {FLAG_TEMP_AQ_GAIN, required_argument, 0, TEMP_AQ_GAIN_ARG}, {FLAG_SPAT_AQ, required_argument, 0, SPAT_AQ_ARG}, {FLAG_TEMP_AQ, required_argument, 0, TEMP_AQ_ARG},
    {FLAG_QP_MODE, required_argument, 0, QP_MODE_ARG}, {FLAG_RC_MODE, required_argument, 0, RC_MODE_ARG}, {FLAG_CRF, required_argument, 0, CRF_ARG},
    {FLAG_CABR_CONFIG, required_argument, 0, CABR_CONFIG_ARG}, {FLAG_MAX_BITRATE, required_argument, 0, MAX_BITRATE_ARG},
    {FLAG_FORCE_IDR, required_argument, 0, FORCE_IDR_ARG}, {FLAG_NUM_SLICES, required_argument, 0, NUM_SLICES_ARG}, {FLAG_NUM_CORES, required_argument, 0, NUM_CORES_ARG},
    {FLAG_NUM_BFRAMES, required_argument, 0, NUM_BFRAMES_ARG}, {FLAG_DYN_IDR, required_argument, 0, DYNAMIC_IDR_ARG}, {FLAG_PRESET, required_argument, 0, PRESET_ARG},
    {FLAG_PROFILE, required_argument, 0, PROFILE_ARG}, {FLAG_LEVEL, required_argument, 0, LEVEL_ARG}, {FLAG_TIER, required_argument, 0, TIER_ARG},
    {FLAG_LOOKAHEAD_DEPTH, required_argument, 0, LOOKAHEAD_DEPTH_ARG}, {FLAG_LATENCY_MS, required_argument, 0, LATENCY_MS_ARG},
    {FLAG_NO_LOWLAT_BFRAMES, required_argument, 0, NO_LOWLAT_BFRAMES_ARG}, {FLAG_TUNE_METRICS, required_argument, 0, TUNE_METRICS_ARG},
    {FLAG_LATENCY_LOGGING, required_argument, 0, LATENCY_LOGGING_ARG}, {FLAG_QP, required_argument, 0, QP_ARG}, {FLAG_DYNAMIC_GOP, required_argument, 0, DYNAMIC_GOP_ARG},
    {FLAG_NUM_FRAMES, required_argument, 0, NUM_FRAMES_ARG}, {FLAG_BUFSIZE, required_argument, 0, BUFSIZE_ARG},
    {FLAG_EXPERT_OPTIONS, required_argument, 0, EXPERT_OPTIONS_ARG}, {FLAG_OUTPUT_FILE, required_argument, 0, OUTPUT_FILE_ARG},
    {FLAG_STAT_FILE, required_argument, 0, STAT_FILE_ARG}, {FLAG_DYN_PARAM_FILE, required_argument, 0, DYN_PARAM_FILE_ARG}, {0, 0, 0, 0}};

XlnxParameterLookup enc_param_codec_lookup[] = {{"h264_ama", ENCODER_ID_H264}, {"hevc_ama", ENCODER_ID_HEVC}, {"av1_ama", ENCODER_ID_AV1}};

XlnxParameterLookup enc_param_preset_lookup[] = {{"slow", XMA_ENC_PRESET_SLOW}, {"medium", XMA_ENC_PRESET_MEDIUM}, {"fast", XMA_ENC_PRESET_FAST}};

XlnxParameterLookup enc_param_h264_profile_lookup[] = {{"auto", ENC_PROFILE_AUTO}, {"baseline", ENC_H264_BASELINE}, {"main", ENC_H264_MAIN}, {"high", ENC_H264_HIGH},
    {"high10", ENC_H264_HIGH_10}, {"high10_intra", ENC_H264_HIGH_10}};

XlnxParameterLookup enc_param_hevc_profile_lookup[] = {
    {"auto", ENC_PROFILE_AUTO}, {"main", ENC_HEVC_MAIN}, {"main_intra", ENC_HEVC_MAIN_INTRA}, {"main10", ENC_HEVC_MAIN_10}, {"main10_intra", PROFILE_HEVC_MAIN10_INTRA}};

XlnxParameterLookup enc_param_av1_profile_lookup[] = {{"auto", ENC_PROFILE_AUTO}, {"main", ENC_AV1_MAIN}};

XlnxParameterLookup enc_param_tier_lookup[] = {{"auto", ENC_TIER_AUTO}, {"main", ENC_TIER_MAIN}, {"high", ENC_TIER_HIGH}};

XlnxParameterLookup enc_param_av1_device_type_lookup[] = {{"any", ENCODER_DEVICE_TYPE_ANY}, {"1", ENCODER_DEVICE_TYPE_1}, {"2", ENCODER_DEVICE_TYPE_2}};

XlnxParameterLookup enc_param_tune_metrics_lookup[] = {
    {"vq", XMA_ENC_TUNE_METRICS_VQ}, {"psnr", XMA_ENC_TUNE_METRICS_PSNR}, {"ssim", XMA_ENC_TUNE_METRICS_SSIM}, {"vmaf", XMA_ENC_TUNE_METRICS_VMAF}};

#define ENC_PARAM_CODEC_KEYS (sizeof(enc_param_codec_lookup) / sizeof(XlnxParameterLookup))
#define ENC_PARAM_PRESET_KEYS (sizeof(enc_param_preset_lookup) / sizeof(XlnxParameterLookup))
#define ENC_PARAM_PROF_H264_KEYS (sizeof(enc_param_h264_profile_lookup) / sizeof(XlnxParameterLookup))
#define ENC_PARAM_PROF_HEVC_KEYS (sizeof(enc_param_hevc_profile_lookup) / sizeof(XlnxParameterLookup))
#define ENC_PARAM_TIER_KEYS (sizeof(enc_param_tier_lookup) / sizeof(XlnxParameterLookup))
#define ENC_PARAM_DEVICE_TYPE_AV1_KEYS (sizeof(enc_param_av1_device_type_lookup) / sizeof(XlnxParameterLookup))
#define ENC_PARAM_TUNE_METRICS_KEYS (sizeof(enc_param_tune_metrics_lookup) / sizeof(XlnxParameterLookup))

/**
 * xlnx_enc_get_help: Prints the list of supported arguments for encoder xma
 * application
 *
 * @return List of supported arguments
 */
char* xlnx_enc_get_help() {
  const char* usage =
      " XMA Encoder App Usage:\n\t"
      "./" XLNX_ENC_APP_MODULE
      " [input options] -i input-file -c:v <codec-option> "
      " [encoder options] -o <output-file>\n\n";
  const char* arguments = \
        "Arguments:\n\n"
        "\t--help                     Print this message and exit.\n"
        "\t-log_level <value>         Log level settings. Supported: 0 (emergency), 1 (alert), 2 (critical), 3 (error), 4 (warning), 5 (notice), 6 (info), 7 (debug). Default: 3 (error)\n"
        "\t-log_location <value>      Log location settings. Supported: 0 (none), 1 (console), 2 (syslog), 3 (file). Default: 1 (console)\n"
        "\t-log_file <log-file>       Name and path of log file. Default: ma35_encoder_app.log\n"
        "\t-d <device-id>             Specify a device on which the encoder to run. Default: 0\n"
        "\t-frames <frame-count>      Number of frames to be processed.\n\n"
        "Input options:\n\n"
        "\t-stream_loop <loop-count>  Number of times to loop the input YUV file.\n"
        "\t-w <width>                 Width of YUV input.\n"
        "\t-h <height>                Height of YUV input.\n"
        "\t-pix_fmt <pixel-format>    Pix format of the input file (yuv420p, yuv420p10le). Default: yuv420p\n"
        "\t-i <input-file>            Name and path of input YUV file\n\n"
        "Codec option:\n\n"
        "\t-c:v <codec>               Encoder codec to be used. Supported are h264_ama, hevc_ama, av1_ama\n"
        "\t-device_type <1/2>         AV1 encoder type. Default: 1\n\n"
        "Encoder params:\n\n"
        "\t-b:v <bitrate>             Bitrate can be given in Kbps or Mbps or bits i.e., 5000000, 5000K, 5M. Default: " STRINGIFY(ENC_DEFAULT_BITRATE) "Kbps\n"
        "\t-fps <fps>                 Input frame rate. Default: " STRINGIFY(ENC_DEFAULT_FRAMERATE) "\n"
        "\t-g <intraperiod>           Intra period. Default: " STRINGIFY(ENC_DYNAMIC_GOP_DEFAULT) "\n"
        "\t-qp <qp>                   QP. Supported are [" STRINGIFY(ENC_SUPPORTED_MIN_QP) ", " STRINGIFY(ENC_SUPPORTED_MAX_QP) "] Default:" STRINGIFY(ENC_DEFAULT_QP) "\n"
        "\t-max_qp <qp>               Maximum QP. Supported [" STRINGIFY(ENC_MIN_MAX_QP) ", " STRINGIFY(ENC_MAX_MAX_QP) "]. Default: " STRINGIFY(ENC_DEFAULT_MAX_QP) "\n"
        "\t-min_qp <qp>               Minimum QP. Supported [" STRINGIFY(ENC_MIN_MIN_QP) ", " STRINGIFY(ENC_MAX_MIN_QP) "] Default: " STRINGIFY(ENC_DEFAULT_MIN_QP) "\n"
        "\t-spat-aq-gain <gain>       Spatial AQ gain. Supported [" STRINGIFY(ENC_SUPPORTED_MIN_AQ_GAIN) ", " STRINGIFY(ENC_SUPPORTED_MAX_AQ_GAIN) "]. Default: " STRINGIFY(ENC_SUPPORTED_MAX_AQ_GAIN) " (auto)\n"
        "\t-temp-aq-gain <gain>       Temporal AQ gain. Supported [" STRINGIFY(ENC_SUPPORTED_MIN_AQ_GAIN) ", " STRINGIFY(ENC_SUPPORTED_MAX_AQ_GAIN) "]. Default: " STRINGIFY(ENC_SUPPORTED_MAX_AQ_GAIN) " (auto)\n"
        "\t-temporal_aq <0/1>         Temporal AQ. Enable/Disable. Default: " STRINGIFY(ENC_DEFAULT_TEMPORAL_AQ) "\n"
        "\t-spatial_aq <0/1>          Spatial AQ. Enable/Disable. Default: " STRINGIFY(ENC_DEFAULT_SPATIAL_AQ) "\n"
        "\t-qp_mode <qp>              QP Mode. Supported values 0 (auto), 1 (relative_load) and 2 (uniform) Default: 0 (auto)\n"
        "\t-control_rate <rc_mode>    Rate Control. Supported values -1 (auto), 0 (const qp), 1 (cbr), 2 (vbr), 3 (cvbr), 4 (cabr : *DEPRECATED*) and 5 (*RESERVED*). Default: -1 (auto)\n"
        "\t-cabr <value>              Content adptive bit rate. parameter impacting Rate Control. Default is auto. Supported values auto, disable, off, enable, on, vq_offset=<value> range from -63 to 63  \n"
        "\t-bf <frames>               Number of B frames. Supported [" STRINGIFY(ENC_MIN_NUM_B_FRAMES) ", " STRINGIFY(ENC_MAX_NUM_B_FRAMES) "] or [" STRINGIFY(ENC_MIN_NUM_B_FRAMES) ", " STRINGIFY(ENC_MAX_NUM_B_FRAMES_AV1) "] if AV1. Default: " STRINGIFY(ENC_DEFAULT_NUM_B_FRAMES) "\n"
        "\t-cores <value>             Cores decide if it's 1 or 2-slice encoding. Supported values are 1 and 2 \n"
        "\t-force_idr <0/1>           If forcing key frames, force them as IDR frames Default: 1\n"
        "\t-preset <value>            Encoder preset. Supported: slow, medium and fast are supported. Default: medium\n"
        "\t-profile <value>           Encoder profile.\n"
        "\t           For AV1, supported -1 (auto) and 200 (main). Default: -1 (auto)\n"
        "\t           For HEVC, supported -1 (auto), 100 (main), 101 (main10_intra), 102 (main10), 103 (main10_intra). Default: -1 (auto)\n"
        "\t           For H264, supported -1 (auto), 0 (baseline), 1 (main), 2 (high), 3 (high10) 4 (high10_intra). Default: -1 (auto)\n"
        "\t-level <value>             Encoder level.\n"
        "\t           For H264, supported are 0 (auto), 10, 11, 12, 13, 20, 21, 22, 30, 31, 32, 40, 41, 42, 50, 51, 52, 60, 61, 62. Default: 0 (auto)\n"
        "\t           For HEVC, supported are 0 (auto), 10, 20, 21, 30, 31, 40, 41, 50, 51, 52, 60, 61, 62. Default: 0 (auto)\n"
        "\t           For AV1, supported are 0 (auto), 20, 21, 30, 31, 40, 41, 50, 51, 52, 53, 60, 61, 62, 63. Default: 0 (auto)\n"
        "\t-tier <value>              HEVC tier, supported are -1 (auto), 0 (main), 1 (high). Default: -1 (auto)\n"
        "\t-crf <value>               CRF 0 Supported : -1 to 63. Default: " STRINGIFY(ENC_CRF_DEFAULT) "\n"
        "\t-bufsize <value>           Size of VBV buffer (in bits). Default is -1. Strict ULL = 0, Relaxed ULL > 0 \n"
        "\t-dynamic_gop <value>       Dynamic GOP supported values are " STRINGIFY(ENC_DYNAMIC_GOP_AUTO) " (auto), " STRINGIFY(ENC_DYNAMIC_GOP_DISABLE) " (disable) and " STRINGIFY(ENC_DYNAMIC_GOP_ENABLE) " (enable). Default: " STRINGIFY(ENC_DYNAMIC_GOP_DEFAULT) "\n"
        "\t-tune_metrics <value>      Tunes encoder's video quality for objective metrics. Supported values are vq or 1, psnr or 2, ssim or 3, vmaf or 4. Default: 1\n"
        "\t-lookahead_depth <value>   Lookahead depth. Supported [" STRINGIFY(ENC_MIN_LOOKAHEAD_DEPTH) ", " STRINGIFY(ENC_MAX_LOOKAHEAD_DEPTH) "]. Default: " STRINGIFY(ENC_DEFAULT_LOOKAHEAD_DEPTH) "\n"
        "\t-latency_ms <value>        Lookahead depth specified in milliseconds. Supported [" STRINGIFY(ENC_MIN_LATENCY_MS) ", " STRINGIFY(ENC_MAX_LATENCY_MS) "]. Default: " STRINGIFY(ENC_DEFAULT_LATENCY_MS) "\n"
        "\t-no_bll <value>            No low latency b-frames. Supported [" STRINGIFY(ENC_NO_LOWLAT_BFRAMES_DEFAULT) ", " STRINGIFY(ENC_NO_LOWLAT_BFRAMES_ENABLE) "]. Default: " STRINGIFY(ENC_NO_LOWLAT_BFRAMES_DEFAULT) "\n"
        "\t-latency_logging <0/1>     Enable latency logging\n"
        "\t-expert_options <string>   Expert options\n"
        "\t-o <file>                  File to which output is written.\n"
        "\t-stats <file>              File to which csv statistics output is written.\n";

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
 * @brief Check if the level / codec combo is supported
 * HEVC - 0 10 11 12 13 20 21 22 30 31 32 40 41 42 50 51 52 60 61 62
 * H264 - 0 10          20 21    30 31    40 41    50 51 52 60 61 62
 * AV1  - 0             20 21    30 31    40 41    50 51 52 53 60 61 62 63
 * @param level 
 * @param codec_id 
 * @return int32_t XMA_APP_SUCCESS on success, XMA_APP_ERROR if unsupported
 */
static int32_t xlnx_enc_validate_level_arg(int level, int codec_id) {

  // Common levels
  switch (level) {
  case 0:
  case 20:
  case 21:
  case 30:
  case 31:
  case 40:
  case 41:
  case 50:
  case 51:
  case 52:
  case 60:
  case 61:
  case 62:
    return XMA_APP_SUCCESS;
  default:
    break;
  }
  if (codec_id == ENCODER_ID_H264) {
    switch (level) {
    case 10:
    case 11:
    case 12:
    case 13:
    case 22:
    case 32:
    case 42:
      return XMA_APP_SUCCESS;
    default:
      break;
    }
  }
  if (codec_id == ENCODER_ID_HEVC) {
    switch (level) {
    case 10:
      return XMA_APP_SUCCESS;
    default:
      break;
    }
  }
  if (codec_id == ENCODER_ID_AV1) {
    switch (level) {
    case 53:
    case 63:
      return XMA_APP_SUCCESS;
    default:
      break;
    }
  }
  return XMA_APP_ERROR;
}

/**
 * xlnx_enc_validate_codec_arguments: Validates encoder codec arguments
 *
 * @param enc_props: Encoder properties
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
static int32_t xlnx_enc_validate_codec_arguments(XlnxEncoderProperties* enc_props) {
  /* FFmpeg set default bitrate to 200K so making xma apps aligned to it */
  if ((enc_props->bitrate <= 0) && enc_props->qp <= ENC_DEFAULT_QP) {
    enc_props->bitrate = ENC_DEFAULT_BITRATE;
  }

  if ((enc_props->fps <= 0) || (enc_props->fps > INT_MAX)) {
    printf("Invalid frame rate %d\n", enc_props->fps);
    return XMA_APP_ERROR;
  }

  if ((enc_props->gop_size < ENC_MIN_GOP_SIZE) || (enc_props->gop_size > ENC_MAX_GOP_SIZE)) {
    printf("Invalid intra period %d\n", enc_props->gop_size);
    return XMA_APP_ERROR;
  }

  if ((enc_props->min_qp < ENC_SUPPORTED_MIN_QP) || (enc_props->min_qp > ENC_SUPPORTED_MAX_QP)) {
    printf("Invalid min qp %d\n", enc_props->min_qp);
    return XMA_APP_ERROR;
  }

  if ((enc_props->max_qp < ENC_SUPPORTED_MIN_QP) || (enc_props->max_qp > ENC_SUPPORTED_MAX_QP)) {
    printf("Invalid max qp %d\n", enc_props->max_qp);
    return XMA_APP_ERROR;
  }

  if (((enc_props->temp_aq_gain < ENC_SUPPORTED_MIN_AQ_GAIN) || (enc_props->temp_aq_gain > ENC_SUPPORTED_MAX_AQ_GAIN)) &&
      (enc_props->temp_aq_gain != ENC_AQ_GAIN_NOT_USED)) {
    printf("Invalid temporal aq gain %d\n", enc_props->temp_aq_gain);
    return XMA_APP_ERROR;
  }

  if (((enc_props->spat_aq_gain < ENC_SUPPORTED_MIN_AQ_GAIN) || (enc_props->spat_aq_gain > ENC_SUPPORTED_MAX_AQ_GAIN)) &&
      (enc_props->spat_aq_gain != ENC_AQ_GAIN_NOT_USED)) {
    printf("Invalid spatial aq gain %d\n", enc_props->spat_aq_gain);
    return XMA_APP_ERROR;
  }

  if (((enc_props->spatial_aq != ENC_SPATIAL_AQ_DISABLE) && (enc_props->spatial_aq != ENC_SPATIAL_AQ_ENABLE) && (enc_props->spatial_aq != ENC_DEFAULT_SPATIAL_AQ))) {
    printf("Invalid value of spatial_aq  %d\n", enc_props->spatial_aq);
    return XMA_APP_ERROR;
  }

  if (((enc_props->temporal_aq != ENC_TEMPORAL_AQ_DISABLE) && (enc_props->temporal_aq != ENC_TEMPORAL_AQ_ENABLE) &&
          (enc_props->temporal_aq != ENC_DEFAULT_TEMPORAL_AQ))) {
    printf("Invalid value of temporal_aq %d\n", enc_props->temporal_aq);
    return XMA_APP_ERROR;
  }

  if (((enc_props->qp_mode < ENC_DEFAULT_QP_MODE) || (enc_props->qp_mode > ENC_QP_MODE_UNIFORM))) {
    printf("Invalid value for qp_mode mode %d\n", enc_props->qp_mode);
    return XMA_APP_ERROR;
  }

  if (((enc_props->rc_mode < ENC_RC_MODE_AUTO) || (enc_props->rc_mode > ENC_RC_MODE_CRF))) {
    printf("Invalid value for control_rate %d\n", enc_props->rc_mode);
    return XMA_APP_ERROR;
  }

  if (((enc_props->crf < ENC_MIN_CRF) || (enc_props->crf > ENC_MAX_CRF))) {
    printf("Invalid value for crf %d\n", enc_props->crf);
    return XMA_APP_ERROR;
  }

  if (((enc_props->force_idr != ENC_IDR_DISABLE) && (enc_props->force_idr != ENC_IDR_ENABLE))) {
    printf("Invalid value for force_idr %d\n", enc_props->force_idr);
    return XMA_APP_ERROR;
  }

  if ((enc_props->tune_metrics < 1) && (enc_props->tune_metrics > ENC_MAX_TUNE_METRICS)) {
    printf("Invalid value for tune_metrics  %d\n", enc_props->tune_metrics);
    return XMA_APP_ERROR;
  }

  if (enc_props->num_bframes > INT_MAX) {
    printf("Invalid number of B frames %d\n", enc_props->num_bframes);
    return XMA_APP_ERROR;
  }

  if ((enc_props->lookahead_depth < ENC_MIN_LOOKAHEAD_DEPTH) || (enc_props->lookahead_depth > ENC_MAX_LOOKAHEAD_DEPTH)) {
    printf("Invalid LA depth %d\n", enc_props->lookahead_depth);
    return XMA_APP_ERROR;
  }

  if ((enc_props->lookahead_depth >= 0) && (enc_props->gop_size >= 0) && (enc_props->lookahead_depth > enc_props->gop_size)) {
    printf("LA Depth %d cannot be more than GOP size(%d)\n", enc_props->lookahead_depth, enc_props->gop_size);
    return XMA_APP_ERROR;
  }

  if (xlnx_enc_validate_level_arg(enc_props->level, enc_props->codec_id) != XMA_APP_SUCCESS) {
    printf("ERROR: Unsupported level %d for codec ", enc_props->level);
    if (enc_props->codec_id == ENCODER_ID_H264) {
      printf("H264\n");
    } else if (enc_props->codec_id == ENCODER_ID_HEVC) {
      printf("HEVC\n");
    } else if (enc_props->codec_id == ENCODER_ID_AV1) {
      printf("AV1\n");
    }
    return XMA_APP_ERROR;
  }

  if (enc_props->latency_ms < ENC_MIN_LATENCY_MS || enc_props->latency_ms > ENC_MAX_LATENCY_MS) {
    fprintf(stderr, "Lookahead depth specified in milliseconds is out of range %d \n", enc_props->latency_ms);
    return XMA_APP_ERROR;
  }

  if (enc_props->no_low_latency_b_frames != ENC_NO_LOWLAT_BFRAMES_DEFAULT && enc_props->no_low_latency_b_frames != ENC_NO_LOWLAT_BFRAMES_ENABLE) {
    fprintf(stderr, "Invalid value for No low latency B-frames %d \n", enc_props->no_low_latency_b_frames);
    return XMA_APP_ERROR;
  }

  if (enc_props->bufsize < ENC_MIN_BUFSIZE || enc_props->bufsize > ENC_MAX_BUFSIZE) {
    fprintf(stderr, "Invalid value of bufsize %d \n", enc_props->bufsize);
    return XMA_APP_ERROR;
  }

  if (enc_props->lookahead_depth == 0) {
    if (enc_props->num_bframes != ENC_DEFAULT_NUM_B_FRAMES && enc_props->num_bframes != 0) {
      fprintf(stderr, "Lookahead_depth is 0, but b-frames are not 0!\n");
      return XMA_APP_ERROR;
    }
  }

  if (enc_props->num_cores > XMA_ENC_CORES_2) {
    fprintf(stderr, "Invalid value for cores %d \n", enc_props->num_cores);
    return XMA_APP_ERROR;
  }
  if (enc_props->num_cores == XMA_ENC_CORES_2) {
    if (enc_props->slice != -1) {
      fprintf(stderr, "For 2-slice encoding, slice id needs to be set internally. Please set it to -1 \n");
      return XMA_APP_ERROR;
    }
  }

  return XMA_APP_SUCCESS;
}

/**
 * xlnx_enc_validate_arguments: Validates generic encoder app arguments
 *
 * @param enc_ctx: Encoder context
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
static int32_t xlnx_enc_validate_arguments(XlnxEncoderArguments* arguments) {
  XlnxEncoderProperties* enc_props = &arguments->enc_props;
  int32_t                ret       = XMA_APP_ERROR;
  if (access(arguments->input_file, F_OK) != 0) {
    printf("Unable to access input file \"%s\"\n", arguments->input_file);
    return XMA_APP_ERROR;
  }
  if ((arguments->num_frames <= 0) || (arguments->num_frames > SIZE_MAX)) {
    printf("Invalid number of frames to encode %zu\n", arguments->num_frames);
    return XMA_APP_ERROR;
  }
  if (arguments->loop_count < -1) {
    printf(
        "Invalid stream_loop %d. 0 <= stream_loop <= MAX_INT."
        " -1 for infinite loop.\n",
        arguments->loop_count);
    return XMA_APP_ERROR;
  }
  if (enc_props->device_id > 15) {
    printf("Unsupported device ID %d\n", enc_props->device_id);
    return XMA_APP_ERROR;
  }
  if ((enc_props->width > ENC_SUPPORTED_MAX_WIDTH) || (enc_props->height > ENC_SUPPORTED_MAX_WIDTH) ||
      ((enc_props->width * enc_props->height) > ENC_SUPPORTED_MAX_PIXELS)) {
    printf(
        "Input resolution %dx%d exceeds maximum supported "
        "resolution %dx%d\n",
        enc_props->width, enc_props->height, ENC_SUPPORTED_MAX_WIDTH, ENC_SUPPORTED_MAX_HEIGHT);
    return XMA_APP_ERROR;
  }

  if ((enc_props->width < ENC_SUPPORTED_MIN_WIDTH) || (enc_props->width % 4)) {
    printf("Unsupported width %d\n", enc_props->width);
    return XMA_APP_ERROR;
  }

  if ((enc_props->height < ENC_SUPPORTED_MIN_HEIGHT) || (enc_props->height % 4)) {
    printf("Unsupported height %d\n", enc_props->height);
    return XMA_APP_ERROR;
  }

  ret = xlnx_enc_validate_codec_arguments(enc_props);
  return ret;
}

/**
 * xlnx_enc_get_br_in_kbps: Get value of bit rate
 * @param desination: Where to store the bitrate
 * @param source: User input value
 * @param param_name: Name of the parameter
 * @return XMA_APP_SUCCESS on success, XMA_APP_ERROR on error
 */
static int xlnx_enc_get_br_in_kbps(int64_t* destination, char* source, char* param_name) {
  float br_in_kbps = atof(source);
  if (xlnx_utils_check_if_pattern_matches("^-?[0-9]*\\.?[0-9]+[M|m|K|k]*$", source) == 0) {
    printf(
        "Unrecognized value \"%s\" for argument -%s! Make sure the value is of proper "
        "type.\n",
        source, param_name);
    return XMA_APP_ERROR;
  }

  if (xlnx_utils_check_if_pattern_matches("[M|m]+", source)) {
    *destination = br_in_kbps * 1000;
  } else if (xlnx_utils_check_if_pattern_matches("[K|k]+", source)) {
    *destination = br_in_kbps;
  } else {
    *destination = (br_in_kbps / 1000);
  }
  return XMA_APP_SUCCESS;
}

/**
 * Retrieve tokens from a string
 * @param idr_input User's input idr periods
 * @param dyanmic_idr_periods Integer array for idr periods
 * @return Number of dynamic idr values in the user's input string
 */
static int xlnx_enc_retrieve_token(char* idr_input, uint32_t* dyn_idr_arr) {
  int        count        = 0, ret;
  const char delimiters[] = "( , )";
  char*      save_ptr     = NULL;
  char*      token        = strtok_r(idr_input, delimiters, &save_ptr);
  while (token != NULL) {
    ret = xlnx_utils_set_uint_arg(&dyn_idr_arr[count], token, FLAG_DYN_IDR);
    if (ret == XMA_APP_ERROR) {
      return XMA_APP_ERROR;
    }
    token = strtok_r(NULL, delimiters, &save_ptr);
    count++;
  }
  return count;
}

/**
 * Retrive user's input idr period and save in integer array
 * @param optargs User's input idr periods
 * @param dynamic_idr Struct for dynamic idr parameters
 * @return XMA_APP_SUCCESS on success, XMA_APP_ERROR on error
 */
static int xlnx_enc_get_idr_frames(char* optargs, XlnxDynIdrFrames* dynamic_idr) {
  int count = 0, i = 0;
  while (optargs[i] != '\0') {
    if (optargs[i] == ',' || (optargs[i] == ' ' && (optargs[i - 1] != ' ' && optargs[i - 1] != ','))) {
      count++;
    }
    i++;
  }
  dynamic_idr->idr_arr = (uint32_t*) calloc(count + 1, sizeof(uint32_t));
  count                = xlnx_enc_retrieve_token(optargs, dynamic_idr->idr_arr);
  if (count == XMA_APP_ERROR) {
    printf("Invalid value for dynamic idr frame");
    return count;
  }
  dynamic_idr->idr_arr_idx = 0;
  dynamic_idr->len_idr_arr = count;

  qsort(dynamic_idr->idr_arr, dynamic_idr->len_idr_arr, sizeof(uint32_t), xlnx_utils_compare);

  return XMA_APP_SUCCESS;
}

/**
 * xlnx_enc_parse_args: Parses the command line arguments
 *
 * @param argc: Number of arguments
 * @param argv: Pointer to the arguments
 * @param enc_ctx: Encoder context
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
static int32_t xlnx_enc_parse_args(int32_t argc, char* argv[], XlnxEncoderArguments* arguments) {
  XlnxEncoderProperties* enc_props = &arguments->enc_props;
  int32_t                flag;
  int32_t                option_index;
  int32_t                ret = INT32_MIN;
  int32_t                temp;
  while (1) {
    flag = getopt_long_only(argc, argv, "", enc_options, &option_index);
    if (flag == -1) {
      break;
    }
    switch (flag) {
    case HELP_ARG:
      printf("%s\n", xlnx_enc_get_help());
      exit(0);

    case LOG_LEVEL_ARG:
      ret                  = xlnx_utils_set_int_arg(&temp, optarg, "");
      arguments->log_level = (XmaLogLevelType) temp;
      if (ret == XMA_APP_ERROR) {
        ret = xlnx_utils_key_from_string(param_log_level_lookup, optarg, PARAM_LOG_LEVEL_KEYS);
        if (ret == XMA_APP_ERROR) {
          if (optarg) {
            printf("Invalid log level value %s\n", optarg);
          } else {
            printf("Invalid log level\n");
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
            printf("Invalid log location value %s\n", optarg);
          } else {
            printf("Invalid log location\n");
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
      if ((int) enc_props->device_id != cmdline_dev_id && enc_props->device_id != 0) {
        fprintf(stderr, "Device ID %d specified on commandline, but xrm reserve id evaluated device id %d!\n", cmdline_dev_id, enc_props->device_id);
        ret = XMA_APP_ERROR;
      } else {
        enc_props->device_id = cmdline_dev_id;
      }
      break;

    case INPUT_FILE_ARG:
      arguments->input_file = optarg;
      break;

    case OUTPUT_FILE_ARG:
      arguments->output_file = optarg;
      break;

    case STAT_FILE_ARG:
      arguments->stat_file = optarg;
      break;

    case DYN_PARAM_FILE_ARG:
      arguments->dyn_param_file = optarg;
      break;

    case NUM_FRAMES_ARG:
      ret = xlnx_utils_set_size_t_arg(&arguments->num_frames, optarg, FLAG_NUM_FRAMES);
      break;

    case LOOP_COUNT_ARG:
      ret = xlnx_utils_set_int_arg(&arguments->loop_count, optarg, FLAG_STREAM_LOOP);
      break;

    case ENCODER_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->codec_id, optarg, "");
      if (ret == XMA_APP_ERROR) {
        ret = xlnx_utils_key_from_string(enc_param_codec_lookup, optarg, ENC_PARAM_CODEC_KEYS);
        if (ret == XMA_APP_ERROR) {
          if (optarg) {
            printf("Unsupported codec %s\n", optarg);
          } else {
            printf("Unsupported codec\n");
          }
        } else {
          enc_props->codec_id = ret;
          ret                 = XMA_APP_SUCCESS;
        }
      }
      if (((enc_props->codec_id == ENCODER_ID_H264) || (enc_props->codec_id == ENCODER_ID_AV1)) && (enc_props->profile == ENC_HEVC_MAIN)) {
        enc_props->profile = ENC_PROFILE_DEFAULT;
      }
      break;

    case DEVICE_TYPE_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->device_type, optarg, "");
      if (ret == XMA_APP_ERROR) {
        ret = xlnx_utils_key_from_string(enc_param_av1_device_type_lookup, optarg, ENC_PARAM_DEVICE_TYPE_AV1_KEYS);
        if (ret == XMA_APP_ERROR) {
          if (optarg) {
            printf("Unsupported device type %s\n", optarg);
          } else {
            printf("Unsupported device type\n");
          }
        } else {
          enc_props->device_type = ret;
          ret                    = XMA_APP_SUCCESS;
        }
      }
      break;

    case INPUT_WIDTH_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->width, optarg, FLAG_INPUT_WIDTH);
      break;

    case INPUT_HEIGHT_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->height, optarg, FLAG_INPUT_HEIGHT);
      break;

    case INPUT_PIX_FMT_ARG:
      ret                = xlnx_utils_set_pix_fmt_arg(&arguments->pix_fmt, optarg, FLAG_INPUT_PIX_FMT);
      enc_props->pix_fmt = arguments->pix_fmt;
      break;

    case BITRATE_ARG:
      ret = xlnx_enc_get_br_in_kbps(&enc_props->bitrate, optarg, FLAG_BITRATE);
      break;

    case FPS_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->fps, optarg, FLAG_FPS);
      break;

    case INTRA_PERIOD_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->gop_size, optarg, FLAG_INTRA_PERIOD);
      break;

    case DYNAMIC_IDR_ARG:
      ret = xlnx_enc_get_idr_frames(optarg, &arguments->dynamic_idr);
      break;

    case QP_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->qp, optarg, FLAG_QP);
      break;

    case MIN_QP_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->min_qp, optarg, FLAG_MIN_QP);
      break;

    case MAX_QP_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->max_qp, optarg, FLAG_MAX_QP);
      break;

    case SPAT_AQ_GAIN_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->spat_aq_gain, optarg, FLAG_SPAT_AQ_GAIN);
      break;

    case TEMP_AQ_GAIN_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->temp_aq_gain, optarg, FLAG_TEMP_AQ_GAIN);
      break;

    case SPAT_AQ_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->spatial_aq, optarg, FLAG_TEMP_AQ);
      break;

    case TEMP_AQ_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->temporal_aq, optarg, FLAG_SPAT_AQ);
      break;

    case QP_MODE_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->qp_mode, optarg, FLAG_QP_MODE);
      break;

    case RC_MODE_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->rc_mode, optarg, FLAG_RC_MODE);
      break;

    case CRF_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->crf, optarg, FLAG_CRF);
      break;

    case CABR_CONFIG_ARG:
      strcpy(enc_props->cabr_config, optarg);
      break;

    case MAX_BITRATE_ARG:
      ret = xlnx_enc_get_br_in_kbps(&enc_props->max_bitrate, optarg, FLAG_MAX_BITRATE);
      break;

    case FORCE_IDR_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->force_idr, optarg, FLAG_FORCE_IDR);
      break;

    case NUM_SLICES_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->slice, optarg, FLAG_NUM_SLICES);
      break;

    case NUM_CORES_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->num_cores, optarg, FLAG_NUM_CORES);
      break;

    case NUM_BFRAMES_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->num_bframes, optarg, FLAG_NUM_BFRAMES);
      break;

    case PRESET_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->preset, optarg, "");
      if (ret == XMA_APP_ERROR) {
        ret = xlnx_utils_key_from_string(enc_param_preset_lookup, optarg, ENC_PARAM_PRESET_KEYS);
        if (ret == XMA_APP_ERROR) {
          if (optarg) {
            printf("Invalid preset value %s\n", optarg);
          } else {
            printf("Invalid preset\n");
          }
        } else {
          enc_props->preset = ret;
          ret               = XMA_APP_SUCCESS;
          strncpy(enc_props->enc_preset, optarg, sizeof(enc_props->enc_preset) - 1);
        }
      } else {
        char* enc_preset = xlnx_utils_key_from_value(enc_param_preset_lookup, enc_props->preset, ENC_PARAM_PRESET_KEYS);
        if (enc_preset == NULL) {
          printf("Invalid preset value %s\n", optarg);
          ret = XMA_APP_ERROR;
        } else {
          strncpy(enc_props->enc_preset, enc_preset, sizeof(enc_props->enc_preset) - 1);
          ret = XMA_APP_SUCCESS;
        }
      }
      break;

    case PROFILE_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->profile, optarg, "");
      if (ret == XMA_APP_ERROR) {
        if (enc_props->codec_id == ENCODER_ID_H264) {
          ret = xlnx_utils_key_from_string(enc_param_h264_profile_lookup, optarg, ENC_PARAM_PROF_H264_KEYS);
          if (ret == XMA_APP_ERROR) {
            if (optarg) {
              printf("Invalid H264 codec profile value %s\n", optarg);
            } else {
              printf("Invalid H264 codec profile\n");
            }
          } else {
            enc_props->profile = ret;
            ret                = XMA_APP_SUCCESS;
          }
        } else {
          ret = xlnx_utils_key_from_string(enc_param_hevc_profile_lookup, optarg, ENC_PARAM_PROF_HEVC_KEYS);
          if (ret == XMA_APP_ERROR) {
            if (optarg) {
              printf("Invalid HEVC codec profile value %s\n", optarg);
            } else {
              printf("Invalid HEVC codec profile\n");
            }
          } else {
            enc_props->profile = ret;
            ret                = XMA_APP_SUCCESS;
          }
        }
      }
      break;

    case LEVEL_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->level, optarg, FLAG_LEVEL);
      break;

    case TIER_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->tier, optarg, "");
      if (ret == XMA_APP_ERROR) {
        ret = xlnx_utils_key_from_string(enc_param_tier_lookup, optarg, ENC_PARAM_TIER_KEYS);
        if (ret == XMA_APP_ERROR) {
          if (optarg) {
            printf("Invalid codec tier value %s\n", optarg);
          } else {
            printf("Invalid codec tier\n");
          }
        } else {
          enc_props->tier = ret;
          ret             = XMA_APP_SUCCESS;
        }
      }
      break;

    case LOOKAHEAD_DEPTH_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->lookahead_depth, optarg, FLAG_LOOKAHEAD_DEPTH);
      break;

    case LATENCY_MS_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->latency_ms, optarg, FLAG_LATENCY_MS);
      break;

    case NO_LOWLAT_BFRAMES_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->no_low_latency_b_frames, optarg, FLAG_NO_LOWLAT_BFRAMES);
      break;

    case TUNE_METRICS_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->tune_metrics, optarg, "");
      if (ret == XMA_APP_ERROR) {
        ret = xlnx_utils_key_from_string(enc_param_tune_metrics_lookup, optarg, ENC_PARAM_TUNE_METRICS_KEYS);
        if (ret == XMA_APP_ERROR) {
          if (optarg) {
            printf("Invalid tune metrics value %s\n", optarg);
          } else {
            printf("Invalid tune metrics\n");
          }
        } else {
          enc_props->tune_metrics = ret;
          ret                     = XMA_APP_SUCCESS;
        }
      }
      break;

    case DYNAMIC_GOP_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->dynamic_gop, optarg, FLAG_DYNAMIC_GOP);
      break;

    case BUFSIZE_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->bufsize, optarg, FLAG_BUFSIZE);
      break;

    case EXPERT_OPTIONS_ARG:
      strcpy(enc_props->expert_options, optarg);
      break;

    case LATENCY_LOGGING_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->latency_logging, optarg, FLAG_LATENCY_LOGGING);
      break;

    default:
      printf("ERROR: Failed to parse commandline.\n");
      return XMA_APP_ERROR;
    }

    if (ret == XMA_APP_ERROR) {
      return XMA_APP_ERROR;
    }
  }
  return ret;
}

/**
 * xlnx_enc_set_arg_defaults: Initializes the encoder arguments with default
 * values.
 *
 * @param enc_ctx: A pointer to the arguments struct
 * @return None
 */
static void xlnx_enc_set_arg_defaults(XlnxEncoderArguments* arguments) {
  XlnxEncoderProperties* enc_props = &arguments->enc_props;

  /* Initialize the encoder parameters to default */
  arguments->loop_count     = 0;
  arguments->num_frames     = SIZE_MAX;
  arguments->input_file     = "";
  arguments->output_file    = "";
  arguments->stat_file      = "";
  arguments->dyn_param_file = "";
  arguments->log_level      = ENC_DEFAULT_LOG_LEVEL;
  arguments->log_location   = ENC_DEFAULT_LOG_LOCATION;
  arguments->log_file       = ENC_DEFAULT_LOG_FILE;
  arguments->pix_fmt        = XMA_YUV420P_FMT_TYPE;
  enc_props->device_id      = xrm_interface_get_dev_index(); // Check xrm reserve IDs
  enc_props->codec_id       = -1;
  enc_props->device_type    = XMA_ENC_DEVICE_TYPE_1;
  enc_props->width          = ENC_DEFAULT_WIDTH;
  enc_props->height         = ENC_DEFAULT_HEIGHT;
  enc_props->pix_fmt        = XMA_YUV420P_FMT_TYPE;
  enc_props->bitrate        = 0;
  enc_props->max_bitrate    = -1;
  enc_props->crf            = ENC_CRF_DEFAULT;
  enc_props->force_idr      = ENC_IDR_ENABLE;
  enc_props->fps            = ENC_DEFAULT_FRAMERATE;
  enc_props->gop_size       = ENC_DEFAULT_GOP_SIZE;
  enc_props->min_qp         = ENC_DEFAULT_MIN_QP;
  enc_props->max_qp         = ENC_DEFAULT_MAX_QP;
  enc_props->spat_aq_gain   = ENC_AQ_GAIN_NOT_USED;
  enc_props->temp_aq_gain   = ENC_AQ_GAIN_NOT_USED;
  enc_props->spatial_aq     = ENC_DEFAULT_SPATIAL_AQ;
  enc_props->temporal_aq    = ENC_DEFAULT_TEMPORAL_AQ;
  enc_props->slice          = DEFAULT_SLICE_ID;
  enc_props->num_cores      = ENC_DEFAULT_CORES;
  enc_props->num_bframes    = ENC_DEFAULT_NUM_B_FRAMES;
  enc_props->qp             = ENC_DEFAULT_QP;
  enc_props->qp_mode        = ENC_DEFAULT_QP_MODE;
  enc_props->rc_mode        = ENC_RC_MODE_DEFAULT;
  enc_props->force_idr      = ENC_IDR_ENABLE;
  enc_props->preset         = XMA_ENC_PRESET_DEFAULT;
  strncpy(enc_props->enc_preset, ENC_PRESET_DEFAULT, sizeof(enc_props->enc_preset) - 1);

  /* Assigning the default profile as HEVC profile. If the codec option
       is H264, this will be updated */
  enc_props->profile                 = ENC_HEVC_MAIN;
  enc_props->level                   = ENC_DEFAULT_LEVEL;
  enc_props->tier                    = ENC_TIER_DEFAULT;
  enc_props->lookahead_depth         = ENC_DEFAULT_LOOKAHEAD_DEPTH;
  enc_props->latency_ms              = ENC_DEFAULT_LATENCY_MS;
  enc_props->no_low_latency_b_frames = ENC_NO_LOWLAT_BFRAMES_DEFAULT;
  ;
  enc_props->bufsize      = ENC_DEFAULT_BUFSIZE;
  enc_props->tune_metrics = ENC_TUNE_METRICS_DEFAULT;
  enc_props->dynamic_gop  = ENC_DYNAMIC_GOP_DEFAULT;
  enc_props->pix_fmt      = XMA_YUV420P_FMT_TYPE;

  strcpy(enc_props->cabr_config, ENC_DEFAULT_CABR_CONFIG);
}

/**
 * xlnx_enc_parser: Parses and initializes the encoder parameters
 *
 * @param argc: Number of arguments
 * @param argv: Pointer to the arguments
 * @param arguments: The arguments struct to be created.
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
int32_t xlnx_enc_get_arguments(int32_t argc, char* argv[], XlnxEncoderArguments* arguments) {
  if (argc < 2) {
    printf("%s\n", xlnx_enc_get_help());
    exit(XMA_APP_SUCCESS);
  }
  memset(arguments, 0, sizeof(XlnxEncoderArguments));
  /* Encoder context parameters initialization */
  xlnx_enc_set_arg_defaults(arguments);

  /* Parse the argumenst and update the structure */
  if (xlnx_enc_parse_args(argc, argv, arguments) != XMA_APP_SUCCESS) {
    return XMA_APP_ERROR;
  }
  if (xlnx_enc_validate_arguments(arguments) != XMA_APP_SUCCESS) {
    return XMA_APP_ERROR;
  }

  return XMA_APP_SUCCESS;
}
