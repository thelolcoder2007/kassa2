/* SPDX-License-Identifier: LGPL-3.0-or-later OR Apache-2.0 */

/*
 * (c) Copyright 2024 VeriSilicon Holdings Co., Ltd. All rights reserved.
 *
 * This file is dual-licensed; you may select either the GNU
 * Lesser General Public License version 3 or
 * Apache License, Version 2.0.
 *
 */

#ifndef __VPI_BUFFER_H__
#define __VPI_BUFFER_H__

#include <stdbool.h>
#include <time.h>
#if !defined(FFMPEG_PTHREADS)
#include <pthread.h> // For pthread_mutex_t
#endif

#include "vpi_memory.h"

typedef unsigned int atomic_uint;

#define MAX_POOL_ITEM_NUM    2048
#define BUFPOOL_ITEM_TIMEOUT 30000   //ms (30sec)

#define vpi_atomic_init(obj, value) \
do {                            \
    *(obj) = (value);           \
} while(0)

#if defined(__linux__)

#define vpi_atomic_fetch_add(object, operand) \
    __sync_fetch_and_add(object, operand)

#define vpi_atomic_fetch_sub(object, operand) \
    __sync_fetch_and_sub(object, operand)

#define vpi_atomic_fetch_add_explicit(object, operand, order) \
    vpi_atomic_fetch_add(object, operand)

#define vpi_atomic_fetch_sub_explicit(object, operand, order) \
    vpi_atomic_fetch_sub(object, operand)

#define vpi_atomic_load(object) \
    (__sync_synchronize(), *(object))

#elif defined(_WIN32)

#include <windows.h>
#undef max
#undef min
#undef SendMessage
#undef ERROR

#define vpi_atomic_fetch_add(object, operand) \
    InterlockedExchangeAdd(object, operand)

// InterlockedExchangeSubtract is not available in winnt.h,
// we will use the standard InterlockedExchangeAdd with -ve
// operand. Also, the old value(before modification) is returned
// in the Linux's __sync_fetch_and_ functions.
// InterlockedExchangeAdd is used for the same reason instead of
// InterlockedAdd/InterlockedIncrement/InterlockedDecrement
#define vpi_atomic_fetch_sub(object, operand) \
    InterlockedExchangeAdd(object, -operand)

#define vpi_atomic_fetch_add_explicit(object, operand, order) \
    vpi_atomic_fetch_add(object, operand)

#define vpi_atomic_fetch_sub_explicit(object, operand, order) \
    vpi_atomic_fetch_sub(object, operand)

#define vpi_atomic_load(object) \
    (MemoryBarrier(), *(object))

#endif


typedef struct VpiBuffer {
    VpiLinearMem *m_handle;
    VpiLinearMem *m_metadata_handle;
    /**
     *  number of existing VpiBuffer instances referring to this buffer
     */
    atomic_uint refcnt;

    void *buf_paddr[VPE_CHANNEL_NUM][VPE_MAX_PLANES];    // physical address for every channel/plane
    void *buf_vaddr[VPE_CHANNEL_NUM][VPE_MAX_PLANES];    // virtual address for every channel/plane
    void *md_paddr[VPE_CHANNEL_NUM][VPE_MAX_PLANES];     // metadata physical address for every channel/plane
    void *md_vaddr[VPE_CHANNEL_NUM][VPE_MAX_PLANES];     // metadata virtual address for every channel/plane
    u32 buf_size;                                        // allocated buffer size
    u32 md_data_size;                                    // metadata valid data size

    void *data;                                          // point to VpiFrm, used to transmit some info
    u32 data_size;                                       // actually used size

    int num;                                             // poc num
    void *opaque;
    int used;
    pthread_mutex_t mutex;
    void *pool;

    u32 added_to_pp_queue;
    /**
     * a callback for freeing the data
     */
    void (*freebuf)(void *opaque, void *data, void *buf);
    /**
     * a callback from consumer for freeing the data
     */
    void (*consume_free)(void *consume_opaque, uint8_t *data);

    /**
     * an consume opaque pointer for outputs, to be used by the freeing callback
     */
    void *consume_opaque[VPE_CHANNEL_NUM];

    //Hack to be removed once decoder consume callback no longer uses data in vpiFrm
    void * split_frame;
} VpiBuffer;

typedef struct VpiBufferList {
    void *item;
    struct VpiBufferList *prev;
    struct VpiBufferList *next;
} VpiBufferList;

typedef struct VpiBufferPool {
    void *mem_handle;
    uint32_t num;
    uint32_t size;
    uint32_t md_size;
    uint32_t flag;
    uint32_t num_capacity;
    atomic_uint pool_refcnt;
    pthread_mutex_t mutex;
    VpiBufferList *free_list;
    uint32_t free_item_cnt;
    VpiBufferList *using_list;
    uint32_t using_item_cnt;
    VpiBuffer *buffer_item[MAX_POOL_ITEM_NUM];
    uint32_t item_cnt;
    void *opaque;
    void (*free)(void *opaque);

    /**
     * a callback for signaling the producer about
     * the extension of pool by the consumer.
     */
    void *signal_data;
    void (*signal_cb)(void *signal_data);

    /**
     * a callback for freeing opaque ctx, under the assumption 
     * that the vpi bufferpool owns it.
     */
    void (*free_opaque)(void *opaque);
} VpiBufferPool;

int vpi_buffer_get_ref_count(const VpiBuffer *buffer);
void vpi_buffer_ref(VpiBuffer *buffer);
void vpi_buffer_unref(VpiBuffer *buffer);
void vpi_buffer_release(VpiBuffer *buffer);

VpiRet vpi_buffer_pool_create(VpiMemHandle handle, uint32_t num, uint32_t size, uint32_t flag,
                                bool has_metadata, uint32_t metadata_size, VpiBufferPool **vpi_buf_pool);
void vpi_buffer_pool_register_cb(VpiBufferPool *pool, void *cb);
int vpi_pool_get_ref_count(const VpiBufferPool *pool);
void vpi_buffer_pool_ref(VpiBufferPool *pool);
void vpi_buffer_pool_unref(VpiBufferPool *pool);
VpiBufferList *vpi_buffer_pool_map(VpiBufferList *list, VpiBuffer *buf);
VpiBuffer *vpi_buffer_pool_map_item(VpiBufferPool *pool, void *data, u8 channel, u8 plane);
VpiBuffer *vpi_buffer_pool_map_data(VpiBufferPool *pool, void *data);
VpiBuffer *vpi_buffer_pool_map_free_item(VpiBufferPool *pool, void *data, u8 channel, u8 plane);
VpiBuffer *vpi_buffer_pool_map_num(VpiBufferPool *pool, int num);
VpiBuffer* vpi_buffer_pool_alloc_item(VpiBufferPool *pool);
VpiRet vpi_buffer_pool_alloc_specified_item(VpiBufferPool *pool, VpiBuffer *vpi_buf);
VpiRet vpi_buffer_pool_free_item(VpiBufferPool *pool, VpiBuffer *buf);
VpiRet vpi_buffer_pool_extend_mem_size(VpiBufferPool *pool, VpiBuffer *buffer, uint32_t size, uint32_t metadata_size);
VpiBuffer *vpi_buffer_pool_get_first_unused_item(VpiBufferPool *pool);
VpiBuffer *vpi_buffer_pool_get_tail(VpiBufferList *list);
int vpi_buffer_pool_free_first_item(VpiBufferPool *pool);
void *vpi_buffer_pool_get_first_opaque(VpiBufferPool *pool);
VpiBuffer* vpi_buffer_pool_get_list_item(VpiBufferPool *list, int index);
VpiRet vpi_buffer_pool_extend(VpiBufferPool *pool,  uint32_t num);
void vpi_buffer_pool_release(void *opaque);
void vpi_buffer_pool_register_signal_cb(VpiBufferPool *pool, void *cb);
void vpi_buffer_pool_register_free_opaque_cb(VpiBufferPool *pool, void *cb);

VpiRet vpi_buffer_get_timeout(struct timespec *end_time, uint32_t timeout_ms);
void *vpi_buffer_get_buf_handle(VpiBuffer *buffer);
void *vpi_buffer_get_md_handle(VpiBuffer *buffer);
int vpi_buffer_set_free_cb(VpiBuffer *buffer, void (*free)(void *opaque, void *data, void *buf));

void *vpi_buffer_get_paddr(VpiBuffer *buffer, u8 channel, u8 plane);
int vpi_buffer_set_paddr(VpiBuffer *buffer, void *paddr, u8 channel, u8 plane);
void *vpi_buffer_get_vaddr(VpiBuffer *buffer, u8 channel, u8 plane);
int vpi_buffer_set_vaddr(VpiBuffer *buffer, void *vaddr, u8 channel, u8 plane);
void *vpi_buffer_get_md_paddr(VpiBuffer *buffer, u8 channel, u8 plane);
void *vpi_buffer_get_md_vaddr(VpiBuffer *buffer, u8 channel, u8 plane);
int vpi_buffer_set_md_paddr(VpiBuffer *buffer, void *paddr, u8 channel, u8 plane);
int vpi_buffer_set_md_vaddr(VpiBuffer *buffer, void *vaddr, u8 channel, u8 plane);
int vpi_buffer_set_opaque(VpiBuffer *buffer, void *opaque);
void *vpi_buffer_get_opaque(VpiBuffer *buffer);

uint32_t vpi_buffer_get_size(VpiBuffer *buffer);
int vpi_buffer_get_datasize(VpiBuffer *buffer);
int vpi_buffer_set_datasize(VpiBuffer *buffer, int data_size);
int vpi_buffer_get_md_datasize(VpiBuffer *buffer);
int vpi_buffer_set_md_datasize(VpiBuffer *buffer, int md_size);
int vpi_buffer_set_data(VpiBuffer *buffer, void *data);
void *vpi_buffer_get_data(VpiBuffer *buffer);
int vpi_buffer_set_used(VpiBuffer *buffer, int used);
int vpi_buffer_set_num(VpiBuffer *buffer, int num);
VpiBufferPool *vpi_buffer_get_pool(VpiBuffer *buffer);

size_t vpi_buffer_pool_get_md_size(VpiBufferPool *pool);
int vpi_buffer_pool_get_depth(VpiBufferPool *pool);
int vpi_buffer_pool_get_capacity(VpiBufferPool *pool);
int vpi_buffer_pool_set_capacity(VpiBufferPool *pool, uint32_t capacity);
int vpi_buffer_pool_set_opaque(VpiBufferPool *pool, void *opaque);
int vpi_buffer_pool_get_flag(VpiBufferPool *pool);
int vpi_buffer_pool_get_free_cnt(VpiBufferPool *pool);
int vpi_buffer_pool_get_using_cnt(VpiBufferPool *pool);
int vpi_buffer_pool_set_signal_data(VpiBufferPool *pool, void *signal_data);

VpiBufferPool* vpiinfo_pool_create(uint32_t num, uint32_t data_size);
VpiRet vpiinfo_pool_extend(VpiBufferPool *pool,  uint32_t num, uint32_t data_size);
void *vpi_create_one_new_buffer(VpiBufferPool *pool, int size);

#endif
