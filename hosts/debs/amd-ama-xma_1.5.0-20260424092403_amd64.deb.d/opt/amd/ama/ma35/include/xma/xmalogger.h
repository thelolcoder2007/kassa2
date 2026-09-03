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

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Alias XmaLogLevelType to standard SysLog levels
 */

#define XMA_LOG_LEVEL_LIST \
  OP(EMERGENCY) \
  OP(ALERT) \
  OP(CRITICAL) \
  OP(ERROR) \
  OP(WARNING) \
  OP(NOTICE) \
  OP(INFO) \
  OP(DEBUG)

#define OP(X) XMA_##X##_LOG,
typedef enum XmaLogLevelType { XMA_LOG_LEVEL_LIST XMA_LOG_LEVEL_LIST_COUNT } XmaLogLevelType;
#undef OP

#define XMA_LOG_TYPE_LIST \
  OP(NONE) \
  OP(CONSOLE) \
  OP(SYSLOG) \
  OP(FILE) \
  OP(CALLBACK) \
  OP(AMA)

#define OP(X) XMA_LOG_TYPE_##X,
typedef enum XmaLogType { XMA_LOG_TYPE_LIST XMA_LOG_TYPE_LIST_COUNT } XmaLogType;
#undef OP

typedef void (*XmaLogCallbackFunction)(void* Opaque, XmaLogLevelType Level, const char* Name, const char* Msg);

typedef void* XmaLogHandle;

/**
 * xma_log_init() - This function initializes the logs.
 *
 * @log_level:     Minimum logging level for a message to be added to the log
 * @log_type:      Location to write the log to
 * @handle:        Pointer to recieve handle
 * ...:            If the log_type is 'XMA_LOG_TYPE_FILE', this will be a
 *                 character string that specifies the file to log to. If the
 *                 log_type is 'XMA_LOG_TYPE_CALLBACK', this will be a function
 *                 pointer of type XmaLogCallbackFunction followed by a void*
 *                 parameter that will be passed to the callback as opaque
 *
 * RETURN: XMA_SUCCESS on success
 *         XMA_ERROR_BAD_ALLOC on out of memory
 *         XMA_ERROR_INVALID on invalid input
*/
int32_t xma_log_init(XmaLogLevelType LogLevel, XmaLogType LogType, XmaLogHandle* Handle, ...);

/**
 * xma_release_logging() - This function releases memory associated with the
 * logs
 *
 * @handle: Handle to log to release
 *
*/
void xma_log_release(XmaLogHandle Handle);

/**
 * xma_logmsg() - This function logs a message to the log handle. The log
 * message includes the current time, logging level, a unique name, and a
 * message.
 *
 * @handle: Handle to XMA logging
 * @level:  Logging level associated with the message
 * @name:   Pointer to a C string indicating the name of the entity that is
 *          generating the log message
 * @msg:    Pointer to a compatible C format string
 * @...:    Variable arguments used in conjunction with the C format string
 *          contained in the msg parameter
 *
*/
void xma_logmsg(XmaLogHandle Handle, XmaLogLevelType Level, const char* Name, const char* Msg, ...);

#ifdef __cplusplus
}
#endif
