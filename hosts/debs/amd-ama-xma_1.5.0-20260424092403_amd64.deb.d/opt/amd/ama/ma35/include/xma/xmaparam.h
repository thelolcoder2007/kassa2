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

#include "xmalogger.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * XmaDataType - Type of data represented by XmaParameter::value
*/
typedef enum {
  XMA_STRING = 1, // < 1
  XMA_INT32,      // < 2
  XMA_UINT32,     // < 3
  XMA_INT64,      // < 4
  XMA_UINT64,     // < 5
  XMA_DOUBLE,     // < 6
  XMA_FUNC_PTR,   // < 7
} XmaDataType;

/**
 * struct XmaParameter - Type-Length-Value data structure used for passing
 * custom arguments to a device or a session
*/
typedef struct XmaParameter {
  char*       name;   // < name of parameter
  XmaDataType type;   // < data type of data
  size_t      length; // < size of data in value
  void*       value;  // < pointer to buffer holding data
} XmaParameter;

/* XMA parameters used to initialize a device */
typedef struct XmaInitParameter {
  const char*   app_name;  // < application name
  uint32_t      device;    // < device ID
  XmaParameter* params;    // < array of custom parameters
  uint32_t      param_cnt; // < count of custom parameters
} XmaInitParameter;

/* XMA API version */
#define XMA_API_VERSION "api_version"

typedef enum XmaAPIVersion {
  XMA_API_VERSION_1_0     = 0x01000000,
  XMA_API_VERSION_1_1     = 0x01010000,
  XMA_API_VERSION_1_1_2   = 0x01010200,
  XMA_API_VERSION_1_2     = 0x01020000,
  XMA_API_VERSION_1_2_1   = 0x01020100,
  XMA_API_VERSION_1_3     = 0x01030000,
  XMA_API_VERSION_DEFAULT = XMA_API_VERSION_1_0, // this is the version used if nothing is set
  XMA_API_VERSION_LATEST  = XMA_API_VERSION_1_3  // this is the latest version supported by this binary
} XmaAPIVersion;

#ifdef __cplusplus
}
#endif

typedef void* XmaSessionHandle;
