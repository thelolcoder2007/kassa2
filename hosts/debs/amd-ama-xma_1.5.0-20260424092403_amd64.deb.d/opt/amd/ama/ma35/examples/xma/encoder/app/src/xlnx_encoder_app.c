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

#include "xlnx_encoder_app.h"

bool signal_caught;

/**
 * xlnx_enc_signal_handler: Signal handler function
 *
 * @param signum: Signal number
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
void xlnx_enc_signal_handler(int32_t signum) {
  switch (signum) {
  case SIGTERM:
  case SIGINT:
  case SIGABRT:
  case SIGHUP:
  case SIGQUIT:
    signal_caught = true;
    break;
  }
}

/**
 * xlnx_enc_set_signal_handler: Signal handler initialization.
 *
 * @param None
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
static int32_t xlnx_enc_set_signal_handler() {

  struct sigaction action;
  action.sa_handler = xlnx_enc_signal_handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGHUP, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGQUIT, &action, NULL);
  return XMA_APP_SUCCESS;
}

/**
 * xlnx_enc_app_close: Deinitialize encoder application
 *
 * @param enc_ctx: Encoder context
 * @param xma_enc_props: XMA encoder properties
 * @param xma_la_props: XMA lookahead properties
 * @return None
 */
static void xlnx_enc_app_close(XlnxEncoderAppCtx* ctx) {
  xlnx_enc_deinit(&ctx->enc_ctx);
  if (ctx->in_file) {
    fclose(ctx->in_file);
  }
  if (ctx->out_file) {
    fclose(ctx->out_file);
  }
  if (ctx->enc_ctx.stat_data) {
    fclose(ctx->enc_ctx.stat_data);
  }
  if (ctx->enc_ctx.dyn_params_fp) {
    fclose(ctx->enc_ctx.dyn_params_fp);
    if (ctx->enc_ctx.dyn_params) {
      xma_side_data_free(ctx->enc_ctx.dyn_params);
      ctx->enc_ctx.dyn_params = NULL;
    }
  }
  xma_release(ctx->enc_ctx.handle);
  ctx->enc_ctx.handle = NULL;
  if (ctx->enc_ctx.log != ctx->enc_ctx.filter_log) {
    xma_log_release(ctx->enc_ctx.filter_log);
    ctx->enc_ctx.filter_log = NULL;
  }
  xma_log_release(ctx->enc_ctx.log);
  ctx->enc_ctx.log = NULL;

  return;
}

/**
 * xlnx_enc_create_app_ctx: Create the encoder app context - create encoder
 * context with encoder and lookahead settings, open input/output files, set app
 * level encoding options.
 *
 * @param arguments: The arguments struct containing enc settings, lookahead
 * settings,
 * @param and app level encoding information
 * @param ctx: The encoder app context to be created.
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
static int32_t xlnx_enc_create_app_ctx(XlnxEncoderArguments* arguments, XlnxEncoderAppCtx* ctx) {
  int ret = xma_log_init(arguments->log_level, arguments->log_location, &ctx->enc_ctx.log, arguments->log_file);
  if (ret != XMA_SUCCESS) {
    fprintf(stderr, "XMA Initialization failed\n");
    return XMA_APP_ERROR;
  }

  ctx->in_file = fopen(arguments->input_file, "rb");
  if (!ctx->in_file) {
    xma_logmsg(ctx->enc_ctx.log, XMA_ERROR_LOG, XLNX_ENC_APP_MODULE, "Unable to open input file %s for reading", arguments->input_file);
    return XMA_APP_ERROR;
  }
  ctx->pix_fmt  = arguments->pix_fmt;
  ctx->out_file = fopen(arguments->output_file, "wb");
  if (!ctx->out_file) {
    xma_logmsg(ctx->enc_ctx.log, XMA_ERROR_LOG, XLNX_ENC_APP_MODULE, "Unable to open output file %s for writing", arguments->output_file);
    return XMA_APP_ERROR;
  }
  if (strlen(arguments->stat_file)) {
    ctx->enc_ctx.stat_data = fopen(arguments->stat_file, "wb");
    fprintf(ctx->enc_ctx.stat_data, "Y PSNR, U PSNR, V PSNR, Y SSIM, U SSIM, V SSIM\n");
  }
  if (strlen(arguments->dyn_param_file)) {
    ctx->enc_ctx.dyn_params_fp = fopen(arguments->dyn_param_file, "r");
  } else {
    ctx->enc_ctx.dyn_params_fp = NULL;
  }
  ctx->enc_ctx.dyn_params   = NULL;
  ctx->loop_count           = arguments->loop_count;
  ctx->num_frames_to_encode = arguments->num_frames;
  ctx->enc_ctx.dynamic_idr  = &arguments->dynamic_idr;

  // Note ctx->enc_ctx.handle is initialized later in
  // xlnx_enc_start_device_session / xma initialize
  if (xlnx_enc_create_enc_ctx(ctx->enc_ctx.handle, &arguments->enc_props, &ctx->enc_ctx) != XMA_APP_SUCCESS) {

    return XMA_APP_ERROR;
  }

  ctx->frame_props.format    = ctx->pix_fmt;
  ctx->frame_props.sw_format = ctx->pix_fmt;
  ctx->frame_props.width     = arguments->enc_props.width;
  ctx->frame_props.height    = arguments->enc_props.height;
  int32_t planes             = xma_frame_planes_get(ctx->enc_ctx.handle, &ctx->frame_props);
  int32_t plane;
  for (plane = 0; plane < planes; plane++) {
    ctx->frame_props.linesize[plane]             = xma_frame_get_plane_stride(ctx->enc_ctx.handle, &ctx->frame_props, plane);
    ctx->enc_ctx.enc_frame_props.linesize[plane] = ctx->frame_props.linesize[plane];
  }
  ctx->enc_ctx.enc_frame_props.format    = XMA_VPE_FMT_TYPE;
  ctx->enc_ctx.enc_frame_props.sw_format = ctx->frame_props.sw_format;
  ctx->enc_ctx.enc_frame_props.width     = ctx->frame_props.width;
  ctx->enc_ctx.enc_frame_props.height    = ctx->frame_props.height;

  return XMA_APP_SUCCESS;
}

/**
 * xlnx_enc_print_total_fps: Print the total performance of the encoder.
 *
 * @param ctx: Encoder app context
 * @return None
 */
void xlnx_enc_print_total_fps(XlnxEncoderAppCtx* ctx) {
  size_t num_frames_encoded = min(ctx->enc_ctx.num_frames_sent, ctx->num_frames_to_encode);
  double time_taken         = xlnx_utils_get_total_time(&ctx->timer);
  fprintf(stderr, "\nFrames Encoded: %zu, Time Elapsed: %.03lf\r\n", num_frames_encoded, time_taken);
  fprintf(stderr, "Total FPS: %.03lf\r\n", num_frames_encoded / time_taken);
}

/**
 * xlnx_enc_print_segment_fps: Calculate and print fps for every second
 *
 * @param ctx: Encoder app context
 * @return None
 */
static void xlnx_enc_print_segment_fps(XlnxEncoderAppCtx* ctx) {

  double segment_time = xlnx_utils_get_segment_time(&ctx->timer);
  if (segment_time < 1) {
    return;
  }
  size_t num_frames_encoded = ctx->enc_ctx.num_frames_received;
  fprintf(stderr, "\rFrame=%5zu Current FPS=%.03f Total FPS=%.03f \r", num_frames_encoded, (num_frames_encoded - ctx->timer.last_displayed_frame) / segment_time,
      (float) num_frames_encoded / xlnx_utils_get_total_time(&ctx->timer));
  fflush(stderr);
  ctx->timer.last_displayed_frame = ctx->enc_ctx.num_frames_received;
  xlnx_utils_set_segment_time(&ctx->timer);
}

static int32_t xlnx_enc_read_frame(XlnxEncoderAppCtx* enc_app_ctx) {
  const XlnxEncoderProperties* enc_props = enc_app_ctx->enc_ctx.enc_props;
  enc_app_ctx->enc_input_xframe          = xma_frame_alloc(enc_app_ctx->enc_ctx.handle, &enc_app_ctx->frame_props, false);
  XmaFrame* input_xframe                 = enc_app_ctx->enc_input_xframe;

  int ret = xlnx_utils_read_frame(enc_app_ctx->enc_ctx.handle, enc_app_ctx->enc_ctx.log, input_xframe, enc_app_ctx->in_file);
  if (ret == XMA_APP_EOF) {
    if (enc_app_ctx->loop_count > 0 || enc_app_ctx->loop_count == -1) {
      fseek(enc_app_ctx->in_file, 0, SEEK_SET);
      if (enc_app_ctx->loop_count != -1) {
        enc_app_ctx->loop_count--;
      }
      ret = xlnx_utils_read_frame(enc_app_ctx->enc_ctx.handle, enc_app_ctx->enc_ctx.log, input_xframe, enc_app_ctx->in_file);
    }
  }
  if (ret == XMA_APP_EOF) {
    xma_frame_free(input_xframe);
    enc_app_ctx->enc_input_xframe = NULL;
    if (enc_app_ctx->enc_ctx.num_frames_received == 0) {
      long int num_frames_in_src = ftell(enc_app_ctx->in_file) / ((enc_props->width * enc_props->height * 3) >> 1);
      if (num_frames_in_src == 0) {
        xma_logmsg(enc_app_ctx->enc_ctx.log, XMA_ERROR_LOG, XLNX_ENC_APP_MODULE, "No frames in input file!\n");
        return XMA_APP_ERROR;
      } else if (num_frames_in_src < enc_props->lookahead_depth) {
        xma_logmsg(enc_app_ctx->enc_ctx.log, XMA_ERROR_LOG, XLNX_ENC_APP_MODULE,
            "Source file must have more frames than lookahead "
            "depth! Num frames in src: %ld; lookahead depth: %d\n",
            num_frames_in_src, enc_props->lookahead_depth);
        return XMA_APP_ERROR;
      }
    }
  }
  return ret;
}

/**
 * xlnx_enc_set_if_idr_frame: Set frames as IDR frames at run time
 *
 * @param enc_ctx: Encoder context
 * @param input_xframe: Lookahead input frame
 * @param enc_frame_cnt: Encoder frame number
 */
static void xlnx_enc_set_if_idr_frame(XlnxEncoderCtx* enc_ctx, XmaFrame* input_xframe, uint32_t enc_frame_cnt) {
  uint32_t* idr_arr     = enc_ctx->dynamic_idr->idr_arr;
  size_t    len_idr_arr = enc_ctx->dynamic_idr->len_idr_arr;
  uint32_t  i           = enc_ctx->dynamic_idr->idr_arr_idx;

  input_xframe->is_idr = 0;
  for (; i < len_idr_arr; i++) {
    if (idr_arr[i] == enc_frame_cnt) {
      input_xframe->is_idr = 1;
      break;
    } else if (idr_arr[i] > enc_frame_cnt) {
      break;
    }
  }

  enc_ctx->dynamic_idr->idr_arr_idx = i;
}

/**
 * xlnx_get_runtime_dyn_params: Gets encoder runtime dynamic parameters
 *
 * @param enc_app_ctx: Encoder app context
 * @param la_in_frame: Lookahead input frame
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
static int32_t xlnx_get_runtime_dyn_params(XlnxEncoderAppCtx* enc_app_ctx, XmaFrame* la_input_xframe) {
  XlnxEncoderCtx* enc_ctx       = &enc_app_ctx->enc_ctx;
  uint32_t        enc_frame_cnt = enc_ctx->num_frames_sent;

  /* Check for Dynamic IDR option */
  if (enc_ctx->dynamic_idr->len_idr_arr) {
    xlnx_enc_set_if_idr_frame(enc_ctx, la_input_xframe, enc_frame_cnt);
  }

  return XMA_APP_SUCCESS;
}

/**
 * xlnx_enc_frame_process: Process an input YUV frame
 *
 * @param enc_ctx: Encoder context
 * @param enc_stop: Encoder stop flag
 * @return XMA_SUCCESS, XMA_ERROR, or XMA_EOS
 */
static int32_t xlnx_enc_frame_process(XlnxEncoderAppCtx* enc_app_ctx) {
  XlnxEncoderCtx* enc_ctx = &enc_app_ctx->enc_ctx;

  if (enc_app_ctx->enc_ctx.num_frames_sent < enc_app_ctx->num_frames_to_encode) {
    /* Read input from file into la input xframe */
    int32_t ret = xlnx_enc_read_frame(enc_app_ctx);
    if (ret <= XMA_APP_ERROR) {
      return ret;
    }
  } else {
    fseek(enc_app_ctx->in_file, 0, SEEK_END);
    enc_app_ctx->loop_count       = 0;
    enc_app_ctx->enc_input_xframe = NULL;
  }

  XmaFrame* enc_input_xframe = enc_app_ctx->enc_input_xframe;

  int32_t ret;
  if (enc_input_xframe && ((ret = xlnx_get_runtime_dyn_params(enc_app_ctx, enc_input_xframe)) != XMA_APP_SUCCESS)) {
    xma_logmsg(enc_app_ctx->enc_ctx.log, XMA_ERROR_LOG, XLNX_ENC_APP_MODULE, "xlnx_get_runtime_dyn_params failed with error %d\n", ret);
    return ret;
  }

  int32_t recv_size = 0;
  ret               = xlnx_enc_process_frame(enc_ctx, enc_input_xframe, &recv_size);
  /* num_frames_received incremented in xlnx_enc_process_frame. Therefore if
    check should be '<=' not '<' */
  if (ret == XMA_SUCCESS) {
    /* Encoder output frame received */
    fwrite(enc_ctx->output_xma_buffer->data.buffer, 1, recv_size, enc_app_ctx->out_file);
    xma_data_buffer_free(enc_ctx->output_xma_buffer);
    enc_ctx->output_xma_buffer = NULL;
  }

  return ret;
}

static int32_t xlnx_enc_encode_file(XlnxEncoderAppCtx* ctx) {
  /* Setting signal handler for SIGINT, SIGTERM and SIGHUP */
  xlnx_enc_set_signal_handler();
  xlnx_utils_set_non_blocking(1);
  xlnx_utils_start_tracking_time(&ctx->timer);
  /* Encoder loop */
  int ret = XMA_APP_SUCCESS;
  do {
    if (xlnx_utils_was_q_pressed() || signal_caught || ctx->enc_ctx.num_frames_sent >= ctx->num_frames_to_encode) {

      fseek(ctx->in_file, 0, SEEK_END);
      ctx->loop_count = 0;
    }
    ret = xlnx_enc_frame_process(ctx);
    xlnx_enc_print_segment_fps(ctx);
  } while (ret != XMA_ERROR && ret != XMA_EOS);
  xma_logmsg(ctx->enc_ctx.log, XMA_NOTICE_LOG, XLNX_ENC_APP_MODULE, "Encoding of input stream completed\n");
  xlnx_utils_set_non_blocking(0);
  return ret == XMA_ERROR ? XMA_ERROR : XMA_SUCCESS;
}

/**
 * main: Main function for xma encoder app
 *
 * @param argc: Number of arguments
 * @param argv: Pointer to the arguments
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
int32_t main(int32_t argc, char* argv[]) {
  XlnxEncoderArguments arguments;
  if (xlnx_enc_get_arguments(argc, argv, &arguments) != XMA_APP_SUCCESS) {
    return XMA_APP_ERROR;
  }

  XlnxEncoderAppCtx ctx;
  memset(&ctx, 0, sizeof(ctx));

  if (xlnx_enc_create_app_ctx(&arguments, &ctx) != XMA_APP_SUCCESS) {
    xlnx_enc_app_close(&ctx);
    return XMA_APP_ERROR;
  }

  if (xlnx_enc_start_device_session(&ctx.enc_ctx) != XMA_APP_SUCCESS) {
    xlnx_enc_app_close(&ctx);
    return XMA_APP_ERROR;
  }

  xlnx_enc_encode_file(&ctx);
  xlnx_enc_print_total_fps(&ctx);
  xlnx_enc_app_close(&ctx);
  return 0;
}
