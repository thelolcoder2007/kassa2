/*
 * Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Xilinx Resouce Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

/**
 * @file xrm_winlog.h
 * @brief Header for Public APIs of XRM.
 */

#ifndef _XRM_WINLOG_H_
#define _XRM_WINLOG_H_


#ifdef __cplusplus
extern "C" {
#endif

// XRM code comes with two different logging mechanisms, and uses syslog directly.
// In src/daemon/xrm_system.hpp you'll find xrm::system::logMsg. This requires an xrm::system class object, which
// keeps track of a log level and uses that to decide if things should be logged. Ultimately this ends up calling
// syslog. In src/lib/xrm.cpp there is a static xrmLog function that takes a context log level and message log level
// to decide if logging should be done. It also ends up calling syslog.
// 
// There is no syslog on Windows, and the equivalent is to create an event in the event log.
// There is some information, not 100% applicable because it is for a "managed application" at
// https://learn.microsoft.com/en-us/troubleshoot/developer/visualstudio/cpp/language-compilers/write-entry-to-event-log
// The key elements are to create an event source with RegisterEventSource, then call ReportEvent, and eventually
// call DeregisterEvenSource. Applications can be a source, and if we end up running as a service, then that might
// make us an event source.
// 
// In the mean time, so that code will compile, we'll adopt the convention found elsewhere in the stack,
// e.g. ma35_xma/lib/XmaLog.cpp, which is to do nothing for now on Windows.
//
// Another alternative would be to change the code to use a different logging mechanism, e.g. LogAma.

typedef enum xrmWinLogLevelType {
    LOG_EMERG = 0,
    LOG_ALERT = 1,
    LOG_CRIT = 2,
    LOG_ERR = 3,
    LOG_WARNING = 4,
    LOG_NOTICE = 5,
    LOG_INFO = 6,
    LOG_DEBUG = 7
} xrmWinLogLevelType;

typedef enum xrmWinLogOpenFlag {
    LOG_PID	   = 0x01,
    LOG_CONS   = 0x02,
    LOG_ODELAY = 0x04,
    LOG_NDELAY = 0x08,
    LOG_NOWAIT = 0x10,
    LOG_PERROR = 0x20,
} xrmWinLogOpenFlag;

// syslog.h often provides a convenience macro LOG_UPTO, which is a simple one-liner.
// But if we copy that, does that throw out our Apache license and/or subject us to
// a University of California license? We'll just do nothing for now.
#define LOG_UPTO

#define	LOG_LOCAL1 136

inline void syslog(int priority, const char* format, ...) {
    priority; // To avoid warnings/errors for unreferenced formal parameter.
    format;
}


#ifdef __cplusplus
}
#endif

#endif // _XRM_WINLOG_H_
