/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 Verisilicon Inc.
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

#include "regs.h"
#include "error_notify.h"

#define VF_PF_IRQ_INDEX 2
#define PF_ERROR_NOTIFY 0x2

struct mailbox_notify {
	u32 event_id;
	u32 vf_data_ready;
	u32 pf_data_ready;
	int pf_bhalf_ready;
	int reserved[2]; /* For future use */
	u32 data[60];
};

struct error_notify_t {
	unsigned int vf_pf_irq;
	struct mutex vf_to_pf_lock;
	struct sn_tranx_t *tdev;
};

static irqreturn_t vf_rec_pf_isr(int index, void *data)
{
	struct sn_tranx_t *tdev = data;
	struct mailbox_notify __iomem *box;
	u32 val;
	int cnt = 0, i = 0;

	val = readl(tdev->bar2_virt +
			ALL_VF_FROM_PF_INT_CON_STUS);
	if (val & 0x1) {
		box = tdev->bar2_virt + ALL_VF_FROM_PF_MAILBOX;
		if ((box->event_id & 0x07) == PF_TO_VF1) {
			cnt = (box->event_id) >> 3;
			sn_pri(tdev, SN_INF,
						"%s VF1 recieve notify from PF:\n", __func__);
			for (i = 0;i < cnt; i++) {
				sn_pri(tdev, SN_INF, "date[%d] = %d \n", i, box->data[i]);
			}
		}
		else if ((box->event_id & 0x07) == PF_TO_VF2) {
			cnt = (box->event_id) >> 3;
			sn_pri(tdev, SN_INF,
						"%s VF2 recieve notify from PF:\n", __func__);
			for (i = 0;i < cnt; i++) {
				sn_pri(tdev, SN_INF, "date[%d] = %d \n", i, box->data[i]);
			}
		}
		writel(0,
				tdev->bar2_virt +
					ALL_VF_FROM_PF_INT_CON_STUS); /*clear interrupt status */
	}
	sn_pri(tdev, SN_DBG, "%s. val = %d\n", __func__, val);
	return IRQ_HANDLED;
}

int sn_error_notify_init(struct sn_tranx_t *tdev)
{
	int ret;
	struct error_notify_t *notify = NULL;
	if (IS_PF(tdev)) {
		sn_pri(tdev, SN_DBG, "notify: ignore sn_error_notify_init in PF\n");
		return 0;
	}
	notify = kzalloc(sizeof(struct error_notify_t), GFP_KERNEL);
	if (!notify) {
		sn_pri(tdev, SN_ERR, "%s: alloc error_notify_t failed.\n",
				__func__);
		goto out;
	}
	tdev->modules[SN_MODULE_ERROR_NOTIFY] = notify;
	notify->tdev = tdev;

	notify->vf_pf_irq = tdev->msix_entries[VF_PF_IRQ_INDEX].vector;
	if (!notify->vf_pf_irq) {
		sn_pri(tdev, SN_ERR,
		       "notify: get irq failed,vf_pf_irq:%d.\n",
		        notify->vf_pf_irq);
		goto free_notify;
	}
	ret = request_irq(notify->vf_pf_irq, vf_rec_pf_isr, IRQF_SHARED,
			  "vf_rec_vf", tdev);
	if (ret) {
		sn_pri(tdev, SN_ERR, "notify: request vf_rec_vf irq failed.\n");
		goto free_notify;
	}
	mutex_init(&notify->vf_to_pf_lock);
	sn_pri(tdev, SN_INF, "notify:sn_error_notify_init done\n");
	return 0;
free_notify:
	kfree(notify);
	sn_pri(tdev, SN_ERR, "notify: vf_pf_irq probe failed.\n");
out:
	return -EFAULT;
}

void sn_error_notify_release(struct sn_tranx_t *tdev)
{
	struct error_notify_t *notify = tdev->modules[SN_MODULE_ERROR_NOTIFY];
	if (!IS_PF(tdev)) {
		free_irq(notify->vf_pf_irq, (void *)tdev);
		mutex_destroy(&notify->vf_to_pf_lock);
		kfree(notify);
	}
}

int mailbox_vf_get_msg(struct sn_tranx_t *tdev, u32 event_id, u32 *data, int cnt)
{
	int ret = 0;
	struct mailbox_notify __iomem *mailbox;
	struct error_notify_t *notify = tdev->modules[SN_MODULE_ERROR_NOTIFY];
	u32 delay;
	u32 int_con_status_offset;
    u32 mail_box_offset;

	int_con_status_offset = ALL_VF_TO_PF_INT_CON_STUS;
	mail_box_offset = ALL_VF_TO_PF_MAILBOX;

    mutex_lock(&notify->vf_to_pf_lock);
	/* Wait for the last interrupt to finish processing */
	delay = 100;
	while (delay--) {
		if (readl(tdev->bar2_virt + int_con_status_offset) == 0)
			break;
		usleep_range(1000, 1020);
	}
	if (readl(tdev->bar2_virt + int_con_status_offset) != 0) {
		sn_pri(tdev, SN_ERR, "notify: send msg to pf failed.\n");
		ret = -EFAULT;
		goto out;
	}

	/* send data to pf, then trigger a pf interrupt */
	mailbox = tdev->bar2_virt + mail_box_offset;
	mailbox->event_id = event_id;
	mailbox->pf_data_ready = 0;
	mailbox->pf_bhalf_ready = 0;
	mailbox->data[0] = *data;
	/* For IP power up/down, sending VM S1 and S2 config to PF */
	if((event_id == EVENT_VF2PF_IP_POWER_UP) || (event_id == EVENT_VF2PF_IP_POWER_DOWN)) {
		mailbox->data[1] = *++data;
	}
	mailbox->vf_data_ready = 1;
	/* trigger a pf interrupt */
	writel(0x1, tdev->bar2_virt + int_con_status_offset);

	/* wait pf prepare data ready */
	delay = 1000;
	while (delay--) {
		if (mailbox->pf_data_ready)
			break;
		sn_pri(tdev, SN_DBG, "notify: wait pf data.\n");
		usleep_range(1000, 1020);
	}
	if (!mailbox->pf_data_ready) {
		sn_pri(tdev, SN_ERR, "notify: get pf data timeout.\n");
		ret = -EFAULT;
		goto out;
	}

	/* copy share mem to *data */
	memcpy(data, mailbox->data, cnt*sizeof(u32));
out:
	writel(0x0, tdev->bar2_virt + int_con_status_offset);
	/* For IP power up/down, mutex unlock happens in bottom half isr */
	if((ret != 0) || ((event_id != EVENT_VF2PF_IP_POWER_UP) &&
		(event_id != EVENT_VF2PF_IP_POWER_DOWN))) {
		mutex_unlock(&notify->vf_to_pf_lock);
	}
	return ret;
}

int ma35_isr_bh_status(struct sn_tranx_t *tdev)
{
	struct mailbox_notify __iomem *mailbox = tdev->bar2_virt + ALL_VF_TO_PF_MAILBOX;
	struct error_notify_t *notify = tdev->modules[SN_MODULE_ERROR_NOTIFY];
	u32 delay;

	/* wait pf prepare data ready */
	delay = 200;
	while (--delay) {
		if (mailbox->pf_bhalf_ready)
			break;
		msleep(10);
	}

	if(!delay && !mailbox->pf_bhalf_ready) {
		sn_pri(tdev, SN_ERR,
			"notify: Timeout waiting for IP initializations in VM \n");
		mutex_unlock(&notify->vf_to_pf_lock);
		return -1;
	}

	if(mailbox->pf_bhalf_ready < 0) {
		sn_pri(tdev, SN_ERR, "notify: IP power up/down failed \n");
		mutex_unlock(&notify->vf_to_pf_lock);
		return mailbox->pf_bhalf_ready;
	}

	mutex_unlock(&notify->vf_to_pf_lock);
	return 0;
}
