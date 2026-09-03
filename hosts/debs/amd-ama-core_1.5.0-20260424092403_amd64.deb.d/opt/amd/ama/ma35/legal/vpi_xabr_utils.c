/*
 * Copyright (C) 2021, Xilinx Inc - All rights reserved
 * Xilinx Scaler Plugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 * 
 * This is a combined library consisting of work based on the libavutil/rational.c
 * which may be located at https://github.com/FFmpeg/FFmpeg/blob/n5.1.2/libavutil/rational.c
 */

#include <string.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include "vpi_xabr_utils.h"
#include "vpi_utils.h"
#include "vpi_api.h"

#define MA35_SCALE_MAXCAPACITY ((uint64_t)XSCALER_MAX_WIDTH * XSCALER_MAX_HEIGHT * 45 * 2)

static int32_t get_vpi_pix_fmt(const char *format);

/*****************************************************************************
 * map Kernel Pixel format to host sw format
*****************************************************************************/
VpiFmt xsc_utils_get_host_pixel_format(XscPixFormat kernel_fmt)
{
    switch(kernel_fmt) {
        case XV_SC_PIX_FMT_Y_U_V8_420:              return VPI_FORMAT_YUV420P;
        case XV_SC_PIX_FMT_Y_UV8_420:               return VPI_FORMAT_NV12;
        case XV_SC_PIX_FMT_Y_U_V10_420_2B_MSB:      return VPI_FORMAT_YUV420P10BE;
        case XV_SC_PIX_FMT_Y_U_V10_420_2B_LSB:      return VPI_FORMAT_YUV420P10LE;
        case XV_SC_PIX_FMT_Y_UV10_420_2B_MSB:       return VPI_FORMAT_P010LE;
        case XV_SC_PIX_FMT_Y_UV10_420_2B_LSB:       return VPI_FORMAT_P010BE;
        case XV_SC_PIX_FMT_Y_UV10_420_4B_LSB:       return VPI_FORMAT_SP1010;
        case XV_SC_PIX_FMT_RGB8:                    return VPI_FORMAT_RGB24_P;

        //for tile formats return the non-tile version of host formats
        case XV_SC_PIX_FMT_Y_UV8_420_T:             return VPI_FORMAT_NV12;
        case XV_SC_PIX_FMT_Y_UV10_420_2B_MSB_T:     return VPI_FORMAT_P010LE;

        default:
            VPILOGE("Unsupported format...");
            return VPI_FORMAT_NONE;
  }
}

/*****************************************************************************
 * map host sw format to Kernel Pixel format
*****************************************************************************/
XscPixFormat xsc_utils_get_kernel_pixel_format(VpiFmt swformat,
                                               uint32_t    tile_en,
                                               uint32_t    compress_en)
{
    XscPixFormat kfmt;

    switch(swformat) {
        case VPI_FORMAT_YUV420P:         kfmt = XV_SC_PIX_FMT_Y_U_V8_420;           break;
        case VPI_FORMAT_YUV420P10BE:     kfmt = XV_SC_PIX_FMT_Y_U_V10_420_2B_MSB;   break;
        case VPI_FORMAT_YUV420P10LE:     kfmt = XV_SC_PIX_FMT_Y_U_V10_420_2B_LSB;   break;
        case VPI_FORMAT_P010BE:          kfmt = XV_SC_PIX_FMT_Y_UV10_420_2B_LSB;    break;
        case VPI_FORMAT_SP1010:          kfmt = XV_SC_PIX_FMT_Y_UV10_420_4B_LSB;    break;
        case VPI_FORMAT_RGB24_P:         kfmt = XV_SC_PIX_FMT_RGB8;                 break;

        case VPI_FORMAT_NV12:
            kfmt = (tile_en || compress_en) ? XV_SC_PIX_FMT_Y_UV8_420_T : XV_SC_PIX_FMT_Y_UV8_420;
            break;

        case VPI_FORMAT_P010LE:
            kfmt = (tile_en || compress_en) ? XV_SC_PIX_FMT_Y_UV10_420_2B_MSB_T : XV_SC_PIX_FMT_Y_UV10_420_2B_MSB;
            break;

        default:
            VPILOGE("Unsupported format...");
            return -1;
    }
    return (kfmt);
}

/*****************************************************************************
 * return number of planes in given pixel format
*****************************************************************************/
int32_t xsc_utils_get_num_video_planes(XscPixFormat format)
{
    switch (format) {
      case XV_SC_PIX_FMT_Y_U_V8_420:           return 3;     // [15:0] Y:Y 8:8, [7:0] U, [7:0] V
      case XV_SC_PIX_FMT_Y_UV8_420:            return 2;     // [15:0] Y:Y 8:8, [15:0] V:U 8:8
      case XV_SC_PIX_FMT_Y_UV8_420_T:          return 2;     // [15:0] Y:Y 8:8, [15:0] V:U 8:8 4x4 tiles
      case XV_SC_PIX_FMT_Y_U_V10_420_2B_MSB:   return 3;     // [31:0] Y:x:Y:x 10:6:10:6,  [15:0] U:x 10:6, [15:0] V:x 10:6
      case XV_SC_PIX_FMT_Y_U_V10_420_2B_LSB:   return 3;     // [31:0] x:Y:x:Y 6:10:6:10,  [15:0] x:U 6:10, [15:0] x:V 6:10
      case XV_SC_PIX_FMT_Y_UV10_420_2B_MSB:    return 2;     // [31:0] Y:x:Y:x 10:6:10:6,  [31:0] V:x:U:x 10:6:10:6
      case XV_SC_PIX_FMT_Y_UV10_420_2B_LSB:    return 2;     // [31:0] x:Y:x:Y 6:10:6:10,  [31:0] x:V:x:U 6:10:6:10
      case XV_SC_PIX_FMT_Y_UV10_420_2B_MSB_T:  return 2;     // [31:0] Y:x:Y:x 10:6:10:6,  [31:0] V:x:U:x 10:6:10:6 4x4 tiles
      case XV_SC_PIX_FMT_Y_UV10_420_4B_LSB:    return 2;     // [31:0] x:Y:Y:Y 2:10:10:10, [31:0] x:U:V:U 2:10:10:10
      case XV_SC_PIX_FMT_RGB8:                 return 3;     // [7:0] R, [7:0] G, [7:0] B RGB 8-bit planar

      default:
        VPILOGE("Unsupported format...");
        return -1;
    }
}


/*****************************************************************************
 * print session configuration if verbose log level is selected
*****************************************************************************/
void xsc_utils_print_session_config(XscContext *ctx)
{
    uint32_t max_channels = MIN(ctx->cfg.num_outs, XSCALER_MAX_CHANNELS);
    uint32_t chan_id;

    VPILOGI("Session Configuration ->");
    for (chan_id = 0; chan_id < max_channels; chan_id++) {
        VPILOGI("----------- Channel [%d] Params START -----------", chan_id);
        VPILOGI("Input  : width = %4u, height = %4u, fmt = %2d (host_fmt = %2d), "
                "stride_y = %4d, stride_u = %4d, stride_v = %4d",
                ctx->cfg.in_width[chan_id], ctx->cfg.in_height[chan_id],
                ctx->cfg.in_format[chan_id],
                chan_id == 0 ? ctx->props.input.format :
                                ctx->props.output[chan_id - 1].format,
                ctx->cfg.in_stride[chan_id][XV_PLANE_Y],
                ctx->cfg.in_stride[chan_id][XV_PLANE_U],
                ctx->cfg.in_stride[chan_id][XV_PLANE_V]);
        VPILOGI("Output : width = %4u, height = %4u, fmt = %2d (host_fmt = %2d), "
                "stride_y = %4d, stride_u = %4d, stride_v = %4d",
                ctx->cfg.out_width[chan_id], ctx->cfg.out_height[chan_id],
                ctx->cfg.out_format[chan_id],
                ctx->props.output[chan_id].format,
                ctx->cfg.out_stride[chan_id][XV_PLANE_Y],
                ctx->cfg.out_stride[chan_id][XV_PLANE_U],
                ctx->cfg.out_stride[chan_id][XV_PLANE_V]);
        VPILOGI("output vid_std:       %s",((ctx->cfg.out_vidstd[chan_id]==XV_STD_BT_2020) ? "BT_2020" :
                                              ((ctx->cfg.out_vidstd[chan_id]==XV_STD_BT_709)  ? "BT_709"  : "BT_601")));
        VPILOGI("Tile Mode:            %s", (ctx->cfg.tile_en[chan_id]     ? "Enabled" : "Disabled"));
        VPILOGI("Dec400 Compression:   %s", (ctx->cfg.compress_en[chan_id] ? "Enabled" : "Disabled"));
        VPILOGI("----------- Channel [%d] Params END   -----------", chan_id);
    }

    if (ctx->cfg.isCropEn) {
        VPILOGI("Scaler Crop Mode:     Enabled (%d,%d) (%d x %d)",
                ctx->cfg.crop[0].startX, ctx->cfg.crop[0].startY,
                ctx->cfg.crop[0].width,  ctx->cfg.crop[0].height);
    } else {
        VPILOGI("Scaler Crop Mode:     Disabled");
    }
}

int32_t get_bit_depth(int32_t format)
{
  switch (format) {
    case VPI_FORMAT_NV12:
    case VPI_FORMAT_YUV420P:
    case VPI_FORMAT_RGB24_P:
        return 8;
    case VPI_FORMAT_SP1010:
    case VPI_FORMAT_YUV420P10LE:
    case VPI_FORMAT_YUV420P10BE:
    case VPI_FORMAT_P010LE:
    case VPI_FORMAT_P010BE:
        return 10;
    default:
        return -1;
    }
}

/* A function that parses resolution string in format widthxheight
** Returns 0 if string is valid, -1 if invalid
** Also returns the parsed width and height as output parameters */
int32_t parse_resolution(const char* str, int32_t* width, int32_t* height)
{
    int32_t parsed_width = 0;
    int32_t parsed_height = 0;
    int32_t num_digits = 0;
    int32_t separator_found = 0;
    const char *curr = str;

    while (*curr) {
        if (isdigit(*curr)) {
            if (separator_found) {
                parsed_height = parsed_height * 10 + (*curr - '0');
                if(parsed_height > (1<<15)){ // A sensible max!!
                  VPILOGE("Height too large");
                  return -1;
                }
            } else {
                parsed_width = parsed_width * 10 + (*curr - '0');
                if(parsed_width > (1<<15)){
                  VPILOGE("Width too large");
                  return -1;
                }
            }
            num_digits++;
        } else if (*curr == 'x' && num_digits > 0 && !separator_found) {
            separator_found = 1;
            num_digits = 0;
        } else {
            VPILOGE("Invalid resolution format %s", str);
            return -1;
        }
        curr++;
    }

    *width = parsed_width;
    *height = parsed_height;
    return VPI_SUCCESS;
}

/*****************************************************************************
 * remove spaces from token (inline)
*****************************************************************************/
static void remove_spaces(char *src) {
    char *curr = src;

    while(1) {
      if (*curr == ' ') {
          ++curr;
          continue;
      }
      *src = *curr++;
      if(!*src) {
        break;
      }
      ++src;
    }
}

/*****************************************************************************
 * parse out_res string
*****************************************************************************/
int32_t xsc_utils_parse_resize(XscContext *ctx, char *str_resizes)
{
    int32_t i;
    char segs[XSCALER_MAX_CHANNELS][256];
    char *psegs[XSCALER_MAX_CHANNELS];
    char tokens[XSCALER_MAX_CHANNELS][30] = {""};
    int32_t seg_num = 0, token_cnt = 0;
    char *p, *curr_seg, *saveptr;
    int32_t num_outs = 0, segs_with_format = 0;
    for (i = 0; i < XSCALER_MAX_CHANNELS; i++)
      psegs[i] = &segs[i][0];
    VPILOGD("str_resizes: %s", str_resizes);
    if (!str_resizes) {
      return VPI_ERR_PARAM;
    }

    seg_num = split_string(&psegs[0], XSCALER_MAX_CHANNELS, str_resizes, "()");
    if ((seg_num <= 0) || ((uint32_t)seg_num < ctx->props.num_outs)) {
      VPILOGE("ERROR(Xabr):: resize info abnormal!");
      return VPI_ERR_XABR_INVALID_CONFIG;
    }
    while (num_outs < seg_num) {
        curr_seg = segs[num_outs];
        VPILOGD("curr_seg: %s", curr_seg);
        token_cnt = 0;
        p = strtok_r(curr_seg, "|", &saveptr);
        while (p != NULL) {
          strcpy(&tokens[token_cnt++][0], p);
          p = strtok_r(NULL, "|", &saveptr);
        }
        for(i=0;i<token_cnt;i++)
          VPILOGD("seg(%d)  token[%d] = %s", num_outs, i, tokens[i]);

        ctx->props.output[num_outs].vidstd     = XV_STD_BT_709; // default video standard.
        ctx->props.output[num_outs].coeffLoad  = XV_SC_COEFF_AUTO_GENERATE;

        for(i=0;i<token_cnt;i++) {
          remove_spaces(tokens[i]);
          if(i==0){
            //The first token should always be output resolution

                if (parse_resolution(tokens[i],
                                     &ctx->props.output[num_outs].width,
                                     &ctx->props.output[num_outs].height)) {
                  VPILOGE("Parse resolution falied");
                  return VPI_ERR_XABR_INVALID_RESOLUTION;
                }
          }
          else{
            //This token can be output pixel format, video standard or the output rate
            if (!strcmp(tokens[i], "full") || !strcmp(tokens[i], "half")){
              continue;           // Ignore rate data here
            } else if (!strcmp(tokens[i], "bt2020")) {
              ctx->props.output[num_outs].vidstd = XV_STD_BT_2020;
            } else if (!strcmp(tokens[i], "bt709")) {
              ctx->props.output[num_outs].vidstd = XV_STD_BT_709;
            } else if (!strcmp(tokens[i], "bt601")) {
              ctx->props.output[num_outs].vidstd = XV_STD_BT_601;
            } else {
              // This token is most probably pixel format handle with care
              int32_t fmt;
              p = strtok_r(tokens[i], "-", &saveptr);
              fmt = get_vpi_pix_fmt(p);
              if(fmt < 0) {
                VPILOGE("ERROR(Xabr):: unsupported output format (%s)", p);
                return VPI_ERR_XABR_INVALID_FORMAT;
              }
              ctx->props.output[num_outs].format = fmt;
              segs_with_format++;
              p = strtok_r(NULL, "-", &saveptr);
              if(p){
                if(fmt != VPI_FORMAT_NV12 && fmt != VPI_FORMAT_P010LE){
                  VPILOGE("ERROR(Xabr):: -compress and -tile is only supported for nv12 or p010le, provided format = %s", tokens[i]);
                  return VPI_ERR_XABR_INVALID_FORMAT;
                }
              }
              if(p && !strcmp(p, "compress")){
                ctx->props.output[num_outs].compress_en = 1;
                ctx->props.out_compress_en = 1; //flag to indicate at-least 1 channel has compression on
                ctx->props.output[num_outs].tile_en = 1;
              } else if (p && !strcmp(p, "tile")) {
                ctx->props.output[num_outs].tile_en = 1;
                ctx->props.output[num_outs].compress_en = 0;
              } else if (p) { //Something other than "ma" or "tile" after "-"
                VPILOGE("ERROR(Xabr):: Invalid format postfix \"%s\" (only -tile and -ma are supported)", p);
                return VPI_ERR_XABR_INVALID_FORMAT;
              }
            }
          }
        }
        num_outs++;
    }

    // Set the format same as input
    if(segs_with_format == 0){
      VPILOGD("No output format provided setting default based on input depth");
      //Figure out the input bit-depth
      int32_t in_depth = get_bit_depth(ctx->props.input.format);
      if(in_depth < 0){
        VPILOGE("ERROR(Xabr):: Unsupported input format (%d)", ctx->props.input.format);
        return VPI_ERR_XABR_INVALID_FORMAT;
      }
      if(in_depth == 8){ // Default 8-bit output is nv12-tile
        ctx->props.output[0].format = VPI_FORMAT_NV12;
        ctx->props.output[0].compress_en = 0;
        ctx->props.output[0].tile_en = 1;
      } else if(in_depth == 10) { // Default 10-bit output is packed10
        ctx->props.output[0].format = VPI_FORMAT_SP1010;
        ctx->props.output[0].compress_en = 0;
        ctx->props.output[0].tile_en = 0;
      } else {
        VPILOGD("Invalid input depth(%d) only 8 and 10 bit input supported", in_depth);
        return VPI_ERR_XABR_INVALID_FORMAT;
      }
    }else if(segs_with_format != seg_num){
      VPILOGE("ERROR(Xabr):: Either provide output format for none of the output or for all");
      return VPI_ERR_XABR_INVALID_CONFIG;
    }

    if(segs_with_format == 0 ) {
      //Set the output format for all outputs same as for 0th output
      VPILOGD("Setting output format for all same as output 0");
      for(i=1; (uint32_t)i<ctx->props.num_outs; ++i) {
        ctx->props.output[i].format       = ctx->props.output[0].format;
        ctx->props.output[i].tile_en      = ctx->props.output[0].tile_en;
        ctx->props.output[i].compress_en  = ctx->props.output[0].compress_en;
        ctx->props.output[i].vidstd       = ctx->props.output[0].vidstd;
      }
    }

    return VPI_SUCCESS;
}

/*****************************************************************************
 * map string to enumeration
*****************************************************************************/
static int32_t get_vpi_pix_fmt(const char *format)
{
    if (!strcmp(format, "nv12")) {
      return VPI_FORMAT_NV12;
    } else if (!strcmp(format, "yuv420p")) {
      return VPI_FORMAT_YUV420P;
    } else if (!strcmp(format, "p010be")) {
      return VPI_FORMAT_P010BE;
    } else if (!strcmp(format, "p010le")) {
      return VPI_FORMAT_P010LE;
    } else if (!strcmp(format, "yuv420p10le")) {
      return VPI_FORMAT_YUV420P10LE;
    } else if (!strcmp(format, "yuv420p10be")) {
      return VPI_FORMAT_YUV420P10BE;
    } else if (!strcmp(format, "rgbp")) {
      return VPI_FORMAT_RGB24_P;
    } else if (!strcmp(format, "sp101010") || !strcmp(format, "packed10")) {
      return VPI_FORMAT_SP1010;
    } else {
      return -1;
    }
}

/*****************************************************************************
 * validate formats and implicitly assign to ctx properties
*****************************************************************************/
int32_t xsc_utils_parse_input_format(XscContext *ctx,
                                  const char *input_format,
                                  const char *input_sw_format)
{
    int32_t ret;

    if (input_format) {
      if (!strcmp(input_format, "vpe")) {
          ret = get_vpi_pix_fmt(input_sw_format);
          if(ret < 0) {
            VPILOGE("unsupported input sw format (%s)", input_sw_format);
            return ret;
          }
          ctx->props.input.format = ret;
      } else {
        VPILOGE("unsupported input format (%s)", input_format);
        return VPI_ERR_XABR_INVALID_FORMAT;
      }

      return VPI_SUCCESS;
    } else {
        VPILOGE("input format in NULL");
        return VPI_ERR_PARAM;
    }
}

/*****************************************************************************
 * parse crop string "crop_str"
*****************************************************************************/
int32_t xsc_utils_parse_inp_crop(XscContext *ctx, char *str_crop)
{
  int32_t cnt=0;
  char crop_data[4][20];

  if(str_crop == NULL){
    VPILOGD("No crop string crop is disabled");
    ctx->props.crop[0].width = 0;
    ctx->props.crop[0].height = 0;
    ctx->props.crop[0].startX = 0;
    ctx->props.crop[0].startY = 0;
    return VPI_SUCCESS;
  }

  char *tmp_str = (char*)malloc(strlen(str_crop) + 1);
  if(tmp_str == NULL){
    VPILOGE("Unable to allocate memory for tmp string %s\n", str_crop);
      return VPI_ERR_NO_AP_MEM;
  }
  strcpy(tmp_str, str_crop);
  char *saveptr;
  char *p = strtok_r(tmp_str, "|", &saveptr);
  while(p != NULL){
      if(cnt >= 4){
          VPILOGE("crop string is invalid (%s)", str_crop);
          free(tmp_str);
          return VPI_ERR_XABR_INVALID_CONFIG;
      }
      else{
          strcpy(&crop_data[cnt++][0], p);
          p = strtok_r(NULL, "|", &saveptr);
      }
  }
  free(tmp_str);

  if(cnt < 4){
    VPILOGE("4 components w|h|x|y must be specified in crop string found only %d in %s", cnt, str_crop);
    return VPI_ERR_XABR_INVALID_CONFIG;
  }

  ctx->props.crop[0].width  = (uint16_t)strtoul(crop_data[0], 0, 10);
  if(ctx->props.crop[0].width == 0){
    VPILOGE("Zero crop-width or converting(%s) to integer failed", crop_data[0]);
    return VPI_ERR_XABR_INVALID_CONFIG;
  }
  ctx->props.crop[0].height = (uint16_t)strtoul(crop_data[1], 0, 10);
  if(ctx->props.crop[0].height == 0){
    VPILOGE("Zero crop-height or converting(%s) to integer failed", crop_data[1]);
    return VPI_ERR_XABR_INVALID_CONFIG;
  }

  //strtoul returns 0 on failure
  //"0" is a valid value for startX or Y
  //How to distinguish between error return vs user specifying 0 ??
  ctx->props.crop[0].startX = (uint16_t)strtoul(crop_data[2], 0, 10);
  ctx->props.crop[0].startY = (uint16_t)strtoul(crop_data[3], 0, 10);

  VPILOGD("parsed crop info W=%d H=%d X=%d Y=%d",
          ctx->props.crop[0].width, ctx->props.crop[0].height,
          ctx->props.crop[0].startX, ctx->props.crop[0].startY);
  return VPI_SUCCESS;
}

/*
* A simple queue data structure
*/

XscQueueHandle xsc_queue_create(const char* name, int max_items)
{
    XscQueue *xq = (XscQueue*)malloc(sizeof(XscQueue));
    if(!xq){
        VPILOGE("[%s] XscQueue memory allocation failed", name);
        return NULL;
    }
    
    if(max_items < 0){
      VPILOGE("[%s] Invalid max_items specified for queue (%d)", name, max_items);
      return NULL;
    }
    xq->m_front = -1;
    xq->m_rear = -1;
    xq->m_capacity = max_items;

    xq->m_items = (void**)malloc(max_items*sizeof(void*));
    if(!xq->m_items){
      VPILOGE("[%s] memory allocation for m_items failed(max_items = %d)", name, max_items);
      return NULL;
    }

    if(name){
      strncpy(xq->m_name, name, 255);
    } else{
      snprintf(xq->m_name, 255, "queue@%p", xq);
    }
    VPILOGD("[%s] queue created with capacity %d", xq->m_name, xq->m_capacity);
    return xq;
}
//Not thread safe
static bool xsc_queue_isempty(const XscQueue *xq)
{
    return xq->m_front==-1;
}

static bool xsc_queue_isfull(const XscQueue *xq)
{
    return (xq->m_rear + 1)%xq->m_capacity == xq->m_front;
}

int32_t xsc_queue_enqueue(XscQueueHandle xscq, void *data)
{
    if(!xscq){
        VPILOGE("Empty queue handle");
        return VPI_ERR_PARAM;
    }
    XscQueue *xq = (XscQueue*)xscq;

    if (xsc_queue_isfull(xq)) {
        VPILOGE("[%s] Queue is full. Cannot enqueue element", xq->m_name);
        return VPI_ERR_SW;
    } else {
        if (xsc_queue_isempty(xq)) {
            xq->m_front = 0;
            xq->m_rear = 0;
        }
        else{
            xq->m_rear = (xq->m_rear+1)%xq->m_capacity;
        }
        xq->m_items[xq->m_rear] = data;
        VPILOGD("[%s] %p enqueued to the queue", xq->m_name, data);
    }

    return VPI_SUCCESS;
}

void* xsc_queue_dequeue(XscQueueHandle xscq)
{   
    void *ret;
    XscQueue *xq;
    if(!xscq){
        VPILOGE("Empty queue handle");
        return NULL;
    }
    xq = (XscQueue*)xscq;
    if (xsc_queue_isempty(xq)) {
        VPILOGE("[%s] Queue is empty. Cannot dequeue element.", xq->m_name);
        return NULL;
    } else {
        ret = xq->m_items[xq->m_front];
        if(xq->m_front == xq->m_rear){
            xq->m_front = -1;
            xq->m_rear = -1;
        }
        else{
          xq->m_front = (xq->m_front+1)%xq->m_capacity;
        }
        
        VPILOGD("[%s] %p dequeued to the queue", xq->m_name, ret);
        return ret;
    }
}

void xsc_queue_destroy(XscQueueHandle* xscq)
{
    if(!*xscq){
        VPILOGE("Empty queue handle");
        return;
    }
    XscQueue **xq = (XscQueue**)xscq;

    VPILOGD("[%s] queue destroy called", (*xq)->m_name);

    if((*xq)->m_items){
      free((*xq)->m_items);
    }
    free(*xq);
    *xq = NULL;
}

void xsc_utils_update_sar(int32_t input_width, int32_t input_height,
                         int32_t output_width, int32_t output_height,
                         int32_t in_sar_width, int32_t in_sar_height,
                         int32_t* out_sar_width, int32_t* out_sar_height)
{
    int64_t tmp_w, tmp_h;
    int32_t ret;

    int min_dim = MIN(XSCALER_MIN_HEIGHT, XSCALER_MIN_WIDTH);
    int max_dim = MAX(XSCALER_MAX_HEIGHT, XSCALER_MAX_WIDTH);

    if(input_width < min_dim || input_height < min_dim || output_width < min_dim || output_height < min_dim ||
        input_width > max_dim || input_height > max_dim || output_width > max_dim || output_height > max_dim){
          VPILOGW("Invalid input ot output resolution in[w=%d, h=%d] out[w=%d, h=%d] setting sar to 0/0\n",
                    input_width, input_height, output_width, output_height);
          out_sar_width = out_sar_height = 0;
          return;
    }

    // Adjust the input dimensions according to the input SAR
    tmp_w = (int64_t)input_width*in_sar_width;
    tmp_h = (int64_t)input_height*in_sar_height;

    // Calculate the SAR as the ratio of input to output dimensions
    tmp_w *= output_height;
    tmp_h *= output_width;

    // Simplify the SAR to its lowest terms
    ret = vpi_rational_reduce(tmp_w, tmp_h, out_sar_width, out_sar_height);
    if(ret){
      VPILOGW("SAR Value approximated to allow sar_width and sar_height to be in range [0,%d]", MAX_SAR_VALUE);
    }

    VPILOGD("SAR UPDATE in_w=%d in_h=%d out_w=%d out_h=%d in_sar_w=%d in_sar_h=%d out_sar_w=%d out_sar_h=%d",
     input_width,  input_height, output_width, output_height, in_sar_width, in_sar_height,
     *out_sar_width, *out_sar_height);
}

int32_t xsc_utils_prepare_out_vpifrm(VpiFrm *out, VpiFrm *in, XscContext *ctx)
{
  VpiSidedataHandle in_sb_handle, out_sb_handle;
  int32_t ret;

  out->poc   = in->poc;
  out->pts   = in->pts;
  out->valid = 1;
  out->log_frm_num = in->log_frm_num;
  ret = vpifrm_sidedata_copy_reference(in, out);
  if(ret)
      return ret;

  //Warning: HDR10plus and ML sidedatas are passed through without modification, which may cause inconsistencies if the output resolution differs
  if(ctx->sidedata_warning_flag && (in->width != out->width || in->height != out->height)) {
      VpiSidedataHandle sd_handle;
      int32_t sidedata_ret = vpifrm_get_first_sidedata_by_type(out, VPI_SB_SEI_HDR10_PLUS, &sd_handle);
      if(sidedata_ret == VPI_SUCCESS) {
        VPILOGW("The scaler currently passes through HDR10+ metadata unmodified, so the metadata might not describe the scaled video accurately. If this is a problem, consider using the encoder option '-encode_hdr10plus_metadata disable' to omit this metadata from the output bitstream");
      }
      sidedata_ret = vpifrm_get_first_sidedata_by_type(out, VPI_SB_ML_ROI, &sd_handle);
      if(sidedata_ret == VPI_SUCCESS) {
        VPILOGW("The scaler currently passes through the ML_ROI metadata unmodified, which means the metadata may not accurately reflect the scaled ROI. At present, there are no known use cases requiring the scaler to modify ROI related metadata. If such a requirement arises for any use case, please reach out to the AMD support team");
      }
      sidedata_ret = vpifrm_get_first_sidedata_by_type(out, VPI_SB_DQPMAP_HEVC_H264, &sd_handle);
      if(sidedata_ret == VPI_SUCCESS) {
        VPILOGW("The scaler currently passes through the DQPMAP metadata unmodified, which means the metadata may not accurately reflect the scaled ROI. At present, there are no known use cases requiring the scaler to modify ROI related metadata. If such a requirement arises for any use case, please reach out to the AMD support team");
      }
      ctx->sidedata_warning_flag = false;
  }

  /*Update SAR values inside VPI_SB_VUI_PARAMS sidedata
  */
  vpifrm_get_first_sidedata_by_type(in, VPI_SB_VUI_PARAMS, &in_sb_handle);
  vpifrm_get_first_sidedata_by_type(out, VPI_SB_VUI_PARAMS, &out_sb_handle);
  if(in_sb_handle && out_sb_handle){
    VpiVuiParameters *in_vui = vpi_get_sidedata_vaddr(in_sb_handle);

      /*If input sar values are not vaild scaler makes no change
      * As per H264 spec sar values are allocated 16-bits.
      */
      if(in_vui->sar_width > 0 && in_vui->sar_height > 0 &&
         in_vui->sar_width <= MAX_SAR_VALUE && in_vui->sar_height <= MAX_SAR_VALUE){
        int32_t new_sar_w, new_sar_h;
        xsc_utils_update_sar(in->width, in->height, out->width, out->height,
            in_vui->sar_width, in_vui->sar_height, &new_sar_w, &new_sar_h);
        //Only update sidedata if sar values change
        if(new_sar_w != in_vui->sar_width || new_sar_h != in_vui->sar_height){
          VpiSidedataHandle new_sd;
          VpiVuiParameters *new_vui;
          VpiVuiParameters *old_vui = vpi_get_sidedata_vaddr(out_sb_handle);
          ret = vpi_sidedata_alloc(ctx->sd_pool_mgr, VPI_SB_VUI_PARAMS, sizeof(VpiVuiParameters), RC_SIDE_EN, &new_sd);
          if(ret){
            VPILOGE("Failed to allocate new VPI_SB_VUI_PARAMS sidedata");
            return ret;
          }
          new_vui = vpi_get_sidedata_vaddr(new_sd);
          memcpy(new_vui, old_vui, sizeof(VpiVuiParameters));
          new_vui->sar_width = new_sar_w;
          new_vui->sar_height = new_sar_h;

          ret = vpifrm_remove_sidedata(out, out_sb_handle);
          if(ret){
              VPILOGE("Failed to remove old VPI_SB_VUI_PARAMS sidedata");
              return ret;
          }
          ret = vpifrm_attach_sidedata(out, new_sd);
          if(ret){
              VPILOGE("Failed to add new VPI_SB_VUI_PARAMS sidedata");
              return ret;
          }
        }
      }
  }
  return ret;
}

bool xsc_is_scaler_load_above_half_capacity(SessionProps props, VpiRational* ouput_frame_rate) {
  uint64_t sess_pixrate = 0;
  int32_t  in_width     = props.input.width;
  int32_t  in_height    = props.input.height;
  VPILOGD(" in_width  = %d", props.input.width);
  VPILOGD(" in_height = %d", props.input.height);
  for (size_t i = 0; i < props.num_outs; i++) {
    if(ouput_frame_rate[i].num<=0 || ouput_frame_rate[i].den<=0) {
      return false;
    }
    uint32_t rounding     = ouput_frame_rate[i].den >> 1;
    uint32_t frame_rate   = (ouput_frame_rate[i].num + rounding) / ouput_frame_rate[i].den;
    sess_pixrate += MAX(props.output[i].width, in_width) * MAX(props.output[i].height, in_height) * frame_rate;
    VPILOGD(" out_width[%zu]       = %d", i, props.output[i].width);
    VPILOGD(" out_height[%zu]      = %d", i, props.output[i].height);
    VPILOGD(" out_framerate[%zu]   = %d", i, frame_rate);
    in_width  = props.output[i].width;
    in_height = props.output[i].height;
  }
  VPILOGD("Requested scaler load (%lu)", (sess_pixrate*1000000)/MA35_SCALE_MAXCAPACITY);
  if (sess_pixrate > (MA35_SCALE_MAXCAPACITY / 2)) {
    VPILOGD("Enable multi_accel as scaler load exceeds half of the total 8kp90 capacity");
    return true;
  }
  return false;
}
