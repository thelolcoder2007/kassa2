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

#ifndef _XRM_ML_INTERFACE_H_
#define _XRM_ML_INTERFACE_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <xrm.h>
#include "xrm_interface.h"

typedef struct XrmMLContext {
  int                 dev_index;
  int                 ml_core_id;
  int                 ml_load;
  int                 xrm_reserve_id;
  int                 ml_res_in_use;
  xrmContext          xrm_ctx;
  xrmCuListResourceV2 ml_cu_list_res;
} XrmMLContext;

// V2 interface structures
typedef struct XrmMLPropsV2 {
  XrmInterfaceApiVersion api_version; // defaults to XRM_INTERFACE_API_VERSION_DEFAULT
  int                    dev_index;
  int                    core_id;
  char                   model[64];
  char                   model_args[1024];
  XrmChannelInfo         input;
} XrmMLPropsV2;

typedef struct XrmMLContextV2 {
  int                 dev_index;
  int                 res_in_use;
  int                 core_id;
  xrmContext          xrm_ctx;
  xrmCuListResourceV2 cu_list_res;
  char                reserved[128];
} XrmMLContextV2;

/**
 * @brief Release the xrm resources which were allocated. Destroy the xrm
 * context.
 * 
 * @param xrm_ml_ctx: The xrm ml context
 */
void xrm_ml_release(XrmMLContext* xrm_ml_ctx);

/**
 * @brief Check & allocate the necessary xrm resources given the relevant 
 * ml parameters
 * 
 * @param xrm_ml_ctx The xrm wrapper struct for xrm information
 * @param dev_index The device which will be queried
 * @param xrm_props Contains relevant information for load calculation
 * @return XRM_SUCCESS on success, XRM_ERROR on error or XRM_ERROR_CONNECT_FAIL
 * when unable to connect to daemon
 */
int xrm_ml_reserve(XrmMLContext* xrm_ml_ctx, int dev_index, XrmInterfaceProperties* xrm_props);

/**
 * @brief Release the XRM resources which were allocated for the ML context.
 *        This function also destroys the XRM context.
 *
 * @param xrm_ml_ctx: A pointer to the XrmMLContextV2 structure that holds the XRM information.
 *
 * @return void: This function does not return any value.
 *
 * @note This function should be called when the ML context is no longer needed to free up the allocated resources.
 *       It is important to call this function to ensure proper resource management and avoid memory leaks.
 */
void xrm_ml_release_v2(XrmMLContextV2* xrm_ml_ctx);

/**
 * @brief Check & allocate the necessary xrm resources given the relevant 
 * 
 * @param xrm_ml_ctx A pointer to the XrmMLContextV2 output structure that will hold the 
 *      
 * @param xrm_ml_props A pointer to the XrmMLPropsV2 input structure that contains 
 * 
 * @return XRM_SUCCESS on success, XRM_ERROR on error, or XRM_ERROR_CONNECT_FAIL 
 *         when unable to connect to the XRM daemon.
 */
int xrm_ml_reserve_v2(XrmMLContextV2* xrm_ml_ctx, const XrmMLPropsV2* xrm_ml_props);

#ifdef __cplusplus
}
#endif
#endif // _XRM_ML_INTERFACE_H_
