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

#include "xlnx_dec_xma_props.h"
#include "xrm_dec_interface.h"

int32_t xlnx_dec_res_chng_callback(XmaFrameProperties*, void*) {
  return XMA_SUCCESS;
}

/**
 * Frees resources allocated in up decoder
 * properties
 * @param dec_xma_props: The decoder properties
 * @return XMA_APP_SUCCESS on success
 */
void xlnx_dec_cleanup_decoder_props(XmaDecoderProperties* xma_dec_props) {
  if (xma_dec_props->params) {
    free(xma_dec_props->params);
  }
}

/**
 * Link the custom decoder params in the decoder
 * context to what will be sent to the decoder plugin
 * @param dec_props The context containing custom decoder parameters
 * @return XMA_APP_SUCCESS on success
 */
static int dec_fill_custom_xma_params(XlnxDecoderProperties* dec_props, XmaParameter* custom_xma_params, uint32_t* param_cnt) {
  custom_xma_params[*param_cnt].name   = XMA_DEC_PARAM_LOW_LATENCY;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(dec_props->low_latency);
  custom_xma_params[*param_cnt].value  = &(dec_props->low_latency);
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = XMA_DEC_PARAM_LATENCY_LOGGING;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(dec_props->latency_logging);
  custom_xma_params[*param_cnt].value  = &(dec_props->latency_logging);
  (*param_cnt)++;

  custom_xma_params[*param_cnt].name   = XMA_DEC_OUTPUT_FORMAT;
  custom_xma_params[*param_cnt].type   = XMA_INT32;
  custom_xma_params[*param_cnt].length = sizeof(dec_props->out_pix_fmt);
  custom_xma_params[*param_cnt].value  = &(dec_props->out_pix_fmt);
  (*param_cnt)++;

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

  custom_xma_params[*param_cnt].name   = XMA_DEC_PARAM_PROP_CHANGE_CALLBACK;
  custom_xma_params[*param_cnt].type   = XMA_FUNC_PTR;
  custom_xma_params[*param_cnt].length = sizeof(void*);
  custom_xma_params[*param_cnt].value  = xlnx_dec_res_chng_callback;
  (*param_cnt)++;

  return XMA_APP_SUCCESS;
}

/**
 * Validates the dec props are in proper ranges. Called internally by
 * xlnx_dec_validate_dec_props, but can also be called externally.
 * @param dec_props The xlnx decoder properties to be validated
 * @param handle: Xma Handle
 * @return XMA_SUCCESS or XMA_ERROR
 */
int xlnx_dec_validate_dec_props(XmaLogHandle log, XlnxDecoderProperties* dec_props) {
  if (dec_props->codec_type == H264_CODEC_TYPE) {
    if (dec_props->width < MIN_H264_DEC_HEIGHT || dec_props->height < MIN_H264_DEC_HEIGHT || dec_props->width > MAX_H264_DEC_WIDTH ||
        dec_props->height > MAX_H264_DEC_WIDTH || dec_props->width * dec_props->height > MAX_H264_DEC_WIDTH * MAX_H264_DEC_HEIGHT ||
        dec_props->width * dec_props->height < MIN_H264_DEC_HEIGHT * MIN_H264_DEC_WIDTH) {
      xma_logmsg(log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE,
          "Invalid resolutions %dx%d.\n Supported"
          " %dx%d <= resolution <= %xx%d (or portrait "
          "equivalent)\n",
          dec_props->width, dec_props->height, MIN_H264_DEC_WIDTH, MIN_H264_DEC_HEIGHT, MAX_H264_DEC_WIDTH, MAX_H264_DEC_HEIGHT);
      return XMA_APP_ERROR;
    }
  } else if (dec_props->codec_type == HEVC_CODEC_TYPE) {
    if (dec_props->width < MIN_HEVC_DEC_HEIGHT || dec_props->height < MIN_HEVC_DEC_HEIGHT || dec_props->width > MAX_HEVC_DEC_WIDTH ||
        dec_props->height > MAX_HEVC_DEC_WIDTH || dec_props->width * dec_props->height > MAX_HEVC_DEC_WIDTH * MAX_HEVC_DEC_HEIGHT ||
        dec_props->width * dec_props->height < MIN_HEVC_DEC_HEIGHT * MIN_HEVC_DEC_WIDTH) {
      xma_logmsg(log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE,
          "Invalid resolutions %dx%d.\n Supported"
          " %dx%d <= resolution <= %xx%d (or portrait "
          "equivalent)\n",
          dec_props->width, dec_props->height, MIN_HEVC_DEC_WIDTH, MIN_HEVC_DEC_HEIGHT, MAX_HEVC_DEC_WIDTH, MAX_HEVC_DEC_HEIGHT);
      return XMA_APP_ERROR;
    }
  } else {
    xma_logmsg(log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Invalid decoder codec id %d.\n", dec_props->codec_type);
    return XMA_APP_ERROR;
  }
  if (dec_props->bit_depth != 8 && dec_props->bit_depth != 10) {
    xma_logmsg(log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Invalid decoder codec id %d.\n", dec_props->bit_depth);
    return XMA_APP_ERROR;
  }
  if (dec_props->resize_width && dec_props->resize_height) {
    if ((dec_props->resize_width > dec_props->width) || (dec_props->resize_height > dec_props->height)) {
      xma_logmsg(log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Invalid resize resolution %dx%d.\n", dec_props->resize_width, dec_props->resize_height);
      return XMA_APP_ERROR;
    }
    if (dec_props->codec_type == H264_CODEC_TYPE) {
      if ((dec_props->resize_width < MIN_H264_DEC_HEIGHT) || (dec_props->resize_height < MIN_H264_DEC_HEIGHT)) {
        xma_logmsg(log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Invalid resize resolution %dx%d.\n", dec_props->resize_width, dec_props->resize_height);
        return XMA_APP_ERROR;
      }
    } else if (dec_props->codec_type == HEVC_CODEC_TYPE) {
      if ((dec_props->resize_width < MIN_HEVC_DEC_HEIGHT) || (dec_props->resize_height < MIN_HEVC_DEC_HEIGHT)) {
        xma_logmsg(log, XMA_ERROR_LOG, XLNX_DEC_APP_MODULE, "Invalid resize resolution %dx%d.\n", dec_props->resize_width, dec_props->resize_height);
        return XMA_APP_ERROR;
      }
    }
  }

  return XMA_APP_SUCCESS;
}

/**
 * Set the default decoder properties to be used to create xma props.
 * Does not set that which should be found in the input header.
 * @param dec_props The xlnx decoder properties whose defaults will be set
 * @return XMA_APP_SUCCESS
 */
int xlnx_dec_set_default_dec_props(XlnxDecoderProperties* dec_props) {
  memset(dec_props, 0, sizeof(XlnxDecoderProperties));
  dec_props->device_id   = xrm_interface_get_dev_index(); // Check xrm reserve IDs
  dec_props->chroma_mode = 420;
  dec_props->fps         = 30;
  dec_props->zero_copy   = 1;
  if (dec_props->out_pix_fmt == XMA_NV12_FMT_TYPE) {
    dec_props->planar = 0;
  } else {
    dec_props->planar = 1;
  }

  return XMA_APP_SUCCESS;
}

/**
 * Sets the decoder properties. Xrm related
 * properties will be set later in xlnx_dec_create_xrm_dec_ctx and
 * xlnx_dec_allocate_xrm_dec_cu
 * @param xma_dec_props The decoder properties to be set
 * @param dec_props The context used to set the decoder properties.
 * @param handle: Xma Handle
 * @param xma_download_props : xma download filter properties
 * @return XMA_APP_SUCCESS on success
 */
int xlnx_dec_create_xma_dec_props(
    XmaHandle handle, XmaLogHandle log, XlnxDecoderProperties* dec_props, XmaDecoderProperties* xma_dec_props, XmaFilterProperties* xma_download_props) {
  xma_download_props->hwfilter_type                = XMA_DOWNLOAD_FILTER_TYPE;
  xma_download_props->param_cnt                    = 0;
  xma_download_props->params                       = NULL;
  xma_download_props->handle                       = handle;
  xma_download_props->input.format                 = XMA_VPE_FMT_TYPE;
  xma_download_props->input.sw_format              = dec_props->out_pix_fmt;
  xma_download_props->input.width                  = dec_props->width;
  xma_download_props->input.height                 = dec_props->height;
  xma_download_props->input.framerate.numerator    = 60;
  xma_download_props->input.framerate.denominator  = 1;
  xma_download_props->output.format                = dec_props->out_pix_fmt;
  xma_download_props->output.sw_format             = dec_props->out_pix_fmt;
  xma_download_props->output.width                 = dec_props->width;
  xma_download_props->output.height                = dec_props->height;
  xma_download_props->output.framerate.numerator   = 60;
  xma_download_props->output.framerate.denominator = 1;
  if (dec_props->resize_width && dec_props->resize_height) {
    xma_download_props->input.width   = dec_props->resize_width;
    xma_download_props->input.height  = dec_props->resize_height;
    xma_download_props->output.width  = dec_props->resize_width;
    xma_download_props->output.height = dec_props->resize_height;
  }

  if (xlnx_dec_validate_dec_props(log, dec_props) != XMA_APP_SUCCESS) {
    return XMA_APP_ERROR;
  }

  xma_dec_props->width                 = dec_props->width;
  xma_dec_props->height                = dec_props->height;
  xma_dec_props->bits_per_pixel        = dec_props->bit_depth;
  xma_dec_props->framerate.numerator   = dec_props->fps;
  xma_dec_props->framerate.denominator = 1;
  xma_dec_props->hwdecoder_type        = dec_props->codec_type;
  xma_dec_props->handle                = handle;
  xma_dec_props->param_cnt             = 0;
  xma_dec_props->params                = calloc(1, MAX_DEC_PARAMS * sizeof(XmaParameter));

  dec_fill_custom_xma_params(dec_props, xma_dec_props->params, &xma_dec_props->param_cnt);

  /* Xrm related xma_dec_props (plugin_lib, ddr_bank_index, cu_index,
    channel_id, dev_index if necessary) will be set in
    xlnx_dec_create_xrm_dec_ctx */
  return XMA_APP_SUCCESS;
}
