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
#include "ama_dynamic_params.h"

void xlnx_log_callback(void* opaque, XmaLogLevelType level, const char* name, const char* msg) {
  XlnxEncoderCtx* enc_ctx = (XlnxEncoderCtx*) opaque;
  if ((level == XMA_INFO_LOG) && (strcmp(name, "EncoderSession") == 0) && (enc_ctx->stat_data != NULL)) {
    if (strstr(msg, "PSNR")) {
      double psnr[3];
      if (sscanf(msg, "Encoder PSNR Y=%lf U=%lf V=%lf", &psnr[0], &psnr[1], &psnr[2]) == 3) {
        fprintf(enc_ctx->stat_data, "%f, %f, %f, ", psnr[0], psnr[1], psnr[2]);
      }
    } else if (strstr(msg, "SSIM")) {
      double ssim[3];
      if (sscanf(msg, "Encoder SSIM Y=%lf U=%lf V=%lf", &ssim[0], &ssim[1], &ssim[2]) == 3) {
        fprintf(enc_ctx->stat_data, "%f, %f, %f\n", ssim[0], ssim[1], ssim[2]);
      }
    }
  }
  xma_logmsg(enc_ctx->log, level, name, msg);
}

int32_t xlnx_enc_create_enc_ctx(XmaHandle handle, XlnxEncoderProperties* enc_props, XlnxEncoderCtx* enc_ctx) {
  enc_ctx->enc_props = enc_props;
  if (xlnx_enc_create_xma_props(handle, enc_ctx->enc_props, &enc_ctx->xma_upload_props, &enc_ctx->xma_enc_props) != XMA_APP_SUCCESS) {
    return XMA_APP_ERROR;
  }
  return XMA_APP_SUCCESS;
}

int32_t xlnx_enc_start_device_session(XlnxEncoderCtx* enc_ctx) {
  int ret = XMA_APP_SUCCESS;
  // Reserve xrm resource
  XrmEncodePropsV2*           xrm_props     = (XrmEncodePropsV2*) xrm_props_create(XRM_IP_ENCODER);
  const XmaEncoderProperties* xma_enc_props = &enc_ctx->xma_enc_props;

  xrm_props->input.width   = xma_enc_props->width;
  xrm_props->input.height  = xma_enc_props->height;
  xrm_props->input.fps_num = xma_enc_props->framerate.numerator;
  xrm_props->input.fps_den = xma_enc_props->framerate.denominator;
  xrm_props->is_la_enabled = xma_enc_props->lookahead_depth != 0;

  XlnxEncoderProperties* enc_props = enc_ctx->enc_props;
  bool                   is_xav1   = enc_props->device_type == ENCODER_DEVICE_TYPE_1 && xma_enc_props->hwencoder_type == XMA_AV1_ENCODER_TYPE;
  xrm_props->is_av1_type1          = is_xav1;
  xrm_props->enc_cores             = enc_props->num_cores;
  xrm_props->dev_index             = enc_props->device_id;
  xrm_props->slice_id              = enc_props->slice;
  strncpy(xrm_props->preset, enc_props->enc_preset, sizeof(xrm_props->preset) - 1);

  ret = xrm_enc_reserve_v2(&enc_ctx->xrm_enc_ctx, xrm_props);
  if (ret == XRM_ERROR) {
    return XMA_APP_ERROR;
  }
  if (ret == XRM_SUCCESS) {
    enc_props->slice = enc_ctx->xrm_enc_ctx.slice_id;
  }
  if (enc_props->slice == DEFAULT_SLICE_ID) { // xrm wasn't used, default to slice -1
    enc_props->slice = DEFAULT_SLICE_ID;
  }
  xrm_props_destroy((void*) &xrm_props);

  ret = xma_log_init(XMA_DEBUG_LOG, XMA_LOG_TYPE_CALLBACK, &enc_ctx->filter_log, xlnx_log_callback, enc_ctx);
  if (ret != XMA_SUCCESS) {
    enc_ctx->filter_log = enc_ctx->log;
  }

  XmaInitParameter xma_init_param;
  char             m_app_name[32];
  memset(&xma_init_param, 0, sizeof(XmaInitParameter));
  strcpy(m_app_name, XLNX_ENC_APP_MODULE);
  xma_init_param.app_name = m_app_name;
  xma_init_param.device   = enc_ctx->enc_props->device_id;

  XmaParameter params[1];
  uint32_t     api_version = XMA_API_VERSION_1_2_1;

  params[0].name   = (char*) XMA_API_VERSION;
  params[0].type   = XMA_UINT32;
  params[0].length = sizeof(uint32_t);
  params[0].value  = &api_version;

  xma_init_param.params    = params;
  xma_init_param.param_cnt = 1;
  if ((ret = xma_initialize(enc_ctx->filter_log, &xma_init_param, &enc_ctx->handle)) != XMA_SUCCESS) {
    xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_ENC_APP_MODULE, "XMA Initialization failed\n");
    return ret;
  }
  enc_ctx->xma_enc_props.handle    = enc_ctx->handle;
  enc_ctx->xma_upload_props.handle = enc_ctx->handle;
  enc_ctx->upload_session          = xma_filter_session_create(&enc_ctx->xma_upload_props);
  if (!enc_ctx->upload_session) {
    xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_ENC_APP_MODULE, "Failed to create upload session\n");
    return ret;
  }
  enc_ctx->enc_session = xma_enc_session_create(&enc_ctx->xma_enc_props);
  if (!enc_ctx->enc_session) {
    xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_ENC_APP_MODULE, "Failed to create encoder session\n");
    return XMA_APP_ERROR;
  }
  return ret;
}

/**
 * Send the input xframe to the xma encoder plugin
 *
 * @param enc_ctx      The encoder context
 * @param input_xframe The input xma frame containing raw frame in yuv420p format
 * @return XMA_SUCCESS, XMA_ERROR, XMA_SEND_MORE_DATA, or
 * XMA_EOS on end of stream.
 */
static int32_t xlnx_enc_send_frame(XlnxEncoderCtx* enc_ctx, XmaFrame* input_xframe) {
  int       ret = INT32_MIN;
  XmaFrame* temp_xframe;

  if (input_xframe) {
    ret = xma_filter_session_send_frame(enc_ctx->upload_session, input_xframe);
    if (ret != XMA_SUCCESS) {
      xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_ENC_APP_MODULE, "Upload filter failed to send frame %d\n", enc_ctx->num_frames_sent);
      return ret;
    }
    input_xframe = xma_frame_alloc(enc_ctx->handle, &enc_ctx->enc_frame_props, true);
    if (!input_xframe) {
      xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_ENC_APP_MODULE, "Failed to allocate frame\n");
      return ret;
    }
    do {
      ret = xma_filter_session_recv_frame(enc_ctx->upload_session, input_xframe);
      if (ret == XMA_TRY_AGAIN) {
        usleep(100);
      }
    } while (ret == XMA_TRY_AGAIN);
    if (ret != XMA_SUCCESS) {
      xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_ENC_APP_MODULE, "Upload filter failed to recieve frame %d\n", enc_ctx->num_frames_sent);
      return ret;
    }

    if (enc_ctx->dyn_params_fp) {
      if (!enc_ctx->dyn_params) {
        enc_ctx->dyn_params = xma_side_data_alloc(enc_ctx->handle, XMA_FRAME_SIDE_DATA_DYN_ENC_PARAMS, XMA_HOST_BUFFER_TYPE, sizeof(XmaDynamicEncParams_v2));
      }
      if (ama_encoder_get_dyn_params_v2(enc_ctx->log, enc_ctx->dyn_params_fp, enc_ctx->num_frames_sent, (XmaDynamicEncParams_v2*) enc_ctx->dyn_params->host) == 1) {
        temp_xframe  = input_xframe;
        input_xframe = xma_frame_clone(enc_ctx->handle, input_xframe);
        xma_frame_free(temp_xframe);
        xma_frame_add_side_data(input_xframe, enc_ctx->dyn_params);
        xma_side_data_dec_ref(enc_ctx->dyn_params);
        enc_ctx->dyn_params = NULL;
      }
    }

    ret = xma_enc_session_send_frame(enc_ctx->enc_session, input_xframe);
  } else if (!enc_ctx->flush_sent) {
    ret = xma_enc_session_send_frame(enc_ctx->enc_session, NULL);
    if (ret != XMA_FLUSH_AGAIN) {
      enc_ctx->flush_sent = true;
    }
  }

  if (ret == XMA_ERROR) {
    xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_ENC_APP_MODULE, "Encoder failed to send frame %d\n", enc_ctx->num_frames_sent);
  } else if (ret == XMA_SUCCESS || ret == XMA_SEND_MORE_DATA) {
    if (!enc_ctx->flush_sent) {
      enc_ctx->num_frames_sent++;
    }
  } else if (ret == INT32_MIN) { // It made it through without sending
    ret = XMA_SUCCESS;
  }
  return ret;
}

/**
 * Processes an input xma frame throug the encoder. Begins flushing when the
 * input xma frame datap[0].buffer is NULL.
 *
 * @param enc_ctx      The encoder context
 * @param input_xframe The input xma frame containing raw frame in yuv420p format
 * @param recv_size    Stores how large the output encoded frame was
 * @return Returns the result of the call to receive. XMA_SUCCESS,
 * XMA_ERROR, XMA_SEND_MORE_DATA, or XMA_EOS on end of
 * stream.
 */
int32_t xlnx_enc_process_frame(XlnxEncoderCtx* enc_ctx, XmaFrame* input_xframe, int32_t* recv_size) {
  int ret = xlnx_enc_send_frame(enc_ctx, input_xframe);
  if (ret == XMA_ERROR || ret == XMA_SEND_MORE_DATA) {
    return ret;
  }
  if (!enc_ctx->output_xma_buffer) {
    enc_ctx->output_xma_buffer = xma_data_buffer_alloc(enc_ctx->handle, 0, true);
  }

  int32_t count = 0;
  do {
    ret = xma_enc_session_recv_data(enc_ctx->enc_session, enc_ctx->output_xma_buffer, recv_size);
    if (ret == XMA_RESEND_AND_RECV) {
      usleep(100);
    }
  } while ((ret == XMA_RESEND_AND_RECV) && (count++ < 1000));

  if (ret == XMA_SUCCESS) {
    enc_ctx->num_frames_received++;
  } else if (ret == XMA_ERROR) {
    xma_logmsg(enc_ctx->log, XMA_ERROR_LOG, XLNX_ENC_APP_MODULE, "Failed to receive frame %d\n", enc_ctx->num_frames_received);
  }
  return ret;
}

/**
 * xlnx_enc_deinit: Encoder deinitialize
 *
 * @param enc_ctx: Encoder context
 * @param xma_enc_props: XMA encoder properties
 * @return None
 */
void xlnx_enc_deinit(XlnxEncoderCtx* enc_ctx) {
  if (enc_ctx->upload_session != NULL) {
    xma_filter_session_destroy(enc_ctx->upload_session);
    enc_ctx->upload_session = NULL;
  }

  if (enc_ctx->enc_session != NULL) {
    xma_enc_session_destroy(enc_ctx->enc_session);
    enc_ctx->enc_session = NULL;
  }
  xrm_enc_release_v2(&enc_ctx->xrm_enc_ctx);
  if (enc_ctx->output_xma_buffer) {
    xma_data_buffer_free(enc_ctx->output_xma_buffer);
    enc_ctx->output_xma_buffer = NULL;
  }

  xlnx_enc_free_xma_props(&enc_ctx->xma_enc_props);

  if (enc_ctx->dynamic_idr && enc_ctx->dynamic_idr->len_idr_arr) {
    free(enc_ctx->dynamic_idr->idr_arr);
  }
  return;
}
