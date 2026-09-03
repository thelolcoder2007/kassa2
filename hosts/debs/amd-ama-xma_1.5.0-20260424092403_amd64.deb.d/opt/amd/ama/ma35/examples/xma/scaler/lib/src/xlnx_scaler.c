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

#include "xlnx_scaler.h"

/**
 * xlnx_scal_cleanup_scaler_ctx: Cleanup the scaler context - free xma frames,
 * destroy scaler session, free xrm resources.
 *
 * @param ctx: The scaler context
 */
void xlnx_scal_cleanup_scaler_ctx(XlnxScalerCtx* ctx) {
  if (!ctx) {
    return;
  }
  if (ctx->upload_session) {
    xma_filter_session_destroy(ctx->upload_session);
  }
  for (uint i = 0; i < ctx->abr_params.num_outputs[0]; i++) {
    if (ctx->download_sessions[i]) {
      xma_filter_session_destroy(ctx->download_sessions[i]);
    }
  }
  for (int session_id = 0; session_id < ctx->num_sessions; session_id++) {
    if (ctx->session[session_id]) {
      xma_scaler_session_destroy(ctx->session[session_id]);
    }
  }
  if (ctx->handle) {
    xma_release(ctx->handle);
  }
  if (ctx->log) {
    xma_log_release(ctx->log);
  }
  xrm_scale_release_v2(&ctx->scaler_xrm_ctx);
  if (ctx->abr_params.is_halfrate_used) {
    xlnx_scal_cleanup_props(&ctx->abr_xma_props[1]);
  }
  xlnx_scal_cleanup_props(&ctx->abr_xma_props[0]);
  for (uint i = 0; i < ctx->abr_params.num_outputs[0]; i++) {
    if (ctx->output_xframe_list[i]) {
      xma_frame_free(ctx->output_xframe_list[i]);
    }
  }
  if (ctx->input_xframe) {
    xma_frame_free(ctx->input_xframe);
  }
}

/**
 * xlnx_scal_device_init: Get/allocate xrm resources, xma initialize.
 *
 * @param ctx: The scaler context
 * @return XMA_APP_SUCCESS on success XMA_APP_ERROR on error
 */
int32_t xlnx_scal_device_init(XlnxScalerCtx* ctx) {
  int32_t ret = XMA_APP_ERROR;
  // Reserve xrm resource
  XrmScalePropsV2* xrm_props = (XrmScalePropsV2*) xrm_props_create(XRM_IP_SCALER);
  if (!xrm_props) {
    xma_logmsg(ctx->log, XMA_ERROR_LOG, XLNX_SCALER_APP_MODULE, "Failed to create xrm properties\n");
    return XMA_APP_ERROR;
  }
  const XlnxScalerProps* scale_props = &ctx->abr_params;
  xrm_props->input.width             = scale_props->input_width;
  xrm_props->input.height            = scale_props->input_height;
  xrm_props->input.fps_num           = scale_props->input_fps_num;
  xrm_props->input.fps_den           = scale_props->input_fps_den;
  xrm_props->dev_index               = scale_props->device_id;
  xrm_props->num_outputs             = scale_props->num_outputs[0];

  for (int i = 0; i < xrm_props->num_outputs; i++) {
    xrm_props->output[i].width   = scale_props->output_widths[i];
    xrm_props->output[i].height  = scale_props->output_heights[i];
    xrm_props->output[i].fps_num = scale_props->output_fps_nums[i];
    xrm_props->output[i].fps_den = scale_props->output_fps_dens[i];
    if (scale_props->is_halfrate[i]) {
      xrm_props->output[i].fps_den *= 2;
    }
  }

  if (xrm_scale_reserve_v2(&ctx->scaler_xrm_ctx, xrm_props) == XRM_ERROR) {
    return XMA_APP_ERROR;
  }
  xrm_props_destroy((void*) &xrm_props);

  // if(ctx->abr_params.is_halfrate_used) {
  //     ctx->abr_xma_props[1].plugin_lib = ctx->abr_xma_props[0].plugin_lib;
  //     ctx->abr_xma_props[1].dev_index  = ctx->abr_xma_props[0].dev_index;
  //     /* XMA to select the ddr bank based on xclbin meta data */
  //     ctx->abr_xma_props[1].ddr_bank_index =
  //         ctx->abr_xma_props[0].ddr_bank_index;
  //     ctx->abr_xma_props[1].cu_index   = ctx->abr_xma_props[0].cu_index;
  //     ctx->abr_xma_props[1].channel_id = ctx->abr_xma_props[0].channel_id;
  // }
  XmaInitParameter xma_init_param;
  memset(&xma_init_param, 0, sizeof(XmaInitParameter));
  char m_app_name[32];
  strcpy(m_app_name, XLNX_SCALER_APP_MODULE);
  xma_init_param.app_name = m_app_name;
  xma_init_param.device   = ctx->abr_params.device_id;

  XmaParameter params[1];
  uint32_t     api_version = XMA_API_VERSION_1_1;

  params[0].name   = (char*) XMA_API_VERSION;
  params[0].type   = XMA_UINT32;
  params[0].length = sizeof(uint32_t);
  params[0].value  = &api_version;

  xma_init_param.params    = params;
  xma_init_param.param_cnt = 1;

  if ((ret = xma_initialize(ctx->log, &xma_init_param, &ctx->handle)) != XMA_SUCCESS) {
    xma_logmsg(ctx->log, XMA_ERROR_LOG, XLNX_SCALER_APP_MODULE, "XMA Initialization failed\n");
    return XMA_APP_ERROR;
  }
  xma_logmsg(ctx->log, XMA_INFO_LOG, XLNX_SCALER_APP_MODULE, "XMA initialization success\n");

  ctx->abr_xma_props[0].handle = ctx->handle;
  ctx->xma_upload_props.handle = ctx->handle;
  for (uint output_id = 0; output_id < ctx->abr_params.num_outputs[0]; output_id++) {
    ctx->xma_download_props[output_id].handle = ctx->handle;
  }
  if (ctx->abr_params.is_halfrate_used) {
    ctx->abr_xma_props[1].handle = ctx->handle;
  }
  return ret;
}

/**
 * xlnx_scal_create_frame_props: Allocate the xframes for input/output
 *
 * @param ctx: The scaler context
 * @return XMA_APP_SUCCESS on success XMA_APP_ERROR on error.
 */
static int xlnx_scal_create_frame_props(XlnxScalerCtx* ctx) {
  XmaScalerProperties props  = ctx->abr_xma_props[0];
  XmaFrameProperties* fprops = &ctx->input_fprops;
  // Create an input frame for abr scaler
  fprops->width          = props.input.width;
  fprops->height         = props.input.height;
  fprops->format         = props.input.format;
  fprops->sw_format      = props.input.sw_format;
  fprops->bits_per_pixel = props.input.bits_per_pixel;
  for (int plane_id = 0; plane_id < xma_frame_planes_get(ctx->handle, fprops); plane_id++) {
    fprops->linesize[plane_id] = xma_frame_get_plane_stride(ctx->handle, fprops, plane_id);
  }
  ctx->upload_fprops        = ctx->input_fprops;
  ctx->upload_fprops.format = ctx->upload_fprops.sw_format;

  // Create an array of output frames for abr scaler
  for (uint output_id = 0; output_id < ctx->abr_params.num_outputs[0]; output_id++) {
    fprops                 = &ctx->output_fprops[output_id];
    fprops->width          = props.output[output_id].width;
    fprops->height         = props.output[output_id].height;
    fprops->format         = props.output[output_id].format;
    fprops->sw_format      = props.output[output_id].sw_format;
    fprops->bits_per_pixel = props.output[output_id].bits_per_pixel;
    for (int plane_id = 0; plane_id < xma_frame_planes_get(ctx->handle, fprops); plane_id++) {
      fprops->linesize[plane_id] = xma_frame_get_plane_stride(ctx->handle, fprops, plane_id);
    }
    ctx->download_fprops[output_id]        = ctx->output_fprops[output_id];
    ctx->download_fprops[output_id].format = ctx->download_fprops[output_id].sw_format;
  }
  return XMA_APP_SUCCESS;
}

/**
 * xlnx_scal_create_scaler_ctx: Uses ctx->abr_params to create the rest of the
 * scaler context. Does not init the device/create session.
 * @param ctx: A pointer to the scaler context
 * @return XMA_APP_SUCCESS on success XMA_APP_ERROR on error
 */
int32_t xlnx_scal_create_scaler_ctx(XlnxScalerCtx* ctx) {
  int ret = XMA_APP_ERROR;
  if (ctx->abr_params.is_halfrate_used > 0) {
    ctx->num_sessions = 2;
  } else {
    ctx->num_sessions = 1;
  }
  for (int session_id = 0; session_id < ctx->num_sessions; session_id++) {
    ret = xlnx_scal_create_xma_props(ctx->handle, &ctx->abr_params, session_id, &ctx->xma_upload_props, &ctx->xma_download_props[0], &ctx->abr_xma_props[session_id]);
    if (ret != XMA_APP_SUCCESS) {
      return ret;
    }
  }
  ret = xlnx_scal_create_frame_props(ctx);
  return ret;
}

/**
 * xlnx_scal_create_scaler_sessions: Creates the scaler sessions using the
 * scaler props. One session for all rates, another for full rate only (If there
 * are half rate sessions specified.)
 * @param ctx: A pointer to the scaler context
 * @return XMA_APP_SUCCESS on success XMA_APP_ERROR on error
 */
int32_t xlnx_scal_create_scaler_sessions(XlnxScalerCtx* ctx) {
  ctx->xma_upload_props.handle = ctx->handle;
  ctx->upload_session          = xma_filter_session_create(&ctx->xma_upload_props);
  if (!ctx->upload_session) {
    xma_logmsg(ctx->log, XMA_ERROR_LOG, XLNX_SCALER_APP_MODULE, "Failed to create upload session\n");
    return XMA_APP_ERROR;
  }
  for (uint i = 0; i < ctx->abr_params.num_outputs[0]; i++) {
    ctx->xma_download_props[i].handle = ctx->handle;
    ctx->download_sessions[i]         = xma_filter_session_create(&ctx->xma_download_props[i]);
    if (!ctx->download_sessions[i]) {
      xma_logmsg(ctx->log, XMA_ERROR_LOG, XLNX_SCALER_APP_MODULE, "Failed to create download %d session\n", i);
      return XMA_APP_ERROR;
    }
  }
  // Create an abr scaler session based on the requested properties
  for (int session_id = 0; session_id < ctx->num_sessions; session_id++) {
    ctx->abr_xma_props[session_id].handle = ctx->handle;
    if (session_id > 0) {
      ctx->abr_xma_props[session_id].params[0].value = ctx->session[session_id - 1];
    }
    ctx->session[session_id] = xma_scaler_session_create(&ctx->abr_xma_props[session_id]);
    if (!ctx->session[session_id]) {
      xma_logmsg(ctx->log, XMA_ERROR_LOG, XLNX_SCALER_APP_MODULE, "Failed to create scaler session\n");
      return XMA_APP_ERROR;
    }
  }
  return XMA_APP_SUCCESS;
}
