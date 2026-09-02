/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 Verisilicon Inc.
 */

#ifndef ERROR_NOTIFY_H
#define ERROR_NOTIFY_H
#include <linux/types.h>

#include "common.h"

#define PF_TO_VF1 0X1
#define PF_TO_VF2 0X2

int sn_error_notify_init(struct sn_tranx_t *tdev);
void sn_error_notify_release(struct sn_tranx_t *tdev);
int mailbox_vf_get_msg(struct sn_tranx_t *tdev, u32 event_id, u32 *data, int cnt);
int ma35_isr_bh_status(struct sn_tranx_t *tdev);
#endif
