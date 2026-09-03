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

#define MAX_COMPOSITOR_INPUTS 64
#define XMA_COMPOSITOR_BG_COLOR_DEFAULT 0xFF000000
#define XMA_COMPOSITOR_ENABLE_INPUT_DEFAULT 0 // All inputs are disable
#define XMA_COMPOSITOR_BORDER_MULTIPLE 4

typedef enum XmaRotationAngle { XMA_ROTATION_0_DEGREE, XMA_ROTATION_90_DEGREE, XMA_ROTATION_180_DEGREE, XMA_ROTATION_270_DEGREE } XmaRotationAngle;
typedef enum Flip { FLIP_NONE = 0, POST_FLIP_X = 1, POST_FLIP_Y = 2 } Flip;

typedef struct XmaCompositorRectProperties {
  uint32_t width;  /* crop width */
  uint32_t height; /* crop height */
  uint32_t x;      /* left position for crop */
  uint32_t y;      /* top position for crop */
} XmaCompositorRectProperties;

typedef struct XmaCompositorPortProperties {
  enum XmaFormatType format;    /* host side video format entering/leaving
                                   port */
  enum XmaFormatType sw_format; /* if data is on device, this specifies the
                                   pixel format on device side */
  int32_t            width;     /* width in pixels of data */
  int32_t            height;    /* height in pixels of data */
  uint32_t           flags;     /* input/output flags, must be a combination
                                   of XMA_FRAME_PROPERTY_FLAG_xxx */
  XmaFraction        framerate; /* framerate data structure specifying frame
                                   rate per second */
  int32_t            stride;    /* stride of video data row */
} XmaCompositorPortProperties;

typedef struct XmaCompositorInputProperties {
  XmaCompositorPortProperties input;             /* Input */
  XmaCompositorRectProperties src_rect;          /* src rectangle */
  XmaCompositorRectProperties dst_rect;          /* location of dst rectangle in output*/
  float                       alpha;             /* alpha per rectangle */
  uint32_t                    z_order;           /* ordering of objects along the Z-axis*/
  XmaRotationAngle            angle;             /* rotation angle */
  uint32_t                    flip;              /* flip horizontal or vertical */
  uint32_t                    border_color;      /* border color: format is RGB 0x00RRGGBB */
  uint32_t                    border_inner_size; /* Inner border thickness */
  uint32_t                    border_outer_size; /* Outer border thickness */
} XmaCompositorInputProperties;

typedef struct XmaCompositorProperties {
  XmaHandle                    handle;                        /* xma handle */
  XmaCompositorInputProperties inputs[MAX_COMPOSITOR_INPUTS]; /* Input properties */
  uint32_t                     num_inputs;                    /* Number of Inputs */
  uint64_t*                    dyn_enable_input;              /* Dynamic value allocation to enable input
                                                                 if its null the default enable_input will
                                                                 be set, Each bit 1 or 0 describes
                                                                 if specific input no is enable */
  bool                         enable_background;             /* Enable background flag */
  XmaCompositorPortProperties  output;                        /* output properties*/
  uint32_t                     core_id;                       /* Core id */
  uint32_t                     threading_flags;               /* Threading flags */
  uint32_t*                    dyn_background_color;          /* Dynamic value allocation to background color 
                                                                 if its null the default background_color will 
                                                                 be set : format is RGB 0x00RRGGBB */
} XmaCompositorProperties;

/* Forward declaration */
typedef struct XmaMioFilterSession XmaCompositorSession;

/**
 *  xma_compositor_session_create() - This function creates a compositor session and
 *  must be called prior to compositor a frame. A session reserves hardware
 *  resources for the duration of a video stream. The number of sessions
 *  allowed depends on a number of factors that include: resolution, frame
 *  rate, bit depth, and the capabilities of the hardware accelerator.
 *
 *  @props: Pointer to a XmaCompositorProperties structure that contains the key
 *          configuration properties needed for finding available hardware
 *          resource.
 *
 *  RETURN: Not NULL on success
 *          NULL on failure
 *
 *  Note: session create & destroy are thread safe APIs
*/
XmaCompositorSession* xma_compositor_session_create(XmaCompositorProperties* props);

/**
 *  xma_compositor_session_destroy() - This function destroys a compositor session
 *  that was previously created with the @ref xma_compositor_session_create().
 *
 *  @session: Pointer to XmaCompositorSession created with
 *            xma_compositor_session_create()
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on failure.
 *
 *  Note: session create & destroy are thread safe APIs
*/
int32_t xma_compositor_session_destroy(XmaCompositorSession* session);
/**
 *  xma_compositor_session_set_log() - This function changes the logging from the
 *  default (set in xma_initialize) to some other logging.
 *
 *  @session: Pointer to session created by xma_compositor_session_create()
 *  @handle:  New log to use
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on error.
*/
int32_t xma_compositor_session_set_log(XmaCompositorSession* session, XmaLogHandle handle);

/**
 *  xma_compositor_session_send_frame() - This function sends frames to the
 *  hardware compositor filter.
 *
 *  @session: Pointer to session created by xma_compositor_session_create()
 *  @frame:   Pointer to a frame to be compositord. A NULL frame should be sent to
 *            flush the compositor filter and to indicate that no more data will be sent.
 *
 *  RETURN: XMA_SUCCESS on success and the compositor filter is ready to produce output.
 *          XMA_TRY_AGAIN on internal buffers are full and need to wait before
 *                        sending the same data again.
 *          XMA_ERROR_INVALID on invalid input.
 *          XMA_ERROR on error.
*/
int32_t xma_compositor_session_send_frame_list(XmaCompositorSession* session, XmaFrame** frame_list);

/**
 *  xma_compositor_session_recv_frame() - This function obtains output frame with
 *  compositord data from the hardware compositor filter. This function is called after
 *  calling the function xma_compositor_session_send_frame.
 *
 *  @session: Pointer to session created by xma_compositor_session_create()
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
int32_t xma_compositor_session_recv_frame(XmaCompositorSession* session, XmaFrame* xma_frame);

#ifdef __cplusplus
}
#endif
