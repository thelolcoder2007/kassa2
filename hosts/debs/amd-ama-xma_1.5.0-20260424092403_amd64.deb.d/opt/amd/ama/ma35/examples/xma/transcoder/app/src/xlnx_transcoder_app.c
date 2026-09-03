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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>

#include "xlnx_transcoder.h"
#include "xlnx_transcoder_parser.h"

#define STREAMS 2

static struct option stream_option[] = {
    {FLAG_TRANSCODE_HELP, no_argument, 0, HELP_ARG}, {FLAG_TRANSCODE_NO_OF_STREAMS, required_argument, 0, NO_OF_STREAMS}, {0, 0, 0, 0}};

XlnxTranscoderCtx        multi_trans_ctx[STREAMS];
XlnxTranscoderProperties multi_trans_props[STREAMS];

/*-----------------------------------------------------------------------------
xlnx_tran_app_close: Deinitialize transcoder application

Parameters:
transcode_ctx: Transcoder context
-----------------------------------------------------------------------------*/
static void xlnx_tran_app_close(XlnxTranscoderCtx* transcode_ctx, XlnxTranscoderProperties* transcode_props, XlnxLogHandle trans_log) {
  /* Deinitialize all the modules and release the resources */
  xrm_dec_release_v2(&transcode_ctx->dec_ctx.xrm_dec_ctx);
  xlnx_dec_deinit(&transcode_ctx->dec_ctx, &transcode_props->xma_dec_props);
  if (transcode_ctx->num_scal_out) {
    xrm_scale_release_v2(&transcode_ctx->scal_ctx.scaler_xrm_ctx);
    xlnx_scal_deinit(&transcode_ctx->scal_ctx, &transcode_props->xma_scal_props);
  }
  for (int32_t i = 0; i < transcode_ctx->num_enc_channels; i++) {
    xrm_enc_release_v2(&transcode_ctx->enc_ctx[i].xrm_enc_ctx);
    xlnx_enc_deinit(&transcode_ctx->enc_ctx[i], &transcode_props->xma_enc_props[i]);
  }
  xma_release(trans_log.handle);
  trans_log.handle = NULL;
  xma_log_release(trans_log.log);
  trans_log.log = NULL;
  return;
}

static int check_xrm_resources(XlnxTranscoderCtx* transcode_ctx, XlnxTranscoderProperties* transcode_props) {
  int               ret           = XMA_APP_SUCCESS;
  int               dev_index     = transcode_ctx->trans_handle.dev_index;
  XrmDecodePropsV2* xrm_props_dec = (XrmDecodePropsV2*) xrm_props_create(XRM_IP_DECODER);

  const XmaDecoderProperties* xma_dec_props = &transcode_props->xma_dec_props;
  xrm_props_dec->input.width                = xma_dec_props->width;
  xrm_props_dec->input.height               = xma_dec_props->height;
  xrm_props_dec->input.fps_num              = xma_dec_props->framerate.numerator;
  xrm_props_dec->input.fps_den              = xma_dec_props->framerate.denominator;
  xrm_props_dec->dev_index                  = dev_index;
  if (xrm_dec_reserve_v2(&transcode_ctx->dec_ctx.xrm_dec_ctx, xrm_props_dec) == XRM_ERROR) {
    return XMA_APP_ERROR;
  }
  xrm_props_destroy((void*) &xrm_props_dec);

  const XmaScalerProperties* xma_scale_props = &transcode_props->xma_scal_props;
  if (xma_scale_props->num_outputs > 0) {
    XrmScalePropsV2* xrm_props_scl = (XrmScalePropsV2*) xrm_props_create(XRM_IP_SCALER);

    xrm_props_scl->input.width   = xma_scale_props->input.width;
    xrm_props_scl->input.height  = xma_scale_props->input.height;
    xrm_props_scl->input.fps_num = xma_scale_props->input.framerate.numerator;
    xrm_props_scl->input.fps_den = xma_scale_props->input.framerate.denominator;
    xrm_props_scl->dev_index     = dev_index;
    xrm_props_scl->num_outputs   = xma_scale_props->num_outputs;

    for (int i = 0; i < xma_scale_props->num_outputs; i++) {
      const XmaScalerInOutProperties* out_props = &xma_scale_props->output[i];
      xrm_props_scl->output[i].width            = out_props->width;
      xrm_props_scl->output[i].height           = out_props->height;
      xrm_props_scl->output[i].fps_num          = out_props->framerate.numerator;
      xrm_props_scl->output[i].fps_den          = out_props->framerate.denominator;
    }
    if (xrm_scale_reserve_v2(&transcode_ctx->scal_ctx.scaler_xrm_ctx, xrm_props_scl) == XRM_ERROR) {
      return XMA_APP_ERROR;
    }
    xrm_props_destroy((void*) &xrm_props_scl);
  }

  for (int32_t i = 0; i < transcode_ctx->num_enc_channels; i++) {
    XrmEncodePropsV2* xrm_props_enc = (XrmEncodePropsV2*) xrm_props_create(XRM_IP_ENCODER);

    const XmaEncoderProperties* xma_enc_props = &transcode_props->xma_enc_props[i];
    xrm_props_enc->input.width                = xma_enc_props->width;
    xrm_props_enc->input.height               = xma_enc_props->height;
    xrm_props_enc->input.fps_num              = xma_enc_props->framerate.numerator;
    xrm_props_enc->input.fps_den              = xma_enc_props->framerate.denominator;
    xrm_props_enc->is_la_enabled              = xma_enc_props->lookahead_depth != 0;
    XlnxEncoderProperties* enc_props          = &transcode_ctx->enc_ctx[i].enc_props;
    bool                   is_xav1            = enc_props->device_type == ENC_AV1_DEVICE_TYPE_1 && xma_enc_props->hwencoder_type == XMA_AV1_ENCODER_TYPE;
    xrm_props_enc->is_av1_type1               = is_xav1;
    xrm_props_enc->enc_cores                  = enc_props->num_cores;
    xrm_props_enc->dev_index                  = dev_index;
    xrm_props_enc->slice_id                   = enc_props->slice;
    strncpy(xrm_props_enc->preset, enc_props->enc_preset, sizeof(xrm_props_enc->preset) - 1);
    ret = xrm_enc_reserve_v2(&transcode_ctx->enc_ctx[i].xrm_enc_ctx, xrm_props_enc);
    if (ret == XRM_ERROR) {
      return XMA_APP_ERROR;
    }
    if (ret == XRM_SUCCESS) {
      enc_props->slice = transcode_ctx->enc_ctx[i].xrm_enc_ctx.slice_id;
    }
    if (enc_props->slice == DEFAULT_SLICE_ID) { // xrm wasn't used, default to slice 0
      enc_props->slice = XMA_ENC_SLICE_DEFAULT;
    }
    ret = XMA_APP_SUCCESS;
    xrm_props_destroy((void*) &xrm_props_enc);
  }
  return ret;
}

/*-----------------------------------------------------------------------------
 ******************** Transcoder Application ***********************************
 -----------------------------------------------------------------------------*/
int32_t main(int32_t argc, char* argv[]) {
  int32_t       ret = XMA_APP_SUCCESS;
  int           option_index;
  int           number_of_streams;
  XlnxLogHandle trans_log;

  if (argc < 2) {
    fprintf(stderr, "%s\n", xlnx_tran_get_help());
    exit(XMA_APP_SUCCESS);
  }

  int flag = getopt_long_only(argc, argv, "", stream_option, &option_index);
  if (flag == -1) {
    return XMA_APP_ERROR;
  }

  switch (flag) {
  case NO_OF_STREAMS:
    ret = xlnx_utils_set_int_arg(&number_of_streams, optarg, FLAG_TRANSCODE_NO_OF_STREAMS);
    break;
  case HELP_ARG:
    printf("%s\n", xlnx_tran_get_help());
    exit(0);
  default:
    fprintf(stderr, "Error in parsing generic transcoder arguments %d \n", flag);
    return XMA_APP_ERROR;
  }

  if (flag == XMA_APP_ERROR || ret == XMA_APP_ERROR) {
    return XMA_APP_ERROR;
  }

  for (int stream_no = 0; stream_no < number_of_streams; stream_no++) {
    memset(&multi_trans_ctx[stream_no], 0, sizeof(multi_trans_ctx[stream_no]));
    /* Parsing command line and updating decoder, scaler and encoder context */
    ret = xlnx_tran_parser(argc, argv, &multi_trans_ctx[stream_no], &multi_trans_props[stream_no], stream_no, number_of_streams);
    if (ret != XMA_APP_SUCCESS) {
      return XMA_APP_ERROR;
    }
  }

  trans_log = multi_trans_ctx[0].trans_handle;
  ret       = xma_log_init(trans_log.log_level, trans_log.log_location, &trans_log.log, trans_log.log_file);
  if (ret != XMA_APP_SUCCESS) {
    printf("XMA Initialization failed\n");
    return XMA_APP_ERROR;
  }
  multi_trans_ctx[0].trans_handle.log = trans_log.log;

  for (int stream_no = 0; stream_no < number_of_streams; stream_no++) {
    ret = check_xrm_resources(&multi_trans_ctx[stream_no], &multi_trans_props[stream_no]);
    if (ret != XMA_APP_SUCCESS) {
      for (int j = 0; j <= stream_no; j++) {
        xlnx_tran_app_close(&multi_trans_ctx[j], &multi_trans_props[j], trans_log);
      }
      return XMA_APP_ERROR;
    }
  }

  XmaInitParameter xma_init_param;
  char             m_app_name[32];
  memset(&xma_init_param, 0, sizeof(XmaInitParameter));
  strcpy(m_app_name, "xma_transcoder_app");
  xma_init_param.app_name = m_app_name;
  xma_init_param.device   = trans_log.dev_index;

  XmaParameter params[1];
  uint32_t     api_version = XMA_API_VERSION_1_2_1;

  params[0].name   = (char*) XMA_API_VERSION;
  params[0].type   = XMA_UINT32;
  params[0].length = sizeof(uint32_t);
  params[0].value  = &api_version;

  xma_init_param.params    = params;
  xma_init_param.param_cnt = 1;

  if ((ret = xma_initialize(trans_log.log, &xma_init_param, &trans_log.handle)) != XMA_SUCCESS) {
    xma_logmsg(trans_log.log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "XMA Initialization failed\n");
    return XMA_APP_ERROR;
  }

  for (int stream_no = 0; stream_no < number_of_streams; stream_no++) {
    multi_trans_ctx[stream_no].trans_handle.log    = trans_log.log;
    multi_trans_ctx[stream_no].handle              = trans_log.handle;
    multi_trans_ctx[stream_no].trans_handle.handle = trans_log.handle;
    /* Session creation for decoder, scaler, LA and encoder */
    ret = xlnx_tran_session_create(&multi_trans_ctx[stream_no], &multi_trans_props[stream_no]);
    if (ret != XMA_APP_SUCCESS) {
      xlnx_tran_app_close(&multi_trans_ctx[stream_no], &multi_trans_props[stream_no], trans_log);
      return XMA_APP_ERROR;
    }
  }

  ret = xlnx_tran_frame_process(multi_trans_ctx);

  for (int stream_no = 0; stream_no < number_of_streams; stream_no++) {
    xlnx_tran_app_close(&multi_trans_ctx[stream_no], &multi_trans_props[stream_no], trans_log);
  }

  return 0;
}
