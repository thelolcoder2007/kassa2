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

#pragma once

#include "xmabuffers.h"
#include "xmaparam.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * enum XmaMioFilterType - Identifier specifying precise type of video filter
 * during session creation
*/
typedef enum XmaMioFilterType {
  XMA_OVERLAY_FILTER_TYPE = 1, /**< 1 */
  XMA_TILE_FILTER_TYPE,        /**< 2 */
  XMA_ML_FILTER_TYPE,          /**< 3 */
  XMA_BLEND_FILTER_TYPE,       /**< 4 */
  XMA_COMPOSITOR_FILTER_TYPE   /**< 5 */
} XmaMioFilterType;

/**
 * struct XmaMioFilterPortProperties - Properties necessary for specifying how
 * an input or output port should be configured by the plugin.
*/
typedef struct XmaMioFilterPortProperties {
  enum XmaFormatType format;    /**< host side video format entering/leaving
                                     port */
  enum XmaFormatType sw_format; /**< if data is on device, this specifies the
                                     pixel format on device side */
  int32_t            width;     /**< width in pixels of data */
  int32_t            height;    /**< height in pixels of data */
  XmaFraction        framerate; /** framerate data structure specifying frame
                                    rate per second */
  int32_t            stride;    /**< stride of video data row */
  int32_t            flags;     /**< input/output flags, must be a combination
                                     of XMA_FRAME_PROPERTY_FLAG_xxx */
  XmaParameter*      params;    /**< array of custom parameters for port */
  uint32_t           param_cnt; /**< count of custom parameters for port */
} XmaMioFilterPortProperties;

/* Forward declaration */
typedef struct XmaMioFilterSession XmaMioFilterSession;

/**
 * struct XmaMioFilterProperties - Properties necessary for specifying which
 * filter kernel to select and how it should be configured by the plugin.
*/
typedef struct XmaMioFilterProperties {
  /* core filter properties */
  XmaMioFilterType           hwfilter_type; /**< Specifying type of filter to
                                                 reserve; @see
                                                 XmaMioFilterType */
  XmaHandle                  handle;        /**< handle to XMA device */
  int                        num_inputs;    /**< number of input streams */
  XmaMioFilterPortProperties inputs[MAX_MIO_FILTER_INPUTS];
  int                        num_outputs; /**< number of output streams */
  XmaMioFilterPortProperties outputs[MAX_MIO_FILTER_OUTPUTS];
  XmaParameter*              params;    /** array of custom parameters for
                                                port */
  uint32_t                   param_cnt; /** count of custom parameters for
                                                port */
  int32_t                    reserved[4];
} XmaMioFilterProperties;

/* Number of threads to use. 0(default) means wait until pull is called to do
 * processing. */
#define XMA_MIOFILTER_PARAM_THREADS "threads" // XMA_INT32

/* If the number of threads is >=1 and there is not a frame done processing,
 * wait for the frame to finish processing before returning from pull,
 * otherwise XMA_RESEND_AND_RECV is returned. */
#define XMA_MIOFILTER_PARAM_WAIT "wait" // XMA_INT32

/* ML core id */
#define XMA_MIOFILTER_ML_CORE_ID "core_id" // XMA_INT32

/* ML model name */
#define XMA_MIOFILTER_ML_MODEL "model" // XMA_STRING
//Below mentioned macro is only for backward compatibility to 1.1.2, not to be used on latest
#define XMA_MIOFILTER_ML_MODEL_NAME "model" // XMA_STRING

/* ML model args */
#define XMA_MIOFILTER_ML_MODEL_ARGS "model_args" // XMA_STRING

/* ML Performance */
#define XMA_MIOFILTER_ML_PERFORMANCE "perf" // XMA_STRING

/* ML config tensor dump */
#define XMA_MIOFILTER_ML_TENSOR_DUMP "tensor_dump" // XMA_STRING

/* ML config inference repeat */
#define XMA_MIOFILTER_ML_INF_REPEAT "inference_loop_repeat_count" // XMA_STRING

/* ML config inference period */
#define XMA_MIOFILTER_ML_INF_PERIOD "inference_period" // XMA_INT32

/* Overlay x_position */
#define XMA_MIOFILTER_OVERLAY_PARAM_X "x" // XMA_INT32

/* Overlay y_position */
#define XMA_MIOFILTER_OVERLAY_PARAM_Y "y" // XMA_INT32

/* Horizontal grid size */
#define XMA_MIOFILTER_TILE_PARAM_X "x" // XMA_INT32

/* Vertical grid size */
#define XMA_MIOFILTER_TILE_PARAM_Y "y" // XMA_INT32

/* Latency logging */
#define XMA_MIO_FILTER_PARAM_LATENCY_LOGGING "latency_logging" // XMA_INT32
#define XMA_MIO_FILTER_LATENCY_LOGGING_DEFAULT (0)

/**
 *  xma_mio_filter_session_create() - This function creates a multiple
 *  input/output filter session and must be called prior to filtering frames.
 *  A session reserves hardware resources for the duration of a video stream.
 *  The number of sessions allowed depends on a number of factors that include:
 *  resolution, frame rate, bit depth, number of input/output streams and the
 *  capabilities of the hardware accelerator.
 *
 *  @props: Pointer to a XmaMioFilterProperties structure that contains the key
 *          configuration properties needed for finding available hardware
 *          resource.
 *
 *  RETURN: Not NULL on success
 *          NULL on failure
 *
 *  Note: session create & destroy are thread safe APIs
*/
XmaMioFilterSession* xma_mio_filter_session_create(XmaMioFilterProperties* props);

/**
 *  xma_mio_filter_session_destroy() - This function destroys a filter session
 *  that was previously created with @ref xma_mio_filter_session_create().
 *
 *  @session: Pointer to XmaMioFilterSession created with
 *            xma_mio_filter_session_create()
 *
 *  RETURN: XMA_SUCCESS on success
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on failure.
 *
 *  Note: session create & destroy are thread safe APIs
*/
int32_t xma_mio_filter_session_destroy(XmaMioFilterSession* session);

/**
 *  xma_mio_filter_session_set_log() - This function changes the logging from
 *  the default (set in xma_initialize) to some other logging.
 *
 *  @session: Pointer to session created by xma_mio_filter_session_create()
 *  @handle:  New log to use
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on error.
*/

int32_t xma_mio_filter_session_set_log(XmaMioFilterSession* session, XmaLogHandle handle);

/**
 *  xma_mio_filter_session_send_frame_list() - This function sends frames to
 *  the hardware filter.
 *
 *  @session:    Pointer to session created by xma_mio_filter_session_create()
 *  @frame_list: Pointer to an array of frames to be filtered. A NULL array
 *               should be sent to flush the filter and to indicate that no
 *               more data will be sent.
 *
 *  RETURN: XMA_SUCCESS on success and the filter is ready to produce output
 *          XMA_TRY_AGAIN on internal buffers are full and need to wait before
 *                        sending the same data again.
 *          XMA_ERROR_INVALID on invalid input.
 *          XMA_ERROR on error.
*/
int32_t xma_mio_filter_session_send_frame_list(XmaMioFilterSession* session, XmaFrame** frame_list);

/**
 *  xma_mio_filter_session_recv_frame_list() - This function obtains output
 *  frames with filtered data from the hardware filter. This function is called
 *  after calling the function xma_mio_filter_session_send_frames.
 *
 *  @session:    Pointer to session created by xma_mio_filter_session_create()
 *  @frame_list: An array of dummy XmaFrame pointers created by
 *               xma_frame_alloc(). All parameters of the XmaFrames will be
 *               filled in by this call. 
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_RESEND_AND_RECV indicates no frames have been sent.
 *          XMA_TRY_AGAIN indicates frames have been sent, but they are still
 *                        processing and are not yet ready
 *          XMA_EOS on reaching the end of the stream
 *          XMA_ERROR_INVALID on invalid input.
 *          XMA_ERROR on error.
*/
int32_t xma_mio_filter_session_recv_frame_list(XmaMioFilterSession* session, XmaFrame** frame_list);

#ifdef __cplusplus
}
#endif
