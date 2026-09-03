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
#include "SideData.h"

namespace xma {
class FrameProperties : public XmaFrameProperties {
protected:
  FrameProperties() = default;
  FrameProperties(const XmaFrameProperties& Other) { *static_cast<XmaFrameProperties*>(this) = Other; }

public:
  virtual ~FrameProperties() = default;

  enum XmaFormatType GetFormat() const { return format; }
  enum XmaFormatType GetSwFormat() const { return sw_format; }
  int32_t            GetWidth() const { return width; }
  int32_t            GetHeight() const { return height; }
  uint32_t           GetFlags() const { return flags; }

  virtual int32_t GetNumPlanes() const                = 0;
  virtual int32_t GetPlaneHeight(int32_t plane) const = 0;
  virtual int32_t GetPlaneStride(int32_t plane) const = 0;
  virtual int32_t GetPlaneSize(int32_t plane) const   = 0;
};

using FramePropertiesPtr = std::shared_ptr<FrameProperties>;

class Frame : public std::enable_shared_from_this<Frame> {
protected:
  void* m_userData = nullptr;

protected:
  Frame() = default;

public:
  virtual ~Frame() = default;

  virtual XmaBufferType      GetBufferType() const                = 0;
  virtual void*              GetBuffer(int32_t plane) const       = 0;
  virtual uint8_t*           GetHostBuffer(int32_t plane) const   = 0;
  virtual void*              GetDeviceBuffer(int32_t plane) const = 0;
  virtual FramePropertiesPtr GetProps() const                     = 0;
  virtual XmaFraction        GetFrameRate() const                 = 0;
  virtual int64_t            GetPts() const                       = 0;
  virtual int                IsIdr() const                        = 0;
  virtual void               SetFrameRate(XmaFraction frame_rate) = 0;
  virtual void               SetPts(int64_t pts)                  = 0;
  virtual void               SetIdr(bool is_idr)                  = 0;
  void                       SetUserData(void* user_data) { m_userData = user_data; }
  void*                      GetUserData() { return m_userData; }

  virtual int32_t     IncRef()                                           = 0;
  virtual int32_t     DecRef()                                           = 0;
  virtual int32_t     GetRefCount()                                      = 0;
  virtual int32_t     AddSideData(SideDataPtr side_data)                 = 0;
  virtual SideDataPtr GetSideData(enum XmaFrameSideDataType type)        = 0;
  virtual int32_t     RemoveSideData(SideDataPtr side_data)              = 0;
  virtual int32_t     RemoveSideDataType(enum XmaFrameSideDataType type) = 0;
  virtual void        ClearAllSideData()                                 = 0;
  virtual SideDataPtr GetFirstSideData()                                 = 0;
  virtual SideDataPtr GetNextSideData(SideDataPtr side_data)             = 0;
  virtual SideDataPtr GetNextSideDataOfSameType(SideDataPtr side_data)   = 0;

  // Added in v1.2
  virtual int32_t GetPlaneFD(int32_t plane) = 0;
  virtual int32_t GetBufferFD()             = 0;
};

using FramePtr = std::shared_ptr<Frame>;

}; // Namespace xma
