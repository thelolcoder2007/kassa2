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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * enum XmaScalerType - Identifier specifying precise type of scaler during
 * session creation
*/
typedef enum XmaScalerType {
  XMA_ABR_SCALER_TYPE = 1, /**< 1 */
} XmaScalerType;

/**
 * struct XmaScalerInOutProperties - Properties necessary for specifying how an
 * input or output port should be configured by the plugin.
*/
typedef struct XmaScalerInOutProperties {
  enum XmaFormatType format;          /**< host side video format
                                           entering/leaving port */
  enum XmaFormatType sw_format;       /**< if data is on device, this specifies
                                           the pixel format on device side */
  int32_t            bits_per_pixel;  /**< not used, use format and sw_format */
  int32_t            width;           /**< width in pixels of data */
  int32_t            height;          /**< height in pixels of data */
  XmaFraction        framerate;       /**< framerate data structure specifying frame
                                           rate per second */
  int32_t            stride;          /**< stride of primary plane */
  int32_t            flags;           /**< input/output flags, must be a combination
                                           of XMA_FRAME_PROPERTY_FLAG_xxx */
  int32_t            filter_idx;      /**< not used */
  int32_t            coeffLoad;       /**< not used */
  char               coeffFile[1024]; /**< not used */
} XmaScalerInOutProperties;

/**
 * struct XmaScalerFilterProperties - not used
 */
typedef struct XmaScalerFilterProperties {
  int16_t h_coeff0[64][12]; /**< horizontal coefficients 1 */
  int16_t h_coeff1[64][12]; /**< horizontal coefficients 2 */
  int16_t h_coeff2[64][12]; /**< horizontal coefficients 3 */
  int16_t h_coeff3[64][12]; /**< horizontal coefficients 4 */
  int16_t v_coeff0[64][12]; /**< vertical coefficients 1 */
  int16_t v_coeff1[64][12]; /**< vertical coefficients 2 */
  int16_t v_coeff2[64][12]; /**< vertical coefficients 3 */
  int16_t v_coeff3[64][12]; /**< vertical coefficients 4 */
} XmaScalerFilterProperties;

/**
 * struct XmaScalerProperties - Properties necessary for specifying which
 * scaler kernel to select and how it should be configured by the plugin.
*/
typedef struct XmaScalerProperties {
  XmaScalerType             hwscaler_type;              /**< specifying type of
                                                             scaler to reserve;
                                                             @see XmaScalerType */
  XmaHandle                 handle;                     /**< handle to XMA
                                                             device */
  int32_t                   num_outputs;                /**< number of scaled
                                                             outputs */
  XmaScalerFilterProperties filter_coefficients;        /**< not used */
  XmaScalerInOutProperties  input;                      /**< input properties */
  XmaScalerInOutProperties  output[MAX_SCALER_OUTPUTS]; /**< output properties
                                                             array */
  XmaParameter*             params;                     /**< array of custom
                                                             parameters for
                                                             port */
  uint32_t                  param_cnt;                  /**< count of custom
                                                             parameters for
                                                             port */
  int32_t                   reserved[4];
} XmaScalerProperties;

/* Number of threads to use. 0 means wait until pull is called to do
 * processing. Default = 1.*/
#define XMA_SCALER_PARAM_THREADS "threads" // XMA_INT32

/* If the number of threads is >=1 and there is not a frame done processing,
 * wait for the frame to finish processing before returning from pull,
 * otherwise XMA_RESEND_AND_RECV is returned. */
#define XMA_SCALER_PARAM_WAIT "wait" // XMA_INT32

/* Mix rate session */
#define XMA_SCALER_PARAM_MIX_RATE "mix_rate" // XMA_FUNC_PTR

/* Enable dynamic cropping */
#define XMA_SCALER_PARAM_DYNAMIC_CROPPING "dynamic_cropping" // XMA_INT32

/* left cropping */
#define XMA_SCALER_PARAM_LEFT_CROPPING "left" // XMA_INT32

/* top cropping */
#define XMA_SCALER_PARAM_TOP_CROPPING "top" // XMA_INT32

/* width cropping */
#define XMA_SCALER_PARAM_WIDTH_CROPPING "width" // XMA_INT32

/* height cropping */
#define XMA_SCALER_PARAM_HEIGHT_CROPPING "height" // XMA_INT32

/* Latency logging */
#define XMA_SCALER_PARAM_LATENCY_LOGGING "latency_logging" // XMA_INT32
#define XMA_SCALER_LATENCY_LOGGING_DEFAULT (0)

/**
 *  xma_scaler_default_filter_coeff_set() - not used
 *
*/
void xma_scaler_default_filter_coeff_set(XmaScalerFilterProperties* props);

/**
 *  xma_scaler_session_create() - This function creates a scaler session and
 *  must be called prior to scaling a frame. A session reserves hardware
 *  resources for the duration of a video stream. The number of sessions
 *  allowed depends on a number of factors that include: resolution, frame
 *  rate, bit depth, and the capabilities of the hardware accelerator.
 *
 *  @props Pointer to a XmaScalerProperties structure that contains the key
 *         configuration properties needed for finding available hardware
 *         resource.
 *
 *  RETURN: Not NULL on success
 *          NULL on failure
 *
 *  Note: session create & destroy are thread safe APIs
*/
XmaSessionHandle xma_scaler_session_create(XmaScalerProperties* props);

/**
 *  xma_scaler_session_destroy() - This function destroys a scaler session
 *  that was previously created with the @ref xma_scaler_session_create().
 *
 *  @session: XmaSessionHandle created with
 *            xma_scaler_session_create()
 *
 *  RETURN: XMA_SUCCESS on success
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on failure.
 *
 *  Note: session create & destroy are thread safe APIs
*/
int32_t xma_scaler_session_destroy(XmaSessionHandle session);

/**
 *  xma_scaler_session_set_log() - This function changes the logging from the
 *  default (set in xma_initialize) to some other logging.
 *
 *  @session: Pointer to session created by xma_scaler_session_create()
 *  @handle:  New log to use
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on error.
*/

int32_t xma_scaler_session_set_log(XmaSessionHandle session, XmaLogHandle handle);

/**
 *  xma_scaler_session_send_frame() - This function sends frames to the
 *  hardware scaler.
 *
 *  @session: Pointer to session created by xma_scaler_sesssion_create()
 *  @frame:   Pointer to a frame to be scaled. A NULL frame should be sent to
 *            flush the scaler and to indicate that no more data will be sent.
 *
 *  RETURN: XMA_SUCCESS on success and the scaler is ready to produce output
 *          XMA_TRY_AGAIN on internal buffers are full and need to wait before
 *                        sending the same data again.
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on error.
*/
int32_t xma_scaler_session_send_frame(XmaSessionHandle session, XmaFrame* frame);

/**
 *  xma_scaler_session_recv_frame_list() -This function obtains output frames
 *  with scaled data from the hardware scaler. This function is called after
 *  calling the function xma_scaler_session_send_frame.
 *
 *  @session:    Pointer to session created by xma_scaler_sesssion_create()
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
int32_t xma_scaler_session_recv_frame_list(XmaSessionHandle session, XmaFrame** frame_list);

#ifdef __cplusplus
}
#endif
