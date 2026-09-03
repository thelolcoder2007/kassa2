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
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.

#pragma once

#include "xmabuffers.h"
#include "xmasidedata.h"
#include "xmalogger.h"
#include "xmadecoder.h"
#include "xmaencoder.h"
#include "xmaerror.h"
#include "xmascaler.h"
#include "xmafilter.h"
#include "xmamiofilter.h"
#include "xmarotate.h"
#include "xmadrawbox.h"
#include "xmacsc.h"
#include "xmasubsample.h"
#include "xmaoverlay.h"
#include "xmatile.h"
#include "xmablend.h"
#include "xmacrop.h"
#include "xmapad.h"
#include "xmacompositor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * DOC: XMA Application Interface
 * The interface used by stand-alone XMA applications or plugins
*/

/**
 *  xma_get_latest_api_version() - Returns the latest version of the API
 *  supported by this binary
 * 
 * RETURN: API version number
 * 
*/
XmaAPIVersion xma_get_latest_api_version();

/**
 *  xma_initialize() - Initialie XMA Library and devices
 *
 *  This is the entry point routine for utilzing the XMA library and must be
 *  the first call within any application before calling any other XMA APIs.
 *
 *  @log: handle to a XmaLogHandle returned by a call to xma_log_init()
 *  @init_params: parameters used to initialize the device with
 *  @handle: pointer to recieve a XmaHandle
 * 
 * RETURN: XMA_SUCCESS on success
 *         XMA_ERROR_BAD_ALLOC on out of memory
 *         XMA_ERROR_INVALID on invalid argument
 *         XMA_ERROR on other errors, check log
 * 
*/
int32_t xma_initialize(XmaLogHandle log, XmaInitParameter* init_params, XmaHandle* handle);

/**
 *  xma_release() - Release XMA devices
 *
 *  This releases memory allocated by the XMA library and must be
 *  the last XMA call within any application.
 *
 *  @handle: handle to XMA device
 * 
*/
void xma_release(XmaHandle handle);

#ifdef __cplusplus
}
#endif
