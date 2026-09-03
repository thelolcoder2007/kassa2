// Copyright(C) 2022 - 2025 Advanced Micro Devices, Inc. All rights reserved.
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

#include <array>
#include <cstdarg>
#include <memory>

#include "xma.h"
#include "xmalogger.h"

#if defined(TRACY_ENABLE)
#include <tracy/Tracy.hpp>
#endif

#if defined(NDEBUG)
#define XMA_LOG_MESSAGE_DEBUG(...)
#else
#define XMA_LOG_MESSAGE_DEBUG(LOG, MODULE, ...) XmaLogMsg(LOG, XMA_DEBUG_LOG, MODULE, ##__VA_ARGS__)
#endif

namespace xma {

class Log {
private:
  XmaLogLevelType m_level;
  XmaLogType      m_type;

  static constexpr std::array<const char*, XmaLogLevelType::XMA_LOG_LEVEL_LIST_COUNT> sm_logLevels = {
      "emergency", "alert", "critical", "error", "warning", "notice", "info", "debug"};
  static constexpr std::array<const char*, XmaLogType::XMA_LOG_TYPE_LIST_COUNT> sm_logTypes = {"none", "console", "syslog", "callback", "ama"};

protected:
  Log(XmaLogLevelType Level, XmaLogType Type) : m_level(Level), m_type(Type) {
  }

public:
  virtual ~Log() = default;

  void LogMsgImpl(XmaLogLevelType Level, const char* Name, const char* Msg, ...) {
    if (Level <= m_level) {
      va_list args;
      va_start(args, Msg);
      LogMsgList(Level, Name, Msg, args);
      va_end(args);
    }
  }

  XmaLogLevelType GetLogLevel() const { return m_level; }
  XmaLogType      GetLogType() const { return m_type; }
  const char*     GetLogLevelStr() const { return sm_logLevels[m_level]; }
  const char*     GetLogTypeStr() const { return sm_logTypes[m_type]; }

  virtual void LogMsgList(XmaLogLevelType Level, const char* Name, const char* Msg, va_list Args) = 0;

  static std::unique_ptr<Log> MakeLog(XmaLogLevelType Level, XmaLogType Type, ...);
  static std::unique_ptr<Log> MakeLogList(XmaLogLevelType Level, XmaLogType Type, va_list Args);
};

#define XmaLogMsg(LOG, LEVEL, MODULE, ...) \
  do { \
    if (LOG->GetLogType() != XMA_LOG_TYPE_NONE && LOG->GetLogLevel() >= LEVEL) { \
      (LOG)->LogMsgImpl(LEVEL, MODULE, __VA_ARGS__); \
    } \
  } while (0)

using LogPtr = Log*;

} // namespace xma
