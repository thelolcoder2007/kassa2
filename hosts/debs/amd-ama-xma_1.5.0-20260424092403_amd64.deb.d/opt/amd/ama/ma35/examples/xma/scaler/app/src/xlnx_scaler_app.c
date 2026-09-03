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

#include "xlnx_scaler_app.h"

int signal_caught;

/**
 * xlnx_scal_signal_handler: Signal handler function
 * @param _signum: Signal number
 */
static void xlnx_scal_signal_handler(int32_t signum) {
  switch (signum) {
  case SIGHUP:
  case SIGINT:
  case SIGQUIT:
  case SIGABRT:
  case SIGTERM:
    signal_caught = 1;
    break;
  }
}

/**
 * xlnx_scal_set_signal_handler: Signal handler initialization.
 * @return XMA_APP_SUCCESS or APP_FAILURE
 */
static int xlnx_scal_set_signal_handler() {
  signal_caught = 0;
  struct sigaction action;
  action.sa_handler = xlnx_scal_signal_handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  sigaction(SIGHUP, &action, NULL);
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGQUIT, &action, NULL);
  sigaction(SIGABRT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  return XMA_APP_SUCCESS;
}

/**
 * xlnx_scal_cleanup_ctx: Cleanup the context - free resources, close files.
 *
 * @param ctx: The scaler context
 */
void xlnx_scal_cleanup_ctx(XlnxScalerAppCtx* ctx) {
  if (ctx->in_fp) {
    fclose(ctx->in_fp);
  }
  int num_outputs = ctx->scaler_ctx.abr_params.num_outputs[0];
  for (int i = 0; i < num_outputs; i++) {
    if (ctx->out_fp[i]) {
      fclose(ctx->out_fp[i]);
    }
  }
  xlnx_scal_cleanup_scaler_ctx(&ctx->scaler_ctx);
}

/**
 * xlnx_scal_create_output_ctx: Open output files and set pix fmt
 * @param arguments: The arguments struct containing commandline info
 * @param ctx: The scaler app ctx
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
static int32_t xlnx_scal_set_output_params(XlnxScalArguments arguments, XlnxScalerAppCtx* ctx) {
  int             num_outputs = ctx->scaler_ctx.abr_params.num_outputs[0];
  XlnxScalOutArgs out_args;
  for (int output_id = 0; output_id < num_outputs; output_id++) {
    out_args               = arguments.out_arg_list[output_id];
    ctx->out_fp[output_id] = fopen(out_args.output_file, "wb");
    if (!ctx->out_fp[output_id]) {
      printf("Unable to open output file %s\n", out_args.output_file);
      return XMA_APP_ERROR;
    }
    ctx->out_pix_fmt[output_id] = out_args.pix_fmt;
  }
  return XMA_APP_SUCCESS;
}

/**
 * xlnx_scal_create_xma_param_ctx: Create the param ctx for xma props.
 *
 * @param arguments: The arguments struct containing commandline info
 * @param ctx: The scaler context
 * @return XMA_APP_SUCCESS on success XMA_APP_ERROR on error
 */
static int32_t xlnx_scal_create_xma_param_ctx(XlnxScalArguments arguments, XlnxScalerProps* param_ctx) {
  // Setup custom xma plugin params
  param_ctx->device_id       = arguments.device_id;
  param_ctx->log_level       = arguments.log_level;
  param_ctx->latency_logging = arguments.latency_logging;
  param_ctx->input_pix_fmt   = arguments.input_pix_fmt;
  param_ctx->input_width     = arguments.input_width;
  param_ctx->input_height    = arguments.input_height;
  if (param_ctx->input_pix_fmt == XMA_YUV420P10LE_FMT_TYPE) {
    param_ctx->input_stride         = 2 * arguments.input_width;
    param_ctx->input_bits_per_pixel = 10;
  } else {
    param_ctx->input_stride         = arguments.input_width;
    param_ctx->input_bits_per_pixel = 8;
  }
  param_ctx->input_fps_num = arguments.fps_num;
  param_ctx->input_fps_den = arguments.fps_den;
  // assign output params

  param_ctx->num_outputs[0]   = arguments.outputs_used;
  param_ctx->num_outputs[1]   = arguments.num_fullrate_outputs;
  param_ctx->is_halfrate_used = arguments.num_halfrate_outputs > 0;
  XlnxScalOutArgs out_args;
  for (uint output_id = 0; output_id < param_ctx->num_outputs[0]; output_id++) {
    out_args                                     = arguments.out_arg_list[output_id];
    param_ctx->output_bits_per_pixels[output_id] = param_ctx->input_bits_per_pixel;
    param_ctx->output_pix_fmts[output_id]        = out_args.pix_fmt;
    param_ctx->output_widths[output_id]          = out_args.width;
    param_ctx->output_heights[output_id]         = out_args.height;
    param_ctx->output_strides[output_id]         = out_args.width;
    param_ctx->output_fps_nums[output_id]        = param_ctx->input_fps_num;
    param_ctx->output_fps_dens[output_id]        = param_ctx->input_fps_den;
    if (param_ctx->output_pix_fmts[output_id] == XMA_YUV420P10LE_FMT_TYPE) {
      param_ctx->output_strides[output_id] *= 2;
    }
    param_ctx->is_halfrate[output_id] = out_args.is_halfrate;
    param_ctx->coeff_loads[output_id] = out_args.coeff_load;
  }
  return XMA_APP_SUCCESS;
}

/**
 * xlnx_scal_create_context: Creates the context based on user arguments. It
 * parses the header of the input file to get relevant codec info. This does
 * not create the xma session. Nor does it initialize the device.
 * @param arguments: The argument struct containing scaler param, input, output
 * file
 * @param info
 * @param ctx: A pointer to the scaler context
 * @return XMA_APP_SUCCESS on success
 */
static int32_t xlnx_scal_create_context(XlnxScalArguments arguments, XlnxScalerAppCtx* ctx) {
  ctx->loops_remaining     = arguments.loop_count;
  ctx->num_frames_to_scale = arguments.num_frames;
  ctx->in_fp               = fopen(arguments.input_file, "rb");
  if (!ctx->in_fp) {
    printf("Unable to open input file %s\n", arguments.input_file);
    return XMA_APP_ERROR;
  }
  ctx->in_pix_fmt = arguments.input_pix_fmt;
  xlnx_scal_create_xma_param_ctx(arguments, &ctx->scaler_ctx.abr_params);
  /* Open output files, set output pix fmts */
  if (xlnx_scal_set_output_params(arguments, ctx) != XMA_APP_SUCCESS) {
    return XMA_APP_ERROR;
  }
  if (xlnx_scal_create_scaler_ctx(&ctx->scaler_ctx) != XMA_APP_SUCCESS) {
    return XMA_APP_ERROR;
  }
  return XMA_APP_SUCCESS;
}

/**
 * xlnx_scal_get_input_frame: Read an input frame into the xframe buffer
 * @param ctx: A pointer to the scaler context
 * @param in_fp: ctx->input_ctx.in_fp
 * @param xframe: ctx->input_ctx.xframe
 * @param props: ctx->arb_xma_props
 * @return XMA_APP_SUCCESS on success XMA_APP_ERROR on error
 */
static int32_t xlnx_scal_get_input_frame(XlnxScalerAppCtx* ctx) {
  int ret                      = XMA_APP_SUCCESS;
  ctx->scaler_ctx.input_xframe = xma_frame_alloc(ctx->scaler_ctx.handle, &ctx->scaler_ctx.upload_fprops, false);
  if (!ctx->scaler_ctx.input_xframe) {
    xma_logmsg(ctx->scaler_ctx.log, XMA_ERROR_LOG, XLNX_SCALER_APP_MODULE, "Failed to allocate xma frame for DMA to device!\n");
    return XMA_APP_ERROR;
  }
  XmaFrame* input_xframe = ctx->scaler_ctx.input_xframe;
  ret                    = xlnx_utils_read_frame(ctx->scaler_ctx.handle, ctx->scaler_ctx.log, input_xframe, ctx->in_fp);
  if (ret == XMA_APP_EOF) {
    int loops_remaining = ctx->loops_remaining;
    if (loops_remaining > 0 || loops_remaining == -1) {
      fseek(ctx->in_fp, 0, SEEK_SET);
      if (loops_remaining != -1) {
        ctx->loops_remaining--;
      }
      ret = xlnx_utils_read_frame(ctx->scaler_ctx.handle, ctx->scaler_ctx.log, input_xframe, ctx->in_fp);
    }
  }
  if (ret == XMA_APP_EOF) {
    xma_frame_free(input_xframe);
    ctx->scaler_ctx.input_xframe = NULL;
  }
  return ret;
}

/**
 * xlnx_scal_print_segment_performance: Print the performance since the last
 * segment.
 * @param ctx: The scaler context
 */
static void xlnx_scal_print_segment_performance(XlnxScalerAppCtx* ctx) {
  double time_since_last_segment = xlnx_utils_get_segment_time(&ctx->timer);
  if (time_since_last_segment < 0.5) {
    return;
  }
  fprintf(stderr, "\rFrame=%5zu Total FPS=%.03f Current FPS=%.03f\r", ctx->num_frames_scaled, (float) ctx->num_frames_scaled / xlnx_utils_get_total_time(&ctx->timer),
      (ctx->num_frames_scaled - ctx->timer.last_displayed_frame) / time_since_last_segment);
  fflush(stderr);
  ctx->timer.last_displayed_frame = ctx->num_frames_scaled;
  xlnx_utils_set_segment_time(&ctx->timer);
}

/**
 * xlnx_scal_print_total_performance: Print the total performance since tracking
 * began.
 * @param ctx: The scaler context
 */
static void xlnx_scal_print_total_performance(XlnxScalerAppCtx* ctx) {
  double realtime_taken = xlnx_utils_get_total_time(&ctx->timer);
  fprintf(stderr, "\nFrames Scaled: %zu, Time Elapsed: %.03lf\r\n", ctx->num_frames_scaled, realtime_taken);
  fprintf(stderr, "Real Time FPS: %.03lf\r\n", ctx->num_frames_scaled / realtime_taken);
}

/**
 * Write the output frame for an output
 * @param ctx The scaler app context
 * @param output_id The output index for the xframelist
 * @return XMA_APP_SUCCESS on success, XMA_APP_ERROR on error
 */
static int xlnx_scal_write_output_frame(XlnxScalerAppCtx* ctx, int output_id, int xframe_id) {
  int            ret;
  XlnxScalerCtx* scaler_ctx    = &ctx->scaler_ctx;
  XmaFrame*      output_xframe = scaler_ctx->output_xframe_list[xframe_id];
  ret                          = xma_filter_session_send_frame(scaler_ctx->download_sessions[output_id], output_xframe);
  if (ret != XMA_SUCCESS) {
    xma_logmsg(scaler_ctx->log, XMA_ERROR_LOG, XLNX_SCALER_APP_MODULE, "Upload filter failed to receive frame from filter!\n");
    return XMA_APP_ERROR;
  }
  output_xframe = xma_frame_alloc(scaler_ctx->handle, &scaler_ctx->download_fprops[output_id], true);
  if (!output_xframe) {
    xma_logmsg(scaler_ctx->log, XMA_ERROR_LOG, XLNX_SCALER_APP_MODULE, "Failed to allocate xma frame for DMA to host!\n");
    return XMA_APP_ERROR;
  }
  uint32_t counter = 1000000;
  do {
    ret = xma_filter_session_recv_frame(scaler_ctx->download_sessions[output_id], output_xframe);
    if (ret == XMA_TRY_AGAIN) {
      usleep(100);
    }
  } while ((ret == XMA_TRY_AGAIN) && (--counter > 0));
  if (ret == XMA_SUCCESS) {
    xlnx_utils_write_xframe(scaler_ctx->handle, scaler_ctx->log, output_xframe, ctx->out_fp[output_id]);
  } else {
    xma_frame_free(output_xframe);
    scaler_ctx->output_xframe_list[xframe_id] = NULL;
  }
  return XMA_APP_SUCCESS;
}

/**
 * Write all of the output frames for a session
 * @param ctx The scaler context
 * @param session_id 0 is all rate, 1 is full rate only.
 * @return XMA_APP_SUCCESS on success
 */
static int xlnx_scal_write_output_frames(XlnxScalerAppCtx* ctx, int session_id) {
  int xframe_id   = 0;
  int num_outputs = ctx->scaler_ctx.abr_params.num_outputs[0];
  for (int output_id = 0; output_id < num_outputs; output_id++) {
    /* If this is full rate only session and the current output is half
        rate, then skip it. */
    if (session_id != 0 && ctx->scaler_ctx.abr_params.is_halfrate[output_id]) {

      continue;
    }
    xlnx_scal_write_output_frame(ctx, output_id, xframe_id);
    xframe_id++;
  }
  return XMA_APP_SUCCESS;
}

/**
 * Scale a frame
 * @param ctx The scaler app context
 * @param session_id The id of the session; used to track mixrate
 * @return The result of sending the frame, or xma error if recv is not
 * successful.
 */
static int xlnx_scal_scale_frame(XlnxScalerAppCtx* ctx, int session_id) {
  int ret;
  int send_rc, recv_rc;
  /* send frame to scaler */
  XlnxScalerCtx* scaler_ctx   = &ctx->scaler_ctx;
  XmaFrame*      input_xframe = scaler_ctx->input_xframe;
  ret                         = xma_filter_session_send_frame(scaler_ctx->upload_session, input_xframe);
  if (ret != XMA_SUCCESS) {
    xma_logmsg(scaler_ctx->log, XMA_ERROR_LOG, XLNX_SCALER_APP_MODULE, "Upload filter failed to send frame %d\n", scaler_ctx->num_frames_scaled);
    return ret;
  }
  input_xframe = xma_frame_alloc(scaler_ctx->handle, &scaler_ctx->input_fprops, true);
  if (!input_xframe) {
    xma_logmsg(scaler_ctx->log, XMA_ERROR_LOG, XLNX_SCALER_APP_MODULE, "Failed to allocate xma frame for DMA to device!\n");
    return ret;
  }
  uint32_t counter = 1000000;
  do {
    ret = xma_filter_session_recv_frame(scaler_ctx->upload_session, input_xframe);
    if (ret == XMA_TRY_AGAIN) {
      usleep(100);
    }
  } while ((ret == XMA_TRY_AGAIN) && (--counter > 0));
  if (ret == XMA_EOS) {
    xma_frame_free(input_xframe);
    input_xframe = NULL;
  } else if (ret != XMA_SUCCESS) {
    xma_logmsg(scaler_ctx->log, XMA_ERROR_LOG, XLNX_SCALER_APP_MODULE, "Upload filter failed to receive frame %d\n", scaler_ctx->num_frames_scaled);
    return ret;
  }
  send_rc = xma_scaler_session_send_frame(scaler_ctx->session[session_id], input_xframe);
  /* receive frame from scaler */
  if ((send_rc == XMA_SUCCESS) || (send_rc == XMA_FLUSH_AGAIN)) {
    for (uint i = 0; i < scaler_ctx->abr_params.num_outputs[0]; i++) {
      scaler_ctx->output_xframe_list[i] = xma_frame_alloc(ctx->scaler_ctx.handle, &scaler_ctx->output_fprops[i], true);
      if (!scaler_ctx->output_xframe_list[i]) {
        xma_logmsg(scaler_ctx->log, XMA_ERROR_LOG, XLNX_SCALER_APP_MODULE, "Failed to allocate xma frame for scaler receive!\n");
        return XMA_APP_ERROR;
      }
    }
    counter = 1000000;
    do {
      recv_rc = xma_scaler_session_recv_frame_list(scaler_ctx->session[session_id], scaler_ctx->output_xframe_list);
      if (recv_rc == XMA_TRY_AGAIN) {
        usleep(100);
      }
    } while ((recv_rc == XMA_TRY_AGAIN) && (--counter > 0));
    if (recv_rc == XMA_EOS) {
      for (uint i = 0; i < scaler_ctx->abr_params.num_outputs[0]; i++) {
        xma_frame_free(scaler_ctx->output_xframe_list[i]);
        scaler_ctx->output_xframe_list[i] = NULL;
      }
      send_rc    = XMA_EOS;
      session_id = 0;
    } else if (recv_rc != XMA_SUCCESS) {
      xma_logmsg(scaler_ctx->log, XMA_ERROR_LOG, XLNX_SCALER_APP_MODULE, "Failed to receive frame list from XMA plugin\n");
      return XMA_ERROR;
    }
    xlnx_scal_write_output_frames(ctx, session_id);
    if (send_rc != XMA_EOS) {
      ctx->num_frames_scaled++;
    }
    xlnx_scal_print_segment_performance(ctx);
  }
  return send_rc;
}

/**
 * xlnx_scal_scale_file: Scale a file
 *
 * @param ctx: The scaler context
 * @return XMA_APP_SUCCESS on success XMA_APP_ERROR on error
 */
static int xlnx_scal_scale_file(XlnxScalerAppCtx* ctx) {
  int send_rc = XMA_SUCCESS;
  xlnx_scal_set_signal_handler();
  xlnx_utils_set_non_blocking(1);
  xlnx_utils_start_tracking_time(&ctx->timer);
  while (send_rc != XMA_EOS && send_rc != XMA_ERROR) {
    if (xlnx_utils_was_q_pressed() || signal_caught || ctx->num_frames_scaled >= ctx->num_frames_to_scale) {

      ctx->loops_remaining = 0;
      fseek(ctx->in_fp, 0, SEEK_END);
    }
    xlnx_scal_get_input_frame(ctx);
    if (ctx->scaler_ctx.num_sessions > 1 && ctx->num_frames_scaled % 2 != 0) {

      send_rc = xlnx_scal_scale_frame(ctx, 1);
    } else {
      send_rc = xlnx_scal_scale_frame(ctx, 0);
    }
  }
  fprintf(stderr, "\n");
  xlnx_utils_set_non_blocking(0);
  return send_rc != XMA_APP_ERROR ? XMA_APP_SUCCESS : XMA_APP_ERROR;
}

/**
 * xlnx_scal_create_device_sessions: Create scaler sessions
 * @param ctx: The scaler context
 * @return XMA_APP_SUCCESS or APP_FAILURE
 */
static int32_t xlnx_scal_create_device_sessions(XlnxScalArguments* arguments, XlnxScalerAppCtx* ctx) {
  XlnxScalerCtx* scaler_ctx = &ctx->scaler_ctx;
  int            ret        = xma_log_init(arguments->log_level, arguments->log_location, &ctx->scaler_ctx.log, arguments->log_file);
  if (ret != XMA_SUCCESS) {
    fprintf(stderr, "XMA logging initialization failed\n");
    return XMA_APP_ERROR;
  }
  // load device
  if (xlnx_scal_device_init(scaler_ctx) != XMA_APP_SUCCESS) {
    return XMA_APP_ERROR;
  }
  if (xlnx_scal_create_scaler_sessions(scaler_ctx) != XMA_APP_SUCCESS) {
    return XMA_APP_ERROR;
  }
  return XMA_APP_SUCCESS;
}

/**
 * main: Main routine
 * @param argc: Arguments count
 * @param argv: Argument array
 * @return XMA_APP_SUCCESS or APP_FAILURE
 */
int main(int argc, char* argv[]) {
  XlnxScalArguments arguments = xlnx_scal_get_arguments(argc, argv);
  XlnxScalerAppCtx  ctx;
  memset(&ctx, 0, sizeof(XlnxScalerAppCtx));
  if (xlnx_scal_create_context(arguments, &ctx) != XMA_APP_SUCCESS) {
    xlnx_scal_cleanup_ctx(&ctx);
    exit(XMA_APP_ERROR);
  }
  if (xlnx_scal_create_device_sessions(&arguments, &ctx) != XMA_APP_SUCCESS) {
    xlnx_scal_cleanup_ctx(&ctx);
    exit(XMA_APP_ERROR);
  }
  // run abr scaler
  xlnx_scal_scale_file(&ctx);
  xlnx_scal_print_total_performance(&ctx);
  xlnx_scal_cleanup_ctx(&ctx);
  return XMA_APP_SUCCESS;
}
