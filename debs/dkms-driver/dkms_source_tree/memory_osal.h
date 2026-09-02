/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2020 VeriSilicon Holdings Co., Ltd.
 */

#ifndef _SN_MEMORY_OSAL_H_
#define _SN_MEMORY_OSAL_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#include "common.h"

int sn_mem_osal_init(struct sn_tranx_t* tdev);
void sn_mem_osal_release(struct sn_tranx_t* tdev);
long sn_mem_osal_ioctl(struct file* filp, unsigned int cmd, unsigned long argp, struct sn_tranx_t* tdev);
void sn_mem_osal_close(struct sn_tranx_t* tdev, struct file* filp);

uint64_t sn_mem_osal_translate_handle(struct sn_tranx_t* tdev, struct file* filp, uint64_t handle, uint32_t size);
uint32_t sn_mem_osal_task_from_handle(uint64_t handle);
uint64_t sn_mem_osal_translate_mem(struct sn_tranx_t* tdev, uint32_t taskId, uint64_t address);

uint64_t sn_mem_osal_alloc_mem(struct sn_tranx_t* tdev, uint32_t size, struct file* filp, int id, int mmio);
int sn_mem_osal_free_mem(struct sn_tranx_t* tdev, uint64_t busaddr, struct file* filp);
static inline void* sn_mem_osal_translate_mmio(struct sn_tranx_t* tdev, uint64_t address) {
  return (address <= tdev->bar4_size) ? (uint8_t*) tdev->bar4_virt + address : NULL;
}

#endif /* _SN_MEMORY_OSAL_H_ */
