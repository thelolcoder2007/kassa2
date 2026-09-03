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

#ifndef _XRM_DEC_INTERFACE_H_
#define _XRM_DEC_INTERFACE_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <xrm.h>
#include "xrm_interface.h"

typedef struct XrmDecodeContext {
  int                 dev_index;
  int                 dec_load;
  int                 xrm_reserve_id;
  int                 decode_res_in_use;
  xrmContext          xrm_ctx;
  xrmCuListResourceV2 decode_cu_list_res;
} XrmDecodeContext;

// V2 interface structures
typedef struct XrmDecodePropsV2 {
  XrmInterfaceApiVersion api_version; // defaults to XRM_INTERFACE_API_VERSION_DEFAULT
  int                    dev_index;
  bool                   is_av1_decode; // is av1 decode, defaults to false
  XrmChannelInfo         input;
} XrmDecodePropsV2;

typedef struct XrmDecodeContextV2 {
  int                 dev_index;
  int                 res_in_use;
  xrmContext          xrm_ctx;
  xrmCuListResourceV2 cu_list_res;
  char                reserved[128];
} XrmDecodeContextV2;

/**
 * @brief Release the xrm resources which were allocated. Destroy the xrm
 * context.
 * 
 * @param xrm_dec_ctx: The xrm decoder context
 */
void xrm_dec_release(XrmDecodeContext* xrm_dec_ctx);

/**
 * @brief Check & allocate the necessary xrm resources given the relevant 
 * decoder parameters
 * 
 * @param xrm_dec_ctx The xrm wrapper struct for xrm information
 * @param dev_index The device which will be queried
 * @param xrm_props Contains relevant information for load calculation
 * @return XRM_SUCCESS on success, XRM_ERROR on error or XRM_ERROR_CONNECT_FAIL
 * when unable to connect to daemon
 */
int xrm_dec_reserve(XrmDecodeContext* xrm_dec_ctx, int dev_index, XrmInterfaceProperties* xrm_props);

/**
 * @brief Release the XRM resources which were allocated for the decoder.
 *        Destroy the XRM context.
 *
 * @param xrm_dec_ctx: A pointer to the XRM decoder context structure.
 *                     This structure should have been initialized and resources
 *                     reserved using xrm_dec_reserve_v2 function.
 *
 * @note: This function should be called when the decoder is no longer in use
 *        to free up the allocated resources and destroy the XRM context.
 */
void xrm_dec_release_v2(XrmDecodeContextV2* xrm_dec_ctx);

/**
 * @brief Reserve XRM resources for the decoder using the provided properties.
 *
 * This function checks if the necessary XRM resources are available for the decoder
 * on the specified device based on the given properties. If the resources are
 * available, it reserves them and initializes the XRM decoder context.
 *
 * @param xrm_dec_ctx A pointer to the XRM decoder context structure. This structure
 *                    should be initialized before calling this function.
 * @param xrm_dec_props A pointer to the XRM decoder properties structure. This
 *                     structure contains relevant information for load calculation.
 *
 * @return An integer representing the result of the operation.
 *         - XRM_SUCCESS: The resources were successfully reserved and the context
 *                        was initialized.
 *         - XRM_ERROR: An error occurred during the reservation process.
 *         - XRM_ERROR_CONNECT_FAIL: Unable to connect to the XRM daemon.
 */
int xrm_dec_reserve_v2(XrmDecodeContextV2* xrm_dec_ctx, const XrmDecodePropsV2* xrm_dec_props);

#ifdef __cplusplus
}
#endif
#endif // _XRM_DEC_INTERFACE_H_
