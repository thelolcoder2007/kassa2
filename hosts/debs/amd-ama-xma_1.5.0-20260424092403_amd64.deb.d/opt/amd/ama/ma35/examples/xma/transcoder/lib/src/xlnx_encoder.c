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

#include "xlnx_encoder.h"

static struct option enc_options[] = {{FLAG_ENC_CODEC_TYPE, required_argument, 0, ENC_CODEC_ID_ARG}, {FLAG_ENC_DEVICE_TYPE, required_argument, 0, ENC_DEVICE_TYPE_ARG},
    {FLAG_ENC_BITRATE, required_argument, 0, ENC_BITRATE_ARG}, {FLAG_ENC_FPS, required_argument, 0, ENC_FPS_ARG},
    {FLAG_ENC_INTRA_PERIOD, required_argument, 0, ENC_INTRA_PERIOD_ARG}, {FLAG_ENC_MIN_QP, required_argument, 0, ENC_MIN_QP_ARG},
    {FLAG_ENC_MAX_QP, required_argument, 0, ENC_MAX_QP_ARG}, {FLAG_ENC_SPAT_AQ_GAIN, required_argument, 0, ENC_SPAT_AQ_GAIN_ARG},
    {FLAG_ENC_TEMP_AQ_GAIN, required_argument, 0, ENC_TEMP_AQ_GAIN_ARG}, {FLAG_ENC_SPAT_AQ, required_argument, 0, ENC_SPAT_AQ_ARG},
    {FLAG_ENC_TEMP_AQ, required_argument, 0, ENC_TEMP_AQ_ARG}, {FLAG_ENC_QP_MODE, required_argument, 0, ENC_QP_MODE_ARG},
    {FLAG_ENC_RC_MODE, required_argument, 0, ENC_RC_MODE_ARG}, {FLAG_ENC_CRF, required_argument, 0, ENC_CRF_ARG},
    {FLAG_CABR_CONFIG, required_argument, 0, ENC_CABR_CONFIG_ARG}, {FLAG_ENC_MAX_BITRATE, required_argument, 0, ENC_MAX_BITRATE_ARG},
    {FLAG_ENC_FORCE_IDR, required_argument, 0, ENC_FORCE_IDR_ARG}, {FLAG_ENC_NUM_SLICES, required_argument, 0, ENC_NUM_SLICES_ARG},
    {FLAG_ENC_NUM_CORES, required_argument, 0, ENC_NUM_CORES_ARG}, {FLAG_ENC_NUM_BFRAMES, required_argument, 0, ENC_NUM_BFRAMES_ARG},
    {FLAG_ENC_PRESET, required_argument, 0, ENC_PRESET_ARG}, {FLAG_ENC_PROFILE, required_argument, 0, ENC_PROFILE_ARG},
    {FLAG_ENC_LEVEL, required_argument, 0, ENC_LEVEL_ARG}, {FLAG_ENC_LOOKAHEAD_DEPTH, required_argument, 0, ENC_LOOKAHEAD_DEPTH_ARG},
    {FLAG_ENC_LATENCY_MS, required_argument, 0, ENC_LATENCY_MS_ARG}, {FLAG_ENC_NO_LOWLAT_BFRAMES, required_argument, 0, ENC_NO_LOWLAT_BFRAMES_ARG},
    {FLAG_ENC_TUNE_METRICS, required_argument, 0, ENC_TUNE_METRICS_ARG}, {FLAG_ENC_QP, required_argument, 0, ENC_QP_ARG}, {FLAG_TIER, required_argument, 0, TIER_ARG},
    {FLAG_ENC_DYNAMIC_GOP, required_argument, 0, ENC_DYNAMIC_GOP_ARG}, {FLAG_ENC_LATENCY_LOGGING, required_argument, 0, ENC_LATENCY_LOGGING_ARG},
    {FLAG_ENC_BUFSIZE, required_argument, 0, ENC_BUFSIZE_ARG}, {FLAG_ENC_EXPERT_OPTIONS, required_argument, 0, ENC_EXPERT_OPTIONS_ARG},
    {FLAG_ENC_OUTPUT_FILE, required_argument, 0, ENC_OUTPUT_FILE_ARG}, {0, 0, 0, 0}};

XlnxParameterLookup enc_param_codec_lookup[] = {{"h264_ama", ENCODER_ID_H264}, {"hevc_ama", ENCODER_ID_HEVC}, {"av1_ama", ENCODER_ID_AV1}};

XlnxParameterLookup enc_param_preset_lookup[] = {{"slow", XMA_ENC_PRESET_SLOW}, {"medium", XMA_ENC_PRESET_MEDIUM}, {"fast", XMA_ENC_PRESET_FAST}};

XlnxParameterLookup enc_param_h264_profile_lookup[] = {{"auto", ENC_PROFILE_AUTO}, {"baseline", ENC_H264_BASELINE}, {"main", ENC_H264_MAIN}, {"high", ENC_H264_HIGH},
    {"high10", ENC_H264_HIGH_10}, {"high10_intra", ENC_H264_HIGH_10_INTRA}};

XlnxParameterLookup enc_param_hevc_profile_lookup[] = {
    {"auto", ENC_PROFILE_AUTO}, {"main", ENC_HEVC_MAIN}, {"main_intra", ENC_HEVC_MAIN_INTRA}, {"main10", ENC_HEVC_MAIN_10}, {"main10_intra", ENC_HEVC_MAIN10_INTRA}};

XlnxParameterLookup enc_param_av1_profile_lookup[] = {{"auto", ENC_PROFILE_AUTO}, {"main", ENC_AV1_MAIN}};

XlnxParameterLookup enc_param_tier_lookup[] = {{"auto", ENC_TIER_AUTO}, {"main", ENC_TIER_MAIN}, {"high", ENC_TIER_HIGH}};

XlnxParameterLookup enc_param_av1_device_type_lookup[] = {{"any", ENC_AV1_DEVICE_TYPE_ANY}, {"1", ENC_AV1_DEVICE_TYPE_1}, {"2", ENC_AV1_DEVICE_TYPE_2}};

XlnxParameterLookup enc_param_tune_metrics_lookup[] = {
    {"vq", XMA_ENC_TUNE_METRICS_VQ}, {"psnr", XMA_ENC_TUNE_METRICS_PSNR}, {"ssim", XMA_ENC_TUNE_METRICS_SSIM}, {"vmaf", XMA_ENC_TUNE_METRICS_VMAF}};

#define ENC_PARAM_CODEC_KEYS (sizeof(enc_param_codec_lookup) / sizeof(XlnxParameterLookup))
#define ENC_PARAM_PRESET_KEYS (sizeof(enc_param_preset_lookup) / sizeof(XlnxParameterLookup))
#define ENC_PARAM_PROF_H264_KEYS (sizeof(enc_param_h264_profile_lookup) / sizeof(XlnxParameterLookup))
#define ENC_PARAM_PROF_HEVC_KEYS (sizeof(enc_param_hevc_profile_lookup) / sizeof(XlnxParameterLookup))
#define ENC_PARAM_PROF_AV1_KEYS (sizeof(enc_param_av1_profile_lookup) / sizeof(XlnxParameterLookup))
#define ENC_PARAM_TIER_KEYS (sizeof(enc_param_tier_lookup) / sizeof(XlnxParameterLookup))
#define ENC_PARAM_DEVICE_TYPE_AV1_KEYS (sizeof(enc_param_av1_device_type_lookup) / sizeof(XlnxParameterLookup))
#define ENC_PARAM_TUNE_METRICS_KEYS (sizeof(enc_param_tune_metrics_lookup) / sizeof(XlnxParameterLookup))

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
 * xlnx_enc_validate_arguments: Validates encoder arguments
 *
 * @param enc_ctx: Encoder properties
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
static int32_t xlnx_enc_validate_arguments(XlnxEncoderProperties* enc_props) {
  if ((enc_props->fps <= 0) || (enc_props->fps > INT_MAX)) {
    printf("Invalid frame rate %d\n", enc_props->fps);
    return XMA_APP_ERROR;
  }

  if ((enc_props->gop_size < ENC_MIN_GOP_SIZE) || (enc_props->gop_size > ENC_MAX_GOP_SIZE)) {
    printf("Invalid intra period %d\n", enc_props->gop_size);
    return XMA_APP_ERROR;
  }

  if ((enc_props->min_qp < ENC_MIN_MIN_QP) || (enc_props->min_qp > ENC_MAX_MIN_QP)) {
    printf("Invalid min qp %d\n", enc_props->min_qp);
    return XMA_APP_ERROR;
  }

  if ((enc_props->max_qp < ENC_MIN_MAX_QP) || (enc_props->max_qp > ENC_MAX_MAX_QP)) {
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

  if (((enc_props->spatial_aq < ENC_SPATIAL_AQ_AUTO) || (enc_props->spatial_aq > ENC_SPATIAL_AQ_ENABLE))) {
    printf("Invalid value of spatial_aq %d\n", enc_props->spatial_aq);
    return XMA_APP_ERROR;
  }

  if (((enc_props->temporal_aq < ENC_TEMPORAL_AQ_AUTO) || (enc_props->temporal_aq > ENC_TEMPORAL_AQ_ENABLE))) {
    printf("Invalid value of temporal_aq %d\n", enc_props->temporal_aq);
    return XMA_APP_ERROR;
  }

  if (enc_props->num_bframes < ENC_MIN_NUM_B_FRAMES || (enc_props->codec_id != ENCODER_ID_AV1 && enc_props->num_bframes > ENC_MAX_NUM_B_FRAMES) ||
      enc_props->num_bframes > ENC_MAX_NUM_B_FRAMES_AV1) {
    printf("Invalid number of B frames %d\n", enc_props->num_bframes);
    return XMA_APP_ERROR;
  }

  if (enc_props->max_bitrate > ENC_MAX_MAX_BITRATE || enc_props->max_bitrate < ENC_MIN_MAX_BITRATE) {
    printf("Invalid max bitrate %ld\n", enc_props->max_bitrate);
    return XMA_APP_ERROR;
  }

  if (enc_props->bitrate > ENC_SUPPORTED_MAX_BITRATE || enc_props->bitrate < ENC_SUPPORTED_MIN_BITRATE) {
    printf("Invalid bitrate %ld\n", enc_props->bitrate);
    return XMA_APP_ERROR;
  }

  if ((enc_props->lookahead_depth < ENC_MIN_LOOKAHEAD_DEPTH) || (enc_props->lookahead_depth > ENC_MAX_LOOKAHEAD_DEPTH)) {
    printf("Invalid LA depth %d\n", enc_props->lookahead_depth);
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

  if ((enc_props->tune_metrics < 1) || (enc_props->tune_metrics > ENC_MAX_TUNE_METRICS)) {
    printf("Invalid value for tune_metrics %d\n", enc_props->tune_metrics);
    return XMA_APP_ERROR;
  }

  if ((enc_props->dynamic_gop < ENC_DYNAMIC_GOP_AUTO) || (enc_props->dynamic_gop > ENC_DYNAMIC_GOP_ENABLE)) {
    printf("Invalid value for dynamic_gop %d\n", enc_props->dynamic_gop);
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
      fprintf(stderr, "Ultra low latency enabled, but b-frames are not 0!\n");
      return XMA_APP_ERROR;
    }
  }

  if (enc_props->num_cores > XMA_ENC_CORES_2) {
    fprintf(stderr, "Invalid value for cores %d \n", enc_props->num_cores);
    return XMA_APP_ERROR;
  }
  if (enc_props->num_cores == XMA_ENC_CORES_2) {
    if (enc_props->slice != DEFAULT_SLICE_ID) {
      fprintf(stderr, "For 2-slice encoding, slice id needs to be set internally. Please set it to -1 \n");
      return XMA_APP_ERROR;
    }
  }

  return XMA_APP_SUCCESS;
}

/**
 * xlnx_enc_context_init: Initializes the encoder context with default values.
 *
 * @param enc_ctx: Encoder context
 */
void xlnx_enc_context_init(XlnxEncoderCtx* enc_ctx) {

  XlnxEncoderProperties* enc_props = &enc_ctx->enc_props;
  /* Initialize the encoder parameters to default */
  enc_props->device_id    = DEFAULT_DEVICE_ID;
  enc_props->codec_id     = -1;
  enc_props->device_type  = ENC_DEFAULT_DEVICE_TYPE;
  enc_props->width        = ENC_DEFAULT_WIDTH;
  enc_props->height       = ENC_DEFAULT_HEIGHT;
  enc_props->bitrate      = 0;
  enc_props->max_bitrate  = ENC_DEFAULT_MAX_BITRATE;
  enc_props->crf          = ENC_CRF_DEFAULT;
  enc_props->force_idr    = ENC_IDR_ENABLE;
  enc_props->fps          = ENC_DEFAULT_FRAMERATE;
  enc_props->gop_size     = ENC_DEFAULT_GOP_SIZE;
  enc_props->min_qp       = ENC_DEFAULT_MIN_QP;
  enc_props->max_qp       = ENC_DEFAULT_MAX_QP;
  enc_props->num_bframes  = ENC_DEFAULT_NUM_B_FRAMES;
  enc_props->spat_aq_gain = ENC_AQ_GAIN_NOT_USED;
  enc_props->temp_aq_gain = ENC_AQ_GAIN_NOT_USED;
  enc_props->spatial_aq   = ENC_DEFAULT_SPATIAL_AQ;
  enc_props->temporal_aq  = ENC_DEFAULT_TEMPORAL_AQ;
  enc_props->slice        = DEFAULT_SLICE_ID;
  enc_props->num_cores    = ENC_DEFAULT_CORES;
  enc_props->qp           = ENC_DEFAULT_QP;
  enc_props->qp_mode      = ENC_DEFAULT_QP_MODE;
  enc_props->rc_mode      = ENC_RC_MODE_DEFAULT;
  enc_props->preset       = XMA_ENC_PRESET_DEFAULT;
  strncpy(enc_props->enc_preset, ENC_PRESET_DEFAULT, sizeof(enc_props->enc_preset) - 1);

  /* Assigning the default profile as HEVC profile. If the codec option
       is H264, this will be updated */
  enc_props->profile                 = ENC_HEVC_MAIN;
  enc_props->level                   = ENC_DEFAULT_LEVEL;
  enc_props->tier                    = ENC_TIER_DEFAULT;
  enc_props->lookahead_depth         = ENC_DEFAULT_LOOKAHEAD_DEPTH;
  enc_props->latency_ms              = ENC_DEFAULT_LATENCY_MS;
  enc_props->no_low_latency_b_frames = ENC_NO_LOWLAT_BFRAMES_DEFAULT;
  enc_props->bufsize                 = ENC_DEFAULT_BUFSIZE;
  enc_props->tune_metrics            = ENC_TUNE_METRICS_DEFAULT;
  enc_props->dynamic_gop             = ENC_DYNAMIC_GOP_DEFAULT;
  enc_props->pix_fmt                 = XMA_YUV420P_FMT_TYPE;

  strcpy(enc_props->cabr_config, ENC_DEFAULT_CABR_CONFIG);
}

/**
 * xlnx_enc_update_props: Updates xma encoder properties and options that will
 * be sent to xma plugin
 *
 * @param enc_ctx: Encoder context
 * @param xma_enc_props: XMA encoder properties
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
int32_t xlnx_enc_update_props(XlnxEncoderCtx* enc_ctx, XmaEncoderProperties* xma_enc_props, XmaHandle handle) {

  XlnxEncoderProperties* enc_props = &enc_ctx->enc_props;
  enc_ctx->handle                  = handle;

  /* FFmpeg set default bitrate to 200K so making xma apps aligned to it */
  if ((enc_props->bitrate <= 0) && enc_props->qp <= ENC_DEFAULT_QP) {
    enc_props->bitrate = ENC_DEFAULT_BITRATE;
  }

  if (enc_props->lookahead_depth) {
    if ((enc_props->width > ENC_MAX_LA_INPUT_WIDTH) || (enc_props->height > ENC_MAX_LA_INPUT_WIDTH) || ((enc_props->width * enc_props->height) > ENC_MAX_LA_PIXELS)) {
      xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Input to encoder for lookahead usecase %dx%d exceeds maximum supported resolution %dx%d\n",
          enc_props->width, enc_props->height, ENC_MAX_LA_INPUT_WIDTH, ENC_MAX_LA_INPUT_HEIGHT);
      return XMA_APP_ERROR;
    }
  } else {
    if ((enc_props->width > ENC_SUPPORTED_MAX_WIDTH) || (enc_props->height > ENC_SUPPORTED_MAX_WIDTH) ||
        ((enc_props->width * enc_props->height) > ENC_SUPPORTED_MAX_PIXELS)) {
      xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Input resolution %dx%d exceeds maximum supported resolution %dx%d\n", enc_props->width,
          enc_props->height, ENC_SUPPORTED_MAX_HEIGHT, ENC_SUPPORTED_MAX_WIDTH);

      return XMA_APP_ERROR;
    }
  }

  if ((enc_props->width < ENC_SUPPORTED_MIN_WIDTH) || (enc_props->width % 4)) {
    xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Unsupported width %d\n", enc_props->width);
    return XMA_APP_ERROR;
  }

  if ((enc_props->height < ENC_SUPPORTED_MIN_HEIGHT) || (enc_props->height % 4)) {
    xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Unsupported height %d\n", enc_props->height);
    return XMA_APP_ERROR;
  }

  return xlnx_enc_get_xma_props(enc_ctx->handle, enc_props, xma_enc_props);
}

/**
 * xlnx_utils_get_br_in_kbps: Get value of bit rate
 * @param desination: Where to store the bitrate
 * @param source: User input value
 * @param param_name: Name of the parameter
 * @return XMA_APP_SUCCESS on success, XMA_APP_ERROR on error
 */
static int xlnx_utils_get_br_in_kbps(int64_t* destination, char* source, char* param_name) {
  float br_in_kbps = atof(source);
  if (xlnx_utils_check_if_pattern_matches("^-?[0-9]*\\.?[0-9]+[M|m|K|k]*$", source) == 0) {

    fprintf(stderr,
        "Unrecognized value "
        "\"%s\" for argument -%s! Make sure the value is of proper "
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
 * xlnx_enc_parse_args: Parses the command line arguments
 *
 * @param argc: Number of arguments
 * @param argv: Pointer to the arguments
 * @param enc_ctx: Encoder context
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
int32_t xlnx_enc_parse_args(int32_t argc, char* argv[], XlnxEncoderCtx* enc_ctx, int32_t param_flag, uint32_t stream_no, uint32_t no_of_streams) {
  int32_t                flag = 0;
  int32_t                option_index;
  int32_t                ret         = XMA_APP_SUCCESS;
  int32_t                channel_end = 0;
  uint32_t               cnt_optind  = 1;
  XlnxEncoderProperties* enc_props   = &enc_ctx->enc_props;

  while (!channel_end) {
    if (param_flag == 0) {
      flag = getopt_long_only(argc, argv, "", enc_options, &option_index);
      if (flag == -1) {
        ret = TRANSCODE_PARSING_DONE;
        break;
      }
    } else {
      flag       = param_flag;
      param_flag = 0;
    }
    switch (flag) {
    case ENC_OUTPUT_FILE_ARG:
      optarg = argv[(optind - 1) + stream_no];

      for (; optind < argc && *argv[optind] != '-'; optind++)
        cnt_optind++;

      if (cnt_optind != no_of_streams) {
        fprintf(stderr, "Number of output args doesn't match with number of streams %d\n", no_of_streams);
        return XMA_APP_ERROR;
      }
      enc_ctx->out_file = open(optarg, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
      if (enc_ctx->out_file == XMA_APP_ERROR) {
        fprintf(stderr, "Error opening output file %s \n", optarg);
        return XMA_APP_ERROR;
      }
      channel_end = 1;
      break;

    case ENC_CODEC_ID_ARG:
      if (!strcmp(optarg, AVC_CODEC_NAME)) {
        enc_props->codec_id = ENCODER_ID_H264;
        /* Change the default profile from HEVC_MAIN to H264_HIGH*/
        enc_props->profile = ENC_PROFILE_AUTO;
      } else if (!strcmp(optarg, HEVC_CODEC_NAME)) {
        enc_props->codec_id = ENCODER_ID_HEVC;
      } else if (!strcmp(optarg, AV1_CODEC_NAME)) {
        enc_props->codec_id = ENCODER_ID_AV1;
        enc_props->profile  = ENC_PROFILE_AUTO;
      } else {
        fprintf(stderr, "Unsupported codec %s\n", optarg);
        return XMA_APP_ERROR;
      }
      break;

    case ENC_DEVICE_TYPE_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->device_type, optarg, "");
      if (ret == XMA_APP_ERROR) {
        ret = xlnx_utils_key_from_string(enc_param_av1_device_type_lookup, optarg, ENC_PARAM_DEVICE_TYPE_AV1_KEYS);
        if (ret == XMA_APP_ERROR) {
          if (optarg) {
            printf("Invalid device type %s\n", optarg);
          } else {
            printf("Invalid device type\n");
          }
        } else {
          enc_props->device_type = ret;
          ret                    = XMA_APP_SUCCESS;
        }
      }
      break;

    case ENC_BITRATE_ARG:
      ret = xlnx_utils_get_br_in_kbps(&enc_props->bitrate, optarg, FLAG_ENC_BITRATE);
      break;

    case ENC_FPS_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->fps, optarg, FLAG_ENC_FPS);
      break;

    case ENC_INTRA_PERIOD_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->gop_size, optarg, FLAG_ENC_INTRA_PERIOD);
      break;

    case ENC_MIN_QP_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->min_qp, optarg, FLAG_ENC_MIN_QP);
      break;

    case ENC_MAX_QP_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->max_qp, optarg, FLAG_ENC_MAX_QP);
      break;

    case ENC_NUM_BFRAMES_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->num_bframes, optarg, FLAG_ENC_NUM_BFRAMES);
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

    case ENC_PRESET_ARG:
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

    case ENC_PROFILE_ARG:
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
        } else if (enc_props->codec_id == ENCODER_ID_HEVC) {
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
        } else if (enc_props->codec_id == ENCODER_ID_AV1) {
          ret = xlnx_utils_key_from_string(enc_param_av1_profile_lookup, optarg, ENC_PARAM_PROF_AV1_KEYS);
          if (ret == XMA_APP_ERROR) {
            if (optarg) {
              printf("Invalid AV1 codec profile value %s\n", optarg);
            } else {
              printf("Invalid AV1 codec profile\n");
            }
          } else {
            enc_props->profile = ret;
            ret                = XMA_APP_SUCCESS;
          }
        }
      }
      break;

    case ENC_LEVEL_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->level, optarg, FLAG_ENC_LEVEL);
      break;

    case ENC_SPAT_AQ_GAIN_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->spat_aq_gain, optarg, FLAG_ENC_SPAT_AQ_GAIN);
      break;

    case ENC_TEMP_AQ_GAIN_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->temp_aq_gain, optarg, FLAG_ENC_TEMP_AQ_GAIN);
      break;

    case ENC_SPAT_AQ_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->spatial_aq, optarg, FLAG_ENC_SPAT_AQ);
      break;

    case ENC_TEMP_AQ_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->temporal_aq, optarg, FLAG_ENC_TEMP_AQ);
      break;

    case ENC_QP_MODE_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->qp_mode, optarg, FLAG_ENC_QP_MODE);
      break;

    case ENC_RC_MODE_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->rc_mode, optarg, FLAG_ENC_RC_MODE);
      break;

    case ENC_CRF_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->crf, optarg, FLAG_ENC_CRF);
      break;

    case ENC_CABR_CONFIG_ARG:
      strcpy(enc_props->cabr_config, optarg);
      break;

    case ENC_MAX_BITRATE_ARG:
      ret = xlnx_utils_get_br_in_kbps(&enc_props->max_bitrate, optarg, FLAG_ENC_MAX_BITRATE);
      break;

    case ENC_FORCE_IDR_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->force_idr, optarg, FLAG_ENC_FORCE_IDR);
      break;

    case ENC_NUM_SLICES_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->slice, optarg, FLAG_ENC_NUM_SLICES);
      break;

    case ENC_NUM_CORES_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->num_cores, optarg, FLAG_ENC_NUM_CORES);
      break;

    case ENC_LOOKAHEAD_DEPTH_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->lookahead_depth, optarg, FLAG_ENC_LOOKAHEAD_DEPTH);
      break;

    case ENC_LATENCY_MS_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->latency_ms, optarg, FLAG_ENC_LATENCY_MS);
      break;

    case ENC_NO_LOWLAT_BFRAMES_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->no_low_latency_b_frames, optarg, FLAG_ENC_NO_LOWLAT_BFRAMES);
      break;

    case ENC_LATENCY_LOGGING_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->latency_logging, optarg, FLAG_ENC_LATENCY_LOGGING);
      break;

    case ENC_BUFSIZE_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->bufsize, optarg, FLAG_ENC_BUFSIZE);
      break;

    case ENC_DYNAMIC_GOP_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->dynamic_gop, optarg, FLAG_ENC_DYNAMIC_GOP);
      break;

    case ENC_EXPERT_OPTIONS_ARG:
      strcpy(enc_props->expert_options, optarg);
      break;

    case ENC_QP_ARG:
      ret = xlnx_utils_set_int_arg(&enc_props->qp, optarg, FLAG_ENC_QP);
      break;

    case ENC_TUNE_METRICS_ARG:
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

    default:
      xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Failed to parse encoder arguments %d \n", flag);
      return XMA_APP_ERROR;
    }

    if (ret == XMA_APP_ERROR || ret == XMA_APP_ERROR) {
      return XMA_APP_ERROR;
    }
  }

  ret |= xlnx_enc_validate_arguments(enc_props);
  return ret;
}

/**
 * enc_session: Creates encoder session
 *
 * @param app_xrm_ctx: Transcoder XRM context
 * @param enc_ctx: Encoder context
 * @param xma_enc_props: XMA encoder properties
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
int32_t xlnx_enc_session(XlnxEncoderCtx* enc_ctx, XmaEncoderProperties* xma_enc_props) {

  /* Encoder session creation */
  if (!enc_ctx->handle) {
    enc_ctx->handle = xma_enc_props->handle;
  }
  enc_ctx->enc_session = xma_enc_session_create(xma_enc_props);
  if (enc_ctx->enc_session == NULL) {
    xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Encoder init failed\n");
    return XMA_APP_ERROR;
  }

  return XMA_APP_SUCCESS;
}

/**
 * xlnx_enc_send_frame: Sends YUV input to the encoder
 *
 * @param enc_ctx: Encoder context
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
static int32_t xlnx_enc_send_frame(XlnxEncoderCtx* enc_ctx) {
  int32_t ret = XMA_APP_SUCCESS;
  if (enc_ctx->enc_in_frame) {
    enc_ctx->enc_in_frame->is_idr = false;
    if ((ret = xma_enc_session_send_frame(enc_ctx->enc_session, enc_ctx->enc_in_frame)) <= XMA_APP_ERROR) {
      xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Failed in encoder send frame \n");
    }
  } else if (!enc_ctx->flush_frame_sent) {
    ret = xma_enc_session_send_frame(enc_ctx->enc_session, NULL);
  }
  return ret;
}

/**
 * xlnx_enc_process_frame: Encoder process frame
 *
 * @param enc_ctx: Encoder context
 * @param _enc_null_frame: Encoder null frame flag
 * @param _enc_out_size: Encoder output size
 * @return The result of send_frame if unsuccessful, the result of recv if
 * send was successful, XMA_APP_ERROR on error.
 */
int32_t xlnx_enc_process_frame(XlnxEncoderCtx* enc_ctx, int32_t* enc_out_size, XmaHandle handle) {
  int send_ret = xlnx_enc_send_frame(enc_ctx);
  if (send_ret <= XMA_ERROR || send_ret == XMA_SEND_MORE_DATA) {
    return send_ret;
  }
  if (!enc_ctx->xma_out_buffer) {
    enc_ctx->xma_out_buffer = xma_data_buffer_alloc(handle, 0, true);
  }

  int recv_ret = xma_enc_session_recv_data(enc_ctx->enc_session, enc_ctx->xma_out_buffer, enc_out_size);

  if (recv_ret <= XMA_ERROR) {
    xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "xma_enc_session_recv_data failed with error %d \n", recv_ret);
    return XMA_APP_ERROR;
  }

  if (send_ret == XMA_TRY_AGAIN) {
    return send_ret;
  }
  return recv_ret;
}

/**
 * xlnx_enc_deinit: Deinitialize encoder module
 *
 * @param xrm_ctx: XRM context
 * @param enc_ctx: Encoder context
 * @param xma_enc_props: XMA encoder properties
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
int32_t xlnx_enc_deinit(XlnxEncoderCtx* enc_ctx, XmaEncoderProperties* xma_enc_props) {
  int32_t ret = XMA_APP_ERROR;
  if (enc_ctx->enc_session != NULL) {
    ret = xma_enc_session_destroy(enc_ctx->enc_session);
  }
  xlnx_enc_free_xma_props(xma_enc_props);

  close(enc_ctx->out_file);
  return ret;
}
