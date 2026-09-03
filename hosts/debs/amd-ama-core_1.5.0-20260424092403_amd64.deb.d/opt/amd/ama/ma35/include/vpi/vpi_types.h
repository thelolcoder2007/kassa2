/* SPDX-License-Identifier: LGPL-3.0-or-later OR Apache-2.0 */

/*
 * (c) Copyright 2024 VeriSilicon Holdings Co., Ltd. All rights reserved.
 *
 * This file is dual-licensed; you may select either the GNU
 * Lesser General Public License version 3 or
 * Apache License, Version 2.0.
 *
 */

#ifndef __VPI_TYPES_H__
#define __VPI_TYPES_H__

#include <stdint.h>
//#include <pthread.h>
#include <stddef.h> // For size_t used in vip_sideband_data_info
#include <stdbool.h>

#define PIC_INDEX_MAX_NUMBER 5

#define VPE_TASK_LIVE 0
#define VPE_TASK_VOD 1

#define VPE_TASK_PRIORITY_VOD 0
#define VPE_TASK_PRIORITY_LIVE 1

#define MAX_WAIT_DEPTH 78

#define HWUPLOAD_FLAG 1
#define PP_FLAG 2
#define RC_CFG_FLAG 4

#define MAX_DEVICE_NUM 32

#define VPI_DEC_FRAME 1
#define VPI_DEC_EOF 2
#define VPI_DEC_EOF_NEED_FINISH 4

#define VPI_ENC_EOF 1

#define OPS_LOAD_MODE 0
#define OPS_PERF_MODE 1

#define VPE_CHANNEL_NUM 16
#define VPE_MAX_PLANES 3

#define VPI_VIP_MAX_INOUT_NUM 16
#define VPI_MAX_SB_NUM 8

#define EP_SIDE_EN (1 << 0)
#define RC_SIDE_EN (1 << 1)
#define EP_MMIO_EN (1 << 2) // only makes sense for EP

// With the following flag, the buffer pool will own the opaque ctx it holds
// and free it up via a callback when the buffer pool's refcount reaches 0.
#define BUFPOOL_OWN_QPAQUE (1 << 3)

#define VPE_SEI_USER_DATA_REGISTERED_ITU_T_T35 4
#define HLG_TRANSFER_CHARACTERISTICS 18
#define MAX_PAYLOAD_NUM 64

#define MAX_RESERVED 100
#define MAX_DYN_PARAM_SIZE 184

#define VPIALIGN(x, a) (((x)+(a)-1)&~((a)-1))
#define VPI_FRAME_FLAG_TILE_4x4 (0x1 << 4)
#define VPI_FRAME_FLAG_COMPRESS (0x1 << 5)
#define VPI_FRAME_NEW_FLAG      (0x1u << 31)

#define MAX_RES_WIDTH 7680
#define MAX_RES_HEIGHT 7680

typedef void *VpiHandle;
typedef void *VpiDevHandle;
typedef void *VpiMemHandle;
typedef void *VpiDecHandle;
typedef void *VpiEncHandle;
typedef void *VpiProcHandle;
typedef struct VpiCCContext VpiCCContext;

typedef void *VpiOsalHwCtx;
typedef void *VpiOsalAccelCtx;
typedef enum VpiOsalAccelType {
    VPI_OSAL_DECODER,
    VPI_OSAL_SCALER,
    VPI_OSAL_GPU,
    VPI_OSAL_ML,
    VPI_OSAL_ENCODER,
    VPI_OSAL_DMA,
    VPI_OSAL_TYPE_MAX
} VpiOsalAccelType;

typedef enum VpiFmt {
    VPI_FORMAT_NONE = 0,
    VPI_FORMAT_NV12,
    VPI_FORMAT_NV21,
    VPI_FORMAT_P010LE,
    VPI_FORMAT_P010BE,
    VPI_FORMAT_YUV420P,
    VPI_FORMAT_YUV420P10LE,
    VPI_FORMAT_YUV420P10BE,
    VPI_FORMAT_YUV422P,
    VPI_FORMAT_YUV422P10LE,
    VPI_FORMAT_YUV422P10BE,
    VPI_FORMAT_YUV444P,
    VPI_FORMAT_ARGB,
    VPI_FORMAT_ABGR,
    VPI_FORMAT_RGBA,
    VPI_FORMAT_BGRA,
    VPI_FORMAT_RGB24,
    VPI_FORMAT_BGR24,
    VPI_FORMAT_VPE,
    VPI_FORMAT_SP1010,
    VPI_FORMAT_BGR0,
    VPI_FORMAT_RGB16,
    VPI_FORMAT_RGB24_P,
    VPI_FORMAT_BGR24_P,
    VPI_FORMAT_RGB48,
    VPI_FORMAT_BGR48,
    VPI_FORMAT_RGB48_P,
    VPI_FORMAT_BGR48_P,
    VPI_FORMAT_YUV422SP,
    VPI_FORMAT_YUV422SP_10BIT,
    VPI_FORMAT_UYVY422,
    VPI_FORMAT_YUY2422,
    VPI_FORMAT_GRAY8,
    VPI_FORMAT_GRAY10LE,
    //below sw format for sidedata
    VPI_FORMAT_Y_ONLY = 100,
    VPI_FORMAT_AMA_SB,
    VPI_FORMAT_MAX
} VpiFmt;

typedef enum VpiSidedataType_t {
    VPI_SB_NONE = -1,
    VPI_SB_ML_ROI = 0,
    VPI_SB_DQPMAP_HEVC_H264,
    VPI_SB_ML_CC,
    VPI_SB_ML_CXC,
    VPI_SB_ML_LA_STAT,
    VPI_SB_ML_BOUNDING_BOX,
    VPI_SB_ML_CLASSIFICATION,
    VPI_SB_ML_YDATA,
    VPI_SB_ML_TENSOR,
    VPI_SB_ML_MAX = VPI_SB_ML_TENSOR,
    VPI_SB_VUI_PARAMS,
    VPI_SB_SEI_MASTERING_DISPLAY_COLOR_VOLUME,
    VPI_SB_SEI_CONTENT_LIGHT_LEVEL,
    VPI_SB_SEI_ATC,
    VPI_SB_SEI_CLOSED_CAPTION,
    VPI_SB_SEI_HDR10_PLUS,
    VPI_SB_SEI_DOLBY_VISION,
    VPI_SB_RPU_DOLBY_VISION,
    VPI_SB_RPU_RAW_DATA,
    VPI_SB_SEI_RAW_T35_DATA,
    VPI_SB_SEI_USER_UNREGISTERED,
    VPI_SB_TYPE_MAX
} VpiSidedataType;

/*
Specifies the (luma) pixel offset of the center of a chroma sample relative to the center of the top-left luma sample.
H.264/H.265 support all positions here (see figures E.1 and E.2 in codec specs for examples).
AV1 only supports (x, y) offsets of (0, 0) and (0, 0.5). Other values may be treated as unknown.

Conversion table:
            enum value                    H.264/AVC, H.265/HEVC                      AV1
    VPI_CHROMA_OFFSET_UNSPECIFIED     chroma_loc_info_present_flag   = 0    chroma_sample_position = 0
    VPI_CHROMA_OFFSET_XZERO_YZERO     chroma_sample_loc_type_*_field = 2    chroma_sample_position = 2
    VPI_CHROMA_OFFSET_XZERO_YHALF     chroma_sample_loc_type_*_field = 0    chroma_sample_position = 1
    VPI_CHROMA_OFFSET_XZERO_YONE      chroma_sample_loc_type_*_field = 4    chroma_sample_position = 0
    VPI_CHROMA_OFFSET_XHALF_YZERO     chroma_sample_loc_type_*_field = 3    chroma_sample_position = 0
    VPI_CHROMA_OFFSET_XHALF_YHALF     chroma_sample_loc_type_*_field = 1    chroma_sample_position = 0
    VPI_CHROMA_OFFSET_XHALF_YONE      chroma_sample_loc_type_*_field = 5    chroma_sample_position = 0
*/
typedef enum VpiChromaSampleOffset {
    VPI_CHROMA_OFFSET_UNSPECIFIED = 99,
    VPI_CHROMA_OFFSET_XZERO_YZERO = 100,
    VPI_CHROMA_OFFSET_XZERO_YHALF,
    VPI_CHROMA_OFFSET_XZERO_YONE,
    VPI_CHROMA_OFFSET_XHALF_YZERO,
    VPI_CHROMA_OFFSET_XHALF_YHALF,
    VPI_CHROMA_OFFSET_XHALF_YONE
} VpiChromaSampleOffset;

typedef enum VpiHwId {
    VPE_HW_NONE,

    VPE_HW_COMMON,

    VPE_HW_H264DEC,
    VPE_HW_HEVCDEC,
    VPE_HW_VP9DEC,
    VPE_HW_AV1DEC,
    VPE_HW_JPEGDEC,
    VPE_HW_DEC,

    VPE_HW_H264ENC,
    VPE_HW_HEVCENC,
    VPE_HW_AV1ENC,
    VPE_HW_JPEGENC,
    VPE_HW_LJPEGENC,
    VPE_HW_XAV1ENC,
    VPE_HW_ENC,

    VPE_HW_DOWNLOAD,
    VPE_HW_UPLOAD,
    VPE_HW_PP,
    VPE_HW_XABR,
    VPE_HW_SPLIT,
    VPE_HW_UPLD,
    VPE_HW_2D,
    VPE_HW_VIP_NBG,
} VpiHwId;

typedef enum VpiStructType {
    VPE_PACKET,
    VPE_FRAME,
    VPE_DEC_INIT_PARAM,
    VPE_ENC_INIT_PARAM,
    VPE_PROC_INIT_PARAM,
    VPE_PROC_INFO,
    VPE_CHANNEL_MANAGEMENT,
} VpiStructType;

typedef enum VpiCodecId {
    VPE_DEC_VP9,
    VPE_DEC_HEVC,
    VPE_DEC_H264,
    VPE_DEC_AV1,
    VPE_DEC_JPEG,
    VPE_ENC_H264,
    VPE_ENC_HEVC,
    VPE_ENC_AV1,
    VPE_ENC_JPEG,
    VPE_ENC_LJPEG,
    VPE_ENC_AV1_T2
} VpiCodecId;

enum { VPI_DATA_UPLOAD, VPI_DATA_DWLOAD };

typedef enum VpiEncType {
    VPE_ENCODER_XAV1,
} VpiEncType;

typedef enum VpiProcType {
    VPE_PROCESSOR_PP_VCDA,
    VPE_PROCESSOR_PP_VCDB,
    VPE_PROCESSOR_HWDOWNLOAD,
    VPE_PROCESSOR_HWUPLOAD,
    VPE_PROCESSOR_SPLIT,
    VPE_PROCESSOR_XABR,
    VPE_PROCESSOR_BLEND,
    VPE_PROCESSOR_TILE,
    VPE_PROCESSOR_OVERLAY,
    VPE_PROCESSOR_DRAWBOX,
    VPE_PROCESSOR_CSC,
    VPE_PROCESSOR_SUBSAMPLE,
    VPE_PROCESSOR_ROTATE,
    VPE_PROCESSOR_CROP,
    VPE_PROCESSOR_PAD,
    VPE_PROCESSOR_COMPOSE,
    VPE_PROCESSOR_VIP_NBG,
    VPE_PROCESSOR_ROI_SCALE,
} VpiProcType;

typedef enum VpiMetaType
{
    VPE_DYNAMIC_META_NONE = 0,
    VPE_DYNAMIC_HDR10_PLUS = 1,
    VPE_DYNAMIC_DOLBY_VISION = 2,
    VPE_CLOSED_CAPTION = 3,
}VpiMetaType;

typedef enum VpiNbgLayoutEnum {
    VPE_LAYOUT_NCHW = 0,
    VPE_LAYOUT_NHWC,
    VPE_LAYOUT_CHWN,
    VPE_LAYOUT_UNSUPPORTED,
} VpiNbgLayoutEnum;

typedef enum {
    ML_ROI_DQP = 0,
    ML_ROI_RAW,
    ML_ROI_DQP_AND_RAW
} MLROIType;

typedef enum VpiPictType{
    VPE_PICT_TYPE_I           = 0,
    VPE_PICT_TYPE_P           = 1,
    VPE_PICT_TYPE_B           = 2,
    VPE_PICT_TYPE_BI          = 3,
    VPE_PICT_TYPE_MAX         = 4
} VpiPictType;

typedef struct VpiParamSet {
    char *key;
    char *value;
    struct VpiParamSet *next;
} VpiParamSet;

#define MAX_BLOCK_LEVEL 254
#define MAX_NUM_WINDOWS 4
#define MAX_CC_COUNT 32
#define NUM_COLOR_COMPONENT 3
#define NUM_CMPS 3
#define NUM_PIVOT_IDX 9
#define MAX_NUM_ROWS_PEAK_LUMINANCE 32
#define MAX_NUM_COLS_PEAK_LUMINANCE 32
#define MAX_NUM_DISTRIBUTION_MAXRGB_PERCENTILES 16
#define MAX_NUM_BEZIER_CURVE_ANCHORS 16

typedef struct VpiMasterDisColVol {
    uint8_t present_flag;
    uint16_t display_primaries_x[3];
    uint16_t display_primaries_y[3];
    uint16_t white_point_x;
    uint16_t white_point_y;
    uint32_t max_display_mastering_luminance;
    uint32_t min_display_mastering_luminance;
}VpiMasterDisColVol;

typedef struct VpiContentLightLevel {
    uint8_t present_flag;
    uint16_t max_content_light_level;
    uint16_t max_pic_average_light_level;
}VpiContentLightLevel;

typedef struct VpiATCInfo {
    uint8_t present_flag;
    uint8_t preferred_transfer_characteristics;
}VpiATCInfo;

typedef struct VpiVuiColorDescription
{
    uint8_t colour_primaries;
    uint8_t transfer_characteristics;
    uint8_t matrix_coeffs;
}VpiVuiColorDescription;

typedef struct VpiVuiParameters
{
    int sar_width;
    int sar_height;
    int vui_video_signal_type_present_flag;
    int vui_video_format;
    int vui_video_full_range;
    int color_description_present_flag;
    VpiVuiColorDescription color_description;
    VpiChromaSampleOffset chroma_sample_offset;
}VpiVuiParameters;

static const VpiVuiParameters VPI_VUI_PARAMS_DEFAULT = {
    .sar_width = 0,
    .sar_height = 0,
    .vui_video_signal_type_present_flag = 0,
    .vui_video_format = 5, // unspecified
    .vui_video_full_range = 0,
    .color_description_present_flag = 0,
    .color_description = {2, 2, 2}, // unspecified
    .chroma_sample_offset = VPI_CHROMA_OFFSET_UNSPECIFIED
};

typedef struct VpiSMPTE_ST2094_40
{
    uint8_t country_code;
    uint8_t extended_country_code;
    uint16_t itu_t_t35_terminal_provider_code;
    uint16_t itu_t_t35_terminal_provider_oriented_code;

    uint8_t application_identifier;
    uint8_t application_version;
    uint8_t num_windows;

    uint16_t window_upper_left_corner_x[MAX_NUM_WINDOWS];
    uint16_t window_upper_left_corner_y[MAX_NUM_WINDOWS];
    uint16_t window_lower_right_corner_x[MAX_NUM_WINDOWS];
    uint16_t window_lower_right_corner_y[MAX_NUM_WINDOWS];
    uint16_t center_of_ellipse_x[MAX_NUM_WINDOWS];
    uint16_t center_of_ellipse_y[MAX_NUM_WINDOWS];
    uint8_t rotation_angle[MAX_NUM_WINDOWS];
    uint16_t semimajor_axis_internal_ellipse[MAX_NUM_WINDOWS];
    uint16_t semimajor_axis_external_ellipse[MAX_NUM_WINDOWS];
    uint16_t semiminor_axis_external_ellipse[MAX_NUM_WINDOWS];
    uint8_t overlap_process_option[MAX_NUM_WINDOWS];

    uint32_t targeted_system_display_maximum_luminance;
    uint8_t targeted_system_display_actual_peak_luminance_flag;
    uint8_t num_rows_targeted_system_display_actual_peak_luminance;
    uint8_t num_cols_targeted_system_display_actual_peak_luminance;

    uint8_t targeted_system_display_actual_peak_luminance[MAX_NUM_ROWS_PEAK_LUMINANCE][MAX_NUM_COLS_PEAK_LUMINANCE];
    uint32_t maxscl[MAX_NUM_WINDOWS][NUM_COLOR_COMPONENT];
    uint32_t average_maxrgb[MAX_NUM_WINDOWS];
    uint8_t num_distribution_maxrgb_percentiles[MAX_NUM_WINDOWS];
    uint8_t distribution_maxrgb_percentages[MAX_NUM_WINDOWS][MAX_NUM_DISTRIBUTION_MAXRGB_PERCENTILES];
    uint32_t distribution_maxrgb_percentiles[MAX_NUM_WINDOWS][MAX_NUM_DISTRIBUTION_MAXRGB_PERCENTILES];
    uint16_t fraction_bright_pixels[MAX_NUM_WINDOWS];

    uint8_t mastering_display_actual_peak_luminance_flag;
    uint8_t num_rows_mastering_display_actual_peak_luminance;
    uint8_t num_cols_mastering_display_actual_peak_luminance;

    uint8_t mastering_display_actual_peak_luminance[MAX_NUM_ROWS_PEAK_LUMINANCE][MAX_NUM_COLS_PEAK_LUMINANCE];
    uint8_t tone_mapping_flag[MAX_NUM_WINDOWS];
    uint16_t knee_point_x[MAX_NUM_WINDOWS];
    uint16_t knee_point_y[MAX_NUM_WINDOWS];
    uint8_t num_bezier_curve_anchors[MAX_NUM_WINDOWS];
    uint16_t bezier_curve_anchors[MAX_NUM_WINDOWS][MAX_NUM_BEZIER_CURVE_ANCHORS];
    uint8_t color_saturation_mapping_flag[MAX_NUM_WINDOWS];
    uint8_t color_saturation_weight[MAX_NUM_WINDOWS];
}VpiHdr10PlusMeta;

typedef struct VpiSMPTE_ST2094_10
{
    uint8_t country_code;
    uint8_t extended_country_code;
    uint16_t itu_t_t35_terminal_provider_code;
    uint16_t itu_t_t35_terminal_provider_oriented_code;

    uint32_t app_identifier;
    uint32_t app_version;
    uint8_t metadata_refresh_flag;
    uint8_t num_ext_blocks;

    uint32_t ext_block_length[MAX_BLOCK_LEVEL];
    uint8_t ext_block_level[MAX_BLOCK_LEVEL];

    /* ext_block_level[i] == 1 */
    uint16_t min_PQ[MAX_BLOCK_LEVEL];
    uint16_t max_PQ[MAX_BLOCK_LEVEL];
    uint16_t avg_PQ[MAX_BLOCK_LEVEL];

    /* ext_block_level[i] == 2 */
    uint16_t target_max_PQ[MAX_BLOCK_LEVEL];
    uint16_t trim_slope[MAX_BLOCK_LEVEL];
    uint16_t trim_offset[MAX_BLOCK_LEVEL];
    uint16_t trim_power[MAX_BLOCK_LEVEL];
    uint16_t trip_chroma_weight[MAX_BLOCK_LEVEL];
    uint16_t trim_saturation_gain[MAX_BLOCK_LEVEL];
    int16_t ms_weight[MAX_BLOCK_LEVEL];

    /* ext_block_level[i] == 5 */
    uint16_t active_area_left_offset[MAX_BLOCK_LEVEL];
    uint16_t active_area_right_offset[MAX_BLOCK_LEVEL];
    uint16_t active_area_top_offset[MAX_BLOCK_LEVEL];
    uint16_t active_area_bottom_offset[MAX_BLOCK_LEVEL];
}VpiDolbyVisionMeta;

typedef struct VpiA53_Closed_Caption
{
    uint8_t country_code;
    uint8_t extended_country_code;
    uint16_t itu_t_t35_provider_code;
    uint32_t user_identifier;
    uint8_t user_data_type_code;
    uint8_t process_em_data_flag;  /* just 1 bit is valid */
    uint8_t process_cc_data_flag;  /* just 1 bit is valid */
    uint8_t additional_data_flag;  /* just 1 bit is valid */
    uint8_t cc_count;  /* just 5 bit is valid */
    uint8_t em_data;
    uint8_t cc_valid[MAX_CC_COUNT];  /* just 1 bit is valid */
    uint8_t cc_type[MAX_CC_COUNT];  /* just 2 bit is valid */
    uint8_t cc_data_1[MAX_CC_COUNT];
    uint8_t cc_data_2[MAX_CC_COUNT];
}VpiClosedCaptionMeta;

typedef struct VpiRawT35Data {
    uint32_t payload_byte_length;
    uint8_t *payload_byte_data;
} VpiRawT35Data;

typedef struct VpiT35Parameters
{
    union meta {
        VpiHdr10PlusMeta hdr10plus;
        VpiDolbyVisionMeta dolbyvision;
        VpiClosedCaptionMeta closedcaption;
    } meta;
    VpiMetaType meta_type;
}VpiT35Parameters;

typedef struct VpiRawRpuData {
    uint32_t payload_byte_length;
    uint8_t *payload_byte_data;
} VpiRawRpuData;

typedef struct VpiRpuData
{
    uint8_t rpu_type;
    uint16_t rpu_format;
    uint8_t vdr_rpu_profile;
    uint8_t vdr_rpu_level;
    uint8_t vdr_seq_info_present_flag;
    uint8_t chroma_resampling_explicit_filter_flag;
    uint8_t coefficient_data_type;
    uint8_t coefficient_log2_denom;
    uint8_t vdr_rpu_normalized_idc;
    uint8_t bl_video_full_range_flag;
    uint8_t bl_bit_depth_minus8;
    uint8_t el_bit_depth_minus8;
    uint8_t vdr_bit_depth_minus8;
    uint8_t spatial_resampling_filter_flag;
    uint8_t el_spatial_resampling_filter_flag;
    uint8_t disable_residual_flag;
    uint8_t vdr_dm_metadata_present_flag;
    uint8_t use_prev_vdr_rpu_flag;
    uint8_t prev_vdr_rpu_id;
    uint8_t vdr_rpu_id;
    uint8_t mapping_color_space;
    uint8_t mapping_chroma_format_idc;
    uint8_t num_pivots_minus2[NUM_CMPS];
    uint16_t pred_pivot_value[NUM_CMPS][NUM_PIVOT_IDX];
    uint8_t nlq_method_idc;
    uint32_t num_x_partitions_minus1;
    uint32_t num_y_partitions_minus1;

    uint8_t mapping_idc[NUM_CMPS][NUM_PIVOT_IDX];
    uint8_t poly_order_minus1[NUM_CMPS][NUM_PIVOT_IDX];
    uint8_t linear_interp_flag[NUM_CMPS][NUM_PIVOT_IDX];
    uint32_t poly_coef_int[NUM_CMPS][NUM_PIVOT_IDX][NUM_CMPS];
    uint32_t poly_coef[NUM_CMPS][NUM_PIVOT_IDX][NUM_CMPS];
    uint8_t mmr_order_minus1[NUM_CMPS][NUM_PIVOT_IDX];
    uint32_t mmr_constant_int[NUM_CMPS][NUM_PIVOT_IDX];
    uint32_t mmr_constant[NUM_CMPS][NUM_PIVOT_IDX];
    uint32_t mmr_coef_int[NUM_CMPS][NUM_PIVOT_IDX][NUM_CMPS][NUM_PIVOT_IDX];
    uint32_t mmr_coef[NUM_CMPS][NUM_PIVOT_IDX][NUM_CMPS][NUM_PIVOT_IDX];

    uint16_t nlq_offset[NUM_CMPS];
    uint32_t vdr_in_max_int[NUM_CMPS];
    uint32_t vdr_in_max[NUM_CMPS];
    uint32_t linear_deadzone_slope_int[NUM_CMPS];
    uint32_t linear_deadzone_slope[NUM_CMPS];
    uint32_t linear_deadzone_threshold_int[NUM_CMPS];
    uint32_t linear_deadzone_threshold[NUM_CMPS];

    uint8_t affected_dm_metadata_id;
    uint8_t current_dm_metadata_id;
    uint8_t scene_refresh_flag;
    int16_t ycc_to_rgb_coef[NUM_PIVOT_IDX];
    uint32_t ycc_to_rgb_offset[NUM_CMPS];
    int16_t rgb_to_lms_coef[NUM_PIVOT_IDX];
    uint16_t signal_eotf;
    uint16_t signal_eotf_param0;
    uint16_t signal_eotf_param1;
    uint32_t signal_eotf_param2;
    uint8_t signal_bit_depth;
    uint8_t signal_color_space;
    uint8_t signal_chroma_format;
    uint8_t signal_full_range_flag;
    uint16_t source_min_pq;
    uint16_t source_max_pq;
    uint16_t source_diagonal;
    uint8_t num_ext_blocks;
    uint32_t ext_block_length[MAX_BLOCK_LEVEL];
    uint8_t ext_block_level[MAX_BLOCK_LEVEL];

    /* ext_block_level[i] == 1 */
    uint16_t min_PQ[MAX_BLOCK_LEVEL];
    uint16_t max_PQ[MAX_BLOCK_LEVEL];
    uint16_t avg_PQ[MAX_BLOCK_LEVEL];

    /* ext_block_level[i] == 2 */
    uint16_t target_max_PQ[MAX_BLOCK_LEVEL];
    uint16_t trim_slope[MAX_BLOCK_LEVEL];
    uint16_t trim_offset[MAX_BLOCK_LEVEL];
    uint16_t trim_power[MAX_BLOCK_LEVEL];
    uint16_t trip_chroma_weight[MAX_BLOCK_LEVEL];
    uint16_t trim_saturation_gain[MAX_BLOCK_LEVEL];
    int16_t ms_weight[MAX_BLOCK_LEVEL];

    /* ext_block_level[i] == 5 */
    uint16_t active_area_left_offset[MAX_BLOCK_LEVEL];
    uint16_t active_area_right_offset[MAX_BLOCK_LEVEL];
    uint16_t active_area_top_offset[MAX_BLOCK_LEVEL];
    uint16_t active_area_bottom_offset[MAX_BLOCK_LEVEL];
}VpiRpuData;

typedef struct VpiUserDataUnReg {
    uint32_t payload_byte_length;
    uint8_t *payload_byte_data;
} VpiUserDataUnReg;

//keep it for xabrsdk build
typedef struct VpiHdrParameters {
    VpiMasterDisColVol mdcv;
    VpiContentLightLevel cll;
    VpiATCInfo atc;
    VpiT35Parameters *t35_param;
} VpiHdrParameters;

typedef struct VpiHwCfg {
    int id;
    int min_width;
    int min_height;
    int max_width;
    int max_height;
    VpiFmt *sw_formats;
    VpiFmt *hw_formats;
} VpiHwCfg;

typedef struct VpiChannelInfo {
    int flag;
    int enabled;
    int width;
    int height;
    int walign;
    int halign;
    uint32_t stride_size[VPE_MAX_PLANES];
    VpiFmt format;
    VpiFmt sw_format;
} VpiChannelInfo;

typedef struct VpiChannelInfoMgt {
    uint8_t output_num;
    uint8_t ch_used[VPE_CHANNEL_NUM];
    int ch_map[VPE_CHANNEL_NUM];
    VpiChannelInfo ch_info[VPE_CHANNEL_NUM];
} VpiChannelInfoMgt;

typedef struct VpiMemInfo {
    VpiChannelInfo ch_info[VPE_CHANNEL_NUM];
} VpiMemInfo;

typedef void * EDMA_HANDLE;

typedef struct VpiMemCtx {
    int         fd_memalloc;
    int         task_id;
    char*       device;
    void*       edma_handle;

    VpiChannelInfo ch_info[VPE_CHANNEL_NUM];
    VpiHwId        hw;
    int            ch_map[VPE_CHANNEL_NUM];
    int            ch_used[VPE_CHANNEL_NUM];
    void* frm_info_pool;
    void* mid_buf_pool;
    //void* sidedata_pool;
} VpiMemCtx;

typedef struct VpiPkt {
    uint8_t *buffer;
    int size;
    int64_t pts;
    int64_t dts;
    int poc;
    int valid;
    int key_frame; /* 1-> pkt contains key frame*/

    int is_eof;
    void *opaque;
    int64_t duration;

    /*Hardware Calculated psnr and ssim numbers
    from AMD encoders.*/
    double psnr[3];
    double ssim[3];

    int flags;
    void* user_data;
} VpiPkt;

typedef void * VpiSidedataHandle;
typedef void * VpiSidePoolHandle;
typedef void * VpiSidedataPoolManagerHandle;

//NOTE: Please do not change the order of the members in this structure
//      If you need to add new members, please add them at the end of the structure
//      and update the MAX_RESERVED value accordingly!
typedef struct VpiDynamicParams {
    bool     is_spatial_aq_gain_changed;
    uint32_t spatial_aq_gain;
    bool     is_temporal_aq_gain_changed;
    uint32_t temporal_aq_gain;
    bool     is_temporal_mode_changed;
    bool     temporal_aq_mode;
    bool     is_spatial_mode_changed;
    bool     spatial_aq_mode;

    bool     is_bit_rate_changed;
    uint32_t bit_rate_kbps;

    bool     is_b_frames_changed;
    uint8_t  num_b_frames;

    bool     is_min_qp_changed;
    uint32_t min_qp;
    bool     is_max_qp_changed;
    uint32_t max_qp;
    bool     is_qp_changed;
    uint32_t qp;
    bool     is_qp_i_offset_changed;
    int32_t  qp_i_offset;
    bool     is_qp_b_offset_changed;
    int32_t  qp_b_offset;
    bool     is_min_bit_rate_changed;
    uint32_t min_bit_rate_kbps;
    bool     is_max_bit_rate_changed;
    uint32_t max_bit_rate_kbps;

    //<- Add new members here ->
    uint8_t reserved[MAX_RESERVED];

} VpiDynamicParams;

typedef struct VpiFrm {
    int valid;
    int width;
    int height;
    int poc;
    VpiFmt format;

    int key_frame; /* 1->key frame, 0->not */

    VpiChannelInfo ch_info[VPE_CHANNEL_NUM];
    void *vpi_picinfo_buf;     // point to VpiBuffer

    int64_t pts;
    int64_t dts;
    int64_t duration;

    void *next;

    VpiDynamicParams dynamic_params;

    //attached sidedata, keep same type in same array list
    VpiSidedataHandle vpi_sidedata[VPI_SB_TYPE_MAX][VPI_MAX_SB_NUM];

    void *opaque;

    /* the buf_ref ref to the original data buffer which shared between avframes */
    void *buf_ref[VPE_CHANNEL_NUM];

    int ch_index;

    int flags;
    int flush_buffers_status_eof;
    uint32_t log_frm_num;
    VpiPictType pict_type;
    void* user_data;
} VpiFrm;

typedef struct VpiDecInitParams {
    VpiCodecId dec_type;
    VpiOsalHwCtx hw_ctx;
    VpiMemHandle mem_ctx;
    char *resize_str;
    char *outfmt_str;
    int transcode;
    uint32_t src_width;
    uint32_t src_height;
    int src_depth; //Used to set default out format
    void *pkt_pool;
    int low_latency;
    char *extradata;
    int extradata_size;
    char *pp_setting;
    char *max_res;
    int core_id;
} VpiDecInitParams;

typedef enum EncProfile {
    PROFILE_AUTO = -1,
    PROFILE_H264_BASELINE = 0,
    PROFILE_H264_MAIN,
    PROFILE_H264_HIGH,
    PROFILE_H264_HIGH10,
    PROFILE_H264_HIGH10_INTRA,

    PROFILE_HEVC_MAIN = 100,
    PROFILE_HEVC_MAIN_INTRA,
    PROFILE_HEVC_MAIN10,
    PROFILE_HEVC_MAIN10_INTRA,

    PROFILE_AV1_MAIN = 200,
} EncProfile;

typedef enum EncTier {
    TEIR_ENC_AUTO = -1,
    TEIR_ENC_MAIN = 0,
    TEIR_ENC_HIGH
} EncTier;

typedef enum RateControlMode {
    MODE_RC_AUTO = -1,
    MODE_RC_CONST_QP = 0,
    MODE_RC_CBR,
    MODE_RC_VBR,
    MODE_RC_CVBR,
    MODE_RC_CABR,
    MODE_RC_CRF,
    MODE_RC_MLCAE,
} RateControlMode;

typedef enum QpMode {
    MODE_QP_AUTO = 0,
    MODE_QP_REL_LOAD,
    MODE_QP_UNIFORM
} QpMode;

typedef enum QLevel {
    QLEVEL_AUTO = -1
} QLevel;

typedef enum StillImage {
    STILL_IMAGE_AUTO = -1,
    STILL_IMAGE_VIDEO,
    STILL_IMAGE_SINGLE_IMAGE,
    STILL_IMAGE_SEQUENCE
} StillImage;

typedef enum LatencyMode {
    MODE_LATENCY_NORMAL = 0,
    MODE_LATENCY_LOW,
    MODE_LATENCY_ULL
} LatencyMode;

typedef enum Av1EncType {
    VPI_ENC_TYPE_1 = 1,
    VPI_ENC_TYPE_2,
} Av1EncType;

typedef enum TuneMetrics {
    TM_DISABLE = 0,
    TM_BEST_VQ,
    TM_BEST_PSNR,
    TM_BEST_SSIM,
    TM_BEST_VMAF
} TuneMetrics;

typedef enum SdPassThroughMode {
    SD_PASS_THROUGH_AUTO,
    SD_PASS_THROUGH_DISABLE,
    SD_PASS_THROUGH_ENABLE,
} SdPassThroughMode;

typedef struct VpiEncInitParams {
    VpiEncType enc_type;
    VpiOsalHwCtx hw_ctx;
    VpiMemHandle mem_ctx;
    int flag;
    int walign;
    int halign;

    int64_t pts;
    int64_t dts;

    int rate_num;
    int rate_denom;
    int width;
    int height;
    int bit_rate;
    int codec_format;
    int input_format;

    int orig_width;
    int orig_height;

    int crf;
    char *preset;
    char *profile;
    char *level;
    char *force_idr;
    int force_8bit;
    int bitdepth;

    /*Additionals init params*/
    int enc_level; //integer version of char *level
    EncProfile enc_profile; //integer version of char *profile
    int enc_force_idr; //integer version of char *force_idr
    RateControlMode rate_control; //encoder rate control mode
    int gop_size;
    int64_t max_bitrate;
    int la_depth;
    int latency_ms;
    int spatial_aq;
    int spatial_aq_gain;
    int temporal_aq;
    int temporal_aq_gain;
    QpMode qp_mode;
    int qp;
    int min_qp;
    int max_qp;
    TuneMetrics tune_metrics;
    Av1EncType av1_type;
    EncTier enc_tier;
    int latency_logging;
    int latency_mode;
    int slice_id;
    int num_b_frames;
    int dynamic_gop;
    int enable_cae; // Content-adaptive encoding feature
    int64_t min_bitrate;
    int  qp_i_offset;
    int  qp_b_offset;

    int ch_index; /* channel index */
    int enc_index;

    char *xav1_options;

    /* params for vui info */
    VpiVuiParameters vui_param;
    /* static metadata for hdr*/
    VpiMasterDisColVol mdcv;
    VpiContentLightLevel cll;
    VpiATCInfo atc;

    void *input_frm_pool;
    int num_cores; // Decides if it's 1 or 2 slice endcoding
    int bufsize;
    int no_low_latency_b_frames;

    /* Still Image Encoding */
    StillImage still_image; // AVIF
    int jpeg_qlevel;
    char *mlcae_config;
    /* cabr_config : parameter impacting Rate Control */
    char *cabr_config;

    /* SD passthrough options */
    SdPassThroughMode hdr10plus_passthrough;
    SdPassThroughMode cc_passthrough;
    char *content_light_level_config;
    char *mastering_display_config;
    char *atc_config;

} VpiEncInitParams;

typedef enum ActionType {
    ACTION_NONE = -1,
    SUBSAMPLE,
    ROTATE,
    CSC,
    RECT_BOX,
    MULTISRC_BLIT,
    BLEND,
    OVERLAY,
    CROP,
    PAD,
    COMPOSE
} ActionType;

typedef struct VpiProcCompParams {
  uint32_t src_width;
  uint32_t src_height;
  uint32_t src_xpos;
  uint32_t src_ypos;
  uint32_t dst_width;
  uint32_t dst_height;
  uint32_t dst_xpos;
  uint32_t dst_ypos;
  float    alpha;
  uint32_t zorder;
  uint32_t angle;
  uint32_t flip;
  uint32_t border_color;
  uint32_t border_inner_size;
  uint32_t border_outer_size;
} VpiProcCompParams;

typedef struct VpiProc2DParams {
    //drawbox
    int box_thickness;
    uint32_t box_color;
    int box_left;
    int box_top;
    int box_w;
    int box_h;
    // crop
    int crop_left;
    int crop_top;
    int crop_w;
    int crop_h;
    //csc
    int csc;
    //rotate
    int rotate_angle;
    //overlay/blend
    int x_position;
    int y_position;
    float src_alpha;
    //tile
    int grid_x;
    int grid_y;
    //pad
    int pad_x;
    int pad_y;
    int pad_w;
    int pad_h;
    uint32_t pad_color;
    //compositor
    bool     bg_enable;
    uint32_t bg_color;
    uint64_t src_enable;
    VpiProcCompParams src_comp[64];
} VpiProc2DParams;

/**
 * Rational number (pair of numerator and denominator).
 */
typedef struct VpiRational{
    int num; ///< Numerator
    int den; ///< Denominator
} VpiRational;

typedef struct VpiProcInitParams {
    VpiProcType processor_type;
    VpiOsalHwCtx hw_ctx;
    VpiMemHandle mem_ctx;
    VpiMemHandle mem_ctx_in;
    int output_num;
    int width;
    int height;
    int walign;
    int halign;
    int out_format;
    const char *input_format;
    const char *input_sw_format;
    char *resize_str;
    int flag;
    int pic_index;
    int pic_index1;
    char *model_filename; // json config file
    char *prepost_libname; //dynamic prepost library

    //For OSML
    char *model_plugin_name; //Which model plugin to call eg. ROI, YOLO
    char *model_plugin_args; //Private argumrnts sent as is to model plugin

    int channel_choose;
    int main_alpha;
    int overlay_alpha;
    int porter_duff_rule;

    //For 2D
    VpiProc2DParams fltr2d;

    int core_id;
    int nb_frames;
    int logical_dev;
    int inference_period;
    void *mixrate_ctx;

    int enable_pipeline;
    // output fps to calculate load of scaler
    VpiRational output_frame_rate[VPE_CHANNEL_NUM];

} VpiProcInitParams;

typedef struct VpiProcInfo {
    VpiChannelInfo ch_info[VPE_CHANNEL_NUM];
} VpiProcInfo;

typedef void (*log_callback)(void *, int);

typedef enum {
    /*Return codes related to XABR pipeline states
    ** Not errors so kept as positive values*/
    XABR_FLUSH_AGAIN        = 3,
    XABR_PIPELINE_FILL_DONE = 2,
    XABR_SEND_MORE_DATA     = 1,

    VPI_SUCCESS             = 0,
    /*Error Return codes*/
    VPI_ERR_SW              = -1,
    VPI_ERR_SYSTEM          = -2,
    VPI_ERR_WRONG_STATE     = -3,
    VPI_ERR_DEVICE          = -4,
    VPI_ERR_OPTION          = -5,
    VPI_ERR_PARAM           = -6,
    VPI_ERR_DEVICE_OPEN     = -7,
    VPI_ERR_DEVICE_TASK     = -8,
    VPI_ERR_TRY_AGAIN       = -9,
    VPI_ERR_WAIT_TIMEOUT    = -10,

    /*Pipeline Return codes*/
    VPI_SEND_MORE_DATA      = -20,
    VPI_FLUSH_AGAIN         = -21,

    VPI_DEC_START = -200,
    /** Invalid parameter was used */
    VPI_DEC_PARAM_ERROR = VPI_DEC_START - 1,
    /* An unrecoverable error in decoding */
    VPI_DEC_STRM_ERROR = VPI_DEC_START - 2,
    /** The decoder has not been initialized */
    VPI_DEC_NOT_INITIALIZED = VPI_DEC_START - 3,
    /** Memory allocation failed */
    VPI_DEC_MEMFAIL = VPI_DEC_START - 4,
    /** Initialization failed */
    VPI_DEC_INITFAIL = VPI_DEC_START - 5,
    /** Video sequence information is not available because stream headers have
       not been decoded */
    VPI_DEC_HDRS_NOT_RDY = VPI_DEC_START - 6,
    /** Video sequence frame size or tools not supported */
    VPI_DEC_STREAM_NOT_SUPPORTED = VPI_DEC_START - 8,
    /** External buffer rejected. (Too much than requested) */
    VPI_DEC_EXT_BUFFER_REJECTED = VPI_DEC_START - 9,
    /** Invalid pp parameter was used */
    VPI_DEC_INFOPARAM_ERROR = VPI_DEC_START - 10,
    /** ERROR */
    VPI_DEC_ERROR = VPI_DEC_START - 11,
    /** Unsupported */
    VPI_DEC_UNSUPPORTED = VPI_DEC_START - 12,
    /** Invalid stream length used */
    VPI_DEC_INVALID_STREAM_LENGTH = VPI_DEC_START - 13,
    /** Invalid input buffer size used */
    VPI_DEC_INVALID_INPUT_BUFFER_SIZE = VPI_DEC_START - 14,
    /** Needs to increase input buffer */
    VPI_DEC_INCREASE_INPUT_BUFFER = VPI_DEC_START - 15,
    /** Slice mode not supported for this operation type */
    VPI_DEC_SLICE_MODE_UNSUPPORTED = VPI_DEC_START - 16,
    /** Supplied metadata is in wrong format */
    VPI_DEC_METADATA_FAIL = VPI_DEC_START - 17,
    /** The decoder has no available buffer */
    VPI_DEC_NO_DECODING_BUFFER = VPI_DEC_START - 99,
    /** ASO/FMO detected in multicore, decoding will stop */
    VPI_DEC_ADVANCED_TOOLS = VPI_DEC_START - 100,
    /** Pending DEC_HDRS_RDY */
    VPI_DEC_PENDING_FLUSH = VPI_DEC_START - 101,
    /** Resolution changed from low to high*/
    VPI_DEC_RES_CHANGED_HIGHER = VPI_DEC_START - 102,
    /** Resolution changed from high to low*/
    VPI_DEC_RES_CHANGED_LOWER = VPI_DEC_START - 103,
    /** Resolution changed and exceeded initial resolution */
    VPI_DEC_RES_CHANGED_EXCEED_INIT = VPI_DEC_START - 104,
    /** Driver could not reserve decoder hardware */
    VPI_DEC_HW_RESERVED = VPI_DEC_START - 254,
    /** Hardware timeout occurred */
    VPI_DEC_HW_TIMEOUT = VPI_DEC_START - 255,
    /** Hardware received error status from system bus */
    VPI_DEC_HW_BUS_ERROR = VPI_DEC_START - 256,
    /** Hardware encountered an unrecoverable system error */
    VPI_DEC_SYSTEM_ERROR = VPI_DEC_START - 257,
    /** Decoder wrapper encountered an error */
    VPI_DEC_DWL_ERROR = VPI_DEC_START - 258,
    /** Hardware encountered an fatal system error */
    VPI_DEC_FATAL_SYSTEM_ERROR = VPI_DEC_START - 259,
    /** Evaluation limit exceeded */
    VPI_DEC_EVALUATION_LIMIT_EXCEEDED = VPI_DEC_START - 999,
    /** Video format not supported */
    VPI_DEC_FORMAT_NOT_SUPPORTED = VPI_DEC_START - 1000,
    /** There is no channel to meet the demand */
    VPI_DEC_NO_VALID_PP_CHANNEL = VPI_DEC_START - 1001,

    VPI_VIP_START = -2000,
    /** VIP PARAM ERROR */
    VPI_VIP_PARAM_ERR = VPI_VIP_START - 1,
    /** VIP FILE ERROR */
    VPI_VIP_FILE_ERR = VPI_VIP_START - 2,
    /** VIP FILE ERROR */
    VPI_VIP_FILE_PARSE_ERR = VPI_VIP_START - 3,
    /** VIP unsupported format */
    VPI_VIP_UNSUPPORT_FORMAT_ERR = VPI_VIP_START - 4,
    /** VIP JSON ERROR */
    VPI_VIP_JSON_ERR = VPI_VIP_START - 5,
    /** VIP JSON ERROR */
    VPI_VIP_CONFIGURATION_ERR = VPI_VIP_START - 6,
    /** VIP unsupported addr */
    VPI_VIP_UNSUPPORT_ADDR_ERR = VPI_VIP_START - 7,
    /** VIP frame buffer poll full */
    VPI_VIP_POOL_FULL_ERR = VPI_VIP_START - 8,
    /** VIP  error from 3rd party osml plugin */
    VPI_VIP_ERR_OSML = VPI_VIP_START - 50,
    /** VIP  unsupported vx op */
    VPI_VIP_VX_BASE_ERR = VPI_VIP_START - 100,
    /** VIP vx error */
    VPI_VIP_NBG_BASE_ERR = VPI_VIP_START - 200,

    VPI_GC620_START = -3000,
    /** GC620 PARAM ERROR */
    VPI_GC620_PARAM_ERR = VPI_GC620_START - 1,
    /** VIP unsupported format */
    VPI_GC620_UNSUPPORT_FORMAT_ERR = VPI_GC620_START - 2,
    /** VIP unsupported format */
    VPI_GC620_RENDER_ERR = VPI_GC620_START - 100,

    VPI_PP_START                     = -4000,
    VPI_PP_PARAM_ERROR               = VPI_PP_START - 1,
    VPI_PP_MEMFAIL                   = VPI_PP_START - 4,
    VPI_PP_IN_SIZE_INVALID           = VPI_PP_START - 64,
    VPI_PP_IN_ADDRESS_INVALID        = VPI_PP_START - 65,
    VPI_PP_IN_FORMAT_INVALID         = VPI_PP_START - 66,
    VPI_PP_CROP_INVALID              = VPI_PP_START - 67,
    VPI_PP_ROTATION_INVALID          = VPI_PP_START - 68,
    VPI_PP_OUT_SIZE_INVALID          = VPI_PP_START - 69,
    VPI_PP_OUT_ADDRESS_INVALID       = VPI_PP_START - 70,
    VPI_PP_OUT_FORMAT_INVALID        = VPI_PP_START - 71,
    VPI_PP_VIDEO_ADJUST_INVALID      = VPI_PP_START - 72,
    VPI_PP_RGB_BITMASK_INVALID       = VPI_PP_START - 73,
    VPI_PP_FRAMEBUFFER_INVALID       = VPI_PP_START - 74,
    VPI_PP_MASK1_INVALID             = VPI_PP_START - 75,
    VPI_PP_MASK2_INVALID             = VPI_PP_START - 76,
    VPI_PP_DEINTERLACE_INVALID       = VPI_PP_START - 77,
    VPI_PP_IN_STRUCT_INVALID         = VPI_PP_START - 78,
    VPI_PP_IN_RANGE_MAP_INVALID      = VPI_PP_START - 79,
    VPI_PP_ABLEND_UNSUPPORTED        = VPI_PP_START - 80,
    VPI_PP_DEINTERLACING_UNSUPPORTED = VPI_PP_START - 81,
    VPI_PP_DITHERING_UNSUPPORTED     = VPI_PP_START - 82,
    VPI_PP_SCALING_UNSUPPORTED       = VPI_PP_START - 83,
    VPI_PP_BUSY                      = VPI_PP_START - 128,
    VPI_PP_HW_BUS_ERROR              = VPI_PP_START - 256,
    VPI_PP_HW_TIMEOUT                = VPI_PP_START - 257,
    VPI_PP_DWL_ERROR                 = VPI_PP_START - 258,
    VPI_PP_SYSTEM_ERROR              = VPI_PP_START - 259,
    VPI_PP_DEC_COMBINED_MODE_ERROR   = VPI_PP_START - 512,
    VPI_PP_DEC_RUNTIME_ERROR         = VPI_PP_START - 513,

    /** Driver error. */
    VPI_DRIVER_START                = -5000,
    /** EP | AP memory alloc error type*/
    VPI_ERR_NO_MEM                  = VPI_DRIVER_START - 1,
    VPI_ERR_NO_EP_MEM               = VPI_DRIVER_START - 2,
    VPI_ERR_NO_AP_MEM               = VPI_DRIVER_START - 3,
    /** RC2EP | EP2RC */
    VPI_ERR_RC2EP                   = VPI_DRIVER_START - 4,
    VPI_ERR_EP2RC                   = VPI_DRIVER_START - 5,
    /** DWL error type */
    VPI_DWL_ERROR                   = VPI_DRIVER_START - 20,
    VPI_DWL_HW_RESERVE_FAILED       = VPI_DRIVER_START - 21,
    VPI_DWL_HW_RELEASE_FAILED       = VPI_DRIVER_START - 22,
    VPI_DWL_HW_PUSH_REG_FAILED      = VPI_DRIVER_START - 23,
    VPI_DWL_HW_PULL_REG_FAILED      = VPI_DRIVER_START - 24,
    VPI_DWL_HW_CORE_WAIT_FAILED     = VPI_DRIVER_START - 25,
    /** EWL error type */
    VPI_EWL_ERROR                   = VPI_DRIVER_START - 40,
    VPI_EWL_HW_RESERVE_FAILED       = VPI_DRIVER_START - 41,
    VPI_EWL_HW_CORE_WAIT_FAILED     = VPI_DRIVER_START - 42,
    VPI_EWL_PAR_ERR                 = VPI_DRIVER_START - 43,
    /** VCMD error type */
    VPI_VCMD_ERROR                  = VPI_DRIVER_START - 60,
    VPI_VCMD_RELEASE_FAILED         = VPI_DRIVER_START - 61,
    VPI_VCMD_RESERVE_FAILED         = VPI_DRIVER_START - 62,
    VPI_VCMD_CORE_WAIT_FAILED       = VPI_DRIVER_START - 63,
    VPI_VCMD_LINK_RUN_FAILED        = VPI_DRIVER_START - 64,
    /** Other driver sw error */
    VPI_DRIVER_SW_ERROR             = VPI_DRIVER_START - 128,

    VPI_ERR_XABR_START              = -6000,
    VPI_ERR_XABR_INIT               = VPI_ERR_XABR_START - 1,
    /*Invalid configuration*/
    VPI_ERR_XABR_INVALID_CONFIG     = VPI_ERR_XABR_START - 2,
    VPI_ERR_XABR_PIPELINE           = VPI_ERR_XABR_START - 3,
    VPI_ERR_XABR_INVALID_FORMAT     = VPI_ERR_XABR_START - 4,
    VPI_ERR_XABR_INVALID_RESOLUTION = VPI_ERR_XABR_START - 5,
    VPI_ERR_XABR_PROCESS            = VPI_ERR_XABR_START - 6,

} VpiRet;

enum { VPI_MEMORY_TRANS_RC2EP, VPI_MEMORY_TRANS_EP2RC };

#endif
