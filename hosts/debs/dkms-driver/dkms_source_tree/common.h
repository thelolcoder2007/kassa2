/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2020 Verisilicon Inc.
 */

#ifndef _SN_COMMON_H_
#define _SN_COMMON_H_

#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <linux/timer.h>

/*
 * Kernel 6.15+ removed del_timer_sync() wrapper - use timer_delete_sync()
 * Kernel 6.16+ renamed from_timer() to timer_container_of()
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 16, 0)
#define from_timer(var, callback_timer, timer_fieldname) \
	timer_container_of((var), callback_timer, timer_fieldname)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 15, 0)
#define del_timer_sync(t) timer_delete_sync(t)
#endif

#include "transcoder.h"
#include "regs.h"

#define MAX_SUBSYS_NUM               2
#define MAX_SLICE_NUM                2

#define PF_MODE                      0x0
#define ONE_VF_MODE                  0x1
#define TWO_VF_MODE                  0x2

#define MAX_MSIX_CNT                 32
#define MIN_MSIX_CNT                 32

/* vf --> pf: event id */
enum SN_VF2PF_EVENT_INDEX {
	EVENT_VF2PF_GET_HDMA_MSIX_INFO = 1,
	EVENT_VF2PF_GET_LINK_STATUS,
	EVENT_VF2PF_RESET_ENCODER_FW,
	EVENT_VF2PF_IP_POWER_UP,
	EVENT_VF2PF_IP_POWER_DOWN,
	EVENT_VF2PF_MAX
};

/* pf --> fw: event id */
#define EVENT_PF2FW_SET_PF_VF_MODE		0x1

/* submodule index */
enum TRANS_MODULE_INDEX {
	SN_MODULE_PCIE = 0,
	SN_MODULE_HW_MONITOR,
	SN_MODULE_ERROR_NOTIFY,
	SN_MODULE_HDMA,
	SN_MODULE_MEMORY_OSAL,
	SN_MODULE_PERF,
	SN_MODULE_OSAL,
	SN_MODULE_VC8000D,
	SN_MODULE_VCMD,
	SN_MODULE_VC8000E,
	SN_MODULE_XABR,
	SN_MODULE_XAV1_ENC,
	SN_MODULE_RISCV,
	SN_MODULE_HOST_DEC,
	SN_MODULE_MEMORY,
	SN_MODULE_MAX,
};

#define QWORD_HI(v)                  (((v)>>32)&0xFFFFFFFF)
#define QWORD_LO(v)                  ((v)&0xFFFFFFFF)

/* hardware error flag, fatal error. */
#define HW_ERR_FLAG			0xDEADDEAD

struct ma35_kwork_ps {
	struct kthread_work ma35_kwork;
	void *tdev;
	u32 power_event;
	u32 slice_config[2];
};

/* The sn_tranx_t structure describes transcoder devices */
struct sn_tranx_t {
	const char *dev_name; /* device name */
	int node_index; /* only a index for device name: /dev/ama_transcoderN */
	struct miscdevice *misc_dev; /* misc device info */
	struct pci_dev *pdev; /* pci device info */
	void __iomem *bar0_virt; /* pci bar0 virtual address */
	void __iomem *bar2_virt; /* pci bar2 virtual address */
	void __iomem *bar4_virt; /* pci bar4 virtual address */
	dma_addr_t bar4_base;
	size_t bar4_size;
	void __iomem *vf_bar0_virt; /* pci vf_bar0 virtual address */
	void *modules[SN_MODULE_MAX]; /* submodule private data */
	unsigned int hw_err_flag; /* hardware error flag */
	int print_level; /* log level */
	struct msix_entry msix_entries[MAX_MSIX_CNT];

	u16 vf_max_count; /* software support max vf count */
	u16 vf_index; /* current vf index, used in vf driver */
	u8 pf_vf_mode; /* current driver work in pf/one vf/two vf mode */
	u8 pf_vf_flag;/* current driver is pf or vf, 0 is pf */
	int init_flag;
	int mem_alloc_method;
	int ddr_ecc_flag;
	int fps_unittest_en;
	struct kthread_worker *kworker_thread_ps;
	struct ma35_kwork_ps *kwork_ps;
};

#define IS_PF(tdev)	(((tdev)->pf_vf_flag) == DEVICE_TYPE_PF)

struct sn_misc_tdev {
	struct miscdevice misc;
	struct sn_tranx_t *tdev;
};

/* get codec utilization in one second */
#define LOADING_TIME		1

/* Used to record the usage of chip */
struct loading_info {
	unsigned long time_cnt; /* unit microsecond */
	unsigned long time_cnt_saved; /* unit microsecond */
	ktime_t tv_s;
	ktime_t tv_e;
	unsigned long total_time; /* unit microsecond */
};

irqreturn_t unify_isr(int irq, void *data);

/* SN_ERR: error; SN_INF:info; SN_DBG:debug */
enum SN_PRI_LEVEL {
	SN_ERR    = 0x0,
	SN_INF    = 0x1,
	SN_DBG    = 0x2,
};

enum SYS_CON_SUBSYS {
	SYS_CTL_VCDA = 0,
	SYS_CTL_VCDB,
	SYS_CTL_VCE,
	SYS_CTL_GPU2D,
	SYS_CTL_VIP,
	SYS_CTL_XABR,
	SYS_CTL_XENC,
	SYS_CTL_XFPS,
	SYS_CTL_XAV1,
	SYS_CTL_MAX,
};

static const char subsys_name[][20] = {
	"vcda",
	"vcdb",
	"vce",
	"gpu2d",
	"vip",
	"xabr",
	"xenc",
	"xfps",
	"xav1"
};

struct sys_ctrl_cfg_regs {
	char name[16];
	u32 slice_off;
	u32 subsys;
	u32 clock_ctrl;
	u32 clock_ctrl_bits;
	u32 sys_reset;
	u32 sys_reset_bits;
	u32 power_setting;
	u32 power_setting_clear_bits;
	u32 power_setting_polling_bits;
	u32 adb_setting;
	u32 adb_setting_clear_bits;
	u32 adb_setting_polling_bits;
};


void __sn_pri(void *dev, int level, const char *fmt, ...);

#define sn_pri(adapter, level, fmt, ...) \
  do { \
    if (likely(!WARN_ON(!adapter)) && unlikely((level) <= ((struct sn_tranx_t*) (adapter))->print_level)) { \
    __sn_pri(adapter, level, fmt, ##__VA_ARGS__); \
    } \
  } while (0)

void sn_wr_b0(struct sn_tranx_t *tdev, unsigned int off, unsigned int val);
unsigned int sn_rd_b0(struct sn_tranx_t *tdev, unsigned int off);
void sn_wr_b2(struct sn_tranx_t *tdev, unsigned int off, unsigned int val);
unsigned int sn_rd_b2(struct sn_tranx_t *tdev, unsigned int off);
void sys_configure(struct sn_tranx_t *tdev, int slice, enum SYS_CON_SUBSYS subsys);
void sys_configure_ext(struct sn_tranx_t *tdev, int slice);
void sys_xav1_whole_subsys_soft_reset(struct sn_tranx_t *tdev, int slice);
void sys_xav1_whole_subsys_release_soft_reset(struct sn_tranx_t *tdev, int slice);
enum SYS_CON_SUBSYS get_subsys_config_index(u32 subsys_base_addr);
int get_slice_by_subsys_base_addr(u32 base_addr);
void sys_configure_on(struct sn_tranx_t *tdev, int slice, enum SYS_CON_SUBSYS subsys);
void sys_configure_off(struct sn_tranx_t *tdev, int slice, enum SYS_CON_SUBSYS subsys);
void sys_configure_reset(struct sn_tranx_t *tdev, int slice, enum SYS_CON_SUBSYS subsys);
void sys_config_reset_keep(struct sn_tranx_t *tdev, int slice, enum SYS_CON_SUBSYS subsys);
void sys_config_reset_release(struct sn_tranx_t *tdev, int slice, enum SYS_CON_SUBSYS subsys);
bool slice_accessible(struct sn_tranx_t *tdev, int slice);

#endif /* _SN_COMMON_H_ */
