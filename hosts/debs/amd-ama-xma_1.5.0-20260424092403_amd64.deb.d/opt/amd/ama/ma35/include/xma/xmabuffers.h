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

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "xmalimits.h"
#include "xmaparam.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * DOC:
 * Video buffer data structures needed for sharing and receiving data from
 * kernels. Library functions for allocating host buffers as well as buffer
 * data structures for sending/receiving data to/from video kernels.
*/

typedef void* XmaHandle;

/**
 * struct XmaFraction - Used for describing video frame rates
*/
typedef struct XmaFraction {
  int32_t numerator;   /**< numerator of fraction */
  int32_t denominator; /**< denominator of fraction */
} XmaFraction;

/**
 * enum XmaBufferType - Describes the location of a buffer.
*/
typedef enum XmaBufferType {
  XMA_HOST_BUFFER_TYPE = 1,    /**< 1 Has host allocated memory */
  XMA_DEVICE_BUFFER_TYPE,      /**< 2 Has both host and device allocated memory */
  XMA_DEVICE_ONLY_BUFFER_TYPE, /**< 3 Has device allocated memory */
  NO_BUFFER,                   /**< 4 Frame/Data is dummy without any buffer */
} XmaBufferType;

/**
 * struct XmaBufferRef - Reference counted buffer used in XmaFrame and
 * XmaDataBuffer
 *
*/
typedef struct XmaBufferRef {
  int32_t       refcount;    /**< references to buffer */
  XmaBufferType buffer_type; /**< location of buffer */
  void*         buffer;      /**< black box */
  uint8_t*      host;        /**< host copy of data */
  void*         device;      /**< device copy of data */
  bool          is_clone;    /**< buffer member allocated externally */
} XmaBufferRef;

/**
 * enum XmaFrameSideDataType - ID describing type of side data
*/
typedef enum XmaFrameSideDataType {
  XMA_FRAME_SIDE_DATA_INVALID        = -1,
  XMA_FRAME_SIDE_DATA_START          = 0,
  XMA_FRAME_SIDE_DATA_DYN_ENC_PARAMS = XMA_FRAME_SIDE_DATA_START,
  XMA_FRAME_SIDE_DATA_HDR10_PARAMS, // deprecated in v1.1
  XMA_FRAME_SIDE_DATA_VUI_PARAMS,
  XMA_FRAME_SIDE_DATA_ML_ROI,                             // added in v1.1
  XMA_FRAME_SIDE_DATA_ML_CC,                              // added in v1.1
  XMA_FRAME_SIDE_DATA_ML_CXC,                             // added in v1.1
  XMA_FRAME_SIDE_DATA_ML_LA_STAT,                         // added in v1.1
  XMA_FRAME_SIDE_DATA_ML_BOUNDING_BOX,                    // added in v1.1
  XMA_FRAME_SIDE_DATA_ML_CLASSIFICATION,                  // added in v1.1
  XMA_FRAME_SIDE_DATA_ML_YDATA,                           // added in v1.1
  XMA_FRAME_SIDE_DATA_ML_TENSOR,                          // added in v1.1
  XMA_FRAME_SIDE_DATA_SEI_MASTERING_DISPLAY_COLOR_VOLUME, // added in v1.1, replacing XMA_FRAME_SIDE_DATA_HDR10_PARAMS
  XMA_FRAME_SIDE_DATA_SEI_CONTENT_LIGHT_LEVEL,            // added in v1.1, replacing XMA_FRAME_SIDE_DATA_HDR10_PARAMS
  XMA_FRAME_SIDE_DATA_SEI_ATC,                            // added in v1.1, replacing XMA_FRAME_SIDE_DATA_HDR10_PARAMS
  XMA_FRAME_SIDE_DATA_SEI_HDR10_PLUS,                     // added in v1.1, replacing XMA_FRAME_SIDE_DATA_HDR10_PARAMS
  XMA_FRAME_SIDE_DATA_SEI_DOLBY_VISION,                   // added in v1.1, replacing XMA_FRAME_SIDE_DATA_HDR10_PARAMS
  XMA_FRAME_SIDE_DATA_SEI_RAW_T35_DATA,                   // added in v1.1, replacing XMA_FRAME_SIDE_DATA_HDR10_PARAMS
  XMA_FRAME_SIDE_DATA_SEI_USER_UNREGISTERED,              // added in v1.1
  XMA_FRAME_SIDE_DATA_DELTA_QP_MAP_H26X,                  // added in v1.1
  XMA_FRAME_SIDE_DATA_DYN_COMPOSITOR_PARAMS,              // added in v1.2
  XMA_FRAME_SIDE_DATA_SEI_CLOSED_CAPTION,                 // added in v1.2
  XMA_FRAME_SIDE_DATA_RPU_DOLBY_VISION,                   // added in v1.2
  XMA_FRAME_SIDE_DATA_RPU_RAW_DATA,                       // added in v1.2
  XMA_FRAME_SIDE_DATA_USER_DEFINED_START = 0x1000         // added in v1.1
} XmaFrameSideDataType;

/**
 * struct XmaFrameSideData - Reference counted buffer used for frame side data
 *
*/
typedef struct XmaFrameSideData {
  int32_t              refcount;             /**< references to buffer */
  XmaFrameSideDataType type;                 /**< type of side data */
  XmaBufferType        buffer_type;          /**< buffer type */
  bool                 on_host;              /**< deprecated in v1.1 */
  uint8_t*             host;                 /**< host copy of data */
  uint64_t             device;               /**< device copy of data */
  uint64_t             size;                 /**< size of data */
  void*                private_do_not_touch; /**< internal, do not touch */
  XmaParameter*        metadata;             /**< extra info about this side
                                                  data */
  uint32_t             reserved[8];          /**< reserved for future
                                                  expansion */
} XmaFrameSideData;

/**
 * XmaFormatType - XMA pixel formats
*/
typedef enum XmaFormatType {
  XMA_NONE_FMT_TYPE = 0,       /**< 0 */
  XMA_NV12_FMT_TYPE,           /**< 1 */
  XMA_NV21_FMT_TYPE,           /**< 2 */
  XMA_P010LE_FMT_TYPE,         /**< 3 */
  XMA_P010BE_FMT_TYPE,         /**< 4 */
  XMA_YUV420P_FMT_TYPE,        /**< 5 */
  XMA_YUV420P10LE_FMT_TYPE,    /**< 6 */
  XMA_YUV420P10BE_FMT_TYPE,    /**< 7 */
  XMA_YUV422P_FMT_TYPE,        /**< 8 */
  XMA_YUV422P10LE_FMT_TYPE,    /**< 9 */
  XMA_YUV422P10BE_FMT_TYPE,    /**< 10 */
  XMA_YUV444P_FMT_TYPE,        /**< 11 */
  XMA_ARGB_FMT_TYPE,           /**< 12 */
  XMA_ABGR_FMT_TYPE,           /**< 13 */
  XMA_RGBA_FMT_TYPE,           /**< 14 */
  XMA_BGRA_FMT_TYPE,           /**< 15 */
  XMA_RGB24_FMT_TYPE,          /**< 16 */
  XMA_BGR24_FMT_TYPE,          /**< 17 */
  XMA_VPE_FMT_TYPE,            /**< 18 */
  XMA_PACKED10_FMT_TYPE,       /**< 19 */
  XMA_BGR0_FMT_TYPE,           /**< 20 */
  XMA_RGB16_FMT_TYPE,          /**< 21 */
  XMA_RGB24_P_FMT_TYPE,        /**< 22 */
  XMA_BGR24_P_FMT_TYPE,        /**< 23 */
  XMA_RGB48_FMT_TYPE,          /**< 24 */
  XMA_BGR48_FMT_TYPE,          /**< 25 */
  XMA_RGB48_P_FMT_TYPE,        /**< 26 */
  XMA_BGR48_P_FMT_TYPE,        /**< 27 */
  XMA_YUV422SP_FMT_TYPE,       /**< 28 */
  XMA_YUV422SP_10BIT_FMT_TYPE, /**< 29 */
  XMA_UYVY422_FMT_TYPE,        /**< 30 */
  XMA_YUY2422_FMT_TYPE,        /**< 31 */
  XMA_Y_ONLY_FMT_TYPE = 100,   /**< 100 */
  XMA_SB,                      /**< 101, added in v1.1 */
} XmaFormatType;

#define XMA_FRAME_PROPERTY_FLAG_TILE_4x4 (0x1 << 4)
#define XMA_FRAME_PROPERTY_FLAG_COMPRESS (0x1 << 5)

/**
 * struct XmaFrameProperties - Description of frame dimensions for XmaFrame
*/
typedef struct XmaFrameProperties {
  enum XmaFormatType format;                   /**< host pixel format,
                                                    XMA_VPE_FMT_TYPE if data is
                                                    on device */
  enum XmaFormatType sw_format;                /**< underlying pixel format (if
                                                    format is not
                                                    XMA_VPE_FMT_TYPE, value
                                                    will be same as format) */
  int32_t            width;                    /**< width of primary plane */
  int32_t            height;                   /**< height of primary plane */
  int32_t            linesize[XMA_MAX_PLANES]; /**< linesize per component */
  int32_t            bits_per_pixel;           /**< not used, use format and
  -                                                    sw_format */
  uint32_t           flags;                    /**< combination of
                                                    XMA_FRAME_PROPERTY_FLAG_xxx
                                                    flags */
  uint32_t           reserved[8];              /**< reserved for future
                                                    expansion */
} XmaFrameProperties;

/**
 * struct XmaFrame - Data structure describing a raw video frame and its
 * buffers
*/
typedef struct XmaFrame {
  XmaBufferRef       data[XMA_MAX_PLANES]; /**< frame data */
  XmaFrameSideData** side_data;            /**< side data */
  XmaFrameProperties frame_props;          /**< frame properties */
  XmaFraction        time_base;            /**< time base as a fraction */
  XmaFraction        frame_rate;           /**< frames per second as a
                                                fraction */
  int64_t            pts;                  /**< presentation time stamp */
  int                is_idr;               /**< is the frame an IDR frame */
  int32_t            do_not_encode;        /**< not used */
  int32_t            is_last_frame;        /**< not used */
  void*              private_do_not_touch; /**< internal, do not touch */
  void*              user_data;            /**< user data pointer */
  uint32_t           reserved[6];          /**< reserved for future expansion */
} XmaFrame;

/**
 * struct XmaDataBuffer - A structure describing a raw data buffer
*/
typedef struct XmaDataBuffer {
  XmaBufferRef data;                 /**< data buffer */
  int32_t      alloc_size;           /**< allocated size of data buffer */
  int32_t      is_eof;               /**< flag to indicate that this buffer is
                                          EOF */
  int64_t      pts;                  /**< presentation time stamp looping back
                                          to application */
  int32_t      poc;                  /**< Picture order count for current
                                          output frame */
  void*        private_do_not_touch; /**< internal, do not touch */
  int64_t      dts;                  /**< decode time stamp looping back to
                                          application. Added in v1.2 */
  void*        user_data;
  int32_t      key_frame;   /**< identify key frames for muxers */
  uint32_t     reserved[4]; /**< reserved for future expansion */
} XmaDataBuffer;

/**
 * struct XmaFrameData - Member structure with array of raw data pointers for
 * multiplane buffer
*/
typedef struct XmaFrameData {
  uint8_t* data[XMA_MAX_PLANES]; /**< buffer pointers */
} XmaFrameData;

/**
 * Side data metadata used by ML, added in v1.1
*/
#define XMA_SIDE_DATA_METADATA_ML_LAYOUT "layout"           /**< layout of width, height, batch size, and layout ch */
#define XMA_SIDE_DATA_METADATA_ML_WIDTH "width"             /**< width of ml image */
#define XMA_SIDE_DATA_METADATA_ML_HEIGHT "height"           /**< height of ml image */
#define XMA_SIDE_DATA_METADATA_ML_BATCH_SIZE "batch_size"   /**< batch size of ml image */
#define XMA_SIDE_DATA_METADATA_ML_LAYOUT_CH "layout_ch"     /**< number of pixel components in ml image */
#define XMA_SIDE_DATA_METADATA_ML_DATA_FORMAT "data_format" /**< format ML data is in */

typedef enum XmaMLLayoutType { XMA_ML_LAYOUT_NCHW = 0, XMA_ML_LAYOUT_NHWC, XMA_ML_LAYOUT_CHWN, XMA_ML_LAYOUT_UNSUPPORTED } XmaMLLayoutType;

typedef enum XmaMLDataFormat {
  XMA_ML_DATA_FORMAT_IMG     = 0,
  XMA_ML_DATA_FORMAT_INT8    = 2,
  XMA_ML_DATA_FORMAT_UINT8   = 3,
  XMA_ML_DATA_FORMAT_INT16   = 4,
  XMA_ML_DATA_FORMAT_UINT16  = 5,
  XMA_ML_DATA_FORMAT_FLOAT32 = 10,
  XMA_ML_DATA_FORMAT_FLOAT64 = 11,
  XMA_ML_DATA_FORMAT_FLOAT16 = 16
} XmaMLDataFormat;

/**
 * xma_frame_clone_free_callback_function() - Callback function called when
 * frame cloned from XmaFrameData is no longer needed and can be freed
 *
 * @opaque:     user defined parameter
 * @frame_data: frame data
*/
typedef void (*xma_frame_clone_free_callback_function)(void* opaque, XmaFrameData* frame_data);

/**
 * xma_frame_alloc() - Allocate a new frame buffer
 *
 * @handle:      handle to XMA device
 * @frame_props: frame properties
 * @dummy:       allocate dummy frame without any buffer
 *
 * RETURN: XmaFrame pointer on success
 *         NULL on failure
*/
XmaFrame* xma_frame_alloc(XmaHandle handle, XmaFrameProperties* frame_props, bool dummy);

/**
 * xma_frame_planes_get() - Return the number of planes in the frame properties
 * specified
 *
 * @handle:      handle to XMA device
 * @frame_props: properties of frame being queried
 *
 * RETURN: number of planes in format specified by frame_props (0-3)
*/
int32_t xma_frame_planes_get(XmaHandle handle, XmaFrameProperties* frame_props);

/**
 * xma_frame_get_plane_height() - Return the height of the plane
 *
 * @handle:      handle to XMA device
 * @frame_props: properties of frame being queried
 * @plane:       which plane to query
 *
 * RETURN: height of plane
*/
int32_t xma_frame_get_plane_height(XmaHandle handle, XmaFrameProperties* frame_props, size_t plane);

/**
 * xma_frame_get_plane_stride() - Return the stride of the plane (distance in
 * bytes from one line to the next)
 *
 * @handle:      handle to XMA device
 * @frame_props: properties of frame being queried
 * @plane:       which plane to query
 *
 * RETURN: stride of plane
*/
int32_t xma_frame_get_plane_stride(XmaHandle handle, XmaFrameProperties* frame_props, size_t plane);

/**
 * xma_frame_get_plane_size() - Return the size of the plane in bytes
 *
 * @handle:      handle to XMA device
 * @frame_props: properties of frame being queried
 * @plane:       which plane to query
 *
 * RETURN: size (in bytes) of plane
*/
int32_t xma_frame_get_plane_size(XmaHandle handle, XmaFrameProperties* frame_props, size_t plane);

/**
 * xma_frame_inc_ref() - Increments reference count of frame data structure.
 *
 * @frame: frame instance to increment reference count of
 *
 * RETURN: new reference count
*/
int32_t xma_frame_inc_ref(XmaFrame* frame);

/**
 * xma_frame_dec_ref() - Decrements reference count of frame data structure.
 *
 * @frame: frame instance to decrement reference count of
 *
 * RETURN: new reference count
*/
int32_t xma_frame_dec_ref(XmaFrame* frame);

/**
 * xma_frame_free() - Free frame data structure. The associated side data
 * handles, if any, are also cleared.
 *
 * @frame: frame instance to free
*/
void xma_frame_free(XmaFrame* frame);

/**
 * xma_frame_from_buffers_clone() - Wraps buffers described in XmaFrameData
 * into XmaFrame container. Buffers will not be freed when XmaFrame is freed,
 * but it is the applications responsibility to guarantee the buffers are not
 * freed or overwritten before the XmaFrame is freed.
 *
 * @handle:        handle to XMA device
 * @frame_props:   properties of XmaFrame to create
 * @frame_data:    container of previously allocated frame buffers
 * @free_callback: callback function called when frame data is no longer needed
 *                 and can be freed
 * @opaque:        pointer to parameter passed to callback function
 *
 * RETURN: XmaFrame pointer populated with frame properties and pointers
 *                  to frame data specified by parameters on success
 *         NULL on failure
*/
XmaFrame* xma_frame_from_buffers_clone(
    XmaHandle handle, XmaFrameProperties* frame_props, XmaFrameData* frame_data, xma_frame_clone_free_callback_function free_callback, void* opaque);

/**
 * xma_frame_clone() - Creates a new XmaFrame that points to the same video
 * buffers and side data as the source frame frame. Changes to the content of
 * the new XmaFrame video buffer will effect the old XmaFrame's video buffer.
 * Modifying the content of the new XmaFrame's side data will effect the old
 * XmaFrame's side data, but adding or removing side data to the new XmaFrame
 * will not effect the old XmaFrame's side data. (Both the new XmaFrame and the
 * old XmaFrame will initially point to the same side data, but the side data
 * can be changed later)
 *
 * @handle: handle to XMA device
 * @frame:  frame to clone
 *
 * RETURN: XmaFrame pointer that is a clone of the source XmaFrame on success
 *         NULL on failure
*/
XmaFrame* xma_frame_clone(XmaHandle handle, XmaFrame* xma_frame);

int32_t xma_frame_get_plane_fd(XmaFrame* frame, int32_t plane);

int32_t xma_frame_get_buffer_fd(XmaFrame* frame);

/**
 * xma_side_data_alloc() - Allocates side data handle, with reference count
 * equal to 1.
 *
 * @handle:      handle to XMA device
 * @type:        type of side data
 * @buffer_type: type of buffer to allocate
 * @size:        size of buffer to allocate
 *
 * RETURN: XmaFrameSideData pointer on success.
 *         NULL on failure
*/
XmaFrameSideData* xma_side_data_alloc(XmaHandle handle, enum XmaFrameSideDataType type, XmaBufferType buffer_type, uint64_t size);

/**
 * xma_side_data_free() - Decrements the reference count of the side_data by 1.
 * If the refrence count reaches 0, then the buffer is deallocated.
 *
 * @side_data: The side data which needs to be freed
 *
*/
void xma_side_data_free(XmaFrameSideData* side_data);

/**
 * xma_side_data_inc_ref() - The reference counter for the side data is
 * incremented.
 *
 * @side_data: The side data whose refcount needs to be incremented.
 *
 * RETURN: if > 0, the reference count of the side data buffer after
 *           incrementing it by 1.
 *         XMA_ERROR_INVALID if side_data is NULL
*/
int32_t xma_side_data_inc_ref(XmaFrameSideData* side_data);

/**
 * xma_side_data_dec_ref() - Decrements the reference count of the side_data by
 * 1. If the refrence count reaches 0, then the buffer handle is deallocated.
 *
 * @side_data: The side data whose refcount needs to be decremented.
 *
 * RETURN: if >= 0, the reference count of the side data buffer after
 *           decrementing it by 1.
 *         XMA_ERROR_INVALID if side_data is NULL
*/
int32_t xma_side_data_dec_ref(XmaFrameSideData* side_data);

/**
 * xma_side_data_get_refcount() - returns the reference count of the side_data
 *
 * @side_data: The side data whose refcount is needed.
 *
 * RETURN: if > 0, the reference count of the side data buffer
 *         XMA_ERROR_INVALID if side_data is NULL
*/
int32_t xma_side_data_get_refcount(XmaFrameSideData* side_data);

/**
 * xma_side_data_get_metadata() - Gets the value of a specified metadata. Added
 * in v1.1
 *
 * @side_data: side data to get metadata from
 * @metadata:  pointer to a valid XmaParameter with the name of the metadata to
 *             get. The type must be correct, the length must be greater than
 *             or equal to the size of the saved metadata value, and the value
 *             must point to a buffer of size length to copy the value to.
 *
 * RETURN: XMA_SUCCESS on success
 *         XMA_ERROR_INVALID on invalid input
 *         XMA_ERROR on error
 *
*/
int32_t xma_side_data_get_metadata(XmaFrameSideData* side_data, XmaParameter* metadata);

/**
 * xma_side_data_set_metadata() - If the side data has metadata of the
 * specified name, the metadata is replaced, otherwise new metadata is added.
 * Added in v1.1
 *
 * @side_data: side data add metadata to
 * @metadata:  pointer to a valid XmaParameter with the name of the metadata to
 *             set. The type, length, and value must be specified.
 *
 * RETURN: XMA_SUCCESS on success
 *         XMA_ERROR_INVALID on invalid input
 *         XMA_ERROR on error
 *
*/
int32_t xma_side_data_set_metadata(XmaFrameSideData* side_data, XmaParameter* metadata);

/**
 * xma_side_data_read() - Copy data from device to host. Deprecated in v1.1
 *
 * @side_data: side data to copy data from
 *
 * RETURN: XMA_SUCCESS on success
 *         XMA_ERROR_INVALID on invalid input
 *         XMA_ERROR on error
 *
*/
int32_t xma_side_data_read(XmaFrameSideData* side_data);

/**
 * xma_side_data_write() - Copy data from host to device. Only use this if you
 * created the side data and it has not yet been sent to any sessions,
 * otherwise risk of a race condition can occur.
 *
 * @side_data: side data to copy data from
 *
 * RETURN: XMA_SUCCESS on success
 *         XMA_ERROR_INVALID on invalid input
 *         XMA_ERROR on error
 *
*/
int32_t xma_side_data_write(XmaFrameSideData* side_data);

/**
 * xma_frame_add_side_data() - Adds the side data to the frame. If there is
 * already side data of the same type associated with the frame, it is removed
 * and the new side data is set. The reference count of the side_data buffer is
 * incremented by 1 on successful execution.
 *
 * @frame:     XmaFrame the side data will be associated.
 * @side_data: the side data to be added to the XmaFrame.
 *
 * RETURN: XMA_SUCCESS on success
 *         XMA_ERROR_INVALID if frame or side_data is NULL
 *         XMA_BAD_ALLOC on out of memory
 *         XMA_ERROR on error
 * XMA_SUCCESS, on successful execution.
*/
int32_t xma_frame_add_side_data(XmaFrame* frame, XmaFrameSideData* side_data);

/**
 * xma_frame_get_first_side_data() - Return the handle to the first side data of any
 * type. Added in v1.1
 *
 * @frame:   XmaFrame to get the side data from
 *
 * RETURN: XmaSideDataHandle on success
 *         NULL on failure
*/
XmaFrameSideData* xma_frame_get_first_side_data(XmaFrame* frame);

/**
 * xma_frame_get_side_data() - Return the handle to the first side data of the
 * given type
 *
 * @frame:   XmaFrame to get the side data from
 * @sd_type: type of side data buffer to get
 *
 * RETURN: XmaSideDataHandle on success
 *         NULL on failure
*/
XmaFrameSideData* xma_frame_get_side_data(XmaFrame* frame, enum XmaFrameSideDataType type);

/**
 * xma_frame_get_next_side_data() - Return the handle to the next side data of
 * any type. Added in v1.1
 *
 * @frame:     XmaFrame to get the side data from
 * @side_data: side data buffer
 *
 * RETURN: XmaSideDataHandle on success
 *         NULL on failure
*/
XmaFrameSideData* xma_frame_get_next_side_data(XmaFrame* frame, XmaFrameSideData* side_data);

/**
 * xma_frame_get_next_side_data_of_type() - Return the handle to the next side data of
 * the given type. Added in v1.1
 *
 * @frame:     XmaFrame to get the side data from
 * @side_data: side data buffer
 *
 * RETURN: XmaSideDataHandle on success
 *         NULL on failure
*/
XmaFrameSideData* xma_frame_get_next_side_data_of_type(XmaFrame* frame, XmaFrameSideData* side_data);

/**
 * xma_frame_remove_side_data() - Removes the side data from the frame. The
 * side data buffer refrence count is decremented by 1. If it results in a
 * reference count of zero, then the side data is freed.
 *
 * @frame:     XmaFrame to remove the side data from
 * @side_data: The side data to be removed
 *
 * RETURN: XMA_SUCCESS on success
 *         XMA_ERROR_INVALID on invalid input
 *         XMA_ERROR on error
*/
int32_t xma_frame_remove_side_data(XmaFrame* frame, XmaFrameSideData* side_data);

/**
 * xma_frame_remove_side_data_type() - Removes all side data of the given type.
 * Any side data buffer that is removed has it's refrence count decremented by
 * 1. If it results in a reference count of zero, then the side data buffer is
 * freed.
 *
 * @frame:     XmaFrame to remove the side data from
 * @side_data: The type of side data to be removed.
 *
 * RETURN: XMA_SUCCESS on success
 *         XMA_ERROR_INVALID on invalid input
 *         XMA_ERROR on error
*/
int32_t xma_frame_remove_side_data_type(XmaFrame* frame, enum XmaFrameSideDataType type);

/**
 * xma_frame_clear_all_side_data() - Removes all side data from the frame. The
 * refrence count of each side data buffer associated with the frame is
 * decremented by 1. If, it results in a reference count of zero, then
 * the side data is freed.
 *
 * @frame: XmaFrame to remove all side data from
 *
*/
void xma_frame_clear_all_side_data(XmaFrame* frame);

/**
 * xma_data_buffer_clone_free_callback_function() - Callback function called
 * when data buffer cloned from data is no longer needed and can be freed
 *
 * @opaque: user defined parameter
 * @data:   data
*/
typedef void (*xma_data_buffer_clone_free_callback_function)(void* opaque, uint8_t* data);

/**
 * xma_data_buffer_alloc() - Allocate a single buffer and return as
 * XmaDataBuffer pointer
 *
 * @handle: handle to XMA device
 * @size:   size of buffer to allocate from heap
 * @dummy:  allocate dummy XmaDataBuffer without any memory
 *
 * RETURN: pointer to XmaDataBuffer on success
 *         NULL on failure
*/
XmaDataBuffer* xma_data_buffer_alloc(XmaHandle handle, size_t size, bool dummy);

/**
 * xma_data_from_buffer_clone() - Create an XmaDataBuffer object from data and
 * size. It is the responsibility of the application to make sure the provided
 * data is not freed or overwritten while this XmaDataBuffer exists.
 *
 * @handle:        handle to XMA device
 * @data:          pointer to raw data previously allocated
 * @size:          size of data
 * @free_callback: callback function called when data is no longer needed and
 *                 can be freed
 * @opaque:        pointer to parameter passed to callback function
 *
 * RETURN: pointer to XmaDataBuffer on success
 *         NULL on failure
*/
XmaDataBuffer* xma_data_from_buffer_clone(XmaHandle handle, uint8_t* data, size_t size, xma_data_buffer_clone_free_callback_function free_callback, void* opaque);

/**
 * xma_data_buffer_free() - Free XmaDataBuffer container structure
 *
 * @data: structure to be freed
 *
 * Note: if the data buffer was created by xma_data_buffer_alloc(), the
 * associated raw buffer will be freed. If the data buffer was created by
 * xma_data_from_buffer_clone(), the raw buffer will not be freed.
*/
void xma_data_buffer_free(XmaDataBuffer* data);

#ifdef __cplusplus
}
#endif
