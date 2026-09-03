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

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ST2094_10_BLOCK_LEVEL 254
#define MAX_ST2094_40_NUM_WINDOWS 4
#define MAX_ST2094_40_NUM_ROWS_PEAK_LUMINANCE 32
#define MAX_ST2094_40_NUM_COLS_PEAK_LUMINANCE 32
#define MAX_ST2094_40_NUM_DISTRIBUTION_MAXRGB_PERCENTILES 16
#define MAX_ST2094_40_NUM_BEZIER_CURVE_ANCHORS 16
#define MAX_CLOSED_CAPTION_COUNT 32
#define MAX_T35_PAYLOAD_NUM 64
#define DYNAMIC_ENC_PARAMS_RESERVED 100
#define MAX_RPU_DOLBY_VISION_NUM_CMPS 3
#define MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX 9

// XMA_FRAME_SIDE_DATA_DYN_ENC_PARAMS deprecated in v1.1.2, replaced by XmaDynamicEncParams_v2
typedef struct XmaDynamicEncParams {
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
} XmaDynamicEncParams;

// XMA_FRAME_SIDE_DATA_DYN_ENC_PARAMS new in v1.1.2
typedef struct XmaDynamicEncParams_v2 {
  XmaDynamicEncParams v1;
  bool                is_qp_changed;
  uint32_t            qp;
  bool                is_qp_i_offset_changed;
  int32_t             qp_i_offset;
  bool                is_qp_b_offset_changed;
  int32_t             qp_b_offset;
  bool                is_min_bit_rate_changed;
  uint32_t            min_bit_rate_kbps;
  bool                is_max_bit_rate_changed;
  uint32_t            max_bit_rate_kbps;

  uint8_t reserved[DYNAMIC_ENC_PARAMS_RESERVED];
} XmaDynamicEncParams_v2;

// XMA_FRAME_SIDE_DATA_DYN_COMPOSITOR_PARAMS new in v1.2
typedef struct XmaDynCompositorParams {
  bool     is_src_rect_updated;
  uint32_t src_width;  /* crop width */
  uint32_t src_height; /* crop height */
  uint32_t src_x;      /* left position for crop */
  uint32_t src_y;      /* top position for crop */
  bool     is_dst_rect_updated;
  uint32_t dst_width;  /* dst width */
  uint32_t dst_height; /* dst height */
  uint32_t dst_x;      /* left position for output */
  uint32_t dst_y;      /* top position for output */
  bool     is_alpha_updated;
  float    alpha; /* alpha per rectangle */
  bool     is_z_order_updated;
  uint32_t z_order; /* ordering of objects along the Z-axis*/
  bool     is_angle_updated;
  uint32_t angle; /* rotation angle */
  bool     is_flip_updated;
  uint32_t flip; /* flip horizontal or vertical */
  bool     is_border_color_updated;
  uint32_t border_color; /* border color: format is RGB 0x00RRGGBB*/
  bool     is_border_inner_size_updated;
  uint32_t border_inner_size; /* Inner border thickness */
  bool     is_border_outer_size_updated;
  uint32_t border_outer_size; /* Outer border thickness */
} XmaDynCompositorParams;

// XMA_FRAME_SIDE_DATA_SEI_MASTERING_DISPLAY_COLOR_VOLUME
typedef struct XmaMasterDisColVol {
  uint8_t  present_flag;
  uint16_t display_primaries_x[3];
  uint16_t display_primaries_y[3];
  uint16_t white_point_x;
  uint16_t white_point_y;
  uint32_t max_display_mastering_luminance;
  uint32_t min_display_mastering_luminance;
} XmaMasterDisColVol;

// XMA_FRAME_SIDE_DATA_SEI_CONTENT_LIGHT_LEVEL
typedef struct XmaContentLightLevel {
  uint8_t  present_flag;
  uint16_t max_content_light_level;
  uint16_t max_pic_average_light_level;
} XmaContentLightLevel;

// XMA_FRAME_SIDE_DATA_SEI_ATC
typedef struct XmaATCInfo {
  uint8_t present_flag;
  uint8_t preferred_transfer_characteristics;
} XmaATCInfo;

typedef struct XmaSMPTE_ST2094_10 {
  uint16_t itu_t_t35_terminal_provider_code;
  uint16_t itu_t_t35_terminal_provider_oriented_code;

  uint32_t app_identifier;
  uint32_t app_version;
  uint8_t  metadata_refresh_flag;
  uint8_t  num_ext_blocks;

  uint32_t ext_block_length[MAX_ST2094_10_BLOCK_LEVEL];
  uint8_t  ext_block_level[MAX_ST2094_10_BLOCK_LEVEL];

  /* ext_block_level[i] == 1 */
  uint16_t min_PQ[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t max_PQ[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t avg_PQ[MAX_ST2094_10_BLOCK_LEVEL];

  /* ext_block_level[i] == 2 */
  uint16_t target_max_PQ[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t trim_slope[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t trim_offset[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t trim_power[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t trip_chroma_weight[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t trim_saturation_grin[MAX_ST2094_10_BLOCK_LEVEL];
  int16_t  ms_weight[MAX_ST2094_10_BLOCK_LEVEL];

  /* ext_block_level[i] == 5 */
  uint16_t active_area_left_offset[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t active_area_right_offset[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t active_area_top_offset[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t active_area_bottom_offset[MAX_ST2094_10_BLOCK_LEVEL];
} XmaSMPTE_ST2094_10;

// XMA_FRAME_SIDE_DATA_SEI_DOLBY_VISION, added in v1.1
typedef struct XmaSMPTE_ST2094_10_v2 {
  uint8_t            country_code;
  uint8_t            extended_country_code;
  XmaSMPTE_ST2094_10 st2094_10;
} XmaSMPTE_ST2094_10_v2;

typedef struct XmaSMPTE_ST2094_40 {
  uint16_t itu_t_t35_terminal_provider_code;
  uint16_t itu_t_t35_terminal_provider_oriented_code;

  uint8_t application_identifier;
  uint8_t application_version;
  uint8_t num_windows;

  uint16_t window_upper_left_corner_x[MAX_ST2094_40_NUM_WINDOWS];
  uint16_t window_upper_left_corner_y[MAX_ST2094_40_NUM_WINDOWS];
  uint16_t window_lower_right_corner_x[MAX_ST2094_40_NUM_WINDOWS];
  uint16_t window_lower_right_corner_y[MAX_ST2094_40_NUM_WINDOWS];
  uint16_t center_of_ellipse_x[MAX_ST2094_40_NUM_WINDOWS];
  uint16_t center_of_ellipse_y[MAX_ST2094_40_NUM_WINDOWS];
  uint8_t  rotation_angle[MAX_ST2094_40_NUM_WINDOWS];
  uint16_t semimajor_axis_internal_ellipse[MAX_ST2094_40_NUM_WINDOWS];
  uint16_t semimajor_axis_external_ellipse[MAX_ST2094_40_NUM_WINDOWS];
  uint16_t semiminor_axis_external_ellipse[MAX_ST2094_40_NUM_WINDOWS];
  uint8_t  overlap_process_option[MAX_ST2094_40_NUM_WINDOWS];

  uint32_t targeted_system_display_maximum_luminance;
  uint8_t  targeted_system_display_actual_peak_luminance_flag;
  uint8_t  num_rows_targeted_system_display_actual_peak_luminance;
  uint8_t  num_cols_targeted_system_display_actual_peak_luminance;

  uint8_t  targeted_system_display_actual_peak_luminance[MAX_ST2094_40_NUM_ROWS_PEAK_LUMINANCE][MAX_ST2094_40_NUM_COLS_PEAK_LUMINANCE];
  uint32_t maxscl[MAX_ST2094_40_NUM_WINDOWS][XMA_MAX_PLANES];
  uint32_t average_maxrgb[MAX_ST2094_40_NUM_WINDOWS];
  uint8_t  num_distribution_maxrgb_percentiles[MAX_ST2094_40_NUM_WINDOWS];
  uint8_t  distribution_maxrgb_percentages[MAX_ST2094_40_NUM_WINDOWS][MAX_ST2094_40_NUM_DISTRIBUTION_MAXRGB_PERCENTILES];
  uint32_t distribution_maxrgb_percentiles[MAX_ST2094_40_NUM_WINDOWS][MAX_ST2094_40_NUM_DISTRIBUTION_MAXRGB_PERCENTILES];
  uint16_t fraction_bright_pixels[MAX_ST2094_40_NUM_WINDOWS];

  uint8_t mastering_display_actual_peak_luminance_flag;
  uint8_t num_rows_mastering_display_actual_peak_luminance;
  uint8_t num_cols_mastering_display_actual_peak_luminance;

  uint8_t  mastering_display_actual_peak_luminance[MAX_ST2094_40_NUM_ROWS_PEAK_LUMINANCE][MAX_ST2094_40_NUM_COLS_PEAK_LUMINANCE];
  uint8_t  tone_mapping_flag[MAX_ST2094_40_NUM_WINDOWS];
  uint16_t knee_point_x[MAX_ST2094_40_NUM_WINDOWS];
  uint16_t knee_point_y[MAX_ST2094_40_NUM_WINDOWS];
  uint8_t  num_bezier_curve_anchors[MAX_ST2094_40_NUM_WINDOWS];
  uint16_t bezier_curve_anchors[MAX_ST2094_40_NUM_WINDOWS][MAX_ST2094_40_NUM_BEZIER_CURVE_ANCHORS];
  uint8_t  color_saturation_mapping_flag[MAX_ST2094_40_NUM_WINDOWS];
  uint8_t  color_saturation_weight[MAX_ST2094_40_NUM_WINDOWS];
} XmaSMPTE_ST2094_40;

// XMA_FRAME_SIDE_DATA_SEI_HDR10_PLUS, added in v1.1
typedef struct XmaSMPTE_ST2094_40_v2 {
  uint8_t            country_code;
  uint8_t            extended_country_code;
  XmaSMPTE_ST2094_40 st2094_40;
} XmaSMPTE_ST2094_40_v2;

// XMA_FRAME_SIDE_DATA_SEI_RAW_T35_DATA, added in v1.1
typedef struct XmaRawT35Data {
  uint32_t payload_byte_length;
  uint8_t* payload_byte_data;
} XmaRawT35Data;

// XMA_FRAME_SIDE_DATA_SEI_CLOSED_CAPTION, added in v1.2
typedef struct XmaA53ClosedCaption {
  uint8_t  country_code;
  uint8_t  extended_country_code;
  uint16_t itu_t_t35_provider_code;
  uint32_t user_identifier;
  uint8_t  user_data_type_code;
  uint8_t  process_em_data_flag; /* just 1 bit is valid */
  uint8_t  process_cc_data_flag; /* just 1 bit is valid */
  uint8_t  additional_data_flag; /* just 1 bit is valid */
  uint8_t  cc_count;             /* just 5 bit is valid */
  uint8_t  em_data;
  uint8_t  cc_valid[MAX_CLOSED_CAPTION_COUNT]; /* just 1 bit is valid */
  uint8_t  cc_type[MAX_CLOSED_CAPTION_COUNT];  /* just 2 bit is valid */
  uint8_t  cc_data_1[MAX_CLOSED_CAPTION_COUNT];
  uint8_t  cc_data_2[MAX_CLOSED_CAPTION_COUNT];
} XmaA53ClosedCaption;

typedef struct XmaT35Params {
  union meta {
    XmaSMPTE_ST2094_10 st2094_10;
    XmaSMPTE_ST2094_40 st2094_40;
  } meta[MAX_T35_PAYLOAD_NUM];

  uint8_t  payload_type[MAX_T35_PAYLOAD_NUM];
  uint32_t payload_byte_length[MAX_T35_PAYLOAD_NUM];
  uint8_t* payload_byte_data[MAX_T35_PAYLOAD_NUM];
  uint8_t  country_code[MAX_T35_PAYLOAD_NUM];
  uint8_t  extended_country_code[MAX_T35_PAYLOAD_NUM];
  uint8_t  payload_count;
} XmaT35Params;

// XMA_FRAME_SIDE_DATA_HDR10_PARAMS, deprecated in v1.1
typedef struct XmaHdrParams {
  XmaMasterDisColVol   mdcv;
  XmaContentLightLevel cll;
  XmaATCInfo           atc;
  XmaT35Params*        t35;
} XmaHdrParams;

typedef struct XmaVuiColorDescription {
  uint8_t colour_primaries;
  uint8_t transfer_characteristics;
  uint8_t matrix_coeffs;
} XmaVuiColorDescription;

// XMA_FRAME_SIDE_DATA_VUI_PARAMS
typedef struct XmaVuiParams {
  int32_t                sar_width;
  int32_t                sar_height;
  int32_t                vui_video_signal_type_present_flag;
  int32_t                vui_video_format;
  int32_t                vui_video_full_range;
  int32_t                color_description_present_flag;
  XmaVuiColorDescription color_description;
} XmaVuiParams;

// XMA_FRAME_SIDE_DATA_RPU_RAW_DATA, added in v1.2
typedef struct XmaRpuRawData {
  uint32_t payload_byte_length;
  uint8_t* payload_byte_data;
} XmaRpuRawData;

// XMA_FRAME_SIDE_DATA_RPU_DOLBY_VISION, added in v1.2
typedef struct XmaRpuDolbyVision {
  uint8_t  rpu_type;
  uint16_t rpu_format;
  uint8_t  vdr_rpu_profile;
  uint8_t  vdr_rpu_level;
  uint8_t  vdr_seq_info_present_flag;
  uint8_t  chroma_resampling_explicit_filter_flag;
  uint8_t  coefficient_data_type;
  uint8_t  coefficient_log2_denom;
  uint8_t  vdr_rpu_normalized_idc;
  uint8_t  bl_video_full_range_flag;
  uint8_t  bl_bit_depth_minus8;
  uint8_t  el_bit_depth_minus8;
  uint8_t  vdr_bit_depth_minus8;
  uint8_t  spatial_resampling_filter_flag;
  uint8_t  el_spatial_resampling_filter_flag;
  uint8_t  disable_residual_flag;
  uint8_t  vdr_dm_metadata_present_flag;
  uint8_t  use_prev_vdr_rpu_flag;
  uint8_t  prev_vdr_rpu_id;
  uint8_t  vdr_rpu_id;
  uint8_t  mapping_color_space;
  uint8_t  mapping_chroma_format_idc;
  uint8_t  num_pivots_minus2[MAX_RPU_DOLBY_VISION_NUM_CMPS];
  uint16_t pred_pivot_value[MAX_RPU_DOLBY_VISION_NUM_CMPS][MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX];
  uint8_t  nlq_method_idc;
  uint32_t num_x_partitions_minus1;
  uint32_t num_y_partitions_minus1;

  uint8_t  mapping_idc[MAX_RPU_DOLBY_VISION_NUM_CMPS][MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX];
  uint8_t  poly_order_minus1[MAX_RPU_DOLBY_VISION_NUM_CMPS][MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX];
  uint8_t  linear_interp_flag[MAX_RPU_DOLBY_VISION_NUM_CMPS][MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX];
  uint32_t poly_coef_int[MAX_RPU_DOLBY_VISION_NUM_CMPS][MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX][MAX_RPU_DOLBY_VISION_NUM_CMPS];
  uint32_t poly_coef[MAX_RPU_DOLBY_VISION_NUM_CMPS][MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX][MAX_RPU_DOLBY_VISION_NUM_CMPS];
  uint8_t  mmr_order_minus1[MAX_RPU_DOLBY_VISION_NUM_CMPS][MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX];
  uint32_t mmr_constant_int[MAX_RPU_DOLBY_VISION_NUM_CMPS][MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX];
  uint32_t mmr_constant[MAX_RPU_DOLBY_VISION_NUM_CMPS][MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX];
  uint32_t mmr_coef_int[MAX_RPU_DOLBY_VISION_NUM_CMPS][MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX][MAX_RPU_DOLBY_VISION_NUM_CMPS][MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX];
  uint32_t mmr_coef[MAX_RPU_DOLBY_VISION_NUM_CMPS][MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX][MAX_RPU_DOLBY_VISION_NUM_CMPS][MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX];

  uint16_t nlq_offset[MAX_RPU_DOLBY_VISION_NUM_CMPS];
  uint32_t vdr_in_max_int[MAX_RPU_DOLBY_VISION_NUM_CMPS];
  uint32_t vdr_in_max[MAX_RPU_DOLBY_VISION_NUM_CMPS];
  uint32_t linear_deadzone_slope_int[MAX_RPU_DOLBY_VISION_NUM_CMPS];
  uint32_t linear_deadzone_slope[MAX_RPU_DOLBY_VISION_NUM_CMPS];
  uint32_t linear_deadzone_threshold_int[MAX_RPU_DOLBY_VISION_NUM_CMPS];
  uint32_t linear_deadzone_threshold[MAX_RPU_DOLBY_VISION_NUM_CMPS];

  uint8_t  affected_dm_metadata_id;
  uint8_t  current_dm_metadata_id;
  uint8_t  scene_refresh_flag;
  int16_t  ycc_to_rgb_coef[MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX];
  uint32_t ycc_to_rgb_offset[MAX_RPU_DOLBY_VISION_NUM_CMPS];
  int16_t  rgb_to_lms_coef[MAX_RPU_DOLBY_VISION_NUM_PIVOT_IDX];
  uint16_t signal_eotf;
  uint16_t signal_eotf_param0;
  uint16_t signal_eotf_param1;
  uint32_t signal_eotf_param2;
  uint8_t  signal_bit_depth;
  uint8_t  signal_color_space;
  uint8_t  signal_chroma_format;
  uint8_t  signal_full_range_flag;
  uint16_t source_min_pq;
  uint16_t source_max_pq;
  uint16_t source_diagonal;
  uint8_t  num_ext_blocks;
  uint32_t ext_block_length[MAX_ST2094_10_BLOCK_LEVEL];
  uint8_t  ext_block_level[MAX_ST2094_10_BLOCK_LEVEL];

  /* ext_block_level[i] == 1 */
  uint16_t min_PQ[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t max_PQ[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t avg_PQ[MAX_ST2094_10_BLOCK_LEVEL];

  /* ext_block_level[i] == 2 */
  uint16_t target_max_PQ[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t trim_slope[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t trim_offset[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t trim_power[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t trip_chroma_weight[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t trim_saturation_gain[MAX_ST2094_10_BLOCK_LEVEL];
  int16_t  ms_weight[MAX_ST2094_10_BLOCK_LEVEL];

  /* ext_block_level[i] == 5 */
  uint16_t active_area_left_offset[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t active_area_right_offset[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t active_area_top_offset[MAX_ST2094_10_BLOCK_LEVEL];
  uint16_t active_area_bottom_offset[MAX_ST2094_10_BLOCK_LEVEL];
} XmaRpuDolbyVision;

#ifdef __cplusplus
}
#endif
