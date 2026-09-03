// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Xilinx Inc.
 */

#pragma once

#include <linux/kfifo.h>
#include "common.h"

int sn_osal_init(struct sn_tranx_t* tdev);
void sn_osal_release(struct sn_tranx_t* tdev);
long sn_osal_ioctl(struct file* filp, unsigned int cmd, unsigned long argp, struct sn_tranx_t* tdev);
void sn_osal_close(struct sn_tranx_t* tdev, struct file* filp);
typedef void (*sn_osal_work_fn)(struct work_struct* work);
typedef void (*sn_osal_close_fn)(struct sn_tranx_t* tdev, struct file* filp);

typedef void* SnOsalHandle;

typedef struct sn_osal_work {
  struct work_struct  work;
  struct sn_tranx_t*  tdev;
  struct file*        filp; // for verifying memory handles, etc
  struct task_struct* task;
  uint64_t            handle;
#if defined(TRACE_OSAL)
  osal_accelerator    accel;
#endif
  uint32_t            cmdId;
  osal_command        cmd;
  uint32_t            numResp;
  uint32_t            data[];
} sn_osal_work;

int sn_osal_register(struct sn_tranx_t* tdev, const char* name, osal_accelerator accel, sn_osal_work_fn workFn, sn_osal_close_fn closeFn);
void sn_osal_finish_work(sn_osal_work* work);
