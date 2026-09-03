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

#include "xlnx_scal_xma_props.h"

/**
 * xlnx_scal_cleanup_props: Free resources allocated in the scaler props.
 *
 * @param scaler_xma_props: The xma scaler properties
 */
void xlnx_scal_cleanup_props(XmaScalerProperties* scaler_xma_props) {
  if (scaler_xma_props->params) {
    free(scaler_xma_props->params);
  }
}

/**
 * xlnx_scal_xma_params_update: Update scaler custom params
 *
 * @param scal_props: Scaler properties
 * @param custom_xma_params: XMA custom parameters
 * @return None
 */
static void xlnx_scal_xma_params_update(XlnxScalerProps* scal_props, XmaParameter* custom_xma_params, uint32_t* param_cnt) {
  custom_xma_params[*param_cnt].name   = XMA_SCALER_PARAM_LATENCY_LOGGING;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(scal_props->latency_logging);
  custom_xma_params[*param_cnt].value  = &(scal_props->latency_logging);
  (*param_cnt)++;

  return;
}

/**
 * xlnx_scal_create_props: Fill the input/output fields of the xma scaler
 * props.
 *
 * @param param_ctx: The parameter context containing scaler param values
 * @param session_id: The session id corresponding to the current session
 * @param scaler_xma_props: The scaler xma properties which will be filled
 * @return XMA_APP_SUCCESS on success
 */
static int32_t xlnx_scal_create_props(XmaHandle handle,
    XlnxScalerProps*                            param_ctx,
    int                                         session_id,
    XmaFilterProperties*                        xma_upload_props,
    XmaFilterProperties*                        xma_download_props,
    XmaScalerProperties*                        scaler_xma_props) {
  // setup frame poperties
  scaler_xma_props->hwscaler_type               = XMA_ABR_SCALER_TYPE;
  scaler_xma_props->param_cnt                   = 0;
  scaler_xma_props->params                      = (XmaParameter*) calloc(1, MAX_SCALER_PARAMS * sizeof(XmaParameter));
  scaler_xma_props->num_outputs                 = param_ctx->num_outputs[session_id];
  scaler_xma_props->input.format                = XMA_VPE_FMT_TYPE;
  scaler_xma_props->input.sw_format             = param_ctx->input_pix_fmt;
  scaler_xma_props->input.bits_per_pixel        = param_ctx->input_bits_per_pixel;
  scaler_xma_props->input.width                 = param_ctx->input_width;
  scaler_xma_props->input.height                = param_ctx->input_height;
  scaler_xma_props->input.stride                = param_ctx->input_width;
  scaler_xma_props->input.framerate.numerator   = param_ctx->input_fps_num;
  scaler_xma_props->input.framerate.denominator = param_ctx->input_fps_den;
  if (session_id > 0) {
    scaler_xma_props->input.framerate.numerator /= 2;
    scaler_xma_props->param_cnt        = 1;
    scaler_xma_props->params[0].name   = (char*) XMA_SCALER_PARAM_MIX_RATE;
    scaler_xma_props->params[0].length = sizeof(void*);
  }
  xma_upload_props->hwfilter_type                = XMA_UPLOAD_FILTER_TYPE;
  xma_upload_props->handle                       = handle;
  xma_upload_props->param_cnt                    = 0;
  xma_upload_props->params                       = NULL;
  xma_upload_props->input.bits_per_pixel         = scaler_xma_props->input.bits_per_pixel;
  xma_upload_props->input.sw_format              = scaler_xma_props->input.sw_format;
  xma_upload_props->input.format                 = xma_upload_props->input.sw_format;
  xma_upload_props->input.width                  = scaler_xma_props->input.width;
  xma_upload_props->input.height                 = scaler_xma_props->input.height;
  xma_upload_props->input.framerate.numerator    = scaler_xma_props->input.framerate.numerator;
  xma_upload_props->input.framerate.denominator  = scaler_xma_props->input.framerate.denominator;
  xma_upload_props->output.format                = XMA_VPE_FMT_TYPE;
  xma_upload_props->output.bits_per_pixel        = scaler_xma_props->input.bits_per_pixel;
  xma_upload_props->output.sw_format             = scaler_xma_props->input.sw_format;
  xma_upload_props->output.width                 = scaler_xma_props->input.width;
  xma_upload_props->output.height                = scaler_xma_props->input.height;
  xma_upload_props->output.framerate.numerator   = scaler_xma_props->input.framerate.numerator;
  xma_upload_props->output.framerate.denominator = scaler_xma_props->input.framerate.denominator;

  // assign output params
  int prop_index = 0;
  for (uint output_id = 0; output_id < param_ctx->num_outputs[0]; output_id++) {
    if (param_ctx->is_halfrate[output_id] && session_id > 0) {
      /* We only want to add full rate outputs to the second session */
      continue;
    }
    XmaScalerInOutProperties* out_props        = &scaler_xma_props->output[prop_index];
    out_props->sw_format                       = param_ctx->output_pix_fmts[output_id];
    out_props->format                          = XMA_VPE_FMT_TYPE;
    out_props->bits_per_pixel                  = param_ctx->output_bits_per_pixels[output_id];
    out_props->width                           = param_ctx->output_widths[output_id];
    out_props->height                          = param_ctx->output_heights[output_id];
    out_props->stride                          = param_ctx->output_widths[output_id];
    out_props->framerate.numerator             = scaler_xma_props->input.framerate.numerator;
    out_props->framerate.denominator           = 1;
    XmaFilterProperties* out_dl_props          = &xma_download_props[output_id];
    out_dl_props->hwfilter_type                = XMA_DOWNLOAD_FILTER_TYPE;
    out_dl_props->handle                       = handle;
    out_dl_props->param_cnt                    = 0;
    out_dl_props->params                       = NULL;
    out_dl_props->input.format                 = XMA_VPE_FMT_TYPE;
    out_dl_props->input.sw_format              = out_props->sw_format;
    out_dl_props->input.bits_per_pixel         = out_props->bits_per_pixel;
    out_dl_props->input.width                  = out_props->width;
    out_dl_props->input.height                 = out_props->height;
    out_dl_props->input.framerate.numerator    = out_props->framerate.numerator;
    out_dl_props->input.framerate.denominator  = out_props->framerate.denominator;
    out_dl_props->output.sw_format             = out_props->sw_format;
    out_dl_props->output.format                = out_dl_props->output.sw_format;
    out_dl_props->output.bits_per_pixel        = out_props->bits_per_pixel;
    out_dl_props->output.width                 = out_props->width;
    out_dl_props->output.height                = out_props->height;
    out_dl_props->output.framerate.numerator   = out_props->framerate.numerator;
    out_dl_props->output.framerate.denominator = out_props->framerate.denominator;
    prop_index++;
  }

  xlnx_scal_xma_params_update(param_ctx, scaler_xma_props->params, &scaler_xma_props->param_cnt);
  return XMA_APP_SUCCESS;
}

/**
 * xlnx_scal_create_xma_props: Create the xma scaler properties given the
 * param ctx and session id.
 *
 * @param handle: The xma handle shared between xma sessions.
 * @param param_ctx: The parameter context containing scaler param values
 * @param session_id: The session id corresponding to the current session
 * @param scaler_xma_props: The scaler xma properties which will be filled
 * @return XMA_APP_SUCCESS on success
 */
int32_t xlnx_scal_create_xma_props(XmaHandle handle, XlnxScalerProps* param_ctx, int session_id, XmaFilterProperties* xma_upload_props,
    XmaFilterProperties* xma_download_props, XmaScalerProperties* scaler_xma_props) {
  // Setup custom xma plugin params
  xlnx_scal_create_props(handle, param_ctx, session_id, xma_upload_props, xma_download_props, scaler_xma_props);
  return XMA_APP_SUCCESS;
}
