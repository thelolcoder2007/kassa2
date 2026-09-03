/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2020 VeriSilicon Holdings Co., Ltd.
 */

#ifndef _VC8000E_H_
#define _VC8000E_H_

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */
#include <linux/types.h>

#define S1_VCE_IRQ 15
#define S1_IM_IRQ 16
#define S2_VCE_IRQ 25
#define S2_IM_IRQ 26

#define VCE_VCMD_CORE_OFF 0x0000
#define VCE_VC8000E_CORE_OFF 0x1000
#define VCE_DEC400_CORE_OFF 0x2000
#define S1_VCE_IM_OFF (S1_VC8000E_OFF + 0x4000)
#define S2_VCE_IM_OFF (S2_VC8000E_OFF + 0x4000)
#define VCE_IM_CORE_OFF 0x1000

#define ENC_HW_ID1 0x48320100
#define ENC_HW_ID2 0x80006000
#define CORE_INFO_MODE_OFFSET 31
#define CORE_INFO_AMOUNT_OFFSET 28

#define ASIC_STATUS_SEGMENT_READY 0x1000
#define ASIC_STATUS_FUSE_ERROR 0x200
#define ASIC_STATUS_SLICE_READY 0x100
#define ASIC_STATUS_LINE_BUFFER_DONE 0x080 /* low latency */
#define ASIC_STATUS_HW_TIMEOUT 0x040
#define ASIC_STATUS_BUFF_FULL 0x020
#define ASIC_STATUS_HW_RESET 0x010
#define ASIC_STATUS_ERROR 0x008
#define ASIC_STATUS_FRAME_READY 0x004
#define ASIC_IRQ_LINE 0x001
#define ASIC_STATUS_ALL                                                        \
	(ASIC_STATUS_SEGMENT_READY | ASIC_STATUS_FUSE_ERROR |                  \
	 ASIC_STATUS_SLICE_READY | ASIC_STATUS_LINE_BUFFER_DONE |              \
	 ASIC_STATUS_HW_TIMEOUT | ASIC_STATUS_BUFF_FULL |                      \
	 ASIC_STATUS_HW_RESET | ASIC_STATUS_ERROR | ASIC_STATUS_FRAME_READY)

long vc8000e_ioctl(struct file *filp, unsigned int cmd, unsigned long arg,
		   struct sn_tranx_t *tdev);
int vc8000e_init(struct sn_tranx_t *tdev);
void vc8000e_release(struct sn_tranx_t *tdev);

#endif /* end _VC8000E_H_ */
