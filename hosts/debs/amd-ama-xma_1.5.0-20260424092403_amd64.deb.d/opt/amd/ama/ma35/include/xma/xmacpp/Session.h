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

#include <variant>
#include <vector>

#include "xma.h"
#include "xmacpp/Log.h"
#include "xmacpp/Frame.h"
#include "xmacpp/DataBuffer.h"

namespace xma {

// Structures used to define the types of data queued to a plugin
using QueueFrame      = FramePtr;
using QueueFrameList  = std::vector<FramePtr>;
using QueueDataBuffer = DataBufferPtr;

using queue_t = std::variant<QueueFrame, QueueFrameList, QueueDataBuffer>;

class Session {
protected:
  xma::LogPtr m_log;

protected:
  Session() = default;

public:
  virtual ~Session() = default;

  xma::LogPtr GetLog() const { return m_log; }

  virtual int32_t SetLog(LogPtr log) = 0;

  virtual int32_t Enqueue(queue_t& data) = 0;
  virtual int32_t Dequeue(queue_t& data) = 0;
};

}; // Namespace xma
