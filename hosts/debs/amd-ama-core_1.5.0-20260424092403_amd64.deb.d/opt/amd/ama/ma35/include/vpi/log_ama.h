// Copyright (C) 2022 - 2025 Advanced Micro Devices, Inc. All rights reserved.
//
// Copyright (C) 2020 - 2022 Xilinx, Inc. All rights reserved
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

#include <stdarg.h>

#ifdef __cplusplus
#include <functional>

extern "C" {
#endif

/*
 * LogAmaLogLevel:
 * Enumeration of different log levels used by the AMA logging system.
 * if LogAmaLogLevel is updated, update string mapping in C file under define LOG_LEVEL_MAPPING.
 */
typedef enum {
  LOG_AMA_LEVEL_EMERG,
  LOG_AMA_LEVEL_FATAL,
  LOG_AMA_LEVEL_ERROR,
  LOG_AMA_LEVEL_WARN,
  LOG_AMA_LEVEL_INFO,
  LOG_AMA_LEVEL_DEBUG,
  LOG_AMA_LEVEL_TRACE,
  LOG_AMA_LEVEL_ALL,
  LOG_AMA_LEVEL_MAX /* Not for use */
} LogAmaLogLevel;

/*
 * LogAmaLayer:
 * Enumeration of different layers for logging purposes.
 * if LogAmaLayer is updated, update string mapping in C file under define LAYER_MAPPING.
 */
typedef enum {
  LOG_AMA_LAYER_HW,
  LOG_AMA_LAYER_FW,
  LOG_AMA_LAYER_KERNEL,
  LOG_AMA_LAYER_SDK,
  LOG_AMA_LAYER_VPI,
  LOG_AMA_LAYER_XMA,
  LOG_AMA_LAYER_AMF,
  LOG_AMA_LAYER_APP,
  LOG_AMA_LAYER_FFMPEG,
  LOG_AMA_LAYER_GST,
  LOG_AMA_LAYER_XRM,
  LOG_AMA_LAYER_GENERAL,
  LOG_AMA_LAYER_UTILS,
  LOG_AMA_LAYER_MAX /* Not for use */
} LogAmaLayer;

/*
 * LogAmaAccelType:
 * Enumeration of different types of hw accelerator logging purposes.
 * if LogAmaAccel is updated, update string mapping in C file under define ACCEL_MAPPING.
 */
typedef enum {
  LOG_AMA_ACCEL_ABR,
  LOG_AMA_ACCEL_DEC,
  LOG_AMA_ACCEL_ENC,
  LOG_AMA_ACCEL_GC,
  LOG_AMA_ACCEL_ML,
  LOG_AMA_ACCEL_HDMA,
  LOG_AMA_ACCEL_GENERAL,
  LOG_AMA_ACCEL_MAX /* Not for use */
} LogAmaAccel;

/**********************************************************************************************/
/*** All APIs/struct with log_ama_priv_ prefix must not be used directly, USE HELPER MACROs ***/
/**********************************************************************************************/
typedef struct {
  LogAmaAccel accel_type;
  const char* instance_name;
} log_ama_priv_LogAmaInstanceInfo;

const char*                     log_ama_priv_GetInstanceInfo(LogAmaAccel* AccelType);
log_ama_priv_LogAmaInstanceInfo log_ama_priv_SetAndGetInstanceInfo(LogAmaAccel, const char* InstanceName);
void                            log_ama_priv_RestoreInstanceInfo(log_ama_priv_LogAmaInstanceInfo);

LogAmaLogLevel log_ama_priv_GetLogFilter(LogAmaLayer layer_type);
LogAmaLogLevel log_ama_priv_GetLogFilterPerf(LogAmaLayer layer_type);

#ifdef __GNUC__
#define LOG_AMA_ATTRIBUTE_PRINTF(formatIndex, ellipsisIndex) __attribute__((format(printf, formatIndex, ellipsisIndex)))
#else
#define LOG_AMA_ATTRIBUTE_PRINTF(formatIndex, ellipsisIndex)
#endif

void log_ama_priv_PerfINFO(LogAmaLayer, int flag, int frameNumber, const char* format, ...) LOG_AMA_ATTRIBUTE_PRINTF(4, 5);
void log_ama_priv_PerfDEBUG(LogAmaLayer, int flag, int frameNumber, const char* format, ...) LOG_AMA_ATTRIBUTE_PRINTF(4, 5);

void log_ama_priv_Message(const char* File, unsigned Line, LogAmaLogLevel LogLevel, LogAmaLayer LayerType, const char* formattedMessage, ...)
    LOG_AMA_ATTRIBUTE_PRINTF(5, 6);
void log_ama_priv_Message_Valist(const char* File, unsigned Line, LogAmaLogLevel LogLevel, LogAmaLayer LayerType, const char* formattedMessage, va_list args);

void log_ama_priv_MessageEMERG(const char* File, unsigned Line, LogAmaLayer LayerType, const char* formattedMessage, ...) LOG_AMA_ATTRIBUTE_PRINTF(4, 5);
void log_ama_priv_MessageFATAL(const char* File, unsigned Line, LogAmaLayer LayerType, const char* formattedMessage, ...) LOG_AMA_ATTRIBUTE_PRINTF(4, 5);
void log_ama_priv_MessageERROR(const char* File, unsigned Line, LogAmaLayer LayerType, const char* formattedMessage, ...) LOG_AMA_ATTRIBUTE_PRINTF(4, 5);
void log_ama_priv_MessageWARN(const char* File, unsigned Line, LogAmaLayer LayerType, const char* formattedMessage, ...) LOG_AMA_ATTRIBUTE_PRINTF(4, 5);
void log_ama_priv_MessageINFO(const char* File, unsigned Line, LogAmaLayer LayerType, const char* formattedMessage, ...) LOG_AMA_ATTRIBUTE_PRINTF(4, 5);
void log_ama_priv_MessageDEBUG(const char* File, unsigned Line, LogAmaLayer LayerType, const char* formattedMessage, ...) LOG_AMA_ATTRIBUTE_PRINTF(4, 5);
void log_ama_priv_MessageTRACE(const char* File, unsigned Line, LogAmaLayer LayerType, const char* formattedMessage, ...) LOG_AMA_ATTRIBUTE_PRINTF(4, 5);
void log_ama_priv_MessageALL(const char* File, unsigned Line, LogAmaLayer LayerType, const char* formattedMessage, ...) LOG_AMA_ATTRIBUTE_PRINTF(4, 5);
/**********************************************************************************************/
/*** All APIs/struct with log_ama_priv_ prefix must not be used directly, USE HELPER MACROs ***/
/**********************************************************************************************/

/*
 * log_ama_setup:
 * Initializes the AMA logging system.
 * This is optional. Framework reads config from env variable at start of program.
 * If CLI is required, putenv may be used by caller of this API.
 */
int  log_ama_setup(void); //setup
void log_ama_shutdown(void);

/*
 * Macros for logging with specific log levels and layer types.
 */
#define LOG_AMA(logLevel, layer_type, logMessage, ...) \
  do { \
    if (log_ama_priv_GetLogFilter(layer_type) >= logLevel) \
      log_ama_priv_Message(__FILE__, __LINE__, logLevel, layer_type, logMessage, ##__VA_ARGS__); \
  } while (0)
#define LOG_AMA_EMERG(layer_type, logMessage, ...) \
  do { \
    if (log_ama_priv_GetLogFilter(layer_type) >= LOG_AMA_LEVEL_EMERG) \
      log_ama_priv_MessageEMERG(__FILE__, __LINE__, layer_type, logMessage, ##__VA_ARGS__); \
  } while (0)
#define LOG_AMA_FATAL(layer_type, logMessage, ...) \
  do { \
    if (log_ama_priv_GetLogFilter(layer_type) >= LOG_AMA_LEVEL_FATAL) \
      log_ama_priv_MessageFATAL(__FILE__, __LINE__, layer_type, logMessage, ##__VA_ARGS__); \
  } while (0)
#define LOG_AMA_ERROR(layer_type, logMessage, ...) \
  do { \
    if (log_ama_priv_GetLogFilter(layer_type) >= LOG_AMA_LEVEL_ERROR) \
      log_ama_priv_MessageERROR(__FILE__, __LINE__, layer_type, logMessage, ##__VA_ARGS__); \
  } while (0)
#define LOG_AMA_WARN(layer_type, logMessage, ...) \
  do { \
    if (log_ama_priv_GetLogFilter(layer_type) >= LOG_AMA_LEVEL_WARN) \
      log_ama_priv_MessageWARN(__FILE__, __LINE__, layer_type, logMessage, ##__VA_ARGS__); \
  } while (0)
#define LOG_AMA_INFO(layer_type, logMessage, ...) \
  do { \
    if (log_ama_priv_GetLogFilter(layer_type) >= LOG_AMA_LEVEL_INFO) \
      log_ama_priv_MessageINFO(__FILE__, __LINE__, layer_type, logMessage, ##__VA_ARGS__); \
  } while (0)
#define LOG_AMA_DEBUG(layer_type, logMessage, ...) \
  do { \
    if (log_ama_priv_GetLogFilter(layer_type) >= LOG_AMA_LEVEL_DEBUG) \
      log_ama_priv_MessageDEBUG(__FILE__, __LINE__, layer_type, logMessage, ##__VA_ARGS__); \
  } while (0)
#define LOG_AMA_TRACE(layer_type, logMessage, ...) \
  do { \
    if (log_ama_priv_GetLogFilter(layer_type) >= LOG_AMA_LEVEL_TRACE) \
      log_ama_priv_MessageTRACE(__FILE__, __LINE__, layer_type, logMessage, ##__VA_ARGS__); \
  } while (0)
#define LOG_AMA_ALL(layer_type, logMessage, ...) \
  do { \
    if (log_ama_priv_GetLogFilter(layer_type) >= LOG_AMA_LEVEL_ALL) \
      log_ama_priv_MessageALL(__FILE__, __LINE__, layer_type, logMessage, ##__VA_ARGS__); \
  } while (0)

//Performance logs
#define LOG_AMA_PERF_INFO_START(layer_type, frameNumber, format, ...) \
  do { \
    if (log_ama_priv_GetLogFilterPerf(layer_type) >= LOG_AMA_LEVEL_INFO) \
      log_ama_priv_PerfINFO(layer_type, 0, frameNumber, format, ##__VA_ARGS__); \
  } while (0)
#define LOG_AMA_PERF_INFO_STOP(layer_type, frameNumber, format, ...) \
  do { \
    if (log_ama_priv_GetLogFilterPerf(layer_type) >= LOG_AMA_LEVEL_INFO) \
      log_ama_priv_PerfINFO(layer_type, 1, frameNumber, format, ##__VA_ARGS__); \
  } while (0)
#define LOG_AMA_PERF_DEBUG_START(layer_type, frameNumber, format, ...) \
  do { \
    if (log_ama_priv_GetLogFilterPerf(layer_type) >= LOG_AMA_LEVEL_DEBUG) \
      log_ama_priv_PerfDEBUG(layer_type, 0, frameNumber, format, ##__VA_ARGS__); \
  } while (0)
#define LOG_AMA_PERF_DEBUG_STOP(layer_type, frameNumber, format, ...) \
  do { \
    if (log_ama_priv_GetLogFilterPerf(layer_type) >= LOG_AMA_LEVEL_DEBUG) \
      log_ama_priv_PerfDEBUG(layer_type, 1, frameNumber, format, ##__VA_ARGS__); \
  } while (0)

// Instance management
#define LOG_AMA_GET_INSTANCE_INFO(accel_type, instance_name) instance_name = log_ama_priv_GetInstanceInfo(&accel_type)
#define LOG_AMA_SET_AND_SAVE_INSTANCE_INFO(accel_type, p_inst_name) \
  log_ama_priv_LogAmaInstanceInfo l_log_ama_instance_info = log_ama_priv_SetAndGetInstanceInfo(accel_type, p_inst_name)
#define LOG_AMA_RESTORE_INSTANCE_INFO() log_ama_priv_RestoreInstanceInfo(l_log_ama_instance_info)

// Query applicable log level for current layer
#define LOG_AMA_GET_LOG_LEVEL(layer_type) log_ama_priv_GetLogFilter(layer_type)

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
class LogAmaInstanceMgmt {
private:
  log_ama_priv_LogAmaInstanceInfo m_log_ama_instance_info;

public:
  LogAmaInstanceMgmt(LogAmaAccel accel_type, const char* p_inst_name) : m_log_ama_instance_info(log_ama_priv_SetAndGetInstanceInfo(accel_type, p_inst_name)) {
  }
  ~LogAmaInstanceMgmt() { log_ama_priv_RestoreInstanceInfo(m_log_ama_instance_info); }

  LogAmaInstanceMgmt(const LogAmaInstanceMgmt&) = delete;
  LogAmaInstanceMgmt& operator=(const LogAmaInstanceMgmt&) = delete;
};

using log_ama_perf_callback_t = std::function<void(const char*, int, int)>;
void log_ama_set_perf_callbackINFO(const log_ama_perf_callback_t& Function);
void log_ama_set_perf_callbackDEBUG(const log_ama_perf_callback_t& Function);

#endif
