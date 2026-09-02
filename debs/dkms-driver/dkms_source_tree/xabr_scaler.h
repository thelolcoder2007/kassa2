// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 Xilinx Inc.
 *
 * This is xilinx scaler driver for Linux.
 * This file provide register operation and initialization,
 * like read/write a register or pull/push a batch of registers.
 */


#ifndef _XABR_H_
#define _XABR_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#include "common.h"


int xabr_scaler_init(struct sn_tranx_t *tdev);
void xabr_scaler_release(struct sn_tranx_t *tdev);
void xabr_close(struct sn_tranx_t *tdev, struct file *filp);

long xabr_scaler_ioctl(struct file *filp, unsigned int cmd, unsigned long argp, struct sn_tranx_t *tdev);

#endif /* end _XABR_H_ */
