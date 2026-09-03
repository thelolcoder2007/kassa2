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

#ifndef _XLNX_ENC_CONSTANTS_H_
#define _XLNX_ENC_CONSTANTS_H_

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <xma.h>
//#include <xrm.h>

#define XLNX_ENC_APP_MODULE "ma35_encoder_app"

#define ENC_DEFAULT_LOG_LEVEL XMA_ERROR_LOG
#define ENC_DEFAULT_LOG_LOCATION XMA_LOG_TYPE_CONSOLE
#define ENC_DEFAULT_LOG_FILE (XLNX_ENC_APP_MODULE ".log")
#define ENC_MIN_NUM_B_FRAMES -1
#define ENC_MAX_NUM_B_FRAMES 3
#define ENC_MAX_NUM_B_FRAMES_AV1 7
#define ENC_DEFAULT_NUM_B_FRAMES ENC_MIN_NUM_B_FRAMES
#define ENC_DEFAULT_LEVEL 0
#define ENC_DEFAULT_TIER -1
#define ENC_DEFAULT_FRAMERATE 25
#define ENC_DEFAULT_GOP_SIZE -1
#define ENC_DYNAMIC_GOP_AUTO -1
#define ENC_DYNAMIC_GOP_DISABLE 0
#define ENC_DYNAMIC_GOP_ENABLE 1
#define ENC_DYNAMIC_GOP_DEFAULT ENC_DYNAMIC_GOP_AUTO
#define ENC_MIN_GOP_SIZE -1
#define ENC_MAX_GOP_SIZE 1000
#define ENC_SUPPORTED_MIN_QP -1
#define ENC_SUPPORTED_MAX_QP 63
#define ENC_DEFAULT_QP ENC_SUPPORTED_MIN_QP
#define ENC_SPATIAL_AQ_DISABLE XMA_ENC_SPATIAL_AQ_DISABLE
#define ENC_SPATIAL_AQ_ENABLE XMA_ENC_SPATIAL_AQ_ENABLE
#define ENC_TEMPORAL_AQ_DISABLE XMA_ENC_TEMPORAL_AQ_DISABLE
#define ENC_TEMPORAL_AQ_ENABLE XMA_ENC_TEMPORAL_AQ_ENABLE
#define ENC_DEFAULT_SPATIAL_AQ XMA_ENC_SPATIAL_AQ_DEFAULT
#define ENC_DEFAULT_TEMPORAL_AQ XMA_ENC_TEMPORAL_AQ_DEFAULT
#define ENC_DEFAULT_TUNE_METRICS 1
#define ENC_MAX_TUNE_METRICS 4
#define ENC_MIN_CRF -1
#define ENC_MAX_CRF 63
#define ENC_DEFAULT_CABR_CONFIG XMA_ENC_PARAM_CABR_CONFIG_DEFAULT
#define ENC_CRF_DEFAULT -1
#define ENC_IDR_DISABLE 0
#define ENC_IDR_ENABLE 1
#define ENC_DEFAULT_CORES 1
#define ENC_PRESET_DEFAULT "medium"
#define ENC_MIN_BUFSIZE XMA_ENC_BUFSIZE_MIN
#define ENC_MAX_BUFSIZE XMA_ENC_BUFSIZE_MAX
#define ENC_DEFAULT_BUFSIZE XMA_ENC_BUFSIZE_DEFAULT

#define ENC_MIN_LOOKAHEAD_DEPTH -1
#define ENC_MAX_LOOKAHEAD_DEPTH 53
#define ENC_DEFAULT_LOOKAHEAD_DEPTH ENC_MIN_LOOKAHEAD_DEPTH

#define ENC_MIN_LATENCY_MS -1
#define ENC_MAX_LATENCY_MS 60000
#define ENC_DEFAULT_LATENCY_MS ENC_MIN_LATENCY_MS

#define ENC_NO_LOWLAT_BFRAMES_ENABLE 1
#define ENC_NO_LOWLAT_BFRAMES_DEFAULT -1

#define ENC_SUPPORTED_MIN_WIDTH 64
#define ENC_DEFAULT_WIDTH 1920
#define ENC_SUPPORTED_MAX_WIDTH 3840

#define ENC_SUPPORTED_MIN_HEIGHT 64
#define ENC_DEFAULT_HEIGHT 1080
#define ENC_SUPPORTED_MAX_HEIGHT 2160

#define ENC_SUPPORTED_MAX_PIXELS ((ENC_SUPPORTED_MAX_WIDTH) * (ENC_SUPPORTED_MAX_HEIGHT))

#define ENC_DEFAULT_BITRATE XMA_ENC_BITRATE_DEFAULT

#define ENC_MIN_MIN_QP ENC_SUPPORTED_MIN_QP
#define ENC_MAX_MIN_QP ENC_SUPPORTED_MAX_QP
#define ENC_DEFAULT_MIN_QP ENC_SUPPORTED_MIN_QP
#define ENC_MIN_MAX_QP ENC_SUPPORTED_MIN_QP
#define ENC_MAX_MAX_QP ENC_SUPPORTED_MAX_QP
#define ENC_DEFAULT_MAX_QP ENC_SUPPORTED_MIN_QP

#define ENC_SUPPORTED_MIN_AQ_GAIN 0
#define ENC_SUPPORTED_MAX_AQ_GAIN 255
#define ENC_AQ_GAIN_NOT_USED 255

#define ENC_OPTION_DISABLE 0
#define ENC_OPTION_ENABLE 1

#define ENC_LEVEL_10 10
#define ENC_LEVEL_11 11
#define ENC_LEVEL_12 12
#define ENC_LEVEL_13 13
#define ENC_LEVEL_20 20
#define ENC_LEVEL_21 21
#define ENC_LEVEL_22 22
#define ENC_LEVEL_30 30
#define ENC_LEVEL_31 31
#define ENC_LEVEL_32 32
#define ENC_LEVEL_40 40
#define ENC_LEVEL_41 41
#define ENC_LEVEL_42 42
#define ENC_LEVEL_50 50
#define ENC_LEVEL_51 51
#define ENC_LEVEL_52 52
#define ENC_LEVEL_53 53

#define ENC_MAX_PARAMS 18

#define ENC_PROFILE_AUTO -1
#define ENC_PROFILE_DEFAULT ENC_PROFILE_AUTO
#define ENC_AV1_MAIN 200

/* H264 Encoder supported profiles */
typedef enum Xlnxh264Profiles { ENC_H264_BASELINE, ENC_H264_MAIN, ENC_H264_HIGH, ENC_H264_HIGH_10, ENC_H264_HIGH_10_INTRA } Xlnxh264Profiles;

/* HEVC Encoder supported profiles */
typedef enum XlnxHevcProfiles { ENC_HEVC_MAIN = 100, ENC_HEVC_MAIN_INTRA, ENC_HEVC_MAIN_10, PROFILE_HEVC_MAIN10_INTRA } XlnxHevcProfiles;

/* QP Mode supported values */
typedef enum XlnxQpModes { ENC_DEFAULT_QP_MODE = 0, ENC_QP_MODE_AUTO, ENC_QP_MODE_UNIFORM } XlnxQpModes;

/*  RC Mode supported values */
typedef enum XlnxRcMode {
  ENC_RC_MODE_AUTO = -1,
  ENC_RC_MODE_CONSTANT_QP,
  ENC_RC_MODE_CBR,
  ENC_RC_MODE_VBR,
  ENC_RC_MODE_CVBR,
  ENC_RC_MODE_CABR,
  ENC_RC_MODE_CRF,
  ENC_RC_MODE_DEFAULT = ENC_RC_MODE_AUTO
} XlnxRcMode;

typedef enum XlnxTuneMetrics {
  ENC_TUNE_METRICS_VQ = 1,
  ENC_TUNE_METRICS_PSNR,
  ENC_TUNE_METRICS_SSIM,
  ENC_TUNE_METRICS_VMAF,
  ENC_TUNE_METRICS_DEFAULT = ENC_TUNE_METRICS_VQ
} XlnxTuneMetrics;

typedef enum XlnxEncTiers { ENC_TIER_AUTO = -1, ENC_TIER_MAIN, ENC_TIER_HIGH, ENC_TIER_DEFAULT = ENC_TIER_AUTO } XlnxEncTiers;

typedef enum XlnxEncoderCodecID {
  ENCODER_ID_H264 = XMA_H264_ENCODER_TYPE,
  ENCODER_ID_HEVC = XMA_HEVC_ENCODER_TYPE,
  ENCODER_ID_AV1  = XMA_AV1_ENCODER_TYPE,
} XlnxEncoderCodecID;

typedef enum XlnxEncoderDeviceType {
  ENCODER_DEVICE_TYPE_ANY = XMA_ENC_DEVICE_TYPE_ANY,
  ENCODER_DEVICE_TYPE_1   = XMA_ENC_DEVICE_TYPE_1,
  ENCODER_DEVICE_TYPE_2   = XMA_ENC_DEVICE_TYPE_2,
} XlnxEncoderDeviceType;

#endif // _XLNX_ENC_CONSTANTS_H_
