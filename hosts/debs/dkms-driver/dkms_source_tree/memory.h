/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2020 VeriSilicon Holdings Co., Ltd.
 */

#ifndef _SN_MEMORY_H_
#define _SN_MEMORY_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#include "common.h"

/* aggress as follows: the first slice is slice_1, another is slice_2 */
#define MAX_TASK_NUM 128
#define MIN_TASK_ID 1
//Keeping in mind 3:3.5GB, and allocating for 4K 10bit frames incl nv12 tile (26MB)
#define S1_BLOCK_CNT 70
#define S2_BLOCK_CNT 80
#define CHUNK_SIZE 0x1000

#define SLICE1_INDEX 1
#define SLICE2_INDEX 2

#define ALLOCATE_METHOD_S1 1
#define ALLOCATE_METHOD_S2 2
#define ALLOCATE_METHOD_S1S2 3

/* The memory_t structure describes memory module */
struct memory_t {
	struct sn_tranx_t *tdev;
	struct mutex mem_mutex_ep;

	struct mem_block *mem_s1_bk; /* memory slice1 block*/
	struct mem_block *mem_s2_bk; /* memory slice2 block*/
	u32 s1_rev_size; /* slice1_reserved_size */
	u32 s2_rev_size; /* slice2_reserved_size */

	spinlock_t taskid_lock;
	int id_st[MAX_TASK_NUM + 1]; /* task id status */
	struct file *id_filp[MAX_TASK_NUM + 1]; /* save owner who use the id */
};

int sn_mem_init(struct sn_tranx_t *tdev);
void sn_mem_release(struct sn_tranx_t *tdev);
long sn_mem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg,
		  struct sn_tranx_t *tdev);
void sn_mem_close(struct sn_tranx_t *tdev, struct file *filp);
int alloc_mem_ep(unsigned long *busaddr, unsigned int size, int task_id, int fd,
		 struct memory_t *tmem);
int free_mem_ep(unsigned long busaddr, unsigned int size, int task_id,
		struct memory_t *tmem);
int get_memory_owner(struct sn_tranx_t *tdev, u64 addr);

int get_task_id(struct memory_t *tmem, struct file *filp);

void free_task_id(struct memory_t *tmem, int id);

int sn_mem_get_method(struct sn_tranx_t *tdev);

#endif /* _SN_MEMORY_H_ */
