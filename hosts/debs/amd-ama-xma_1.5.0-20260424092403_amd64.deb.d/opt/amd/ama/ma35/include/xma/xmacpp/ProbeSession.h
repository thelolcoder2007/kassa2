// Copyright(C) 2024 Advanced Micro Devices, Inc. All rights reserved.
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
#include "xmacpp/DataBuffer.h"
#include "xmacpp/Log.h"
#include "xmaprobe.h"

namespace xma {

// Forward declarations
class StreamInfo;
using StreamInfoPtr = std::shared_ptr<StreamInfo>;

/**
 * StreamInfo - C++ wrapper for XmaStreamInfo
 * 
 * Provides convenient access to stream information with type-safe getters
 * and conversion utilities.
 */
class StreamInfo {
public:
  StreamInfo();
  ~StreamInfo() = default;

  // Basic getters
  XmaDecoderType GetCodecType() const { return m_info.codec_type; }
  uint32_t       GetWidth() const { return m_info.width; }
  uint32_t       GetHeight() const { return m_info.height; }
  uint32_t       GetDisplayWidth() const { return m_info.display_width; }
  uint32_t       GetDisplayHeight() const { return m_info.display_height; }

  // Bit depth information
  XmaBitDepth GetBitDepth() const { return m_info.bit_depth; }
  uint32_t    GetRawBitDepth() const { return m_info.bit_depth_raw; }
  bool        Is10Bit() const { return m_info.bit_depth == XMA_10BIT; }
  bool        Is8Bit() const { return m_info.bit_depth == XMA_8BIT; }

  // Format information
  uint32_t      GetChromaFormat() const { return m_info.chroma_format; }
  XmaFormatType GetPixelFormat() const { return m_info.pixel_format; }
  XmaFormatType GetPixelFormat8Bit() const { return m_info.pixel_format_8bit; }
  XmaFormatType GetPixelFormat10Bit() const { return m_info.pixel_format_10bit; }

  // Hardware compatibility
  XmaStreamCompatibility GetHwCompatibility() const { return m_info.hw_compatibility; }
  bool     IsHardwareCompatible() const { return m_info.hw_compatibility == XMA_STREAM_COMPATIBLE || m_info.hw_compatibility == XMA_STREAM_NEEDS_CONVERSION; }
  uint32_t GetHwWarnings() const { return m_info.hw_warnings; }

  // Frame rate
  XmaFraction GetFrameRate() const { return m_info.frame_rate; }
  double      GetFrameRateAsDouble() const {
    if (m_info.frame_rate.denominator == 0)
      return 0.0;
    return static_cast<double>(m_info.frame_rate.numerator) / static_cast<double>(m_info.frame_rate.denominator);
  }

  // HDR information
  bool IsHDR() const { return m_info.hdr_present != 0; }
  bool IsHDR10() const { return m_info.is_hdr10 != 0; }
  bool IsDolbyVision() const { return m_info.is_dolby_vision != 0; }

  // Performance hints
  uint32_t GetEstimatedDecodeLoad() const { return m_info.estimated_decode_load; }
  uint32_t GetRecommendedBuffers() const { return m_info.recommended_buffers; }

  // Validity checks
  bool     IsValid() const { return (m_info.valid_fields & XMA_INFO_VALID_BASIC) != 0; }
  uint32_t GetValidFields() const { return m_info.valid_fields; }

  // Convert to C struct
  void CopyTo(XmaStreamInfo* info) const;

  // Get internal structure (for internal use)
  XmaStreamInfo&       GetInternal() { return m_info; }
  const XmaStreamInfo& GetInternal() const { return m_info; }

private:
  friend class InternalProbeSession;
  XmaStreamInfo m_info{};
};

/**
 * ProbeSession - Abstract base class for stream probing
 * 
 * Provides interface for analyzing encoded video streams to extract
 * codec parameters without full decoder initialization.
 */
class ProbeSession {
public:
  virtual ~ProbeSession() = default;

  /**
     * SendData - Send encoded data for probing
     * 
     * @param data      Input data buffer containing encoded stream
     * @param data_used Output: number of bytes consumed
     * @return XMA_SUCCESS when headers found, XMA_SEND_MORE_DATA if more needed
     */
  virtual int32_t SendData(DataBufferPtr data, int32_t* data_used) = 0;

  /**
     * GetStreamInfo - Get probed stream information
     * 
     * @return Stream information if available, nullptr if more data needed
     */
  virtual StreamInfoPtr GetStreamInfo() = 0;

  /**
     * ProbeStream - Single-shot probe of a data buffer
     * 
     * @param data Input data buffer
     * @param info Output stream information
     * @return XMA_SUCCESS on success, error code otherwise
     */
  virtual int32_t ProbeStream(DataBufferPtr data, StreamInfoPtr& info) = 0;

  /**
     * SetLog - Set logging handle
     * 
     * @param log Logging handle
     * @return XMA_SUCCESS on success
     */
  virtual int32_t SetLog(LogPtr log) = 0;

  /**
     * GetLog - Get current logging handle
     * 
     * @return Current log handle
     */
  virtual LogPtr GetLog() const = 0;

protected:
  ProbeSession() = default;
};

using ProbeSessionPtr = std::unique_ptr<ProbeSession>;

} // namespace xma
