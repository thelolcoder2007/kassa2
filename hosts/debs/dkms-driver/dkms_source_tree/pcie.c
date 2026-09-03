// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 Verisilicon Inc.
 *
 * This is pcie configuration driver for Linux.
 */

#include <linux/version.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <linux/errno.h>
#include <linux/pagemap.h>
#include <linux/aer.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/version.h>

#include <asm/set_memory.h>

#include "common.h"
#include "hdma.h"
#include "pcie.h"
#include "hw_monitor.h"
#include "transcoder.h"
#include "error_notify.h"

#ifdef TEST_MSIX_IRQ
irqreturn_t test_msix_isr(int irq, void *data)
{
	struct sn_tranx_t *tdev = data;
	int start_irq;

	start_irq = pci_irq_vector(tdev->pdev, 0);
	sn_pri(tdev, SN_ERR, "test: msi_msix_test_isr irq:%d - index:%d\n", irq,
	       irq - start_irq);
	return IRQ_HANDLED;
}

static int test_msix_intr(struct sn_tranx_t *tdev)
{
	int i, ret;
	u32 irq_num;

	for (i = 0; i < MAX_MSIX_CNT; i++) {
		irq_num = tdev->msix_entries[i].vector;
		ret = request_irq(irq_num, test_msix_isr,
				  IRQF_SHARED | IRQF_NO_THREAD, "only_test",
				  (void *)tdev);
		if (ret != 0) {
			sn_pri(tdev, SN_ERR,
			       "core: msi_msix_int_test request irq:%d failed\n",
			       i);
		}
	}

	return 0;
}

static void test_msix_free(struct sn_tranx_t *tdev)
{
	int i;
	u32 irq_num;

	/* free the IRQ */
	for (i = 0; i < MAX_MSIX_CNT; i++) {
		irq_num = tdev->msix_entries[i].vector;
		free_irq(irq_num, (void *)tdev);
	}
}
#endif

static ssize_t flr_mode_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	u32 flr, val;

	if (count == 0)
		return 0;

	if (sscanf(buf, "%d", &flr) != 1) {
		sn_pri(tdev, SN_ERR, "veri: not in hex or decimal form.\n");
		return count;
	}

	if (flr == 1) {
		val = readl(FLR_HANDLING_CON_STUS + tdev->bar2_virt);
		sn_pri(tdev, SN_ERR,
		       "veri: default FLR_HANDLING_CON_STUS:0x%x\n", val);
		writel(val | 0x1, FLR_HANDLING_CON_STUS + tdev->bar2_virt);

		val = readl(FLR_HANDLING_CON_STUS + tdev->bar2_virt);
		sn_pri(tdev, SN_ERR, "veri: cfg FLR_HANDLING_CON_STUS:0x%x\n",
		       val);
	} else if (flr == 0) {
		val = readl(FLR_HANDLING_CON_STUS + tdev->bar2_virt);
		sn_pri(tdev, SN_ERR,
		       "veri: default FLR_HANDLING_CON_STUS:0x%x\n", val);
		writel(val & 0xfffffffe,
		       FLR_HANDLING_CON_STUS + tdev->bar2_virt);

		val = readl(FLR_HANDLING_CON_STUS + tdev->bar2_virt);
		sn_pri(tdev, SN_ERR, "veri: cfg FLR_HANDLING_CON_STUS:0x%x\n",
		       val);
	} else
		sn_pri(tdev, SN_ERR, "veri: not support flr val:0x%x.\n", flr);

	return count;
}

static ssize_t flr_mode_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	unsigned int val;
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	int pos;

	val = readl(FLR_HANDLING_CON_STUS + tdev->bar2_virt);

	pos = sprintf(buf, "echo 1: software; 0: hardware\n");
	pos += sprintf(pos + buf, "0x%x - %s\n", val,
		       (val & 0x1) ? "software" : "hardware");

	return pos;
}

static ssize_t err_bypass_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	u32 bypass;

	if (count == 0)
		return 0;

	if (sscanf(buf, "%d", &bypass) != 1) {
		sn_pri(tdev, SN_ERR, "veri: not in hex or decimal form.\n");
		return count;
	}

	if (bypass == 1) {
		writel(0x7ffff,
		       THS1_APB_AXI_ERR_BYPASS_CON_STUS + tdev->bar2_virt);
		writel(0x7ffff,
		       THS2_APB_AXI_ERR_BYPASS_CON_STUS + tdev->bar2_virt);
	} else if (bypass == 0) {
		writel(0x0, THS1_APB_AXI_ERR_BYPASS_CON_STUS + tdev->bar2_virt);
		writel(0x0, THS2_APB_AXI_ERR_BYPASS_CON_STUS + tdev->bar2_virt);
	} else {
		sn_pri(tdev, SN_ERR, "veri: not support bypass val:0x%x.\n",
		       bypass);
    }

	return count;
}

static ssize_t err_bypass_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	unsigned int val1, val2;
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	int pos;

	val1 = readl(THS1_APB_AXI_ERR_BYPASS_CON_STUS + tdev->bar2_virt);
	val2 = readl(THS2_APB_AXI_ERR_BYPASS_CON_STUS + tdev->bar2_virt);

	pos = sprintf(buf, "echo 1: bypass;  0: no bypass\n");
	pos += sprintf(pos + buf, "s1:0x%x - %s  s2:0x%x - %s\n", val1,
		       (val1 == 0x7ffff) ? "bypass" : "no-bypass", val2,
		       (val2 == 0x7ffff) ? "bypass" : "no-bypass");
	return pos;
}

static ssize_t flr_chk_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	u32 data;

	if (count == 0)
		return 0;

	if (sscanf(buf, "%x", &data) != 1) {
		sn_pri(tdev, SN_ERR, "veri: not in hex or decimal form.\n");
		return count;
	}

	writel(data, TOP_SYS_CON_OFF + 0x3b0 + tdev->bar2_virt); //debug reg
	writel(data, SRAM_SC_OFF + tdev->bar2_virt); //zsp sram

	writel(data, VF1_TO_PF_MAILBOX + tdev->bar2_virt);
	writel(data, VF2_TO_PF_MAILBOX + tdev->bar2_virt);

	writel(data, S1_SYS_CON_OFF + tdev->bar2_virt);
	writel(data, S2_SYS_CON_OFF + tdev->bar2_virt);

	return count;
}

static ssize_t flr_chk_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	unsigned int val1, val2;
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	int pos = 0;

	val1 = readl(TOP_SYS_CON_OFF + 0x3b0 + tdev->bar2_virt);
	pos += sprintf(buf + pos, "debug reg0: 0x%x\n", val1);

	val1 = readl(SRAM_SC_OFF + tdev->bar2_virt);
	pos += sprintf(buf + pos, "zsp-sram: 0x%x\n", val1);

	val1 = readl(VF1_TO_PF_MAILBOX + tdev->bar2_virt);
	val2 = readl(VF2_TO_PF_MAILBOX + tdev->bar2_virt);
	pos += sprintf(buf + pos, "mailbox: s1:0x%x  s2:0x%x\n", val1, val2);

	val1 = readl(S1_SYS_CON_OFF + tdev->bar2_virt);
	val2 = readl(S2_SYS_CON_OFF + tdev->bar2_virt);
	pos += sprintf(buf + pos, "sys_con: s1:0x%x  s2:0x%x\n", val1, val2);

	return pos;
}

static ssize_t isolate_vf_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	u32 data2;

	if (count == 0)
		return 0;

	if (sscanf(buf, "%x", &data2) != 2) {
		sn_pri(tdev, SN_ERR, "veri: not in hex or decimal form.\n");
		return -EFAULT;
	}

	sn_pri(tdev, SN_DBG, "veri: data2=0x%x\n", data2);
	if (data2 & 0x1)
		data2 |= (1 << 2);
	if (data2 & 0x2)
		data2 |= (1 << 3);
	writel(data2, THS_ISOLATE_SEL + tdev->bar2_virt);

	return count;
}

static ssize_t isolate_vf_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	unsigned int val1, val2;
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	int pos = 0;

	val1 = readl(VF_MODE_SEL + tdev->bar2_virt);
	pos += sprintf(buf + pos, "0x%x - %s\n", val1,
		(val1 & 0x1) ? "one VF" : "two VF");

	val2 = readl(THS_ISOLATE_SEL + tdev->bar2_virt);
	pos += sprintf(buf + pos, "0x%x - %s, fps_limit:%d \n", val2,
		(val2 & 0x1) ? "isolate" : "no-isolate",
		(val2 & 0x2) >> 1);

	return pos;
}

static ssize_t trig_zsp_int_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	u32 val;

	if (count == 0)
		return 0;

	if (sscanf(buf, "%d", &val) != 1) {
		sn_pri(tdev, SN_ERR, "veri: not in hex or decimal form.\n");
		return -EFAULT;
	}

	if (val == 1) {
		trigger_zsp_interrupt(tdev);
	}

	return count;
}

static DEVICE_ATTR_WO(trig_zsp_int);
static DEVICE_ATTR_RW(isolate_vf);
static DEVICE_ATTR_RW(flr_chk);
static DEVICE_ATTR_RW(err_bypass);
static DEVICE_ATTR_RW(flr_mode);

static int translate_link_status(u32 val, char *buf)
{
	int pos = 0;
	if (((val & 0xF0000) >> 16) == 1)
		pos += sprintf(buf + pos, "Speed = 2.5GT/s, ");
	else if (((val & 0xF0000) >> 16) == 2)
		pos += sprintf(buf + pos, "Speed = 5GT/s, ");
	else if (((val & 0xF0000) >> 16) == 3)
		pos += sprintf(buf + pos, "Speed = 8GT/s, ");
	else if (((val & 0xF0000) >> 16) == 4)
		pos += sprintf(buf + pos, "Speed = 16GT/s, ");
	else if (((val & 0xF0000) >> 16) == 5)
		pos += sprintf(buf + pos, "Speed = 32GT/s, ");
	else
		pos += sprintf(buf + pos, "Speed error, val=0x%x!\n", val);

	if (((val & 0x3F00000) >> 20) == 1)
		pos += sprintf(buf + pos, "Width = x1\n");
	else if (((val & 0x3F00000) >> 20) == 2)
		pos += sprintf(buf + pos, "Width = x2\n");
	else if (((val & 0x3F00000) >> 20) == 4)
		pos += sprintf(buf + pos, "Width = x4\n");
	else if (((val & 0x3F00000) >> 20) == 8)
		pos += sprintf(buf + pos, "Width = x8\n");
	else
		pos += sprintf(buf + pos, "Width error, val=0x%x!\n", val);
	return pos;
}

static ssize_t link_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u32 val;
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct pci_dev *pcie_dev = tdev->pdev;

	if (IS_PF(tdev)) {
		pci_read_config_dword(pcie_dev, 0x80, &val);
	}
	else {
		mailbox_vf_get_msg(tdev, EVENT_VF2PF_GET_LINK_STATUS, &val, 1);
	}

	return translate_link_status(val, buf);
}

static ssize_t bus_id_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct pci_dev *pcie_dev = tdev->pdev;

	return sprintf(buf, "%04x:%02x:%02x.%d\n", pci_domain_nr(pcie_dev->bus),
		       pcie_dev->bus->number, PCI_SLOT(pcie_dev->devfn),
		       PCI_FUNC(pcie_dev->devfn));
}

static ssize_t vf_count_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	int pos = 0;

	if (tdev->vf_max_count == 1)
		pos += sprintf(buf + pos, "one VF mode\n");
	else if (tdev->vf_max_count == 2)
		pos += sprintf(buf + pos, "two VFs mode\n");
	else
		pos += sprintf(buf + pos, "VF count:%d error\n",
			       tdev->vf_max_count);

	return pos;
}

static ssize_t pw_cold_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct pci_dev *pdev = tdev->pdev;
	int data;
	int ret;

	if (count == 0)
		return 0;

	if (sscanf(buf, "%d", &data) != 1) {
		sn_pri(tdev, SN_ERR, "pcie: not in hex or decimal form.\n");
		return count;
	}

	if ((data > 1) || (data < 0)) {
		sn_pri(tdev, SN_ERR, "pcie: input data:%d error.\n", data);
		count = 0;
	} else if (data == 1) {
		sn_pri(tdev, SN_ERR, "pcie: pcie will enter D3cold mode.\n");
		pci_save_state(pdev);
		pci_disable_device(pdev);
		pci_set_power_state(pdev, PCI_D3cold);
	} else {
		sn_pri(tdev, SN_ERR, "pcie: pcie will enter D0 mode.\n");
		pci_set_power_state(pdev, PCI_D0);
		pci_restore_state(pdev);
		ret = pci_enable_device(pdev);
		if (ret)
			return ret;
		pci_set_master(pdev);
	}

	return count;
}

static ssize_t send_date_to_vf1_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	u32 data;
	int ret = 0;
	if (count == 0)
		return 0;

	if (sscanf(buf, "%u", &data) != 1) {
		sn_pri(tdev, SN_ERR, "%s: not in hex or decimal form.\n", __func__);
		return count;
	}

	ret = mailbox_pf_send_msg(tdev, &data, 1, PF_TO_VF1);
	if (ret) {
		sn_pri(tdev, SN_ERR, "%s: send has error.\n", __func__);
		return 0;
	}
	return count;
}

static ssize_t send_date_to_vf2_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	u32 data;
	int ret = 0;
	if (count == 0)
		return 0;

	if (sscanf(buf, "%u", &data) != 1) {
		sn_pri(tdev, SN_ERR, "%s: not in hex or decimal form.\n", __func__);
		return count;
	}

	ret = mailbox_pf_send_msg(tdev, &data, 1, PF_TO_VF2);
	if (ret) {
		sn_pri(tdev, SN_ERR, "%s: send has error.\n", __func__);
		return 0;
	}

	return count;
}

static DEVICE_ATTR_RO(link_status);
static DEVICE_ATTR_RO(bus_id);
static DEVICE_ATTR_RO(vf_count);
static DEVICE_ATTR_WO(pw_cold);
static DEVICE_ATTR_WO(send_date_to_vf1);
static DEVICE_ATTR_WO(send_date_to_vf2);

static struct attribute *trans_pcie_pf_sysfs_entries[] = {
	&dev_attr_trig_zsp_int.attr,
	&dev_attr_isolate_vf.attr,
	&dev_attr_flr_mode.attr,
	&dev_attr_err_bypass.attr,
	&dev_attr_flr_chk.attr,

	&dev_attr_pw_cold.attr,
	&dev_attr_vf_count.attr,
	&dev_attr_send_date_to_vf1.attr,
	&dev_attr_send_date_to_vf2.attr,
	NULL
};

static struct attribute_group trans_pcie_pf_attribute_group = {
	.name = NULL,
	.attrs = trans_pcie_pf_sysfs_entries,
};

static struct attribute *trans_pcie_sysfs_entries[] = {
	&dev_attr_link_status.attr,
	&dev_attr_bus_id.attr,
	NULL
};

static struct attribute_group trans_pcie_attribute_group = {
	.name = NULL,
	.attrs = trans_pcie_sysfs_entries,
};

static void get_vf_info(struct sn_tranx_t *tdev)
{
	int pos;
	u16 ctrl, total, initial, offset, stride;

	pos = pci_find_ext_capability(tdev->pdev, PCI_EXT_CAP_ID_SRIOV);
	if (!pos) {
		sn_pri(tdev, SN_ERR,
		       "pcie: failed to find SRIOV capability in device\n");
		return;
	}
	sn_pri(tdev, SN_DBG, "pcie: sriov ext pos 0x%x\n", pos);

	pci_read_config_word(tdev->pdev, pos + PCI_SRIOV_CTRL, &ctrl);
	pci_read_config_word(tdev->pdev, pos + PCI_SRIOV_TOTAL_VF, &total);
	pci_read_config_word(tdev->pdev, pos + PCI_SRIOV_INITIAL_VF, &initial);
	pci_read_config_word(tdev->pdev, pos + PCI_SRIOV_VF_OFFSET, &offset);
	pci_read_config_word(tdev->pdev, pos + PCI_SRIOV_VF_STRIDE, &stride);
	sn_pri(tdev, SN_DBG,
	       "pcie: iov: ctrl=0x%x, total=0x%x, initial=0x%x, offset=0x%x, stride=0x%x\n",
	       ctrl, total, initial, offset, stride);

	tdev->vf_max_count = 1;
}

void enable_interrupt_data_path(struct sn_tranx_t *tdev)
{
	sn_wr_b2(tdev, 0x940, 0x9ffff1);
	sn_pri(tdev, SN_DBG, "pcie: 0x940:0x%x.\n", sn_rd_b2(tdev, 0x940));

	sn_wr_b2(tdev, 0x944, 0x0);
	sn_pri(tdev, SN_DBG, "pcie: 0x944:0x%x.\n", sn_rd_b2(tdev, 0x944));

	sn_wr_b2(tdev, PCIE_GLUE_LOGIC_OFF + 0x20, 0x0);
	sn_pri(tdev, SN_DBG, "pcie: PCIE_GLUE_LOGIC_OFF+0x20:0x%x.\n",
	       sn_rd_b2(tdev, PCIE_GLUE_LOGIC_OFF + 0x20));

	sn_wr_b2(tdev, PCIE_GLUE_LOGIC_OFF + 0x24, 0x0);
	sn_pri(tdev, SN_DBG, "pcie: PCIE_GLUE_LOGIC_OFF+0x24:0x%x.\n",
	       sn_rd_b2(tdev, PCIE_GLUE_LOGIC_OFF + 0x24));
}

static u8 get_vf_mode(struct sn_tranx_t *tdev)
{
	u8 mode = 0;
	if (IS_PF(tdev))
		mode = PF_MODE;
	else {
		if (tdev->vf_index == VF2_INDEX) {
			mode = TWO_VF_MODE;
		}
		else {
			mode = ONE_VF_MODE;
		}
	}
	return mode;
}

void disable_interrupt_data_path(struct sn_tranx_t *tdev)
{
}

/*
 * enable pcie, get pcie region address and size,
 * config (BARn)region mapping, request msix/msi number,
 * map bar space.
 */
int sn_pci_init(struct sn_tranx_t *tdev)
{
	int ret, i;
	resource_size_t bar4_base, bar2_base, bar0_base;
	u32 bar4_len, bar2_len, bar0_len;
#ifdef FLR_HARDWARE
	u32 val;
#endif

	ret = pci_enable_device(tdev->pdev);
	if (ret) {
		sn_pri(tdev, SN_ERR,
		       "pcie: Unable to Enable PCIe device, ret:%d.\n", ret);
		goto out;
	}

	pci_set_master(tdev->pdev);
	if (dma_set_mask_and_coherent(&tdev->pdev->dev, DMA_BIT_MASK(64))) {
		sn_pri(tdev, SN_ERR, "pcie: Set pci dma mask 64 failed.\n");
		goto out_disable_pci;
	}
	if (IS_PF(tdev)) {
#ifdef CONFIG_PCIEAR
		ret = pci_aer_init(tdev->pdev);
		if (ret) {
			sn_pri(tdev, SN_INF,
			       "pcie: PCIe error reporting failed,ret=%d.\n", ret);
		}
#endif
		pci_write_config_byte(tdev->pdev, 0x80, 0x40); // disable ASPM L1
	}
	/* get every regions information */
	bar0_base = pci_resource_start(tdev->pdev, 0);
	bar0_len = pci_resource_len(tdev->pdev, 0);
	bar2_base = pci_resource_start(tdev->pdev, 2);
	bar2_len = pci_resource_len(tdev->pdev, 2);
	bar4_base = pci_resource_start(tdev->pdev, 4);
	bar4_len = pci_resource_len(tdev->pdev, 4);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
	tdev->bar0_virt = ioremap(bar0_base, bar0_len);
#else
	tdev->bar0_virt = ioremap_nocache(bar0_base, bar0_len);
#endif
	if (!tdev->bar0_virt) {
		sn_pri(tdev, SN_ERR, "pcie: ioremap bar0 failed.\n");
		goto out_disable_err_report;
	}

	if (IS_PF(tdev)) {
		get_vf_info(tdev);
	}
	tdev->bar2_virt = ioremap(bar2_base, bar2_len);
	tdev->bar4_virt = ioremap(bar4_base, bar4_len);

	set_memory_wc((uint64_t) tdev->bar4_virt, bar4_len / PAGE_SIZE);
	{
		uint8_t* buf = kvmalloc(1024 * 1024, GFP_KERNEL);
		ktime_t startNs, elapsedNs;
		uint32_t writeSpeed, readSpeed;
		for (i = 0; i < 1024 * 1024; ++i) {
			buf[i] = i;
		}
		startNs = ktime_get();
		memcpy((uint8_t*) tdev->bar4_virt + 0x11900000, buf, 1024 * 1024);

		elapsedNs = ktime_get() - startNs;
		writeSpeed = 1000000000ull / elapsedNs;
		startNs = ktime_get();
		memcpy(buf, (uint8_t*) tdev->bar4_virt + 0x11900000, 1024 * 1024);
		elapsedNs = ktime_get() - startNs;
		readSpeed = 1000000000ull / elapsedNs;
		sn_pri(tdev, SN_INF, "BAR4: write %lu MB/s read %lu MB/s\n", writeSpeed, readSpeed);
		kvfree(buf);
	}
	tdev->bar4_base = bar4_base;
	tdev->bar4_size = bar4_len;
	if (!tdev->bar2_virt) {
		sn_pri(tdev, SN_ERR, "pcie: failed to ioremap bar2.\n");
		goto out_unmap_bar0;
	}

	if (IS_PF(tdev))
		tdev->vf_index = PF_INDEX;
	else
		tdev->vf_index = readl(tdev->bar2_virt + VF_ID_CON_STUS);

	tdev->pf_vf_mode = get_vf_mode(tdev);

	for (i = 0; i < MAX_MSIX_CNT; i++)
		tdev->msix_entries[i].entry = i;

	ret = pci_enable_msix_range(tdev->pdev, tdev->msix_entries,
				    MIN_MSIX_CNT, MAX_MSIX_CNT);
	if (ret == MAX_MSIX_CNT) {
		sn_pri(tdev, SN_DBG,
		       "pcie: Have requested msix irq number:%d.\n", ret);
	} else {
		pci_disable_msix(tdev->pdev);
		sn_pri(tdev, SN_ERR,
		       "pcie: allocate msix irq vectors failed,ret=%d.\n", ret);
		goto out_free_irq;
	}

	if (IS_PF(tdev))
	{
		ret = sysfs_create_group(&tdev->misc_dev->this_device->kobj,
				 &trans_pcie_pf_attribute_group);
		if (ret) {
			sn_pri(tdev, SN_ERR,
				"pcie: failed to create sysfs device attributes in PF\n");
			goto out_free_irq;
		}
	}

	ret = sysfs_create_group(&tdev->misc_dev->this_device->kobj,
				 &trans_pcie_attribute_group);
	if (ret) {
		sn_pri(tdev, SN_ERR,
			"pcie: failed to create sysfs device attributes\n");
		goto out_free_irq;
	}

#ifdef TEST_MSIX_IRQ
	test_msix_intr(tdev);
#endif

	return 0;

out_free_irq:
	pci_disable_msix(tdev->pdev);
	iounmap(tdev->bar2_virt);
out_unmap_bar0:
	iounmap(tdev->bar0_virt);
out_disable_err_report:
#ifdef CONFIG_PCIEAR
	pci_aer_exit(tdev->pdev);
#endif
	pci_clear_master(tdev->pdev);
out_disable_pci:
	pci_disable_device(tdev->pdev);
out:
	return -EFAULT;
}

long sn_pci_ioctl(struct file *filp, unsigned int cmd, unsigned long arg,
		  struct sn_tranx_t *tdev)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;
	struct bar_info bar_inf;

	switch (cmd) {
	case SN_TRANX_GET_BARADDR:
		if (copy_from_user(&bar_inf, argp, sizeof(bar_inf))) {
			sn_pri(tdev, SN_ERR,
			       "pcie: GET_BARADDR copy_from_user failed.\n");
			return -EFAULT;
		}

		if (bar_inf.bar_num == 0) {
			bar_inf.bar_addr = pci_resource_start(tdev->pdev, 0);
			bar_inf.bar_size = pci_resource_len(tdev->pdev, 0);
		} else if (bar_inf.bar_num == 2) {
			bar_inf.bar_addr = pci_resource_start(tdev->pdev, 2);
			bar_inf.bar_size = pci_resource_len(tdev->pdev, 2);
		} else if (bar_inf.bar_num == 4) {
			bar_inf.bar_addr = pci_resource_start(tdev->pdev, 4);
			bar_inf.bar_size = pci_resource_len(tdev->pdev, 4);
		}
		if (copy_to_user(argp, &bar_inf, sizeof(bar_inf))) {
			sn_pri(tdev, SN_ERR,
			       "pcie: GET_BARADDR copy_to_user failed.\n");
			return -EFAULT;
		}
		break;
	default:
		sn_pri(tdev, SN_ERR, "pcie: %s, cmd:0x%x is error.\n", __func__,
		       cmd);
		ret = -EINVAL;
	}

	return ret;
}

void sn_pci_release(struct sn_tranx_t *tdev)
{
#ifdef TEST_MSIX_IRQ
	test_msix_free(tdev);
#endif

	set_memory_wb((uint64_t) tdev->bar4_virt, tdev->bar4_size / PAGE_SIZE);

	if (IS_PF(tdev) && pci_num_vf(tdev->pdev)) {
		pci_disable_sriov(tdev->pdev);
	}

	pci_disable_msix(tdev->pdev);
	if (tdev->bar0_virt)
		iounmap(tdev->bar0_virt);
	if (tdev->bar2_virt)
		iounmap(tdev->bar2_virt);
	if (tdev->bar4_virt)
		iounmap(tdev->bar4_virt);
	if (IS_PF(tdev)) {
#ifdef CONFIG_PCIEAR
		pci_aer_exit(tdev->pdev);
#endif
		pci_write_config_byte(tdev->pdev, 0x80, 0x42); // re-enable ASPM L1
	}

	pci_clear_master(tdev->pdev);
	if (IS_PF(tdev))
		sysfs_remove_group(&tdev->misc_dev->this_device->kobj,
				   &trans_pcie_pf_attribute_group);
	sysfs_remove_group(&tdev->misc_dev->this_device->kobj,
				   &trans_pcie_attribute_group);

	sn_pri(tdev, SN_DBG, "pcie: remove module done.\n");
}

/* pcie error handler callbacks */
pci_ers_result_t sn_err_detected(struct pci_dev *pdev,
				 pci_channel_state_t state)
{
	struct sn_tranx_t *tdev = pci_get_drvdata(pdev);

	sn_pri(tdev, SN_ERR, "pcie: %s.\n", __func__);
	return PCI_ERS_RESULT_NEED_RESET;
}

pci_ers_result_t sn_slot_reset(struct pci_dev *pdev)
{
	struct sn_tranx_t *tdev = pci_get_drvdata(pdev);

	sn_pri(tdev, SN_ERR, "pcie: %s.\n", __func__);
	return PCI_ERS_RESULT_RECOVERED;
}

void sn_resume(struct pci_dev *pdev)
{
	struct sn_tranx_t *tdev = pci_get_drvdata(pdev);

	sn_pri(tdev, SN_ERR, "pcie: %s.\n", __func__);
}

void sn_reset_prepare(struct pci_dev *pdev)
{
	struct sn_tranx_t *tdev = pci_get_drvdata(pdev);

	sn_pri(tdev, SN_ERR, "pcie: %s.\n", __func__);
}

void sn_reset_done(struct pci_dev *pdev)
{
	struct sn_tranx_t *tdev = pci_get_drvdata(pdev);

	sn_pri(tdev, SN_ERR, "pcie: %s.\n", __func__);
}

extern void pf_function_modules_release(struct sn_tranx_t *tdev);
extern int pf_function_modules_reinit(struct sn_tranx_t *tdev);

int sn_sriov_configure(struct pci_dev *pdev, int numvfs)
{
	struct sn_tranx_t *tdev = pci_get_drvdata(pdev);
	int err = 0, ret = 0;
	u32 old_sideband_status = 0, old_mode_sel = 0;

	sn_pri(tdev, SN_DBG, "**** sriov configure\n");

	/* allocate VFs if not already allocated */
	if (numvfs > 0) {
		if (numvfs > tdev->vf_max_count) {
			sn_pri(tdev, SN_ERR,
			       "pcie: enable sriov failed, numvfs:%d > max count:%d\n",
			       numvfs, tdev->vf_max_count);
			return -EFAULT;
		}
		if (tdev->vf_max_count == 2) {
			pf_function_modules_release(tdev);
			old_mode_sel = readl(VF_MODE_SEL + tdev->bar2_virt);
			old_sideband_status = sn_rd_b2(tdev, PCIE_SIDEBAND_CON_STUS);
			writel(0x0,
					VF_MODE_SEL +
					tdev->bar2_virt);
		/* assign hdma channel(0-3) to VF1, channel(4-7) for VF2*/
			sn_wr_b2(tdev, PCIE_SIDEBAND_CON_STUS, 0xaa55);
		/* send message to zsp, zsp_fw will transmit interrupt to two different vf */
			set_pf_vf_mode(tdev, TWO_VF_MODE);
			sn_pri(tdev, SN_INF, "%s,enable two_vf mode\n",
			       __func__);
		} else if (tdev->vf_max_count == 1) {
			pf_function_modules_release(tdev);
			old_mode_sel = readl(VF_MODE_SEL + tdev->bar2_virt);
			old_sideband_status = sn_rd_b2(tdev, PCIE_SIDEBAND_CON_STUS);
			writel(0x3,
					VF_MODE_SEL +
					tdev->bar2_virt);
		/* assign hdma channel(0-7) to VF1*/
			sn_wr_b2(tdev, PCIE_SIDEBAND_CON_STUS, 0x5555);
		/* send message to zsp, zsp_fw will transmit interrupt to one vf */
			set_pf_vf_mode(tdev, ONE_VF_MODE);
			sn_pri(tdev, SN_INF, "%s,enable one_vf mode\n",
			       __func__);
		} else {
			sn_pri(tdev, SN_ERR, "%s vf_max_count:%d error\n",
			       __func__, tdev->vf_max_count);
			pci_disable_sriov(pdev);
			return -EFAULT;
		}
		err = pci_enable_sriov(pdev, numvfs);
		if (err) {
			sn_pri(tdev, SN_ERR, "Enable PCI SR-IOV failed: %d\n",err);
			writel(old_mode_sel, VF_MODE_SEL + tdev->bar2_virt);
			sn_wr_b2(tdev, PCIE_SIDEBAND_CON_STUS, old_sideband_status);
			set_pf_vf_mode(tdev, PF_MODE);
			ret = pf_function_modules_reinit(tdev);
			if (ret) {
				sn_pri(tdev, SN_ERR, "core: modules_init failed\n");
				return -EFAULT;
			}
			return err;
		}
	} else if (numvfs == 0) {
		pci_disable_sriov(pdev);
		sn_wr_b2(tdev, PCIE_SIDEBAND_CON_STUS, 0x0);
		/* send message to zsp, fw will transmit interrupt to pf */
		set_pf_vf_mode(tdev, PF_MODE);
		ret = pf_function_modules_reinit(tdev);
		if (ret) {
			sn_pri(tdev, SN_ERR, "core: modules_init failed\n");
			return -EFAULT;
		}
		sn_pri(tdev, SN_INF, "%s, enable pf mode\n", __func__);
	}
	return numvfs;
}
