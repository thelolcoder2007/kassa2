/*
 * Copyright (c) 2018 Mohammad Izadi <moh.izadi at gmail.com>
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

#ifndef AVUTIL_CC_METADATA_H
#define AVUTIL_CC_METADATA_H

#include "frame.h"
#define MAX_CC_COUNT 32

/**
 * This struct represents closed caption metadata for ATSC 
 * A53 Part 4 Closed Captions standard.
 *
 * To be used as payload of a AVFrameSideData or AVPacketSideData with the
 * appropriate type.
 *
 * @note The struct should be allocated with
 * av_closed_caption_alloc() and its size is not a part of
 * the public ABI.
 */
typedef struct AVClosedCaption {
    /**
     * Country code by Rec. ITU-T T.35 Annex A. The value shall be 0xB5.
     */
    uint8_t itu_t_t35_country_code;
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
} AVClosedCaption;

/**
 * Allocate an AVClosedCaption structure and set its fields to
 * default values. The resulting struct can be freed using av_freep().
 *
 * @return An AVClosedCaption filled with default values or NULL
 *         on failure.
 */
AVClosedCaption *av_closed_caption_alloc(size_t *size);

/**
 * Allocate a complete AVClosedCaption and add it to the frame.
 * @param frame The frame which side data is added to.
 *
 * @return The AVClosedCaption structure to be filled by caller or NULL
 *         on failure.
 */
AVClosedCaption *av_closed_caption_create_side_data(AVFrame *frame);

#endif /* AVUTIL_CC_METADATA_H */
