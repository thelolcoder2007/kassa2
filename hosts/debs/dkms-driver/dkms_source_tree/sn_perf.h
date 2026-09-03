// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Xilinx Inc.
 */

#pragma once

#include "common.h"

int sn_perf_init(struct sn_tranx_t* tdev);
void sn_perf_release(struct sn_tranx_t* tdev);
long sn_perf_ioctl(struct file* filp, unsigned int cmd, unsigned long argp, struct sn_tranx_t* tdev);
void sn_perf_close(struct sn_tranx_t* tdev, struct file* filp);

typedef void* SnPerfHandle;
typedef int (*sn_perf_callback_fn)(struct sn_tranx_t* tdev, __u32 ipId, __u32 cmd, __u32 arg);
SnPerfHandle sn_perf_register(struct sn_tranx_t* tdev, const char* name, SN_PERF_IP_ID ipId, sn_perf_callback_fn callback, int num);
void sn_perf_record(SnPerfHandle handle, sn_perf_event* event);
void sn_perf_load(SnPerfHandle handle, __u32 instance, __u32 load);
