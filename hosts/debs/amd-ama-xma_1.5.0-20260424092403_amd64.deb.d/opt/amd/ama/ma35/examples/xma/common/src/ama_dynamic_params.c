/* SPDX-License-Identifier: LGPL-3.0-or-later OR Apache-2.0 */

/*
 * Copyright (C) 2022 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * Copyright (C) 2022, Xilinx Inc - All rights reserved
 *
 * AMA dynamic params interface
 *
 * This file is dual-licensed; you may select either the GNU
 * Lesser General Public License version 3 or
 * Apache License, Version 2.0.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>

#include "xma.h"

#include "ama_dynamic_params.h"

#define AMA_DYNAMIC_PARAMS "ama_dynamic_params"

/**
 * check_dyn_param_value: Check if the dynamic encoder parameter is valid
 *
 * @param log Handle to log
 * @param value dynamic param value
 * @param key dynamic param key
 * @param min minimum value
 * @param max maximum value
 * @param current_frame Frame number in config file
 * @return 0 on success or -1 on error
 */
static int check_dyn_param_value(XmaLogHandle log, long value, char* key, long min, long max, size_t current_frame) {
  if (value < min || value > max) {
    xma_logmsg(log, XMA_ERROR_LOG, AMA_DYNAMIC_PARAMS, "Invalid value=%ld for key=%s at frame num %zu in dynamic config file\n", value, key, current_frame);
    return -1;
  }
  return 0;
}

/**
 * trim_space: Remove space or tab in the parsed string
 *
 * @param str Pointer to parsed string
 * @return NONE
 */
static void trim_space(char* str) {
  int count = 0;
  for (int i = 0; str[i]; i++) {
    if (!isspace(str[i])) {
      str[count++] = str[i];
    }
  }
  str[count] = '\0';
}

/**
 * get_long_val: Converts string to integer
 *
 * @param str Input string
 * @return Integer value
 */
static inline long get_long_val(char* str) {
  char* end;
  long  i = strtol(str, &end, 10);
  return i;
}

#ifdef DEBUG
static int32_t print_dyn_params(XmaLogHandle log, XmaDynamicEncParams_v2* dyn_params) {
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] is_bit_rate_changed = %d\n", dyn_params->v1.is_bit_rate_changed);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] bit_rate_kbps = %d\n", dyn_params->v1.bit_rate);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] is_min_bit_rate_changed = %d\n", dyn_params->is_min_bit_rate_changed);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] min_bit_rate_kbps = %d\n", dyn_params->min_bit_rate_kbps);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] is_max_bit_rate_changed = %d\n", dyn_params->is_max_bit_rate_changed);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] max_bit_rate_kbps = %d\n", dyn_params->max_bit_rate_kbps);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] is_b_frames_changed = %d\n", dyn_params->v1.is_b_frames_changed);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] num_b_frames = %d\n", dyn_params->v1.num_b_frames);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] is_qp_changed = %d\n", dyn_params->is_qp_changed);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] qp = %d\n", dyn_params->qp);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] is_min_qp_changed = %d\n", dyn_params->v1.is_min_qp_changed);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] is_max_qp_changed = %d\n", dyn_params->v1.is_max_qp_changed);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] min_qp = %d\n", dyn_params->v1.min_qp);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] max_qp = %d\n", dyn_params->v1.max_qp);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] is_qp_i_offset_changed = %d\n", dyn_params->is_qp_i_offset_changed);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] qp_i_offset = %d\n", dyn_params->qp_i_offset);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] is_qp_b_offset_changed = %d\n", dyn_params->is_qp_b_offset_changed);
  xma_logmsg(log, XMA_DEBUG_LOG, AMA_DYNAMIC_PARAMS, "[Dynamic Params] qp_b_offset = %d\n", dyn_params->qp_b_offset);
  return 0;
}
#endif

/**
 * parse_dyn_param_value: Extracts key value pair and stores in dynamic
 * encoder parameter structure
 *
 * @param log Handle to log
 * @param str Pointer to a single parsed key-value pair string
 * @param current_frame The current frame number
 * @param dyn_params Pointer to VpiDynamicParams structure
 * @return 1 on success and paraams changed, 0 on success but no params changed, or -1 on error
 */
static int parse_dyn_param_value(XmaLogHandle log, char* str, size_t current_frame, XmaDynamicEncParams_v2* dyn_params) {
  const char delimiters[] = "=";
  char*      save_ptr;
  char*      key          = strtok_r(str, delimiters, &save_ptr);
  char*      parsed_value = NULL;
  long       parsed_num   = 0;
  int        ret          = 0;
  if (key == NULL) {
    return ret;
  }

  parsed_value = strtok_r(NULL, delimiters, &save_ptr);
  if (parsed_value == NULL) {
    xma_logmsg(log, XMA_ERROR_LOG, AMA_DYNAMIC_PARAMS, "Missing value for key=%s in dynamic params config file for frame=%zu \n", key, current_frame);
    return -1;
  }
  parsed_num = get_long_val(parsed_value);
  if (!strcmp(key, DYNAMIC_PARAM_B_FRAMES_KEY)) {
    if ((ret = check_dyn_param_value(log, parsed_num, key, DYNAMIC_PARAM_MIN_BFRAMES, DYNAMIC_PARAM_MAX_BFRAMES, current_frame)) != -1) {
      dyn_params->v1.is_b_frames_changed = true;
      dyn_params->v1.num_b_frames        = parsed_num;
      ret                                = 1;
    }
  } else if (!strcmp(key, DYNAMIC_PARAM_BITRATE_KEY)) {
    if ((ret = check_dyn_param_value(log, parsed_num, key, DYNAMIC_PARAM_MIN_BITRATE, DYNAMIC_PARAM_MAX_BITRATE, current_frame)) != -1) {
      dyn_params->v1.is_bit_rate_changed = true;
      dyn_params->v1.bit_rate_kbps       = parsed_num;
      ret                                = 1;
    }
  } else if (!strcmp(key, DYNAMIC_PARAM_MIN_BITRATE_KEY)) {
    if ((ret = check_dyn_param_value(log, parsed_num, key, DYNAMIC_PARAM_MIN_BITRATE, DYNAMIC_PARAM_MAX_BITRATE, current_frame)) != -1) {
      dyn_params->is_min_bit_rate_changed = true;
      dyn_params->min_bit_rate_kbps       = parsed_num;
    }
  } else if (!strcmp(key, DYNAMIC_PARAM_MAX_BITRATE_KEY)) {
    if ((ret = check_dyn_param_value(log, parsed_num, key, DYNAMIC_PARAM_MIN_BITRATE, DYNAMIC_PARAM_MAX_BITRATE, current_frame)) != -1) {
      dyn_params->is_max_bit_rate_changed = true;
      dyn_params->max_bit_rate_kbps       = parsed_num;
    }
  } else if (!strcmp(key, DYNAMIC_PARAM_T_AQ_KEY)) {
    if ((ret = check_dyn_param_value(log, parsed_num, key, DYNAMIC_PARAM_MIN_TAQ, DYNAMIC_PARAM_MAX_TAQ, current_frame)) != -1) {
      dyn_params->v1.is_temporal_mode_changed = true;
      dyn_params->v1.temporal_aq_mode         = parsed_num;
      ret                                     = 1;
    }
  } else if (!strcmp(key, DYNAMIC_PARAM_T_AQ_GAIN_KEY)) {
    if ((ret = check_dyn_param_value(log, parsed_num, key, DYNAMIC_PARAM_MIN_TAQ_GAIN, DYNAMIC_PARAM_MAX_TAQ_GAIN, current_frame)) != -1) {
      dyn_params->v1.is_temporal_aq_gain_changed = true;
      dyn_params->v1.temporal_aq_gain            = parsed_num;
      ret                                        = 1;
    }
  } else if (!strcmp(key, DYNAMIC_PARAM_S_AQ_KEY)) {
    if ((ret = check_dyn_param_value(log, parsed_num, key, DYNAMIC_PARAM_MIN_SAQ, DYNAMIC_PARAM_MAX_SAQ, current_frame)) != -1) {
      dyn_params->v1.is_spatial_mode_changed = true;
      dyn_params->v1.spatial_aq_mode         = parsed_num;
      ret                                    = 1;
    }
  } else if (!strcmp(key, DYNAMIC_PARAM_S_AQ_GAIN_KEY)) {
    if ((ret = check_dyn_param_value(log, parsed_num, key, DYNAMIC_PARAM_MIN_SPAT_AQ_GAIN, DYNAMIC_PARAM_MAX_SPAT_AQ_GAIN, current_frame)) != -1) {
      dyn_params->v1.is_spatial_aq_gain_changed = true;
      dyn_params->v1.spatial_aq_gain            = parsed_num;
      ret                                       = 1;
    }
  } else if (!strcmp(key, DYNAMIC_PARAM_QP_KEY)) {
    if ((ret = check_dyn_param_value(log, parsed_num, key, DYNAMIC_PARAM_MIN_QP, DYNAMIC_PARAM_MAX_QP, current_frame)) != -1) {
      dyn_params->is_qp_changed = true;
      dyn_params->qp            = parsed_num;
      ret                       = 1;
    }
  } else if (!strcmp(key, DYNAMIC_PARAM_MIN_QP_KEY)) {
    if ((ret = check_dyn_param_value(log, parsed_num, key, DYNAMIC_PARAM_MIN_QP, DYNAMIC_PARAM_MAX_QP, current_frame)) != -1) {
      dyn_params->v1.is_min_qp_changed = true;
      dyn_params->v1.min_qp            = parsed_num;
      ret                              = 1;
    }
  } else if (!strcmp(key, DYNAMIC_PARAM_MAX_QP_KEY)) {
    if ((ret = check_dyn_param_value(log, parsed_num, key, DYNAMIC_PARAM_MIN_QP, DYNAMIC_PARAM_MAX_QP, current_frame)) != -1) {
      dyn_params->v1.is_max_qp_changed = true;
      dyn_params->v1.max_qp            = parsed_num;
      ret                              = 1;
    }
  } else if (!strcmp(key, DYNAMIC_PARAM_QP_I_OFFSET_KEY)) {
    if ((ret = check_dyn_param_value(log, parsed_num, key, (-1 * DYNAMIC_PARAM_MAX_QP), DYNAMIC_PARAM_MAX_QP, current_frame)) != -1) {
      dyn_params->is_qp_i_offset_changed = true;
      dyn_params->qp_i_offset            = parsed_num;
      ret                                = 1;
    }
  } else if (!strcmp(key, DYNAMIC_PARAM_QP_B_OFFSET_KEY)) {
    if ((ret = check_dyn_param_value(log, parsed_num, key, (-1 * DYNAMIC_PARAM_MAX_QP), DYNAMIC_PARAM_MAX_QP, current_frame)) != -1) {
      dyn_params->is_qp_b_offset_changed = true;
      dyn_params->qp_b_offset            = parsed_num;
      ret                                = 1;
    }
  } else {
    xma_logmsg(log, XMA_ERROR_LOG, AMA_DYNAMIC_PARAMS,
        "Invalid key \"%s\" for frame=%zu in dynamic params config file. Valid keys are:\n%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s and %s\n", key, current_frame,
        DYNAMIC_PARAM_B_FRAMES_KEY, DYNAMIC_PARAM_BITRATE_KEY, DYNAMIC_PARAM_T_AQ_KEY, DYNAMIC_PARAM_T_AQ_GAIN_KEY, DYNAMIC_PARAM_S_AQ_KEY, DYNAMIC_PARAM_S_AQ_GAIN_KEY,
        DYNAMIC_PARAM_MIN_QP_KEY, DYNAMIC_PARAM_MAX_QP_KEY, DYNAMIC_PARAM_QP_KEY, DYNAMIC_PARAM_MIN_QP_KEY, DYNAMIC_PARAM_MAX_QP_KEY, DYNAMIC_PARAM_QP_I_OFFSET_KEY,
        DYNAMIC_PARAM_QP_B_OFFSET_KEY);
    return -1;
  }
  key = strtok_r(NULL, delimiters, &save_ptr);
  return ret;
}

/**
 * ama_encoder_parse_dyn_params: Parses each line and stores dynamic encoder parameters
 *
 * @param log Handle to log
 * @param fp File pointer to Config file
 * @param current_frame The current frame to be encoded. Used to determine if
 * any parameter needs updating
 * @param dyn_params Pointer to VpiDynamicParams structure
 * @return 1 on success and paraams changed, 0 on success but no params changed, or -1 on error
 */
static int ama_encoder_parse_dyn_params(XmaLogHandle log, FILE* fp, size_t current_frame, XmaDynamicEncParams_v2* dyn_params) {
  char       line[DYNAMIC_PARAM_MAX_LENGTH] = "";
  char *     key_value_pairs, *frame_as_str, *pair, *save_ptr;
  size_t     parsed_frame   = 0;
  const char delimiters_1[] = ":";
  const char delimiters_2[] = ",";
  long int   file_position  = ftell(fp);
  int        ret            = 0;
  while (fgets(line, DYNAMIC_PARAM_MAX_LENGTH, fp) != NULL) {
    trim_space(&line[0]);
    if (line[0] == '\n' || line[0] == '#' || line[0] == '\r') {
      continue;
    }
    frame_as_str = strtok_r(line, delimiters_1, &save_ptr);
    if (frame_as_str == NULL || !isdigit(frame_as_str[0])) {
      continue;
    }
    parsed_frame = atoi(frame_as_str);
    if (parsed_frame < current_frame) {
      xma_logmsg(log, XMA_ERROR_LOG, AMA_DYNAMIC_PARAMS, "Invalid frame order! Current frame %zu, parsed frame %zu\n", current_frame, parsed_frame);
      return -1;
    } else if (parsed_frame > current_frame) {
      fseek(fp, file_position, SEEK_SET);
      break;
    }

    key_value_pairs = strtok_r(NULL, delimiters_1, &save_ptr);
    if (key_value_pairs == NULL) {
      xma_logmsg(log, XMA_ERROR_LOG, AMA_DYNAMIC_PARAMS, "Unable to parse dynamic params settings for frame %zu\n", current_frame);
      return -1;
    }
    pair = strtok_r(key_value_pairs, delimiters_2, &save_ptr);
    while (pair != NULL) {
      int tret;
      if ((tret = parse_dyn_param_value(log, pair, current_frame, dyn_params)) < 0) {
        return -1;
      }
      if (tret == 1) {
        ret = 1;
      }
      pair = strtok_r(NULL, delimiters_2, &save_ptr);
    }
    file_position = ftell(fp);
  }
  return ret;
}

/**
 * ama_encoder_get_dyn_params: Gets dynamic encoder parameters
 *
 * @param log Handle to log
 * @param dynamic_params_config_fp Pointer to input dynamic params config file
 * @param current_frame The current frame to be encoded. Used to determine if
 * any parameter needs updating
 * @param dynamic_params The parameters to be set
 * @return 1 on success and paraams changed, 0 on success but no params changed, or -1 on error
 */
int ama_encoder_get_dyn_params(XmaLogHandle log, FILE* dynamic_params_config_fp, size_t current_frame, XmaDynamicEncParams* dynamic_params) {
  int ret = -1;
  if (!dynamic_params_config_fp || !dynamic_params) {
    xma_logmsg(log, XMA_ERROR_LOG, AMA_DYNAMIC_PARAMS, "Failed to parse dynamic params! Invalid arguments!");
    return -1;
  }
  XmaDynamicEncParams_v2 dynamic_params_v2;
  memset(&dynamic_params_v2, 0, sizeof(XmaDynamicEncParams_v2));
  memcpy(&(dynamic_params_v2.v1), dynamic_params, sizeof(XmaDynamicEncParams));
  ret = ama_encoder_parse_dyn_params(log, dynamic_params_config_fp, current_frame, &dynamic_params_v2);
  if (ret != -1) {
    memcpy(dynamic_params, &(dynamic_params_v2.v1), sizeof(XmaDynamicEncParams));
  }
  return ret;
}

/**
 * ama_encoder_get_dyn_params_v2: Gets dynamic encoder parameters
 *
 * @param log Handle to log
 * @param dynamic_params_config_fp Pointer to input dynamic params config file
 * @param current_frame The current frame to be encoded. Used to determine if
 * any parameter needs updating
 * @param dynamic_params The parameters to be set
 * @return 1 on success and paraams changed, 0 on success but no params changed, or -1 on error
 */
int ama_encoder_get_dyn_params_v2(XmaLogHandle log, FILE* dynamic_params_config_fp, size_t current_frame, XmaDynamicEncParams_v2* dynamic_params) {
  if (!dynamic_params_config_fp || !dynamic_params) {
    xma_logmsg(log, XMA_ERROR_LOG, AMA_DYNAMIC_PARAMS, "Failed to parse dynamic params! Invalid arguments!");
    return -1;
  }
  memset(dynamic_params, 0, sizeof(XmaDynamicEncParams_v2));
  return ama_encoder_parse_dyn_params(log, dynamic_params_config_fp, current_frame, dynamic_params);
}
