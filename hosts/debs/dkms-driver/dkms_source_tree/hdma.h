/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2020 Verisilicon Inc.
 */

#ifndef _SN_HDMA_H_
#define _SN_HDMA_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#include "common.h"

#define HDMA_CH_CNT 8 //every direction has 8 channels

/* hdma link table info */
struct dma_link_table {
	__u32 control; /* control configuration */
	__u32 size; /* transfer size */
	__u32 sar_low; /* source address low 32 bits */
	__u32 sar_high; /* source address high 32 bits */
	__u32 dst_low; /* destination address low 32 bits */
	__u32 dst_high; /* destination address high 32 bits */
};

/*
 * This struct record hdma performance detail information.
 * Statistics hdma performance by direction separation, two atomic variables
 * real-time size, two regular ints record last second of bandwidth.
 * @rc2ep_size: rc2ep transmission size, util is byte.
 * @ep2rc_size: ep2rc transmission size, util is byte.
 * @rc2ep_per: rc2ep hdma transmission performance, util is MB/s.
 * @ep2rc_per: ep2rc hdma transmission performance, util is MB/s.
 */
struct hdma_perf {
	atomic64_t rc2ep_size;
	atomic64_t ep2rc_size;
	unsigned int rc2ep_per;
	unsigned int ep2rc_per;
};

/* Structure to track transfer statistics */
struct hdma_transfer_stats {
	u32 descriptor_count;      /* Number of descriptors in transfer */
	u32 total_size;            /* Total transfer size in bytes */
	u32 *descriptor_sizes;     /* Array of descriptor sizes */
	u64 timestamp;             /* When transfer occurred (jiffies) */
};

struct hdma_chn {
	u32 condition;
	u32 check_lt;
	u32 chn_status;
    u32 chn_err;
	u32 used_cnt;
	void __iomem *chn_off;
	void __iomem *lt_vaddr;
	u64 lt_paddr;
    struct file *filp;
	wait_queue_head_t queue_wait;
	struct hdma_transfer_stats last_transfer; /* Statistics for last transfer */
	spinlock_t stats_lock;     /* Protect statistics updates */
};

/*
 * The hdma_t structure describes hdma module.
 * @vhdma_lt: hdma link table virtual address.
 * @queue_wait: Waiting for hdma interruption.
 * @ep2rc_sem: protect get/free hdma channel, rc2ep direction.
 * @rc2ep_sem: protect get/free hdma channel, ep2rc direction.
 * @rc2ep_cs_lock: protect get/free hdma channel, rc2ep direction.
 * @ep2rc_cs_lock: protect get/free hdma channel, ep2rc direction.
 * @hdma_perf: record hdma performance data.
 * @perf_timer: it is a timer, calculate performance periodically.
 * @tdev: record struct sn_tranx_t point.
 * @total_chn: hdma total count
 * @start_chn: first channel index
 * @end_chn: the last channel index
 * @link_table_pa: hdma link table physical address in ep side
 */
struct hdma_t {
	void __iomem *vhdma_lt;
	struct hdma_chn rc2ep[HDMA_CH_CNT];
	struct hdma_chn ep2rc[HDMA_CH_CNT];
	struct semaphore ep2rc_sem;
	struct semaphore rc2ep_sem;
	spinlock_t rc2ep_bm_lock;
	spinlock_t ep2rc_bm_lock;
	struct hdma_perf hdma_perf;
	struct timer_list perf_timer;
	struct sn_tranx_t *tdev;

	u8 total_chn;
	u8 start_chn;
	u8 end_chn;

	int err_flag;
	int fix_chn; /* only for test */
	unsigned long rc2ep_bitmap[BITS_TO_LONGS(HDMA_CH_CNT)];
	unsigned long ep2rc_bitmap[BITS_TO_LONGS(HDMA_CH_CNT)];
};

long sn_hdma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg,
		   struct sn_tranx_t *tdev);
int sn_hdma_init(struct sn_tranx_t *tdev);
void sn_hdma_release(struct sn_tranx_t *tdev);
void sn_hdma_close(struct sn_tranx_t *tdev, struct file *filp);
int hdma_cfg_interrupt(struct sn_tranx_t *tdev);
int hdma_normal_rc2ep_xfer(struct trans_pcie_hdma *hdma_info,
			   struct sn_tranx_t *tdev);

int hdma_normal_ep2rc_xfer(struct trans_pcie_hdma *hdma_info,
			   struct sn_tranx_t *tdev);

int hdma_tranx_viraddr_mode(struct trans_pcie_hdma *hdma_info,
                                   struct sn_tranx_t *tdev, struct file *filp);
#endif /* _SN_HDMA_H_ */
