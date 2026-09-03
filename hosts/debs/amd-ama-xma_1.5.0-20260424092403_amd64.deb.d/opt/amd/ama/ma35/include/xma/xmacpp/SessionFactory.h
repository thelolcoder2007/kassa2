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
#include <functional>
#include "xma.h"

#include "Log.h"

#include "DecoderSession.h"
#include "ScalerSession.h"
#include "FilterSession.h"
#include "MioFilterSession.h"
#include "EncoderSession.h"

#include "SideData.h"
#include "Frame.h"
#include "DataBuffer.h"

namespace xma {
// Forward declaration

class SessionFactory : public std::enable_shared_from_this<SessionFactory> {
protected:
  SessionFactory() = default;

public:
  virtual ~SessionFactory() = default;

  // Factory for creating a sessions that contains a plugin
  virtual std::unique_ptr<DecoderSession> MakeDecoderSession(XmaDecoderProperties* Props)     = 0;
  virtual std::unique_ptr<ScalerSession>  MakeScalerSession(XmaScalerProperties* props)       = 0;
  virtual std::unique_ptr<FilterSession>  MakeFilterSession(XmaFilterProperties* Props)       = 0;
  virtual std::unique_ptr<EncoderSession> MakeEncoderSession(XmaEncoderProperties* props)     = 0;
  virtual MioFilterSessionPtr             MakeMioFilterSession(XmaMioFilterProperties* props) = 0;

  // useful functions
  virtual SideDataPtr        AllocSideData(enum XmaFrameSideDataType type, XmaBufferType buffer_type, uint64_t size)                                                = 0;
  virtual FramePropertiesPtr AllocFrameProperties(enum XmaFormatType format, enum XmaFormatType sw_format, int32_t width, int32_t height, uint32_t flags)           = 0;
  virtual FramePtr      AllocFrame(FramePropertiesPtr props, XmaFraction frame_rate = XMA_DEFAULT_FRAMERATE, int64_t pts = XMA_AV_NOPTS_VALUE, bool is_idr = false) = 0;
  virtual FramePtr      CloneFrame(FramePropertiesPtr props, uint8_t** frame_data, std::function<void(uint8_t**)> free_callback = nullptr,
           XmaFraction frame_rate = XMA_DEFAULT_FRAMERATE, int64_t pts = XMA_AV_NOPTS_VALUE, bool is_idr = false)                                                   = 0;
  virtual FramePtr      CloneFrame(FramePtr frame, XmaFraction frame_rate = XMA_DEFAULT_FRAMERATE, int64_t pts = XMA_AV_NOPTS_VALUE, bool is_idr = false)           = 0;
  virtual DataBufferPtr AllocDataBuffer(size_t size, bool dummy, int64_t pts = XMA_AV_NOPTS_VALUE, int64_t poc = -1)                                                = 0;
  virtual DataBufferPtr CloneDataBuffer(
      uint8_t* data, size_t size, std::function<void(uint8_t*)> free_callback = nullptr, int64_t pts = XMA_AV_NOPTS_VALUE, int64_t poc = -1) = 0;

  // Added in v1.2
  virtual DataBufferPtr AllocDataBuffer(size_t size, bool dummy, int64_t pts, int64_t dts, int64_t poc)                                                 = 0;
  virtual DataBufferPtr CloneDataBuffer(uint8_t* data, size_t size, std::function<void(uint8_t*)> free_callback, int64_t pts, int64_t dts, int64_t poc) = 0;

  // Added in v1.2
  virtual std::unique_ptr<FilterSession> MakeRotateSession(XmaRotateProperties* props)         = 0;
  virtual std::unique_ptr<FilterSession> MakeDrawBoxSession(XmaDrawBoxProperties* props)       = 0;
  virtual std::unique_ptr<FilterSession> MakeCscSession(XmaCscProperties* props)               = 0;
  virtual std::unique_ptr<FilterSession> MakeSubSampleSession(XmaSubSampleProperties* props)   = 0;
  virtual std::unique_ptr<FilterSession> MakeCropSession(XmaCropProperties* props)             = 0;
  virtual std::unique_ptr<FilterSession> MakePadSession(XmaPadProperties* props)               = 0;
  virtual MioFilterSessionPtr            MakeOverlaySession(XmaOverlayProperties* props)       = 0;
  virtual MioFilterSessionPtr            MakeTileSession(XmaTileProperties* props)             = 0;
  virtual MioFilterSessionPtr            MakeBlendSession(XmaBlendProperties* props)           = 0;
  virtual MioFilterSessionPtr            MakeCompositorSession(XmaCompositorProperties* props) = 0;

  static XmaAPIVersion                   GetLatestAPIVersion();
  static std::unique_ptr<SessionFactory> MakeSessionFactory(LogPtr log, XmaInitParameter* param, bool IsC = false);
};

using SessionFactoryPtr = std::shared_ptr<SessionFactory>;

}; // Namespace xma
