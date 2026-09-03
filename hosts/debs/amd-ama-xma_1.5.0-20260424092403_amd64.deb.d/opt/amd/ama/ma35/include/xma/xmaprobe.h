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

#include "xmabuffers.h"
#include "xmadecoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * DOC: XMA Stream Probing API
 * 
 * The XMA probe API provides functionality to analyze encoded video streams
 * and extract codec parameters without creating a full decoder session.
 * This is useful for validating stream compatibility, extracting display
 * parameters, and determining optimal decoding configuration.
 */

/**
 * enum XmaBitDepth - Supported bit depths for hardware acceleration
 */
typedef enum XmaBitDepth {
  XMA_8BIT  = 8,  /**< 8-bit video (standard dynamic range) */
  XMA_10BIT = 10, /**< 10-bit video (high dynamic range) */
} XmaBitDepth;

/**
 * enum XmaStreamCompatibility - Hardware compatibility status
 */
typedef enum XmaStreamCompatibility {
  XMA_STREAM_COMPATIBLE = 0,        /**< Fully compatible with HW acceleration */
  XMA_STREAM_NEEDS_CONVERSION,      /**< Compatible after format conversion */
  XMA_STREAM_INCOMPATIBLE_BITDEPTH, /**< Incompatible bit depth (not 8 or 10) */
  XMA_STREAM_INCOMPATIBLE_CODEC,    /**< Codec not supported by hardware */
  XMA_STREAM_INCOMPATIBLE_PROFILE,  /**< Profile not supported by hardware */
  XMA_STREAM_INCOMPATIBLE_OTHER,    /**< Other incompatibility */
} XmaStreamCompatibility;

/**
 * struct XmaStreamInfo - Detailed information about an encoded stream
 */
typedef struct XmaStreamInfo {
  // Basic stream identification
  XmaDecoderType codec_type;    /**< H264, HEVC, VP9, AV1, etc. */
  uint32_t       codec_profile; /**< Profile (e.g., Main, High, etc.) */
  uint32_t       codec_level;   /**< Level */

  // Video dimensions
  uint32_t width;          /**< Coded picture width */
  uint32_t height;         /**< Coded picture height */
  uint32_t display_width;  /**< Display width after cropping */
  uint32_t display_height; /**< Display height after cropping */

  // Cropping information
  struct {
    uint32_t left;
    uint32_t right;
    uint32_t top;
    uint32_t bottom;
  } crop_params;

  // Bit depth information (simplified for hardware constraints)
  XmaBitDepth bit_depth;     /**< Bit depth: 8 or 10 (for both luma and chroma) */
  uint32_t    bit_depth_raw; /**< Raw bit depth from stream (may be unsupported) */

  // Pixel format information
  uint32_t      chroma_format;      /**< 0=400(mono), 1=420, 2=422, 3=444 */
  XmaFormatType pixel_format;       /**< Recommended output format based on bit depth */
  XmaFormatType pixel_format_8bit;  /**< Recommended 8-bit format if conversion needed */
  XmaFormatType pixel_format_10bit; /**< Recommended 10-bit format if conversion needed */

  // Hardware compatibility
  XmaStreamCompatibility hw_compatibility; /**< Hardware acceleration compatibility */
  uint32_t               hw_warnings;      /**< Bitmask of potential issues */

  // Frame rate and timing
  XmaFraction frame_rate;        /**< Frame rate as fraction */
  uint32_t    time_scale;        /**< Time scale */
  uint32_t    num_units_in_tick; /**< Timing info */

  // Aspect ratio
  uint32_t sar_width;  /**< Sample aspect ratio width */
  uint32_t sar_height; /**< Sample aspect ratio height */

  // Video characteristics
  uint32_t interlaced;  /**< 0=progressive, 1=interlaced */
  uint32_t field_order; /**< Top field first or bottom field first */

  // Color information
  uint32_t video_format;             /**< Video format (component, PAL, NTSC, etc.) */
  uint32_t video_full_range;         /**< 0=limited range, 1=full range */
  uint32_t color_primaries;          /**< Color primaries (BT.709, BT.2020, etc.) */
  uint32_t transfer_characteristics; /**< Transfer characteristics (SDR, HDR10, HLG, etc.) */
  uint32_t matrix_coefficients;      /**< Matrix coefficients */

  // HDR information
  uint32_t hdr_present;     /**< HDR metadata present */
  uint32_t is_hdr10;        /**< Stream is HDR10 (10-bit with HDR metadata) */
  uint32_t is_dolby_vision; /**< Stream has Dolby Vision metadata */
  struct {
    uint32_t max_display_mastering_luminance;
    uint32_t min_display_mastering_luminance;
    uint16_t display_primaries_x[3];
    uint16_t display_primaries_y[3];
    uint16_t white_point_x;
    uint16_t white_point_y;
    uint32_t max_content_light_level;
    uint32_t max_frame_average_light_level;
  } hdr_params;

  // Stream structure
  uint32_t max_ref_frames; /**< Maximum reference frames */
  uint32_t max_dpb_size;   /**< Maximum DPB size */
  uint32_t reorder_frames; /**< Number of reorder frames */

  // Performance hints
  uint32_t estimated_decode_load; /**< Estimated % of decoder capacity needed */
  uint32_t recommended_buffers;   /**< Recommended number of output buffers */

  // Codec-specific information
  union {
    struct {
      uint32_t poc_type; /**< Picture order count type */
      uint32_t log2_max_frame_num;
      uint32_t direct_8x8_inference;
      uint32_t mvc_enabled;          /**< MVC (3D) enabled */
      uint32_t svc_enabled;          /**< SVC enabled */
      uint32_t constrained_baseline; /**< Constrained baseline profile */
    } h264;

    struct {
      uint32_t max_sub_layers;
      uint32_t temporal_id_nesting;
      uint32_t tiles_enabled;
      uint32_t wpp_enabled; /**< Wavefront parallel processing */
      uint32_t sao_enabled; /**< Sample adaptive offset */
    } hevc;

    struct {
      uint32_t profile;
      uint32_t show_existing_frame;
      uint32_t color_space;
    } vp9;

    struct {
      uint32_t operating_points_cnt;
      uint32_t film_grain_params_present;
      uint32_t still_picture;
    } av1;
  } codec_specific;

  // Extended information
  uint32_t stream_format;     /**< 0=Annex B, 1=AVCC/HVCC, 2=Raw NAL, 3=IVF */
  uint32_t estimated_bitrate; /**< Estimated bitrate in kbps */
  uint32_t valid_fields;      /**< Bitmask of valid fields */

  uint32_t reserved[12]; /**< Reserved for future use */
} XmaStreamInfo;

// Hardware warning flags for hw_warnings field
#define XMA_HW_WARN_HIGH_BITRATE 0x0001     /**< Bitrate may exceed HW capacity */
#define XMA_HW_WARN_HIGH_RESOLUTION 0x0002  /**< Resolution at upper limit */
#define XMA_HW_WARN_HIGH_FRAMERATE 0x0004   /**< Frame rate may be challenging */
#define XMA_HW_WARN_CHROMA_FORMAT 0x0008    /**< Chroma format may need conversion */
#define XMA_HW_WARN_PROFILE_COMPLEX 0x0010  /**< Complex profile features */
#define XMA_HW_WARN_REFERENCE_FRAMES 0x0020 /**< Many reference frames */

// Valid fields flags
#define XMA_INFO_VALID_BASIC 0x0001   /**< Basic info (dimensions, codec) valid */
#define XMA_INFO_VALID_TIMING 0x0002  /**< Frame rate and timing valid */
#define XMA_INFO_VALID_COLOR 0x0004   /**< Color space information valid */
#define XMA_INFO_VALID_HDR 0x0008     /**< HDR metadata valid */
#define XMA_INFO_VALID_PROFILE 0x0010 /**< Profile/level information valid */
#define XMA_INFO_VALID_ALL 0xFFFF     /**< All information valid */

/**
 * struct XmaProbeProperties - Properties for stream probing
 */
typedef struct XmaProbeProperties {
  XmaHandle      handle;           /**< XMA device handle */
  XmaDecoderType codec_hint;       /**< Optional codec type hint (0=auto-detect) */
  uint32_t       max_probe_size;   /**< Maximum bytes to probe (0=default 1MB) */
  uint32_t       flags;            /**< Probe flags */
  uint32_t       target_bit_depth; /**< Preferred bit depth if conversion needed (8 or 10) */
  uint32_t       reserved[3];
} XmaProbeProperties;

// Probe flags
#define XMA_PROBE_FLAG_QUICK 0x01              /**< Quick probe, may return less info */
#define XMA_PROBE_FLAG_DEEP 0x02               /**< Deep probe, parse more data */
#define XMA_PROBE_FLAG_AUTO_CODEC 0x04         /**< Auto-detect codec type */
#define XMA_PROBE_FLAG_CHECK_HW_COMPAT 0x08    /**< Check hardware compatibility */
#define XMA_PROBE_FLAG_SUGGEST_CONVERSION 0x10 /**< Suggest format conversion if needed */

/* Forward declaration */
typedef void* XmaProbeSession;

/**
 * xma_probe_stream() - Probe an encoded stream for parameters
 * 
 * This function analyzes an encoded stream to extract codec parameters
 * without creating a full decoder session. It's useful for:
 * - Validating stream compatibility
 * - Extracting display parameters
 * - Getting codec configuration
 * 
 * @probe_props: Probe configuration
 * @data:        Input stream data to probe
 * @stream_info: Output structure to receive stream information
 * 
 * RETURN: XMA_SUCCESS on successful probe
 *         XMA_SEND_MORE_DATA if more data needed for complete probe
 *         XMA_ERROR_INVALID on invalid parameters
 *         XMA_ERROR_STREAM_NOT_SUPPORTED if stream type not recognized
 *         XMA_ERROR on other errors
 */
int32_t xma_probe_stream(XmaProbeProperties* probe_props, XmaDataBuffer* data, XmaStreamInfo* stream_info);

/**
 * xma_probe_session_create() - Create a persistent probe session
 * 
 * For scenarios where multiple buffers need to be analyzed to get
 * complete stream information (e.g., when headers span multiple packets)
 * 
 * @probe_props: Probe configuration
 * 
 * RETURN: Handle to probe session on success
 *         NULL on failure
 */
XmaProbeSession xma_probe_session_create(XmaProbeProperties* probe_props);

/**
 * xma_probe_session_send_data() - Send data to probe session
 * 
 * @session:     Probe session handle
 * @data:        Input data buffer
 * @data_used:   Output: bytes consumed from input
 * 
 * RETURN: XMA_SUCCESS when enough data received
 *         XMA_SEND_MORE_DATA if more data needed
 *         XMA_ERROR on error
 */
int32_t xma_probe_session_send_data(XmaProbeSession session, XmaDataBuffer* data, int32_t* data_used);

/**
 * xma_probe_session_get_info() - Get stream info from probe session
 * 
 * @session:     Probe session handle
 * @stream_info: Output structure for stream information
 * 
 * RETURN: XMA_SUCCESS on success
 *         XMA_ERROR_INVALID if not enough data probed yet
 *         XMA_ERROR on error
 */
int32_t xma_probe_session_get_info(XmaProbeSession session, XmaStreamInfo* stream_info);

/**
 * xma_probe_session_destroy() - Destroy probe session
 * 
 * @session: Probe session to destroy
 */
void xma_probe_session_destroy(XmaProbeSession session);

/**
 * xma_probe_codec_detect() - Quick codec type detection
 * 
 * Lightweight function to just identify codec type from stream headers
 * 
 * @data:       Input stream data (at least 32 bytes recommended)
 * @codec_type: Output codec type
 * 
 * RETURN: XMA_SUCCESS on successful detection
 *         XMA_SEND_MORE_DATA if more data needed
 *         XMA_ERROR on error
 */
int32_t xma_probe_codec_detect(XmaDataBuffer* data, XmaDecoderType* codec_type);

/**
 * xma_probe_check_hw_compatibility() - Check if stream is compatible with hardware
 * 
 * Quick check to determine if a stream can be hardware accelerated
 * 
 * @stream_info: Stream information from probe
 * @handle:      XMA device handle to check against
 * 
 * RETURN: XmaStreamCompatibility enum value
 */
XmaStreamCompatibility xma_probe_check_hw_compatibility(const XmaStreamInfo* stream_info, XmaHandle handle);

/**
 * xma_probe_suggest_format() - Suggest best pixel format for stream
 * 
 * Based on stream bit depth and hardware capabilities, suggest the
 * optimal pixel format for decoding
 * 
 * @stream_info:    Stream information from probe
 * @prefer_10bit:   If true, prefer 10-bit formats when possible
 * 
 * RETURN: Suggested XmaFormatType
 */
XmaFormatType xma_probe_suggest_format(const XmaStreamInfo* stream_info, bool prefer_10bit);

#ifdef __cplusplus
}
#endif
