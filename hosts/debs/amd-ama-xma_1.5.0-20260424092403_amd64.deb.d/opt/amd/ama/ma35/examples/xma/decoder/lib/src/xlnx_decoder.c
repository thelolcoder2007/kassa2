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

#include "xlnx_decoder.h"

/**
 * xlnx_dec_cleanup_input_ctx: Close the input file and free the input buffer.
 * @param input_ctx: The input context
 */
static void xlnx_dec_cleanup_input_ctx(XlnxDecoderInputCtx* input_ctx) {
  if (fcntl(input_ctx->input_file_info.in_file, F_GETFD) != -1 || errno != EBADF) {
    close(input_ctx->input_file_info.in_file);
  }
  if (input_ctx->input_file_info.input_buffer.data) {
    free(input_ctx->input_file_info.input_buffer.data);
  }
}

/**
 * xlnx_dec_cleanup_dec_ctx: Cleanup the context - free resources, close files.
 *
 * @param ctx: The decoder context
 */
void xlnx_dec_cleanup_dec_ctx(XlnxDecoderCtx* ctx) {
  if (!ctx) {
    return;
  }
  if (ctx->xma_dec_session) {
    xma_dec_session_destroy(ctx->xma_dec_session);
  }
  if (ctx->download_session) {
    xma_filter_session_destroy(ctx->download_session);
  }
  if (ctx->handle) {
    xma_release(ctx->handle);
  }
  if (ctx->log) {
    xma_log_release(ctx->log);
  }
  xrm_dec_release_v2(&ctx->xrm_dec_ctx);
  xlnx_dec_cleanup_decoder_props(&ctx->dec_xma_props);
  xlnx_dec_cleanup_input_ctx(&ctx->input_ctx);
}

/**
 * xlnx_dec_device_init: Get/allocate xrm resources, xma initialize.
 *
 * @param ctx: The decoder context
 * @return XMA_APP_SUCCESS on success XMA_APP_ERROR on error
 */
int32_t xlnx_dec_device_init(XlnxDecoderCtx* ctx) {
  int ret;
  // Reserve xrm resource
  XrmDecodePropsV2* xrm_props = (XrmDecodePropsV2*) xrm_props_create(XRM_IP_DECODER);
  if (!xrm_props) {
    xma_logmsg(ctx->log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Unable to create xrm properties\n");
    return XMA_APP_ERROR;
  }
  const XmaDecoderProperties* xma_dec_props = &ctx->dec_xma_props;
  xrm_props->input.width                    = xma_dec_props->width;
  xrm_props->input.height                   = xma_dec_props->height;
  xrm_props->input.fps_num                  = xma_dec_props->framerate.numerator;
  xrm_props->input.fps_den                  = xma_dec_props->framerate.denominator;
  xrm_props->dev_index                      = ctx->dec_props.device_id;
  if (xrm_dec_reserve_v2(&ctx->xrm_dec_ctx, xrm_props) == XRM_ERROR) {
    return XMA_APP_ERROR;
  }
  xrm_props_destroy((void*) &xrm_props);

  XmaInitParameter xma_init_param;
  memset(&xma_init_param, 0, sizeof(XmaInitParameter));
  char m_app_name[32];
  strcpy(m_app_name, XLNX_DEC_APP_MODULE);
  xma_init_param.app_name = m_app_name;
  xma_init_param.device   = ctx->dec_props.device_id;

  XmaParameter params[1];
  uint32_t     api_version = XMA_API_VERSION_1_2;

  params[0].name   = (char*) XMA_API_VERSION;
  params[0].type   = XMA_UINT32;
  params[0].length = sizeof(uint32_t);
  params[0].value  = &api_version;

  xma_init_param.params    = params;
  xma_init_param.param_cnt = 1;

  if ((ret = xma_initialize(ctx->log, &xma_init_param, &ctx->handle)) != XMA_SUCCESS) {
    xma_logmsg(ctx->log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "XMA Initialization failed\n");
    return XMA_APP_ERROR;
  }
  xma_logmsg(ctx->log, XMA_INFO_LOG, XLNX_DEC_APP_MODULE, "XMA initialization success\n");
  ctx->dec_xma_props.handle      = ctx->handle;
  ctx->xma_download_props.handle = ctx->handle;
  return ret;
}

/**
 * xlnx_dec_validate_parse_data: Checks the parse_data to make sure it doesn't
 * have obvious violations.
 * @param parse_data: The parse data struct to be checked.
 * @param handle: Xma Handle
 * @return XMA_APP_SUCCESS on success XMA_APP_ERROR on error
 */
static int32_t xlnx_dec_validate_parse_data(XmaLogHandle log, XlnxDecFrameParseData parse_data) {
  if (parse_data.width == 0 || parse_data.height == 0 || parse_data.width > MAX_HEVC_DEC_WIDTH || parse_data.height > MAX_HEVC_DEC_WIDTH ||
      parse_data.width * parse_data.height > MAX_HEVC_DEC_WIDTH * MAX_HEVC_DEC_HEIGHT) {
    xma_logmsg(log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Invalid resolution! %dx%d\n", parse_data.width, parse_data.height);
    return XMA_APP_ERROR;
  }
  if (parse_data.fr_num == 0 || parse_data.fr_den == 0) {
    xma_logmsg(log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Invalid fps! Numerator: %d, denominator %d\n", parse_data.fr_num, parse_data.fr_den);
    return XMA_APP_ERROR;
  }
  return XMA_APP_SUCCESS;
}

/**
 * xlnx_dec_create_input_ctx: Create the input context based on commandline
 * arguments
 * @param arguments: The arguments containing commandline info
 * @param ctx: The decoder context
 * @return XMA_APP_SUCCESS on success XMA_APP_ERROR on error
 */
static int32_t xlnx_dec_create_input_ctx(char* input_file_name, long loop_count, int decoder_type, XlnxDecoderCtx* ctx) {
  int                  ret;
  XlnxDecoderInputCtx* input_ctx = &ctx->input_ctx;
  struct XlnxStateInfo input_info;
  input_info.input_buffer.data      = NULL;
  input_info.input_buffer.size      = 0;
  input_info.input_buffer.allocated = 0;
  input_info.loop                   = loop_count;
  input_info.in_file                = open(input_file_name, O_RDONLY);
  if (input_info.in_file == -1) {
    xma_logmsg(ctx->log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Unable to open input file %s\n", input_file_name);
    exit(XMA_APP_ERROR);
  }
  XlnxDecFrameParseData parse_data;
  xlnx_dec_init_parse_data(&parse_data);
  ret = xlnx_dec_get_in_buf(ctx->handle, input_info.in_file, &input_info.input_buffer, 4096);
  if (ret != XMA_APP_SUCCESS) {
    xma_logmsg(ctx->log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Unable to read from file %s!\n", input_file_name);
    close(input_info.in_file);
    exit(XMA_APP_ERROR);
  }
  int offset = 0;
  if (decoder_type == HEVC_CODEC_TYPE) {
    ret = xlnx_dec_parse_next_h265_au(ctx->log, input_info.in_file, &input_info.input_buffer, &parse_data, 1, &offset);
  } else {
    ret = xlnx_dec_parse_next_h264_au(ctx->log, input_info.in_file, &input_info.input_buffer, &parse_data, 1, &offset);
  }
  if (ret != XMA_APP_SUCCESS) {
    xma_logmsg(ctx->log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE,
        "Unable to parse first input unit from input file %s!\nMake "
        "sure decoder type matches file type and is an elementary "
        "bitstream!\n",
        input_file_name);
    close(input_info.in_file);
    exit(XMA_APP_ERROR);
  }
  ret = xlnx_dec_validate_parse_data(ctx->log, parse_data);
  if (ret != XMA_APP_SUCCESS) {
    exit(XMA_APP_ERROR);
  }
  input_ctx->input_file_info = input_info;
  input_ctx->parse_data      = parse_data;
  return XMA_APP_SUCCESS;
}

/**
 * xlnx_dec_create_frame_props: Create the xma frame properties for
 * decoding
 * @param frame_props: the frame properties
 * @param bool: Check type of frame properties
 * @return XMA_APP_SUCCESS on success
 */
int32_t xlnx_dec_create_frame_props(XlnxDecoderCtx* ctx, bool is_device_props) {
  XmaFrameProperties* frame_props;
  if (is_device_props) {
    frame_props = &ctx->device_frame_props;
  } else {
    frame_props = &ctx->host_frame_props;
  }
  int32_t planes, plane;
  memset(frame_props, 0, sizeof(XmaFrameProperties));
  frame_props->format    = is_device_props == true ? XMA_VPE_FMT_TYPE : ctx->dec_props.out_pix_fmt;
  frame_props->sw_format = ctx->dec_props.out_pix_fmt;
  if (ctx->dec_props.resize_width && ctx->dec_props.resize_height) {
    frame_props->width  = ctx->dec_props.resize_width;
    frame_props->height = ctx->dec_props.resize_height;
  } else {
    frame_props->width  = ctx->dec_props.width;
    frame_props->height = ctx->dec_props.height;
  }
  frame_props->bits_per_pixel = ctx->dec_props.bit_depth;
  planes                      = xma_frame_planes_get(ctx->handle, frame_props);
  for (plane = 0; plane < planes; plane++) {
    frame_props->linesize[plane] = xma_frame_get_plane_stride(ctx->handle, frame_props, plane);
  }
  return XMA_APP_SUCCESS;
}

/**
 * xlnx_dec_create_xma_param_ctx: Add custom decoder params to the decoder
 * context
 * @param arguments: The arguments containing commandline info
 * @param ctx: The decoder context
 * @return XMA_APP_SUCCESS on success
 */
static int32_t xlnx_dec_create_xma_param_ctx(XlnxDecoderCtx* ctx)

{
  int                    ret;
  XlnxDecFrameParseData  parse_data = ctx->input_ctx.parse_data;
  XlnxDecoderProperties* param_ctx  = &ctx->dec_props;
  param_ctx->fps                    = parse_data.fr_num / parse_data.fr_den;
  param_ctx->width                  = parse_data.width;
  param_ctx->height                 = parse_data.height;
  /* Decoder plugin only supports zero copy */
  param_ctx->zero_copy   = 1;
  param_ctx->bit_depth   = parse_data.luma_bit_depth;
  param_ctx->chroma_mode = 420;
  /* 0 = unknown; 1 = progressive; 2 = top first; 3 = bottom first */
  param_ctx->scan_type = 1;
  if (param_ctx->codec_type == H264_CODEC_TYPE) {
    param_ctx->profile_idc = parse_data.h264_seq_parameter_set[parse_data.h264_pic_parameter_set[parse_data.current_h264_pps].seq_param_set_id].profile_idc;

    param_ctx->level_idc = parse_data.h264_seq_parameter_set[parse_data.h264_pic_parameter_set[parse_data.current_h264_pps].seq_param_set_id].level_idc;
  } else if (param_ctx->codec_type == H265_CODEC_TYPE) {
    param_ctx->profile_idc = parse_data.hevc_seq_parameter_set[parse_data.latest_hevc_sps].profile_idc;

    param_ctx->level_idc = parse_data.hevc_seq_parameter_set[parse_data.latest_hevc_sps].level_idc;
  }
  /* Initialize frame properties for device frame */
  ret = xlnx_dec_create_frame_props(ctx, true);
  if (ret != XMA_SUCCESS) {
    return XMA_APP_ERROR;
  }
  /* Initialize frame properties for host frame */
  ret = xlnx_dec_create_frame_props(ctx, false);
  if (ret != XMA_APP_SUCCESS) {
    return XMA_APP_ERROR;
  }
  xlnx_dec_create_xma_dec_props(ctx->handle, ctx->log, param_ctx, &ctx->dec_xma_props, &ctx->xma_download_props);
  return XMA_APP_SUCCESS;
}

/**
 * Creates the decoder context. Opens input file and parses the first header
 * from it to get info, copies the dec_props to its own fields.
 * @param input_file_name: The name of the input file to be opened and parsed
 * @param loop_count: How many loops the user specified.
 * @param dec_props Contains the decoder properties used to create the xma
 * decoder properties. This is copied into the dec_ctx
 * @param dec_ctx The decoder context which will be created
 * @return XMA_APP_SUCCESS on success, XMA_APP_ERROR on error
 */
int32_t xlnx_dec_create_decoder_context(char* input_file_name, long loop_count, XlnxDecoderProperties dec_props, XlnxDecoderCtx* ctx) {
  int ret                    = XMA_APP_SUCCESS;
  ctx->dec_props             = dec_props; // Copy dec params to decoder context
  ctx->dec_props.out_pix_fmt = ctx->out_pix_fmt;
  ret                        = xlnx_dec_create_input_ctx(input_file_name, loop_count, dec_props.codec_type, ctx);
  if (ret != XMA_APP_SUCCESS) {
    return ret;
  }
  ret = xlnx_dec_create_xma_param_ctx(ctx);
  if (ret != XMA_APP_SUCCESS) {
    return ret;
  }
  return ret;
}

/**
 * xlnx_dec_scan_next_au: Scans the next access unit from the input file into
 * ctx->input_ctx.input_file_info.buffer. This should not be used for initial
 * setup.
 * @param ctx: The decoder context
 * @param input_ctx.input_file_info: Contains the input file and input buffer
 * @param input_ctx.parse_data: Contains/stores the parse data
 * @param offset: Pointer to the offset which is set by the decoder parser.
 * @return XMA_APP_SUCCESS on success XMA_APP_EOS on end of stream
 * XMA_APP_EOF on end of file
 */
int xlnx_dec_scan_next_au(XlnxDecoderCtx* ctx, int* offset) {
  int                    ret        = XMA_APP_ERROR;
  int                    codec_type = ctx->dec_props.codec_type;
  struct XlnxStateInfo*  input_info = &ctx->input_ctx.input_file_info;
  XlnxDecFrameParseData* parse_data = &ctx->input_ctx.parse_data;
  if (codec_type == H264_CODEC_TYPE) {
    ret = xlnx_dec_parse_next_h264_au(ctx->log, input_info->in_file, &input_info->input_buffer, parse_data, 0, offset);
  } else if (codec_type == H265_CODEC_TYPE) {
    ret = xlnx_dec_parse_next_h265_au(ctx->log, input_info->in_file, &input_info->input_buffer, parse_data, 0, offset);
  }
  if (ret == XMA_APP_ERROR) {
    xma_logmsg(ctx->log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Error finding access unit from input file!\n");
    xlnx_dec_cleanup_dec_ctx(ctx);
    exit(XMA_APP_ERROR);
  }

  if (ret == XMA_APP_EOF) {
    if (input_info->loop > 0 || input_info->loop == -1) {
      lseek(input_info->in_file, 0, SEEK_SET);
      if (input_info->loop != -1) {
        input_info->loop--;
      }
    } else if (input_info->input_buffer.size <= 0) {
      return XMA_APP_EOS;
    }
  }
  return ret;
}

/**
 * xlnx_dec_send_null_data: Send null data to flush the remaining frames out of
 * the device
 * @param ctx: The decoder context
 * @param is_eof: Whether this is the last frame to be sent or not.
 * @return XMA_SUCCESS on success XMA_ERROR on error
 */
int xlnx_dec_send_null_data(XlnxDecoderCtx* ctx) {
  // Flush decoder
  int ret = XMA_APP_ERROR;
  ret     = xma_dec_session_send_data(ctx->xma_dec_session, NULL, 0);
  if (ret == XMA_TRY_AGAIN) {
    ret = XMA_SUCCESS;
  }
  return ret;
}

/**
 * xlnx_dec_send_data: sends data to the decoder for processing
 * @param ctx: ctx for the decoder.
 * @param session: Used to send data to xma plugin
 * @param input_buffer: Contains the data to send
 * @param end: amount of data to send
 * @return XMA_APP_SUCCESS on success, otherwise XMA_APP_ERROR
 */
int xlnx_dec_send_data(XlnxDecoderCtx* ctx, int size) {
  int            data_used    = 0;
  int            offset       = 0;
  int            rc           = XMA_ERROR;
  XlnxDecBuffer* input_buffer = &ctx->input_ctx.input_file_info.input_buffer;
  ctx->input_ctx.xbuffer      = xma_data_buffer_alloc(ctx->handle, size, false);
  XmaDataBuffer* xbuffer      = ctx->input_ctx.xbuffer;
  while (offset < size) {
    memmove(xbuffer->data.buffer, input_buffer->data + offset, size - offset);
    xbuffer->alloc_size = size;
    xbuffer->is_eof     = 0;
    xbuffer->pts        = XMA_AV_NOPTS_VALUE;
    rc                  = xma_dec_session_send_data(ctx->xma_dec_session, xbuffer, &data_used);
    if (rc == XMA_ERROR) {
      xma_logmsg(ctx->log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Error sending data to decoder. Data %d\n", ctx->num_frames_sent);
      xlnx_dec_cleanup_dec_ctx(ctx);
      exit(XMA_APP_ERROR);
    } else if (rc == XMA_TRY_AGAIN) {
      /* plugin buffers full, need to take frames out. */
      xma_data_buffer_free(ctx->input_ctx.xbuffer);
      break;
    }
    offset += data_used;
    ctx->num_frames_sent++;
  }
  memmove(input_buffer->data, input_buffer->data + offset, input_buffer->size - offset);
  input_buffer->size = input_buffer->size - offset;

  return XMA_APP_SUCCESS;
}

/**
 * Send the next access unit and receive the next frame into the output_xframe
 * @param ctx: The decoder context
 * @param output_xframe: The xma frame which stores the decoder output
 * @return XMA_SUCCESS on success or XMA_ERROR on error.
 */
int xlnx_dec_process_frame(XlnxDecoderCtx* dec_ctx) {
  int ret;
  int offset = 0;
  ret        = xlnx_dec_scan_next_au(dec_ctx, &offset); // eof, success, eos, error..
  if (ret == XMA_APP_EOF) {
    ret = xlnx_dec_scan_next_au(dec_ctx, &offset);
  }
  if (ret == XMA_APP_EOS) {
    ret = xlnx_dec_send_null_data(dec_ctx);
  } else {
    ret = xlnx_dec_send_data(dec_ctx, offset);
  }
  if (ret == XMA_ERROR) {
    xma_logmsg(dec_ctx->log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Unable to send data to decoder plugin.\n");
    return XMA_ERROR;
  }
  dec_ctx->output_xframe = xma_frame_alloc(dec_ctx->handle, &dec_ctx->device_frame_props, true);
  if (ret != XMA_APP_SUCCESS) {
    return XMA_ERROR;
  }

  ret = xma_dec_session_recv_frame(dec_ctx->xma_dec_session, dec_ctx->output_xframe);

  if (ret < 0) {
    xma_frame_free(dec_ctx->output_xframe);
    dec_ctx->output_xframe = NULL;
    xma_logmsg(dec_ctx->log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Unable to receive data from decoder plugin.\n");
    return XMA_ERROR;
  }
  if (ret == XMA_SUCCESS) {
    dec_ctx->num_frames_received++;
    ret = xma_filter_session_send_frame(dec_ctx->download_session, dec_ctx->output_xframe);
    if (ret < 0) {
      xma_logmsg(dec_ctx->log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "xma filter: Unable to send frame.\n");
    } else if (ret == XMA_SUCCESS) {
      dec_ctx->output_xframe = xma_frame_alloc(dec_ctx->handle, &dec_ctx->host_frame_props, true);
      ret                    = xma_filter_session_recv_frame(dec_ctx->download_session, dec_ctx->output_xframe);
      if (ret < 0) {
        xma_logmsg(dec_ctx->log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "xma filter: Unable to recv frame.\n");
      } else if (ret != XMA_SUCCESS) {
        xma_frame_free(dec_ctx->output_xframe);
        dec_ctx->output_xframe = NULL;
      }
    }
  } else if (ret == XMA_EOS) {
    xma_frame_free(dec_ctx->output_xframe);
    dec_ctx->output_xframe = NULL;
    if (!dec_ctx->is_flush_sent) {
      ret                    = xma_filter_session_send_frame(dec_ctx->download_session, dec_ctx->output_xframe);
      dec_ctx->is_flush_sent = true;
    } else {
      ret = XMA_SUCCESS;
    }
    if (ret < 0) {
      xma_logmsg(dec_ctx->log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "xma filter: Unable to send frame.\n");
    } else if (ret == XMA_SUCCESS) {
      dec_ctx->output_xframe = xma_frame_alloc(dec_ctx->handle, &dec_ctx->host_frame_props, true);
      ret                    = xma_filter_session_recv_frame(dec_ctx->download_session, dec_ctx->output_xframe);
      if (ret < 0) {
        xma_logmsg(dec_ctx->log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "xma filter: Unable to recv frame.\n");
      } else if (ret != XMA_SUCCESS) {
        xma_frame_free(dec_ctx->output_xframe);
        dec_ctx->output_xframe = NULL;
      }
    }
  } else {
    xma_frame_free(dec_ctx->output_xframe);
    dec_ctx->output_xframe = NULL;
  }
  return ret;
}
