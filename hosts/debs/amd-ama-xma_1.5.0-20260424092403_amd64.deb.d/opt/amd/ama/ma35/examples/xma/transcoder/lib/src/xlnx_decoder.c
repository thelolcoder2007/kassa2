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

#include <pthread.h>
#include "xlnx_decoder.h"

static struct option dec_options[] = {{FLAG_DEC_INPUT_FILE, required_argument, 0, DEC_INPUT_FILE_ARG}, {FLAG_DEC_CODEC_TYPE, required_argument, 0, DEC_CODEC_ID_ARG},
    {FLAG_DEC_LOW_LATENCY, required_argument, 0, DEC_LOW_LATENCY_ARG}, {FLAG_DEC_LATENCY_LOGGING, required_argument, 0, DEC_LATENCY_LOGGING_ARG},
    {FLAG_DEC_RESIZE_WIDTH, required_argument, 0, DEC_RESIZE_WIDTH_ARG}, {FLAG_DEC_RESIZE_HEIGHT, required_argument, 0, DEC_RESIZE_HEIGHT_ARG}, {0, 0, 0, 0}};

/**
 * xlnx_dec_create_frame_props: Create the xma frame properties for
 * decoding
 * @param frame_props: the frame properties
 * @param bool: Check type of frame properties
 * @return XMA_APP_SUCCESS on success
 */
int32_t xlnx_dec_create_frame_props(XlnxDecoderCtx* ctx) {
  XmaFrameProperties* frame_props;
  frame_props = &ctx->device_frame_props;
  int32_t planes, plane;

  memset(frame_props, 0, sizeof(XmaFrameProperties));
  frame_props->format    = XMA_VPE_FMT_TYPE;
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
  return XMA_SUCCESS;
}

/**
 * xlnx_dec_update_props: Updates XMA decoder properties and the parameters
 *
 * @param dec_ctx: Decoder context
 * @param xma_dec_props: XMA decoder properties structure
 */
int32_t xlnx_dec_update_props(XlnxDecoderCtx* dec_ctx, XmaDecoderProperties* xma_dec_props, XmaHandle handle) {
  XlnxDecoderProperties* dec_props  = &dec_ctx->dec_props;
  XlnxDecFrameData*      frame_data = &dec_ctx->frame_data;

  dec_props->width     = frame_data->width;
  dec_props->height    = frame_data->height;
  dec_props->fps       = ((double) frame_data->fr_num / (double) frame_data->fr_den);
  dec_props->bit_depth = frame_data->luma_bit_depth;

  int32_t ret;
  dec_ctx->handle = handle;

  if (dec_props->codec_type == DECODER_ID_H264) {
    dec_props->profile_idc = frame_data->h264_seq_parameter_set[frame_data->h264_pic_parameter_set[frame_data->current_h264_pps].seq_parameter_set_id].profile_idc;
  } else if (dec_props->codec_type == DECODER_ID_HEVC) {
    dec_props->profile_idc = frame_data->hevc_seq_parameter_set[frame_data->latest_hevc_sps].profile_idc;
  }

  if (dec_props->codec_type == DECODER_ID_H264) {
    dec_props->level_idc = frame_data->h264_seq_parameter_set[frame_data->h264_pic_parameter_set[frame_data->current_h264_pps].seq_parameter_set_id].level_idc;
  } else if (dec_props->codec_type == DECODER_ID_HEVC) {
    dec_props->level_idc = frame_data->hevc_seq_parameter_set[frame_data->latest_hevc_sps].level_idc;
  }

  if (dec_props->bit_depth == BITS_PER_PIXEL_8) {
    dec_props->out_pix_fmt = XMA_YUV420P_FMT_TYPE;
  } else if (dec_props->bit_depth == BITS_PER_PIXEL_10) {
    dec_props->out_pix_fmt = XMA_YUV420P10LE_FMT_TYPE;
  }

  /* Initialize frame properties for device frame */
  ret = xlnx_dec_create_frame_props(dec_ctx);
  if (ret != XMA_SUCCESS) {
    return XMA_ERROR;
  }

  xlnx_dec_get_xma_props(dec_ctx->handle, &dec_ctx->dec_props, xma_dec_props);

  return XMA_SUCCESS;
}

/**
 * init_parse_data: Initialize the decoder header structure
 *
 * @param dec_frame_data: Decoder frame data
 * @return XMA_APP_SUCCESS on success
 */
static int32_t xlnx_dec_init_parse_data(XlnxDecFrameData* dec_frame_data) {
  memset(dec_frame_data, 0, sizeof(XlnxDecFrameData));

  int i;
  for (i = 0; i < 32; i++)
    dec_frame_data->h264_seq_parameter_set[i].valid = 0;
  for (i = 0; i < 256; i++)
    dec_frame_data->h264_pic_parameter_set[i].valid = 0;

  dec_frame_data->last_h264_slice_header.delta_pic_order_cnt_bottom = -1;
  dec_frame_data->last_h264_slice_header.delta_pic_order_cnt[0]     = -1;
  dec_frame_data->last_h264_slice_header.delta_pic_order_cnt[1]     = -1;
  dec_frame_data->last_h264_slice_header.frame_num                  = 0;
  dec_frame_data->last_h264_slice_header.idr_pic_id                 = 0;
  dec_frame_data->last_h264_slice_header.pic_order_cnt_lsb          = 0;
  dec_frame_data->last_h264_slice_header.pic_parameter_set_id       = 0;
  dec_frame_data->last_h264_slice_header.field_pic_flag             = 0;
  dec_frame_data->last_h264_slice_header.bottom_field_flag          = 0;
  dec_frame_data->last_h264_slice_header.nal_ref_idc                = 0;
  dec_frame_data->last_h264_slice_header.nal_unit_type              = 0;

  return XMA_APP_SUCCESS;
}

/**
 * xlnx_dec_context_init: Initializes decoder context
 *
 * @param dec_ctx: Decoder context
 */
void xlnx_dec_context_init(XlnxDecoderCtx* dec_ctx) {
  /* Initialize the decoder parameters to default */
  dec_ctx->dec_props.entropy_buf_cnt = 2;
  dec_ctx->dec_props.low_latency     = 0;
  dec_ctx->dec_props.latency_logging = 0;
  dec_ctx->dec_props.splitbuff_mode  = 0;
  dec_ctx->dec_props.bit_depth       = 8;
  dec_ctx->dec_props.codec_type      = -1;
  dec_ctx->flush_sent                = false;
  dec_ctx->dec_props.scan_type       = 1;
  dec_ctx->dec_props.chroma_mode     = 420;
  dec_ctx->dec_props.out_pix_fmt     = XMA_YUV420P_FMT_TYPE;
  dec_ctx->dec_props.resize_width    = 0;
  dec_ctx->dec_props.resize_height   = 0;
  /* always zero copy output */
  dec_ctx->dec_props.zero_copy = 1;

  memset(dec_ctx->out_frame, 0, sizeof(XmaFrame));
  dec_ctx->out_frame->data[0].buffer_type = XMA_DEVICE_BUFFER_TYPE;
  dec_ctx->out_frame->data[0].buffer      = NULL;

  xlnx_dec_init_parse_data(&dec_ctx->frame_data);
  return;
}

/**
 * xlnx_dec_validate_arguments: Validates decoder arguments
 *
 * @param dec_props: Decoder properties to be validated
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
static int32_t xlnx_dec_validate_arguments(XlnxDecoderCtx* dec_ctx) {
  if ((dec_ctx->dec_props.low_latency != 0) && (dec_ctx->dec_props.low_latency != 1)) {
    fprintf(stderr, "Invalid decoder low_latency %d \n", dec_ctx->dec_props.low_latency);
    return XMA_APP_ERROR;
  }

  if ((dec_ctx->dec_props.latency_logging != 0) && (dec_ctx->dec_props.latency_logging != 1)) {
    fprintf(stderr, "Invalid decoder latency_logging %d \n", dec_ctx->dec_props.latency_logging);
    return XMA_APP_ERROR;
  }

  if ((dec_ctx->dec_props.stream_model != 0) && (dec_ctx->dec_props.stream_model != 1)) {
    fprintf(stderr, "Invalid decoder stream model %d \n", dec_ctx->dec_props.stream_model);
    return XMA_APP_ERROR;
  }

  return XMA_APP_SUCCESS;
}

/**
 * xlnx_dec_parse_args: Parses the decode command line arguments
 *
 * @param argc: Argument count
 * @param *argv[]: Arguments
 * @param dec_ctx: Decoder context
 * @param the caller
 * @return XMA_APP_SUCCESS on success, otherwise XMA_APP_ERROR
 */
int32_t xlnx_dec_parse_args(int32_t argc, char* argv[], XlnxDecoderCtx* dec_ctx, int32_t param_flag) {
  size_t  file_name_length;
  int32_t option_index;
  int32_t flag = 0;
  int32_t ret  = XMA_APP_SUCCESS;

  while (flag != DEC_INPUT_FILE_ARG) {
    if (param_flag == 0) {
      flag = getopt_long_only(argc, argv, "", dec_options, &option_index);
      if (flag == -1) {
        fprintf(stderr, "Error in decoder parameters parsing\n");
        return XMA_APP_ERROR;
      }
    } else {
      flag       = param_flag;
      param_flag = 0;
    }

    switch (flag) {
    case DEC_CODEC_ID_ARG:
      if (!strcmp(optarg, AVC_PATTERN_MATCH)) {
        dec_ctx->dec_props.codec_type = DECODER_ID_H264;
      } else if (!strcmp(optarg, HEVC_PATTERN_MATCH)) {
        dec_ctx->dec_props.codec_type = DECODER_ID_HEVC;
      } else {
        fprintf(stderr, "Unsupported decoder codec option %s\n", optarg);
        return XMA_APP_ERROR;
      }
      break;

    case DEC_INPUT_FILE_ARG:
      dec_ctx->in_file = open(optarg, O_RDONLY);
      if (dec_ctx->in_file == XMA_APP_ERROR) {
        fprintf(stderr, "Error opening input file %s\n", optarg);
        return XMA_APP_ERROR;
      }
      file_name_length      = strlen(optarg);
      dec_ctx->in_file_name = calloc(file_name_length + 1, 1);
      if (dec_ctx->in_file_name == NULL) {
        fprintf(stderr, "Memory allocation failure\n");
        close(dec_ctx->in_file);
        return XMA_APP_ERROR;
      }
      strcpy(dec_ctx->in_file_name, optarg);
      break;

    case DEC_LOW_LATENCY_ARG:
      ret = xlnx_utils_set_uint_arg(&dec_ctx->dec_props.low_latency, optarg, FLAG_DEC_LOW_LATENCY);
      break;

    case DEC_RESIZE_WIDTH_ARG:
      ret = xlnx_utils_set_uint_arg(&dec_ctx->dec_props.resize_width, optarg, FLAG_DEC_RESIZE_WIDTH);
      break;

    case DEC_RESIZE_HEIGHT_ARG:
      ret = xlnx_utils_set_uint_arg(&dec_ctx->dec_props.resize_height, optarg, FLAG_DEC_RESIZE_HEIGHT);
      break;

    case DEC_LATENCY_LOGGING_ARG:
      ret = xlnx_utils_set_uint_arg(&dec_ctx->dec_props.latency_logging, optarg, FLAG_DEC_LATENCY_LOGGING);
      break;

    default:
      fprintf(stderr, "Error in parsing decoder arguments\n");
      return XMA_APP_ERROR;
    }

    if (ret == XMA_APP_ERROR) {
      return XMA_APP_ERROR;
    }
  }

  return xlnx_dec_validate_arguments(dec_ctx);
}

/**
 * xlnx_dec_parse_frame: Parses decoder frame to get the properties
 *
 * @param dec_ctx: Decoder context
 * @return XMA_APP_SUCCESS on success, otherwise XMA_APP_ERROR
 */
int32_t xlnx_dec_parse_frame(XlnxDecoderCtx* dec_ctx) {
  int32_t ret;

  /* Parsing the input file for decoder properties */
  /* Initializing input buffer */
  if ((ret = xlnx_dec_get_in_buf(dec_ctx->in_file, &dec_ctx->in_buffer, 1024)) != XMA_APP_SUCCESS)
    return XMA_APP_ERROR;

  /* parsing the first unit to get frame size and frame rate */
  XlnxDecFrameData* in_frame_data = &dec_ctx->frame_data;

  if (dec_ctx->dec_props.codec_type == DECODER_ID_H264) {
    if ((ret = xlnx_dec_parse_h264_au(dec_ctx->in_file, &dec_ctx->in_buffer, in_frame_data, &dec_ctx->in_offset)) != XMA_APP_SUCCESS) {
      fprintf(stderr, "Failed to find first unit in h264 input video file!\n");
      return XMA_APP_ERROR;
    }
  } else if (dec_ctx->dec_props.codec_type == DECODER_ID_HEVC) {
    if ((ret = xlnx_dec_parse_hevc_au(dec_ctx->in_file, &dec_ctx->in_buffer, in_frame_data, &dec_ctx->in_offset)) != XMA_APP_SUCCESS) {
      fprintf(stderr, "Failed to find first unit in hevc input video file!\n");
      return XMA_APP_ERROR;
    }
  }

  if ((in_frame_data->width == 0) || (in_frame_data->height == 0)) {
    fprintf(stderr, "Decoder frame size not set!\n");
    return XMA_APP_ERROR;
  } else if ((in_frame_data->fr_num == 0) || (in_frame_data->fr_den == 0)) {
    fprintf(stderr, "Decoder frame rate not set!\n");
    return XMA_APP_ERROR;
  }
  if ((in_frame_data->luma_bit_depth != BITS_PER_PIXEL_8) && (in_frame_data->luma_bit_depth != BITS_PER_PIXEL_10)) {
    fprintf(stderr, "Unsupported input bit depth\n");
    return XMA_APP_ERROR;
  }

  return ret;
}

/**
 * dec_session: Creates decoder session
 *
 * @param app_xrm_ctx: Transcoder XRM context
 * @param dec_ctx: Decoder context
 * @param xma_dec_props: XMA decoder properties
 * @return XMA_APP_SUCCESS on success, otherwise XMA_APP_ERROR
 */
int32_t xlnx_dec_session(XlnxDecoderCtx* dec_ctx, XmaDecoderProperties* xma_dec_props) {
  if (!dec_ctx->handle) {
    dec_ctx->handle = xma_dec_props->handle;
  }
  dec_ctx->dec_session = xma_dec_session_create(xma_dec_props);
  if (!dec_ctx->dec_session) {
    xma_logmsg(dec_ctx->log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Failed decoder session create\n");
    return XMA_APP_ERROR;
  }

  return XMA_APP_SUCCESS;
}

/*-----------------------------------------------------------------------------
dec_read_frame_args: Reads decoder frame data from the input file with arguments

Parameters:
dec_ctx: Decoder context

Return:
XMA_APP_SUCCESS on success, otherwise ERROR
-----------------------------------------------------------------------------*/
int32_t xlnx_dec_read_frame_args(int32_t codec_type, int32_t in_file, int32_t* in_offset, XlnxDecFrameData* in_frame_data, XlnxDecBuffer* in_buffer) {
  int32_t ret = XMA_APP_SUCCESS;

  if (codec_type == DECODER_ID_H264) {
    if ((ret = xlnx_dec_parse_h264_au(in_file, in_buffer, in_frame_data, in_offset)) <= XMA_APP_ERROR) {
      fprintf(stderr, "Failed to find first unit in h264 input video file!\n");
      return ret;
    }
  } else if (codec_type == DECODER_ID_HEVC) {
    if ((ret = xlnx_dec_parse_hevc_au(in_file, in_buffer, in_frame_data, in_offset)) <= XMA_APP_ERROR) {
      fprintf(stderr, "Failed to find first unit in hevc input video file!\n");
      return ret;
    }
  }
  return ret;
}

/**
 * dec_read_frame: Reads decoder frame data from the input file
 *
 * @param dec_ctx: Decoder context
 * @return XMA_APP_SUCCESS on success, otherwise ERROR
 */
int32_t xlnx_dec_read_frame(XlnxDecoderCtx* dec_ctx) {
  return xlnx_dec_read_frame_args(dec_ctx->dec_props.codec_type, dec_ctx->in_file, &dec_ctx->in_offset, &dec_ctx->frame_data, &dec_ctx->in_buffer);
}

/**
 * dec_send_frame: Sends data to the decoder for processing
 *
 * @param dec_ctx: Decoder context
 * @return XMA_APP_SUCCESS on success, otherwise XMA_APP_ERROR
 */
int32_t xlnx_dec_send_frame(XlnxDecoderCtx* dec_ctx) {
  int                data_used;
  int                index       = 0;
  int32_t            ret         = XMA_APP_ERROR;
  XmaDecoderSession* dec_session = dec_ctx->dec_session;
  XlnxDecBuffer*     in_buf      = &(dec_ctx->in_buffer);
  dec_ctx->dec_in_buf            = xma_data_buffer_alloc(dec_ctx->handle, dec_ctx->in_offset, false);
  if (!dec_ctx->dec_in_buf) {
    xma_logmsg(dec_ctx->log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Failed to allocate data buffer for sending!\n");
    return XMA_APP_ERROR;
  }

  while (index < dec_ctx->in_offset) {
    memmove(dec_ctx->dec_in_buf->data.buffer, in_buf->data + index, dec_ctx->in_offset - index);
    dec_ctx->dec_in_buf->pts        = XMA_AV_NOPTS_VALUE;
    dec_ctx->dec_in_buf->alloc_size = dec_ctx->in_offset;
    dec_ctx->dec_in_buf->is_eof     = 0;
    ret                             = xma_dec_session_send_data(dec_session, dec_ctx->dec_in_buf, &data_used);
    if (ret == XMA_ERROR) {
      xma_logmsg(dec_ctx->log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Error sending data to decoder =%d\n", ret);
      return XMA_APP_ERROR;
    } else if (ret == XMA_TRY_AGAIN) {
      xma_data_buffer_free(dec_ctx->dec_in_buf);
      break;
    }
    index += data_used;
  }
  memmove(in_buf->data, in_buf->data + index, in_buf->size - index);
  in_buf->size = in_buf->size - index;

  return ret;
}

/**
 * dec_recv_frame: Receives output data from the decoder
 *
 * @param dec_ctx: Decoder context
 * @return XMA_APP_SUCCESS on success, otherwise XMA_APP_ERROR
 */
int32_t xlnx_dec_recv_frame(XlnxDecoderCtx* dec_ctx) {
  return xma_dec_session_recv_frame(dec_ctx->dec_session, dec_ctx->out_frame);
}

/**
 * dec_get_input_size: Returns the size of the input read and to be sent to the
 * decoder
 *
 * @param dec_ctx: Decoder context
 * @return Size of decoder input
 */
int32_t xlnx_dec_get_input_size(XlnxDecoderCtx* dec_ctx) {
  return dec_ctx->in_buffer.size;
}

/**
 * dec_send_null_frame: Sends null frame to the decoder to flush the pipeline
 *
 * @param dec_ctx: Decoder context
 * @return XMA_APP_SUCCESS on success, otherwise ERROR
 */
int32_t xlnx_dec_send_null_frame(XlnxDecoderCtx* dec_ctx) {
  // Flush decoder
  int ret = XMA_ERROR;
  ret     = xma_dec_session_send_data(dec_ctx->dec_session, NULL, 0);
  if (ret == XMA_TRY_AGAIN) {
    ret = XMA_SUCCESS;
  }
  return ret;
}

/**
 * dec_deinit: Sends null frame to the decoder to start decoder flush
 *
 * @param xrm_ctx: XRM context
 * @param dec_ctx: Decoder context
 * @param xma_dec_props: XMA decoder properties
 * @return XMA_APP_SUCCESS on success, otherwise ERROR
 */
int32_t xlnx_dec_deinit(XlnxDecoderCtx* dec_ctx, XmaDecoderProperties* xma_dec_props) {
  int32_t ret = XMA_APP_ERROR;
  if (dec_ctx->dec_session != NULL) {
    ret = xma_dec_session_destroy(dec_ctx->dec_session);
  }
  xlnx_dec_free_xma_props(xma_dec_props);

  free(dec_ctx->in_file_name);
  close(dec_ctx->in_file);
  if (dec_ctx->in_buffer.data) {
    free(dec_ctx->in_buffer.data);
  }

  return ret;
}
