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

#include "xlnx_decoder_app.h"

int signal_caught;

/**
 * dec_signal_handler: Signal handler function
 * @param signum: Signal number
 */
static void dec_signal_handler(int32_t signum) {
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
 * dec_set_signal_handler: Signal handler initialization.
 * @return XMA_APP_SUCCESS or APP_FAILURE
 */
static int dec_set_signal_handler() {
  signal_caught = 0;
  struct sigaction action;
  action.sa_handler = dec_signal_handler;
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
 * xlnx_dec_cleanup_ctx: Frees software/hw resources and closes files.
 * @param ctx: The decoder App context
 */
static void xlnx_dec_cleanup_ctx(XlnxDecoderAppCtx* ctx) {
  if (ctx->out_fp) {
    fclose(ctx->out_fp);
  }
  xlnx_dec_cleanup_dec_ctx(&ctx->dec_ctx);
}

/**
 * xlnx_dec_check_pix_fmt: Creates the context based on user arguments. It
 * parses the header of the input file to get relevant codec info. This does
 * not create the xma session.
 * @param fmt: pixel format
 * @param bpp: bits per pixel
 * @param log: xma log handle
 * @return XMA_APP_SUCCESS on success
 */
static int32_t xlnx_dec_check_pix_fmt(XmaLogHandle log, XmaFormatType fmt, int bpp) {
  switch (fmt) {
  case XMA_YUV420P_FMT_TYPE:
  case XMA_NV12_FMT_TYPE:
    if (bpp == BITS_PER_PIXEL_10) {
      xma_logmsg(log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "8 bit format for 10 bit input file unsupported\n");
      return XMA_APP_ERROR;
    }
    break;
  case XMA_YUV420P10LE_FMT_TYPE:
  case XMA_P010LE_FMT_TYPE:
  case XMA_PACKED10_FMT_TYPE:
    if (bpp == BITS_PER_PIXEL_8) {
      xma_logmsg(log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "10 bit format for 8 bit input file unsupported\n");
      return XMA_APP_ERROR;
    }
    break;
  case XMA_RGB24_P_FMT_TYPE:
    break;
  default:
    xma_logmsg(log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Unknown format %d given! %s:%s:%d", fmt, __FILE__, __func__, __LINE__);
    return XMA_APP_ERROR;
  }
  return XMA_APP_SUCCESS;
}

/**
 * xlnx_dec_create_context: Creates the context based on user arguments. It
 * parses the header of the input file to get relevant codec info. This does
 * not create the xma session.
 * @param arguments: The argument struct containing decoder param, input, output
 * file
 * @param ctx: A pointer to the scaler context
 * @return XMA_APP_SUCCESS on success
 */
static int32_t xlnx_dec_create_context(const XlnxDecoderArguments* arguments, XlnxDecoderAppCtx* ctx) {
  int ret = xma_log_init(arguments->log_level, arguments->log_location, &ctx->dec_ctx.log, arguments->log_file);
  if (ret != XMA_SUCCESS) {
    printf("XMA Initialization failed\n");
    return XMA_APP_ERROR;
  }
  ctx->out_fp = fopen(arguments->output_file, "wb");
  if (!ctx->out_fp) {
    xma_logmsg(ctx->dec_ctx.log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Unable to open output file %s\n", arguments->output_file);
    return XMA_APP_ERROR;
  }

  ctx->num_frames_to_decode = arguments->num_frames;
  ctx->dec_ctx.out_pix_fmt  = arguments->pix_fmt;

  if (xlnx_dec_create_decoder_context(arguments->input_file, arguments->loop_count, arguments->dec_props, &ctx->dec_ctx) != XMA_APP_SUCCESS) {

    return XMA_APP_ERROR;
  }
  if (xlnx_dec_check_pix_fmt(ctx->dec_ctx.log, arguments->pix_fmt, ctx->dec_ctx.dec_props.bit_depth) != XMA_APP_SUCCESS) {
    return XMA_APP_ERROR;
  }

  return XMA_APP_SUCCESS;
}

/**
 * xlnx_dec_print_segment_performance: Print the performance since the last
 * segment.
 * @param ctx: The decoder App context
 */
static void xlnx_dec_print_segment_performance(XlnxDecoderAppCtx* ctx) {
  double time_since_last_segment = xlnx_utils_get_segment_time(&ctx->timer);
  if (time_since_last_segment < 0.5) {
    return;
  }
  fprintf(stderr, "\rFrame=%5zu Total FPS=%.03f Current FPS=%.03f\r", ctx->num_frames_decoded, (float) ctx->num_frames_decoded / xlnx_utils_get_total_time(&ctx->timer),
      (ctx->num_frames_decoded - ctx->timer.last_displayed_frame) / time_since_last_segment);
  fflush(stderr);
  ctx->timer.last_displayed_frame = ctx->num_frames_decoded;
  xlnx_utils_set_segment_time(&ctx->timer);
}

/**
 * xlnx_dec_print_total_performance: Print the total performance of the decoder.
 * @param ctx: The decoder context
 */
static void xlnx_dec_print_total_performance(XlnxDecoderAppCtx* ctx) {
  double realtime_taken = xlnx_utils_get_total_time(&ctx->timer);
  fprintf(stderr, "\nFrames Decoded: %zu, Time Elapsed: %.03lf\r\n", ctx->num_frames_decoded, realtime_taken);
  fprintf(stderr, "Total FPS: %.03lf\r\n", ctx->num_frames_decoded / realtime_taken);
}

/**
 * Write the decoded output to the output file
 * @param ctx The decoder context
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
static int32_t dec_export_to_file(XlnxDecoderAppCtx* ctx) {
  FILE* out_fp = ctx->out_fp;
  xlnx_utils_write_xframe(ctx->dec_ctx.handle, ctx->dec_ctx.log, ctx->dec_ctx.output_xframe, out_fp);
  return XMA_APP_SUCCESS;
}

/**
 * dec_decode_frame: Decode a frame
 *
 * @param ctx: The decoder context
 * @return XMA_SUCCESS on success
 */
static int dec_decode_frame(XlnxDecoderAppCtx* ctx) {
  int ret;
  ret = xlnx_dec_process_frame(&ctx->dec_ctx);
  if (ret == XMA_SUCCESS) {
    if (ctx->num_frames_decoded < ctx->num_frames_to_decode) {
      ctx->num_frames_decoded++;
      xlnx_dec_print_segment_performance(ctx);
      /* we have recieved a decoded frame, write it to the output file */
      if (dec_export_to_file(ctx) != XMA_APP_SUCCESS) {
        return XMA_APP_ERROR;
      }
    } else {
      xma_frame_free(ctx->dec_ctx.output_xframe);
      ctx->dec_ctx.output_xframe = NULL;
    }
  } else {
    usleep(5);
  }
  return ret;
}

/**
 * dec_decode_frame: Decode a file
 *
 * @param ctx: The decoder context
 */
static void dec_decode_file(XlnxDecoderAppCtx* ctx) {
  int ret = XMA_SUCCESS;
  dec_set_signal_handler();
  xlnx_utils_set_non_blocking(1);
  xlnx_utils_start_tracking_time(&ctx->timer);
  while (ret != XMA_EOS && ret != XMA_ERROR) {
    if (xlnx_utils_was_q_pressed() || signal_caught || ctx->num_frames_decoded >= ctx->num_frames_to_decode) {

      XlnxStateInfo* input_info = &ctx->dec_ctx.input_ctx.input_file_info;
      input_info->loop          = 0;
      lseek(input_info->in_file, 0, SEEK_END);
      input_info->input_buffer.size = 0;
    }
    ret = dec_decode_frame(ctx);
  }
  fprintf(stderr, "\n");
  xlnx_utils_set_non_blocking(0);
  xma_logmsg(ctx->dec_ctx.log, XMA_INFO_LOG, XLNX_DEC_APP_MODULE, "\nDecode session done\n");
}

/**
 * xlnx_dec_create_device_session: Initializes the device, creates the decoder session.
 * @param ctx: The decoder app ctx containing the decoder context
 * @return XMA_APP_SUCCESS on success XMA_APP_ERROR on error
 */
static int32_t xlnx_dec_create_device_session(XlnxDecoderAppCtx* ctx) {
  XlnxDecoderCtx* dec_ctx = &ctx->dec_ctx;
  /* load device */
  if (xlnx_dec_device_init(dec_ctx) != XMA_APP_SUCCESS) {
    return XMA_APP_ERROR;
  }
  dec_ctx->download_session = xma_filter_session_create(&dec_ctx->xma_download_props);
  if (!dec_ctx->download_session) {
    printf("Failed to create upload session\n");
    return XMA_APP_ERROR;
  }

  dec_ctx->xma_dec_session = xma_dec_session_create(&dec_ctx->dec_xma_props);
  if (!dec_ctx->xma_dec_session) {
    xma_logmsg(dec_ctx->log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Failed to create decoder session\n");
    return XMA_APP_ERROR;
  }
  return XMA_APP_SUCCESS;
}

/**
 * main: Main function for xma decoder app
 * @param argc: Number of arguments
 * @param argv: Pointer to the arguments
 * @return XMA_APP_SUCCESS on success XMA_APP_ERROR on error
 */
int main(int argc, char* argv[]) {
  XlnxDecoderArguments arguments;
  if (xlnx_dec_get_arguments(argc, argv, &arguments) != XMA_APP_SUCCESS) {
    exit(XMA_APP_ERROR);
  }
  XlnxDecoderAppCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  if (xlnx_dec_create_context(&arguments, &ctx) != XMA_APP_SUCCESS) {
    xlnx_dec_cleanup_ctx(&ctx);
    exit(XMA_APP_ERROR);
  }
  if (xlnx_dec_create_device_session(&ctx) != XMA_APP_SUCCESS) {
    xlnx_dec_cleanup_ctx(&ctx);
    exit(XMA_APP_ERROR);
  }

  /* Run Decoder */
  dec_decode_file(&ctx);
  xlnx_dec_print_total_performance(&ctx);
  xlnx_dec_cleanup_ctx(&ctx);
  return XMA_APP_SUCCESS;
}
