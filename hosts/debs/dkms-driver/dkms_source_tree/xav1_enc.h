// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2021 Xilinx Inc.
 *
 * This is xilinx av1 encoder driver for Linux.
 * This file provide register operation and initialization,
 * like read/write a register or pull/push a batch of registers.
 */


#ifndef _XAV1_ENC_H_
#define _XAV1_ENC_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#include "common.h"

int xav1_enc_init(struct sn_tranx_t *tdev);
void xav1_enc_release(struct sn_tranx_t *tdev);
long xav1_enc_ioctl(struct file *filp,
                    unsigned int cmd,
                    unsigned long argp,
                    struct sn_tranx_t *tdev);
void xav1_close(struct sn_tranx_t *tdev, struct file *filp);
int xav1_get_register(struct sn_tranx_t *tdev, u32 slice_num, u32 reg);
int xav1_soft_reset(struct sn_tranx_t *tdev);
#endif
