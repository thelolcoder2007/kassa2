/* SPDX-License-Identifier: LGPL-3.0-or-later OR Apache-2.0 */

/*
 * (c) Copyright 2024 VeriSilicon Holdings Co., Ltd. All rights reserved.
 *
 * This file is dual-licensed; you may select either the GNU
 * Lesser General Public License version 3 or
 * Apache License, Version 2.0.
 *
 */

#ifndef __VPI_API_H__
#define __VPI_API_H__

/*
 * provides application programming interface for the application layer.
 */
#if defined(__cplusplus)
extern "C" {
#endif

#include "vpi_types.h"

//System
VpiHandle vpi_create(const char *app_name);
void vpi_destroy(VpiHandle);

//Device
VpiDevHandle vpi_device_create(VpiHandle handle, const char *device);
void vpi_device_destroy(VpiDevHandle handle);
VpiRet vpi_device_set_param(VpiDevHandle dev_ctx, char * key, char * value);
VpiRet vpi_device_open(VpiDevHandle dev_ctx);
int vpi_device_get_fd(VpiDevHandle dev_ctx);
void vpi_device_close(VpiDevHandle dev_ctx);
VpiHwCfg* vpi_device_set_hw_config(VpiDevHandle dev_ctx, VpiHwId hw_id);
VpiHwCfg* vpi_device_get_hw_config(VpiDevHandle dev_ctx, VpiHwId hw_id);
void vpi_device_free_hw_config(VpiHwCfg *hw_cfg);
VpiRet vpi_device_get_device_index(VpiDevHandle handle, int* device_index);
void vpi_device_set_ctx(void *params, VpiDevHandle ctx, VpiHwId type);

//Memory
VpiMemHandle vpi_memory_init(VpiHandle handle, VpiDevHandle dev_ctx);
VpiRet vpi_memory_get_info(VpiMemHandle handle, VpiMemInfo *info);
VpiRet vpi_memory_set_map_info(VpiMemHandle handle, int pp_index, int out_index);
VpiRet vpi_memory_get_map_info(VpiMemHandle handle, int out_index, int *p_pp_index);
VpiRet vpi_memory_set_channel_info(VpiMemHandle handle, VpiChannelInfo *ch_info, VpiHwId hw_id);
VpiRet vpi_memory_set_single_channel_info(VpiMemHandle handle, int idx, VpiChannelInfo *ch_info, VpiHwId hw_id);
VpiRet vpi_memory_get_channel_info(VpiMemHandle handle, VpiChannelInfo *ch_info);
int vpi_info_get_struct_size(VpiStructType struct_type);
void *vpi_memory_get_struct(VpiStructType struct_type);
void vpi_memory_free_struct(void *p_struct);
void vpi_memory_release(VpiMemHandle handle);

//Picture/Frame
VpiRet vpi_pic_set_buffer(VpiFrm *in_pic, int ch, int plane, void *buffer, unsigned int linesize);
VpiRet vpi_frame_alloc(VpiMemHandle mem_ctx, VpiFrm **frame);
void vpi_frame_free(VpiMemHandle mem_ctx, VpiFrm *pic);

// Increments the ref of all attached sidedata as well
void vpi_frame_ref(VpiFrm *frame);

// Decrements the ref of all attached sidedata as well
// NULL set is not done on sidedata pointers
void vpi_frame_unref(VpiFrm *frame);

// Below two apis are expert level interface
// It should not be needed in most cases
// It operates only on vpi_picinfo_buf and not sidedata
void vpi_frame_imagedata_ref(VpiFrm *frame);
void vpi_frame_imagedata_unref(VpiFrm *frame);

void vpi_frame_release(VpiFrm *frame);
void vpi_frame_remove_ref(VpiFrm *frame);

int vpi_frame_get_refcount(VpiFrm *frame);

//Decoder
VpiDecHandle vpi_decoder_init(VpiDecInitParams *params);
VpiRet vpi_decoder_set_param(VpiDecHandle handle, const char * key, char * value);
VpiRet vpi_decoder_open(VpiDecHandle handle);
void vpi_decoder_close(VpiDecHandle handle);
int vpi_decoder_put_packet(VpiDecHandle handle, VpiPkt *pkt);
int vpi_decoder_get_frame(VpiDecHandle handle);
VpiRet vpi_decoder_get_output_frame(VpiDecHandle handle, VpiFrm *pic);
void *vpi_decoder_release_pkt(VpiDecHandle handle);
void *vpi_decoder_flush_pkt(void *pool);

//Encoder
VpiEncHandle vpi_encoder_init(VpiEncInitParams *param);
VpiRet vpi_encoder_set_param(VpiEncHandle handle, const char *key, char *value);
VpiRet vpi_encoder_open(VpiEncHandle handle);
VpiRet vpi_encoder_put_frame(VpiEncHandle handle, VpiFrm *pic);
VpiRet vpi_encoder_get_packet_size(VpiEncHandle handle, int *size);
VpiRet vpi_encoder_get_packet(VpiEncHandle handle, VpiPkt *pkt);
VpiRet vpi_encoder_get_reorder_delay(VpiEncHandle handle, unsigned int *reorder_delay);
void vpi_encoder_close(VpiEncHandle handle);
void *vpi_encoder_get_extradata(VpiEncHandle handle, int *extradata_size);
void *vpi_encoder_release_frame(VpiEncHandle handle);
void *vpi_encoder_flush_frame(void *pool);

//Processor
VpiRet vpi_processor_init(VpiProcInitParams *param, VpiProcHandle *handle);
VpiRet vpi_processor_prepare_reinit(VpiProcHandle handle, VpiProcInitParams *param);
VpiRet vpi_processor_commit_reinit(VpiProcHandle handle, VpiProcInitParams *param);
VpiRet vpi_processor_set_param(VpiProcHandle handle, const char *key, char *value);
VpiRet vpi_processor_open(VpiProcHandle handle);
VpiRet vpi_processor_get_info(VpiProcHandle handle, VpiProcInfo *info);
VpiRet vpi_processor_process_frame(VpiProcHandle handle, VpiFrm * in, VpiFrm * out);
VpiRet vpi_processor_process_frame_list(VpiProcHandle handle, VpiFrm * in, VpiFrm ** out);
void vpi_processor_close(VpiProcHandle handle);

// Common
char *vpi_error_str(int vpi_error);
void vpi_insert_mem_node(void *ptr, unsigned int size, unsigned int mem_type);
void vpi_remove_mem_node(void *ptr, unsigned int size);
VpiRet vpi_pool_create(VpiMemHandle handle, unsigned int num, void **pool);
void vpi_pool_unref(void *pool);
VpiRet vpi_frm_pool_extend(VpiFrm *frm, unsigned int extend_num);

// OSAL
VpiOsalHwCtx vpi_osal_create_hwcontext(const char *dev);
int vpi_osal_destroy_hwcontext(VpiOsalHwCtx hwctx);
int vpi_osal_set_param(VpiOsalHwCtx hwctx, char *key, char *value);
VpiHwCfg* vpi_osal_get_hw_config(VpiOsalHwCtx hwctx, VpiHwId hw_id);
VpiMemHandle vpi_osal_get_memctx(VpiOsalHwCtx hwctx);
typedef enum { VPI_CLIENT_FFMPEG } VpiClientHint;
void vpi_set_client(VpiClientHint hint);

//sidedata basic functions
VpiRet vpi_sidedata_pool_manager_init(VpiMemHandle mem_handle, VpiSidedataPoolManagerHandle *pool_mgr);
VpiRet vpi_sidedata_pool_manager_uninit(VpiSidedataPoolManagerHandle sd_pool_mgr);

VpiRet vpi_sidedata_alloc(VpiSidedataPoolManagerHandle sd_mgr, int type, size_t size, int flag, VpiSidedataHandle *sb_handle);
VpiRet vpi_sidedata_sync(VpiSidedataHandle sb_handle, int dir);
VpiRet vpi_sidedata_ref(VpiSidedataHandle sb_handle);
VpiRet vpi_sidedata_unref(VpiSidedataHandle sb_handle);
int vpi_get_sidedata_type(VpiSidedataHandle sb_handle);
int vpi_get_sidedata_size(VpiSidedataHandle sb_handle);
int vpi_get_sidedata_layout(VpiSidedataHandle sb_handle);
int vpi_get_sidedata_height(VpiSidedataHandle sb_handle);
int vpi_get_sidedata_width(VpiSidedataHandle sb_handle);
int vpi_get_sidedata_layoutch(VpiSidedataHandle sb_handle);
int vpi_get_sidedata_batchsize(VpiSidedataHandle sb_handle);
int vpi_get_sidedata_dataformat(VpiSidedataHandle sb_handle);
//int vpi_get_sidedata_typesize(VpiSidedataHandle sb_handle);
// ffmpeg transfer avframe's sidedata to vpifrm's
void *vpi_get_sidedata_vaddr(VpiSidedataHandle sb_handle);
void *vpi_get_sidedata_paddr(VpiSidedataHandle sb_handle);
VpiRet vpi_sidedata_get_refcount(VpiSidedataHandle sb_handle);

VpiRet vpifrm_attach_sidedata(VpiFrm *frame, VpiSidedataHandle sb_handle);
VpiRet vpifrm_remove_sidedata(VpiFrm *frame, VpiSidedataHandle sb_handle);
VpiRet vpifrm_remove_sidedata_by_type(VpiFrm *frame, int type);
VpiRet vpifrm_remove_all_sidedata(VpiFrm *frame);
VpiRet vpifrm_get_first_sidedata_by_type(VpiFrm *frame, int type, VpiSidedataHandle *sb_handle);
VpiRet vpifrm_sidedata_copy_reference(VpiFrm *in, VpiFrm *out);
VpiSidedataPoolManagerHandle vpi_get_sidedata_pool_mgr(VpiProcHandle proc_handle);
VpiSidedataHandle vpifrm_find_next_sidedata(VpiFrm *frame, VpiSidedataHandle sb_handle);

//sidedata helper functions
VpiRet vpi_update_side_data_attribute(VpiSidedataHandle *sidedata, int type, int *nchw, int layout, int data_format);

int vpi_frame_extend_pool_depth(VpiFrm *frame, int extend_by);

/**
 * Check if the side data is closed caption data i.e VpiRawT35Data with the correct header
 * returns 1 is data is of the required type, 0 otherwise.
 */
int vpi_sidedata_is_cc_raw(VpiSidedataHandle sb_handle);
/**
*   Removes the CC data from the frame, should be called after vpi_cc_enqueue_data
*   returns VPI_SUCCESS on success, a negative error code otherwise.
*/
VpiRet vpifrm_remove_cc_data(VpiFrm *frame);

/**--------------------------------------------------------------------------------------
*    Functions to handle CEA-608 and CEA-708 Closed caption data with FPS changes
*    These functions handle both VPI_SB_SEI_CLOSED_CAPTION and VPI_SB_SEI_RAW_T35_DATA
* --------------------------------------------------------------------------------------*/

/**
*   Initialize the CC handling context
*   returns null on error.
*/
VpiCCContext *vpi_cc_init(int32_t fps_num, int32_t fps_den);

/*
*   Deinitialize the CC handling context
*/
void vpi_cc_deinit(VpiCCContext *ctx);

/**
*   Enqueue the CC data to the context, a deep copy of the data is made.
*   returns VPI_SUCCESS on success, a negative error code otherwise.
*   Does not remove the side data from frame (call vpifrm_remove_cc_data for that).
*/
VpiRet vpi_cc_enqueue_data(VpiCCContext *ctx, VpiFrm *frame);

/**
*   Take the CC data from the context and inject it to all the frames
*   Either attaches the side data to all the frames or leaves all the frames untouched.
*   returns VPI_SUCCESS on success, a negative error code otherwise.
*   Support for more than one frame is added to handle scaler ladder usecase.
*   The data injected into the frames is removed from the context.
*/
VpiRet vpi_cc_inject_data(VpiCCContext *ctx, VpiFrm **frames, int num_frames);

#if defined(__cplusplus)
}
#endif

#endif
