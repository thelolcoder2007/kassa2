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

#if defined(_WIN32)
#define strtok_r strtok_s
#endif

#include <string>
#include <vector>
#include <memory>
#include <xrm.h>
#include "xrm_interface.h"

#define XRM_UTILS_PRECISION_1000000_BIT_MASK(load) ((load << 8))

typedef struct {
  std::string ip_name;
  std::string plugin_name;
  std::string kernel_name;
  std::string kernel_alias;
} XrmUtilsIPMapping;

/* XRM Plugin Interface data structure */
typedef struct XrmPluginInterface {
  int         width         = 0;
  int         height        = 0;
  int         fps_num       = 0;
  int         fps_den       = 0;
  bool        is_la_enabled = false;
  uint32_t    enc_cores     = 0;
  std::string preset        = "";
  std::string model         = "";
  std::string model_args    = "";
} XrmPluginInterface;

/**
 * @brief Checks and allocates resources for xrm given the cu pool properties
 * with relevant device info
 * 
 * @param xrm_ctx The xrm context used for xrm calls
 * @param pool_property Pool property containing relevant device / job info
 * @param xrm_cu_list_res Will be filled by xrmCuListAllocV2
 * @return XRM_SUCCESS or XRM_ERROR
 */
int xrm_utils_reserve_resources(xrmContext xrm_ctx, xrmCuPoolPropertyV2* pool_property, xrmCuListResourceV2* xrm_cu_list_res);

/**
 * @brief Update the cu pool properties based on the relevant information
 * provided
 * 
 * @param ip_name What module this is: "DECODER", "SCALER", "ENCODER" or "ML"
 * @param device_id The device index whose resources we should query
 * @param cu_load The load of the job
 * @param cu_pool_prop The properties to be set
 * @return XRM_SUCCESS or XRM_ERROR
 */
int xrm_utils_update_pool_props(const std::string& ip_name, int dev_index, int cu_load, xrmCuPoolPropertyV2* cu_pool_prop, bool is_xav1);

/**
 * @brief Calculate the load for the module specified by ip_name. Can be
 * "DECODER", "SCALER", "ENCODER" or "ML"
 * 
 * @param xrm_ctx xrm context used for calls to xrm
 * @param xrm_props Values relevant for creating a json for MA35 load calculation
 * @param input_props Props used to calculate load
 * @param output_props Props used to calculate load (if the output affects the density such as scaler)
 *     s_no_output can be used if not applicable.
 * @param ip_name Can be "DECODER", "SCALER", "ENCODER" or "ML"
 * @param cu_load Pointer to the cu load to be set, in the case of encoder, this
 * is an array of size 3.
 * cu_load[0] = is_xav1 flag
 * cu_load[1] = encoder load
 * cu_load[2] = lookahead load
 * @return The cu_load calculated, or XRM_ERROR on error
 */
typedef std::vector<XrmPluginInterface> PropsVector;
extern PropsVector                      s_no_outputs;
int xrm_utils_load_calc(xrmContext xrm_ctx, XrmPluginInterface* input_props, PropsVector& output_props, const std::string& ip_name, int* cu_load, bool is_xav1);
