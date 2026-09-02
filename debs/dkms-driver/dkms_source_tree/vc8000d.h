/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2020 VeriSilicon Holdings Co., Ltd.
 */

#ifndef _VC8000D_H_
#define _VC8000D_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#include "common.h"

#define VCD_MAX_SUBSYS_NUM 4

#define S1_VCD_A_IRQ 11
#define S1_VCD_B_IRQ 12
#define S2_VCD_A_IRQ 21
#define S2_VCD_B_IRQ 22

#define VCD_VCMD_CORE_OFF 0x0000
#define VCD_VC8000D_CORE_OFF 0x1000
#define VCD_L2CACHE_CORE_OFF 0x2000
#define VCD_DEC400_CORE_OFF 0x6000

#define VCD_VCMD_REGS_CNT ASIC_VCMD_SWREG_AMOUNT
#define VCD_VC8000D_REGS_CNT 768 /*VC8000D total regs*/
#define VCD_L2CACHE_REGS_CNT 231
#define VCD_DEC400_REGS_CNT 1568

#define VCD_VCMD_REGS_IOSIZE (VCD_VCMD_REGS_CNT * 4)
#define VCD_VC8000D_REGS_IOSIZE (VCD_VC8000D_REGS_CNT * 4)
#define VCD_L2CACHE_REGS_IOSIZE (VCD_L2CACHE_REGS_CNT * 4)
#define VCD_DEC400_REGS_IOSIZE (VCD_DEC400_REGS_CNT * 4)

int vc8000d_init(struct sn_tranx_t *tdev);
void vc8000d_close(struct sn_tranx_t *tdev, struct file *filp);
void vc8000d_release(struct sn_tranx_t *tdev);
long vc8000d_ioctl(struct file *filp, unsigned int cmd, unsigned long argp,
		   struct sn_tranx_t *tdev);
int vc8000d_get_hw_iosize(void *vcd, struct regsize_desc *desc);

#endif /* end _VC8000D_H_ */
