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

#ifndef _XRM_INTERFACE_H_
#define _XRM_INTERFACE_H_

#include <stdbool.h>
#ifdef __cplusplus
#include <cstdint>

extern "C" {
#endif

typedef enum { XRM_IP_SCALER = 0, XRM_IP_DECODER, XRM_IP_ENCODER, XRM_IP_ML, XRM_IP_GPU, XRM_IP_MAX } XrmIPType;

/* XRM INTERFACE API version : For future extension of ABI compatibility*/
// Older APIs (v1.1.2) will be availble as is
typedef enum XrmInterfaceApiVersion {
  XRM_INTERFACE_API_VERSION_1_2     = 0x01020000,                    /**< this is the version used if nothing is set */
  XRM_INTERFACE_API_VERSION_DEFAULT = XRM_INTERFACE_API_VERSION_1_2, /**< this is the version used
                                                              if nothing is set */
  XRM_INTERFACE_API_VERSION_LATEST  = XRM_INTERFACE_API_VERSION_1_2  /**< this is the latest
                                                      version supported by this
                                                      binary */
} XrmInterfaceApiVersion;

typedef struct XrmChannelInfo {
  int width;
  int height;
  int fps_num;
  int fps_den;
} XrmChannelInfo;

typedef struct XrmInterfaceProperties {
  int      width;
  int      height;
  int      fps_num;
  int      fps_den;
  bool     is_la_enabled; // defaults to false
  uint32_t enc_cores;     // defaults to 1-core encoding
  char     preset[16];    // default value is "medium"
  char     model[64];
  char     model_args[512];
} XrmInterfaceProperties;

/**
 * @brief Get the device index using the XRM_RESERVE_ID environment variable.
 *
 * @return The device index corresponding to XRM_RESERVE_ID, 0 if unset, or
 * XRM_ERROR on error
 */
int xrm_interface_get_dev_index(void);

/**
 * @brief Creates a new instance of XRM interface properties based on the given IP type.
 *
 * @param ip_type The type of the IP for which the properties are being created.
 *
 * @return A pointer to the newly created XRM interface properties instance.
 *         Returns NULL if memory allocation fails or if the IP type is invalid.
 *
 * @note The caller is responsible for freeing the memory allocated for the returned pointer
 *       using the xrm_props_destroy() function.
 */
void* xrm_props_create(XrmIPType ip_type);

/**
 * @brief Destroys the XRM interface properties instance and frees the allocated memory.
 *
 * @param xrm_ip_props A double pointer to the XRM interface properties instance.
 *                     After successful destruction, the pointer will be set to NULL.
 *
 * @return 0 on success, XRM_ERROR on failure.
 *
 * @note The caller is responsible for passing a valid non-NULL pointer to this function.
 *       If the pointer is NULL or points to an invalid memory location, the behavior is undefined.
 *       The function does not check if the memory pointed by xrm_ip_props was allocated by xrm_props_create().
 *       It is the caller's responsibility to ensure proper memory management.
 */
int xrm_props_destroy(void** xrm_ip_props);

/**
 * @brief Validates the device index for given reservation Id.
 *
 * This function validates the device index for XRM_RESERVE_ID environment variable,
 *
 * @param dev_id The device index to validate.
 * @return True if the device index is valid, false otherwise.
 */
bool xrm_interface_validate_dev_index(int dev_id);

/**
 * @brief Check the status of XRM.
 *
 * @return 1 if the XRM is enable, 0 otherwise.
 */
int is_xrm_enable(void);

#ifdef __cplusplus
}
#endif

#endif // _XRM_INTERFACE_H_
