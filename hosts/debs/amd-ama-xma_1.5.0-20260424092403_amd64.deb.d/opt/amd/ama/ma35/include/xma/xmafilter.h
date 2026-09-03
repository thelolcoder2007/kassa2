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
 * enum XmaFilterType - Identifier specifying precise type of video filter
 * during session creation
*/
typedef enum XmaFilterType {
  XMA_DRAWBOX_FILTER_TYPE = 1, /**< 1 */
  XMA_UPLOAD_FILTER_TYPE,      /**< 2 Uploads from host to device */
  XMA_DOWNLOAD_FILTER_TYPE,    /**< 3 Downloads from device to host */
} XmaFilterType;

/**
 * struct XmaFilterPortProperties - Properties necessary for specifying how an
 * input or output port should be configured by the plugin.
*/
typedef struct XmaFilterPortProperties {
  enum XmaFormatType format;         /**< host side video format
                                          entering/leaving port */
  enum XmaFormatType sw_format;      /**< if data is on device, this specifies
                                          the pixel format on device side */
  int32_t            bits_per_pixel; /**< not used, use format and sw_format */
  int32_t            width;          /**< width in pixels of data */
  int32_t            height;         /**< height in pixels of data */
  XmaFraction        framerate;      /**< framerate data structure specifying
                                          frame rate per second */
  int32_t            stride;         /**< stride of video data row */
  int32_t            flags;          /**< input/output flags, must be a
                                          combination of
                                          XMA_FRAME_PROPERTY_FLAG_xxx */
  XmaParameter*      params;         /**< array of custom parameters for port */
  uint32_t           param_cnt;      /**< count of custom parameters for port */
} XmaFilterPortProperties;

/* Number of threads to use. 0(default) means wait until pull is called to do
 * processing. */
#define XMA_FILTER_PARAM_THREADS "threads" // XMA_INT32

/* If the number of threads is >=1 and there is not a frame done processing,
 * wait for the frame to finish processing before returning from pull,
 * otherwise XMA_RESEND_AND_RECV is returned. */
#define XMA_FILTER_PARAM_WAIT "wait" // XMA_INT32

/* DrawBox color as 0xAARRGGBB with ranges for each component from 0 to 255 */
#define XMA_FILTER_DRAWBOX_PARAM_COLOR "color" // XMA_UINT32

/* DrawBox box thickness */
#define XMA_FILTER_DRAWBOX_PARAM_THICKNESS "thickness" // XMA_INT32

/* Latency logging */
#define XMA_FILTER_PARAM_LATENCY_LOGGING "latency_logging" // XMA_INT32
#define XMA_FILTER_LATENCY_LOGGING_DEFAULT (0)

#define XMA_THREADING_FLAG_USE_THREAD (1 << 0)
#define XMA_THREADING_FLAG_WAIT (1 << 1)

/**
 * struct XmaFilterProperties - Properties necessary for specifying which
 * filter kernel to select and how it should be configured by the plugin.
*/
typedef struct XmaFilterProperties {
  /* core filter properties */
  XmaFilterType           hwfilter_type; /**< specifying type of filter to
                                              reserve; @see XmaFilterType */
  XmaHandle               handle;        /**< handle to XMA device */
  XmaFilterPortProperties input;         /**< input properties */
  XmaFilterPortProperties output;        /**< output properties */
  XmaParameter*           params;        /**< array of custom parameters */
  uint32_t                param_cnt;     /**< count of custom parameters */
  int32_t                 reserved[4];
} XmaFilterProperties;

/**
 *  xma_filter_session_create() - This function creates a filter session and
 *  must be called prior to filtering a frame. A session reserves hardware
 *  resources for the duration of a video stream. The number of sessions
 *  allowed depends on a number of factors that include: resolution, frame
 *  rate, bit depth, and the capabilities of the hardware accelerator.
 *
 *  @props: Pointer to a XmaFilterProperties structure that contains the key
 *          configuration properties needed for finding available hardware
 *          resource.
 *
 *  RETURN: Not NULL on success
 *          NULL on failure
 *
 *  Note: session create & destroy are thread safe APIs
*/
XmaSessionHandle xma_filter_session_create(XmaFilterProperties* Props);

/**
 *  xma_filter_session_destroy() - This function destroys a filter session
 *  that was previously created with the @ref xma_filter_session_create().
 *
 *  @session: Pointer to XmaFilterSession created with
 *            xma_filter_session_create()
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on failure.
 *
 *  Note: session create & destroy are thread safe APIs
*/
int32_t xma_filter_session_destroy(XmaSessionHandle session);

/**
 *  xma_filter_session_set_log() - This function changes the logging from the
 *  default (set in xma_initialize) to some other logging.
 *
 *  @session: Pointer to session created by xma_filter_session_create()
 *  @handle:  New log to use
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on error.
*/
int32_t xma_filter_session_set_log(XmaSessionHandle session, XmaLogHandle handle);

/**
 *  xma_filter_session_send_frame() - This function sends frames to the
 *  hardware filter.
 *
 *  @session: Pointer to session created by xma_filter_session_create()
 *  @frame:   Pointer to a frame to be filtered. A NULL frame should be sent to
 *            flush the filter and to indicate that no more data will be sent.
 *
 *  RETURN: XMA_SUCCESS on success and the filter is ready to produce output.
 *          XMA_TRY_AGAIN on internal buffers are full and need to wait before
 *                        sending the same data again.
 *          XMA_ERROR_INVALID on invalid input.
 *          XMA_ERROR on error.
*/
int32_t xma_filter_session_send_frame(XmaSessionHandle session, XmaFrame* frame);

/**
 *  xma_filter_session_recv_frame() - This function obtains output frame with
 *  filtered data from the hardware filter. This function is called after
 *  calling the function xma_filter_session_send_frame.
 *
 *  @session: Pointer to session created by xma_filter_session_create()
 *  @frame:   Pointer to a dummy XmaFrame structure created by
 *            xma_frame_alloc(). All parameters will be filled in by this call.
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_RESEND_AND_RECV indicates no frames have been sent.
 *          XMA_TRY_AGAIN indicates frames have been sent, but they are still
 *                        processing and are not yet ready
 *          XMA_EOS on reaching the end of the stream
 *          XMA_ERROR_INVALID on invalid input.
 *          XMA_ERROR on error.
*/
int32_t xma_filter_session_recv_frame(XmaSessionHandle session, XmaFrame* frame);

#ifdef __cplusplus
}
#endif
