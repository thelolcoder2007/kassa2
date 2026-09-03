// Copyright(C) 2022 - 2024 Advanced Micro Devices, Inc. All rights reserved.
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

#include <string>
#include "xrm_interface.h"
#include "xrm_utils.h"

/**
 * @brief Create a json object using the given attributes
 * 
 * @param xrm_props Properties relevant for creating a json for MA35
 * @param func_name The name of the function: DECODER, SCALER, LOOKAHEAD, ENCODER, ML
 * @param output Where to store the resulting json
 * @param is_xav1 bool indicating type-1 (xav1) or type-2 (vav1) encoder
 */
void create_json(XrmPluginInterface* input_props, const PropsVector& output_props, const std::string& func_name, std::string& output, bool is_xav1);
