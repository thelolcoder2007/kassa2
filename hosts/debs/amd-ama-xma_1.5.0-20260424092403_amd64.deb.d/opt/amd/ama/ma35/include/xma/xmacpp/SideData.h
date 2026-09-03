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
class SideData : public std::enable_shared_from_this<SideData> {
protected:
  SideData() = default;

public:
  virtual ~SideData() = default;

  virtual XmaFrameSideDataType GetType() const       = 0;
  virtual XmaBufferType        GetBufferType() const = 0;
  virtual bool                 IsOnHost() const      = 0;
  virtual uint8_t*             GetHostPtr() const    = 0;
  virtual uint64_t             GetDevicePtr() const  = 0;
  virtual uint64_t             GetSize() const       = 0;

  virtual int32_t Read()  = 0;
  virtual int32_t Write() = 0;

  virtual int32_t SetMetadata(const XmaParameter* metadata) = 0;
  virtual int32_t GetMetadata(XmaParameter* metadata) const = 0;
};

using SideDataPtr = std::shared_ptr<SideData>;

}; // Namespace xma
