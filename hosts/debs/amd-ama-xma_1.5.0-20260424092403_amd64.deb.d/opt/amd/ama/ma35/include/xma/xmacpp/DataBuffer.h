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

#include <memory>
#include "xma.h"

namespace xma {
class DataBuffer : public std::enable_shared_from_this<DataBuffer> {
protected:
  DataBuffer() = default;

public:
  virtual ~DataBuffer() = default;

  virtual uint8_t* GetBuffer() const = 0;
  virtual int32_t  GetSize() const   = 0;
  virtual int64_t  GetPts() const    = 0;
  virtual int64_t  GetPoc() const    = 0;

  // Added in v1.2
  virtual int64_t GetDts() const = 0;
};

using DataBufferPtr = std::shared_ptr<DataBuffer>;

}; // Namespace xma
