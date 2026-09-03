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
 * enum XmaDecoderType - Identifier specifying precise type of video decoder
 * during session creation
 */
typedef enum XmaDecoderType {
  XMA_H264_DECODER_TYPE = 1, /**< 1 */
  XMA_HEVC_DECODER_TYPE,     /**< 2 */
  XMA_VP9_DECODER_TYPE,      /**< 3 */
  XMA_JPEG_DECODER_TYPE,     /**< 4 */
  XMA_AV1_DECODER_TYPE,      /**< 5 */
} XmaDecoderType;

/**
 * struct XmaDecoderProperties - Properties necessary for specifying which
 * decoder to select and how it should be configured by the plugin.
 */
typedef struct XmaDecoderProperties {
  XmaDecoderType hwdecoder_type; /**< Specifying type of decoder to reserve;
                                      @see XmaDecoderType */
  XmaHandle      handle;         /**< handle to XMA device */
  int32_t        intraOnly;      /**< not used */
  int32_t        bits_per_pixel; /**< bit depth of Y component */
  int32_t        width;          /**< used to determine amount of memory to
                                      allocate */
  int32_t        height;         /**< used to determine amount of memory to
                                      allocate */
  XmaFraction    framerate;      /**< not used */
  int32_t        chroma_width;   /**< not used */
  int32_t        chroma_height;  /**< not used */
  int32_t        num_of_UV;      /**< not used */
  XmaParameter*  params;         /**< array of custom parameters */
  uint32_t       param_cnt;      /**< count of custom parameters */
  uint32_t       flags;          /**< output flags, must be a combination
                                       of XMA_FRAME_PROPERTY_FLAG_xxx */
  int32_t        reserved[3];
} XmaDecoderProperties;

/* Use a background thread to improve processing on host. Default is 0, added
 * in API v1.1. */
#define XMA_DEC_PARAM_THREADS "threads" // XMA_INT32

/* If XMA_DEC_PARAM_THREADS is 1, xma_dec_session_send_data() waits for
 * internal buffers to have room if necessary before sending data to the device
 * and returning instead of returning XMA_TRY_AGAIN.
 * xma_dec_session_recv_frame() will wait for a decoded frame or end of stream
 * before returning. Default is 0, added in API v1.1. */
#define XMA_DEC_PARAM_WAIT "wait" // XMA_INT32

/* Output format */
#define XMA_DEC_OUTPUT_FORMAT "out_fmt" // XMA_INT32

/* Low latency */
#define XMA_DEC_PARAM_LOW_LATENCY "low_latency" // XMA_INT32
#define XMA_DEC_LOW_LATENCY_DEFAULT (0)

/* Maximum resolution added in v1.2 */
#define XMA_DEC_PARAM_MAX_WIDTH "max_width"   // XMA_UINT32
#define XMA_DEC_PARAM_MAX_HEIGHT "max_height" // XMA_UINT32

/* Resize resolution added in v1.2 */
#define XMA_DEC_PARAM_RESIZE_WIDTH "resize_width"   // XMA_UINT32
#define XMA_DEC_PARAM_RESIZE_HEIGHT "resize_height" // XMA_UINT32

/* Enable multi-core */
#define XMA_DEC_ENABLE_MC "enable_mc" // XMA_INT32

/* Latency logging */
#define XMA_DEC_PARAM_LATENCY_LOGGING "latency_logging" // XMA_INT32
#define XMA_DEC_LATENCY_LOGGING_DEFAULT (0)

/* If XMA_DEC_PARAM_RES_CHANGE_CALLBACK is set and the source changes it's
 * properties and the decoder is able to continue, the provided function will
 * be called to notify the application of the new properties. The return value
 * from the function should be either XMA_SUCCESS (indicating to continue
 * decoding), XMA_EOS (indicating to terminate successfully), or any error
 * code. If the callback function is not provided, or the decoder can not
 * continue, XMA_ERROR_INVALID will be returned by
 * xma_dec_session_recv_frame(). The value returned by the callback function
 * will be returned by xma_dec_session_recv_frame().
 * XMA_DEC_PARAM_PROP_CHANGE_PARAM can provide a custom parameter to be sent to
 * the callback function. */
typedef int32_t (*DecPropChangeCallbackFunction)(XmaFrameProperties* new_props, void* opaque);
#define XMA_DEC_PARAM_PROP_CHANGE_CALLBACK "prop_change_callback" // XMA_FUNC_PTR, added in API v1.1
#define XMA_DEC_PARAM_PROP_CHANGE_PARAM "prop_change_param"       // XMA_FUNC_PTR, added in API v1.1

/* Forward declaration */
typedef struct XmaDecoderSession XmaDecoderSession;

/**
 *  xma_dec_session_create() - This function creates a decoder session and
 *  must be called prior to decoding a frame. A session reserves hardware
 *  resources for the duration of a video stream. The number of sessions
 *  allowed depends on a number of factors that include: resolution, frame
 *  rate, bit depth, and the capabilities of the hardware accelerator.
 *
 *  @dec_props: Pointer to a XmaDecoderProperties structure that contains the
 *              key configuration properties needed for finding available
 *              hardware resource.
 *
 *  RETURN: Not NULL on success
 *          NULL on failure
 *
 *  Note: session create & destroy are thread safe APIs
*/
XmaSessionHandle xma_dec_session_create(XmaDecoderProperties* dec_props);

/**
 *  xma_dec_session_destroy() - This function destroys a decoder session
 *  that was previously created with the @ref xma_dec_session_create().
 *
 *  @session: Pointer to XmaDecoderSession created with
 *            xma_dec_session_create()
 *
 *  RETURN: XMA_SUCCESS on success
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on failure.
 *
 *  Note: session create & destroy are thread safe APIs
*/
int32_t xma_dec_session_destroy(XmaSessionHandle session);

/**
 *  xma_dec_session_set_log() - This function changes the logging from the
 *  default (set in xma_initialize) to some other logging.
 *
 *  @session: Pointer to session created by xma_dec_session_create()
 *  @handle:  New log to use
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on error.
*/
int32_t xma_dec_session_set_log(XmaSessionHandle session, XmaLogHandle handle);

/**
 *  xma_dec_session_send_data() - This function sends compressed data to the
 *  hardware decoder.
 *
 *  @session:   Pointer to session created by xma_dec_sesssion_create()
 *  @data:      Pointer to a data buffer to be decoded
 *  @data_used: Pointer to an integer to receive the amount of data used
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_SEND_MORE_DATA if more data needs to be sent before first frame
 *                             is ready.
 *          XMA_TRY_AGAIN if NULL needs to be sent again because the input has
 *                        reached the end of stream, but the first frame is not
 *                        yet ready.
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on error.
*/

int32_t xma_dec_session_send_data(XmaSessionHandle session, XmaDataBuffer* data, int32_t* data_used);

/**
 *  xma_dec_session_get_properties() - This function returns properties of the
 *  stream being decoded. This function will only succeed after
 *  xma_dec_session_send_data() has succeeded at least once.
 *
 *  @session: Pointer to session crerated by xma_dec_session_create()
 *  @fprops:  Pointer to a frame properties structure to be filled in
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_SEND_MORE_DATA if more data needs to be sent to determine the
 *                             properties
 *          XMA_ERROR_INVALID on invalid session or invalid fprops.
 *          XMA_ERROR on error.
*/
int32_t xma_dec_session_get_properties(XmaSessionHandle session, XmaFrameProperties* fprops);

/**
 *  xma_dec_session_recv_frame() - This function obtains an output frame with
 *  from the hardware decoder. This function is called after a call to the
 *  function xma_dec_session_send_data() has returned XMA_SUCCESS.
 *
 *  @session: Pointer to session created by xma_dec_sesssion_create()
 *  @frame:   Pointer to a dummy XmaFrame structure created by
 *            xma_frame_alloc(). All parameters will be filled in by this call.
 *            If this function does not return XMA_SUCCESS, xma_frame_free()
 *            must be called on the XmaFrame pointer to release resources.
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_RESEND_AND_RECV if no frames are available yet.
 *          XMA_EOS on end of stream.
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on error.
*/
int32_t xma_dec_session_recv_frame(XmaSessionHandle session, XmaFrame* frame);

#ifdef __cplusplus
}
#endif
