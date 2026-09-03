// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 VeriSilicon Holdings Co., Ltd.
 *
 * This is vc8000e management driver for Linux.
 * vc8000e is a video encoder.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>

#include "common.h"
#include "vc8000e.h"
#include "transcoder.h"

#if (S1_VCE && S2_VCE)
#define MAX_SUBSYS 4
#else
#define MAX_SUBSYS 2
#endif

#if S1_VCE
#define SUBSYS_0_IO_ADDR (S1_VC8000E_OFF + 0x01000)
#define SUBSYS_0_IO_SIZE (40000 * 4) /* bytes */
#define S_ONEVF_VCE_0_IO_ADDR (ONE_VF_S1_VC8000E_OFF + 0x01000)

#define SUBSYS_1_IO_ADDR (S1_VC8000E_OFF + 0x05000)
#define SUBSYS_1_IO_SIZE (20000 * 4) /* bytes */
#define S_ONEVF_VCE_1_IO_ADDR (ONE_VF_S1_VC8000E_OFF + 0x05000)
#endif

#if S2_VCE
#define SUBSYS_2_IO_ADDR (S2_VC8000E_OFF + 0x01000)
#define SUBSYS_2_IO_SIZE (40000 * 4) /* bytes */
#define S_ONEVF_VCE_2_IO_ADDR (ONE_VF_S2_VC8000E_OFF + 0x01000)

#define SUBSYS_3_IO_ADDR (S2_VC8000E_OFF + 0x05000)
#define SUBSYS_3_IO_SIZE (20000 * 4) /* bytes */
#define S_ONEVF_VCE_3_IO_ADDR (ONE_VF_S2_VC8000E_OFF + 0x05000)
#endif
#define S_VCE_IO_ADDR  (VF_VC8000E_OFF + 0x05000)
#define S_VCE_IO_SIZE (20000 * 4)
#define S_VCE_0_IO_ADDR  (VF_VC8000E_OFF + 0x01000)
#define S_VCE_0_IO_SIZE (40000 * 4)

#define TIME_OUT_SEC 3

struct subsys_config {
	unsigned long base_addr;
	u32 iosize;
	u32 resouce_shared;
	u32 ctype;
	int slice_index;
};

struct vce_core_desc {
	u32 subsys_idx;
	u32 core_type;
	unsigned long offset;
	u32 reg_size;
	int irq;
	char *name;
	int slice_index;
};

struct subsys_data {
	struct subsys_config cfg;
	struct subsys_core_info core_info;
};

/*for all subsystem, the subsys info should be listed here for subsequent use*/
/*base_addr, iosize, resource_shared*/
static struct subsys_config pf_subsys_array[] = {
#if (S1_VCE && S2_VCE)
#if !SKIP_VCE
	{ SUBSYS_0_IO_ADDR, SUBSYS_0_IO_SIZE, 0, CORE_VC8000E, 0 }, //subsys_0
#endif
	{ SUBSYS_1_IO_ADDR, SUBSYS_1_IO_SIZE, 0, CORE_CUTREE, 0 }, //subsys_1
#if !SKIP_VCE
	{ SUBSYS_2_IO_ADDR, SUBSYS_2_IO_SIZE, 0, CORE_VC8000E, 1 }, //subsys_2
#endif
	{ SUBSYS_3_IO_ADDR, SUBSYS_3_IO_SIZE, 0, CORE_CUTREE, 1 } //subsys_3
#elif S1_VCE
#if !SKIP_VCE
	{ SUBSYS_0_IO_ADDR, SUBSYS_0_IO_SIZE, 0, CORE_VC8000E, 0 }, //subsys_0
#endif
	{ SUBSYS_1_IO_ADDR, SUBSYS_1_IO_SIZE, 0, CORE_CUTREE, 0 } //subsys_1
#elif S2_VCE
#if !SKIP_VCE
	{ SUBSYS_2_IO_ADDR, SUBSYS_2_IO_SIZE, 0, CORE_VC8000E, 1 }, //subsys_2
#endif
	{ SUBSYS_3_IO_ADDR, SUBSYS_3_IO_SIZE, 0, CORE_CUTREE, 1 } //subsys_3
#endif
};
static struct subsys_config vf1_subsys_array[] = {
#if !SKIP_VCE
	{ S_VCE_0_IO_ADDR, S_VCE_0_IO_SIZE, 0, CORE_VC8000E, 0 }, //subsys_0
#endif
	{ S_VCE_IO_ADDR, S_VCE_IO_SIZE, 0, CORE_CUTREE, 0 }
};
static struct subsys_config vf2_subsys_array[] = {
#if !SKIP_VCE
	{ S_VCE_0_IO_ADDR, S_VCE_0_IO_SIZE, 0, CORE_VC8000E, 1 }, //subsys_2
#endif
	{ S_VCE_IO_ADDR, S_VCE_IO_SIZE, 0, CORE_CUTREE, 1 }
};
static struct subsys_config vf_subsys_array[] = {
#if (S1_VCE && S2_VCE)
#if !SKIP_VCE
	{ S_ONEVF_VCE_0_IO_ADDR, SUBSYS_0_IO_SIZE, 0, CORE_VC8000E, 0 },
#endif
	{ S_ONEVF_VCE_1_IO_ADDR, SUBSYS_1_IO_SIZE, 0, CORE_CUTREE, 0 },
#if !SKIP_VCE
	{ S_ONEVF_VCE_2_IO_ADDR, SUBSYS_2_IO_SIZE, 0, CORE_VC8000E, 1 },
#endif
	{ S_ONEVF_VCE_3_IO_ADDR, SUBSYS_3_IO_SIZE, 0, CORE_CUTREE, 1 }
#elif S1_VCE
#if !SKIP_VCE
	{ S_ONEVF_VCE_0_IO_ADDR, SUBSYS_0_IO_SIZE, 0, CORE_VC8000E, 0 },
#endif
	{ S_ONEVF_VCE_1_IO_ADDR, SUBSYS_1_IO_SIZE, 0, CORE_CUTREE, 0 }
#elif S2_VCE
#if !SKIP_VCE
	{ S_ONEVF_VCE_2_IO_ADDR, SUBSYS_2_IO_SIZE, 0, CORE_VC8000E, 1 },
#endif
	{ S_ONEVF_VCE_3_IO_ADDR, SUBSYS_3_IO_SIZE, 0, CORE_CUTREE, 1 }
#endif
};
/*here config every core in all subsystem*/
/*NOTE: no matter what format(HEVC/H264/JPEG/AV1/...) is supported in VC8000E, just use [CORE_VC8000E] to indicate it's a VC8000E core*/
/*subsys_idx, core_type, offset, reg_size, irq*/
static struct vce_core_desc pf_core_array[] = {
#if (S1_VCE && S2_VCE)
#if !SKIP_VCE
	{ 0, CORE_VC8000E, 0x0, ENCODER_REGISTER_SIZE * 4, S1_VCE_IRQ,
	  "vc8000e_0", 0 }, //subsys_0_VC8000E
	{ 0, CORE_DEC400, 0x1000, DEC400_REGISTER_SIZE * 4, -1,
	  "null", 0 }, //subsys_0_DEC400
#endif
	{ 1, CORE_CUTREE, 0x0, IM_REGISTER_SIZE * 4, S1_IM_IRQ,
	  "im_0", 0 }, //subsys_1_CUTREE
#if !SKIP_VCE
	{ 2, CORE_VC8000E, 0x0, ENCODER_REGISTER_SIZE * 4, S2_VCE_IRQ,
	  "vc8000e_1", 1 }, //subsys_2_VC8000E
	{ 2, CORE_DEC400, 0x1000, DEC400_REGISTER_SIZE * 4, -1,
	  "null", 1 }, //subsys_2_DEC400
#endif
	{ 3, CORE_CUTREE, 0x0, IM_REGISTER_SIZE * 4, S2_IM_IRQ,
	  "im_1", 1 } //subsys_3_CUTREE
#elif S1_VCE
#if !SKIP_VCE
	{ 0, CORE_VC8000E, 0x0, ENCODER_REGISTER_SIZE * 4, S1_VCE_IRQ,
	  "vc8000e_0", 0 }, //subsys_0_VC8000E
	{ 0, CORE_DEC400, 0x1000, DEC400_REGISTER_SIZE * 4, -1,
	  "null", 0 }, //subsys_0_DEC400
#endif
	{ 1, CORE_CUTREE, 0x0, IM_REGISTER_SIZE * 4, S1_IM_IRQ,
	  "im_0", 0 } //subsys_1_CUTREE
#elif S2_VCE
#if !SKIP_VCE
	{ 0, CORE_VC8000E, 0x0, ENCODER_REGISTER_SIZE * 4, S2_VCE_IRQ,
	  "vc8000e_1", 1 }, //subsys_0_VC8000E
	{ 0, CORE_DEC400, 0x1000, DEC400_REGISTER_SIZE * 4, -1,
	  "null", 1 }, //subsys_0_DEC400
#endif
	{ 1, CORE_CUTREE, 0x0, IM_REGISTER_SIZE * 4, S2_IM_IRQ,
	  "im_1", 1 } //subsys_1_CUTREE
#endif
};
static struct vce_core_desc vf1_core_array[] = {
#if S1_VCE
#if !SKIP_VCE
	{ 0, CORE_VC8000E, 0x0, ENCODER_REGISTER_SIZE * 4, S1_VCE_IRQ,
	  "vc8000e_0", 0 }, //subsys_0_VC8000E
	{ 0, CORE_DEC400, 0x1000, DEC400_REGISTER_SIZE * 4, -1,
	  "null", 0 }, //subsys_0_DEC400
#endif
	{ 1, CORE_CUTREE, 0x0, IM_REGISTER_SIZE * 4, S1_IM_IRQ,
	  "im_0", 0 } //subsys_1_CUTREE
#endif
};
static struct vce_core_desc vf2_core_array[] = {
#if S2_VCE
#if !SKIP_VCE
	{ 0, CORE_VC8000E, 0x0, ENCODER_REGISTER_SIZE * 4, S2_VCE_IRQ,
	  "vc8000e_0", 1 }, //subsys_0_VC8000E
	{ 0, CORE_DEC400, 0x1000, DEC400_REGISTER_SIZE * 4, -1,
	  "null", 1 }, //subsys_0_DEC400
#endif
	{ 1, CORE_CUTREE, 0x0, IM_REGISTER_SIZE * 4, S2_IM_IRQ,
	  "im_0", 1 } //subsys_1_CUTREE
#endif
};

/* here's all the must remember stuff */
struct hantroenc_t {
	struct subsys_data
	subsys_data; //config of each core,such as base addr, iosize,etc
	u32 hw_id; //VC8000E/VC8000EJ hw id to indicate project
	u32 subsys_id; //subsys id for driver and sw internal use
	u32 is_valid; //indicate this subsys is hantro's core or not
	int pid[CORE_MAX]; //indicate which process is occupying the subsys
	volatile u8 __iomem *hwregs;
	u32 job_id[CORE_MAX];
	unsigned long subsys_offset;
	struct enc_vce *tvce;
	struct file *filp[CORE_MAX];
};

struct enc_vce {
	int total_subsys_num;
	struct hantroenc_t hantroenc[MAX_SUBSYS];
	wait_queue_head_t reserve_queue;
	wait_queue_head_t enc_wait_queue;
	struct sn_tranx_t *tdev;
	int cutree_used[2];    // IM_0, IM_1 status
	wait_queue_head_t reset_core_queue;
};

extern int total_vcmd_vc8000e_core_num;

static int vce_check_id(struct sn_tranx_t *tdev)
{
	int i;
	u32 hwid, found_hw = 0, hw_cfg;
	u32 val;

	struct enc_vce *tvce = tdev->modules[SN_MODULE_VC8000E];

	for (i = 0; i < tvce->total_subsys_num; i++) {
		tvce->hantroenc[i].hwregs =
			tdev->bar2_virt + tvce->hantroenc[i].subsys_offset;
		/*read hwid and check validness and store it*/
		hwid = readl(tvce->hantroenc[i].hwregs);
		sn_pri(tdev, SN_DBG, "vce: hwid=0x%08x\n", hwid);

		/* check for encoder HW ID */
		if (((((hwid >> 16) & 0xFFFF) !=
		      ((ENC_HW_ID1 >> 16) & 0xFFFF))) &&
		    ((((hwid >> 16) & 0xFFFF) !=
		      ((ENC_HW_ID2 >> 16) & 0xFFFF)))) {
			sn_pri(tdev, SN_DBG, "vce: HW not found at 0x%lx\n",
			       tvce->hantroenc[i].subsys_data.cfg.base_addr);

			tvce->hantroenc[i].is_valid = 0;
			continue;
		}
		tvce->hantroenc[i].hw_id = hwid;
		tvce->hantroenc[i].is_valid = 1;
		found_hw = 1;

		hw_cfg = readl(tvce->hantroenc[i].hwregs + 320);
		tvce->hantroenc[i].subsys_data.core_info.type_info &=
			0xFFFFFFFC;
		if (hw_cfg & 0x88000000)
			tvce->hantroenc[i].subsys_data.core_info.type_info |=
				(1 << CORE_VC8000E);
		if (hw_cfg & 0x00008000)
			tvce->hantroenc[i].subsys_data.core_info.type_info |=
				(1 << CORE_VC8000EJ);

		/* disable dec400 interrupt */
		val = readl(tdev->bar2_virt +
			    (tvce->hantroenc[i].subsys_offset - 0x1000) + 0x64);
		writel(val | (1 << 2),
		       tdev->bar2_virt + (tvce->hantroenc[i].subsys_offset - 0x1000) +
			       0x64);

		sn_pri(tdev, SN_DBG,
		       "vce: HW at base <0x%lx> with ID <0x%08x>\n",
		       tvce->hantroenc[i].subsys_data.cfg.base_addr, hwid);
	}

	if (found_hw == 0) {
		sn_pri(tdev, SN_ERR, "vce: NO ANY HW found!!\n");
		return -1;
	}

	return 0;
}

static struct subsys_config* get_total_subsys_num(struct enc_vce *tvce,
								int* total_subsys_num)
{
	struct subsys_config *ret_subsys_pointer = NULL;
	u8 pf_vf_mode = tvce->tdev->pf_vf_mode;
	switch (pf_vf_mode) {
		case PF_MODE:
			if (tvce->tdev->vf_index == PF_INDEX) {
				*total_subsys_num =
				sizeof(pf_subsys_array) / sizeof(pf_subsys_array[0]);
				ret_subsys_pointer = pf_subsys_array;
			}
			else {
				sn_pri(tvce->tdev, SN_ERR, "%s,vf_index incorrect in PF_MODE\n",
						__func__);
			}
			break;
		case TWO_VF_MODE:
			if (tvce->tdev->vf_index == VF2_INDEX) {
				*total_subsys_num =
				sizeof(vf2_subsys_array) / sizeof(vf2_subsys_array[0]);
				ret_subsys_pointer = vf2_subsys_array;
			}
			else if (tvce->tdev->vf_index == VF1_INDEX) {
				*total_subsys_num =
				sizeof(vf1_subsys_array) / sizeof(vf1_subsys_array[0]);
				ret_subsys_pointer = vf1_subsys_array;
			}
			else {
				sn_pri(tvce->tdev, SN_ERR, "%s,vf_index incorrect in TWO_VF_MODE\n",
						__func__);
			}
			break;
		case ONE_VF_MODE:
			if (tvce->tdev->vf_index == VF1_INDEX) {
				*total_subsys_num =
				sizeof(vf_subsys_array) / sizeof(vf_subsys_array[0]);
				ret_subsys_pointer = vf_subsys_array;
			}
			else {
				sn_pri(tvce->tdev, SN_ERR, "%s,vf_index incorrect in ONE_VF_MODE\n",
						__func__);
			}
			break;
		default:
			sn_pri(tvce->tdev, SN_INF, "%s,vf_max_count incorrect\n",
					__func__);
			break;
	}
	return ret_subsys_pointer;
}

static struct vce_core_desc* get_current_core_array(struct enc_vce *tvce,
						int *total_core_num)
{
	struct vce_core_desc* ret_core_pointer = NULL;
	u8 pf_vf_mode = tvce->tdev->pf_vf_mode;
	switch (pf_vf_mode) {
		case PF_MODE:
			if (tvce->tdev->vf_index == PF_INDEX) {
				*total_core_num =
				sizeof(pf_core_array) / sizeof(pf_core_array[0]);
				ret_core_pointer = pf_core_array;
			}
			else {
				sn_pri(tvce->tdev, SN_ERR, "%s,vf_index incorrect in PF_MODE\n",
						__func__);
			}
			break;
		case TWO_VF_MODE:
			if (tvce->tdev->vf_index == VF2_INDEX) {
				*total_core_num =
				sizeof(vf2_core_array) / sizeof(vf2_core_array[0]);
				ret_core_pointer = vf2_core_array;
			}
			else if (tvce->tdev->vf_index == VF1_INDEX) {
				*total_core_num =
				sizeof(vf1_core_array) / sizeof(vf1_core_array[0]);
				ret_core_pointer = vf1_core_array;
			}
			else {
				sn_pri(tvce->tdev, SN_ERR, "%s,vf_index incorrect in TWO_VF_MODE\n",
						__func__);
			}
			break;
		case ONE_VF_MODE:
			if (tvce->tdev->vf_index == VF1_INDEX) {
				*total_core_num =
				sizeof(pf_core_array) / sizeof(pf_core_array[0]);
				ret_core_pointer = pf_core_array;
			}
			else {
				sn_pri(tvce->tdev, SN_ERR, "%s,vf_index incorrect in ONE_VF_MODE\n",
						__func__);
			}
			break;
		default:
			sn_pri(tvce->tdev, SN_INF, "%s,vf_max_count incorrect\n",
					__func__);
			break;
	}
	return ret_core_pointer;
}

static void disable_vcmd_gate(struct enc_vce *tvce)
{
	u8 pf_vf_mode = tvce->tdev->pf_vf_mode;
	switch (pf_vf_mode) {
		case PF_MODE:
#if S2_VCE
#if !SKIP_VCE
			writel(0x0000FFFE, tvce->tdev->bar2_virt +
					S2_VC8000E_OFF + 0x64);
#endif
			writel(0x0000FFFD, tvce->tdev->bar2_virt +
					S2_VC8000E_OFF + 0x4000 + 0x64);
#endif
#if S1_VCE
#if !SKIP_VCE
			writel(0x0000FFFE, tvce->tdev->bar2_virt +
					S1_VC8000E_OFF + 0x64);
#endif
			writel(0x0000FFFD, tvce->tdev->bar2_virt +
					S1_VC8000E_OFF + 0x4000 + 0x64);
#endif
			break;
		case TWO_VF_MODE:
#if S2_VCE
#if !SKIP_VCE
			writel(0x0000FFFE, tvce->tdev->bar2_virt +
					VF_VC8000E_OFF + 0x64);
#endif
			writel(0x0000FFFD, tvce->tdev->bar2_virt +
					VF_VC8000E_OFF + 0x4000 + 0x64);
#endif
#if S1_VCE
#if !SKIP_VCE
			writel(0x0000FFFE, tvce->tdev->bar2_virt +
					VF_VC8000E_OFF + 0x64);
#endif
			writel(0x0000FFFD, tvce->tdev->bar2_virt +
					VF_VC8000E_OFF + 0x4000 + 0x64);
#endif
			break;
		case ONE_VF_MODE:
#if S2_VCE
#if !SKIP_VCE
			writel(0x0000FFFE, tvce->tdev->bar2_virt +
					ONE_VF_S2_VC8000E_OFF + 0x64);
#endif
			writel(0x0000FFFD, tvce->tdev->bar2_virt +
					ONE_VF_S2_VC8000E_OFF + 0x4000 + 0x64);
#endif
#if S1_VCE
#if !SKIP_VCE
			writel(0x0000FFFE, tvce->tdev->bar2_virt +
					ONE_VF_S1_VC8000E_OFF + 0x64);
#endif
			writel(0x0000FFFD, tvce->tdev->bar2_virt +
					ONE_VF_S1_VC8000E_OFF + 0x4000 + 0x64);
#endif
			break;
		default:
			sn_pri(tvce->tdev, SN_INF, "%s,vf_max_count incorrect\n",
			       __func__);
			break;
	}
}

static int vc8000e_subsys_reset_keep(struct sn_tranx_t *tdev)
{
#if S1_VCE
	sys_config_reset_keep(tdev, 0, SYS_CTL_VCE);
#endif

#if S2_VCE
	sys_config_reset_keep(tdev, 1, SYS_CTL_VCE);
#endif

	return 0;
}

static int vc8000e_subsys_reset_release(struct sn_tranx_t *tdev)
{
#if S1_VCE
	sys_config_reset_release(tdev, 0, SYS_CTL_VCE);
#endif

#if S2_VCE
	sys_config_reset_release(tdev, 1, SYS_CTL_VCE);
#endif


#if VCMD_ENABLE_VC8000E

#if S1_VCE
	hantrovcmd_init_ex(tdev, 0, SYS_CTL_VCE, 0);
	hantrovcmd_init_ex(tdev, 0, SYS_CTL_VCE, 1);
#endif

#if S2_VCE
	hantrovcmd_init_ex(tdev, 1, SYS_CTL_VCE, 0);
	hantrovcmd_init_ex(tdev, 1, SYS_CTL_VCE, 1);
#endif

#else

	switch (tdev->pf_vf_mode) {
		case PF_MODE:
#if S1_VCE && !SKIP_VCE
			writel(0xFFFFFFFE, tdev->bar2_virt + S1_VC8000E_OFF + 0x64);
			writel(0xFFFFFFFD, tdev->bar2_virt + S1_VC8000E_OFF + 0x4000 + 0x64);
#endif
#if S2_VCE && !SKIP_VCE
			writel(0xFFFFFFFE, tdev->bar2_virt + S2_VC8000E_OFF + 0x64);
			writel(0xFFFFFFFD, tdev->bar2_virt + S2_VC8000E_OFF + 0x4000 + 0x64);
#endif
	break;
		case ONE_VF_MODE:
#if S1_VCE && !SKIP_VCE
			writel(0xFFFFFFFE, tdev->bar2_virt + ONE_VF_S1_VC8000E_OFF + 0x64);
			writel(0xFFFFFFFD, tdev->bar2_virt + ONE_VF_S1_VC8000E_OFF + 0x4000 + 0x64);
#endif
#if S2_VCE && !SKIP_VCE
			writel(0xFFFFFFFE, tdev->bar2_virt + ONE_VF_S2_VC8000E_OFF + 0x64);
			writel(0xFFFFFFFD, tdev->bar2_virt + ONE_VF_S2_VC8000E_OFF + 0x4000 + 0x64);
#endif
		break;
		case TWO_VF_MODE:
#if S1_VCE && !SKIP_VCE
			writel(0xFFFFFFFE, tdev->bar2_virt + VF_VC8000E_OFF + 0x64);
			writel(0xFFFFFFFD, tdev->bar2_virt + VF_VC8000E_OFF + 0x4000 + 0x64);
#endif
#if S2_VCE && !SKIP_VCE
			writel(0xFFFFFFFE, tdev->bar2_virt + VF_VC8000E_OFF + 0x64);
			writel(0xFFFFFFFD, tdev->bar2_virt + VF_VC8000E_OFF + 0x4000 + 0x64);
#endif
		break;
	}
#endif

	return 0;
}

static int vc8000e_soft_reset(struct sn_tranx_t *tdev)
{
#if !VCMD_ENABLE_VC8000E
	int ret = 0;

	struct enc_vce *tvce = tdev->modules[SN_MODULE_VC8000E];
#endif
	vc8000e_subsys_reset_keep(tdev);
	vc8000e_subsys_reset_release(tdev);
#if !VCMD_ENABLE_VC8000E
	if (tvce == NULL) {
		sn_pri(tdev, SN_ERR, "vce: it's null please check!!!\n");
		return -EFAULT;
	}
	ret = vce_check_id(tdev);
	if (ret < 0) {
		kfree(tvce);
		tdev->modules[SN_MODULE_VC8000E] = NULL;
		sn_pri(tdev, SN_ERR, "vce: module reset failed\n");
		return ret;
	}
#endif

	sn_pri(tdev, SN_INF, "vce: soft reset done.\n");
	return 0;

}

int vc8000e_init(struct sn_tranx_t *tdev)
{
	int ret = -1, i;
	u32 id, ctype;
	unsigned long bar2_base;
	struct enc_vce *tvce;
	int total_core_num;
	struct subsys_config *subsys_array = NULL;
	struct vce_core_desc *core_array = NULL;

	tvce = (struct enc_vce *)kzalloc(sizeof(*tvce), GFP_KERNEL);
	if (!tvce)
		return -ENOMEM;
	memset(tvce, 0, sizeof(*tvce));

	tdev->modules[SN_MODULE_VC8000E] = tvce;
	tvce->tdev = tdev;

	subsys_array = get_total_subsys_num( tvce, &(tvce->total_subsys_num));
	if(subsys_array == NULL) {
		sn_pri(tvce->tdev, SN_ERR, "%s,subsys_array is NULL\n",
			       __func__);
		goto err;
	}

	bar2_base = pci_resource_start(tdev->pdev, 2);
	for (i = 0; i < tvce->total_subsys_num; i++) {
		tvce->hantroenc[i].subsys_data.cfg = *(subsys_array + i);
		tvce->hantroenc[i].subsys_data.cfg.base_addr += bar2_base;
		tvce->hantroenc[i].subsys_id = i;
		tvce->hantroenc[i].tvce = tvce;
		tvce->hantroenc[i].subsys_offset = (subsys_array + i)->base_addr;
	}
	tvce->cutree_used[0] = tvce->cutree_used[1] = 0;
	core_array = get_current_core_array(tvce,&total_core_num);
	if(core_array == NULL) {
		sn_pri(tvce->tdev, SN_ERR, "%s,core_array is NULL\n",
			       __func__);
		goto err;
	}
	for (i = 0; i < total_core_num; i++) {
		id = (core_array + i)->subsys_idx;
		ctype = (core_array + i)->core_type;
		tvce->hantroenc[id].subsys_data.core_info.type_info |=
			(1 << ctype);
		tvce->hantroenc[id].subsys_data.core_info.offset[ctype] =
			(core_array + i)->offset;
		tvce->hantroenc[id].subsys_data.core_info.regSize[ctype] =
			(core_array + i)->reg_size;
	}

	init_waitqueue_head(&tvce->reserve_queue);
	init_waitqueue_head(&tvce->enc_wait_queue);
	init_waitqueue_head(&tvce->reset_core_queue);

	ret = vce_check_id(tdev);
	if (ret < 0)
		goto err;

	/*disable VCMD gate control:vce & im */
	disable_vcmd_gate(tvce);

	sn_pri(tdev, SN_INF, "vce: module initialize done.\n");
	return 0;

err:
	kfree(tvce);
	sn_pri(tdev, SN_DBG, "vce: module not inserted\n");
	return ret;
}

void vc8000e_release(struct sn_tranx_t *tdev)
{
	struct enc_vce *tvce = tdev->modules[SN_MODULE_VC8000E];

	kfree(tvce);
	sn_pri(tdev, SN_DBG, "vce: module removed\n");
}

long vc8000e_ioctl(struct file *filp, unsigned int cmd, unsigned long arg,
		   struct sn_tranx_t *tdev)
{
	int ret = 0;

	switch (cmd) {
	case HANTRO_IOC_SOFT_RESET:
		vc8000e_soft_reset(tdev);
		break;
	default:
		sn_pri(tdev, SN_ERR, "vce: ioctl cmd:0x%x is error\n", cmd);
	}

	return ret;
}
