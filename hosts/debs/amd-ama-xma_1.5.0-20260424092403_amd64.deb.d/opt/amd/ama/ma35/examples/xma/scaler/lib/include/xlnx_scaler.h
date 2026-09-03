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

#ifndef _XLNX_ABR_SCALER_H_
#define _XLNX_ABR_SCALER_H_

#include <dlfcn.h>
#include <errno.h>
#include <memory.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include <xma.h>

#include "xlnx_scal_arg_parse.h"
#include "xlnx_scal_common.h"
#include "xlnx_scal_xma_props.h"
#include "xrm_scale_interface.h"

typedef enum { PLANE_Y = 0, PLANE_U, PLANE_V } PlaneId;

typedef struct XlnxScalerCtx {
  XlnxScalerProps     abr_params;
  XmaScalerProperties abr_xma_props[2]; // All rate and full rate sessions
  XmaFilterProperties xma_upload_props;
  XmaFrameProperties  upload_fprops;
  XmaFilterProperties xma_download_props[MAX_SCALER_OUTPUTS];
  XmaFrameProperties  download_fprops[MAX_SCALER_OUTPUTS];
  XrmScaleContextV2   scaler_xrm_ctx;
  XmaLogHandle        log;
  XmaHandle           handle;
  int                 num_sessions;
  XmaSessionHandle    session[2];
  XmaSessionHandle    upload_session;
  XmaSessionHandle    download_sessions[MAX_SCALER_OUTPUTS];
  XmaFrame*           input_xframe;
  XmaFrameProperties  input_fprops;
  XmaFrame*           output_xframe_list[MAX_SCALER_OUTPUTS];
  XmaFrameProperties  output_fprops[MAX_SCALER_OUTPUTS];
  size_t              num_frames_scaled;
  int                 pts;
} XlnxScalerCtx;

/**
 * xlnx_scal_cleanup_scaler_ctx: Cleanup the scaler context - free xma frames,
 * destroy scaler session, free xrm resources.
 * @param ctx: The scaler context
 */
void xlnx_scal_cleanup_scaler_ctx(XlnxScalerCtx* ctx);

/**
 * xlnx_scal_device_init: Get/allocate xrm resources, xma initialize.
 *
 * @param ctx: The scaler context
 * @return XMA_APP_SUCCESS on success XMA_APP_ERROR on error
 */
int32_t xlnx_scal_device_init(XlnxScalerCtx* ctx);

/**
 * xlnx_scal_create_context: Uses ctx->abr_params to create the rest of the
 * scaler context. Does not init the device/create session.
 * @param ctx: A pointer to the scaler context
 * @return XMA_APP_SUCCESS on success XMA_APP_ERROR on error
 */
int32_t xlnx_scal_create_scaler_ctx(XlnxScalerCtx* ctx);

/**
 * xlnx_scal_create_scaler_sessions: Creates the scaler sessions using the
 * scaler props. One session for all rates, another for full rate only (If there
 * are half rate sessions specified.)
 * @param ctx: A pointer to the scaler context
 * @return XMA_APP_SUCCESS on success XMA_APP_ERROR on error
 */
int32_t xlnx_scal_create_scaler_sessions(XlnxScalerCtx* ctx);

#endif
