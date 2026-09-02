/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 Verisilicon Inc.
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
#include <linux/module.h>

#include "common.h"
#include "riscv.h"

#define DEBUG_RISCV
#ifdef DEBUG_RISCV
#define DBG_PRINTK printk
#else
#define DBG_PRINTK(x...)
#endif

struct riscv_priv {
	struct sn_tranx_t *tdev;
};

int riscv_init(struct sn_tranx_t *tdev)
{
	unsigned long bar2_base;
	struct riscv_priv *triscv;

	if (!IS_PF(tdev)) {
		sn_pri(tdev, SN_DBG, "=== riscv: not support in VF. ===\n");
		return 0;
	}
	triscv = (struct riscv_priv *)kzalloc(sizeof(*triscv), GFP_KERNEL);
	if (!triscv)
		return -ENOMEM;
	tdev->modules[SN_MODULE_RISCV] = triscv;
	triscv->tdev = tdev;

	bar2_base = pci_resource_start(tdev->pdev, 2);

	DBG_PRINTK("Config SN_MODULE_RISCV begining...");
	// riscv start address
#ifdef SYS_RISCV_0
	DBG_PRINTK("Config SN_MODULE_RISCV start address\n");
	writel(0x8000000,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa0c); // CPU1_RESET_PC0_1
	writel(0x0,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa10); // CPU1_RESET_PC0_2
	DBG_PRINTK("HART0 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa10),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa0c));

	writel(0x8000000,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa34); // CPU1_RESET_PC1_1
	writel(0x0,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa38); // CPU1_RESET_PC1_2
	DBG_PRINTK("HART1 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa38),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa34));

	writel(0x8000000,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa5c); // CPU1_RESET_PC2_1
	writel(0x0,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa60); // CPU1_RESET_PC2_1
	DBG_PRINTK("HART2 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa60),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa5c));

	writel(0x8000000,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa84); // CPU1_RESET_PC3_1
	writel(0x0,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa88); // CPU1_RESET_PC3_1
	DBG_PRINTK("HART3 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa88),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa84));

	// riscv HARTID
	DBG_PRINTK("Config SN_MODULE_RISCV HARTID\n");
	writel(0x20,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa00); // CPU1_MHARTID0_1
	DBG_PRINTK("HARTID 0:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa00));
	writel(0x21,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa28); // CPU1_MHARTID1_1
	DBG_PRINTK("HARTID 1:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa28));
	writel(0x22,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa50); // CPU1_MHARTID2_1
	DBG_PRINTK("HARTID 2:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa50));
	writel(0x23,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa78); // CPU1_MHARTID3_1
	DBG_PRINTK("HARTID 3:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa78));

	// riscv set jtag id
	DBG_PRINTK("Config SN_MODULE_RISCV set jtag id\n");
	writel(0xdab17001, tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa08);
	DBG_PRINTK("Set jtag ID:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa08));
	writel(0xdbb17001, tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa30);
	DBG_PRINTK("Set jtag ID:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa30));
	writel(0xdcb17001, tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa58);
	DBG_PRINTK("Set jtag ID:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa58));
	writel(0xddb17001, tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa80);
	DBG_PRINTK("Set jtag ID:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa80));
#endif
#ifdef SYS_RISCV_1
	DBG_PRINTK("Config SN_MODULE_RISCV start address\n");
	writel(0x8000000,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa20); // CPU2_RESET_PC0_1
	writel(0x0,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa24); // CPU2_RESET_PC0_2
	DBG_PRINTK("HART0 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa24),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa20));

	writel(0x8000000,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa48); // CPU2_RESET_PC1_1
	writel(0x0,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa4c); // CPU2_RESET_PC1_2
	DBG_PRINTK("HART1 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa4c),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa48));

	writel(0x8000000,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa70); // CPU2_RESET_PC2_1
	writel(0x0,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa74); // CPU2_RESET_PC2_1
	DBG_PRINTK("HART2 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa74),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa70));

	writel(0x8000000,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa98); // CPU2_RESET_PC3_1
	writel(0x0,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa9c); // CPU2_RESET_PC3_1
	DBG_PRINTK("HART3 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa9c),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa98));

	// riscv HARTID
	DBG_PRINTK("Config SN_MODULE_RISCV HARTID\n");
	writel(0x20,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa14); // CPU2_MHARTID0_1
	DBG_PRINTK("HARTID 0:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa14));
	writel(0x21,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa3c); // CPU2_MHARTID1_1
	DBG_PRINTK("HARTID 1:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa3c));
	writel(0x22,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa64); // CPU2_MHARTID2_1
	DBG_PRINTK("HARTID 2:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa64));
	writel(0x23,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa8c); // CPU2_MHARTID3_1
	DBG_PRINTK("HARTID 3:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa8c));

	// riscv set jtag id
	DBG_PRINTK("Config SN_MODULE_RISCV set jtag id\n");
	writel(0xdab17001, tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa1c);
	DBG_PRINTK("Set jtag ID:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa1c));
	writel(0xdbb17001, tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa44);
	DBG_PRINTK("Set jtag ID:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa44));
	writel(0xdcb17001, tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa6c);
	DBG_PRINTK("Set jtag ID:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa6c));
	writel(0xddb17001, tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa94);
	DBG_PRINTK("Set jtag ID:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa94));
#endif

	sn_pri(tdev, SN_INF, "=== riscv: module initialize done. ===\n");
	return 0;
}
EXPORT_SYMBOL(riscv_init);

void riscv_release(struct sn_tranx_t *tdev)
{
	if (!IS_PF(tdev)) {
		sn_pri(tdev, SN_DBG, "=== riscv: not support in VF. ===\n");
	}
	else
		sn_pri(tdev, SN_DBG, "=== riscv: module removed ===\n");
}
EXPORT_SYMBOL(riscv_release);

static void riscv_power_on(int cluster, struct sn_tranx_t *tdev)
{
	unsigned int val;
	unsigned int loop = 800;
	DBG_PRINTK("=== Config SN_MODULE_RISCV power on ===\n");

	// riscv system clock setting
	DBG_PRINTK("Config SN_MODULE_RISCV clock\n");
	val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x86c + 0x4 +
		    0x7c * cluster);
	writel(val & ~(0x1 << 0), tdev->bar2_virt + TOP_SYS_CON_OFF + 0x86c +
					  0x4 + 0x7c * cluster); //PLL disbypass
	DBG_PRINTK("PLL bypass config:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x86c + 0x4 +
			 0x7c * cluster));

	// riscv system clock setting
	writel(0x1, tdev->bar2_virt + TOP_SYS_CON_OFF + 0x8 + 0x28 * cluster);
	DBG_PRINTK("clock enable:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x8 +
			 0x28 * cluster));

	// riscv system rst setting
	writel(0x1, tdev->bar2_virt + TOP_SYS_CON_OFF + 0x108 + 0xc * cluster);
	DBG_PRINTK("Config SN_MODULE_RISCV release rst:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x108 +
			 0xc * cluster));

	//TODO: riscv system security, no source

	// riscv system power up timer
	val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x700 + 0x4 * cluster);
	writel(val | (501 << 3),
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0x700 + 0x4 * cluster);

	// riscv power up THS1 ABR SCL subsystem
	val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x700 + 0x4 * cluster);
	writel(val & (~(0x1 << 0)),
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0x700 + 0x4 * cluster);

	// polling power up done
	while ((val & 0x2) != 0x2) {
		val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x700 +
			    0x4 * cluster);
		DBG_PRINTK("Polling:0x%x\n",
			   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x700 +
				 0x4 * cluster));
		msleep(1);
		loop--;
		if (!loop) {
			sn_pri(tdev, SN_ERR, "riscv: system power up error!\n");
			break;
		}
	}
	DBG_PRINTK("polling power up done\n");
	// riscv release ADB bus
	DBG_PRINTK("Config SN_MODULE_RISCV release ADB bus\n");
	writel(0x110001,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0x518 + 0x4 * cluster);
	DBG_PRINTK("release ADB BUS:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x518 +
			 0x4 * cluster));

	// polling ADB LPI status "1"
	while ((val & 0x20) != 0x20) {
		val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x518 +
			    0x4 * cluster);
		msleep(1);
		loop--;
		if (!loop) {
			sn_pri(tdev, SN_ERR,
			       "riscv: system ADB release error!\n");
			break;
		}
	}
	DBG_PRINTK("polling ADB LPI status \"1\" done\n");
}

static void riscv_power_off(int cluster, struct sn_tranx_t *tdev)
{
	unsigned int val;
	unsigned int loop = 800;
	DBG_PRINTK("=== Config SN_MODULE_RISCV power off ===\n");

	// riscv system clock setting
	DBG_PRINTK("Config SN_MODULE_RISCV clock\n");
	val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x86c + 0x4 +
		    0x7c * cluster);
	writel(val & ~(0x1 << 0), tdev->bar2_virt + TOP_SYS_CON_OFF + 0x86c +
					  0x4 + 0x7c * cluster); //PLL disbypass
	DBG_PRINTK("PLL bypass config:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x86c + 0x4 +
			 0x7c * cluster));

	// riscv system clock setting
	writel(0x1, tdev->bar2_virt + TOP_SYS_CON_OFF + 0x8 + 0x28 * cluster);
	DBG_PRINTK("clock enable:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x8 +
			 0x28 * cluster));

	// riscv release ADB bus
	DBG_PRINTK("Config SN_MODULE_RISCV release ADB bus\n");
	val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x518 + 0x4 * cluster);
	writel(val & (~0x1),
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0x518 + 0x4 * cluster);
	DBG_PRINTK("release ADB BUS:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x518 +
			 0x4 * cluster));

	// polling ADB LPI status "1"
	while ((val & 0x20) != 0x0) {
		val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x518 +
			    0x4 * cluster);
		msleep(1);
		loop--;
		if (!loop) {
			sn_pri(tdev, SN_ERR,
			       "riscv: system ADB release error!\n");
			break;
		}
	}
	DBG_PRINTK("polling ADB LPI status \"0\" done\n");

	// riscv system power up timer
	val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x700 + 0x4 * cluster);
	writel(val | (501 << 3),
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0x700 + 0x4 * cluster);

	// riscv power up THS1 ABR SCL subsystem
	val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x700 + 0x4 * cluster);
	writel(val | (0x1 << 0),
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0x700 + 0x4 * cluster);

	// polling power up done
	while ((val & 0x4) != 0x4) {
		val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x700 +
			    0x4 * cluster);
		DBG_PRINTK("Polling:0x%x\n",
			   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x700 +
				 0x4 * cluster));
		msleep(1);
		loop--;
		if (!loop) {
			sn_pri(tdev, SN_ERR, "riscv: system power up error!\n");
			break;
		}
	}
	DBG_PRINTK("polling power up done\n");

	// riscv system rst setting
	writel(0x0, tdev->bar2_virt + TOP_SYS_CON_OFF + 0x108 + 0xc * cluster);
	DBG_PRINTK("Config SN_MODULE_RISCV release rst:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x108 +
			 0xc * cluster));
}

static void riscv_software_reset(int cluster, struct sn_tranx_t *tdev)
{
	unsigned int val;
	unsigned int loop = 800;
	DBG_PRINTK("=== Config SN_MODULE_RISCV software reset ===\n");

	// riscv release ADB bus
	DBG_PRINTK("Config SN_MODULE_RISCV release ADB bus\n");
	val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x518 + 0x4 * cluster);
	writel(val & (~0x1),
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0x518 + 0x4 * cluster);
	DBG_PRINTK("release ADB BUS:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x518 +
			 0x4 * cluster));

	// polling ADB LPI status "0"
	while ((val & 0x20) != 0x0) {
		val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x518 +
			    0x4 * cluster);
		msleep(1);
		loop--;
		if (!loop) {
			sn_pri(tdev, SN_ERR,
			       "riscv: system ADB release error!\n");
			break;
		}
	}
	DBG_PRINTK("polling ADB LPI status \"0\" done\n");

	// riscv system clock setting
	writel(0x0, tdev->bar2_virt + TOP_SYS_CON_OFF + 0x8 + 0x28 * cluster);
	DBG_PRINTK("clock enable:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x8 +
			 0x28 * cluster));

	// riscv system rst setting
	val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x108 + 0xc * cluster);
	writel(val & (~0x1),
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0x108 + 0xc * cluster);
	DBG_PRINTK("Config SN_MODULE_RISCV release rst:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x108 +
			 0xc * cluster));
}

static void riscv_release_software_reset(int cluster, struct sn_tranx_t *tdev)
{
	unsigned int val;
	unsigned int loop = 800;
	DBG_PRINTK("=== Config SN_MODULE_RISCV release software reset ===\n");
	// riscv system clock setting
	writel(0x1, tdev->bar2_virt + TOP_SYS_CON_OFF + 0x8 + 0x28 * cluster);
	DBG_PRINTK("clock enable:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x8 +
			 0x28 * cluster));

	// riscv system rst setting
	writel(0x1, tdev->bar2_virt + TOP_SYS_CON_OFF + 0x108 + 0xc * cluster);
	DBG_PRINTK("Config SN_MODULE_RISCV release rst:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x108 +
			 0xc * cluster));

	// riscv release ADB bus
	DBG_PRINTK("Config SN_MODULE_RISCV release ADB bus\n");
	val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x518 + 0x4 * cluster);
	writel(val | 0x1,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0x518 + 0x4 * cluster);
	DBG_PRINTK("release ADB BUS:0x%x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x518 +
			 0x4 * cluster));

	// polling ADB LPI status "1"
	while ((val & 0x20) != 0x20) {
		val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x518 +
			    0x4 * cluster);
		msleep(1);
		loop--;
		if (!loop) {
			sn_pri(tdev, SN_ERR,
			       "riscv: system ADB release error!\n");
			break;
		}
	}
	DBG_PRINTK("polling ADB LPI status \"1\" done\n");
}

static void riscv_release_release_addr(int cluster, struct release_addr_t *pc_val,
				struct sn_tranx_t *tdev)
{
#ifdef SYS_RISCV_0
	DBG_PRINTK("Config SN_MODULE_RISCV release address\n");
	DBG_PRINTK("pc_h = 0x%x\n", pc_val->pc_h);
	DBG_PRINTK("pc_l = 0x%x\n", pc_val->pc_l);
	writel(pc_val->pc_l,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa0c); // CPU1_RESET_PC0_1
	writel(pc_val->pc_h,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa10); // CPU1_RESET_PC0_2
	DBG_PRINTK("HART0 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa10),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa0c));

	writel(pc_val->pc_l,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa34); // CPU1_RESET_PC1_1
	writel(pc_val->pc_h,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa38); // CPU1_RESET_PC1_2
	DBG_PRINTK("HART1 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa38),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa34));

	writel(pc_val->pc_l,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa5c); // CPU1_RESET_PC2_1
	writel(pc_val->pc_h,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa60); // CPU1_RESET_PC2_1
	DBG_PRINTK("HART2 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa60),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa5c));

	writel(pc_val->pc_l,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa84); // CPU1_RESET_PC3_1
	writel(pc_val->pc_h,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa88); // CPU1_RESET_PC3_1
	DBG_PRINTK("HART3 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa88),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa84));
#endif
#ifdef SYS_RISCV_1
	DBG_PRINTK("Config SN_MODULE_RISCV start address\n");
	writel(pc_val->pc_l,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa20); // CPU2_RESET_PC0_1
	writel(pc_val->pc_h,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa24); // CPU2_RESET_PC0_2
	DBG_PRINTK("HART0 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa24),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa20));

	writel(pc_val->pc_l,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa48); // CPU2_RESET_PC1_1
	writel(pc_val->pc_h,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa4c); // CPU2_RESET_PC1_2
	DBG_PRINTK("HART1 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa4c),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa48));

	writel(pc_val->pc_l,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa70); // CPU2_RESET_PC2_1
	writel(pc_val->pc_h,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa74); // CPU2_RESET_PC2_1
	DBG_PRINTK("HART2 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa74),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa70));

	writel(pc_val->pc_l,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa98); // CPU2_RESET_PC3_1
	writel(pc_val->pc_h,
	       tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa9c); // CPU2_RESET_PC3_1
	DBG_PRINTK("HART3 start address:0x%x %x\n",
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa9c),
		   readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0xa98));
#endif
}

long riscv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg,
		 struct sn_tranx_t *tdev)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	struct release_addr_t release_addr;
	struct riscv_priv *triscv = tdev->modules[SN_MODULE_RISCV];

	switch (cmd) {
	case RISCV_0_IOCS_POWER_ON:
		riscv_power_on(0, triscv->tdev);
		break;
	case RISCV_0_IOCS_POWER_OFF:
		riscv_power_off(0, triscv->tdev);
		break;
	case RISCV_0_IOCS_SOFT_RESET:
		riscv_software_reset(0, triscv->tdev);
		break;
	case RISCV_0_IOCS_RESET_REALEASE:
		riscv_release_software_reset(0, triscv->tdev);
		break;
	case RISCV_0_IOCS_RELEASE_ADDR:
		if (copy_from_user(&release_addr, argp, sizeof(release_addr))) {
			sn_pri(tdev, SN_ERR,
			       "pc: get pc - copy_from_user failed.\n");
			return -EFAULT;
		}
		riscv_release_release_addr(0, &release_addr, triscv->tdev);
		break;
	case RISCV_1_IOCS_POWER_ON:
		riscv_power_on(1, triscv->tdev);
		break;
	case RISCV_1_IOCS_POWER_OFF:
		riscv_power_off(1, triscv->tdev);
		break;
	case RISCV_1_IOCS_SOFT_RESET:
		riscv_software_reset(1, triscv->tdev);
		break;
	case RISCV_1_IOCS_RESET_REALEASE:
		riscv_release_software_reset(1, triscv->tdev);
		break;
	case RISCV_1_IOCS_RELEASE_ADDR:
		if (copy_from_user(&release_addr, argp, sizeof(release_addr))) {
			sn_pri(tdev, SN_ERR,
			       "pc: get pc - copy_from_user failed.\n");
			return -EFAULT;
		}
		riscv_release_release_addr(1, &release_addr, triscv->tdev);

		break;
	default:
		sn_pri(tdev, SN_ERR, "riscv: ioctl cmd:0x%x is error\n", cmd);
	}

	return ret;
}
EXPORT_SYMBOL(riscv_ioctl);

// MODULE_LICENSE("GPL");
// MODULE_AUTHOR("Yijie Wang <Yijie.Wang@verisilicon.com>");
// MODULE_DESCRIPTION("riscv driver");
// MODULE_VERSION("V0.0.1");
