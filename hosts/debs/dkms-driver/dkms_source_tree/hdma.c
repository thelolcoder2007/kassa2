// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2021 Verisilicon Inc.
 *
 * This is hdma transmission driver for Linux.
 */

#include <linux/errno.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>
#include <linux/fcntl.h>
#include <linux/pagemap.h>
#include <linux/firmware.h>
#include <linux/pci.h>
#include <linux/hrtimer.h>
#include <linux/bitmap.h>

#include "regs.h"
#include "common.h"
#include "hdma.h"
#include "transcoder.h"
#include "hw_monitor.h"
#include "error_notify.h"

#include "memory_osal.h"

#define HDMA_TIMEOUT_MS (500)
#define HDMA_TIMEOUT_INTERVAL_MS (50)
#define HDMA_INT_IDX 0

/* if this macro is 1, will show more information */
#define HDMA_DEBUG 0

#define HDMA_ENABLE 0x1
#define HDMA_DISABLE 0x0

#define HDMA_START 0x1
#define HDMA_STOP 0x2

#define LINK_LIST_EN 0x1
#define LINK_LIST_DIS 0xfffffffe

/* HDMA_DOORBELL_OFF */
#define DB_START (1 << 0)
#define DB_STOP (1 << 1)

/* HDMA_CONTROL1_OFF */
#define LLEN (1 << 0)
#define MEM_TYPE (1 << 1)
#define SRC_SNOOP (1 << 2)
#define DST_SNOOP (1 << 3)
#define RO (1 << 4)
#define AT (1 << 5)

/* control config for link list element */
#define LL_CB (1 << 0)
#define LL_TCB (1 << 1)
#define LL_LLP (1 << 2)
#define LL_LWIE (1 << 3)
#define LL_RWIE (1 << 4)

/* HDMA_CYCLE_OFF */
#define CYCLE_BIT (1 << 0)
#define CYCLE_STATE (1 << 1)

/* HDMA_WATERMARK_EN_OFF */
#define RWIE (1 << 0)
#define LWIE (1 << 1)

/* HDMA_FUNC_NUM_OFF */
#define PF_NUM_(v) (v)
#define VF_EN (1 << 16)
#define VF_NUM_(v) (v << 17)

/* HDMA_QOS_OFF */
#define TC_(v) (v)
#define WEIGHT_(v) (v << 3)
#define PF_DEPTH_(v) (v << 16)

/* for HDMA_INT_SETUP_OFF */
#define STOP_MASK (1 << 0)
#define WATERMARK_MASK (1 << 1)
#define ABORT_MASK (1 << 2)
#define RSIE (1 << 3)
#define LSIE (1 << 4)
#define RAIE (1 << 5)
#define LAIE (1 << 6)

/* Interrupt Clear */
#define STOP_CLEAR (1 << 0)
#define WATERMARK_CLEAR (1 << 1)
#define ABORT_CLEAR (1 << 2)

/* Channel Interrupt Status */
#define STOP (1 << 0)
#define WATERMARK (1 << 1)
#define ABORT (1 << 2)
#define ERROR (1 << 3)

/* hdma registers list */
#define HDMA_EN 0x00
#define HDMA_DOORBELL 0x04
#define HDMA_ELEM_PF 0x08
#define HDMA_LLP_LOW 0x10
#define HDMA_LLP_HIGH 0x14
#define HDMA_CYCLE 0x18
#define HDMA_XFERSIZE 0x1c
#define HDMA_SAR_LOW 0x20
#define HDMA_SAR_HIGH 0x24
#define HDMA_DAR_LOW 0x28
#define HDMA_DAR_HIGH 0x2c
#define HDMA_WATERMARK_EN 0x30
#define HDMA_CONTROL1 0x34
#define HDMA_FUNC_NUM 0x38
#define HDMA_QOS 0x3c
#define HDMA_STATUS 0x80
#define HDMA_INT_SETUP 0x88
#define HDMA_INT_STATUS 0x84
#define HDMA_INT_CLEAR 0x8c
#define HDMA_MSI_STOP_LOW 0x90
#define HDMA_MSI_STOP_HIGH 0x94
#define HDMA_MSI_ABORT_LOW 0x98
#define HDMA_MSI_ABORT_HIGH 0x9c
#define HDMA_MSI_WATERMARK_LOW 0xa0
#define HDMA_MSI_WATERMARK_HIGH 0xa4
#define HDMA_MSI_MSGD 0xa8

#define HDMA_WRCH_OFF(c) (0x200 * (c) + 0x000)
#define HDMA_RDCH_OFF(c) (0x200 * (c) + 0x100)

/* record hdma transmission size over one second(PCIE_BW_TIMER)
 * then use this size to calculate the pcie bandwidth roughly.
 */
#define PCIE_BW_TIMER (1 * HZ)

/*
 * xx_ELTA is hdma link table address in DDR (ep side). There are 8 rc2ep
 * channels and 8 ep2rc channels. every channel have HDMA_LT_SIZE
 * space to save link table.
 * sequence:
 *        rc2ep0,rc2ep1,rc2ep2,rc2ep3,rc2ep4,rc2ep5,rc2ep6,rc2ep7;
 *        ep2rc0,ep2rc1,ep2rc2,ep2rc3,ep2rc4,ep2rc5,ep2rc6,ep2rc7;
 */
#define ELTA_OFFSET 0x100000
#define PF_ELTA (0x0000000 + ELTA_OFFSET) /* slice1 DDR */
#define VF_ELTA (0x0000000 + ELTA_OFFSET) /* slice1 DDR */
#define VF1_ELTA (0x0000000 + ELTA_OFFSET) /* slice1 DDR */
#define VF2_ELTA (0x200000000 + ELTA_OFFSET) /* slice2 DDR */
#define HDMA_LT_SIZE 0x30000
/* link table offset */
#define LT_OFF(c) ((c)*HDMA_LT_SIZE)

/* hdma chanel's status, CHN_BUSY: used, CHN_IDLE: idle */
#define CHN_BUSY 0x1
#define CHN_IDLE 0x0

#define HDMA_VF_EN (1 << 16)
#define VF_NUM_OFF 17

#define TO_STRING(dir) (((dir) == RC2EP) ? "rc2ep" : "ep2rc")

struct rc_addr_info {
	dma_addr_t paddr;
	unsigned int size;
};

/* HDMA link table end description */
struct dw_hdma_llp {
	u32 control;
	u32 reserved;
	u32 llp_low;
	u32 llp_high;
};

static inline void hdma_write(struct sn_tranx_t *tdev, void __iomem *addr,
			      unsigned int val)
{
	writel(val, addr);
}

static inline unsigned int hdma_read(struct sn_tranx_t *tdev, void __iomem *addr)
{
	return readl(addr);
}

/*
 * according transmission dircetion, get a idle channel.
 *
 * @direction: the transmission direction of request channel.
 * @thdma: hdma struct detail information.
 * return value: <0: failed; >=0 ok;
 */
static int get_hdma_channel(u8 direction, struct hdma_t *thdma)
{
    int channel = -1;

    if (direction == RC2EP) {
        if (down_interruptible(&thdma->rc2ep_sem))
            return -ERESTARTSYS;

        spin_lock(&thdma->rc2ep_bm_lock);
        // Find the first available channel using the bitmap
        channel = find_first_zero_bit(thdma->rc2ep_bitmap, HDMA_CH_CNT);
        // Mark the channel as used
        set_bit(channel, thdma->rc2ep_bitmap);
        spin_unlock(&thdma->rc2ep_bm_lock);

        thdma->rc2ep[channel].used_cnt++;
        return channel;

    } else if (direction == EP2RC) {
        if (down_interruptible(&thdma->ep2rc_sem))
            return -ERESTARTSYS;

        spin_lock(&thdma->ep2rc_bm_lock);
        // Find the first available channel using the bitmap
        channel = find_first_zero_bit(thdma->ep2rc_bitmap, HDMA_CH_CNT);
        // Mark the channel as used
        set_bit(channel, thdma->ep2rc_bitmap);
        spin_unlock(&thdma->ep2rc_bm_lock);

        thdma->ep2rc[channel].used_cnt++;
        return channel;
    } else {
        sn_pri(thdma->tdev, SN_ERR,
               "hdma: %s, input direction:%d error.\n", __func__,
               direction);
    }

    return -EFAULT;
}

/*
 * free a channel;
 * when the channel is used up, the status need to be set to idle.
 *
 * @channel: channel index.
 * @direction: the transmission direction of this channel.
 * @thdma: hdma struct detail information.
 */
static void free_hdma_channel(int channel, u8 direction, struct hdma_t *thdma)
{
	if (direction == RC2EP) {
		thdma->rc2ep[channel].condition = 0;
        thdma->rc2ep[channel].chn_status = CHN_IDLE;
		spin_lock(&thdma->rc2ep_bm_lock);
		clear_bit(channel, thdma->rc2ep_bitmap);
		spin_unlock(&thdma->rc2ep_bm_lock);
		up(&thdma->rc2ep_sem);
	} else {
		thdma->ep2rc[channel].condition = 0;
        thdma->ep2rc[channel].chn_status = CHN_IDLE;
		spin_lock(&thdma->ep2rc_bm_lock);
		clear_bit(channel, thdma->ep2rc_bitmap);
		spin_unlock(&thdma->ep2rc_bm_lock);
		up(&thdma->ep2rc_sem);
	}
}

/*
 * Because writing EP DDR has latency,
 * ensure the link table has been writed to ddr completely,
 * add a flag at the end of the link table,check the falg,
 * untill flag is right. The latency is very short.
 */
static int check_link_table_done(struct sn_tranx_t *tdev, int c, u8 dir,
				 void __iomem *table_end, u32 flag)
{
	int try_times = 100;

	writel(flag, table_end);
	while (try_times--) {
		int loop = 10;
		while (loop--) {
			if (readl(table_end) == flag)
				return 0;
		}
		usleep_range(1, 2);
	}
	tdev->hw_err_flag = HW_ERR_FLAG;
	sn_pri(tdev, SN_ERR,
	       "hdma: check link table failed, enable hw_err, %s_chn:%d\n",
	       TO_STRING(dir), c);

	return -EFAULT;
}

/*
 * dump hdma link table information.
 */
static void dump_link_table(struct dma_link_table __iomem *table_info, int cnt,
			    struct sn_tranx_t *tdev, char lt_end, int dir,
			    int channel, int en_log)
{
	int i;
	struct dw_hdma_llp __iomem *hdma_llp;
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];
	u64 link_table_pa;

	if (!en_log)
		return;

	if (dir == RC2EP)
		link_table_pa = thdma->rc2ep[channel].lt_paddr;
	else
		link_table_pa = thdma->ep2rc[channel].lt_paddr;
	sn_pri(tdev, SN_DBG,
	       "hdma: dir:%s element_cnt=%d channel:%d link_table_pa:0x%llx\n",
	       TO_STRING(dir), cnt, channel, link_table_pa);

	for (i = 0; i < cnt; i++) {
		sn_pri(tdev, SN_DBG,
		       "hdma: ctl:0x%02x size:0x%x sh:0x%x sl:0x%x dh:0x%x dl:0x%x\n",
		       table_info[i].control, table_info[i].size,
		       table_info[i].sar_high, table_info[i].sar_low,
		       table_info[i].dst_high, table_info[i].dst_low);
	}

	if (lt_end) {
		hdma_llp = (struct dw_hdma_llp __iomem *)(&table_info[cnt]);
		sn_pri(tdev, SN_DBG,
		       "hdma: end ctrl:0x%02x rsv:0x%x llp_h:0x%x llp_l:0x%x\n",
		       hdma_llp->control, hdma_llp->reserved,
		       hdma_llp->llp_high, hdma_llp->llp_low);
	}
}

static void dump_hdma_configure(struct sn_tranx_t *tdev, u32 dir, u32 chn, u8 flag, u32 offset)
{
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];
	if (flag > tdev->print_level) {
		return; // skip all the checks
	}
	if (dir == RC2EP) {
		sn_pri(tdev, flag, "hdma:%s HDMA_EN = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_EN + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_DOORBELL = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_DOORBELL + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_ELEM_PF = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_ELEM_PF + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_LLP_LOW = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_LLP_LOW + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_LLP_HIGH = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_LLP_HIGH + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_CYCLE = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_CYCLE + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_XFERSIZE = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_XFERSIZE + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_SAR_LOW = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_SAR_LOW + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_SAR_HIGH = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_SAR_HIGH + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_DAR_LOW = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_DAR_LOW + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_DAR_HIGH = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_DAR_HIGH + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_WATERMARK_EN = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_WATERMARK_EN + offset));
		sn_pri(tdev, flag, "hdma:%s link tab status = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_CONTROL1 + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_FUNC_NUM = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_FUNC_NUM + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_QOS = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_QOS + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_STATUS = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_STATUS + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_INT_STATUS = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_INT_STATUS + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_INT_SETUP = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_INT_SETUP + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_INT_CLEAR = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_INT_CLEAR + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_MSI_STOP_LOW = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_MSI_STOP_LOW + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_MSI_STOP_HIGH = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_MSI_STOP_HIGH + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_MSI_ABORT_LOW = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_MSI_ABORT_LOW + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_MSI_ABORT_HIGH = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_MSI_ABORT_HIGH + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_MSI_WATERMARK_LOW = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_MSI_WATERMARK_LOW + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_MSI_WATERMARK_HIGH = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_MSI_WATERMARK_HIGH + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_MSI_MSGD = 0x%x.\n", __func__,
				readl(thdma->rc2ep[chn].chn_off + HDMA_MSI_MSGD + offset));
	}
	else if (dir == EP2RC) {
		sn_pri(tdev, flag, "hdma:%s HDMA_EN = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_EN + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_DOORBELL = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_DOORBELL + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_ELEM_PF = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_ELEM_PF + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_LLP_LOW = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_LLP_LOW + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_LLP_HIGH = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_LLP_HIGH + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_CYCLE = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_CYCLE + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_XFERSIZE = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_XFERSIZE + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_SAR_LOW = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_SAR_LOW + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_SAR_HIGH = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_SAR_HIGH + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_DAR_LOW = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_DAR_LOW + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_DAR_HIGH = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_DAR_HIGH + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_WATERMARK_EN = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_WATERMARK_EN + offset));
		sn_pri(tdev, flag, "hdma:%s link tab status = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_CONTROL1 + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_FUNC_NUM = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_FUNC_NUM + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_QOS = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_QOS + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_STATUS = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_STATUS + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_INT_STATUS = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_INT_STATUS + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_INT_SETUP = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_INT_SETUP + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_INT_CLEAR = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_INT_CLEAR + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_MSI_STOP_LOW = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_MSI_STOP_LOW + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_MSI_STOP_HIGH = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_MSI_STOP_HIGH + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_MSI_ABORT_LOW = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_MSI_ABORT_LOW + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_MSI_ABORT_HIGH = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_MSI_ABORT_HIGH + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_MSI_WATERMARK_LOW = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_MSI_WATERMARK_LOW + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_MSI_WATERMARK_HIGH = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_MSI_WATERMARK_HIGH + offset));
		sn_pri(tdev, flag, "hdma:%s HDMA_MSI_MSGD = 0x%x.\n", __func__,
				readl(thdma->ep2rc[chn].chn_off + HDMA_MSI_MSGD + offset));
	}
	else {
		sn_pri(tdev, SN_ERR, "hdma:%s dir error.\n", __func__);
	}
}

/*
 * dump hdma current channel all registers value.
 */
static void dump_regs(struct sn_tranx_t *tdev, int c,
		      struct trans_pcie_hdma *hdma_info, int en_log,
		      int linkmode)
{
	int i;
	void __iomem *start;
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];
	if (!en_log)
		return;

	if (!linkmode) {
		sn_pri(tdev, SN_ERR,
		       "hdma: %s size=0x%x sar=0x%llx dar=0x%llx\n",
		       TO_STRING(hdma_info->dir), hdma_info->size,
		       hdma_info->sar, hdma_info->dar);
	}

	if (hdma_info->dir == RC2EP) {
		start = thdma->rc2ep[c].chn_off;
	} else if (hdma_info->dir == EP2RC) {
		start = thdma->ep2rc[c].chn_off;
	} else {
		sn_pri(tdev, SN_ERR, "hdma: %s dir=0x%x error\n",
		       hdma_info->dir);
		return;
	}

	for (i = 0; i <= 0x3c;) {
		sn_pri(tdev, SN_ERR, "hdma: %s PF c=%d 0x%03x = 0x%x\n",
		       TO_STRING(hdma_info->dir), c, (u64)(start + i) & 0xfff,
		       hdma_read(tdev, start + i));
		i += 0x4;
		if (i == 0xc)
			i += 0x4;
	}

	for (i = 0x80; i <= 0xa8;) {
		sn_pri(tdev, SN_ERR, "hdma: %s PF c=%d 0x%03x = 0x%x\n",
		       TO_STRING(hdma_info->dir), c, (u64)(start + i) & 0xfff,
		       hdma_read(tdev, start + i));
		i += 0x4;
	}
	dump_hdma_configure(tdev, hdma_info->dir, c, SN_ERR, 0);
}

static int isChannelIdle(struct sn_tranx_t *tdev, int c, void __iomem *chn_off)
{
    u32 val;
    u32 retry_cnt = 20;

    //ensure channel is idle
    val = hdma_read(tdev, chn_off + HDMA_STATUS);
    if (val == CHN_BUSY) {
        sn_pri(tdev, SN_DBG, "hdma: %s new tx req but channel not idle, c:%d,status=0x%x. stopping channel...\n", __func__, c, val);
        /* stop channel */
        hdma_write(tdev, chn_off + HDMA_DOORBELL, HDMA_STOP);
        //confirm channel is now idle
        while (retry_cnt--) {
            val = hdma_read(tdev, chn_off + HDMA_STATUS);
            if (val > CHN_BUSY) {
                break;
            }
            usleep_range(100, 200);
        }
        if (!retry_cnt) {
            sn_pri(tdev, SN_ERR, "hdma: %s unable to stop channel before starting new tx, c:%d,status=0x%x\n", __func__, c, val);
            return 0;
        }
        sn_pri(tdev, SN_DBG, "hdma: %s channel stopped, c:%d,retries=%d\n", __func__, c, (20-retry_cnt));
    }
    return 1;
}


/*
 * transmission data from RC to EP by hdma link mode.
 *
 * @table_info: hdma link table.
 * @hdma_info: hdma transmission information.
 * @cnt: the count of link table elements.
 * @tdev: core struct, record driver info.
 * return value:
 *       0:success    -EFAULT:failed, -ERESTARTSYS:terminated.
 */
static int hdma_link_rc2ep_xfer(struct dma_link_table *table_info,
				struct trans_pcie_hdma *hdma_info, u32 cnt,
				struct sn_tranx_t *tdev, struct file *filp)
{
	u32 val;
	int ret, c, i;
	u8 done = 0;
    unsigned long timeout, interval, wakeup_count;
	/* hdma link table which are saved in ep side ddr */
	struct dma_link_table __iomem *link_table;
	struct dw_hdma_llp __iomem *hdma_llp;
	void __iomem *lt_end;
	void __iomem *chn_off;
	u32 *desc_sizes = NULL;
	u32 total_size = 0;

	/* link table physical address, EP pcie axi master view memory space */
	u64 table_paddr;
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];

	if (tdev->hw_err_flag)
		return -EFAULT;

	/* need to get a free channel and prepare the link table. */
	c = get_hdma_channel(RC2EP, thdma);
	if (c < 0) {
		if (c != -ERESTARTSYS) {
			sn_pri(tdev, SN_ERR, "hdma: %s get channel failed. %d\n", __func__, c);
		}
		return c;
	}
    //save the file pointer
    thdma->rc2ep[c].filp = filp;

    chn_off     = thdma->rc2ep[c].chn_off;
    link_table  = thdma->rc2ep[c].lt_vaddr;
    table_paddr = thdma->rc2ep[c].lt_paddr;

    ret = isChannelIdle(tdev, c, chn_off);
    if (!ret) {
        sn_pri(tdev, SN_ERR, "hdma: %s allocated rc2ep channel %d not idle\n", __func__, c);
        goto end;
    }

    /* copy link table to ep side ddr */
	memcpy(link_table, table_info, sizeof(struct dma_link_table) * cnt);
	link_table[cnt - 1].control |= LL_CB;

	/* Collect transfer statistics */
	desc_sizes = kvzalloc(cnt * sizeof(u32), GFP_KERNEL);
	if (desc_sizes) {
		for (i = 0; i < cnt; i++) {
			desc_sizes[i] = table_info[i].size;
			total_size += table_info[i].size;
		}
	}

	/* link table end description */
	hdma_llp = (struct dw_hdma_llp __iomem *)(&link_table[cnt]);
	hdma_llp->control = LL_LLP | LL_TCB;
	hdma_llp->reserved = 0;
	hdma_llp->llp_high = QWORD_HI(table_paddr);
	hdma_llp->llp_low = QWORD_LO(table_paddr);

	lt_end = hdma_llp + 1;
    if (check_link_table_done(tdev, c, RC2EP, lt_end, thdma->rc2ep[c].check_lt)) {
        ret = -EFAULT;
        goto end;
    }
	thdma->rc2ep[c].check_lt++;

	sn_pri(tdev, SN_DBG, "hdma: chn:%d element_size:%d direct:%s.\n", c,
	       cnt, TO_STRING(hdma_info->dir));

#if HDMA_DEBUG
	dump_link_table(link_table, cnt, tdev, 1, RC2EP, c, HDMA_DEBUG);
#endif

	/* if hdma is disable, enable it */
	val = hdma_read(tdev, chn_off + HDMA_EN);
	if (!(val & HDMA_ENABLE))
		hdma_write(tdev, chn_off + HDMA_EN, val | HDMA_ENABLE);

	/* clear all interrupt */
	val = STOP_CLEAR | ABORT_CLEAR;
	hdma_write(tdev, chn_off + HDMA_INT_CLEAR, val);

	if (tdev->pf_vf_mode == PF_MODE) {
		/* disbale VF channel and enable PF_0 */
		hdma_write(tdev, chn_off + HDMA_FUNC_NUM, 0);
	} else {
		/* Virtual Function Enable | Virtual Function Number, first is 0 */
		val = HDMA_VF_EN | ((tdev->vf_index - 1) << VF_NUM_OFF);
		hdma_write(tdev, chn_off + HDMA_FUNC_NUM, val);
	}

	/* set CYCLE_BIT and CYCLE_STATUS */
	hdma_write(tdev, chn_off + HDMA_CYCLE, CYCLE_BIT | CYCLE_STATE);

	hdma_write(tdev, chn_off + HDMA_WATERMARK_EN, 0x0);
	/* unmask all RC2EP and enable interrupt */
	val = RSIE | RAIE | LSIE | LAIE;
	hdma_write(tdev, chn_off + HDMA_INT_SETUP, val);

	/* enable link list mode */
	val = hdma_read(tdev, chn_off + HDMA_CONTROL1);
	hdma_write(tdev, chn_off + HDMA_CONTROL1, val | LINK_LIST_EN);

	/* set LLP */
	hdma_write(tdev, chn_off + HDMA_LLP_LOW, QWORD_LO(table_paddr));
	hdma_write(tdev, chn_off + HDMA_LLP_HIGH, QWORD_HI(table_paddr));

#if HDMA_DEBUG
	dump_regs(tdev, c, hdma_info, HDMA_DEBUG, 1);
#endif

    /* enable this channel */
    hdma_write(tdev, chn_off + HDMA_DOORBELL, HDMA_START);

    /* Mark channel as busy */
    thdma->rc2ep[c].chn_status = CHN_BUSY;

    timeout      = msecs_to_jiffies(HDMA_TIMEOUT_MS);          // 500ms total timeout
    interval     = msecs_to_jiffies(HDMA_TIMEOUT_INTERVAL_MS); // check every 50ms
    wakeup_count = timeout/interval;                           // number of retries

    do {
        /* wait for transfer to complete or timeout */
        ret = wait_event_interruptible_timeout(thdma->rc2ep[c].queue_wait,
                                               thdma->rc2ep[c].condition,
                                               interval);
        // condition met
        if (thdma->rc2ep[c].condition) {
            ret = 1;
            break;
        }
        // check for signal recvd
        if (ret < 0) {
            break;
        }
        //timeout expired, retry
        --wakeup_count;
        sn_pri(tdev, SN_DBG, "hdma: rc2ep[%d] wait_loop_count=%d  size=%d\n",
                              c, ((timeout/interval)-wakeup_count),hdma_info->size);
    } while (wakeup_count > 0);

    /* check for timeout */
    if (ret == 0) {
        u32 status, size;
        //check if transfer has completed successfully
        status = hdma_read(tdev, chn_off + HDMA_STATUS);
        size   = hdma_read(tdev, chn_off + HDMA_XFERSIZE);
        if ((status == 0x03) && (size == 0x0)) {
            //transfer completed successfully but no irq generated
            done = 1;
            atomic64_add(hdma_info->size, &thdma->hdma_perf.rc2ep_size);
            sn_pri(tdev, SN_DBG, "hdma: rc2ep[%d] wait timeout but tx complete, tx_size=%d\n",
                    c, hdma_info->size);
        } else {
            val = hdma_read(tdev, chn_off + HDMA_INT_STATUS);
            sn_pri(tdev, SN_ERR, "hdma: %s timeout: c:%d,status=0x%x,condition=%d,tx_size=%d\n",
                    __func__, c, val, thdma->rc2ep[c].condition,hdma_info->size);

            dump_link_table(link_table, cnt, tdev, 1, RC2EP, c, 1);
            dump_regs(tdev, c, hdma_info, 1, 1);
            /* stop channel on timeout */
            hdma_write(tdev, chn_off + HDMA_DOORBELL, HDMA_STOP);
            done = 0; //transfer failed
        }
    } else if (ret < 0) {
        /* stop channel if interrupted */
        hdma_write(tdev, chn_off + HDMA_DOORBELL, HDMA_STOP);
        sn_pri(tdev, SN_DBG, "hdma: rc2ep,wait terminated, c:%d\n", c);
    } else {
        /* check for abort condition */
        if (thdma->rc2ep[c].chn_err > 0) {
            sn_pri(tdev, SN_ERR, "hdma: %s transfer aborted c:%d, error code:%d\n", __func__, c, thdma->rc2ep[c].chn_err);
        } else {
            /* transfer completed successfully */
            done = 1;
            atomic64_add(hdma_info->size, &thdma->hdma_perf.rc2ep_size);
        }
    }

    /* Update transfer statistics if successful */
    if (done && desc_sizes) {
        spin_lock(&thdma->rc2ep[c].stats_lock);
        /* Free old descriptor sizes array if exists */
        if (thdma->rc2ep[c].last_transfer.descriptor_sizes)
            kvfree(thdma->rc2ep[c].last_transfer.descriptor_sizes);
        
        thdma->rc2ep[c].last_transfer.descriptor_count = cnt;
        thdma->rc2ep[c].last_transfer.total_size = total_size;
        thdma->rc2ep[c].last_transfer.descriptor_sizes = desc_sizes;
        thdma->rc2ep[c].last_transfer.timestamp = jiffies;
        desc_sizes = NULL; /* Ownership transferred */
        spin_unlock(&thdma->rc2ep[c].stats_lock);
    }

end:
    if (desc_sizes)
        kvfree(desc_sizes);
    free_hdma_channel(c, hdma_info->dir, thdma);

    if (ret == -ERESTARTSYS)
        return ret;
    return (done == 1) ? 0 : -EFAULT;
}

/*
 * transmission data from EP to RC by hdma link mode;
 *
 * @table_info: hdma link table.
 * @hdma_info: hdma transmission information.
 * @cnt: the count of link table elements.
 * @tdev: core struct, record driver info.
 * return value:
 *       0:success    -EFAULT:failed, -ERESTARTSYS:terminated.
 */
static int hdma_link_ep2rc_xfer(struct dma_link_table *table_info,
				struct trans_pcie_hdma *hdma_info, u32 cnt,
				struct sn_tranx_t *tdev, struct file *filp)
{
	u32 val;
	int ret, c, i;
	u8 done = 0;
    unsigned long timeout, interval, wakeup_count;
	/* hdma link table which are saved in ep side ddr */
	struct dma_link_table __iomem *link_table;
	struct dw_hdma_llp __iomem *hdma_llp;
	void __iomem *lt_end;
	void __iomem *chn_off;
	u32 *desc_sizes = NULL;
	u32 total_size = 0;

	/* link table physical address, ep pcie axi master view memory space */
	u64 table_paddr;
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];

	if (tdev->hw_err_flag)
		return -EFAULT;

	c = get_hdma_channel(EP2RC, thdma);
	if (c < 0) {
		if (c != -ERESTARTSYS) {
			sn_pri(tdev, SN_ERR, "hdma: %s get channel failed. %d\n", __func__, c);
		}
		return c;
	}
    //save the file pointer
    thdma->ep2rc[c].filp = filp;

    chn_off     = thdma->ep2rc[c].chn_off;
    link_table  = thdma->ep2rc[c].lt_vaddr;
    table_paddr = thdma->ep2rc[c].lt_paddr;

    ret = isChannelIdle(tdev, c, chn_off);
    if (!ret) {
        sn_pri(tdev, SN_ERR, "hdma: %s allocated ep2rc channel %d not idle\n", __func__, c);
        goto end;
    }

    /* copy link table to ep side ddr */
	memcpy(link_table, table_info, sizeof(struct dma_link_table) * cnt);
	link_table[cnt - 1].control |= LL_CB;

	/* Collect transfer statistics */
	desc_sizes = kvzalloc(cnt * sizeof(u32), GFP_KERNEL);
	if (desc_sizes) {
		for (i = 0; i < cnt; i++) {
			desc_sizes[i] = table_info[i].size;
			total_size += table_info[i].size;
		}
	}

	/* link table end description */
	hdma_llp = (struct dw_hdma_llp __iomem *)(&link_table[cnt]);
	hdma_llp->control = LL_LLP | LL_TCB;
	hdma_llp->reserved = 0;
	hdma_llp->llp_high = QWORD_HI(table_paddr);
	hdma_llp->llp_low = QWORD_LO(table_paddr);

	lt_end = hdma_llp + 1;
    if (check_link_table_done(tdev, c, EP2RC, lt_end, thdma->ep2rc[c].check_lt)) {
        ret = -EFAULT;
        goto end;
    }
	thdma->ep2rc[c].check_lt++;

	sn_pri(tdev, SN_DBG, "hdma: chn:%d element_size:%d dir:%s.\n", c, cnt,
	       TO_STRING(hdma_info->dir));
#if HDMA_DEBUG
	dump_link_table(link_table, cnt, tdev, 1, EP2RC, c, HDMA_DEBUG);
#endif

	/* if hdma is disable, enable it */
	val = hdma_read(tdev, chn_off + HDMA_EN);
	if (!(val & HDMA_ENABLE))
		hdma_write(tdev, chn_off + HDMA_EN, val | HDMA_ENABLE);

	/* clear all interrupt */
	val = STOP_CLEAR | ABORT_CLEAR;
	hdma_write(tdev, chn_off + HDMA_INT_CLEAR, val);

	if (tdev->pf_vf_mode == PF_MODE) {
		/* disbale VF channel and enable PF_0 */
		hdma_write(tdev, chn_off + HDMA_FUNC_NUM, 0);
	} else {
		/* Virtual Function Enable | Virtual Function Number, first is 0 */
		val = HDMA_VF_EN | ((tdev->vf_index - 1) << VF_NUM_OFF);
		hdma_write(tdev, chn_off + HDMA_FUNC_NUM, val);
	}

	/* set CYCLE_BIT and CYCLE_STATUS */
	hdma_write(tdev, chn_off + HDMA_CYCLE, CYCLE_BIT | CYCLE_STATE);

	hdma_write(tdev, chn_off + HDMA_WATERMARK_EN, 0x0);
	/* unmask all EP2RC and enable interrupt */
	val = RSIE | RAIE | LSIE | LAIE;
	hdma_write(tdev, chn_off + HDMA_INT_SETUP, val);

	/* enable link list mode */
	val = hdma_read(tdev, chn_off + HDMA_CONTROL1);
	hdma_write(tdev, chn_off + HDMA_CONTROL1, val | LINK_LIST_EN);

	/* set LLP */
	hdma_write(tdev, chn_off + HDMA_LLP_LOW, QWORD_LO(table_paddr));
	hdma_write(tdev, chn_off + HDMA_LLP_HIGH, QWORD_HI(table_paddr));

#if HDMA_DEBUG
	dump_regs(tdev, c, hdma_info, HDMA_DEBUG, 1);
#endif

    /* enable this channel */
    hdma_write(tdev, chn_off + HDMA_DOORBELL, HDMA_START);

    /* Mark channel as busy */
    thdma->ep2rc[c].chn_status = CHN_BUSY;

    timeout      = msecs_to_jiffies(HDMA_TIMEOUT_MS);          // 500ms total timeout
    interval     = msecs_to_jiffies(HDMA_TIMEOUT_INTERVAL_MS); // check every 50ms
    wakeup_count = timeout/interval;                           // number of retries

    do {
        /* wait for transfer to complete or timeout */
        ret = wait_event_interruptible_timeout(thdma->ep2rc[c].queue_wait,
                                               thdma->ep2rc[c].condition,
                                               interval);
        // condition met
        if (thdma->ep2rc[c].condition) {
            ret = 1;
            break;
        }
        // check for signal recvd
        if (ret < 0) {
            break;
        }
        //timeout expired, retry
        --wakeup_count;
        sn_pri(tdev, SN_DBG, "hdma: ep2rc[%d] wait_loop_count=%d size=%d\n",
                              c, ((timeout/interval)-wakeup_count), hdma_info->size);
    } while (wakeup_count > 0);

    /* check for timeout */
    if (ret == 0) {
        u32 status, size;
        //check if transfer has completed successfully
        status = hdma_read(tdev, chn_off + HDMA_STATUS);
        size   = hdma_read(tdev, chn_off + HDMA_XFERSIZE);
        if ((status == 0x03) && (size == 0x0)) {
            //transfer completed successfully but no irq generated
            done = 1;
            atomic64_add(hdma_info->size, &thdma->hdma_perf.ep2rc_size);
            sn_pri(tdev, SN_DBG, "hdma: ep2rc[%d] wait timeout but tx complete, tx_size=%d\n",
                    c, hdma_info->size);
        } else {
            val = hdma_read(tdev, chn_off + HDMA_INT_STATUS);
            sn_pri(tdev, SN_ERR, "hdma: %s timeout: c:%d,status=0x%x,condition=%d,tx_size=%d\n",
                    __func__, c, val, thdma->ep2rc[c].condition, hdma_info->size);

            dump_link_table(link_table, cnt, tdev, 1, EP2RC, c, 1);
            dump_regs(tdev, c, hdma_info, 1, 1);
            /* stop channel on timeout */
            hdma_write(tdev, chn_off + HDMA_DOORBELL, HDMA_STOP);
            done = 0; // transfer failed
        }
    } else if (ret < 0) {
        /* stop channel if interrupted */
        hdma_write(tdev, chn_off + HDMA_DOORBELL, HDMA_STOP);
        sn_pri(tdev, SN_DBG, "hdma: ep2rc,wait terminated, c:%d\n", c);
    } else {
        /* check for abort condition */
        if (thdma->ep2rc[c].chn_err > 0) {
            sn_pri(tdev, SN_ERR, "hdma: %s transfer aborted c:%d, error code:%d\n", __func__, c, thdma->ep2rc[c].chn_err);
        } else {
            /* transfer completed successfully */
            done = 1;
            atomic64_add(hdma_info->size, &thdma->hdma_perf.ep2rc_size);
        }
    }

    /* Update transfer statistics if successful */
    if (done && desc_sizes) {
        spin_lock(&thdma->ep2rc[c].stats_lock);
        /* Free old descriptor sizes array if exists */
        if (thdma->ep2rc[c].last_transfer.descriptor_sizes)
            kvfree(thdma->ep2rc[c].last_transfer.descriptor_sizes);
        
        thdma->ep2rc[c].last_transfer.descriptor_count = cnt;
        thdma->ep2rc[c].last_transfer.total_size = total_size;
        thdma->ep2rc[c].last_transfer.descriptor_sizes = desc_sizes;
        thdma->ep2rc[c].last_transfer.timestamp = jiffies;
        desc_sizes = NULL; /* Ownership transferred */
        spin_unlock(&thdma->ep2rc[c].stats_lock);
    }

end:
    if (desc_sizes)
        kvfree(desc_sizes);
    free_hdma_channel(c, hdma_info->dir, thdma);

    if (ret == -ERESTARTSYS)
        return ret;
    return (done == 1) ? 0 : -EFAULT;
}

static inline unsigned int count_pages(unsigned long iov_base, size_t iov_len)
{
	unsigned long first = (iov_base & PAGE_MASK) >> PAGE_SHIFT;
	unsigned long last =
		((iov_base + iov_len - 1) & PAGE_MASK) >> PAGE_SHIFT;

	return last - first + 1;
}

/*
 * get the page information of a virtual address, then translet to physical
 * address array.
 */
static int get_dma_addr(struct sn_tranx_t *tdev, unsigned long start, u32 len,
			struct rc_addr_info *paddr_array, struct page **user_pages, struct sg_table *sgt)
{
	u32 page_cnt;
	long rv = -EFAULT;
	int i, sg_cnt;
	struct scatterlist *sg;
	unsigned int flags = FOLL_WRITE;

	if (!len) {
		sn_pri(tdev, SN_ERR, "hdma: get_dma_addr invalid len\n");
		return -EFAULT;
    }

	page_cnt = count_pages(start, len);
	rv = pin_user_pages_fast(start, page_cnt, flags, user_pages);
	if (rv != page_cnt) {
		sn_pri(tdev, SN_ERR, "hdma: pin_user_pages failed, request %ld, actual %ld\n", page_cnt, rv);
		rv = -ERESTARTSYS;
		goto exit;
	}

	rv = sg_alloc_table_from_pages(sgt, user_pages, page_cnt,
				       start & (PAGE_SIZE - 1), len,
				       GFP_KERNEL);
	if (rv) {
		sn_pri(tdev, SN_ERR, "hdma: alloc sg_table failed:%ld\n", rv);
		rv = -EFAULT;
		goto exit;
	}

	sg_cnt = dma_map_sg(&tdev->pdev->dev, sgt->sgl, sgt->nents, 0);
	if (sg_cnt <= 0) {
		rv = -EFAULT;
		sn_pri(tdev, SN_ERR, "hdma: dma_map_sg failed:%d\n", sg_cnt);
		goto exit;
	}

	for_each_sg (sgt->sgl, sg, sg_cnt, i) {
		paddr_array[i].paddr = sg_dma_address(sg);
		paddr_array[i].size = sg_dma_len(sg);
		sn_pri(tdev, SN_DBG, "hdma: vaddr = 0x%x,SG_paddr= 0x%x\n", start, paddr_array[i].paddr);
	}
	rv = sg_cnt;

exit:
	return rv;
}

/*
 * the RC address is virtual, this function will get its physical address,
 * generate link table, use hdma link mode to transmit.
 * return value:
 *     0:success  -EFAULT:failed  -ERESTARTSYS:terminated/try-again
 */
int hdma_tranx_viraddr_mode(struct trans_pcie_hdma *hdma_info,
				   struct sn_tranx_t *tdev, struct file *filp)
{
	int j, page_cnt;
	struct dma_link_table *link_table;
	unsigned int ctl = LL_CB;
	u64 vaddr, ep_addr;
	struct rc_addr_info *paddr_array;
	struct page **user_pages;
	struct sg_table sgt;
	int rv = 0;

	if (hdma_info->size == 0) {
		sn_pri(tdev, SN_ERR, "hdma: tranx size is 0, return error\n");
		return -EFAULT;
	}

	if (hdma_info->dir == RC2EP) {
		vaddr = hdma_info->sar;
		ep_addr = hdma_info->dar;
	} else if (hdma_info->dir == EP2RC) {
		ep_addr = hdma_info->sar;
		vaddr = hdma_info->dar;
	} else {
		sn_pri(tdev, SN_ERR, "hdma: %s hdma dir=0x%x error.\n",
		       __func__, hdma_info->dir);
		return -EFAULT;
	}

	if (hdma_info->fd)
		ep_addr = sn_mem_osal_translate_handle(tdev, filp, ep_addr, hdma_info->size);
	else
		ep_addr = sn_mem_osal_translate_handle(tdev, NULL, ep_addr, hdma_info->size);
	if (ep_addr == 1) // invalid
	  return -EFAULT;
	page_cnt = count_pages(vaddr, hdma_info->size);
	paddr_array = kvzalloc(sizeof(*paddr_array) * page_cnt, GFP_KERNEL);
	if (!paddr_array) {
		sn_pri(tdev, SN_ERR, "hdma: allocate paddr_array failed\n");
		return -EFAULT;
	}

	user_pages = kvzalloc(sizeof(struct page *) * page_cnt, GFP_KERNEL);
	if (!user_pages) {
		sn_pri(tdev, SN_ERR, "hdma: allocate user page failed\n");
        rv = -ENOMEM;
		goto out_free_paddr_array;
	}

	rv = get_dma_addr(tdev, vaddr, hdma_info->size, paddr_array, user_pages, &sgt);
	if (rv < 0) {
		sn_pri(tdev, SN_ERR, "hdma: %s, get_dma_addr failed, vaddr=%llx, size=%d\n",
		       __func__, vaddr, hdma_info->size);
		goto out_free_user_pages;
	}

	link_table = kvzalloc(rv * sizeof(*link_table), GFP_KERNEL);
	if (!link_table) {
		sn_pri(tdev, SN_ERR, "hdma: allocate link table failed.\n");
        rv = -ENOMEM;
		goto out_free_sg_table;
	}

	/* create link table */
	for (j = 0; j < rv; j++) {
		link_table[j].control = ctl;
		link_table[j].size = paddr_array[j].size;
		if (hdma_info->dir == RC2EP) {
			link_table[j].sar_high = QWORD_HI(paddr_array[j].paddr);
			link_table[j].sar_low = QWORD_LO(paddr_array[j].paddr);
			link_table[j].dst_high = QWORD_HI(ep_addr);
			link_table[j].dst_low = QWORD_LO(ep_addr);
		} else {
			link_table[j].dst_high = QWORD_HI(paddr_array[j].paddr);
			link_table[j].dst_low = QWORD_LO(paddr_array[j].paddr);
			link_table[j].sar_high = QWORD_HI(ep_addr);
			link_table[j].sar_low = QWORD_LO(ep_addr);
		}
		ep_addr += paddr_array[j].size;
	}

	if (hdma_info->dir == RC2EP)
		rv = hdma_link_rc2ep_xfer(link_table, hdma_info, j, tdev, filp);
	else
		rv = hdma_link_ep2rc_xfer(link_table, hdma_info, j, tdev, filp);

	kvfree(link_table);
out_free_sg_table:
	dma_unmap_sg(&tdev->pdev->dev, sgt.sgl, sgt.nents, 0);
	sg_free_table(&sgt);
out_free_user_pages:
    unpin_user_pages(user_pages, page_cnt);
	kvfree(user_pages);
out_free_paddr_array:
	kvfree(paddr_array);

	return rv;
}

static void pcie_bw_timer_isr(struct timer_list *t)
{
	struct hdma_t *thdma = from_timer(thdma, t, perf_timer);

	mod_timer(&thdma->perf_timer, jiffies + PCIE_BW_TIMER);
	thdma->hdma_perf.ep2rc_per =
		atomic64_read(&thdma->hdma_perf.ep2rc_size) / 1024;
	thdma->hdma_perf.rc2ep_per =
		atomic64_read(&thdma->hdma_perf.rc2ep_size) / 1024;
	atomic64_set(&thdma->hdma_perf.ep2rc_size, 0);
	atomic64_set(&thdma->hdma_perf.rc2ep_size, 0);
}

static irqreturn_t __attribute__((no_sanitize("undefined")))hdma_isr(int irq, void *data)
{
	u32 rc2ep_rec, ep2rc_rec;
	int c;
	struct sn_tranx_t *tdev = data;
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];

	/* check/clear interrupt and record interrupt channel */
	for (c = 0; c < HDMA_CH_CNT; c++) {
        if (thdma->rc2ep[c].chn_status == CHN_BUSY) {
            rc2ep_rec = hdma_read(tdev, thdma->rc2ep[c].chn_off + HDMA_INT_STATUS);
            if (likely(rc2ep_rec & STOP)) {
               thdma->rc2ep[c].condition = 0x1;
                thdma->rc2ep[c].chn_err   = 0;
                rc2ep_rec &= STOP_CLEAR;
                hdma_write(tdev, thdma->rc2ep[c].chn_off + HDMA_INT_CLEAR, rc2ep_rec);
                wake_up_interruptible_all(&thdma->rc2ep[c].queue_wait);
            } else if (rc2ep_rec & ABORT) {
               thdma->rc2ep[c].condition = 0x1;
               thdma->rc2ep[c].chn_err   = rc2ep_rec>>3;
                rc2ep_rec &= ABORT_CLEAR;
                hdma_write(tdev, thdma->rc2ep[c].chn_off + HDMA_INT_CLEAR, rc2ep_rec);
                wake_up_interruptible_all(&thdma->rc2ep[c].queue_wait);
                sn_pri(tdev, SN_ERR, "hdma: rc2ep channel %d aborted\n", c);
            }
        }

        if (thdma->ep2rc[c].chn_status == CHN_BUSY) {
            ep2rc_rec = hdma_read(tdev, thdma->ep2rc[c].chn_off + HDMA_INT_STATUS);
            if (likely(ep2rc_rec & STOP)) {
                thdma->ep2rc[c].condition = 0x1;
                thdma->ep2rc[c].chn_err   = 0;
                ep2rc_rec &= STOP_CLEAR;
                hdma_write(tdev, thdma->ep2rc[c].chn_off + HDMA_INT_CLEAR, ep2rc_rec);
                wake_up_interruptible_all(&thdma->ep2rc[c].queue_wait);
            } else if (ep2rc_rec & ABORT) {
                thdma->ep2rc[c].condition = 0x1;
                thdma->ep2rc[c].chn_err   = ep2rc_rec>>3;
                ep2rc_rec &= ABORT_CLEAR;
                hdma_write(tdev, thdma->ep2rc[c].chn_off + HDMA_INT_CLEAR, ep2rc_rec);
                wake_up_interruptible_all(&thdma->ep2rc[c].queue_wait);
                sn_pri(tdev, SN_ERR, "hdma: ep2rc channel %d aborted\n", c);
            }
        }
    }
    return IRQ_HANDLED;
}


/* setting hdma msi interrupt table */
static void hdma_msi_config(struct sn_tranx_t *tdev, u64 msi_addr, u32 msi_data)
{
	int c;
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];
	void __iomem *chn_off;
	u32 msi_addr_l, msi_addr_h;

	msi_addr_l = QWORD_LO(msi_addr);
	msi_addr_h = QWORD_HI(msi_addr);
	/* ep2rc Stop/watermark/abort Interrupt MWr TLP address and data */
	for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
		chn_off = thdma->ep2rc[c].chn_off;
		hdma_write(tdev, chn_off + HDMA_MSI_STOP_LOW, msi_addr_l);
		hdma_write(tdev, chn_off + HDMA_MSI_STOP_HIGH, msi_addr_h);
		hdma_write(tdev, chn_off + HDMA_MSI_WATERMARK_LOW, msi_addr_l);
		hdma_write(tdev, chn_off + HDMA_MSI_WATERMARK_HIGH, msi_addr_h);
		hdma_write(tdev, chn_off + HDMA_MSI_ABORT_LOW, msi_addr_l);
		hdma_write(tdev, chn_off + HDMA_MSI_ABORT_HIGH, msi_addr_h);
		hdma_write(tdev, chn_off + HDMA_MSI_MSGD, msi_data & 0xFFFF);
	}
	/* rc2ep Stop/watermark/abort Interrupt MWr TLP address and data */
	for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
		chn_off = thdma->rc2ep[c].chn_off;
		hdma_write(tdev, chn_off + HDMA_MSI_STOP_LOW, msi_addr_l);
		hdma_write(tdev, chn_off + HDMA_MSI_STOP_HIGH, msi_addr_h);
		hdma_write(tdev, chn_off + HDMA_MSI_WATERMARK_LOW, msi_addr_l);
		hdma_write(tdev, chn_off + HDMA_MSI_WATERMARK_HIGH, msi_addr_h);
		hdma_write(tdev, chn_off + HDMA_MSI_ABORT_LOW, msi_addr_l);
		hdma_write(tdev, chn_off + HDMA_MSI_ABORT_HIGH, msi_addr_h);
		hdma_write(tdev, chn_off + HDMA_MSI_MSGD, msi_data & 0xFFFF);
	}
}

static ssize_t dump_hdma_regs_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];
	int dir, c, i;
	char dir_name[10];
	void __iomem *start;

	if (sscanf(buf, "%s %d", dir_name, &c) != 2) {
		sn_pri(tdev, SN_ERR, "hdma: not in hex or decimal form.\n");
		return -EFAULT;
	}
	if (strcmp(dir_name, "rc2ep") == 0)
		dir = RC2EP;
	else if (strcmp(dir_name, "ep2rc") == 0)
		dir = EP2RC;
	else {
		sn_pri(tdev, SN_ERR, "hdma: dir name error.\n");
		return -EFAULT;
	}
	if ((c < 0) || (c >= HDMA_CH_CNT)) {
		sn_pri(tdev, SN_ERR, "hdma: channel:%d error.\n", c);
		return -EFAULT;
	}

	if (dir == RC2EP) {
		start = thdma->rc2ep[c].chn_off;
	} else if (dir == EP2RC) {
		start = thdma->ep2rc[c].chn_off;
	} else {
		sn_pri(tdev, SN_ERR, "hdma: %s dir=0x%x error\n", dir);
		return -EFAULT;
	}

	for (i = 0; i <= 0x3c;) {
		sn_pri(tdev, SN_ERR, "hdma: %s c=%d 0x%03x = 0x%x\n",
		       TO_STRING(dir), c, (u64)(start + i) & 0xfff,
		       hdma_read(tdev, start + i));
		i += 0x4;
		if (i == 0xc)
			i += 0x4;
	}

	for (i = 0x80; i <= 0xa8;) {
		sn_pri(tdev, SN_ERR, "hdma: %s c=%d 0x%03x = 0x%x\n",
		       TO_STRING(dir), c, (u64)(start + i) & 0xfff,
		       hdma_read(tdev, start + i));
		i += 0x4;
	}

	return count;
}

/* display pcie tranx bandwidth, it is a probable value, EP2RC */
static ssize_t pcie_ep2rc_bw_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];

	return sprintf(buf, "%d\n", thdma->hdma_perf.ep2rc_per);
}

/* display pcie tranx bandwidth, it is a probable value, RC2EP */
static ssize_t pcie_rc2ep_bw_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];

	return sprintf(buf, "%d\n", thdma->hdma_perf.rc2ep_per);
}

/* display pcie hdma used information, eveyr channel used count */
static ssize_t hdma_used_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];
	int c;
	int pos = 0;

	pos += sprintf(buf + pos, "       ");
	for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
		pos += sprintf(buf + pos, "%11d", c);
	}
	pos += sprintf(buf + pos, "\n");

	pos += sprintf(buf + pos, "rc2ep: ");
	for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
		pos += sprintf(buf + pos, "%11d", thdma->rc2ep[c].used_cnt);
	}
	pos += sprintf(buf + pos, "\n");

	pos += sprintf(buf + pos, "ep2rc: ");
	for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
		pos += sprintf(buf + pos, "%11d", thdma->ep2rc[c].used_cnt);
	}

	pos += sprintf(buf + pos, "\n");

	return pos;
}

/* display hdma transfer statistics for all channels */
static ssize_t hdma_transfer_stats_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];
	int c, i;
	int pos = 0;
	unsigned long flags;

	pos += sprintf(buf + pos, "HDMA Transfer Statistics\n");
	pos += sprintf(buf + pos, "========================\n\n");

	/* RC2EP channels */
	pos += sprintf(buf + pos, "RC2EP Channels:\n");
	for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
		spin_lock_irqsave(&thdma->rc2ep[c].stats_lock, flags);
		if (thdma->rc2ep[c].last_transfer.descriptor_count > 0) {
			pos += sprintf(buf + pos, "  Channel %d:\n", c);
			pos += sprintf(buf + pos, "    Descriptors: %u\n", 
				      thdma->rc2ep[c].last_transfer.descriptor_count);
			pos += sprintf(buf + pos, "    Total Size: %u bytes\n",
				      thdma->rc2ep[c].last_transfer.total_size);
			pos += sprintf(buf + pos, "    Timestamp: %llu jiffies\n",
				      thdma->rc2ep[c].last_transfer.timestamp);
			if (thdma->rc2ep[c].last_transfer.descriptor_sizes) {
				pos += sprintf(buf + pos, "    Descriptor Sizes: [");
				for (i = 0; i < thdma->rc2ep[c].last_transfer.descriptor_count && i < 10; i++) {
					if (i > 0)
						pos += sprintf(buf + pos, ", ");
					pos += sprintf(buf + pos, "%u",
						      thdma->rc2ep[c].last_transfer.descriptor_sizes[i]);
				}
				if (thdma->rc2ep[c].last_transfer.descriptor_count > 10)
					pos += sprintf(buf + pos, ", ...");
				pos += sprintf(buf + pos, "]\n");
			}
		}
		spin_unlock_irqrestore(&thdma->rc2ep[c].stats_lock, flags);
	}

	/* EP2RC channels */
	pos += sprintf(buf + pos, "\nEP2RC Channels:\n");
	for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
		spin_lock_irqsave(&thdma->ep2rc[c].stats_lock, flags);
		if (thdma->ep2rc[c].last_transfer.descriptor_count > 0) {
			pos += sprintf(buf + pos, "  Channel %d:\n", c);
			pos += sprintf(buf + pos, "    Descriptors: %u\n",
				      thdma->ep2rc[c].last_transfer.descriptor_count);
			pos += sprintf(buf + pos, "    Total Size: %u bytes\n",
				      thdma->ep2rc[c].last_transfer.total_size);
			pos += sprintf(buf + pos, "    Timestamp: %llu jiffies\n",
				      thdma->ep2rc[c].last_transfer.timestamp);
			if (thdma->ep2rc[c].last_transfer.descriptor_sizes) {
				pos += sprintf(buf + pos, "    Descriptor Sizes: [");
				for (i = 0; i < thdma->ep2rc[c].last_transfer.descriptor_count && i < 10; i++) {
					if (i > 0)
						pos += sprintf(buf + pos, ", ");
					pos += sprintf(buf + pos, "%u",
						      thdma->ep2rc[c].last_transfer.descriptor_sizes[i]);
				}
				if (thdma->ep2rc[c].last_transfer.descriptor_count > 10)
					pos += sprintf(buf + pos, ", ...");
				pos += sprintf(buf + pos, "]\n");
			}
		}
		spin_unlock_irqrestore(&thdma->ep2rc[c].stats_lock, flags);
	}

	return pos;
}

/* display detailed descriptor information for all channels */
static ssize_t hdma_descriptor_details_show(struct device *dev,
					    struct device_attribute *attr, char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];
	int c, i;
	int pos = 0;
	unsigned long flags;

	pos += sprintf(buf + pos, "HDMA Descriptor Details\n");
	pos += sprintf(buf + pos, "=======================\n\n");

	/* RC2EP channels */
	for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
		spin_lock_irqsave(&thdma->rc2ep[c].stats_lock, flags);
		if (thdma->rc2ep[c].last_transfer.descriptor_count > 0 &&
		    thdma->rc2ep[c].last_transfer.descriptor_sizes) {
			pos += sprintf(buf + pos, "RC2EP Channel %d (Last Transfer):\n", c);
			pos += sprintf(buf + pos, "  Total Descriptors: %u\n",
				      thdma->rc2ep[c].last_transfer.descriptor_count);
			pos += sprintf(buf + pos, "  Total Size: %u bytes\n",
				      thdma->rc2ep[c].last_transfer.total_size);
			for (i = 0; i < thdma->rc2ep[c].last_transfer.descriptor_count; i++) {
				pos += sprintf(buf + pos, "    Descriptor[%d]: %u bytes\n",
					      i, thdma->rc2ep[c].last_transfer.descriptor_sizes[i]);
			}
			pos += sprintf(buf + pos, "\n");
		}
		spin_unlock_irqrestore(&thdma->rc2ep[c].stats_lock, flags);
	}

	/* EP2RC channels */
	for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
		spin_lock_irqsave(&thdma->ep2rc[c].stats_lock, flags);
		if (thdma->ep2rc[c].last_transfer.descriptor_count > 0 &&
		    thdma->ep2rc[c].last_transfer.descriptor_sizes) {
			pos += sprintf(buf + pos, "EP2RC Channel %d (Last Transfer):\n", c);
			pos += sprintf(buf + pos, "  Total Descriptors: %u\n",
				      thdma->ep2rc[c].last_transfer.descriptor_count);
			pos += sprintf(buf + pos, "  Total Size: %u bytes\n",
				      thdma->ep2rc[c].last_transfer.total_size);
			for (i = 0; i < thdma->ep2rc[c].last_transfer.descriptor_count; i++) {
				pos += sprintf(buf + pos, "    Descriptor[%d]: %u bytes\n",
					      i, thdma->ep2rc[c].last_transfer.descriptor_sizes[i]);
			}
			pos += sprintf(buf + pos, "\n");
		}
		spin_unlock_irqrestore(&thdma->ep2rc[c].stats_lock, flags);
	}

	return pos;
}

/* clear all HDMA transfer statistics */
static ssize_t hdma_clear_stats_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];
	int c;
	unsigned long flags;

	/* Clear RC2EP channel statistics */
	for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
		spin_lock_irqsave(&thdma->rc2ep[c].stats_lock, flags);
		if (thdma->rc2ep[c].last_transfer.descriptor_sizes) {
			kvfree(thdma->rc2ep[c].last_transfer.descriptor_sizes);
			thdma->rc2ep[c].last_transfer.descriptor_sizes = NULL;
		}
		thdma->rc2ep[c].last_transfer.descriptor_count = 0;
		thdma->rc2ep[c].last_transfer.total_size = 0;
		thdma->rc2ep[c].last_transfer.timestamp = 0;
		spin_unlock_irqrestore(&thdma->rc2ep[c].stats_lock, flags);
	}

	/* Clear EP2RC channel statistics */
	for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
		spin_lock_irqsave(&thdma->ep2rc[c].stats_lock, flags);
		if (thdma->ep2rc[c].last_transfer.descriptor_sizes) {
			kvfree(thdma->ep2rc[c].last_transfer.descriptor_sizes);
			thdma->ep2rc[c].last_transfer.descriptor_sizes = NULL;
		}
		thdma->ep2rc[c].last_transfer.descriptor_count = 0;
		thdma->ep2rc[c].last_transfer.total_size = 0;
		thdma->ep2rc[c].last_transfer.timestamp = 0;
		spin_unlock_irqrestore(&thdma->ep2rc[c].stats_lock, flags);
	}

	sn_pri(tdev, SN_DBG, "hdma: cleared all transfer statistics\n");
	return count;
}

static DEVICE_ATTR_WO(dump_hdma_regs);
static DEVICE_ATTR_RO(pcie_ep2rc_bw);
static DEVICE_ATTR_RO(pcie_rc2ep_bw);
static DEVICE_ATTR_RO(hdma_used);
static DEVICE_ATTR_RO(hdma_transfer_stats);
static DEVICE_ATTR_RO(hdma_descriptor_details);
static DEVICE_ATTR_WO(hdma_clear_stats);
static struct attribute *trans_hdma_sysfs_entries[] = {
	&dev_attr_dump_hdma_regs.attr, &dev_attr_pcie_ep2rc_bw.attr,
	&dev_attr_pcie_rc2ep_bw.attr, &dev_attr_hdma_used.attr,
	&dev_attr_hdma_transfer_stats.attr, &dev_attr_hdma_descriptor_details.attr,
	&dev_attr_hdma_clear_stats.attr, NULL
};

static struct attribute_group trans_hdma_attribute_group = {
	.name = NULL,
	.attrs = trans_hdma_sysfs_entries,
};

static int hdma_register_irq(struct sn_tranx_t *tdev)
{
	int ret;

	/* request hdma irq. */
	ret = request_irq(tdev->msix_entries[HDMA_INT_IDX].vector, hdma_isr,
			  IRQF_SHARED | IRQF_NO_THREAD, "thdma", tdev);
	if (ret)
		sn_pri(tdev, SN_ERR, "hdma: request hdma irq failed.\n");
	return ret;
}

static void hdma_free_irq(struct sn_tranx_t *tdev)
{
	if (tdev->msix_entries[HDMA_INT_IDX].vector)
		free_irq(tdev->msix_entries[HDMA_INT_IDX].vector, tdev);
}

static void hdma_hw_init(struct sn_tranx_t *tdev)
{
	u32 val, c;
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];

   	// Initialize the bitmaps to zero
    bitmap_zero(thdma->rc2ep_bitmap, HDMA_CH_CNT);
    bitmap_zero(thdma->ep2rc_bitmap, HDMA_CH_CNT);

	/* config EP2RC */
	for (c = 0; c < HDMA_CH_CNT; c++) {
		/* disable all EP2RC channel */
		hdma_write(tdev, thdma->ep2rc[c].chn_off + HDMA_EN, HDMA_DISABLE);

		/* mask all EP2RC and disable interrupt */
		val = STOP_MASK | WATERMARK_MASK | ABORT_MASK;
		hdma_write(tdev, thdma->ep2rc[c].chn_off + HDMA_INT_SETUP, val);

		/* clear all EP2RC interrupt */
		val = STOP_CLEAR | ABORT_CLEAR;
		hdma_write(tdev, thdma->ep2rc[c].chn_off + HDMA_INT_CLEAR, val);

		/* unmask all EP2RC disable interrupt */
		hdma_write(tdev, thdma->ep2rc[c].chn_off + HDMA_INT_SETUP, 0);

		/* enable all EP2RC channel */
		hdma_write(tdev, thdma->ep2rc[c].chn_off + HDMA_EN, HDMA_ENABLE);
	}

	/* config RC2EP */
	for (c = 0; c < HDMA_CH_CNT; c++) {
		/* disable all RC2EP channel */
		hdma_write(tdev, thdma->rc2ep[c].chn_off + HDMA_EN, HDMA_DISABLE);

		/* mask all RC2EP and disable interrupt */
		val = STOP_MASK | WATERMARK_MASK | ABORT_MASK;
		hdma_write(tdev, thdma->rc2ep[c].chn_off + HDMA_INT_SETUP, val);

		/* clear all RC2EP interrupt */
		val = STOP_CLEAR | ABORT_CLEAR;
		hdma_write(tdev, thdma->rc2ep[c].chn_off + HDMA_INT_CLEAR, val);

		/* unmask all RC2EP disable interrupt */
		hdma_write(tdev, thdma->rc2ep[c].chn_off + HDMA_INT_SETUP, 0);

		/* enable all RC2EP channel */
		hdma_write(tdev, thdma->rc2ep[c].chn_off + HDMA_EN, HDMA_ENABLE);
	}
}

int hdma_cfg_interrupt(struct sn_tranx_t *tdev)
{
	u32 table, msix_bar, msix_offset, msix_size;
	u32 msi_addr_l, msi_addr_h, msi_data;
//	u32 data[3] = {0};
	u64 msi_addr;
	u16 flags;
	u8 cap_off;
	int ret = -EFAULT;
	struct pci_dev *pdev = tdev->pdev;

	if (pdev->msix_cap && pdev->msix_enabled) {
		pci_read_config_word(pdev, pdev->msix_cap + PCI_MSIX_FLAGS,
				     &flags);
		pci_read_config_dword(pdev, pdev->msix_cap + PCI_MSIX_TABLE,
				      &table);

		msix_bar = table & PCI_MSIX_TABLE_BIR;
		msix_offset = table & PCI_MSIX_TABLE_OFFSET;
		msix_size = ((flags & PCI_MSIX_FLAGS_QSIZE) + 1) * 16;
		sn_pri(tdev, SN_DBG,
		       "hdma: %s msix: bar=%d offset=0x%x size=0x%x\n",
		       __func__, msix_bar, msix_offset, msix_size);

		if (msix_bar == 0) {
			msi_addr_l = sn_rd_b0(tdev, msix_offset);
			msi_addr_h = sn_rd_b0(tdev, msix_offset + 0x4);
			msi_data   = sn_rd_b0(tdev, msix_offset + 0x8);

		} else {
			sn_pri(tdev, SN_ERR,
			       "hdma: msix table should be in bar0\n");
			goto out;
		}
		msi_addr = msi_addr_h;
		msi_addr <<= 32;
		msi_addr |= msi_addr_l;

		sn_pri(tdev, SN_DBG,
		       "hdma: %s msix_addr=0x%lx, msix_data=0x%x\n", __func__,
		       msi_addr, msi_data);
		hdma_msi_config(tdev, msi_addr, msi_data);
	} else if (pdev->msi_cap && pdev->msi_enabled) {
		cap_off = pdev->msi_cap + PCI_MSI_FLAGS;
		pci_read_config_word(pdev, cap_off, &flags);
		if (flags & PCI_MSI_FLAGS_ENABLE) {
			cap_off = pdev->msi_cap + PCI_MSI_ADDRESS_LO;
			pci_read_config_dword(pdev, cap_off, &msi_addr_l);

			if (flags & PCI_MSI_FLAGS_64BIT) {
				cap_off = pdev->msi_cap + PCI_MSI_ADDRESS_HI;
				pci_read_config_dword(pdev, cap_off,
						      &msi_addr_h);
				cap_off = pdev->msi_cap + PCI_MSI_DATA_64;
			} else {
				msi_addr_h = 0;
				cap_off = pdev->msi_cap + PCI_MSI_DATA_32;
			}

			msi_addr = msi_addr_h;
			msi_addr <<= 32;
			msi_addr |= msi_addr_l;

			pci_read_config_dword(pdev, cap_off, &msi_data);
			msi_data &= 0xffff;

			sn_pri(tdev, SN_DBG,
			       "hdma: %s msi_addr=0x%lx, msi_data=0x%x\n",
			       __func__, msi_addr, msi_data);
			hdma_msi_config(tdev, msi_addr, msi_data);
		} else {
			sn_pri(tdev, SN_DBG, "hdma: msi alse is disable\n");
			goto out;
		}
	} else {
		sn_pri(tdev, SN_ERR, "hdma: msi and msix both are disable\n");
		goto out;
	}

	ret = 0;
out:
	return ret;
}

int sn_hdma_init(struct sn_tranx_t *tdev)
{
	struct hdma_t *thdma;
	struct pci_dev *pdev = tdev->pdev;
	int ret, c;
	u64 link_table = 0;
	void __iomem *hdma_base = NULL;

	thdma = kzalloc(sizeof(struct hdma_t), GFP_KERNEL);
	if (!thdma) {
		sn_pri(tdev, SN_ERR, "hdma: kzalloc thdma failed.\n");
		return -ENOMEM;
	}
	tdev->modules[SN_MODULE_HDMA] = thdma;
	thdma->tdev = tdev;

	if (tdev->pf_vf_mode == PF_MODE) {
		link_table = PF_ELTA;
		thdma->total_chn = HDMA_CH_CNT;
		thdma->start_chn = 0;
		hdma_base = tdev->bar0_virt + PF_HDMA_BASE_OFF;
		/* there are thdma->total_chn link tables which are saved in ddr. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
		thdma->vhdma_lt = ioremap(pci_resource_start(pdev, 4) + ELTA_OFFSET,
							HDMA_LT_SIZE * thdma->total_chn * 2);
#else
		thdma->vhdma_lt = ioremap_nocache(pci_resource_start(pdev, 4) + ELTA_OFFSET,
						HDMA_LT_SIZE * thdma->total_chn * 2);
#endif
	} else if (tdev->pf_vf_mode == ONE_VF_MODE) {
		link_table = VF_ELTA;
		thdma->total_chn = HDMA_CH_CNT;
		thdma->start_chn = 0;
		hdma_base = tdev->bar2_virt + ONE_VF_HDMA_BASE_OFF;
		/* there are thdma->total_chn link tables which are saved in ddr. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
		thdma->vhdma_lt = ioremap(pci_resource_start(pdev, 4) + ELTA_OFFSET,
						HDMA_LT_SIZE * thdma->total_chn * 2);
#else
		thdma->vhdma_lt = ioremap_nocache(pci_resource_start(pdev, 4) + ELTA_OFFSET,
						HDMA_LT_SIZE * thdma->total_chn * 2);
#endif
	} else if (tdev->pf_vf_mode == TWO_VF_MODE) { /* 2 VF mode */
		if (tdev->vf_index == 1) {
			link_table = VF1_ELTA;
			thdma->total_chn = HDMA_CH_CNT/2;
			thdma->start_chn = 0;
			hdma_base = tdev->bar2_virt + VF_HDMA_BASE_OFF;
			/* there are thdma->total_chn link tables which are saved in ddr. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
			thdma->vhdma_lt = ioremap(pci_resource_start(pdev, 4) + ELTA_OFFSET,
						HDMA_LT_SIZE * thdma->total_chn * 2);
#else
			thdma->vhdma_lt = ioremap_nocache(pci_resource_start(pdev, 4) + ELTA_OFFSET,
						HDMA_LT_SIZE * thdma->total_chn * 2);
#endif
		} else if (tdev->vf_index == 2) {
			link_table = VF2_ELTA;
			thdma->total_chn = HDMA_CH_CNT/2;
			thdma->start_chn = HDMA_CH_CNT/2;
			hdma_base = tdev->bar2_virt + VF_HDMA_BASE_OFF;
			/* there are thdma->total_chn link tables which are saved in ddr. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
			thdma->vhdma_lt = ioremap(pci_resource_start(pdev, 4),
						HDMA_LT_SIZE * thdma->total_chn * 4);
#else
			thdma->vhdma_lt = ioremap_nocache(pci_resource_start(pdev, 4),
						HDMA_LT_SIZE * thdma->total_chn * 4);
#endif
		} else {
			sn_pri(tdev, SN_ERR, "hdma: sriov index:%d error\n",
			       tdev->vf_index);
			goto out_free_hdma;
		}
	}

	if (!thdma->vhdma_lt) {
		sn_pri(tdev, SN_ERR, "hdma: map hdma link table failed.\n");
		goto out_free_hdma;
	}

	thdma->end_chn = thdma->start_chn + thdma->total_chn - 1;
	for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
		thdma->rc2ep[c].chn_off = hdma_base + HDMA_RDCH_OFF(c);
		thdma->rc2ep[c].lt_vaddr = thdma->vhdma_lt + LT_OFF(c);
		thdma->rc2ep[c].lt_paddr = link_table + LT_OFF(c);
	}
	for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
		thdma->ep2rc[c].chn_off = hdma_base + HDMA_WRCH_OFF(c);
		/* skip rc2ep link table */
		thdma->ep2rc[c].lt_vaddr =
			thdma->vhdma_lt + LT_OFF(c + thdma->total_chn);
		thdma->ep2rc[c].lt_paddr =
			link_table + LT_OFF(c + thdma->total_chn);
	}

	atomic64_set(&thdma->hdma_perf.ep2rc_size, 0);
	atomic64_set(&thdma->hdma_perf.rc2ep_size, 0);

	thdma->perf_timer.expires = jiffies + PCIE_BW_TIMER;
	timer_setup(&thdma->perf_timer, pcie_bw_timer_isr, 0);
	add_timer(&thdma->perf_timer);

	sema_init(&thdma->ep2rc_sem, thdma->total_chn);
	sema_init(&thdma->rc2ep_sem, thdma->total_chn);
    for (c = 0; c < HDMA_CH_CNT; c++) {
	    init_waitqueue_head(&thdma->rc2ep[c].queue_wait);
	    init_waitqueue_head(&thdma->ep2rc[c].queue_wait);
	    spin_lock_init(&thdma->rc2ep[c].stats_lock);
	    spin_lock_init(&thdma->ep2rc[c].stats_lock);
	    /* Initialize transfer stats */
	    thdma->rc2ep[c].last_transfer.descriptor_count = 0;
	    thdma->rc2ep[c].last_transfer.total_size = 0;
	    thdma->rc2ep[c].last_transfer.descriptor_sizes = NULL;
	    thdma->rc2ep[c].last_transfer.timestamp = 0;
	    thdma->ep2rc[c].last_transfer.descriptor_count = 0;
	    thdma->ep2rc[c].last_transfer.total_size = 0;
	    thdma->ep2rc[c].last_transfer.descriptor_sizes = NULL;
	    thdma->ep2rc[c].last_transfer.timestamp = 0;
    }
	spin_lock_init(&thdma->rc2ep_bm_lock);
	spin_lock_init(&thdma->ep2rc_bm_lock);

	ret = sysfs_create_group(&tdev->misc_dev->this_device->kobj,
				 &trans_hdma_attribute_group);
	if (ret) {
		sn_pri(tdev, SN_ERR,
		       "hdma: failed to create sysfs device attributes\n");
		goto out_free_table;
	}

	if (hdma_register_irq(tdev)) {
		sn_pri(tdev, SN_ERR, "hdma: register irq failed.\n");
		goto out_remove_sysnode;
	}

	hdma_hw_init(tdev);

	if (hdma_cfg_interrupt(tdev))
		goto out_free_irq;

	sn_pri(tdev, SN_INF, "hdma: module initialization done\n");
	return 0;

out_free_irq:
	hdma_free_irq(tdev);
out_remove_sysnode:
	sysfs_remove_group(&tdev->misc_dev->this_device->kobj,
			   &trans_hdma_attribute_group);
out_free_table:
	del_timer_sync(&thdma->perf_timer);
	iounmap(thdma->vhdma_lt);
out_free_hdma:
	kfree(thdma);
	sn_pri(thdma->tdev, SN_ERR, "hdma: module initialize filed.\n");
	return -1;
}

void sn_hdma_release(struct sn_tranx_t *tdev)
{
	u32 val;
	int c;
	struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];

	del_timer_sync(&thdma->perf_timer);
	sysfs_remove_group(&tdev->misc_dev->this_device->kobj,
			   &trans_hdma_attribute_group);

	/* release EP2RC */
	for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
		/* disable all EP2RC channel */
		hdma_write(tdev, thdma->ep2rc[c].chn_off + HDMA_EN, HDMA_DISABLE);

		/* mask all EP2RC and disable interrupt */
		val = STOP_MASK | WATERMARK_MASK | ABORT_MASK;
		hdma_write(tdev, thdma->ep2rc[c].chn_off + HDMA_INT_SETUP, val);

		/* clear all EP2RC interrupt */
		val = STOP_CLEAR | ABORT_CLEAR;
		hdma_write(tdev, thdma->ep2rc[c].chn_off + HDMA_INT_CLEAR, val);
		
		/* Free transfer statistics memory */
		if (thdma->ep2rc[c].last_transfer.descriptor_sizes) {
			kvfree(thdma->ep2rc[c].last_transfer.descriptor_sizes);
			thdma->ep2rc[c].last_transfer.descriptor_sizes = NULL;
		}
	}

	/* release RC2EP */
	for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
		/* disable all RC2EP channel */
		hdma_write(tdev, thdma->rc2ep[c].chn_off + HDMA_EN, HDMA_DISABLE);

		/* mask all RC2EP and disable interrupt */
		val = STOP_MASK | WATERMARK_MASK | ABORT_MASK;
		hdma_write(tdev, thdma->rc2ep[c].chn_off + HDMA_INT_SETUP, val);

		/* clear all RC2EP interrupt */
		val = STOP_CLEAR | ABORT_CLEAR;
		hdma_write(tdev, thdma->rc2ep[c].chn_off + HDMA_INT_CLEAR, val);
		
		/* Free transfer statistics memory */
		if (thdma->rc2ep[c].last_transfer.descriptor_sizes) {
			kvfree(thdma->rc2ep[c].last_transfer.descriptor_sizes);
			thdma->rc2ep[c].last_transfer.descriptor_sizes = NULL;
		}
	}

	iounmap(thdma->vhdma_lt);
	hdma_free_irq(tdev);

	kfree(thdma);
	sn_pri(tdev, SN_DBG, "hdma: remove module done.\n");
}

long sn_hdma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg,
		   struct sn_tranx_t *tdev)
{
	long ret;
	void __user *argp = (void __user *)arg;
	struct trans_pcie_hdma hdma_info;

	switch (cmd) {
	case SN_TRANX_HDMA_TRANX_VIR:
		if (copy_from_user(&hdma_info, argp, sizeof(hdma_info)))
			return -EFAULT;
		ret = hdma_tranx_viraddr_mode(&hdma_info, tdev, filp);
		break;

	default:
		sn_pri(tdev, SN_ERR, "hdma: %s,cmd:0x%x error\n", __func__,cmd);
		ret = -EINVAL;
	}

	return ret;
}

void sn_hdma_close(struct sn_tranx_t *tdev, struct file *filp)
{
    struct hdma_t *thdma = tdev->modules[SN_MODULE_HDMA];
    int c;
    u32 val;

    for (c = thdma->start_chn; c <= thdma->end_chn; c++) {
        if (thdma->rc2ep[c].filp == filp) {
            sn_pri(tdev, SN_DBG, "hdma_close: rc2ep chn %d \n", c);
            /* stop channel */
            hdma_write(tdev, thdma->rc2ep[c].chn_off + HDMA_DOORBELL, HDMA_STOP);
            /* disable channel */
            hdma_write(tdev, thdma->rc2ep[c].chn_off + HDMA_EN, HDMA_DISABLE);
            /* mask RC2EP and disable interrupt */
            val = STOP_MASK | WATERMARK_MASK | ABORT_MASK;
            hdma_write(tdev, thdma->rc2ep[c].chn_off + HDMA_INT_SETUP, val);
            /* clear RC2EP interrupt */
            val = STOP_CLEAR | ABORT_CLEAR;
            hdma_write(tdev, thdma->rc2ep[c].chn_off + HDMA_INT_CLEAR, val);

            thdma->rc2ep[c].filp = NULL;
            thdma->rc2ep[c].used_cnt--;
            isChannelIdle(tdev, c, thdma->rc2ep[c].chn_off);
        }

        if (thdma->ep2rc[c].filp == filp) {
            sn_pri(tdev, SN_DBG, "hdma_close: ep2rc chn %d \n", c);
            /* stop channel */
            hdma_write(tdev, thdma->ep2rc[c].chn_off + HDMA_DOORBELL, HDMA_STOP);
            /* disable all EP2RC channel */
            hdma_write(tdev, thdma->ep2rc[c].chn_off + HDMA_EN, HDMA_DISABLE);
            /* mask all EP2RC and disable interrupt */
            val = STOP_MASK | WATERMARK_MASK | ABORT_MASK;
            hdma_write(tdev, thdma->ep2rc[c].chn_off + HDMA_INT_SETUP, val);
            /* clear all EP2RC interrupt */
            val = STOP_CLEAR | ABORT_CLEAR;
            hdma_write(tdev, thdma->ep2rc[c].chn_off + HDMA_INT_CLEAR, val);
            thdma->ep2rc[c].filp = NULL;
            thdma->ep2rc[c].used_cnt--;
            isChannelIdle(tdev, c, thdma->ep2rc[c].chn_off);
        }
    }
}
