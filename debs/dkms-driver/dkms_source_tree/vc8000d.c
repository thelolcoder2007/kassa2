// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 VeriSilicon Holdings Co., Ltd.
 *
 * This is vc8000d management driver for Linux.
 * vc8000d is a video decoder.
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
#include "vc8000d.h"
#include "vcmd/hantrovcmd.h"
#include "vcmd/vcmdswhwregisters.h"
#include "regs.h"

#define DWL_MPEG2_E 31 /* 1 bit  */
#define DWL_VC1_E 29 /* 2 bits */
#define DWL_JPEG_E 28 /* 1 bit  */
#define DWL_HJPEG_E 17 /* 1 bit  */
#define DWL_AV1_E 16 /* 1 bit  */
#define DWL_MPEG4_E 26 /* 2 bits */
#define DWL_H264_E 24 /* 2 bits */
#define DWL_H264HIGH10_E 20 /* 1 bits */
#define DWL_AVS2_E 18 /* 2 bits */
#define DWL_VP6_E 23 /* 1 bit  */
#define DWL_RV_E 26 /* 2 bits */
#define DWL_VP8_E 23 /* 1 bit  */
#define DWL_VP7_E 24 /* 1 bit  */
#define DWL_WEBP_E 19 /* 1 bit  */
#define DWL_AVS_E 22 /* 1 bit  */
#define DWL_G1_PP_E 16 /* 1 bit  */
#define DWL_G2_PP_E 31 /* 1 bit  */
#define DWL_PP_E 31 /* 1 bit  */
#define DWL_HEVC_E 26 /* 3 bits */
#define DWL_VP9_E 29 /* 3 bits */

#define DWL_H264_PIPELINE_E 31 /* 1 bit  */
#define DWL_JPEG_PIPELINE_E 30 /* 1 bit  */

#define DWL_G2_HEVC_E 0 /* 1 bits */
#define DWL_G2_VP9_E 1 /* 1 bits */
#define DWL_G2_RFC_E 2 /* 1 bits */
#define DWL_RFC_E 17 /* 2 bits */
#define DWL_G2_DS_E 3 /* 1 bits */
#define DWL_DS_E 28 /* 3 bits */
#define DWL_HEVC_VER 8 /* 4 bits */
#define DWL_VP9_PROFILE 12 /* 3 bits */
#define DWL_RING_E 16 /* 1 bits */

#define HANTRODEC_IRQ_STAT_DEC 1
#define HANTRODEC_IRQ_STAT_DEC_OFF (HANTRODEC_IRQ_STAT_DEC * 4)
#define BIGOCEAN_IRQ_STAT_DEC 2
#define BIGOCEAN_IRQ_STAT_DEC_OFF (HANTRODEC_IRQ_STAT_DEC * 4)

#define HANTRODECPP_SYNTH_CFG 60
#define HANTRODECPP_SYNTH_CFG_OFF (HANTRODECPP_SYNTH_CFG * 4)
#define HANTRODEC_SYNTH_CFG 50
#define HANTRODEC_SYNTH_CFG_OFF (HANTRODEC_SYNTH_CFG * 4)
#define HANTRODEC_SYNTH_CFG_2 54
#define HANTRODEC_SYNTH_CFG_2_OFF (HANTRODEC_SYNTH_CFG_2 * 4)
#define HANTRODEC_SYNTH_CFG_3 56
#define HANTRODEC_SYNTH_CFG_3_OFF (HANTRODEC_SYNTH_CFG_3 * 4)
#define HANTRODEC_CFG_STAT 23
#define HANTRODEC_CFG_STAT_OFF (HANTRODEC_CFG_STAT * 4)
#define HANTRODECPP_CFG_STAT 260
#define HANTRODECPP_CFG_STAT_OFF (HANTRODECPP_CFG_STAT * 4)

#define HANTRODEC_DEC_E 0x01
#define HANTRODEC_PP_E 0x01
#define HANTRODEC_DEC_ABORT 0x20
#define HANTRODEC_DEC_IRQ_DISABLE 0x10
#define HANTRODEC_DEC_IRQ 0x100
#define HANTRODEC_DEC_STATUS_ALL ((0x7FFF << 11)|HANTRODEC_DEC_IRQ)

/* VC8000D HW build id reg */
#define HANTRODEC_HW_BUILD_ID 309
#define HANTRODEC_HW_BUILD_ID_OFF (HANTRODEC_HW_BUILD_ID * 4)

#define HANTRO_DEC_E 0x01
#define HANTRO_PP_E 0x01
#define HANTRO_DEC_ABORT 0x20
#define HANTRO_DEC_IRQ_DISABLE 0x10
#define HANTRO_PP_IRQ_DISABLE 0x10
#define HANTRO_DEC_IRQ 0x100

#define DEC_IRQ_ABORT (1 << 11)
#define DEC_IRQ_RDY (1 << 12)
#define DEC_IRQ_BUS (1 << 13)
#define DEC_IRQ_BUFFER (1 << 14)
#define DEC_IRQ_ASO (1 << 15)
#define DEC_IRQ_ERROR (1 << 16)
#define DEC_IRQ_SLICE (1 << 17)
#define DEC_IRQ_TIMEOUT (1 << 18)
#define DEC_IRQ_LAST_SLICE_INT (1 << 19)
#define DEC_IRQ_NO_SLICE_INT (1 << 20)
#define DEC_IRQ_EXT_TIMEOUT (1 << 21)
#define DEC_IRQ_SCAN_RDY (1 << 25)
#define DEC_ABORT 0x20
#define DEC_ENABLE 0x01

#define BIGOCEANDEC_CFG 1
#define BIGOCEANDEC_AV1_E 5

/* hantro VC8000D reg config */
#define HANTRO_VC8000D_FIRST_REG 0
#define HANTRO_VC8000D_LAST_REG (VCD_VC8000D_REGS_CNT - 1)
#define HANTRODEC_HWBUILD_ID_OFF (309 * 4)

#define VCD_HWID 0x8001
#define IS_VC8000D(hw_id) (((hw_id) == VCD_HWID) ? 1 : 0)

#define VC8000D_NUM_MASK_REG 336
#define VC8000D_NUM_MODE 4
#define VC8000D_MASK_REG_OFFSET 4096
#define VC8000D_MASK_BITS_PER_REG 1

#define AV1_NUM_MASK_REG 303
#define AV1_NUM_MODE 1
#define AV1_MASK_REG_OFFSET 4096
#define AV1_MASK_BITS_PER_REG 1

#define CORE_TYPE_STR_CASE(ct)                                             \
	case (ct):                                                             \
		return (#ct + 3)

struct core_info {
	int slice_index;
	enum vcd_core_type core_type;
	int offset; /* offset to subsystem base */
	u64 base_addr; /* offset addr on bar2 */
	int iosize;
	int irq_index;
	int hw_id;
	volatile u8 __iomem *hwreg;
	u32 *shadow;
};

struct vcd_subsys {
	char subsys_name[20];
	int core_index;
	int slice_index;
	int irq;
	struct core_info core[HW_CORE_MAX];
	unsigned long subsys_offset;
	u32 irq_rcvd[HW_CORE_MAX];
	u32 irq_status[HW_CORE_MAX];
	struct file *dec_owner;
	u32 cfg;
	int client_type;
	enum SYS_CON_SUBSYS sysctl_sub;
	u32 status_shadow_offset;
};

struct vc8000d_t {
	int cores;
	int dec_irq;
	struct vcd_subsys subsys[VCD_MAX_SUBSYS_NUM];
	spinlock_t owner_lock;
	wait_queue_head_t wait_irq_queue;
	wait_queue_head_t rsv_rls_queue;
	void *regs_shadow;
	struct sn_tranx_t *tdev;
	struct loading_info loading[2];
	struct timer_list loading_timer;
};

/* subsys_decs are used for configuration */
struct subsys_decs {
	int slice_index; /* slice this subsys belongs to */
	long base;
	int irq_index;
	u32 status_shadow_offset;
};

/* {slice_index, index, base, iosize} */
static struct subsys_decs pf_subsys_array[] = {
#if S1_VCD_A
	{ 0, S1_VC8000D_A_OFF, S1_VCD_A_IRQ, 0x9fc },
#endif
#if S1_VCD_B
	{ 0, S1_VC8000D_B_OFF, S1_VCD_B_IRQ, 0x9f8 },
#endif
#if S2_VCD_A
	{ 1, S2_VC8000D_A_OFF, S2_VCD_A_IRQ, 0x9fc },
#endif
#if S2_VCD_B
	{ 1, S2_VC8000D_B_OFF, S2_VCD_B_IRQ, 0x9f8 },
#endif
};
static struct subsys_decs vf1_subsys_array[] = {
#if S1_VCD_A
	{ 0, VF_VC8000D_A_OFF, S1_VCD_A_IRQ, 0x9fc },
#endif
#if S1_VCD_B
	{ 0, VF_VC8000D_B_OFF, S1_VCD_B_IRQ, 0x9f8 },
#endif
};
static struct subsys_decs vf2_subsys_array[] = {
#if S2_VCD_A
	{ 1, VF_VC8000D_A_OFF, S2_VCD_A_IRQ, 0x9fc },
#endif
#if S2_VCD_B
	{ 1, VF_VC8000D_B_OFF, S2_VCD_B_IRQ, 0x9f8 },
#endif
};
static struct subsys_decs vf_subsys_array[] = {
#if S1_VCD_A
	{ 0, ONE_VF_S1_VC8000D_A_OFF, S1_VCD_A_IRQ, 0x9fc },
#endif
#if S1_VCD_B
	{ 0, ONE_VF_S1_VC8000D_B_OFF, S1_VCD_B_IRQ, 0x9f8 },
#endif
#if S2_VCD_A
	{ 1, ONE_VF_S2_VC8000D_A_OFF, S2_VCD_A_IRQ, 0x9fc },
#endif
#if S2_VCD_B
	{ 1, ONE_VF_S2_VC8000D_B_OFF, S2_VCD_B_IRQ, 0x9f8 },
#endif
};

static char *vcd_get_coretype_str(enum vcd_core_type ct)
{
	switch (ct) {
		CORE_TYPE_STR_CASE(HW_VC8000D);
		CORE_TYPE_STR_CASE(HW_VC8000DJ);
		CORE_TYPE_STR_CASE(HW_BIGOCEAN);
		CORE_TYPE_STR_CASE(HW_VCMD);
		CORE_TYPE_STR_CASE(HW_MMU);
		CORE_TYPE_STR_CASE(HW_MMU_WR);
		CORE_TYPE_STR_CASE(HW_DEC400);
		CORE_TYPE_STR_CASE(HW_L2CACHE);
		CORE_TYPE_STR_CASE(HW_SHAPER);
		CORE_TYPE_STR_CASE(HW_NOC);
		CORE_TYPE_STR_CASE(HW_AXIFE);
		CORE_TYPE_STR_CASE(HW_AFBC);
	default:
		return "Invalid core type";
	}
}

static struct subsys_decs* get_total_subsys_num(struct vc8000d_t *tvcd,
								u32 *total_subsys_num)
{
	struct subsys_decs *ret_subsys_pointer = NULL;
	u8 pf_vf_mode = tvcd->tdev->pf_vf_mode;
	switch (pf_vf_mode) {
		case PF_MODE:
			if (tvcd->tdev->vf_index != PF_INDEX) {
				sn_pri(tvcd->tdev, SN_ERR, "%s,vf_index incorrect in PF_MODE\n",
						__func__);
			}
			else {
				*total_subsys_num =
				sizeof(pf_subsys_array) / sizeof(pf_subsys_array[0]);
				ret_subsys_pointer = pf_subsys_array;
			}
			break;
		case TWO_VF_MODE:
			if (tvcd->tdev->vf_index == VF2_INDEX) {
				*total_subsys_num =
				sizeof(vf2_subsys_array) / sizeof(vf2_subsys_array[0]);
				ret_subsys_pointer = vf2_subsys_array;
			}
			else if (tvcd->tdev->vf_index == VF1_INDEX) {
				*total_subsys_num =
				sizeof(vf1_subsys_array) / sizeof(vf1_subsys_array[0]);
				ret_subsys_pointer = vf1_subsys_array;
			}
			else {
				sn_pri(tvcd->tdev, SN_ERR, "%s,vf_index incorrect in TWO_VF_MODE\n",
						__func__);
			}
			break;
		case ONE_VF_MODE:
			if (tvcd->tdev->vf_index == VF1_INDEX) {
				*total_subsys_num =
				sizeof(vf_subsys_array) / sizeof(vf_subsys_array[0]);
				ret_subsys_pointer = vf_subsys_array;
			}
			else {
				sn_pri(tvcd->tdev, SN_ERR, "%s,vf_index incorrect in ONE_VF_MODE\n",
						__func__);
			}
			break;
		default:
			sn_pri(tvcd->tdev, SN_INF, "%s,vf_max_count incorrect\n",
					__func__);
			break;
	}
	return ret_subsys_pointer;
}

static int init_subsys_core_info(struct vc8000d_t *tvcd)
{
	int i;
	u32 total_subsys_num;
	u32 total_io_size = 0;
	struct subsys_decs* subsys_array = NULL;

	subsys_array = get_total_subsys_num(tvcd, &total_subsys_num);
	if (subsys_array == NULL) {
		sn_pri(tvcd->tdev, SN_ERR, "%s, subsys_array is NULL\n",
				__func__);
		return total_io_size;
	}
	for (i = 0; i < total_subsys_num; i++) {
		tvcd->subsys[i].subsys_offset = (subsys_array + i)->base;
		tvcd->subsys[i].slice_index = (subsys_array + i)->slice_index;
		tvcd->subsys[i].sysctl_sub =
			get_subsys_config_index((subsys_array + i)->base);
#if !VCMD_ENABLE_VC8000D
		tvcd->subsys[i].irq =
			tvcd->tdev->msix_entries[(subsys_array + i)->irq_index]
				.vector;
#else
		tvcd->subsys[i].irq = -1;
#endif
		tvcd->subsys[i].core[HW_VCMD].offset = VCD_VCMD_CORE_OFF;
		tvcd->subsys[i].core[HW_VCMD].iosize = VCD_VCMD_REGS_IOSIZE;
		total_io_size += VCD_VCMD_REGS_IOSIZE;

		tvcd->subsys[i].core[HW_VC8000D].offset = VCD_VC8000D_CORE_OFF;
		tvcd->subsys[i].core[HW_VC8000D].iosize =
			VCD_VC8000D_REGS_IOSIZE;
		total_io_size += VCD_VC8000D_REGS_IOSIZE;

		tvcd->subsys[i].core[HW_L2CACHE].offset = VCD_L2CACHE_CORE_OFF;
		tvcd->subsys[i].core[HW_L2CACHE].iosize =
			VCD_L2CACHE_REGS_IOSIZE;
		total_io_size += VCD_L2CACHE_REGS_IOSIZE;

		if ((tvcd->subsys[i].subsys_offset == VF_VC8000D_A_OFF) ||
			(tvcd->subsys[i].subsys_offset == S1_VC8000D_A_OFF)||
			(tvcd->subsys[i].subsys_offset == S2_VC8000D_A_OFF)||
			(tvcd->subsys[i].subsys_offset == ONE_VF_S2_VC8000D_A_OFF)) {
			tvcd->subsys[i].core[HW_DEC400].offset =
				VCD_DEC400_CORE_OFF;
			tvcd->subsys[i].core[HW_DEC400].iosize =
				VCD_DEC400_REGS_IOSIZE;
			total_io_size += VCD_DEC400_REGS_IOSIZE;
		}
		tvcd->subsys[i].status_shadow_offset =
			(subsys_array + i)->status_shadow_offset;
		sprintf(tvcd->subsys[i].subsys_name, "s%d_%s",
			tvcd->subsys[i].slice_index,
			subsys_name[tvcd->subsys[i].sysctl_sub]);

		sn_pri(tvcd->tdev, SN_INF,
		       "vcd: %d name:%s subsys_offset:0x%x\n", i,
		       tvcd->subsys[i].subsys_name,
		       tvcd->subsys[i].subsys_offset);
	}
	return total_io_size;
}

static void read_core_config(struct vc8000d_t *tvcd)
{
	int c, j;
	u32 reg, tmp, mask;
	volatile u8 __iomem *hw_regs;

	for (c = 0; c < tvcd->cores; c++) {
		for (j = 0; j < HW_CORE_MAX; j++) {
			if (j != HW_VC8000D && j != HW_VC8000DJ &&
			    j != HW_BIGOCEAN)
				continue;
			if (!tvcd->subsys[c]
				     .core[j]
				     .hwreg) /* NOT defined core type */
				continue;

			hw_regs = tvcd->subsys[c].core[j].hwreg;
			/* Decoder configuration */
			if (IS_VC8000D(tvcd->subsys[c].core[j].hw_id)) {
				reg = readl(hw_regs + HANTRODEC_SYNTH_CFG * 4);
				tmp = (reg >> DWL_H264_E) & 0x3U;
				if (tmp)
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core[%d] has H264\n", c);
				tvcd->subsys[c].cfg |=
					tmp ? 1 << VC8000D_CLIENT_TYPE_H264_DEC : 0;

				tmp = (reg >> DWL_H264HIGH10_E) & 0x01U;
				if (tmp)
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core[%d] has H264HIGH10\n",
					       c);
				tvcd->subsys[c].cfg |=
					tmp ? 1 << VC8000D_CLIENT_TYPE_H264_DEC : 0;

				tmp = (reg >> DWL_AVS2_E) & 0x03U;
				if (tmp)
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core[%d] has AVS2\n", c);
				tvcd->subsys[c].cfg |=
					tmp ? 1 << VC8000D_CLIENT_TYPE_AVS2_DEC : 0;

				tmp = (reg >> DWL_AV1_E) & 0x01U;
				if (tmp)
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core[%d] has AV1\n", c);
				tvcd->subsys[c].cfg |=
					tmp ? 1 << VC8000D_CLIENT_TYPE_AV1_DEC : 0;

				tmp = (reg >> DWL_JPEG_E) & 0x01U;
				if (tmp)
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core[%d] has JPEG\n", c);
				tvcd->subsys[c].cfg |=
					tmp ? 1 << VC8000D_CLIENT_TYPE_JPEG_DEC : 0;

				tmp = (reg >> DWL_HJPEG_E) & 0x01U;
				if (tmp)
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core[%d] has HJPEG\n", c);
				tvcd->subsys[c].cfg |=
					tmp ? 1 << VC8000D_CLIENT_TYPE_JPEG_DEC : 0;

				tmp = (reg >> DWL_MPEG4_E) & 0x3U;
				if (tmp)
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core[%d] has MPEG4\n", c);
				tvcd->subsys[c].cfg |=
					tmp ? 1 << VC8000D_CLIENT_TYPE_MPEG4_DEC :
					      0;

				tmp = (reg >> DWL_VC1_E) & 0x3U;
				if (tmp)
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core[%d] has VC1\n", c);
				tvcd->subsys[c].cfg |=
					tmp ? 1 << VC8000D_CLIENT_TYPE_VC1_DEC : 0;

				tmp = (reg >> DWL_MPEG2_E) & 0x01U;
				if (tmp)
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core[%d] has MPEG2\n", c);
				tvcd->subsys[c].cfg |=
					tmp ? 1 << VC8000D_CLIENT_TYPE_MPEG2_DEC :
					      0;

				tmp = (reg >> DWL_VP6_E) & 0x01U;
				if (tmp)
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core[%d] has VP6\n", c);
				tvcd->subsys[c].cfg |=
					tmp ? 1 << VC8000D_CLIENT_TYPE_VP6_DEC : 0;

				reg = readl(hw_regs +
					    HANTRODEC_SYNTH_CFG_2 * 4);
				if (0x1F70 !=
				    readl(hw_regs + HANTRODEC_SYNTH_CFG * 4)) {
					/* VP7 and WEBP is part of VP8 */
					mask = (1 << DWL_VP8_E) |
					       (1 << DWL_VP7_E) |
					       (1 << DWL_WEBP_E);
					tmp = (reg & mask);
					if (tmp & (1 << DWL_VP8_E))
						sn_pri(tvcd->tdev, SN_INF,
						       "vcd: core[%d] has VP8\n",
						       c);
					if (tmp & (1 << DWL_VP7_E))
						sn_pri(tvcd->tdev, SN_INF,
						       "vcd: core[%d] has VP7\n",
						       c);
					if (tmp & (1 << DWL_WEBP_E))
						sn_pri(tvcd->tdev, SN_INF,
						       "vcd: core[%d] has WebP\n",
						       c);
					tvcd->subsys[c].cfg |=
						tmp ? 1 << VC8000D_CLIENT_TYPE_VP8_DEC :
						      0;
				}

				tmp = (reg >> DWL_AVS_E) & 0x01U;
				if (tmp)
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core[%d] has AVS\n", c);
				tvcd->subsys[c].cfg |=
					tmp ? 1 << VC8000D_CLIENT_TYPE_AVS_DEC : 0;

				tmp = (reg >> DWL_RV_E) & 0x03U;
				if (tmp)
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core[%d] has RV\n", c);
				tvcd->subsys[c].cfg |=
					tmp ? 1 << VC8000D_CLIENT_TYPE_RV_DEC : 0;

				reg = readl(hw_regs +
					    HANTRODEC_SYNTH_CFG_3 * 4);
				tmp = (reg >> DWL_HEVC_E) & 0x07U;
				if (tmp)
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core[%d] has HEVC\n", c);
				tvcd->subsys[c].cfg |=
					tmp ? 1 << VC8000D_CLIENT_TYPE_HEVC_DEC : 0;

				tmp = (reg >> DWL_VP9_E) & 0x07U;
				if (tmp)
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core[%d] has VP9\n", c);
				tvcd->subsys[c].cfg |=
					tmp ? 1 << VC8000D_CLIENT_TYPE_VP9_DEC : 0;

				/* Post-processor configuration */
				reg = readl(hw_regs + HANTRODECPP_CFG_STAT * 4);
				tmp = (reg >> DWL_PP_E) & 0x01U;
				if (tmp)
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core[%d] has PP\n", c);
				tvcd->subsys[c].cfg |=
					tmp ? 1 << VC8000D_CLIENT_TYPE_PP : 0;

				tvcd->subsys[c].cfg |= 1
						       << VC8000D_CLIENT_TYPE_ST_PP;
			}
		}
	}
}

static void release_decoder(struct vc8000d_t *tvcd, long core)
{
	u32 status;
	unsigned long flags;

	status = readl(tvcd->subsys[core].core[HW_VC8000D].hwreg +
		       HANTRODEC_IRQ_STAT_DEC_OFF);

	/* make sure HW is disabled */
	if (status & HANTRODEC_DEC_E) {
		sn_pri(tvcd->tdev, SN_ERR,
		       "vcd: DEC[%d] still enabled -> reset\n", core);
		/* abort decoder */
		status |= HANTRODEC_DEC_ABORT | HANTRODEC_DEC_IRQ_DISABLE;
		writel(status, tvcd->subsys[core].core[HW_VC8000D].hwreg +
				       HANTRODEC_IRQ_STAT_DEC_OFF);
	}

	if (core == 0) {
		tvcd->loading[0].tv_e = ktime_get();
		tvcd->loading[0].time_cnt +=
			ktime_to_us(ktime_sub(tvcd->loading[0].tv_e,
				tvcd->loading[0].tv_s));
	} else if (core == 2) {
		tvcd->loading[1].tv_e = ktime_get();
		tvcd->loading[1].time_cnt +=
			ktime_to_us(ktime_sub(tvcd->loading[1].tv_e,
				tvcd->loading[1].tv_s));
	}

	spin_lock_irqsave(&tvcd->owner_lock, flags);
	sn_pri(tvcd->tdev, SN_DBG,"release_decoder fp=%x, core=%d\n", tvcd->subsys[core].dec_owner, core);
	tvcd->subsys[core].dec_owner = NULL;
	tvcd->subsys[core].irq_status[HW_VC8000D] = 0;
	spin_unlock_irqrestore(&tvcd->owner_lock, flags);

	wake_up_interruptible_all(&tvcd->rsv_rls_queue);
}

static int check_core_info(struct vc8000d_t *tvcd)
{
	int i, j;
	u32 hwid, build_id, tmp_hwid;
	unsigned long bar2_base;

	bar2_base = pci_resource_start(tvcd->tdev->pdev, 2);
	for (i = 0; i < VCD_MAX_SUBSYS_NUM; i++) {
		for (j = 0; j < HW_CORE_MAX; j++) {
			if (tvcd->subsys[i].core[j].iosize) {
				tvcd->subsys[i].core[j].hwreg =
					tvcd->tdev->bar2_virt +
					tvcd->subsys[i].subsys_offset +
					tvcd->subsys[i].core[j].offset;
				tvcd->subsys[i].core[j].base_addr =
					bar2_base +
					tvcd->subsys[i].subsys_offset +
					tvcd->subsys[i].core[j].offset;

				tmp_hwid = hwid =
					readl(tvcd->subsys[i].core[j].hwreg);
				hwid = (hwid >> 16) & 0xFFFF;
				build_id = readl(tvcd->subsys[i].core[j].hwreg +
						 HANTRODEC_HW_BUILD_ID_OFF);
				if ((j == HW_VC8000D) && (hwid == VCD_HWID)) {
					tvcd->cores++;
					tvcd->subsys[i].core[j].hw_id = hwid;
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core_name:%s offset:0x%lx hwid:0x%x build_id:0x%x\n",
					       vcd_get_coretype_str(j),
					       tvcd->subsys[i].subsys_offset +
						       tvcd->subsys[i]
							       .core[j]
							       .offset,
					       tmp_hwid, build_id);
				} else {
					sn_pri(tvcd->tdev, SN_INF,
					       "vcd: core_name:%s offset:0x%lx, iosize:%d\n",
					       vcd_get_coretype_str(j),
					       tvcd->subsys[i].subsys_offset +
						       tvcd->subsys[i]
							       .core[j]
							       .offset,
					       tvcd->subsys[i].core[j].iosize);
				}
			}
		}
	}

	if (!tvcd->cores)
		return -EBUSY;

	return 0;
}

static irqreturn_t vc8000d_isr(int irq, void *data)
{
	unsigned long flags;
	unsigned int handled = 0;
	int i;
	int core = -1;
	volatile u8 __iomem *hwregs;
	u32 clear_irq;
	struct vc8000d_t *tvcd = (struct vc8000d_t *)data;
	u32 irq_status_dec = 0;
	u32 l2cache_shaper_status = 0;

	spin_lock_irqsave(&tvcd->owner_lock, flags);
	for (i = 0; i < tvcd->cores; i++) {
		if (tvcd->subsys[i].irq == irq) {
			core = i;
			break;
		}
	}

	if (core == -1) {
		sn_pri(tvcd->tdev, SN_ERR, "wrong irq=%d\n", irq);
		goto exit;
	}

		/* interrupt status register read */
		hwregs = tvcd->subsys[core].core[HW_VC8000D].hwreg;
		irq_status_dec = readl(hwregs + HANTRODEC_IRQ_STAT_DEC_OFF);
		sn_pri(tvcd->tdev, SN_DBG,
			"vcd: irq:%d received! core:%d, vcd status:0x%x\n",
			irq, core, irq_status_dec);

		if (irq_status_dec & HANTRODEC_DEC_STATUS_ALL) {
			sn_pri(tvcd->tdev, SN_DBG,
				"vcd: irq:%d received! core:%d, status:0x%x\n",
				irq, core, irq_status_dec);

			/* check irq error */
			if ((irq_status_dec & DEC_IRQ_EXT_TIMEOUT) ||
				(((irq_status_dec & DEC_ENABLE) && ((irq_status_dec >> 11) & 0xFF) == 0))) {

				sn_pri(tvcd->tdev, SN_ERR,
					"vcd: interrupt exception: 0x%x on irq %d, core=%d\n", irq_status_dec, irq, core);
				//reset shaper. it will be enabled in DWLEnableHw again.
				// writel(0, tvcd->subsys[i].core[HW_L2CACHE].hwreg + 0x20);
			}
			/* clear dec IRQ */
			clear_irq = irq_status_dec;
			clear_irq &= (~HANTRODEC_DEC_STATUS_ALL);
			writel(clear_irq, hwregs + HANTRODEC_IRQ_STAT_DEC_OFF);
			// clean l2cache error interrupt
			if (tvcd->subsys[core].core[HW_L2CACHE].hwreg) {
				l2cache_shaper_status = readl(tvcd->subsys[i].core[HW_L2CACHE].hwreg + 0x2c);
				sn_pri(tvcd->tdev, SN_DBG,
					"vcd: l2cache shaper status:0x%x\n", l2cache_shaper_status);
				if (l2cache_shaper_status & 1) {
					sn_pri(tvcd->tdev, SN_INF, "vcd: irq:%d! core=%d clear l2cache interrupt", irq, core);
					writel(1, tvcd->subsys[core].core[HW_L2CACHE].hwreg + 0x2c);
				}
			}

			tvcd->subsys[i].irq_status[HW_VC8000D] = irq_status_dec;
			tvcd->dec_irq |= (1 << core);
			handled++;
		}

exit:
	spin_unlock_irqrestore(&tvcd->owner_lock, flags);

	if (handled)
		wake_up_interruptible_all(&tvcd->wait_irq_queue);

	return IRQ_RETVAL(handled);
}

static void reset_asic(struct vc8000d_t *tvcd)
{
	int i, j;
	u32 status;
	volatile u8 __iomem *hw_regs;

	for (j = 0; j < tvcd->cores; j++) {
		if (!tvcd->subsys[j].core[HW_VC8000D].hwreg)
			continue;
		hw_regs = tvcd->subsys[j].core[HW_VC8000D].hwreg;
		status = readl(hw_regs + HANTRODEC_IRQ_STAT_DEC_OFF);
		if (status & HANTRODEC_DEC_E) {
			/* abort with IRQ disabled */
			status =
				HANTRODEC_DEC_ABORT | HANTRODEC_DEC_IRQ_DISABLE;
			writel(status, hw_regs + HANTRODEC_IRQ_STAT_DEC_OFF);
		}
		for (i = 4; i < tvcd->subsys[j].core[HW_VC8000D].iosize;
		     i += 4) {
			writel(0, hw_regs + i);
		}
	}
}

/*
 * Calculate the utilization of decoder in one second.
 * So the total used time divide one second is the utilization.
 */
static void dec_loading_timer_isr(struct timer_list *t)
{
	struct vc8000d_t *tvcd = from_timer(tvcd, t, loading_timer);
	mod_timer(&tvcd->loading_timer, jiffies + LOADING_TIME*HZ);

	tvcd->loading[0].time_cnt_saved = tvcd->loading[0].time_cnt;
	tvcd->loading[1].time_cnt_saved = tvcd->loading[1].time_cnt;

	tvcd->loading[0].time_cnt = 0;
	tvcd->loading[1].time_cnt = 0;

	tvcd->loading[0].total_time = LOADING_TIME * 1000000;
	tvcd->loading[1].total_time = LOADING_TIME * 1000000;
}

/* Display encoder utilization which is the average of the two core. */
static ssize_t dec_util_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct vc8000d_t *tvcd = tdev->modules[SN_MODULE_VC8000D];
	unsigned long average_loading;

	average_loading = (tvcd->loading[0].time_cnt_saved +
			   tvcd->loading[1].time_cnt_saved) * 100 /
			   (tvcd->loading[0].total_time +
			   tvcd->loading[1].total_time);
	if (average_loading > 100)
		average_loading = 100;

	return sprintf(buf, "%ld%%\n", average_loading);
}

static DEVICE_ATTR_RO(dec_util);

static struct attribute *trans_dec_sysfs_entries[] = {
	&dev_attr_dec_util.attr,
	NULL
};

static struct attribute_group trans_dec_attribute_group = {
	.name = NULL,
	.attrs = trans_dec_sysfs_entries,
};

int vc8000d_init(struct sn_tranx_t *tdev)
{
	int result = 0, i, j;
	int total_iosize;
	void *tmp_mem;
	struct vc8000d_t *tvcd;

	tvcd = kzalloc(sizeof(struct vc8000d_t), GFP_KERNEL);
	if (!tvcd) {
		sn_pri(tdev, SN_ERR, "vcd: kmalloc vc8000d_t failed\n");
		return -ENOMEM;
	}
	tdev->modules[SN_MODULE_VC8000D] = tvcd;
	tvcd->tdev = tdev;

	total_iosize = init_subsys_core_info(tvcd);
	if (!total_iosize) {
		sn_pri(tdev, SN_ERR, "vcd: total subsystem count is zero\n");
		goto out_free_tvcd;
	}

	spin_lock_init(&tvcd->owner_lock);
	init_waitqueue_head(&tvcd->wait_irq_queue);
	init_waitqueue_head(&tvcd->rsv_rls_queue);

	result = check_core_info(tvcd);
	if (result < 0) {
		sn_pri(tdev, SN_ERR, "vcd: reserve io failed\n");
		goto out_free_tvcd;
	}

	/* read configuration fo all cores */
	read_core_config(tvcd);

	/* reset hardware */
	reset_asic(tvcd);

	tmp_mem = vzalloc(total_iosize);
	if (!tmp_mem) {
		sn_pri(tdev, SN_ERR, "vcd: malloc regs_shadow mem failed\n");
		goto out_free_tvcd;
	}

	tvcd->regs_shadow = tmp_mem;
	for (i = 0; i < tvcd->cores; i++) {
		for (j = 0; j < HW_CORE_MAX; j++) {
			if (tvcd->subsys[i].core[j].iosize) {
				tvcd->subsys[i].core[j].shadow = tmp_mem;
				tmp_mem += tvcd->subsys[i].core[j].iosize;
			}
		}
	}

	for (i = 0; i < tvcd->cores; i++) {
		if (tvcd->subsys[i].irq > 0) {
			result = request_irq(tvcd->subsys[i].irq, vc8000d_isr,
					     IRQF_SHARED,
					     tvcd->subsys[i].subsys_name,
					     (void *)tvcd);
			if (result != 0) {
				if (result == -EINVAL) {
					sn_pri(tdev, SN_ERR,
					       "vcd: Bad IRQ:%d or handler\n",
					       tvcd->subsys[i].irq);
				} else if (result == -EBUSY) {
					sn_pri(tdev, SN_ERR,
					       "vcd: IRQ:%d busy\n",
					       tvcd->subsys[i].irq);
				}
				goto out_free_irq;
			} else {
				sn_pri(tdev, SN_INF, "%s irq %d registered!\n",
				       tvcd->subsys[i].subsys_name,
				       tvcd->subsys[i].irq);
			}
		} else
			sn_pri(tdev, SN_DBG, "vcd: IRQ:%d not in use!\n", i);
	}

	for (i = 0; i < tvcd->cores; i++)
		writel(0x0000FEFF, tvcd->subsys[i].core[HW_VCMD].hwreg + 0x64);

	result = sysfs_create_group(&tdev->misc_dev->this_device->kobj,
				 &trans_dec_attribute_group);
	if (result) {
		sn_pri(tdev, SN_ERR,
			"vc8000d: failed to create sysfs device attributes\n");
		goto out_free_irq;
	}

	tvcd->loading_timer.expires = jiffies + LOADING_TIME*HZ;
	timer_setup(&tvcd->loading_timer, dec_loading_timer_isr, 0);
	add_timer(&tvcd->loading_timer);

	sn_pri(tdev, SN_INF, "vcd: module initialization done\n");
	return 0;

out_free_irq:
	for (j = 0; j < i; j++) {
		if (tvcd->subsys[i].irq > 0)
			free_irq(tvcd->subsys[i].irq, (void *)tvcd);
	}
	vfree(tvcd->regs_shadow);
out_free_tvcd:
	kfree(tvcd);
	tdev->modules[SN_MODULE_VC8000D] = NULL;
	sn_pri(tdev, SN_ERR, "vcd: module not inserted\n");
	return result;
}

void vc8000d_close(struct sn_tranx_t *tdev, struct file *filp)
{
	int id;
	u32 status;
	struct vc8000d_t *tvcd = tdev->modules[SN_MODULE_VC8000D];

	for (id = 0; id < tvcd->cores; id++) {
		if (tvcd->subsys[id].dec_owner == filp) {
			sn_pri(tvcd->tdev, SN_INF,
			       "vc8000d: Abnormal exit, %s core:%d, filp=%p\n",
			       __func__, id, filp);
			status = readl(tvcd->subsys[id].core[HW_VC8000D].hwreg +
				       HANTRODEC_IRQ_STAT_DEC_OFF);
			/* make sure HW is disabled */
			if (status & HANTRODEC_DEC_E) {
				sn_pri(tvcd->tdev, SN_ERR,
				       "vcd: DEC[%li] still enabled -> reset\n",
				       id);
				/* abort decoder */
				status |= HANTRODEC_DEC_ABORT |
					  HANTRODEC_DEC_IRQ_DISABLE;
				writel(status,
				       tvcd->subsys[id].core[HW_VC8000D].hwreg +
					       HANTRODEC_IRQ_STAT_DEC_OFF);
			}
			release_decoder(tvcd, id);
		}
	}
}

void vc8000d_release(struct sn_tranx_t *tdev)
{
	struct vc8000d_t *tvcd = tdev->modules[SN_MODULE_VC8000D];
#if !VCMD_ENABLE_VC8000D
	int i = 0;
#endif

	del_timer_sync(&tvcd->loading_timer);
#if !VCMD_ENABLE_VC8000D
	/* free the IRQ */
	for (i = 0; i < tvcd->cores; i++) {
		if (tvcd->subsys[i].irq != -1)
			free_irq(tvcd->subsys[i].irq, (void *)tvcd);
	}
#endif

	sysfs_remove_group(&tdev->misc_dev->this_device->kobj,
				 &trans_dec_attribute_group);

	vfree(tvcd->regs_shadow);
	kfree(tvcd);
	tdev->modules[SN_MODULE_VC8000D] = NULL;
	sn_pri(tdev, SN_DBG, "vcd: remove module done\n");
}

int vc8000d_get_hw_iosize(void *vcd, struct regsize_desc *desc)
{
	struct vc8000d_t *tvcd = (struct vc8000d_t *)vcd;
	u32 id;

	if (desc->id >= VCD_MAX_SUBSYS_NUM)
		return -EFAULT;

	if (desc->type == HW_SHAPER) {
		/* Shaper is configured with l2cache. */
		if (tvcd->subsys[desc->id].core[HW_L2CACHE].hwreg) {
			id = readl(tvcd->subsys[desc->id]
					   .core[HW_L2CACHE]
					   .hwreg);
			switch ((id >> 16) & 0x3) {
			case 1: /* cache only */
				desc->size = 0;
				break;
			case 0: /* cache + shaper */
			case 2: /* shaper only*/
				desc->size =
					tvcd->subsys[desc->id]
						.core[HW_L2CACHE]
						.iosize;
				break;
			default:
				return -EFAULT;
			}
		} else
			desc->size = 0;
	} else
		desc->size = tvcd->subsys[desc->id]
					.core[desc->type]
					.iosize;

	return 0;
}

static int vc8000d_subsys_reset_keep(struct sn_tranx_t *tdev)
{
	struct vc8000d_t *tvcd = tdev->modules[SN_MODULE_VC8000D];
	int core;

	for(core = 0; core < tvcd->cores; core++) {
		sys_config_reset_keep(tdev, tvcd->subsys[core].slice_index, tvcd->subsys[core].sysctl_sub);
	}

	return 0;
}

static int vc8000d_subsys_reset_release(struct sn_tranx_t *tdev)
{
    struct vc8000d_t *tvcd = tdev->modules[SN_MODULE_VC8000D];
    int core;
    //int i;
    //int result = 0;

    for(core = 0; core < tvcd->cores; core++) {
        sys_config_reset_release(tdev, tvcd->subsys[core].slice_index, tvcd->subsys[core].sysctl_sub);
    }

#if VCMD_ENABLE_VC8000D
    for(core = 0; core < tvcd->cores; core++) {
        hantrovcmd_init_ex(tdev, tvcd->subsys[core].slice_index, tvcd->subsys[core].sysctl_sub, 0);
    }
#else
    for(core = 0; core < tvcd->cores; core++)
        writel(0xFFFFFEFF, tvcd->subsys[core].core[HW_VCMD].hwreg + 0x64);
#endif

	return 0;
}

static int vc8000d_soft_reset(struct sn_tranx_t *tdev)
{
	struct vc8000d_t *tvcd = tdev->modules[SN_MODULE_VC8000D];
	if (tvcd == NULL) {
		sn_pri(tdev, SN_ERR, "vcd: it's null please check!!!\n");
		return -EFAULT;
	}

	vc8000d_subsys_reset_keep(tdev);
	vc8000d_subsys_reset_release(tdev);
	/* read configuration fo all cores */
	read_core_config(tvcd);

	/* reset hardware */
	reset_asic(tvcd);

	sn_pri(tdev, SN_ERR, "vcd: soft reset done\n");
	return 0;
}

long vc8000d_ioctl(struct file *filp, unsigned int cmd, unsigned long argp,
		   struct sn_tranx_t *tdev)
{
	int ret = 0;

	(void)filp;
	(void)argp;

	switch (cmd) {
	case HANTRODEC_IOC_SOFT_RESET:
		vc8000d_soft_reset(tdev);
		ret = 0;
		break;
	default:
		return -ENOTTY;
	}
	return ret;
}
