// Copyright(C) 2023 - 2024 Advanced Micro Devices, Inc. All rights reserved.
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

#include "xmamiofilter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XmaOverlayProperties {
  XmaHandle                  handle;
  XmaMioFilterPortProperties inputs[2];
  XmaMioFilterPortProperties output;
  int32_t                    x_position;
  int32_t                    y_position;
  float                      alpha; /* overlay one partially transparent
                                       png picture over another, support ARGB format */
  int32_t                    core_id;
  uint32_t                   threading_flags;
} XmaOverlayProperties;

/* Forward declaration */
typedef struct XmaMioFilterSession XmaOverlaySession;

/**
 *  xma_overlay_session_create() - This function creates a overlay session and
 *  must be called prior to overlay a frame. A session reserves hardware
 *  resources for the duration of a video stream. The number of sessions
 *  allowed depends on a number of factors that include: resolution, frame
 *  rate, bit depth, and the capabilities of the hardware accelerator.
 *
 *  @props: Pointer to a XmaOverlayProperties structure that contains the key
 *          configuration properties needed for finding available hardware
 *          resource.
 *
 *  RETURN: Not NULL on success
 *          NULL on failure
 *
 *  Note: session create & destroy are thread safe APIs
*/
XmaOverlaySession* xma_overlay_session_create(XmaOverlayProperties* props);

/**
 *  xma_overlay_session_destroy() - This function destroys a overlay session
 *  that was previously created with the @ref xma_overlay_session_create().
 *
 *  @session: Pointer to XmaOverlaySession created with
 *            xma_overlay_session_create()
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on failure.
 *
 *  Note: session create & destroy are thread safe APIs
*/
int32_t xma_overlay_session_destroy(XmaOverlaySession* session);
/**
 *  xma_overlay_session_set_log() - This function changes the logging from the
 *  default (set in xma_initialize) to some other logging.
 *
 *  @session: Pointer to session created by xma_overlay_session_create()
 *  @handle:  New log to use
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on error.
*/
int32_t xma_overlay_session_set_log(XmaOverlaySession* session, XmaLogHandle handle);

/**
 *  xma_overlay_session_send_frame() - This function sends frames to the
 *  hardware overlay filter.
 *
 *  @session: Pointer to session created by xma_overlay_session_create()
 *  @frame:   Pointer to a frame to be overlayd. A NULL frame should be sent to
 *            flush the overlay filter and to indicate that no more data will be sent.
 *
 *  RETURN: XMA_SUCCESS on success and the overlay filter is ready to produce output.
 *          XMA_TRY_AGAIN on internal buffers are full and need to wait before
 *                        sending the same data again.
 *          XMA_ERROR_INVALID on invalid input.
 *          XMA_ERROR on error.
*/
int32_t xma_overlay_session_send_frame_list(XmaOverlaySession* session, XmaFrame** xma_frame);

/**
 *  xma_overlay_session_recv_frame() - This function obtains output frame with
 *  overlayd data from the hardware overlay filter. This function is called after
 *  calling the function xma_overlay_session_send_frame.
 *
 *  @session: Pointer to session created by xma_overlay_session_create()
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
int32_t xma_overlay_session_recv_frame(XmaOverlaySession* session, XmaFrame* xma_frame);

#ifdef __cplusplus
}
#endif
