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

#include "xlnx_transcoder_xma_props.h"

int32_t xlnx_dec_res_chng_callback(XmaFrameProperties*, void*) {
  return XMA_SUCCESS;
}

/**
 * xlnx_dec_xma_params_update: Update decoder custom params
 *
 * @param dec_props: Decoder properties
 * @param xma_dec_props: XMA decoder properties
 * @return None
 */
static void xlnx_dec_xma_params_update(XlnxDecoderProperties* dec_props, XmaParameter* custom_xma_params, uint32_t* param_cnt) {
  custom_xma_params[*param_cnt].name   = XMA_DEC_PARAM_LOW_LATENCY;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(dec_props->low_latency);
  custom_xma_params[*param_cnt].value  = &(dec_props->low_latency);
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = XMA_DEC_PARAM_PROP_CHANGE_CALLBACK;
  custom_xma_params[*param_cnt].type   = XMA_FUNC_PTR;
  custom_xma_params[*param_cnt].length = sizeof(void*);
  custom_xma_params[*param_cnt].value  = xlnx_dec_res_chng_callback;
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = XMA_DEC_PARAM_LATENCY_LOGGING;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(dec_props->latency_logging);
  custom_xma_params[*param_cnt].value  = &(dec_props->latency_logging);
  (*param_cnt)++;

  if (dec_props->out_pix_fmt != XMA_NONE_FMT_TYPE) {
    custom_xma_params[*param_cnt].name   = XMA_DEC_OUTPUT_FORMAT;
    custom_xma_params[*param_cnt].type   = XMA_INT32;
    custom_xma_params[*param_cnt].length = sizeof(dec_props->out_pix_fmt);
    custom_xma_params[*param_cnt].value  = &(dec_props->out_pix_fmt);
    (*param_cnt)++;
  }

  if (dec_props->resize_width && dec_props->resize_height) {
    custom_xma_params[*param_cnt].name   = XMA_DEC_PARAM_RESIZE_WIDTH;
    custom_xma_params[*param_cnt].type   = XMA_UINT32;
    custom_xma_params[*param_cnt].length = sizeof(dec_props->resize_width);
    custom_xma_params[*param_cnt].value  = &(dec_props->resize_width);
    (*param_cnt)++;

    custom_xma_params[*param_cnt].name   = XMA_DEC_PARAM_RESIZE_HEIGHT;
    custom_xma_params[*param_cnt].type   = XMA_UINT32;
    custom_xma_params[*param_cnt].length = sizeof(dec_props->resize_height);
    custom_xma_params[*param_cnt].value  = &(dec_props->resize_height);
    (*param_cnt)++;
  }

  return;
}

/**
 * xlnx_dec_get_xma_props:  Populate XmaDecoderProperties struct from decoder
 * properties
 *
 * @param handle: XMA handle
 * @param dec_props: decoder properties
 * @param xma_dec_props: XMA decoder properties
 * @return None
 */
void xlnx_dec_get_xma_props(XmaHandle handle, XlnxDecoderProperties* dec_props, XmaDecoderProperties* xma_dec_props) {
  xma_dec_props->width                 = dec_props->width;
  xma_dec_props->height                = dec_props->height;
  xma_dec_props->bits_per_pixel        = dec_props->bit_depth;
  xma_dec_props->framerate.numerator   = dec_props->fps;
  xma_dec_props->framerate.denominator = 1;
  xma_dec_props->hwdecoder_type        = dec_props->codec_type;
  xma_dec_props->handle                = handle;
  xma_dec_props->param_cnt             = 0;
  xma_dec_props->params                = calloc(1, DEC_MAX_PARAMS * sizeof(XmaParameter));
  if (dec_props->out_pix_fmt == XMA_NV12_FMT_TYPE) {
    dec_props->planar = 0;
  } else {
    dec_props->planar = 1;
  }

  xlnx_dec_xma_params_update(dec_props, xma_dec_props->params, &xma_dec_props->param_cnt);
  return;
}

/**
 * xlnx_scal_xma_params_update: Update scaler custom params
 *
 * @param scal_props: Scaler properties
 * @param custom_xma_params: XMA custom parameters
 * @return None
 */
static void xlnx_scal_xma_params_update(XlnxScalerProperties* scal_props, XmaParameter* custom_xma_params, uint32_t* param_cnt) {
  custom_xma_params[*param_cnt].name   = XMA_SCALER_PARAM_LATENCY_LOGGING;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(scal_props->latency_logging);
  custom_xma_params[*param_cnt].value  = &(scal_props->latency_logging);
  (*param_cnt)++;

  scal_props->threads                  = 0;
  custom_xma_params[*param_cnt].name   = XMA_SCALER_PARAM_THREADS;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(scal_props->threads);
  custom_xma_params[*param_cnt].value  = &(scal_props->threads);
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = (char*) XMA_SCALER_PARAM_MIX_RATE;
  custom_xma_params[*param_cnt].length = sizeof(void*);

  return;
}

/**
 * xlnx_scal_get_xma_props:  Populate XmaScalerProperties struct from scaler
 * properties
 *
 * @param handle: XMA handle
 * @param scal_props: scaler properties
 * @param xma_scal_props: XMA scaler properties
 * @return None
 */
void xlnx_scal_get_xma_props(XmaHandle handle, XlnxScalerProperties* scal_props, XmaScalerProperties* xma_scal_props) {
  xma_scal_props->hwscaler_type               = XMA_ABR_SCALER_TYPE;
  xma_scal_props->param_cnt                   = 0;
  xma_scal_props->params                      = (XmaParameter*) calloc(1, SCAL_MAX_PARAMS * sizeof(XmaParameter));
  xma_scal_props->handle                      = handle;
  xma_scal_props->num_outputs                 = scal_props->nb_outputs;
  xma_scal_props->input.format                = XMA_VPE_FMT_TYPE;
  xma_scal_props->input.sw_format             = scal_props->xma_fmt_type;
  xma_scal_props->input.width                 = scal_props->in_width;
  xma_scal_props->input.height                = scal_props->in_height;
  xma_scal_props->input.bits_per_pixel        = scal_props->bits_per_pixel;
  xma_scal_props->input.framerate.numerator   = scal_props->fr_num;
  xma_scal_props->input.framerate.denominator = scal_props->fr_den;

  for (int32_t i = 0; i < scal_props->nb_outputs; i++) {
    xma_scal_props->output[i].format         = XMA_VPE_FMT_TYPE;
    xma_scal_props->output[i].sw_format      = scal_props->xma_fmt_type;
    xma_scal_props->output[i].bits_per_pixel = scal_props->bits_per_pixel;
    xma_scal_props->output[i].width          = scal_props->out_width[i];
    xma_scal_props->output[i].height         = scal_props->out_height[i];

    xma_scal_props->output[i].stride                = scal_props->out_width[i];
    xma_scal_props->output[i].framerate.numerator   = scal_props->fr_num;
    xma_scal_props->output[i].framerate.denominator = scal_props->fr_den;
  }

  xlnx_scal_xma_params_update(scal_props, xma_scal_props->params, &xma_scal_props->param_cnt);

  return;
}

/**
 * xlnx_enc_xma_params_update: Update encoder custom params
 *
 * @param enc_props: Encoder properties
 * @param xma_enc_props: XMA encoder properties
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
static void xlnx_enc_xma_params_update(XlnxEncoderProperties* enc_props, XmaParameter* custom_xma_params, uint32_t* param_cnt) {
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
  (*param_cnt)++;

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

  custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_EXPERT_OPTIONS;
  custom_xma_params[*param_cnt].type   = XMA_STRING;
  custom_xma_params[*param_cnt].length = strlen(enc_props->expert_options);
  custom_xma_params[*param_cnt].value  = &(enc_props->expert_options);
  (*param_cnt)++;

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

  if (enc_props->codec_id == XMA_AV1_ENCODER_TYPE) {
    custom_xma_params[*param_cnt].name   = XMA_ENC_PARAM_DEVICE_TYPE;
    custom_xma_params[*param_cnt].type   = XMA_INT32;
    custom_xma_params[*param_cnt].length = sizeof(enc_props->device_type);
    custom_xma_params[*param_cnt].value  = &(enc_props->device_type);
    (*param_cnt)++;
  }

  return;
}

/**
 * xlnx_enc_get_xma_props: Populate XmaEncoderProperties struct from encoder
 * properties
 *
 * @param handle: XMA handle
 * @param enc_props: Encoder properties
 * @param xma_enc_props: XMA encoder properties
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
int32_t xlnx_enc_get_xma_props(XmaHandle handle, XlnxEncoderProperties* enc_props, XmaEncoderProperties* xma_enc_props) {
  xma_enc_props->handle = handle;
  /* Initialize encoder properties */
  xma_enc_props->hwencoder_type        = enc_props->codec_id;
  xma_enc_props->param_cnt             = 0;
  xma_enc_props->params                = (XmaParameter*) calloc(1, ENC_MAX_PARAMS * sizeof(XmaParameter));
  xma_enc_props->format                = XMA_VPE_FMT_TYPE;
  xma_enc_props->sw_format             = enc_props->pix_fmt;
  xma_enc_props->width                 = enc_props->width;
  xma_enc_props->height                = enc_props->height;
  xma_enc_props->bitrate               = enc_props->bitrate * 1000;
  xma_enc_props->lookahead_depth       = enc_props->lookahead_depth;
  xma_enc_props->framerate.numerator   = enc_props->fps;
  xma_enc_props->framerate.denominator = 1;
  xma_enc_props->preset                = enc_props->preset;
  xma_enc_props->profile               = enc_props->profile;
  xma_enc_props->level                 = enc_props->level;
  xma_enc_props->gop_size              = enc_props->gop_size;
  xma_enc_props->qp                    = enc_props->qp;
  xma_enc_props->rc_mode               = enc_props->rc_mode;
  xma_enc_props->minQP                 = enc_props->min_qp;
  xma_enc_props->maxQP                 = enc_props->max_qp;
  xma_enc_props->bits_per_pixel        = enc_props->bits_per_pixel;
  xma_enc_props->temp_aq_gain          = enc_props->temp_aq_gain;
  xma_enc_props->spat_aq_gain          = enc_props->spat_aq_gain;

  xma_enc_props->framerate.numerator   = enc_props->fps;
  xma_enc_props->framerate.denominator = 1;

  if (enc_props->max_bitrate > 0) {
    enc_props->max_bitrate = enc_props->max_bitrate * 1000;
  }

  xlnx_enc_xma_params_update(enc_props, xma_enc_props->params, &xma_enc_props->param_cnt);

  return XMA_APP_SUCCESS;
}

/**
 * xlnx_dec_free_xma_props: Function to deinitialize XMA decoder properties
 *
 * @param xma_dec_props: XMA decoder properties
 * @return None
 */
void xlnx_dec_free_xma_props(XmaDecoderProperties* xma_dec_props) {
  if (xma_dec_props->params) {
    free(xma_dec_props->params);
  }

  return;
}

/**
 * xlnx_scal_free_xma_props: Function to deinitialize XMA scaler properties
 *
 * @param xma_scal_props: XMA scaler properties
 * @return None
 */
void xlnx_scal_free_xma_props(XmaScalerProperties* xma_scal_props) {
  if (xma_scal_props->params) {
    free(xma_scal_props->params);
  }

  return;
}

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
