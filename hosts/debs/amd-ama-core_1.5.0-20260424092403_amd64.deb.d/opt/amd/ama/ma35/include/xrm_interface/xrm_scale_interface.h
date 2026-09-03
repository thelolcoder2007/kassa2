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

#ifndef _XRM_SCALE_INTERFACE_H_
#define _XRM_SCALE_INTERFACE_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <xrm.h>
#include "xrm_interface.h"

#define XRM_MAX_SCALER_CHANNEL 16

typedef struct XrmScaleContext {
  int                 dev_index;
  xrmContext          xrm_ctx;
  int                 xrm_reserve_id;
  int                 scaler_load;
  int                 scaler_res_in_use;
  xrmCuListResourceV2 scaler_cu_list_res;
} XrmScaleContext;

// V2 interface structures
typedef struct XrmScalePropsV2 {
  XrmInterfaceApiVersion api_version; // defaults to XRM_INTERFACE_API_VERSION_DEFAULT
  int                    dev_index;
  int                    num_outputs;
  XrmChannelInfo         input;
  XrmChannelInfo         output[XRM_MAX_SCALER_CHANNEL];
} XrmScalePropsV2;

typedef struct XrmScaleContextV2 {
  int                 dev_index;
  int                 res_in_use;
  xrmContext          xrm_ctx;
  xrmCuListResourceV2 cu_list_res;
  char                reserved[128];
} XrmScaleContextV2;

/**
 * xrm_scale_device_deinit: Release and relinquish the xrm resources which
 * were reserved and allocated. Destroy the xrm context API.
 * @param scaler_xrm_ctx: The xrm scaler context
 */
void xrm_scale_release(XrmScaleContext* scaler_xrm_ctx);

/**
 * @brief Check & allocate the necessary xrm resources given the relevant 
 * scaler parameters
 * 
 * @param scaler_xrm_ctx Used to store information about resources this function
 * allocates so they can be safely released later
 * @param dev_index The device index
 * @param input_props Input props used to calculate load
 * @param output_props Output props used to calculate load
 * @param num_outputs Number of outputs in output_props
 * @return XRM_SUCCESS on success, XRM_ERROR on error or XRM_ERROR_CONNECT_FAIL
 * when unable to connect to daemon
 */
int32_t xrm_scale_reserve(XrmScaleContext* scaler_xrm_ctx, int dev_index, XrmInterfaceProperties* input_props, XrmInterfaceProperties* output_props, int num_outputs);

/**
 * @brief This function releases the XRM resources that were reserved using the
 * xrm_scale_reserve_v2 function. It also destroys the XRM context associated with the scaler operation.
 *
 * @param scaler_xrm_ctx Pointer to the XRM scale context structure. 
 * This structure contains information about the reserved resources and the XRM context.
 *
 * @return void.
 */
void xrm_scale_release_v2(XrmScaleContextV2* scaler_xrm_ctx);

/**
 * @brief Reserves and allocates necessary XRM resources for scaler operation.
 *
 * @param xrm_scale_ctx Pointer to the XRM scale context output structure.
 *
 * @param xrm_scale_input_props Pointer to the XRM scale properties input structure.
 *
 * @param xrm_scale_output_props Pointer to the XRM scale output properties structure.
 * This structure will be populated with the allocated resources.
 *
 * @param num_outputs Number of outputs in the xrm_scale_output_props structure.
 *
 * @return int32_t Returns XRM_SUCCESS on success, XRM_ERROR on error, or XRM_ERROR_CONNECT_FAIL
 * when unable to connect to the daemon.
 */
int32_t xrm_scale_reserve_v2(XrmScaleContextV2* xrm_scale_ctx, const XrmScalePropsV2* xrm_scale_props);

#ifdef __cplusplus
}
#endif
#endif // _XRM_SCALE_INTERFACE_H_
