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

#ifndef _XRM_ENC_INTERFACE_H_
#define _XRM_ENC_INTERFACE_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <xrm.h>
#include "xrm_interface.h"

#define XRM_HEVC_ENC "ENCODER"

/* See https://confluence.xilinx.com/display/VDC/XRM+host+plugins for diagram 
showing slice layout */
typedef enum { XAV1_SLICE_0 = 0, VC8000E_SLICE_0 = 1, XAV1_SLICE_1 = 2, VC8000E_SLICE_1 = 3 } XRM_ENCODER_GROUP_IDs;

typedef struct {
  int                 dev_index;
  int                 slice_id;
  xrmContext          xrm_ctx;
  xrmCuListResourceV2 encode_cu_list_res;
  int32_t             xrm_reserve_id;
  int32_t             enc_res_in_use;
  int                 enc_load[3]; // is_xav1 flag, encoder load & lookahead load
} XrmEncodeContext;

// V2 interface structures
typedef struct XrmEncodePropsV2 {
  XrmInterfaceApiVersion api_version; // defaults to XRM_INTERFACE_API_VERSION_DEFAULT
  int                    dev_index;
  bool                   is_av1_type1;  // Applicable to AV1 encoders only, default to false
  int                    slice_id;      // defaults to -1
  bool                   is_la_enabled; // defaults to true
  int                    enc_cores;     // defaults to 1-core encoding
  char                   preset[16];    // default value is "medium"
  XrmChannelInfo         input;
} XrmEncodePropsV2;

typedef struct XrmEncodeContextV2 {
  int                 dev_index;
  int                 res_in_use;
  int                 slice_id;
  xrmContext          xrm_ctx;
  xrmCuListResourceV2 cu_list_res;
  char                reserved[128];
} XrmEncodeContextV2;

/**
 * @brief Release the xrm resources which were allocated. Destroy the xrm
 * context.
 * 
 * @param xrm_enc_ctx: Encoder XRM context
 * @return None
 */
void xrm_enc_release(XrmEncodeContext* xrm_enc_ctx);

/**
 * @brief Check & allocate the necessary xrm resources given the relevant 
 * encoder parameters
 * 
 * @param xrm_enc_ctx Used to store information about resources this function
 * allocates so they can be safely released later
 * @param dev_index The device index
 * @param slice_id The slice index. -1 for xrm to choose the slice, otherwise
 * 0 or 1
 * @param is_av1 Is the codec to be used xav1 or not
 * @param is_ull Is ultra low latency enabled or not
 * @param xrm_props Contains relevant information for load calculation
 * @return XRM_SUCCESS on success, XRM_ERROR on error or XRM_ERROR_CONNECT_FAIL
 * when unable to connect to daemon
 */
int32_t xrm_enc_reserve(XrmEncodeContext* xrm_enc_ctx, int dev_index, int slice_id, bool is_xav1, bool is_ull, XrmInterfaceProperties* xrm_props);

/**
 * @brief Release the XRM resources which were allocated for the encoder.
 *        Destroy the XRM context.
 *
 * @param xrm_enc_ctx: Pointer to the XrmEncodeContextV2 structure that holds the allocated resources and context information.
 *
 * @warning The caller is responsible for ensuring that the XrmEncodeContextV2 structure is properly initialized before calling this function.
 *          Failure to do so may result in undefined behavior.
 */
void xrm_enc_release_v2(XrmEncodeContextV2* xrm_enc_ctx);

/**
 * @brief Reserve XRM resources for the encoder based on the provided properties.
 *
 * @param xrm_enc_ctx Pointer to the XrmEncodeContextV2 output structure to store the allocated resources and context information.
 * @param xrm_enc_props Pointer to the XrmEncodePropsV2 input structure containing the relevant encoder properties.
 *
 * @return XRM_SUCCESS on success, XRM_ERROR on error, or XRM_ERROR_CONNECT_FAIL when unable to connect to the XRM daemon.
 *
 * @warning The caller is responsible for releasing the allocated resources using the xrm_enc_release_v2 function.
 */
int32_t xrm_enc_reserve_v2(XrmEncodeContextV2* xrm_enc_ctx, const XrmEncodePropsV2* xrm_enc_props);

#ifdef __cplusplus
}
#endif
#endif // _XRM_ENC_INTERFACE_H_
