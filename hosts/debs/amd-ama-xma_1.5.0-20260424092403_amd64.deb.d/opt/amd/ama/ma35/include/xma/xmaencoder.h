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

#include "xmabuffers.h"
#include "xmaparam.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * enum XmaEncoderType - Identifier specifying precise type of video encoder
 * during session creation
*/
typedef enum XmaEncoderType {
  XMA_H264_ENCODER_TYPE = 1,      /**< 1 */
  XMA_HEVC_ENCODER_TYPE,          /**< 2 */
  XMA_JPEG_ENCODER_TYPE,          /**< 3 */
  XMA_AV1_ENCODER_TYPE,           /**< 4 */
  XMA_LOSSLESS_JPEG_ENCODER_TYPE, /**< 5 */
} XmaEncoderType;

/**
 * struct XmaEncoderProperties - Properties necessary for specifying which
 * encoder to select and how it should be configured by the plugin.
 *
*/
typedef struct XmaEncoderProperties {
  XmaEncoderType hwencoder_type;  /**< specifying type of encoder to reserve;
                                       @see XmaEncoderType */
  XmaHandle      handle;          /**< handle to XMA device */
  XmaFormatType  format;          /**< host side video format */
  XmaFormatType  sw_format;       /**< if data is on device, this specifies the
                                       pixel format on device side */
  int32_t        bits_per_pixel;  /**< not used, use format and sw_format */
  int32_t        width;           /**< width in pixels of incoming video
                                       stream */
  int32_t        height;          /**< height in pixels of incoming video
                                       stream */
  XmaFraction    timebase;        /**< not used */
  XmaFraction    framerate;       /**< framerate data structure specifying
                                       frame rate per second */
  int32_t        bitrate;         /**< fixed bitrate requested for output,
                                       default=-1(not used) */
  int32_t        qp;              /**< fixed quantization value, 0-63,
                                       default=-1(not used) */
  int32_t        gop_size;        /**< group-of-pictures size in frames */
  int32_t        idr_interval;    /**< interval between idr frame insertion */
  int32_t        lookahead_depth; /**< maximum number of input frames to
                                       request to identify video complexity to
                                       calculate bitrate */
  int32_t        qp_offset_I;     /**< not used */
  int32_t        qp_offset_B0;    /**< not used */
  int32_t        qp_offset_B1;    /**< not used */
  int32_t        qp_offset_B2;    /**< not used */
  int32_t        temp_aq_gain;    /**< temporal AQ gain, 0-100, default=50 */
  int32_t        spat_aq_gain;    /**< spatial AQ gain, 0-100, default=50 */
  int32_t        aq_mode;         /**< not used */
  int32_t        minQP;           /**< minimum QP, 0-63, default=0 */
  int32_t        maxQP;           /**< maximum QP, 0-63, default=63 */
  int32_t        rc_mode;         /**< rate control mode, 0=constant QP, 1=CBR,
                                       2=VBR, 3=CVBR, 4=CRF, default=CBR */
  int32_t        la_depth;        /**< not used */
  int32_t        force_param;     /**< not used */
  int32_t        preset;          /**< preset */
  int32_t        profile;         /**< profile */
  int32_t        level;           /**< level */
  XmaParameter*  params;          /**< array of custom parameters */
  uint32_t       param_cnt;       /**< count of custom parameters */
  uint32_t       flags;           /**< input flags, must be a combination
                                       of XMA_FRAME_PROPERTY_FLAG_xxx */
  int32_t        reserved[3];
} XmaEncoderProperties;

/* Use a background thread to improve processing on host. Default is 0, added
 * in API v1.1. */
#define XMA_ENC_PARAM_THREADS "threads" // XMA_INT32

/* If XMA_ENC_PARAM_THREADS is 1, xma_enc_session_send_frame() waits for
 * internal buffers to have room if necessary before sending the frame to the
 * device and returning instead of returning XMA_TRY_AGAIN.
 * xma_enc_session_recv_data() will wait for encoded data or end of stream
 * before returning. Default is 0, added in API v1.1. */
#define XMA_ENC_PARAM_WAIT "wait" // XMA_INT32

/* Preset */
typedef enum XmaEncoderPreset {
  XMA_ENC_PRESET_SUPER_FAST, // not yet supported
  XMA_ENC_PRESET_FAST,       // added in API v1.1
  XMA_ENC_PRESET_MEDIUM,
  XMA_ENC_PRESET_SLOW,
  XMA_ENC_PRESET_SUPER_SLOW, // not yet supported
  XMA_ENC_PRESET_DEFAULT = XMA_ENC_PRESET_MEDIUM
} XmaEncoderPreset;

/* Profile */
typedef enum XmaEncoderProfile {
  XMA_ENC_PROFILE_AUTO = -1,

  XMA_ENC_PROFILE_H264_BASELINE = 0,
  XMA_ENC_PROFILE_H264_MAIN,
  XMA_ENC_PROFILE_H264_HIGH,
  XMA_ENC_PROFILE_H264_HIGH10,
  XMA_ENC_PROFILE_H264_HIGH10_INTRA,

  XMA_ENC_PROFILE_HEVC_MAIN = 100,
  XMA_ENC_PROFILE_HEVC_MAIN_INTRA,
  XMA_ENC_PROFILE_HEVC_MAIN10,
  XMA_ENC_PROFILE_HEVC_MAIN10_INTRA,

  XMA_ENC_PROFILE_AV1_MAIN = 200,
} XmaEncoderProfile;

/* Device type */
#define XMA_ENC_PARAM_DEVICE_TYPE "device_type" // XMA_INT32
typedef enum XmaEncoderDeviceType { XMA_ENC_DEVICE_TYPE_ANY = 0, XMA_ENC_DEVICE_TYPE_1, XMA_ENC_DEVICE_TYPE_2 } XmaEncoderDeviceType;

/* Rate control mode */
typedef enum XmaEncoderRCMode {
  XMA_ENC_RC_MODE_AUTO = -1,
  XMA_ENC_RC_MODE_CONSTANT_QP,
  XMA_ENC_RC_MODE_CBR,
  XMA_ENC_RC_MODE_VBR,
  XMA_ENC_RC_MODE_CVBR,
  XMA_ENC_RC_MODE_CABR,
  XMA_ENC_RC_MODE_CRF, // added in API v1.2
  XMA_ENC_RC_MODE_DEFAULT = XMA_ENC_RC_MODE_AUTO
} XmaEncoderRCMode;

#define XMA_ENC_PARAM_CABR_CONFIG "cabr" // XMA_STRING, added in API v1.2
#define XMA_ENC_PARAM_CABR_CONFIG_DEFAULT "auto"
#define XMA_ENC_PARAM_CABR_CONFIG_DISABLE "disable"

#define XMA_ENC_PARAM_SLICE "slice" // XMA_INT32
typedef enum XmaEncoderSlice { XMA_ENC_SLICE_DEFAULT = -1, XMA_ENC_SLICE_0, XMA_ENC_SLICE_1 } XmaEncoderSlice;

#define XMA_ENC_PARAM_CORES "cores" // XMA_INT32, added in API v1.1
typedef enum XmaEncoderCores { XMA_ENC_CORES_1 = 1, XMA_ENC_CORES_2, XMA_ENC_CORES_DEFAULT = XMA_ENC_CORES_1 } XmaEncoderCores;

#define XMA_ENC_SLICE_MIN_VALUE (-1)

/* Maximum bitrate : It is 64 bit value */
#define XMA_ENC_PARAM_MAX_BITRATE "max_bitrate" // XMA_INT64
#define XMA_ENC_PARAM_MAX_BITRATE_DEFAULT (-1)
#define XMA_ENC_MAX_BITRATE_MIN_VALUE (-1)
#define XMA_ENC_MAX_BITRATE_MAX_VALUE 35000000000

/* Minimum bitrate : It is 64 bit value added in v1.1.2 */
#define XMA_ENC_PARAM_MIN_BITRATE "min_bitrate" // XMA_INT64
#define XMA_ENC_PARAM_MIN_BITRATE_DEFAULT (-1)
#define XMA_ENC_MIN_BITRATE_MIN_VALUE (-1)
#define XMA_ENC_MIN_BITRATE_MAX_VALUE 35000000000

#define XMA_ENC_PARAM_EXPERT_OPTIONS "expert_options" // XMA_STRING

/* crf */
#define XMA_ENC_PARAM_CRF "crf" // XMA_INT32
#define XMA_ENC_PARAM_CRF_DEFAULT (-1)
#define XMA_ENC_CRF_MIN_VALUE (-1)
#define XMA_ENC_CRF_MAX_VALUE (63)
#define XMA_ENC_PARAM_CRF_DISABLE (0) // deprecated in API v1.2
#define XMA_ENC_PARAM_CRF_ENABLE (1)  // deprecated in API v1.2

/* Bit rate*/
#define XMA_ENC_BITRATE_DEFAULT (0)

/* qp value */
#define XMA_ENC_QP_DEFAULT (-1)
#define XMA_ENC_MAX_QP_VALUE (63)

/* gop size */
#define XMA_ENC_GOP_SIZE_DEFAULT (-1)

/* lookahead depth */
#define XMA_ENC_LOOKAHEAD_DEPTH_DEFAULT (-1)
#define XMA_ENC_MAX_LOOKAHEAD_DEPTH (53)

/* spatial aq gain value */
#define XMA_ENC_MAX_SPATIAL_AQ_GAIN (255)
#define XMA_ENC_SPATIAL_AQ_GAIN_DEFAULT (255)

/* Temporal aq gain value */
#define XMA_ENC_MAX_TEMPORAL_AQ_GAIN (255)
#define XMA_ENC_TEMPORAL_AQ_GAIN_DEFAULT (255)

/*  min and max qp default value */
#define XMA_ENC_MIN_QP_DEFAULT (-1)
#define XMA_ENC_MAX_QP_DEFAULT (-1)
#define XMA_ENC_QP_MIN_VALUE (-1)

/*  min and max qp default value */
#define XMA_ENC_MIN_QP_MAX_VALUE (63)

/*  min and max qp default value */
#define XMA_ENC_MAX_QP_MAX_VALUE (63)

/* qp i offset added in v1.1.2 */
#define XMA_ENC_PARAM_QP_I_OFFSET "qp_i_offset" // XMA_INT32
#define XMA_ENC_PARAM_QP_I_OFFSET_DEFAULT (-64)
#define XMA_ENC_QP_I_OFFSET_MIN_VALUE (-64)
#define XMA_ENC_QP_I_OFFSET_MAX_VALUE (63)

/* qp b offset added in v1.1.2 */
#define XMA_ENC_PARAM_QP_B_OFFSET "qp_b_offset" // XMA_INT32
#define XMA_ENC_PARAM_QP_B_OFFSET_DEFAULT (-64)
#define XMA_ENC_QP_B_OFFSET_MIN_VALUE (-64)
#define XMA_ENC_QP_B_OFFSET_MAX_VALUE (63)

/* Forced IDR */
#define XMA_ENC_PARAM_FORCED_IDR "forced_idr" // XMA_INT32
#define XMA_ENC_FORCED_IDR_DEFAULT (1)

#define XMA_ENC_FORCED_IDR_DISABLE (0)
#define XMA_ENC_FORCED_IDR_ENABLE (1)

/* Number of B-frames */
#define XMA_ENC_PARAM_BF "bf" // XMA_INT32
#define XMA_ENC_BF_DEFAULT (-1)

#define XMA_ENC_MAX_BF_VALUE (3)
#define XMA_AV1_AMD_ENC_MAX_BF_VALUE (7)
#define XMA_ENC_MIN_BF_VALUE (-1)

/* QP mode */
#define XMA_ENC_PARAM_QP_MODE "qp_mode" // XMA_INT32
typedef enum XmaEncoderQPMode {
  XMA_ENC_QP_MODE_AUTO = 0,
  XMA_ENC_QP_MODE_RELATIVE_LOAD,
  XMA_ENC_QP_MODE_UNIFORM,
  XMA_ENC_QP_MODE_DEFAULT = XMA_ENC_QP_MODE_AUTO
} XmaEncoderQPMode;

/* Enable spatial AQ */
#define XMA_ENC_PARAM_SPATIAL_AQ "spatial_aq" // XMA_INT32
#define XMA_ENC_SPATIAL_AQ_DEFAULT (-1)

/* Enable temporal AQ */
#define XMA_ENC_PARAM_TEMPORAL_AQ "temporal_aq" // XMA_INT32
#define XMA_ENC_TEMPORAL_AQ_DEFAULT (-1)

/* Max value for spatial and temporal AQ */
#define XMA_ENC_SPATIAL_AQ_AUTO (-1)
#define XMA_ENC_SPATIAL_AQ_DISABLE (0)
#define XMA_ENC_SPATIAL_AQ_ENABLE (1)
#define XMA_ENC_TEMPORAL_AQ_AUTO (-1)
#define XMA_ENC_TEMPORAL_AQ_DISABLE (0)
#define XMA_ENC_TEMPORAL_AQ_ENABLE (1)

/* Enable dynamic gop */
#define XMA_ENC_PARAM_DYNAMIC_GOP "dynamic_gop" // XMA_INT32
#define XMA_ENC_DYNAMIC_GOP_DEFAULT (-1)

/* Values for dynamic gop */
#define XMA_ENC_DYNAMIC_GOP_AUTO (-1)
#define XMA_ENC_DYNAMIC_GOP_DISABLE (0)
#define XMA_ENC_DYNAMIC_GOP_ENABLE (1)

/* Tune metrics */
#define XMA_ENC_PARAM_TUNE_METRICS "tune_metrics" // XMA_INT32
typedef enum XmaEncoderTuneMetrics {
  XMA_ENC_TUNE_METRICS_VQ = 1,
  XMA_ENC_TUNE_METRICS_PSNR,
  XMA_ENC_TUNE_METRICS_SSIM,
  XMA_ENC_TUNE_METRICS_VMAF,
  XMA_ENC_TUNE_METRICS_DEFAULT = XMA_ENC_TUNE_METRICS_VQ
} XmaEncoderTuneMetrics;

/* Tier */
#define XMA_ENC_PARAM_TIER "tier" // XMA_INT32
typedef enum XmaEncoderTier { XMA_ENC_TIER_AUTO = -1, XMA_ENC_TIER_MAIN = 0, XMA_ENC_TIER_HIGH, XMA_ENC_TIER_DEFAULT = XMA_ENC_TIER_AUTO } XmaEncoderTier;

#define XMA_ENC_PARAM_LATENCY_MS "latency_ms" // XMA_INT32, added in API v1.1
#define XMA_ENC_LATENCY_MS_MIN -1
#define XMA_ENC_LATENCY_MS_MAX 60000
#define XMA_ENC_LATENCY_MS_DEFAULT -1

#define XMA_ENC_PARAM_NO_LOWLAT_BFRAMES "no_bll"
#define XMA_ENC_NO_LOWLAT_BFRAMES_MIN -1
#define XMA_ENC_NO_LOWLAT_BFRAMES_ENABLE 1
#define XMA_ENC_NO_LOWLAT_BFRAMES_DEFAULT -1

#define XMA_ENC_PARAM_BUFSIZE "bufsize" // XMA_INT32, added in API v1.1
#define XMA_ENC_BUFSIZE_MIN -1
#define XMA_ENC_BUFSIZE_MAX 400000000
#define XMA_ENC_BUFSIZE_DEFAULT -1

#define XMA_ENC_PARAM_QUALITY "quality" // XMA_INT32, added in API v1.2
#define XMA_ENC_QUALITY_MIN -1
#define XMA_ENC_QUALITY_MAX 100
#define XMA_ENC_QUALITY_DEFAULT -1

#define XMA_ENC_PARAM_STILL_IMAGE "still_image" // XMA_INT32, added in API v1.2
typedef enum XmaEncoderStillImage {
  XMA_ENC_STILL_IMAGE_AUTO = -1,
  XMA_ENC_STILL_IMAGE_VIDEO,
  XMA_ENC_STILL_IMAGE_SINGLE_IMAGE,
  XMA_ENC_STILL_IMAGE_SEQUENCE, // Not yet supported
  XMA_ENC_STILL_IMAGE_DEFAULT = XMA_ENC_STILL_IMAGE_AUTO
} XmaEncoderStillImage;

#define XMA_ENC_STILL_IMAGE_MIN XMA_ENC_STILL_IMAGE_AUTO
#define XMA_ENC_STILL_IMAGE_MAX XMA_ENC_STILL_IMAGE_SEQUENCE

/* If delay initialization is true, the internal encoder is not initialized
 * until the first frame is recieved. This allows it to be initialized with
 * HDR10 and VUI data provided in the first frame's sidedata. The downside is
 * that the extradata, if needed, is not available until after the first frame
 * is recieved. If delay initialization is false, the internal encoder is
 * initialized when the session is created. This means the extradata is
 * available immediately afterward, but any HDR10 or VUI data is ignored. */
#define XMA_ENC_PARAM_DELAY_INITIALIZATION "delay_initialization" // XMA_INT32
#define XMA_ENC_DELAY_INITIALIZATION_DEFAULT (1)

/* The extra data is a optional buffer provided by the application to be filled
 * in by the encoder when the internal encoder is initialized. This data is
 * used by some muxers. The extra data size provided by the application is
 * initially the maximum size of the extra data. After the encoder is
 * initialized, it's value is replaced by the size of the extra data used. */
#define XMA_ENC_PARAM_EXTRA_DATA "extradata"           // XMA_STRING
#define XMA_ENC_PARAM_EXTRA_DATA_SIZE "extradata_size" // XMA_INT32

/* Latency logging */
#define XMA_ENC_PARAM_LATENCY_LOGGING "latency_logging" // XMA_INT32
#define XMA_ENC_LATENCY_LOGGING_DEFAULT (0)

#define XMA_ENC_LATENCY_LOGGING_DISABLE (0)
#define XMA_ENC_LATENCY_LOGGING_ENABLE (1)

/* Forward declaration */
typedef struct XmaEncoderSession XmaEncoderSession;

/**
 *  xma_enc_session_create() - This function creates an encoder session and
 *  must be called prior to encoding frames. A session reserves hardware
 *  resources for the duration of a video stream. The number of sessions
 *  allowed depends on a number of factors that include: resolution, frame
 *  rate, bit depth, number of input/output streams and the capabilities of the
 *  hardware accelerator.
 *
 *  @enc_props: Pointer to a XmaEncoderProperties structure that contains the
 *              key configuration properties needed for finding available
 *              hardware resource.
 *
 *  RETURN: Not NULL on success
 *          NULL on failure
 *
 *  Note: session create & destroy are thread safe APIs
*/
XmaSessionHandle xma_enc_session_create(XmaEncoderProperties* enc_props);

/**
 *  xma_enc_session_destroy() - This function destroys an encoder session
 *  that was previously created with @ref xma_enc_session_create().
 *
 *  @session: Pointer to XmaEncoderSession created with
 *            xma_enc_session_create()
 *
 *  RETURN: XMA_SUCCESS on success
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on failure.
 *
 *  Note: session create & destroy are thread safe APIs
*/
int32_t xma_enc_session_destroy(XmaSessionHandle session);

/**
 *  xma_enc_session_set_log() - This function changes the logging from
 *  the default (set in xma_initialize) to some other logging.
 *
 *  @session: Pointer to session created by xma_enc_sesssion_create()
 *  @handle:  New log to use
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on error.
*/

int32_t xma_enc_session_set_log(XmaSessionHandle session, XmaLogHandle handle);

/**
 *  xma_enc_session_send_frame() - This function sends frames to
 *  the hardware encoder.
 *
 *  @session: Pointer to session created by xm_enc_sesssion_create()
 *  @frame:   Pointer to a frame to be encoded. A NULL frame should be sent to flush the encoder and to indicate that no more frames will be sent.
 *
 *  RETURN: XMA_SUCCESS on success
 *          XMA_SEND_MORE_DATA if more frames need to be sent before first data
 *                             is ready.
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR_BAD_ALLOC on out of memory.
 *          XMA_ERROR on error.
*/
int32_t xma_enc_session_send_frame(XmaSessionHandle session, XmaFrame* frame);

/**
 *  xma_enc_session_recv_data() -This function obtains encoded data from the
 *  hardware encoder. This function is called after a call to the function
 *  xma_enc_session_send_frame() has returned XMA_SUCCESS.
 *
 *  @session:   Pointer to session created by xm_enc_sesssion_create()
 *  @data:      Pointer to a dummy XmaDataBuffer structure created by
 *              xma_data_buffer_alloc(). All parameters will be filled in by
 *              this call. If this function does not return XMA_SUCCESS,
 *              xma_data_buffer_free() must be called on the XmaDataBuffer
 *              pointer to release resources.
 *  @data_size: Pointer to hold the size of the data buffer returned
 *
 *  RETURN: XMA_SUCCESS on success.
 *          XMA_RESEND_AND_RECV if no frames are available yet.
 *          XMA_EOS on end of stream.
 *          XMA_ERROR_INVALID on invalid session.
 *          XMA_ERROR on error.
*/
int32_t xma_enc_session_recv_data(XmaSessionHandle session, XmaDataBuffer* data, int32_t* data_size);

#ifdef __cplusplus
}
#endif
