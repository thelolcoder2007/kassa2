// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 Verisilicon Inc.
 *  Author: Fengyin Wu <Fengyin.Wu@verisilicon.com>
 */

#include <linux/errno.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>
#include <linux/fcntl.h>
#include <linux/pagemap.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/aer.h>
#include <linux/slab.h>

#include "common.h"

#include "memory_osal.h"

#include "version.h"
#include "pcie.h"
#include "hdma.h"
#include "vc8000e.h"
#include "vc8000d.h"
#include "riscv.h"
#include "hw_monitor.h"
#include "vcmd/hantrovcmd.h"
#if SUB_SYS_XABR
#include "xabr_scaler.h"
#endif
#include "xav1_enc.h"
#include "transcoder.h"
#include "sn_perf.h"
#include "sn_osal.h"
#include "error_notify.h"

struct tdev_private {
	void *data;
	int minor;
	int node_num;
};

#define description_string "AMD ama transcoder driver"

#define VENDOR_ID 0x10EE
#define DEVICE_ID 0x5070
#define VF_DEVICE_ID 0x5071

struct tdev_node_status {
	struct tdev_private *sn_transcoder_dev;
	int max_device_cnt;
	spinlock_t node_status_lock;
};
static struct tdev_node_status sn_transcoder_node;

static unsigned int level = SN_INF;
module_param(level, uint, 0644);
MODULE_PARM_DESC(level, "print level: 2:DBG; 1:INF; 0:ERR; default is 1.");

static unsigned int ddr_ecc_flag = 1;
module_param(ddr_ecc_flag, uint, 0644);
MODULE_PARM_DESC(ddr_ecc_flag, "DDR ECC flag: 0:disable; 1:enable; default is 1.");

static unsigned int fps_unittest_en = 0;
module_param(fps_unittest_en, uint, 0644);
MODULE_PARM_DESC(fps_unittest_en, "FPS Unit Test flag: 0:disable; 1:enable; default is 0.");

static inline void show_version(void)
{
	printk(KERN_INFO "%s version %s\n", description_string, VERSION);
}

static struct sn_tranx_t *get_trans_dev(int minor)
{
	int i;
	struct sn_tranx_t *tdev = NULL;
	spin_lock(&sn_transcoder_node.node_status_lock);
	for (i = 0; i < sn_transcoder_node.max_device_cnt; i++) {
		if (minor == sn_transcoder_node.sn_transcoder_dev[i].minor)
			break;
	}
	tdev = (i < sn_transcoder_node.max_device_cnt) ? sn_transcoder_node.sn_transcoder_dev[i].data : NULL;
	spin_unlock(&sn_transcoder_node.node_status_lock);

	return tdev;
}

static long all_ip_soft_reset(struct file *filp, unsigned long arg, struct sn_tranx_t *tdev)
{
	long ret = 0;
#if SUB_SYS_XABR
	ret |= xabr_scaler_ioctl(filp, XABR_IOCTL_TWO_SLICE_SOFT_RESET, arg, tdev);
#endif
#if SUB_SYS_XAV1
	ret |= xav1_enc_ioctl(filp, XAV1_ENC_IOCTL_TWO_SLICE_SOFT_RESET, arg, tdev);
#endif
#if SUB_SYS_VCD
	ret |= vc8000d_ioctl(filp, HANTRODEC_IOC_SOFT_RESET, arg, tdev);
#endif
#if SUB_SYS_VCE
	ret |= vc8000e_ioctl(filp, HANTRO_IOC_SOFT_RESET, arg, tdev);
#endif
	return ret;
}

static long sn_global_ioctl(struct file *filp, unsigned int cmd, unsigned long arg, struct sn_tranx_t *tdev)
{
	long ret = 0;
	switch(cmd) {
		case IOCTL_ALL_IP_SOFT_RESET:
			ret = all_ip_soft_reset(filp, arg, tdev);
			break;
		default:
			return -ENOTTY;
	}
	return ret;
}

static long trans_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = -EINVAL;
	struct inode *inode = file_inode(filp);
	struct sn_tranx_t *tdev;
	unsigned int cid;

	tdev = get_trans_dev(iminor(inode));
	if (!tdev) {
		sn_pri(tdev, SN_ERR, "core: get_trans_dev failed.\n");
		return -EFAULT;
	}

	if (_IOC_NR(cmd) > TRANS_MAXNR)
		return -ENOTTY;

	cid = _IOC_NR(cmd);

	if ((cid >= IOCTL_CMD_PCIE_MINNR) && (cid <= IOCTL_CMD_PCIE_MAXNR))
		ret = sn_pci_ioctl(filp, cmd, arg, tdev);
	else if ((cid >= IOCTL_CMD_HDMA_MINNR) && (cid <= IOCTL_CMD_HDMA_MAXNR))
		ret = sn_hdma_ioctl(filp, cmd, arg, tdev);
	else if (cid >= IOCTL_CMD_MEM_MINNR && cid <= IOCTL_CMD_MEM_MAXNR) {
		ret = sn_mem_osal_ioctl(filp, cmd, arg, tdev);
	}
#if SUB_SYS_RISCV
	else if (cid >= IOCTL_CMD_RISCV_IP_MINNR &&
		 cid <= IOCTL_CMD_RISCV_IP_MAXNR)
		ret = riscv_ioctl(filp, cmd, arg, tdev);
#endif
	else if (cid >= IOCTL_CMD_MA_MINNR &&
		 cid <= IOCTL_CMD_MA_MAXNR  && !(IS_PF(tdev) && tdev->pf_vf_mode != PF_MODE))
		ret = sn_osal_ioctl(filp, cmd, arg, tdev);
	else if (cid >= IOCTL_CMD_PERF_MINNR && cid <= IOCTL_CMD_PERF_MAXNR)
		ret = sn_perf_ioctl(filp, cmd, arg, tdev);
	else if (cid >= IOCTL_CMD_GLOBAL_MINNR && cid <= IOCTL_CMD_GLOBAL_MAXNR)
		ret = sn_global_ioctl(filp, cmd, arg, tdev);
	else if (cid >= IOCTL_DMA_BUF_MINNR && cid <= IOCTL_DMA_BUF_MAXNR) {
		long sn_dma_buf_ioctl(struct file* filp, unsigned cmd, unsigned long arg, struct sn_tranx_t* tdev);
		ret = sn_dma_buf_ioctl(filp, cmd, arg, tdev);
	} else
		sn_pri(tdev, SN_ERR, "core: ioctl cmd:0x%x is error\n", cid);

	return ret;
}

static int trans_close(struct inode *inode, struct file *filp)
{
	struct sn_tranx_t *tdev = get_trans_dev(iminor(inode));

	if (WARN_ON(!tdev))
		return -EFAULT;

    if (tdev->init_flag & (1 << SN_MODULE_HDMA)) {
        sn_hdma_close(tdev, filp);
    }

	// OSAL shutdown *must* come before IP shutdown (flush pending work)
	if (tdev->init_flag & (1 << SN_MODULE_OSAL)) {
		sn_osal_close(tdev, filp);
	}
	// This needs to come *before* the IP specific closes,
	if (tdev->init_flag & (1 << SN_MODULE_PERF)) {
		sn_perf_close(tdev, filp);
	}

#if (VCMD_ENABLE_VC8000D == 1 || VCMD_ENABLE_VC8000E == 1)
	if (tdev->init_flag & (1 << SN_MODULE_VCMD)) {
		hantrovcmd_release(tdev, filp);
	}
#endif

#if (SUB_SYS_VCD)
	if (tdev->init_flag & (1 << SN_MODULE_VC8000D)) {
		vc8000d_close(tdev, filp);
	}
#endif

#if (SUB_SYS_XAV1)
	if (tdev->init_flag & (1 << SN_MODULE_XAV1_ENC)) {
		xav1_close(tdev, filp);
	}
#endif

#if SUB_SYS_XABR
	if (tdev->init_flag & (1 << SN_MODULE_XABR)) {
		xabr_close(tdev, filp);
	}
#endif

	if (tdev->init_flag & (1 << SN_MODULE_MEMORY_OSAL)) {
		sn_mem_osal_close(tdev, filp);
	}

	return 0;
}

static int trans_open(struct inode *inode, struct file *filp)
{
	struct sn_tranx_t *tdev = get_trans_dev(iminor(inode));

	if (WARN_ON(!tdev))
		return -EFAULT;

#if (VCMD_ENABLE_VC8000D == 1 || VCMD_ENABLE_VC8000E == 1)
	if (tdev->init_flag & (1 << SN_MODULE_VCMD)) {
		return hantrovcmd_open(tdev, filp);
	}
	else
		return 0;
#endif

	return 0;

}

static int trans_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret = 0;
	struct inode *inode = file_inode(file);
	unsigned long size = vma->vm_end - vma->vm_start;
	struct sn_tranx_t *tdev = get_trans_dev(iminor(inode));

	resource_size_t bar2_base = pci_resource_start(tdev->pdev, 2);
	resource_size_t bar4_base = pci_resource_start(tdev->pdev, 4);
	uint64_t bar4_mailbox_f2d_start = bar4_base + 0x80000;	// MAILBOX_BASE_ADDRESS = 0x80000
	uint64_t bar4_mailbox_d2f_start = bar4_mailbox_f2d_start + 0x20000;	// MAILBOX_RESERVE_SIZE = 0x20000

	if (!tdev) {
		sn_pri(tdev, SN_ERR, "core: get_trans_dev failed.\n");
		return -EFAULT;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0) || (defined(RHEL_MAJOR) && RHEL_MAJOR == 9)
	vm_flags_reset(vma, VM_IO);
	vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);
#else
	vma->vm_flags &= ~VM_IO;
	vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);
#endif

	if (bar2_base != (vma->vm_pgoff << 12) &&
		(bar4_mailbox_f2d_start != (vma->vm_pgoff << 12) || size != 0x1000) && (bar4_mailbox_d2f_start != (vma->vm_pgoff << 12) || size != 0x1000) ) {	// size(minimum) = 0x1000, MAILBOX_F2D_SIZE = 0x200
		uint64_t address = sn_mem_osal_translate_handle(tdev, file, vma->vm_pgoff << 12, size);
		if (address == 1) {
			sn_pri(tdev, SN_ERR, "Invalid mmap handle\n");
			return -EINVAL;
		}
		address = (uintptr_t) sn_mem_osal_translate_mmio(tdev, address);
		if (!address) {
			sn_pri(tdev, SN_ERR, "Bad address translation\n");
			return -EINVAL;
		}
		address -= (uintptr_t) tdev->bar4_virt;
		address += pci_resource_start(tdev->pdev, 4);
		vma->vm_pgoff = address >> 12;
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot); // doesn't seem to work - revisit
	}

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size,
			    vma->vm_page_prot) < 0) {
		sn_pri(tdev, SN_ERR, "core: remap_pfn_range failed.\n");
		sn_pri(tdev, SN_ERR,
		       "core: start:0x%lx end:0x%lx size:0x%lx paddr:0x%lx\n",
		       vma->vm_start, vma->vm_end, size,
		       vma->vm_pgoff << PAGE_SHIFT);
		ret = -ENOMEM;
	}

	return ret;
}

static const struct file_operations trans_char_fops = {
	.owner = THIS_MODULE,
	.open = trans_open,
	.release = trans_close,
	.unlocked_ioctl = trans_ioctl,
	.compat_ioctl = trans_ioctl,
	.mmap = trans_mmap,
};

struct modules_info {
	enum TRANS_MODULE_INDEX index;
	int (*init)(struct sn_tranx_t *);
	void (*release)(struct sn_tranx_t *);
	char name[64];
};

/*
 * init sequence is from modules_list[0] to module_slist[n], release
 * sequence from modules_list[n] to module_slist[0], pay attention to
 * the position if new item need insert.
*/
static struct modules_info modules_list[] = {
	{SN_MODULE_PCIE,			sn_pci_init,	sn_pci_release,		"pci"	},
	{SN_MODULE_HW_MONITOR,		sn_hwm_init,	sn_hwm_release,	"monitor"},
	{SN_MODULE_ERROR_NOTIFY,	sn_error_notify_init,	sn_error_notify_release,"error_notify"},
	{SN_MODULE_HDMA,			sn_hdma_init,	sn_hdma_release,	"hdma"	},
	{SN_MODULE_MEMORY_OSAL,		sn_mem_osal_init,	sn_mem_osal_release,		"memory_osal"},
	{SN_MODULE_PERF,			sn_perf_init,	sn_perf_release,	"perf"	},
	{SN_MODULE_OSAL,			sn_osal_init,	sn_osal_release,	"osal"	},
#if SUB_SYS_VCD
	{SN_MODULE_VC8000D,			vc8000d_init,	vc8000d_release,	"vc8000d"},
#endif
#if (VCMD_ENABLE_VC8000D == 1 || VCMD_ENABLE_VC8000E == 1)
	{SN_MODULE_VCMD,			hantrovcmd_init, hantrovcmd_cleanup,	"vcmd"},
#endif
#if (SUB_SYS_VCE == 1 && VCMD_ENABLE_VC8000E == 0)
	{SN_MODULE_VC8000E,			vc8000e_init,	vc8000e_release,	"vc8000e"},
#endif
#if SUB_SYS_XABR
	{SN_MODULE_XABR,			xabr_scaler_init, xabr_scaler_release,	"xabr scaler"},
#endif
#if SUB_SYS_XAV1
	{SN_MODULE_XAV1_ENC,		xav1_enc_init,	xav1_enc_release,	"xav1 enc"},
#endif
#if SUB_SYS_RISCV
	{SN_MODULE_RISCV,			riscv_init,	riscv_release,		"riscv"}
#endif
};

static void ma35_sys_modules_release(struct sn_tranx_t *tdev)
{
	int i;
	for (i = SN_MODULE_MEMORY_OSAL; i >= 0 ; i--) {
		if (tdev->init_flag & (1 << modules_list[i].index)) {
			modules_list[i].release(tdev);
			tdev->init_flag &= ~(1 << modules_list[i].index);
		}
	}
}

static void ma35_ip_modules_release(struct sn_tranx_t *tdev)
{
	int i;
	for (i = sizeof(modules_list)/sizeof(modules_list[0]) - 1; i > SN_MODULE_MEMORY_OSAL ; i--) {
		if (tdev->init_flag & (1 << modules_list[i].index)) {
			modules_list[i].release(tdev);
			tdev->init_flag &= ~(1 << modules_list[i].index);
		}
	}
}

static int ma35_sys_modules_init(struct sn_tranx_t *tdev)
{
	int i;
	for (i = 0; i <= SN_MODULE_MEMORY_OSAL; i++) {
		if (modules_list[i].init(tdev)) {
			sn_pri(tdev, SN_ERR, "core: initialize %s failed.\n", modules_list[i].name);
			goto out;
		} else {
			tdev->init_flag |= (1 << modules_list[i].index);
		}
	}

	return 0;

out:
	ma35_sys_modules_release(tdev);
	return -EFAULT;
}

static int ma35_ip_modules_init(struct sn_tranx_t *tdev)
{
	int i;
	for (i = SN_MODULE_PERF; i < sizeof(modules_list)/sizeof(modules_list[0]); i++) {
		if (modules_list[i].init(tdev)) {
			sn_pri(tdev, SN_ERR, "core: initialize %s failed.\n", modules_list[i].name);
			goto out;
		} else {
			tdev->init_flag |= (1 << modules_list[i].index);
		}
	}

	return 0;

out:
	ma35_ip_modules_release(tdev);
	ma35_sys_modules_release(tdev);
	return -EFAULT;
}

extern void pf_function_modules_release(struct sn_tranx_t *tdev);

void pf_function_modules_release(struct sn_tranx_t *tdev)
{

	if (IS_PF(tdev)) {
		ma35_ip_modules_release(tdev);
		if (ma35_ip_power_config(tdev, EVENT_VF2PF_IP_POWER_DOWN)) {
			sn_pri(tdev, SN_ERR, "core: ma35_ip_powerdown failed\n");
		}
	}
	else
		sn_pri(tdev, SN_INF, "core: %s failed! it only runs in PF\n", __func__);
}

extern int pf_function_modules_reinit(struct sn_tranx_t *tdev);

int pf_function_modules_reinit(struct sn_tranx_t *tdev)
{
	int ret = 0;
	if (IS_PF(tdev)) {
		ret = ma35_ip_power_config(tdev, EVENT_VF2PF_IP_POWER_UP);
		if (ret) {
			sn_pri(tdev, SN_ERR, "core: ma35_ip_powerup failed\n");
			return -EFAULT;
		}

		ret = ma35_ip_modules_init(tdev);
		if (ret) {
			sn_pri(tdev, SN_ERR, "core: ma35_ip_modules_init failed\n");
			goto out;
		}
	}
	else {
		sn_pri(tdev, SN_INF, "core: %s failed! it only runs in PF\n", __func__);
		return -EFAULT;
	}

	return 0;

out:
	ma35_ip_modules_release(tdev);
	return -EFAULT;
}

static ssize_t drv_log_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	u32 level;
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;

	if (count == 0)
		return 0;

	if (sscanf(buf, "%d", &level) != 1) {
		sn_pri(tdev, SN_ERR, "core: not in hex or decimal form.\n");
		return -1;
	}
	if (level <= SN_DBG)
		tdev->print_level = level;
	else {
		sn_pri(tdev, SN_ERR, "core: level:%d is invalid.\n", level);
		return -1;
	}

	return count;
}

static ssize_t drv_log_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;

	return sprintf(buf, "%d\n", tdev->print_level);
}
static ssize_t drv_rev_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "%s\n", VERSION);
}

static ssize_t hw_err_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	int flag, ret;

	if (count == 0)
		return 0;

	ret = sscanf(buf, "%d", &flag);
	if (ret != 1) {
		sn_pri(tdev, SN_ERR, "core: %s ret=%d, input_val:%d\n",
		       __func__, ret, flag);
		return -1;
	}
	tdev->hw_err_flag = flag;

	return count;
}

static ssize_t hw_err_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;

	return sprintf(buf, "0x%x\n", tdev->hw_err_flag);
}

static DEVICE_ATTR_RW(hw_err);
static DEVICE_ATTR_RW(drv_log);
static DEVICE_ATTR_RO(drv_rev);

static struct attribute *trans_sysfs_entries[] = { &dev_attr_drv_log.attr,
						   &dev_attr_drv_rev.attr,
						   &dev_attr_hw_err.attr,
						   NULL };

static struct attribute_group trans_attribute_group = {
	.name = NULL,
	.attrs = trans_sysfs_entries,
};

static bool get_empty_dev_node(int *pnode)
{
	int new_max_device_cnt = 0, i = 0;
	struct tdev_private *new_sn_dev = NULL;
	spin_lock(&sn_transcoder_node.node_status_lock);
	for (*pnode = 0; *pnode < sn_transcoder_node.max_device_cnt; (*pnode)++)
		if (sn_transcoder_node.sn_transcoder_dev[*pnode].node_num == -1)
			break;
	if (*pnode >= sn_transcoder_node.max_device_cnt) {
		new_max_device_cnt = sn_transcoder_node.max_device_cnt * 2;
		new_sn_dev = krealloc_array(sn_transcoder_node.sn_transcoder_dev,
					new_max_device_cnt, sizeof(struct tdev_private), GFP_KERNEL);
		if (!new_sn_dev) {
			spin_unlock(&sn_transcoder_node.node_status_lock);
			return false;
		}
		sn_transcoder_node.sn_transcoder_dev = new_sn_dev;
		sn_transcoder_node.max_device_cnt = new_max_device_cnt;
		for (i = *pnode; i < sn_transcoder_node.max_device_cnt; i++)
			sn_transcoder_node.sn_transcoder_dev[i].node_num = -1;
	}
	spin_unlock(&sn_transcoder_node.node_status_lock);
	return true;
}

static int register_tranx_dev(struct pci_dev *pdev, struct sn_tranx_t *tdev)
{
	int ret = 0;
	int node;
	struct sn_misc_tdev *mtdev = NULL;
	struct miscdevice *trans_misc_dev = NULL;

	if (!get_empty_dev_node(&node)) {
		sn_pri(tdev, SN_ERR,
		"core: node:%d error, realloc failed.\n", node);
		return -EFAULT;
	}

	tdev->dev_name = kasprintf(GFP_KERNEL, "ama_transcoder%d", node);
	mtdev = kzalloc(sizeof(struct sn_misc_tdev), GFP_KERNEL);
	if (!mtdev) {
		sn_pri(tdev, SN_ERR, "core: kzalloc mtdev failed\n");
		goto out;
	}

	trans_misc_dev = &mtdev->misc;
	trans_misc_dev->minor = MISC_DYNAMIC_MINOR;
	trans_misc_dev->fops = &trans_char_fops;
	trans_misc_dev->name = tdev->dev_name;
	trans_misc_dev->mode = 0666;
	ret = misc_register(trans_misc_dev);
	if (ret) {
		sn_pri(tdev, SN_ERR,
		       "core: misc_register trans_misc_dev failed.\n");
		goto out_free_misc;
	}
	tdev->misc_dev = trans_misc_dev;
	pci_set_drvdata(pdev, tdev);
	mtdev->tdev = tdev;

	ret = sysfs_create_group(&trans_misc_dev->this_device->kobj,
				 &trans_attribute_group);
	if (ret) {
		sn_pri(tdev, SN_ERR,
		       "core: failed to create sysfs device attributes\n");
		goto out_dereg_misc;
	}

	sn_transcoder_node.sn_transcoder_dev[node].data = tdev;
	sn_transcoder_node.sn_transcoder_dev[node].minor = trans_misc_dev->minor;
	tdev->node_index = node;
	sn_transcoder_node.sn_transcoder_dev[node].node_num = node;

	sn_pri(tdev, SN_DBG,
	       "core: register transcoder successfully: name:%s, minor:%d\n",
	       trans_misc_dev->name, trans_misc_dev->minor);

	return 0;

out_dereg_misc:
	misc_deregister(trans_misc_dev);
out_free_misc:
	kfree(trans_misc_dev);
out:
	return -EFAULT;
}

static void clean_node(struct sn_tranx_t *tdev)
{
	int i = 0, empty_node_cnt = 0, half_max_dev_cnt = 0;
	struct tdev_private *new_sn_dev = NULL;
	sn_transcoder_node.sn_transcoder_dev[tdev->node_index].node_num = -1;
	sn_transcoder_node.sn_transcoder_dev[tdev->node_index].minor = -1;
	spin_lock(&sn_transcoder_node.node_status_lock);
	for (i = sn_transcoder_node.max_device_cnt - 1; i >= INIT_DEVICE_CNT; i--) {
		if (sn_transcoder_node.sn_transcoder_dev[i].node_num == -1) {
			empty_node_cnt++;
		}
		else {
			break;
		}
	}
	half_max_dev_cnt = sn_transcoder_node.max_device_cnt / 2;
	if (empty_node_cnt >= half_max_dev_cnt) {
		new_sn_dev = krealloc_array(sn_transcoder_node.sn_transcoder_dev,
					half_max_dev_cnt, sizeof(struct tdev_private), GFP_KERNEL);
		if (!new_sn_dev) {
			sn_pri(tdev, SN_ERR, "%s: realloc failed\n", __func__);
		}
		else {
			sn_transcoder_node.sn_transcoder_dev = new_sn_dev;
			sn_transcoder_node.max_device_cnt = half_max_dev_cnt;
		}
	}
	spin_unlock(&sn_transcoder_node.node_status_lock);
}

static int sn_trans_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int i;
	int ret = 0;
	void *modules;
	struct sn_tranx_t *tdev;

	if(id == NULL) {
		dev_info(&pdev->dev, "probe failed! pci_device_id is NULL\n");
		return -EFAULT;
	}

	dev_info(&pdev->dev, "devfn = %d\n", pdev->devfn);

	tdev = kzalloc(sizeof(struct sn_tranx_t), GFP_KERNEL);
	if (!tdev) {
		dev_info(&pdev->dev, "core: alloc sn_tranx_t failed\n");
		goto out;
	}

	/* default level is info */
	tdev->print_level     = level;
	tdev->ddr_ecc_flag    = ddr_ecc_flag;
	tdev->fps_unittest_en = fps_unittest_en;
	tdev->pdev            = pdev;
	tdev->pf_vf_flag      = (id->device == VF_DEVICE_ID) ? DEVICE_TYPE_VF: DEVICE_TYPE_PF;

	/* pci+memory+hdma ...*/
	modules = kzalloc(sizeof(void *) * SN_MODULE_MAX, GFP_KERNEL);
	if (!modules) {
		dev_info(&pdev->dev, "core: allocate modules failed.\n");
		goto out_free_dev;
	}

	for (i = 0; i < SN_MODULE_MAX; i++)
		tdev->modules[i] = modules + i;

	ret = register_tranx_dev(pdev, tdev);
	if (ret) {
		sn_pri(tdev, SN_ERR, "core: register_tranx_dev failed.\n");
		goto out_free_module;
	}

	tdev->init_flag = 0;
	ret = ma35_sys_modules_init(tdev);
	if (ret) {
		sn_pri(tdev, SN_ERR, "core: ma35_sys_modules_init failed\n");
		goto out_free_misc;
	}

	ret = ma35_ip_power_config(tdev, EVENT_VF2PF_IP_POWER_UP);
	if (ret) {
		sn_pri(tdev, SN_ERR, "core: ma35_ip_powerup failed\n");
		goto out_free_mod;
	}

	ret = ma35_ip_modules_init(tdev);
	if (ret) {
		sn_pri(tdev, SN_ERR, "core: ma35_ip_modules_init failed\n");
		goto out_free_mod;
	}

	if (IS_PF(tdev)) {
		enable_interrupt_data_path(tdev);

		/* Create kworker thread for IPs initialization in VFIO and VM */
		tdev->kworker_thread_ps = kthread_create_worker(0, "ma35_power_setting_kthread");
		wake_up_process(tdev->kworker_thread_ps->task);
		tdev->kwork_ps = kmalloc(sizeof(struct ma35_kwork_ps), GFP_KERNEL);
		kthread_init_work(&tdev->kwork_ps->ma35_kwork, ma35_kwork_power_setting);
		tdev->kwork_ps->tdev = (void *)tdev;
	}

	sn_pri(tdev, SN_INF, "ama_transcoder inserted successfully.\n");
	return 0;
out_free_mod:
	ma35_sys_modules_release(tdev);
out_free_misc:
	misc_deregister(tdev->misc_dev);
	kfree(tdev->misc_dev);
	clean_node(tdev);
out_free_module:
	kfree(modules);
out_free_dev:
	kfree(tdev);
out:
	return -EFAULT;
}

static void sn_trans_remove(struct pci_dev *pdev)
{
	struct sn_tranx_t *tdev = pci_get_drvdata(pdev);
	struct miscdevice *trans_misc_dev = tdev->misc_dev;

	sn_pri(tdev, SN_DBG,
	       "core: trans remove. tdev:0x%p trans_misc_dev:0x%p %d\n", tdev,
	       trans_misc_dev, trans_misc_dev->minor);

	if (IS_PF(tdev)) {
		disable_interrupt_data_path(tdev);
		pci_disable_sriov(pdev);
		sn_wr_b2(tdev, PCIE_SIDEBAND_CON_STUS, 0x0);
		/* send message to zsp, fw will transmit interrupt to pf */
		set_pf_vf_mode(tdev, PF_MODE);
		sn_pri(tdev, SN_INF, "%s, enable pf mode\n", __func__);
	}

	ma35_ip_modules_release(tdev);
	if(ma35_ip_power_config(tdev, EVENT_VF2PF_IP_POWER_DOWN)) {
		sn_pri(tdev, SN_ERR, "%s, IP modules power down failed\n", __func__);
	}
	ma35_sys_modules_release(tdev);

	sysfs_remove_group(&trans_misc_dev->this_device->kobj,
			   &trans_attribute_group);

	if (IS_PF(tdev)) {
		kthread_destroy_worker(tdev->kworker_thread_ps);
	}

	misc_deregister(trans_misc_dev);
	clean_node(tdev);

	kfree(tdev->modules[0]);
	kfree(trans_misc_dev);
	kfree(tdev);
}

static const struct pci_error_handlers sn_err_handler = {
	.error_detected = sn_err_detected,
	.slot_reset = sn_slot_reset,
	.resume = sn_resume,
	.reset_prepare = sn_reset_prepare,
	.reset_done = sn_reset_done,
};

static struct pci_device_id sn_trans_pcie_table[] = {
	{ VENDOR_ID, DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ VENDOR_ID, VF_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{
		0,
	}
};

MODULE_DEVICE_TABLE(pci, sn_trans_pcie_table);

static struct pci_driver sn_trans_pcie_driver = {
	.name = "transcoder",
	.id_table = sn_trans_pcie_table,
	.probe = sn_trans_probe,
	.remove = sn_trans_remove,
	.err_handler = &sn_err_handler,
	.sriov_configure = sn_sriov_configure,
	.shutdown = sn_trans_remove,
};

static int __init sn_trans_pcie_init(void)
{
	int i;

	show_version();
	spin_lock_init(&sn_transcoder_node.node_status_lock);
	sn_transcoder_node.max_device_cnt = INIT_DEVICE_CNT;
	sn_transcoder_node.sn_transcoder_dev = kmalloc_array(sn_transcoder_node.max_device_cnt,
						sizeof(struct tdev_private), GFP_KERNEL);
	if (!sn_transcoder_node.sn_transcoder_dev) {
		return -ENOMEM;
	}
	for (i = 0; i < sn_transcoder_node.max_device_cnt; i++)
		sn_transcoder_node.sn_transcoder_dev[i].node_num = -1;
	return pci_register_driver(&sn_trans_pcie_driver);
}

static void __exit sn_trans_pcie_cleanup(void)
{
	pci_unregister_driver(&sn_trans_pcie_driver);
	if (sn_transcoder_node.sn_transcoder_dev)
		kfree(sn_transcoder_node.sn_transcoder_dev);
}

module_init(sn_trans_pcie_init);
module_exit(sn_trans_pcie_cleanup);

MODULE_AUTHOR("AMD");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(description_string);
MODULE_VERSION(VERSION);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0) || (defined(RHEL_MAJOR) && RHEL_MAJOR >= 10)
MODULE_IMPORT_NS("DMA_BUF");
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
MODULE_IMPORT_NS(DMA_BUF);
#endif

