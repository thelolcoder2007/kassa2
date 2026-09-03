/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 Verisilicon Inc.
 */

#ifndef __RISCV_H__
#define __RISCV_H__

#include <linux/ioctl.h>
#include <linux/types.h>

long riscv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg,
		 struct sn_tranx_t *tdev);
int riscv_init(struct sn_tranx_t *tdev);
void riscv_release(struct sn_tranx_t *tdev);

#endif // __RISCV_H__
