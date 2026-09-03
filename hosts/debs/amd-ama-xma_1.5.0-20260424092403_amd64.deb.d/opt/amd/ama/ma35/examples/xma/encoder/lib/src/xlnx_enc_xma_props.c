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

#include "xlnx_enc_xma_props.h"

/**
 * xlnx_enc_free_xma_props: Function to deinitialize XMA encoder properties
 *
 * @param xma_enc_props: XMA encoder properties
 * @return None
 */
void xlnx_enc_free_xma_props(XmaEncoderProperties* xma_enc_props) {
  if (xma_enc_props->params) {
    free(xma_enc_props->params);
  }

  return;
}

/**
 * xlnx_enc_xma_params_update: Update encoder custom params
 *
 * @param enc_props: Encoder properties
 * @param custom_xma_params: XMA encoder custom parameters
 * @return XMA_SUCCESS or XMA_ERROR
 */
static int32_t xlnx_enc_fill_custom_xma_params(XlnxEncoderProperties* enc_props, XmaParameter* custom_xma_params, uint32_t* param_cnt) {
  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_SLICE;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(enc_props->slice);
  custom_xma_params[*param_cnt].value  = &(enc_props->slice);
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_CORES;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(enc_props->num_cores);
  custom_xma_params[*param_cnt].value  = &(enc_props->num_cores);
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_SPATIAL_AQ;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(enc_props->spatial_aq);
  custom_xma_params[*param_cnt].value  = &(enc_props->spatial_aq);
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_TEMPORAL_AQ;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(enc_props->temporal_aq);
  custom_xma_params[*param_cnt].value  = &(enc_props->temporal_aq);
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_LATENCY_LOGGING;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(enc_props->latency_logging);
  custom_xma_params[*param_cnt].value  = &(enc_props->latency_logging);

  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_TUNE_METRICS;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(enc_props->tune_metrics);
  custom_xma_params[*param_cnt].value  = &(enc_props->tune_metrics);
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_QP_MODE;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(enc_props->qp_mode);
  custom_xma_params[*param_cnt].value  = &(enc_props->qp_mode);
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_FORCED_IDR;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(enc_props->force_idr);
  custom_xma_params[*param_cnt].value  = &(enc_props->force_idr);
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_CRF;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(enc_props->crf);
  custom_xma_params[*param_cnt].value  = &(enc_props->crf);
  (*param_cnt)++;

  if (enc_props->cabr_config) {
    custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_CABR_CONFIG;
    custom_xma_params[*param_cnt].type   = XMA_STRING;
    custom_xma_params[*param_cnt].length = strlen(enc_props->cabr_config) + 1;
    custom_xma_params[*param_cnt].value  = &(enc_props->cabr_config);
    (*param_cnt)++;
  }

  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_MAX_BITRATE;
  custom_xma_params[*param_cnt].type   = XMA_INT64;
  custom_xma_params[*param_cnt].length = sizeof(enc_props->max_bitrate);
  custom_xma_params[*param_cnt].value  = &(enc_props->max_bitrate);
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_BF;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(enc_props->num_bframes);
  custom_xma_params[*param_cnt].value  = &(enc_props->num_bframes);
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_DYNAMIC_GOP;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(enc_props->dynamic_gop);
  custom_xma_params[*param_cnt].value  = &(enc_props->dynamic_gop);
  (*param_cnt)++;

  if (strlen(enc_props->expert_options) > 0) {
    custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_EXPERT_OPTIONS;
    custom_xma_params[*param_cnt].type   = XMA_STRING;
    custom_xma_params[*param_cnt].length = strlen(enc_props->expert_options) + 1;
    custom_xma_params[*param_cnt].value  = &(enc_props->expert_options);
    (*param_cnt)++;
  }

  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_LATENCY_MS;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(enc_props->latency_ms);
  custom_xma_params[*param_cnt].value  = &(enc_props->latency_ms);
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_NO_LOWLAT_BFRAMES;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(enc_props->no_low_latency_b_frames);
  custom_xma_params[*param_cnt].value  = &(enc_props->no_low_latency_b_frames);
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_BUFSIZE;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(enc_props->bufsize);
  custom_xma_params[*param_cnt].value  = &(enc_props->bufsize);
  (*param_cnt)++;

  if (enc_props->codec_id == ENCODER_ID_AV1) {
    custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_DEVICE_TYPE;
    custom_xma_params[*param_cnt].type   = XMA_UINT32;
    custom_xma_params[*param_cnt].length = sizeof(enc_props->device_type);
    custom_xma_params[*param_cnt].value  = &(enc_props->device_type);
    (*param_cnt)++;
  }

  return XMA_SUCCESS;
}

static int32_t xlnx_enc_create_basic_xma_props(XlnxEncoderProperties* enc_props, XmaFilterProperties* xma_upload_props, XmaEncoderProperties* xma_enc_props) {
  /* Initialize upload properties */
  xma_upload_props->hwfilter_type                = XMA_UPLOAD_FILTER_TYPE;
  xma_upload_props->param_cnt                    = 0;
  xma_upload_props->params                       = NULL;
  xma_upload_props->input.format                 = enc_props->pix_fmt;
  xma_upload_props->input.sw_format              = enc_props->pix_fmt;
  xma_upload_props->input.width                  = enc_props->width;
  xma_upload_props->input.height                 = enc_props->height;
  xma_upload_props->input.framerate.numerator    = enc_props->fps;
  xma_upload_props->input.framerate.denominator  = 1;
  xma_upload_props->output.format                = XMA_VPE_FMT_TYPE;
  xma_upload_props->output.sw_format             = enc_props->pix_fmt;
  xma_upload_props->output.width                 = enc_props->width;
  xma_upload_props->output.height                = enc_props->height;
  xma_upload_props->output.framerate.numerator   = enc_props->fps;
  xma_upload_props->output.framerate.denominator = 1;
  xma_enc_props->hwencoder_type                  = enc_props->codec_id;
  xma_enc_props->param_cnt                       = 0;
  xma_enc_props->params                          = (XmaParameter*) calloc(1, ENC_MAX_PARAMS * sizeof(XmaParameter));
  xma_enc_props->format                          = XMA_VPE_FMT_TYPE;
  xma_enc_props->sw_format                       = enc_props->pix_fmt;
  xma_enc_props->width                           = enc_props->width;
  xma_enc_props->height                          = enc_props->height;
  xma_enc_props->bitrate                         = enc_props->bitrate * 1000;
  xma_enc_props->lookahead_depth                 = enc_props->lookahead_depth;
  xma_enc_props->framerate.numerator             = enc_props->fps;
  xma_enc_props->framerate.denominator           = 1;
  xma_enc_props->preset                          = enc_props->preset;
  xma_enc_props->profile                         = enc_props->profile;
  xma_enc_props->level                           = enc_props->level;
  xma_enc_props->gop_size                        = enc_props->gop_size;
  xma_enc_props->qp                              = enc_props->qp;
  xma_enc_props->minQP                           = enc_props->min_qp;
  xma_enc_props->maxQP                           = enc_props->max_qp;
  xma_enc_props->temp_aq_gain                    = enc_props->temp_aq_gain;
  xma_enc_props->spat_aq_gain                    = enc_props->spat_aq_gain;
  xma_enc_props->rc_mode                         = enc_props->rc_mode;

  if (enc_props->max_bitrate > 0) {
    enc_props->max_bitrate = enc_props->max_bitrate * 1000;
  }

  return XMA_SUCCESS;
}

/**
 * xlnx_enc_create_xma_props: Create the xma encoder properties using the given
 * xilinx encoder properties.
 *
 * @param enc_props: Encoder properties
 * @param xma_enc_props: XMA encoder properties to be created
 * @return XMA_SUCCESS or XMA_ERROR
 */
int32_t xlnx_enc_create_xma_props(XmaHandle handle, XlnxEncoderProperties* enc_props, XmaFilterProperties* xma_upload_props, XmaEncoderProperties* xma_enc_props) {
  xma_upload_props->handle = handle;
  xma_enc_props->handle    = handle;
  xlnx_enc_create_basic_xma_props(enc_props, xma_upload_props, xma_enc_props);
  if (xlnx_enc_fill_custom_xma_params(enc_props, xma_enc_props->params, &xma_enc_props->param_cnt) != XMA_SUCCESS) {
    return XMA_ERROR;
  }
  return XMA_SUCCESS;
}
