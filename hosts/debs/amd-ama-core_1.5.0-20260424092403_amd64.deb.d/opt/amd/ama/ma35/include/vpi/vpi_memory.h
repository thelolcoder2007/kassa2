/* SPDX-License-Identifier: LGPL-3.0-or-later OR Apache-2.0 */

/*
 * (c) Copyright 2024 VeriSilicon Holdings Co., Ltd. All rights reserved.
 *
 * This file is dual-licensed; you may select either the GNU
 * Lesser General Public License version 3 or
 * Apache License, Version 2.0.
 *
 */

#ifndef __VPI_MEMORY_H__
#define __VPI_MEMORY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "vpi_types.h"
#include "vpi_int.h"

#include <trans_mem_api.h>
#include <trans_edma_api.h>

typedef void* VpiMemHandle;

enum { DWL_MEM_TYPE, EWL_MEM_TYPE };
typedef struct VpiLinearMem {

    u32 size;         /* physical size (rounded to page multiple) */
    u32 request_size; /* requested size in bytes */

    /* rc side */
    void*  virtual_address_rc;

    /* ep side */
    addr_t bus_address_ep;

    int task_id;
    int mem_type;

} VpiLinearMem;

VpiRet vpi_memory_malloc_linear(VpiMemHandle handle, u32 size, int flag, VpiLinearMem* info);
void  vpi_memory_free_linear(VpiMemHandle handle, VpiLinearMem* info);
VpiRet vpi_memory_mem_sync(VpiMemHandle handle, VpiLinearMem* src, VpiLinearMem* dst, size_t offset, size_t size, int flag);
void* vpi_memory_malloc(u32 n);
void* vpi_memory_calloc(u32 n, u32 s);
void* vpi_memory_realloc(void* ptr, u32 size);
void  vpi_memory_free(void* p);
VpiRet vpi_memory_mem_sync_merge(VpiMemHandle handle, VpiLinearMem *src[],
                                 VpiLinearMem *dst, int src_num, int flag);
VpiMemHandle vpi_memory_get_ctx(void *params, VpiHwId type, u8 in);

void* vpi_memory_translate_host_ptr(u64 handle);

#if defined(__linux__)
int vpi_memory_wrap(VpiMemHandle handle, VpiLinearMem* mem);
#endif

#ifdef __cplusplus
}
#endif

#endif
