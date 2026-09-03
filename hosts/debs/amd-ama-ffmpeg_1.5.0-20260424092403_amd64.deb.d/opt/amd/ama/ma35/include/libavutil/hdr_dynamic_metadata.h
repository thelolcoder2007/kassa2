/*
 * Copyright (c) 2018 Mohammad Izadi <moh.izadi at gmail.com>
 * Modifications Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVUTIL_HDR_DYNAMIC_METADATA_H
#define AVUTIL_HDR_DYNAMIC_METADATA_H

#include "frame.h"
#include "rational.h"

#if CONFIG_VPE
#define MAX_BLOCK_LEVEL 254
#define NUM_CMPS 3
#define NUM_PIVOT_IDX 9
#endif
/**
 * Option for overlapping elliptical pixel selectors in an image.
 */
enum AVHDRPlusOverlapProcessOption {
    AV_HDR_PLUS_OVERLAP_PROCESS_WEIGHTED_AVERAGING = 0,
    AV_HDR_PLUS_OVERLAP_PROCESS_LAYERING = 1,
};

/**
 * Represents the percentile at a specific percentage in
 * a distribution.
 */
typedef struct AVHDRPlusPercentile {
    /**
     * The percentage value corresponding to a specific percentile linearized
     * RGB value in the processing window in the scene. The value shall be in
     * the range of 0 to100, inclusive.
     */
    uint8_t percentage;

    /**
     * The linearized maxRGB value at a specific percentile in the processing
     * window in the scene. The value shall be in the range of 0 to 1, inclusive
     * and in multiples of 0.00001.
     */
    AVRational percentile;
} AVHDRPlusPercentile;

/**
 * Color transform parameters at a processing window in a dynamic metadata for
 * SMPTE 2094-40.
 */
typedef struct AVHDRPlusColorTransformParams {
    /**
     * The relative x coordinate of the top left pixel of the processing
     * window. The value shall not exceed 65535. The value for
     * first processing window shall be 0.
     */
    AVRational window_upper_left_corner_x;

    /**
     * The relative y coordinate of the top left pixel of the processing
     * window. The value shall not exceed 65535. The value for
     * first processing window shall be 0.
     */
    AVRational window_upper_left_corner_y;

    /**
     * The relative x coordinate of the bottom right pixel of the processing
     * window. The value shall not exceed 65535. The value for
     * first processing window shall be (width of Picture - 1).
     */
    AVRational window_lower_right_corner_x;

    /**
     * The relative y coordinate of the bottom right pixel of the processing
     * window. The value shall not exceed 65535. The value for
     * first processing window shall be (height of Picture - 1).
     */
    AVRational window_lower_right_corner_y;

    /**
     * The x coordinate of the center position of the concentric internal and
     * external ellipses of the elliptical pixel selector in the processing
     * window. The value shall be in the range of 0 to (width of Picture - 1),
     * inclusive and in multiples of 1 pixel.
     */
    uint16_t center_of_ellipse_x;

    /**
     * The y coordinate of the center position of the concentric internal and
     * external ellipses of the elliptical pixel selector in the processing
     * window. The value shall be in the range of 0 to (height of Picture - 1),
     * inclusive and in multiples of 1 pixel.
     */
    uint16_t center_of_ellipse_y;

    /**
     * The clockwise rotation angle in degree of arc with respect to the
     * positive direction of the x-axis of the concentric internal and external
     * ellipses of the elliptical pixel selector in the processing window. The
     * value shall be in the range of 0 to 180, inclusive and in multiples of 1.
     */
    uint8_t rotation_angle;

    /**
     * The semi-major axis value of the internal ellipse of the elliptical pixel
     * selector in amount of pixels in the processing window. The value shall be
     * in the range of 1 to 65535, inclusive and in multiples of 1 pixel.
     */
    uint16_t semimajor_axis_internal_ellipse;

    /**
     * The semi-major axis value of the external ellipse of the elliptical pixel
     * selector in amount of pixels in the processing window. The value
     * shall not be less than semimajor_axis_internal_ellipse of the current
     * processing window. The value shall be in the range of 1 to 65535,
     * inclusive and in multiples of 1 pixel.
     */
    uint16_t semimajor_axis_external_ellipse;

    /**
     * The semi-minor axis value of the external ellipse of the elliptical pixel
     * selector in amount of pixels in the processing window. The value shall be
     * in the range of 1 to 65535, inclusive and in multiples of 1 pixel.
     */
    uint16_t semiminor_axis_external_ellipse;

    /**
     * Overlap process option indicates one of the two methods of combining
     * rendered pixels in the processing window in an image with at least one
     * elliptical pixel selector. For overlapping elliptical pixel selectors
     * in an image, overlap_process_option shall have the same value.
     */
    enum AVHDRPlusOverlapProcessOption overlap_process_option;

    /**
     * The maximum of the color components of linearized RGB values in the
     * processing window in the scene. The values should be in the range of 0 to
     * 1, inclusive and in multiples of 0.00001. maxscl[ 0 ], maxscl[ 1 ], and
     * maxscl[ 2 ] are corresponding to R, G, B color components respectively.
     */
    AVRational maxscl[3];

    /**
     * The average of linearized maxRGB values in the processing window in the
     * scene. The value should be in the range of 0 to 1, inclusive and in
     * multiples of 0.00001.
     */
    AVRational average_maxrgb;

    /**
     * The number of linearized maxRGB values at given percentiles in the
     * processing window in the scene. The maximum value shall be 15.
     */
    uint8_t num_distribution_maxrgb_percentiles;

    /**
     * The linearized maxRGB values at given percentiles in the
     * processing window in the scene.
     */
    AVHDRPlusPercentile distribution_maxrgb[15];

    /**
     * The fraction of selected pixels in the image that contains the brightest
     * pixel in the scene. The value shall be in the range of 0 to 1, inclusive
     * and in multiples of 0.001.
     */
    AVRational fraction_bright_pixels;

    /**
     * This flag indicates that the metadata for the tone mapping function in
     * the processing window is present (for value of 1).
     */
    uint8_t tone_mapping_flag;

    /**
     * The x coordinate of the separation point between the linear part and the
     * curved part of the tone mapping function. The value shall be in the range
     * of 0 to 1, excluding 0 and in multiples of 1/4095.
     */
    AVRational knee_point_x;

    /**
     * The y coordinate of the separation point between the linear part and the
     * curved part of the tone mapping function. The value shall be in the range
     * of 0 to 1, excluding 0 and in multiples of 1/4095.
     */
    AVRational knee_point_y;

    /**
     * The number of the intermediate anchor parameters of the tone mapping
     * function in the processing window. The maximum value shall be 15.
     */
    uint8_t num_bezier_curve_anchors;

    /**
     * The intermediate anchor parameters of the tone mapping function in the
     * processing window in the scene. The values should be in the range of 0
     * to 1, inclusive and in multiples of 1/1023.
     */
    AVRational bezier_curve_anchors[15];

    /**
     * This flag shall be equal to 0 in bitstreams conforming to this version of
     * this Specification. Other values are reserved for future use.
     */
    uint8_t color_saturation_mapping_flag;

    /**
     * The color saturation gain in the processing window in the scene. The
     * value shall be in the range of 0 to 63/8, inclusive and in multiples of
     * 1/8. The default value shall be 1.
     */
    AVRational color_saturation_weight;
} AVHDRPlusColorTransformParams;

/**
 * This struct represents dynamic metadata for color volume transform -
 * application 4 of SMPTE 2094-40:2016 standard.
 *
 * To be used as payload of a AVFrameSideData or AVPacketSideData with the
 * appropriate type.
 *
 * @note The struct should be allocated with
 * av_dynamic_hdr_plus_alloc() and its size is not a part of
 * the public ABI.
 */
typedef struct AVDynamicHDRPlus {
    /**
     * Country code by Rec. ITU-T T.35 Annex A. The value shall be 0xB5.
     */
    uint8_t itu_t_t35_country_code;

    /**
     * Application version in the application defining document in ST-2094
     * suite. The value shall be set to 0.
     */
    uint8_t application_version;

    /**
     * The number of processing windows. The value shall be in the range
     * of 1 to 3, inclusive.
     */
    uint8_t num_windows;

    /**
     * The color transform parameters for every processing window.
     */
    AVHDRPlusColorTransformParams params[3];

    /**
     * The nominal maximum display luminance of the targeted system display,
     * in units of 0.0001 candelas per square metre. The value shall be in
     * the range of 0 to 10000, inclusive.
     */
    AVRational targeted_system_display_maximum_luminance;

    /**
     * This flag shall be equal to 0 in bit streams conforming to this version
     * of this Specification. The value 1 is reserved for future use.
     */
    uint8_t targeted_system_display_actual_peak_luminance_flag;

    /**
     * The number of rows in the targeted system_display_actual_peak_luminance
     * array. The value shall be in the range of 2 to 25, inclusive.
     */
    uint8_t num_rows_targeted_system_display_actual_peak_luminance;

    /**
     * The number of columns in the
     * targeted_system_display_actual_peak_luminance array. The value shall be
     * in the range of 2 to 25, inclusive.
     */
    uint8_t num_cols_targeted_system_display_actual_peak_luminance;

    /**
     * The normalized actual peak luminance of the targeted system display. The
     * values should be in the range of 0 to 1, inclusive and in multiples of
     * 1/15.
     */
    AVRational targeted_system_display_actual_peak_luminance[25][25];

    /**
     * This flag shall be equal to 0 in bitstreams conforming to this version of
     * this Specification. The value 1 is reserved for future use.
     */
    uint8_t mastering_display_actual_peak_luminance_flag;

    /**
     * The number of rows in the mastering_display_actual_peak_luminance array.
     * The value shall be in the range of 2 to 25, inclusive.
     */
    uint8_t num_rows_mastering_display_actual_peak_luminance;

    /**
     * The number of columns in the mastering_display_actual_peak_luminance
     * array. The value shall be in the range of 2 to 25, inclusive.
     */
    uint8_t num_cols_mastering_display_actual_peak_luminance;

    /**
     * The normalized actual peak luminance of the mastering display used for
     * mastering the image essence. The values should be in the range of 0 to 1,
     * inclusive and in multiples of 1/15.
     */
    AVRational mastering_display_actual_peak_luminance[25][25];
} AVDynamicHDRPlus;

/**
 * Allocate an AVDynamicHDRPlus structure and set its fields to
 * default values. The resulting struct can be freed using av_freep().
 *
 * @return An AVDynamicHDRPlus filled with default values or NULL
 *         on failure.
 */
AVDynamicHDRPlus *av_dynamic_hdr_plus_alloc(size_t *size);

/**
 * Allocate a complete AVDynamicHDRPlus and add it to the frame.
 * @param frame The frame which side data is added to.
 *
 * @return The AVDynamicHDRPlus structure to be filled by caller or NULL
 *         on failure.
 */
AVDynamicHDRPlus *av_dynamic_hdr_plus_create_side_data(AVFrame *frame);

/**
 * Parse the user data registered ITU-T T.35 to AVbuffer (AVDynamicHDRPlus).
 * The T.35 buffer must begin with the application mode, skipping the
 * country code, terminal provider codes, and application identifier.
 * @param s A pointer containing the decoded AVDynamicHDRPlus structure.
 * @param data The byte array containing the raw ITU-T T.35 data.
 * @param size Size of the data array in bytes.
 *
 * @return >= 0 on success. Otherwise, returns the appropriate AVERROR.
 */
int av_dynamic_hdr_plus_from_t35(AVDynamicHDRPlus *s, const uint8_t *data,
                                 size_t size);

#define AV_HDR_PLUS_MAX_PAYLOAD_SIZE 907

/**
 * Serialize dynamic HDR10+ metadata to a user data registered ITU-T T.35 buffer,
 * excluding the first 48 bytes of the header, and beginning with the application mode.
 * @param s A pointer containing the decoded AVDynamicHDRPlus structure.
 * @param[in,out] data A pointer to pointer to a byte buffer to be filled with the
 *                     serialized metadata.
 *                     If *data is NULL, a buffer be will be allocated and a pointer to
 *                     it stored in its place. The caller assumes ownership of the buffer.
 *                     May be NULL, in which case the function will only store the
 *                     required buffer size in *size.
 * @param[in,out] size A pointer to a size to be set to the returned buffer's size.
 *                     If *data is not NULL, *size must contain the size of the input
 *                     buffer. May be NULL only if *data is NULL.
 *
 * @return >= 0 on success. Otherwise, returns the appropriate AVERROR.
 */
int av_dynamic_hdr_plus_to_t35(const AVDynamicHDRPlus *s, uint8_t **data, size_t *size);

#if CONFIG_VPE
/**
 * This struct represents raw data of user_data_registered_itu_t_t35.
 *
 * To be used as payload of a AVFrameSideData or AVPacketSideData with the
 * appropriate type.
 *
 * @note The struct should be allocated with
 * av_raw_t35_data_alloc() and its size is not a part of
 * the public ABI.
 */
typedef struct AVRawT35Data {
    /**
     * The length of payload_byte of raw t35 data.
     */
    uint32_t payload_byte_length;

    /**
     * The payload_byte of raw t35 data
     */
    uint8_t *payload_byte_data;
}AVRawT35Data;

/**
 * Allocate an AVRawT35Data structure and set its fields to
 * default values. The resulting struct can be freed using av_freep().
 *
 * @return An AVRawT35Data filled with default values or NULL
 *         on failure.
 */
AVRawT35Data *av_raw_t35_data_alloc(size_t *size);

/**
 * Allocate a complete AVRawT35Data and add it to the frame.
 * @param frame The frame which side data is added to.
 *
 * @return The AVRawT35Data structure to be filled by caller or NULL
 *         on failure.
 */
AVRawT35Data *av_raw_t35_data_create_side_data(AVFrame *frame);

typedef struct AVRawRpuData {
    /**
     * The length of payload_byte of raw rpu data.
     */
    uint32_t payload_byte_length;

    /**
     * The payload_byte of raw rpu data
     */
    uint8_t *payload_byte_data;
}AVRawRpuData;

/**
 * Allocate an AVRawRpuData structure and set its fields to
 * default values. The resulting struct can be freed using av_freep().
 *
 * @return An AVRawRpuData filled with default values or NULL
 *         on failure.
 */
AVRawRpuData *av_raw_rpu_data_alloc(size_t *size);

/**
 * Allocate a complete AVRawRpuData and add it to the frame.
 * @param frame The frame which side data is added to.
 *
 * @return The AVRawRpuData structure to be filled by caller or NULL
 *         on failure.
 */
AVRawRpuData *av_raw_rpu_data_create_side_data(AVFrame *frame);

typedef struct AVVuiParameters {
    uint16_t sar_width;
    uint16_t sar_height;
    uint8_t vui_video_signal_type_present_flag;
    uint8_t vui_video_format;
    uint8_t vui_video_full_range;
    uint8_t color_description_present_flag;
    uint8_t colour_primaries;
    uint8_t transfer_characteristics;
    uint8_t matrix_coeffs;
}AVVuiParameters;

/**
 * This struct represents dynamic metadata for color volume transform -
 * application 1 of SMPTE 2094-10:2016 standard.
 *
 * To be used as payload of a AVFrameSideData or AVPacketSideData with the
 * appropriate type.
 *
 * @note The struct should be allocated with
 * av_dynamic_dolby_vision_alloc() and its size is not a part of
 * the public ABI.
 */
typedef struct AVDynamicDolbyVision {
    /**
     * Country code by Rec. ITU-T T.35 Annex A. The value shall be 0xB5.
     */
    uint8_t itu_t_t35_country_code;
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

} AVDynamicDolbyVision;

/**
 * Allocate an AVDynamicDolbyVision structure and set its fields to
 * default values. The resulting struct can be freed using av_freep().
 *
 * @return An AVDynamicDolbyVision filled with default values or NULL
 *         on failure.
 */
AVDynamicDolbyVision *av_dynamic_dolby_vision_alloc(size_t *size);

/**
 * Allocate a complete AVDynamicDolbyVision and add it to the frame.
 * @param frame The frame which side data is added to.
 *
 * @return The AVDynamicDolbyVision structure to be filled by caller or NULL
 *         on failure.
 */
AVDynamicDolbyVision *av_dynamic_dolby_vision_create_side_data(AVFrame *frame);

typedef struct AVDolbyVisionRpu {

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

} AVDolbyVisionRpu;

/**
 * Allocate an AVDolbyVisionRpu structure and set its fields to
 * default values. The resulting struct can be freed using av_freep().
 *
 * @return An AVDolbyVisionRpu filled with default values or NULL
 *         on failure.
 */
AVDolbyVisionRpu *av_dolby_vision_rpu_alloc(size_t *size);

/**
 * Allocate a complete AVDolbyVisionRpu and add it to the frame.
 * @param frame The frame which side data is added to.
 *
 * @return The AVDolbyVisionRpu structure to be filled by caller or NULL
 *         on failure.
 */
AVDolbyVisionRpu *av_dolby_vision_rpu_create_side_data(AVFrame *frame);
#endif
#endif /* AVUTIL_HDR_DYNAMIC_METADATA_H */
