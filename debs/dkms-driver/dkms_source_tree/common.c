// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 Verisilicon Inc.
 */

#include <linux/pci.h>
#include <linux/pagemap.h>

#include "common.h"
#include "transcoder.h"
#include "vc8000e.h"

#define LOOP_NUM 50

void __sn_pri(void *dev, int level, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	struct sn_tranx_t *tdev = dev;
	struct pci_dev *pdev;

	pdev = tdev->pdev;
	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	printk(KERN_ERR "%s %04x:%02x:%02x.%d %pV", tdev->dev_name,
	       pci_domain_nr(pdev->bus), pdev->bus->number,
	       PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn), &vaf);

	va_end(args);
}

void sn_wr_b0(struct sn_tranx_t *tdev, unsigned int off,
		     unsigned int val)
{
	writel((val), ((off) + tdev->bar0_virt));
}

unsigned int sn_rd_b0(struct sn_tranx_t *tdev, unsigned int off)
{
	return readl((off) + (tdev->bar0_virt));
}

void sn_wr_b2(struct sn_tranx_t *tdev, unsigned int off,
		     unsigned int val)
{
	writel((val), ((off) + tdev->bar2_virt));
}

unsigned int sn_rd_b2(struct sn_tranx_t *tdev, unsigned int off)
{
	return readl((off) + (tdev->bar2_virt));
}

static struct sys_ctrl_cfg_regs *
get_sys_configre_regs(struct sn_tranx_t *tdev, int slice,
		      enum SYS_CON_SUBSYS subsys)
{
	struct sys_ctrl_cfg_regs *pregs = NULL;
	static struct sys_ctrl_cfg_regs sys_cfg_pf[MAX_SLICE_NUM][SYS_CTL_MAX] = {
		[0] = {
            //                                                            clock bits reset  bit  power  clr polling adb  clr  polling
			[SYS_CTL_VCDA]   = {"vcda",   S1_SYS_CON_OFF, SYS_CTL_VCDA,   0x00, 0x1, 0x100, 0x1, 0x700, 0x1, 0x2, 0x510, 0x1, 0x20},
			[SYS_CTL_VCDB]   = {"vcdb",   S1_SYS_CON_OFF, SYS_CTL_VCDB,   0x04, 0x1, 0x104, 0x1, 0x704, 0x1, 0x2, 0x514, 0x1, 0x20},
			[SYS_CTL_VCE]    = {"vce",    S1_SYS_CON_OFF, SYS_CTL_VCE,    0x14, 0x1, 0x114, 0x1, 0x710, 0x1, 0x2, 0x520, 0x3, 0xa0},
			[SYS_CTL_GPU2D]  = {"gpu2d",  S1_SYS_CON_OFF, SYS_CTL_GPU2D,  0x1c, 0x1, 0x11c, 0x1, 0x720, 0x1, 0x2, 0x528, 0x1, 0x20},
			[SYS_CTL_VIP]    = {"vip",    S1_SYS_CON_OFF, SYS_CTL_VIP,    0x18, 0x7, 0x118, 0x7, 0x724, 0x1, 0x2, 0x524, 0x3, 0xa0},
			[SYS_CTL_XABR]   = {"xabr",   S1_SYS_CON_OFF, SYS_CTL_XABR,   0x24, 0x1, 0x124, 0x1, 0x71c, 0x1, 0x2, 0x52c, 0x1, 0x20},
			[SYS_CTL_XENC]   = {"xenc",   S1_SYS_CON_OFF, SYS_CTL_XENC,   0x20, 0x7, 0x120, 0x7, 0x718, 0x1, 0x2, 0x530, 0x3, 0xa00},
			[SYS_CTL_XFPS]   = {"xfps",   S1_SYS_CON_OFF, SYS_CTL_XFPS,   0x28, 0x1, 0x128, 0x1, 0x714, 0x1, 0x2, 0x530, 0x3, 0xa00},
			[SYS_CTL_XAV1]   = {"xav1",   S1_SYS_CON_OFF, SYS_CTL_XAV1,   0x20, 0x1, 0x120, 0x1, 0x714, 0x1, 0x2, 0x60c, 0x1, 0x6}
		},
		[1] = {
            //                                                            clock bits reset  bit  power  clr polling adb  clr  polling
			[SYS_CTL_VCDA]   = {"vcda",   S2_SYS_CON_OFF, SYS_CTL_VCDA,   0x00, 0x1, 0x100, 0x1, 0x700, 0x1, 0x2, 0x510, 0x1, 0x20},
			[SYS_CTL_VCDB]   = {"vcdb",   S2_SYS_CON_OFF, SYS_CTL_VCDB,   0x04, 0x1, 0x104, 0x1, 0x704, 0x1, 0x2, 0x514, 0x1, 0x20},
			[SYS_CTL_VCE]    = {"vce",    S2_SYS_CON_OFF, SYS_CTL_VCE,    0x14, 0x1, 0x114, 0x1, 0x710, 0x1, 0x2, 0x520, 0x3, 0xa0},
			[SYS_CTL_GPU2D]  = {"gpu2d",  S2_SYS_CON_OFF, SYS_CTL_GPU2D,  0x1c, 0x1, 0x11c, 0x1, 0x720, 0x1, 0x2, 0x528, 0x1, 0x20},
			[SYS_CTL_VIP]    = {"vip",    S2_SYS_CON_OFF, SYS_CTL_VIP,    0x18, 0x7, 0x118, 0x7, 0x724, 0x1, 0x2, 0x524, 0x3, 0xa0},
			[SYS_CTL_XABR]   = {"xabr",   S2_SYS_CON_OFF, SYS_CTL_XABR,   0x24, 0x1, 0x124, 0x1, 0x71c, 0x1, 0x2, 0x52c, 0x1, 0x20},
			[SYS_CTL_XENC]   = {"xenc",   S2_SYS_CON_OFF, SYS_CTL_XENC,   0x20, 0x7, 0x120, 0x7, 0x718, 0x1, 0x2, 0x530, 0x3, 0xa00},
			[SYS_CTL_XFPS]   = {"xfps",   S2_SYS_CON_OFF, SYS_CTL_XFPS,   0x28, 0x1, 0x128, 0x1, 0x714, 0x1, 0x2, 0x530, 0x3, 0xa00},
			[SYS_CTL_XAV1]   = {"xav1",   S2_SYS_CON_OFF, SYS_CTL_XAV1,   0x20, 0x1, 0x120, 0x1, 0x714, 0x1, 0x2, 0x60c, 0x1, 0x6}
		}
	};
	static struct sys_ctrl_cfg_regs sys_cfg_vf[1][SYS_CTL_MAX] = {
		[0] = {
            //                                                            clock bits reset  bit  power  clr polling adb  clr  polling
			[SYS_CTL_VCDA]   = {"vcda",   VF_SYS_CON_OFF, SYS_CTL_VCDA,   0x00, 0x1, 0x100, 0x1, 0x700, 0x1, 0x2, 0x510, 0x1, 0x20},
			[SYS_CTL_VCDB]   = {"vcdb",   VF_SYS_CON_OFF, SYS_CTL_VCDB,   0x04, 0x1, 0x104, 0x1, 0x704, 0x1, 0x2, 0x514, 0x1, 0x20},
			[SYS_CTL_VCE]    = {"vce",    VF_SYS_CON_OFF, SYS_CTL_VCE,    0x14, 0x1, 0x114, 0x1, 0x710, 0x1, 0x2, 0x520, 0x3, 0xa0},
			[SYS_CTL_GPU2D]  = {"gpu2d",  VF_SYS_CON_OFF, SYS_CTL_GPU2D,  0x1c, 0x1, 0x11c, 0x1, 0x720, 0x1, 0x2, 0x528, 0x1, 0x20},
			[SYS_CTL_VIP]    = {"vip",    VF_SYS_CON_OFF, SYS_CTL_VIP,    0x18, 0x7, 0x118, 0x7, 0x724, 0x1, 0x2, 0x524, 0x3, 0xa0},
			[SYS_CTL_XABR]   = {"xabr",   VF_SYS_CON_OFF, SYS_CTL_XABR,   0x24, 0x1, 0x124, 0x1, 0x71c, 0x1, 0x2, 0x52c, 0x1, 0x20},
			[SYS_CTL_XENC]   = {"xenc",   VF_SYS_CON_OFF, SYS_CTL_XENC,   0x20, 0x7, 0x120, 0x7, 0x718, 0x1, 0x2, 0x530, 0x3, 0xa00},
			[SYS_CTL_XFPS]   = {"xfps",   VF_SYS_CON_OFF, SYS_CTL_XFPS,   0x28, 0x1, 0x128, 0x1, 0x714, 0x1, 0x2, 0x530, 0x3, 0xa00},
			[SYS_CTL_XAV1]   = {"xav1",   VF_SYS_CON_OFF, SYS_CTL_XAV1,   0x20, 0x1, 0x120, 0x1, 0x714, 0x1, 0x2, 0x60c, 0x1, 0x6}
		}
	};
	static struct sys_ctrl_cfg_regs sys_cfg_one_vf[MAX_SLICE_NUM][SYS_CTL_MAX] = {
		[0] = {
            //                                                            clock bits reset  bit  power  clr polling adb  clr  polling
			[SYS_CTL_VCDA]   = {"vcda",   ONE_VF_S1_SYS_CON_OFF, SYS_CTL_VCDA,   0x00, 0x1, 0x100, 0x1, 0x700, 0x1, 0x2, 0x510, 0x1, 0x20},
			[SYS_CTL_VCDB]   = {"vcdb",   ONE_VF_S1_SYS_CON_OFF, SYS_CTL_VCDB,   0x04, 0x1, 0x104, 0x1, 0x704, 0x1, 0x2, 0x514, 0x1, 0x20},
			[SYS_CTL_VCE]    = {"vce",    ONE_VF_S1_SYS_CON_OFF, SYS_CTL_VCE,    0x14, 0x1, 0x114, 0x1, 0x710, 0x1, 0x2, 0x520, 0x3, 0xa0},
			[SYS_CTL_GPU2D]  = {"gpu2d",  ONE_VF_S1_SYS_CON_OFF, SYS_CTL_GPU2D,  0x1c, 0x1, 0x11c, 0x1, 0x720, 0x1, 0x2, 0x528, 0x1, 0x20},
			[SYS_CTL_VIP]    = {"vip",    ONE_VF_S1_SYS_CON_OFF, SYS_CTL_VIP,    0x18, 0x7, 0x118, 0x7, 0x724, 0x1, 0x2, 0x524, 0x3, 0xa0},
			[SYS_CTL_XABR]   = {"xabr",   ONE_VF_S1_SYS_CON_OFF, SYS_CTL_XABR,   0x24, 0x1, 0x124, 0x1, 0x71c, 0x1, 0x2, 0x52c, 0x1, 0x20},
			[SYS_CTL_XENC]   = {"xenc",   ONE_VF_S1_SYS_CON_OFF, SYS_CTL_XENC,   0x20, 0x7, 0x120, 0x7, 0x718, 0x1, 0x2, 0x530, 0x3, 0xa00},
			[SYS_CTL_XFPS]   = {"xfps",   ONE_VF_S1_SYS_CON_OFF, SYS_CTL_XFPS,   0x28, 0x1, 0x128, 0x1, 0x714, 0x1, 0x2, 0x530, 0x3, 0xa00},
			[SYS_CTL_XAV1]   = {"xav1",   ONE_VF_S1_SYS_CON_OFF, SYS_CTL_XAV1,   0x20, 0x1, 0x120, 0x1, 0x714, 0x1, 0x2, 0x60c, 0x1, 0x6}
		},
		[1] = {
            //                                                            clock bits reset  bit  power  clr polling adb  clr  polling
			[SYS_CTL_VCDA]   = {"vcda",   ONE_VF_S2_SYS_CON_OFF, SYS_CTL_VCDA,   0x00, 0x1, 0x100, 0x1, 0x700, 0x1, 0x2, 0x510, 0x1, 0x20},
			[SYS_CTL_VCDB]   = {"vcdb",   ONE_VF_S2_SYS_CON_OFF, SYS_CTL_VCDB,   0x04, 0x1, 0x104, 0x1, 0x704, 0x1, 0x2, 0x514, 0x1, 0x20},
			[SYS_CTL_VCE]    = {"vce",    ONE_VF_S2_SYS_CON_OFF, SYS_CTL_VCE,    0x14, 0x1, 0x114, 0x1, 0x710, 0x1, 0x2, 0x520, 0x3, 0xa0},
			[SYS_CTL_GPU2D]  = {"gpu2d",  ONE_VF_S2_SYS_CON_OFF, SYS_CTL_GPU2D,  0x1c, 0x1, 0x11c, 0x1, 0x720, 0x1, 0x2, 0x528, 0x1, 0x20},
			[SYS_CTL_VIP]    = {"vip",    ONE_VF_S2_SYS_CON_OFF, SYS_CTL_VIP,    0x18, 0x7, 0x118, 0x7, 0x724, 0x1, 0x2, 0x524, 0x3, 0xa0},
			[SYS_CTL_XABR]   = {"xabr",   ONE_VF_S2_SYS_CON_OFF, SYS_CTL_XABR,   0x24, 0x1, 0x124, 0x1, 0x71c, 0x1, 0x2, 0x52c, 0x1, 0x20},
			[SYS_CTL_XENC]   = {"xenc",   ONE_VF_S2_SYS_CON_OFF, SYS_CTL_XENC,   0x20, 0x7, 0x120, 0x7, 0x718, 0x1, 0x2, 0x530, 0x3, 0xa00},
			[SYS_CTL_XFPS]   = {"xfps",   ONE_VF_S2_SYS_CON_OFF, SYS_CTL_XFPS,   0x28, 0x1, 0x128, 0x1, 0x714, 0x1, 0x2, 0x530, 0x3, 0xa00},
			[SYS_CTL_XAV1]   = {"xav1",   ONE_VF_S2_SYS_CON_OFF, SYS_CTL_XAV1,   0x20, 0x1, 0x120, 0x1, 0x714, 0x1, 0x2, 0x60c, 0x1, 0x6}
		}
	};
	if (slice >= MAX_SLICE_NUM || subsys >= SYS_CTL_MAX) {
		sn_pri(tdev, SN_ERR,
		       "syscfg: sysconfig input param error, slice: %d, subsys: %d!\n",
		       slice, subsys);
		return NULL;
	}
	switch (tdev->pf_vf_mode) {
		case PF_MODE:
			pregs = &sys_cfg_pf[slice][subsys];
			break;
		case TWO_VF_MODE:
			pregs = &sys_cfg_vf[0][subsys];
			break;
		case ONE_VF_MODE:
			pregs = &sys_cfg_one_vf[slice][subsys];
			break;
		default:
			break;
	}
	sn_pri(tdev, SN_DBG,
		       "%s: mode = %d, slice: %d, subsys: %d!,sys_configure_off: 0x%x\n",
		       __func__, tdev->pf_vf_mode, slice, subsys, pregs->slice_off);
	return pregs;
}

static void clock_setting(struct sn_tranx_t *tdev,
			  struct sys_ctrl_cfg_regs *psub, int on_off)
{
	int val;

	if (on_off) {
		// system clock setting-on
		val = readl(tdev->bar2_virt + psub->slice_off +
			    psub->clock_ctrl);
		writel(val | psub->clock_ctrl_bits,
		       tdev->bar2_virt + psub->slice_off + psub->clock_ctrl);
	} else {
		// system clock setting-off
		val = readl(tdev->bar2_virt + psub->slice_off +
			    psub->clock_ctrl);
		writel(val & (~psub->clock_ctrl_bits),
		       tdev->bar2_virt + psub->slice_off + psub->clock_ctrl);
	}
}

static void reset_setting(struct sn_tranx_t *tdev,
			  struct sys_ctrl_cfg_regs *psub, int on_off)
{
	int val;

	if (on_off) {
		// system rst setting-on
		val = readl(tdev->bar2_virt + psub->slice_off +
			    psub->sys_reset);
		writel(val | psub->sys_reset_bits,
		       tdev->bar2_virt + psub->slice_off + psub->sys_reset);
	} else {
		// system rst setting-off
		val = readl(tdev->bar2_virt + psub->slice_off +
			    psub->sys_reset);
		writel(val & (~psub->sys_reset_bits),
		       tdev->bar2_virt + psub->slice_off + psub->sys_reset);
	}
}

static int adb_setting(struct sn_tranx_t *tdev, enum SYS_CON_SUBSYS subsys,
		       struct sys_ctrl_cfg_regs *psub, int on_off)
{
	int val;
	int ret = 0;
	unsigned int loop = LOOP_NUM;

	if (on_off) {
		//adb setting-on
		if (subsys == SYS_CTL_VIP) {
			val = readl(tdev->bar2_virt + psub->slice_off +
				    psub->adb_setting);
			writel(val | psub->adb_setting_clear_bits,
			       tdev->bar2_virt + psub->slice_off +
				       psub->adb_setting);
			val = readl(tdev->bar2_virt + psub->slice_off +
				    psub->adb_setting);
			while ((val & psub->adb_setting_polling_bits) !=
			       psub->adb_setting_polling_bits) {
				val = readl(tdev->bar2_virt + psub->slice_off +
					    psub->adb_setting);
				msleep(1);
				loop--;
				if (loop == 0) {
					sn_pri(tdev, SN_ERR,
					       "%s: system controller adb configuration error!\n",
					       psub->name);
					break;
				}
			}
			loop = LOOP_NUM;

			val = readl(tdev->bar2_virt + psub->slice_off + 0x540);
			writel(val | 0x3,
			       tdev->bar2_virt + psub->slice_off + 0x540);
			while ((val & 0xa0) != 0xa0) {
				val = readl(tdev->bar2_virt + psub->slice_off +
					    0x540);
				msleep(1);
				loop--;
				if (loop == 0) {
					sn_pri(tdev, SN_ERR,
					       "%s: system controller adb configuration error!\n",
					       psub->name);
					break;
				}
			}
		} else if (subsys == SYS_CTL_XAV1) { //axi config
			val = readl(tdev->bar2_virt + psub->slice_off +
				    psub->adb_setting);
			writel(val & ~(psub->adb_setting_clear_bits),
			       tdev->bar2_virt + psub->slice_off +
				       psub->adb_setting);
			val = readl(tdev->bar2_virt + psub->slice_off +
				    psub->adb_setting);
			while ((val & psub->adb_setting_polling_bits) != 0x0) {
				val = readl(tdev->bar2_virt + psub->slice_off +
					    psub->adb_setting);
				msleep(1);
				loop--;
				if (loop == 0) {
					sn_pri(tdev, SN_ERR,
					       "%s: system controller axi configuration error!\n",
					       psub->name);
					break;
				}
			}
		} else {
			val = readl(tdev->bar2_virt + psub->slice_off +
				    psub->adb_setting);
			writel(val | psub->adb_setting_clear_bits,
			       tdev->bar2_virt + psub->slice_off +
				       psub->adb_setting);
			val = readl(tdev->bar2_virt + psub->slice_off +
				    psub->adb_setting);
			while ((val & psub->adb_setting_polling_bits) !=
			       psub->adb_setting_polling_bits) {
				val = readl(tdev->bar2_virt + psub->slice_off +
					    psub->adb_setting);
				msleep(1);
				loop--;
				if (loop == 0) {
					sn_pri(tdev, SN_ERR,
					       "%s: system controller adb configuration error!\n",
					       psub->name);
					break;
				}
			}
		}
	} else {
		//adb setting-off
		if (subsys == SYS_CTL_VIP) {
			val = readl(tdev->bar2_virt + psub->slice_off +
				    psub->adb_setting);
			writel(val & (~psub->adb_setting_clear_bits),
			       tdev->bar2_virt + psub->slice_off +
				       psub->adb_setting);
			val = readl(tdev->bar2_virt + psub->slice_off +
				    psub->adb_setting);
			while ((val & psub->adb_setting_polling_bits) ==
			       psub->adb_setting_polling_bits) {
				val = readl(tdev->bar2_virt + psub->slice_off +
					    psub->adb_setting);
				msleep(1);
				loop--;
				if (loop == 0) {
					sn_pri(tdev, SN_ERR,
					       "%s: system controller adb configuration error!\n",
					       psub->name);
					break;
				}
			}
			loop = LOOP_NUM;

			val = readl(tdev->bar2_virt + psub->slice_off + 0x540);
			writel(val & (~0x3),
			       tdev->bar2_virt + psub->slice_off + 0x540);
			while ((val & 0xa0) == 0xa0) {
				val = readl(tdev->bar2_virt + psub->slice_off +
					    0x540);
				msleep(1);
				loop--;
				if (loop == 0) {
					sn_pri(tdev, SN_ERR,
					       "%s: system controller adb configuration error!\n",
					       psub->name);
					break;
				}
			}
		} else if (subsys == SYS_CTL_XAV1) { //axi config
			val = readl(tdev->bar2_virt + psub->slice_off +
				    psub->adb_setting);
			writel(val | psub->adb_setting_clear_bits,
			       tdev->bar2_virt + psub->slice_off +
				       psub->adb_setting);
			val = readl(tdev->bar2_virt + psub->slice_off +
				    psub->adb_setting);
			while ((val & psub->adb_setting_polling_bits) == 0x0) {
				val = readl(tdev->bar2_virt + psub->slice_off +
					    psub->adb_setting);
				msleep(1);
				loop--;
				if (loop == 0) {
					sn_pri(tdev, SN_ERR,
					       "%s: system controller axi-off configuration error!\n",
					       psub->name);
					ret = -1;
					break;
				}
			}
		} else {
			val = readl(tdev->bar2_virt + psub->slice_off +
				    psub->adb_setting);
			writel(val & ~(psub->adb_setting_clear_bits),
			       tdev->bar2_virt + psub->slice_off +
				       psub->adb_setting);
			val = readl(tdev->bar2_virt + psub->slice_off +
				    psub->adb_setting);
			while ((val & psub->adb_setting_polling_bits) ==
			       psub->adb_setting_polling_bits) {
				val = readl(tdev->bar2_virt + psub->slice_off +
					    psub->adb_setting);
				msleep(1);
				loop--;
				if (loop == 0) {
					sn_pri(tdev, SN_ERR,
					       "%s: system controller adb-off configuration error, val:0x%x, polling_bit: 0x%x!\n",
					       psub->name, val,
					       psub->adb_setting_polling_bits);
					ret = -1;
					break;
				}
			}
		}
	}

	return ret;
}

void sys_configure_reset(struct sn_tranx_t *tdev, int slice,
			 enum SYS_CON_SUBSYS subsys)
{
	int ret;
	struct sys_ctrl_cfg_regs *psub;

	psub = get_sys_configre_regs(tdev, slice, subsys);
	if (psub == NULL) {
		return;
	}
	sn_pri(tdev, SN_INF,
	       "com: s%d_%s sys_configure_reset: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x!\n",
	       slice + 1, psub->name, psub->slice_off, psub->clock_ctrl,
	       psub->sys_reset, psub->power_setting, psub->adb_setting);
	ret = adb_setting(tdev, subsys, psub, 0);
	if (ret != 0) {
		return;
	}
	clock_setting(tdev, psub, 0);
	reset_setting(tdev, psub, 0);

	clock_setting(tdev, psub, 1);
	reset_setting(tdev, psub, 1);
	ret = adb_setting(tdev, subsys, psub, 1);
	if (ret != 0) {
		return;
	}
}

void sys_config_reset_keep(struct sn_tranx_t *tdev, int slice, enum SYS_CON_SUBSYS subsys)
{
	int ret;
	struct sys_ctrl_cfg_regs *psub;

	psub = get_sys_configre_regs(tdev, slice, subsys);
	if (psub == NULL) {
		return;
	}
	sn_pri(tdev, SN_INF, "com: s%d_%s sys_config_reset_keep: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x!\n",
		slice+1,psub->name, psub->slice_off, psub->clock_ctrl, psub->sys_reset, psub->power_setting, psub->adb_setting);
	ret = adb_setting(tdev, subsys, psub, 0);
	if (ret != 0) {
		return;
	}
	clock_setting(tdev, psub, 0);
	reset_setting(tdev, psub, 0);
}

void sys_config_reset_release(struct sn_tranx_t *tdev, int slice, enum SYS_CON_SUBSYS subsys)
{
	int ret;
	struct sys_ctrl_cfg_regs *psub;

	psub = get_sys_configre_regs(tdev, slice, subsys);
	if (psub == NULL) {
		return;
	}
	sn_pri(tdev, SN_INF, "com: s%d_%s sys_config_reset_release: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x!\n",
		slice+1,psub->name, psub->slice_off, psub->clock_ctrl, psub->sys_reset, psub->power_setting, psub->adb_setting);

	clock_setting(tdev, psub, 1);
	reset_setting(tdev, psub, 1);
	ret = adb_setting(tdev, subsys, psub, 1);
	if (ret != 0) {
		return;
	}
}

bool slice_accessible(struct sn_tranx_t *tdev, int slice)
{
	if (tdev->pf_vf_mode == TWO_VF_MODE && tdev->vf_index > 0 &&
		tdev->vf_index - 1 != slice)
		return false;
	return true;
}

void sys_xav1_whole_subsys_soft_reset(struct sn_tranx_t *tdev, int slice)
{
  unsigned int val;
  unsigned int loop = LOOP_NUM;
  struct sys_ctrl_cfg_regs *pencsub = get_sys_configre_regs(tdev, slice, SYS_CTL_XENC);
  struct sys_ctrl_cfg_regs *pfpssub = get_sys_configre_regs(tdev, slice, SYS_CTL_XFPS);
  struct sys_ctrl_cfg_regs *pav1sub = get_sys_configre_regs(tdev, slice, SYS_CTL_XAV1);

  if (pencsub == NULL || pfpssub == NULL || pav1sub == NULL)
  {
    return;
  }

  // 1.
  val = readl(tdev->bar2_virt + pencsub->slice_off + pencsub->adb_setting);
  writel(val & (~(pencsub->adb_setting_clear_bits)), tdev->bar2_virt + pencsub->slice_off + pencsub->adb_setting);

  // 2.
  val = readl(tdev->bar2_virt + pencsub->slice_off + pencsub->adb_setting);
  while ((val & pencsub->adb_setting_polling_bits) != 0x0)
  {
    val = readl(tdev->bar2_virt + pencsub->slice_off + pencsub->adb_setting);
    msleep(1);
    loop--;
    if (loop == 0)
    {
      sn_pri(tdev, SN_ERR, "%s: %s adb_setting configuration error!\n", __func__, pencsub->name);
      break;
    }
  }

  // 3.
  val = readl(tdev->bar2_virt + pencsub->slice_off + pencsub->clock_ctrl);
  writel(val & (~(pencsub->clock_ctrl_bits)), tdev->bar2_virt + pencsub->slice_off + pencsub->clock_ctrl);

  val = readl(tdev->bar2_virt + pfpssub->slice_off + pfpssub->clock_ctrl);
  writel(val & (~(pfpssub->clock_ctrl_bits)), tdev->bar2_virt + pfpssub->slice_off + pfpssub->clock_ctrl);

  // 4.
  val = readl(tdev->bar2_virt + pencsub->slice_off + pencsub->sys_reset);
  writel(val & (~(pencsub->sys_reset_bits)), tdev->bar2_virt + pencsub->slice_off + pencsub->sys_reset);

  val = readl(tdev->bar2_virt + pfpssub->slice_off + pfpssub->sys_reset);
  writel(val & (~(pfpssub->sys_reset_bits)), tdev->bar2_virt + pfpssub->slice_off + pfpssub->sys_reset);

  sn_pri(tdev, SN_INF, "+++ %s %d over\n", __func__, __LINE__);
}

void sys_xav1_whole_subsys_release_soft_reset(struct sn_tranx_t *tdev, int slice)
{
  unsigned int val;
  unsigned int loop = LOOP_NUM;
  struct sys_ctrl_cfg_regs *pencsub = get_sys_configre_regs(tdev, slice, SYS_CTL_XENC);
  struct sys_ctrl_cfg_regs *pfpssub = get_sys_configre_regs(tdev, slice, SYS_CTL_XFPS);
  struct sys_ctrl_cfg_regs *pav1sub = get_sys_configre_regs(tdev, slice, SYS_CTL_XAV1);

  if (pencsub == NULL || pfpssub == NULL || pav1sub == NULL)
  {
    return;
  }

  // 1. system clock setting
  val = readl(tdev->bar2_virt + pencsub->slice_off + pencsub->clock_ctrl);
  writel(val | pencsub->clock_ctrl_bits, tdev->bar2_virt + pencsub->slice_off + pencsub->clock_ctrl);

  val = readl(tdev->bar2_virt + pfpssub->slice_off + pfpssub->clock_ctrl);
  writel(val | pfpssub->clock_ctrl_bits, tdev->bar2_virt + pfpssub->slice_off + pfpssub->clock_ctrl);

  // 2. system rst setting
  val = readl(tdev->bar2_virt + pencsub->slice_off + pencsub->sys_reset);
  writel(val | pencsub->sys_reset_bits, tdev->bar2_virt + pencsub->slice_off + pencsub->sys_reset);

  val = readl(tdev->bar2_virt + pfpssub->slice_off + pfpssub->sys_reset);
  writel(val | pfpssub->sys_reset_bits, tdev->bar2_virt + pfpssub->slice_off + pfpssub->sys_reset);

  // 3.
  val = readl(tdev->bar2_virt + pencsub->slice_off + pencsub->adb_setting);
  writel(val | pencsub->adb_setting_clear_bits, tdev->bar2_virt + pencsub->slice_off + pencsub->adb_setting);

  // 4.
  val = readl(tdev->bar2_virt + pencsub->slice_off + pencsub->adb_setting);
  while ((val & pencsub->adb_setting_polling_bits) != pencsub->adb_setting_polling_bits)
  {
    val = readl(tdev->bar2_virt + pencsub->slice_off + pencsub->adb_setting);
    msleep(1);
    loop--;
    if (loop == 0)
    {
      sn_pri(tdev, SN_ERR, "%s: %s adb configuration error!\n", __func__, pencsub->name);
      break;
    }
  }

  sn_pri(tdev, SN_INF, "+++ %s %d over\n", __func__, __LINE__);
}

enum SYS_CON_SUBSYS get_subsys_config_index(u32 subsys_base_addr)
{
	enum SYS_CON_SUBSYS ret;

	switch (subsys_base_addr) {
	case S1_VC8000D_A_OFF:
	case S2_VC8000D_A_OFF:
	case ONE_VF_S1_VC8000D_A_OFF:// the same to VF_VC8000D_A_OFF
	case ONE_VF_S2_VC8000D_A_OFF:
		ret = SYS_CTL_VCDA;
		break;
	case S1_VC8000D_B_OFF:
	case S2_VC8000D_B_OFF:
	case ONE_VF_S1_VC8000D_B_OFF:// the same to VF_VC8000D_B_OFF
	case ONE_VF_S2_VC8000D_B_OFF:
		ret = SYS_CTL_VCDB;
		break;
	case S1_VC8000E_OFF:
	case S1_VCE_IM_OFF:
	case S2_VCE_IM_OFF:
	case S2_VC8000E_OFF:
	case VF_VCE_IM_OFF:
	case VF_VC8000E_OFF:
	case ONE_VF_S2_VC8000E_OFF:
	case ONE_VF_S2_VCE_IM_OFF:
		ret = SYS_CTL_VCE;
		break;
	default:
		ret = SYS_CTL_MAX;
		break;
	}

	return ret;
}

int get_slice_by_subsys_base_addr(u32 base_addr)
{
	int ret = MAX_SLICE_NUM;

	switch (base_addr) {
	case S1_VC8000D_A_OFF:
	case S1_VC8000D_B_OFF:
	case S1_VC8000E_OFF:
		ret = 0;
		break;
	case S2_VC8000D_A_OFF:
	case S2_VC8000D_B_OFF:
	case S2_VC8000E_OFF:
		ret = 1;
		break;
	default:
		ret = MAX_SLICE_NUM;
		break;
	}

	return ret;
}
