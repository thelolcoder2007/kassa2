// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2018 Verisilicon Inc.
 *
 * This is hardware monitor driver for Linux.
 * this driver for monitoring the hardware health.
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
#include <linux/kthread.h>

#include "regs.h"
#include "common.h"
#include "pcie.h"
#include "hw_monitor.h"
#include "xav1_enc.h"
#include "transcoder.h"
#include "error_notify.h"

#define FW_LOAD_ADDR SRAM_SC_OFF
#define ZSP_SRAM_ADDR SRAM_SC_OFF

#define FIRMWARE "supernova_zsp_fw.bin"

#define ZSP_CPU_IRQ_INDEX 1
#define VF_PF_IRQ_INDEX 2

/* Jump to iTCM, run firmware. */
#define GO_CMD 0x5a5a5a5a

#define ENABLE_DDR_ECC 0x5d5d5b5b /*E,E,C,C*/
#define DISABLE_DDR_ECC 0x5c5d5b5b /*D,E,C,C*/

#define DDR_ECC_ENABLED 0x5d5b5b5d /*E,C,C,E*/
#define DDR_ECC_DISABLED 0x5d5b5b5c /*E,C,C,D*/

/* get command status:successfully. */
#define DONE 0x444F4E45

#define RUN_SUCCESS 0x83856767

#define SIG_ERROR 0x4641494C

/* ('T','O','E','P'):host send to EP:0x544F4550 */
#define SIGNATURE_TO_EP 0x544F4550

/* ('T','O','R','C'):EP send to host:0x544F5243 */
#define SIGNATURE_TO_RC 0x544F5243

#define CMD_DIR_ADDR 0x1fff4
#define CMD_RSN_ADDR 0x1fff8
#define CMD_EXT_ADDR 0x1fffc
#define LOAD_TIMEOUT 30
#define CHECK_TIMEOUT 1
#define WAIT_ZSP_TIME 20 /* jiffers */
#define SEND_INFO_TIMEOUT 30 /* jiffers */

/* In �sys_adb_lpi_con�sheet */
#define SC_MST_ADB_LPI_CON_STUS_OFF (TOP_SYS_CON_OFF + 0x508)
#define SC_SLV_ADB_LPI_CON_STUS_OFF (TOP_SYS_CON_OFF + 0x50c)
/* In �sys_rst_con� sheet */
#define SYS_RST_CON_STUS_OFF (TOP_SYS_CON_OFF + 0x100)

#define THS1_VF_PF_INT_CON_STUS (S1_SYS_CON_OFF + 0x40C)
#define THS1_PF_VF_INT_CON_STUS (S1_SYS_CON_OFF + 0x41C)
#define THS2_VF_PF_INT_CON_STUS (S2_SYS_CON_OFF + 0x40C)
#define THS2_PF_VF_INT_CON_STUS (S2_SYS_CON_OFF + 0x41C)

#define DDR_CNT_SLICE 4
#define LPDDR5_DATA_RATE 5500
#define DDR_CTRL_FREQ (LPDDR5_DATA_RATE / 8) /* for LPDDR5 */

#define BOOT_MODE_DEBUG  0
#define BOOT_MODE_NORMAL 1
#define BOOT_MODE_PCIE   2

#define MAILBOX_BASE_ADDRESS        0x80000
#define MAILBOX_RESERVE_SIZE        0x20000

/* FPS subsystem */
#define MBZ_UPDATE_TIMEOUT          5

#define NUM_SLICES_PER_DEVICE       2
#define NUM_MICROBLAZES_PER_SLICE   4

// Ref: https://confluence.xilinx.com/display/VDC/FPS+Firmware+CPU+Processing+Subsystem
#define MICROBLAZE_FW_MAX_SIZE      0x80000  // 512kB

#define FPS_REGISTER_BASE           0x402000
#define FPS_STATUS                  FPS_REGISTER_BASE + 0xa8

#define MAX_ENC_FW_NAME		    100

struct mailbox_info {
	u32 event_id;
	u32 vf_data_ready;
	u32 pf_data_ready;
	int pf_bhalf_ready;
	int reserved[2]; /* For future use */
	u32 data[60];
};

struct hwm_t {
	spinlock_t zsp_soft_lock;
	spinlock_t pf_to_vf1_lock;
	spinlock_t pf_to_vf2_lock;
	unsigned int zsp_soft_irq;
	unsigned int vf_pf_irq;
	void __iomem *mbm_f2d; /* mailbox memory, firmware to driver */
	void __iomem *mbm_d2f; /* mailbox memory, driver to firmware */
	struct semaphore info_zsp_sem;
	struct sn_tranx_t *tdev;
};

static ssize_t update_version(char *buf, char *module_name, unsigned int version)
{
	return sprintf(buf + strlen(buf), "\t %s = %u.%u.%u\n", module_name,
			(version >> 16) & 0xFF, (version >> 8) & 0xFF, (version & 0xFF));
}

static ssize_t update_board_vc(char *buf, char *module_name, char *unit,
                              u16 board_vc)
{
	return sprintf(buf + strlen(buf), "\t %s = %u %s\n", module_name,
			       board_vc, unit);
}

static int reset_zsp(struct sn_tranx_t *tdev)
{
	u32 val, delay;

	sn_pri(tdev, SN_DBG, "hwm: start reset zsp.\n");
	/********* Soft reset flow *********/
	val = readl(tdev->bar2_virt + SC_MST_ADB_LPI_CON_STUS_OFF);
	val &= 0xfffffffe;
	writel(val, tdev->bar2_virt + SC_MST_ADB_LPI_CON_STUS_OFF);

	delay = 10000;
	while (delay--) {
		val = readl(tdev->bar2_virt + SC_MST_ADB_LPI_CON_STUS_OFF);
		/* bit5 should be 0 */
		if ((val & (1 << 5)) == 0)
			break;
		usleep_range(100, 200);
	}
	if ((val & (1 << 5)) != 0) {
		sn_pri(tdev, SN_ERR,
		       "hwm: reset:check SC_MST_ADB_LPI_CON_STUS failed.\n");
		return -1;
	}

	val = readl(tdev->bar2_virt + SC_SLV_ADB_LPI_CON_STUS_OFF);
	val &= 0xfffffffe;
	writel(val, tdev->bar2_virt + SC_SLV_ADB_LPI_CON_STUS_OFF);

	while (delay--) {
		val = readl(tdev->bar2_virt + SC_SLV_ADB_LPI_CON_STUS_OFF);
		/* bit5 should be 0 */
		if ((val & (1 << 5)) == 0)
			break;
		usleep_range(100, 200);
	}
	if ((val & (1 << 5)) != 0) {
		sn_pri(tdev, SN_ERR,
		       "hwm: reset:check SC_SLV_ADB_LPI_CON_STUS failed.\n");
		return -1;
	}

	val = readl(tdev->bar2_virt + SYS_RST_CON_STUS_OFF);
	val &= 0xFFC04000;
	writel(val, tdev->bar2_virt + SYS_RST_CON_STUS_OFF);

	/*********  Release soft reset flow *********/
	val = readl(tdev->bar2_virt + SYS_RST_CON_STUS_OFF);
	val |= 0x3FBFFF;
	writel(val, tdev->bar2_virt + SYS_RST_CON_STUS_OFF);

	val = readl(tdev->bar2_virt + SC_MST_ADB_LPI_CON_STUS_OFF);
	val |= 0x1;
	writel(val, tdev->bar2_virt + SC_MST_ADB_LPI_CON_STUS_OFF);

	while (delay--) {
		val = readl(tdev->bar2_virt + SC_MST_ADB_LPI_CON_STUS_OFF);
		/* bit5 should be 1 */
		if ((val & (1 << 5)) == 0x20)
			break;
		usleep_range(100, 200);
	}
	if ((val & (1 << 5)) != 0x20) {
		sn_pri(tdev, SN_ERR,
		       "hwm: release: check SC_MST_ADB_LPI_CON_STUS failed\n");
		return -1;
	}

	val = readl(tdev->bar2_virt + SC_SLV_ADB_LPI_CON_STUS_OFF);
	val |= 0x1;
	writel(val, tdev->bar2_virt + SC_SLV_ADB_LPI_CON_STUS_OFF);

	while (delay--) {
		val = readl(tdev->bar2_virt + SC_SLV_ADB_LPI_CON_STUS_OFF);
		/* bit5 should be 1 */
		if ((val & (1 << 5)) == 0x20)
			break;
		usleep_range(100, 200);
	}
	if ((val & (1 << 5)) != 0x20) {
		sn_pri(tdev, SN_ERR,
		       "hwm: release: check SC_SLV_ADB_LPI_CON_STUS failed\n");
		return -1;
	}

	sn_pri(tdev, SN_DBG, "hwm: reset zsp done.\n");
	return 0;
}

static int check_firmware(struct hwm_t *thwm, u32 sec)
{
	unsigned long delay = 1000;
	u32 val_dir, val_st;
	void __iomem *sram = thwm->tdev->bar2_virt + ZSP_SRAM_ADDR;

	/* check zsp firmware respond */
	delay = jiffies + sec * HZ;
	while (time_before(jiffies, delay)) {
		val_dir = readl(sram + CMD_DIR_ADDR);
		val_st = readl(sram + CMD_RSN_ADDR);
		if ((val_dir == SIGNATURE_TO_RC) && (val_st == RUN_SUCCESS)) {
			sn_pri(thwm->tdev, SN_INF,
			       "hwm: run firmware successfully.\n");
			return 0;
		}
		usleep_range(10000, 20000);
	}
	return -1;
}

static void set_ddr_ecc_flag(struct sn_tranx_t *tdev)
{
	void __iomem *sram = tdev->bar2_virt + ZSP_SRAM_ADDR;
	if (tdev->ddr_ecc_flag)
		writel(ENABLE_DDR_ECC, sram + CMD_EXT_ADDR);
	else
		writel(DISABLE_DDR_ECC, sram + CMD_EXT_ADDR);
}

static int fps_update_status(struct sn_tranx_t *tdev, u32 fw_type)
{
	unsigned long delay;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *f2d_mb = (struct sn_mail_box_f2d __iomem *)thwm->mbm_f2d;
	struct sn_mail_box_d2f __iomem *d2f_mb = (struct sn_mail_box_d2f __iomem *)thwm->mbm_d2f;

	usleep_range(10000, 12000);
	delay = jiffies + MBZ_UPDATE_TIMEOUT * HZ;
	/* polling for FPS update status */
	while (time_before(jiffies, delay)) {
		if (f2d_mb->mbz_update_status == MBZ_FW_LOAD_SUCCESS) {
			d2f_mb->mbz_fw_info.fw_type = 0;
			f2d_mb->mbz_update_status = MBZ_FW_LOAD_IDLE;
			return 0;
		}
		else if (f2d_mb->mbz_update_status == MBZ_FW_LOAD_IN_PROGRESS) {
			d2f_mb->mbz_fw_info.fw_type = 0;
		}
		else if (f2d_mb->mbz_update_status != MBZ_FW_LOAD_IDLE) {
			d2f_mb->mbz_fw_info.fw_type = 0;
			sn_pri(tdev, SN_ERR,
				  "hwm: FPS firmware update failed, Error code %d \n",
				  f2d_mb->mbz_update_status);
			return -EIO;
		}
		usleep_range(10000, 12000);
	}

	sn_pri(tdev, SN_ERR, "hwm: FPS firmware update timed out\n");
	return -EIO;

}

static void trigger_fps_load(struct sn_tranx_t *tdev, u32 fw_type)
{
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_d2f __iomem *d2f_mb = (struct sn_mail_box_d2f __iomem *)thwm->mbm_d2f;
	struct sn_mail_box_f2d __iomem *f2d_mb = (struct sn_mail_box_f2d __iomem *)thwm->mbm_f2d;

	/* Reset the FPS update status */
	f2d_mb->mbz_update_status = MBZ_FW_LOAD_IDLE;
	/* Notify ZSP of FPS firmware */
	d2f_mb->mbz_fw_info.fw_type = (fw_type | 0x10000);
}

static int copy_mbz_firmware(struct sn_tranx_t *tdev, const char *mbz_fw, bool print_version)
{
    const struct firmware *fw = NULL;
    void __iomem *load = tdev->bar4_virt;
    struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
    struct sn_mail_box_d2f __iomem *d2f_mb = (struct sn_mail_box_d2f __iomem *)thwm->mbm_d2f;
    int ret = 0;
    ret = request_firmware(&fw, mbz_fw, &tdev->pdev->dev);
    if (ret == -EINVAL || ret == -ENOENT) {
        sn_pri(tdev, SN_ERR, "hwm: request_firmware error %d: invalid or "
                "missing firmware file %s in /lib/firmware\n", ret, mbz_fw);
        return ret;
    } else if (ret) {
        sn_pri(tdev, SN_ERR, "hwm: request_firmware error %d\n", ret);
        return ret;
    }
    d2f_mb->mbz_fw_info.fw_size = fw->size;
    {
        uint8_t __iomem *image = (uint8_t __iomem *) load;
        unsigned i;
        if (print_version) {
            const char* version_key = strstr(fw->data, "\"version\":");
            if (version_key) {
                const char* version_start = version_key + 12;
                const char* version_end = strchr(version_start, '\"');
                sn_pri(tdev, SN_INF, "hwm: FPS version: %.*s\n", (version_end - version_start), version_start);
            } else {
                sn_pri(tdev, SN_ERR, "hwm: FPS version extraction failed\n");
            }
        }
        for (i = 0; i < fw->size && fw->data[i]; ++i) {
        }
        if (i != fw->size) {
            ++i;
        }
        memcpy_toio(image, &fw->data[i], fw->size - i);
        wmb();
        d2f_mb->mbz_fw_info.fw_size -= i;
    }
    release_firmware(fw);
    return ret;
}

int load_fps_subsystem(struct sn_tranx_t *tdev)
{
	static const char* fps_fw[] = {"lego0", "lego1", "inline", "system",
					"unittest0", "unittest1", "unittest2", "unittest3"};

	int ret = 0;
	u32 i, j;
	u32 ut_offset = tdev->fps_unittest_en * 4; //index offset for unit_test fw
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *f2d_mb = (struct sn_mail_box_f2d __iomem *)thwm->mbm_f2d;
	char buf[MAX_ENC_FW_NAME];
	bool is_es;
	char digit = f2d_mb->device_info.id_info.part_number[7];

	if (digit != '1' && digit != '2') {
		sn_pri(thwm->tdev, SN_ERR, "hwm: cannot determine board type.  Firmware cannot be loaded.\n");
		return -EINVAL;
	}

	is_es = (digit != '2');

	if (tdev->fps_unittest_en) {
		sn_pri(tdev, SN_INF,
			"hwm:[WARN] FPS Unit Test FW load requested. DO NOT USE to run use-case\n");
	}

	/* Load slice 0 and 1 fps firmwares. */
	for (j = 0; j < NUM_SLICES_PER_DEVICE; j++) {
		for (i = 0; i < NUM_MICROBLAZES_PER_SLICE; i++) {
			int fw_type;
			/* Reading status register of the microblaze */
			u32 fps_status = xav1_get_register(tdev, j, FPS_STATUS);
			if (fps_status & (0x1 << i)) {
				sprintf(buf, "%sama_fw_%s_%u.bin", (is_es) ? "ama_es/" : "",
					fps_fw[i + ut_offset], j);
				/* Copy microblaze firmware to DDR */
				ret = copy_mbz_firmware(tdev, buf, (!j && !i));
				if (ret != 0) {
					return ret;
				}
				/* fw_type goes from 0-7*/
				fw_type =  i + (j*NUM_MICROBLAZES_PER_SLICE);
				if (IS_PF(tdev)) {
					/* Notify ZSP of fps firmware */
					trigger_fps_load(tdev, fw_type);
				} else {
					ret = mailbox_vf_get_msg(tdev, EVENT_VF2PF_RESET_ENCODER_FW,
						  &fw_type, 0);
					if(ret != 0) {
						return ret;
					}
				}
				/* Read fps update status */
				ret = fps_update_status(tdev, fw_type);
				if (ret != 0) {
					return ret;
				}
				sn_pri(tdev, SN_INF,
					   "hwm: FPS firmware %s loaded successfully\n", buf);
			}
		}
	}
	return 0;

}

static int load_firmware(struct hwm_t *thwm)
{
	unsigned long delay = 1000;
	const struct firmware *fw;
	u32 val_dir, val_st;
	int ret = -EFAULT;
	void __iomem *load = thwm->tdev->bar2_virt + FW_LOAD_ADDR;
	void __iomem *sram = thwm->tdev->bar2_virt + ZSP_SRAM_ADDR;

	/* for test,  wait bootrom work done */
	while (delay--) {
		if (readl(load + CMD_DIR_ADDR) == 0)
			break;
		usleep_range(1000, 1020);
	}
	if (readl(load + CMD_DIR_ADDR) != 0) {
		sn_pri(thwm->tdev, SN_ERR,
		       "hwm: bootrom pcie boot failed, please reboot host.\n");
		return ret;
	}
	if (request_firmware(&fw, FIRMWARE, &thwm->tdev->pdev->dev)) {
		sn_pri(thwm->tdev, SN_ERR, "hwm: request_firmware failed.\n");
		goto out;
	}

	memcpy_toio(load, fw->data, fw->size);
	/* send command "go" to ep */
	writel(SIGNATURE_TO_EP, sram + CMD_DIR_ADDR);
	writel(GO_CMD, sram + CMD_RSN_ADDR);
	set_ddr_ecc_flag(thwm->tdev);

	/* check zsp respond */
	delay = jiffies + LOAD_TIMEOUT * HZ;
	while (time_before(jiffies, delay)) {
		val_dir = readl(sram + CMD_DIR_ADDR);
		val_st = readl(sram + CMD_RSN_ADDR);
		if ((val_dir == SIGNATURE_TO_RC) && (val_st == DONE)) {
			/* Check again if firmware check failed*/
			if (check_firmware(thwm, CHECK_TIMEOUT) ==0){
				ret = 0;
				sn_pri(thwm->tdev, SN_INF,
					"hwm: load firmware successfully.\n");
				goto out_release_fw;
			}
		}
		if ((val_dir == SIGNATURE_TO_RC) && (val_st == SIG_ERROR)) {
			sn_pri(thwm->tdev, SN_ERR,
			       "hwm: signature check failed!\n");
			goto out_release_fw;
		}
		usleep_range(100, 200);
	}
	sn_pri(thwm->tdev, SN_ERR, "hwm: load firmware failed\n");

out_release_fw:
	release_firmware(fw);
out:
	return ret;
}

void trigger_zsp_interrupt(struct sn_tranx_t *tdev)
{
	sn_wr_b2(tdev, CHIP_SFT_INT_CON_STUS, 0x0);
	sn_wr_b2(tdev, CHIP_SFT_INT_CON_STUS, 0x1);
	msleep(200);
	sn_wr_b2(tdev, CHIP_SFT_INT_CON_STUS, 0x0);
}

static int send_msg_to_zsp(struct sn_tranx_t *tdev,
			   struct sn_mail_box_d2f *data)
{
	int ret = 0;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_d2f __iomem *mb_info = (struct sn_mail_box_d2f __iomem *)thwm->mbm_d2f;
	unsigned long delay;

	if (down_timeout(&thwm->info_zsp_sem, SEND_INFO_TIMEOUT)) {
		sn_pri(tdev, SN_INF, "hwm: %s acquire the semaphore timeout\n",
			__func__);
		return -ETIME;
	}
	/*
	 * when zsp receive the interrupt,
	 * need to clear the share memory with zero after handling.
	 * so if event_ID and error_ID are not 0,
	 * that means zsp has not handled the last interrupt.
	 */
	delay = jiffies + WAIT_ZSP_TIME;
	while (time_before(jiffies, delay)) {
		if ((mb_info->event_ID + mb_info->error_ID) == 0)
			break;
		usleep_range(10, 20);
	}

	if ((mb_info->event_ID + mb_info->error_ID) == 0) {
		memcpy_toio(mb_info, data, sizeof(struct sn_mail_box_d2f));
		trigger_zsp_interrupt(tdev);
	} else {
		sn_pri(tdev, SN_INF, "hwm: %s,but ZSP not respond\n", __func__);
		ret = -EFAULT;
	}
	up(&thwm->info_zsp_sem);

	return ret;
}

int set_pf_vf_mode(struct sn_tranx_t *tdev, int mode)
{
	struct sn_mail_box_d2f data;

	tdev->pf_vf_mode = mode;
	memset(&data, 0x0, sizeof(data));
	data.event_ID = EVENT_PF2FW_SET_PF_VF_MODE;
	data.param[0] = mode;

	return send_msg_to_zsp(tdev, &data);
}

static void ma35_get_config(struct sn_tranx_t *tdev, unsigned int *config)
{

	unsigned int s1_config = 0;
	unsigned int s2_config = 0;

#if (SUB_SYS_VCD == 1 && S1_VCD_A == 1 && S1_VCD_B == 1)
	s1_config |= (1 << SYS_CTL_VCDA);
	s1_config |= (1 << SYS_CTL_VCDB);
#elif ((SUB_SYS_VCD == 1 && S1_VCD_A == 1)  || VCMD_ENABLE_VC8000D == 1)
	s1_config |= (1 << SYS_CTL_VCDA);
#elif (SUB_SYS_VCD == 1 && S1_VCD_B == 1)
	s1_config |= (1 << SYS_CTL_VCDB);
#endif

#if ((SUB_SYS_VCE == 1  && S1_VCE == 1) || VCMD_ENABLE_VC8000E == 1)
	s1_config |= (1 << SYS_CTL_VCE);
#endif

#if (SUB_SYS_XABR == 1)
	s1_config |= (1 << SYS_CTL_XABR);
	s2_config |= (1 << SYS_CTL_XABR);
#endif

#if (SUB_SYS_XAV1 == 1)
	s1_config |= (1 << SYS_CTL_XENC) | (1 << SYS_CTL_XFPS) | (1 << SYS_CTL_XAV1);
	s2_config |= (1 << SYS_CTL_XENC) | (1 << SYS_CTL_XFPS) | (1 << SYS_CTL_XAV1);
#endif

	*config++ = s1_config;

#if (SUB_SYS_VCD == 1 && S2_VCD_A == 1 && S2_VCD_B == 1)
	s2_config |= (1 << SYS_CTL_VCDA);
	s2_config |= (1 << SYS_CTL_VCDB);
#elif ((SUB_SYS_VCD == 1 && S2_VCD_A == 1) || VCMD_ENABLE_VC8000D == 1)
	s2_config |= (1 << SYS_CTL_VCDA);
#elif (SUB_SYS_VCD == 1 && S2_VCD_B == 1)
	s2_config |= (1 << SYS_CTL_VCDB);
#endif

#if ((SUB_SYS_VCE == 1  && S2_VCE == 1) || VCMD_ENABLE_VC8000E == 1)
	s2_config |= (1 << SYS_CTL_VCE);
#endif

	*config = s2_config;
	return;
}

static int ma35_power_config_core(struct sn_tranx_t *tdev, u32 power_event,
								u32 *slice_config)
{
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *f2d_mb = thwm->mbm_f2d;
	struct sn_mail_box_d2f __iomem *d2f_mb = thwm->mbm_d2f;
	unsigned long delay = 200;

	d2f_mb->param[0] = slice_config[0];
	d2f_mb->param[1] = slice_config[1];
	/* Notify ZSP to configure IP power */
	d2f_mb->event_ID = power_event;

	/* wait for IP power up/down */
	while (delay--) {
		if (f2d_mb->power_event_status)
			break;
		msleep(10);
	}

	if(!f2d_mb->power_event_status) {
		sn_pri(tdev, SN_ERR, "hwm: ma35_power_config_core timed out.\n");
		return -1;
	} else if(f2d_mb->power_event_status < 0) {
		sn_pri(tdev, SN_ERR, "hwm: ma35_power_config_core failed.\n");
		f2d_mb->power_event_status = 0;
		return -1;
	}

	/* Reset the flag */
	f2d_mb->power_event_status = 0;
	return 0;
}

void ma35_kwork_power_setting(struct kthread_work* kwork)
{
	int ret = 0;
	struct ma35_kwork_ps *kwork_ps = (struct ma35_kwork_ps *)kwork;
	struct sn_tranx_t *tdev = kwork_ps->tdev;
	struct mailbox_info __iomem *mailbox = tdev->bar2_virt + VF1_TO_PF_MAILBOX;

	ret = ma35_power_config_core(tdev, kwork_ps->power_event,
								kwork_ps->slice_config);
	if(ret < 0) {
		sn_pri(tdev, SN_ERR, "hwm: ma35_kwork_power_setting failed.\n");
		mailbox->pf_bhalf_ready = -1;
	}

	mailbox->pf_bhalf_ready = 1;
	return;
}

int ma35_ip_power_config(struct sn_tranx_t *tdev, u32 power_event)
{
	int ret = 0;
	u32 slice_config[2];

	ma35_get_config(tdev, slice_config);
	if (IS_PF(tdev)) {
		ret = ma35_power_config_core(tdev, power_event, slice_config);
	} else {
		ret = mailbox_vf_get_msg(tdev, power_event, slice_config, 0);
		if(!ret) {
			ret = ma35_isr_bh_status(tdev);
		}
	}
	return ret;
}

static irqreturn_t zsp_softint_isr(int index, void *data)
{
	struct sn_tranx_t *tdev = data;

	sn_pri(tdev, SN_INF, "hwm: %s.\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t vf_to_pf_isr(int index, void *data)
{
	struct sn_tranx_t *tdev = data;
	struct mailbox_info __iomem *box;
	struct pci_dev *pcie_dev = tdev->pdev;
	u32 val;

	sn_pri(tdev, SN_INF, "hwm: %s.\n", __func__);

	if ((tdev->vf_max_count == 2)) {
		val = readl(tdev->bar2_virt +
			    THS1_VF_PF_INT_CON_STUS); /* vf1 trigger */
		if (val & 0x1) {
			sn_pri(tdev, SN_INF, "hwm: %s vf1 trigger.\n",
			       __func__);
			box = tdev->bar2_virt + VF1_TO_PF_MAILBOX;
			if (box->event_id == EVENT_VF2PF_GET_HDMA_MSIX_INFO) {
				box->data[0] = readl(tdev->bar0_virt + 0x20000);
				box->data[1] = readl(tdev->bar0_virt + 0x20004);
				box->data[2] = readl(tdev->bar0_virt + 0x20008);
				sn_pri(tdev, SN_INF,
				       "hwm: %s 0x%x 0x%x 0x%x.\n", __func__,
				       box->data[0], box->data[1],
				       box->data[2]);
				box->pf_data_ready = 1;
			}
			writel(0,
			       tdev->bar2_virt +
				       THS1_VF_PF_INT_CON_STUS); /*clear int status */
		}

		val = readl(tdev->bar2_virt +
			    THS2_VF_PF_INT_CON_STUS); /* vf2 trigger */
		if (val & 0x1) {
			sn_pri(tdev, SN_INF, "hwm: %s vf2 trigger.\n",
			       __func__);
			box = tdev->bar2_virt + VF2_TO_PF_MAILBOX;
			if (box->event_id == EVENT_VF2PF_GET_HDMA_MSIX_INFO) {
				box->data[0] = readl(tdev->bar0_virt + 0x20000);
				box->data[1] = readl(tdev->bar0_virt + 0x20004);
				box->data[2] = readl(tdev->bar0_virt + 0x20008);
				sn_pri(tdev, SN_INF,
				       "hwm: %s 0x%x 0x%x 0x%x.\n", __func__,
				       box->data[0], box->data[1],
				       box->data[2]);
				box->pf_data_ready = 1;
			}
			writel(0,
			       tdev->bar2_virt +
				       THS2_VF_PF_INT_CON_STUS); /*clear interrupt status */
		}
	} else if ((tdev->vf_max_count == 1)) {
		val = readl(tdev->bar2_virt +
			    THS1_VF_PF_INT_CON_STUS); /* vf trigger */
		if (val & 0x1) {
			sn_pri(tdev, SN_INF, "hwm: %s vf trigger.\n", __func__);
			box = tdev->bar2_virt + VF1_TO_PF_MAILBOX;
			if (box->event_id == EVENT_VF2PF_GET_HDMA_MSIX_INFO) {
				box->data[0] = readl(tdev->bar0_virt + 0x20000);
				box->data[1] = readl(tdev->bar0_virt + 0x20004);
				box->data[2] = readl(tdev->bar0_virt + 0x20008);
				sn_pri(tdev, SN_INF,
				       "hwm: %s 0x%x 0x%x 0x%x.\n", __func__,
				       box->data[0], box->data[1],
				       box->data[2]);
				box->pf_data_ready = 1;
			}
			else if (box->event_id == EVENT_VF2PF_GET_LINK_STATUS) {
				pci_read_config_dword(pcie_dev, 0x80, (unsigned int*)&box->data[0]);
				box->pf_data_ready = 1;
			}
			else if (box->event_id == EVENT_VF2PF_RESET_ENCODER_FW) {
				trigger_fps_load(tdev, box->data[0]);
				box->pf_data_ready = 1;
			}
			else if((box->event_id == EVENT_VF2PF_IP_POWER_UP) ||
					(box->event_id == EVENT_VF2PF_IP_POWER_DOWN)) {
				tdev->kwork_ps->power_event = box->event_id;
				tdev->kwork_ps->slice_config[0] = box->data[0];
				tdev->kwork_ps->slice_config[1] = box->data[1];
				sn_pri(tdev, SN_INF, "hwm: %s In hw_monitor isr\n", __func__);
				val = kthread_queue_work(tdev->kworker_thread_ps,
										&tdev->kwork_ps->ma35_kwork);
				box->pf_data_ready = 1;
			}
			writel(0, tdev->bar2_virt + THS1_VF_PF_INT_CON_STUS); /*clear interrupt status */
		}
	} else {
		sn_pri(tdev, SN_INF, "hwm: %s vf_max_count:%d error.\n",
		       __func__, tdev->vf_max_count);
	}

	return IRQ_HANDLED;
}

static ssize_t reset_zsp_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	u32 reset;

	if (count == 0)
		return 0;

	if (sscanf(buf, "%d", &reset) != 1) {
		sn_pri(tdev, SN_ERR, "hwm: not in hex or decimal form.\n");
		return count;
	}
	if (reset == 1) {
		if (reset_zsp(tdev)) //reset_zsp
			sn_pri(tdev, SN_ERR,
			       "hwm: reset zsp failed, please reboot host.\n");
	}

	return count;
}

static ssize_t ddrbw_s1_axi_r_MBps_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	unsigned long count, timer, ddr_bw[DDR_CNT_SLICE];
	int i;

	for (i = 0; i < DDR_CNT_SLICE; i++) {
		if (mail_box->ddr_bw_info.slice1[i].timer_cnt == 0) {
			sn_pri(tdev, SN_ERR,
			       "hwm: slice1 ddr%d axi rd timer_cnt:0.\n", i);
			ddr_bw[i] = 0;
		} else {
			timer = mail_box->ddr_bw_info.slice1[i].timer_cnt;
			count = mail_box->ddr_bw_info.slice1[i].rd_cnt;
			ddr_bw[i] = (count * DDR_CTRL_FREQ) / timer;
		}
	}

	return sprintf(buf, "%ld %ld %ld %ld\n", ddr_bw[0], ddr_bw[1],
		       ddr_bw[2], ddr_bw[3]);
}

static ssize_t ddrbw_s1_axi_w_MBps_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	unsigned long count, timer, ddr_bw[DDR_CNT_SLICE];
	int i;

	for (i = 0; i < DDR_CNT_SLICE; i++) {
		if (mail_box->ddr_bw_info.slice1[i].timer_cnt == 0) {
			sn_pri(tdev, SN_ERR,
			       "hwm: slice1 ddr%d axi wr timer_cnt:0.\n", i);
			ddr_bw[i] = 0;
		} else {
			timer = mail_box->ddr_bw_info.slice1[i].timer_cnt;
			count = mail_box->ddr_bw_info.slice1[i].wr_cnt;
			ddr_bw[i] = (count * DDR_CTRL_FREQ) / timer;
		}
	}

	return sprintf(buf, "%ld %ld %ld %ld\n", ddr_bw[0], ddr_bw[1],
		       ddr_bw[2], ddr_bw[3]);
}

static ssize_t ddrbw_s1_dfi_r_MBps_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	unsigned long count, timer, ddr_bw[DDR_CNT_SLICE];
	int i;

	for (i = 0; i < DDR_CNT_SLICE; i++) {
		if (mail_box->ddr_bw_info.slice1[i].timer_cnt == 0) {
			sn_pri(tdev, SN_ERR,
			       "hwm: slice1 ddr%d dfi rd timer_cnt:0.\n", i);
			ddr_bw[i] = 0;
		} else {
			timer = mail_box->ddr_bw_info.slice1[i].timer_cnt;
			count = mail_box->ddr_bw_info.slice1[i].rd_dfi_cnt;
			ddr_bw[i] = (count * 16 * DDR_CTRL_FREQ) / timer;
		}
	}

	return sprintf(buf, "%ld %ld %ld %ld\n", ddr_bw[0], ddr_bw[1],
		       ddr_bw[2], ddr_bw[3]);
}

static ssize_t ddrbw_s1_dfi_w_MBps_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	unsigned long count, timer, ddr_bw[DDR_CNT_SLICE];
	int i;

	for (i = 0; i < DDR_CNT_SLICE; i++) {
		if (mail_box->ddr_bw_info.slice1[i].timer_cnt == 0) {
			sn_pri(tdev, SN_ERR,
			       "hwm: slice1 ddr%d dfi wr timer_cnt:0.\n", i);
			ddr_bw[i] = 0;
		} else {
			timer = mail_box->ddr_bw_info.slice1[i].timer_cnt;
			count = mail_box->ddr_bw_info.slice1[i].wr_dfi_cnt;
			ddr_bw[i] = (count * 16 * DDR_CTRL_FREQ) / timer;
		}
	}

	return sprintf(buf, "%ld %ld %ld %ld\n", ddr_bw[0], ddr_bw[1],
		       ddr_bw[2], ddr_bw[3]);
}

static ssize_t ddrbw_s2_axi_r_MBps_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	unsigned long count, timer, ddr_bw[DDR_CNT_SLICE];
	int i;

	for (i = 0; i < DDR_CNT_SLICE; i++) {
		if (mail_box->ddr_bw_info.slice2[i].timer_cnt == 0) {
			sn_pri(tdev, SN_ERR,
			       "hwm: slice2 ddr%d axi rd timer_cnt:0.\n", i);
			ddr_bw[i] = 0;
		} else {
			timer = mail_box->ddr_bw_info.slice2[i].timer_cnt;
			count = mail_box->ddr_bw_info.slice2[i].rd_cnt;
			ddr_bw[i] = (count * DDR_CTRL_FREQ) / timer;
		}
	}

	return sprintf(buf, "%ld %ld %ld %ld\n", ddr_bw[0], ddr_bw[1],
		       ddr_bw[2], ddr_bw[3]);
}

static ssize_t ddrbw_s2_axi_w_MBps_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	unsigned long count, timer, ddr_bw[DDR_CNT_SLICE];
	int i;

	for (i = 0; i < DDR_CNT_SLICE; i++) {
		if (mail_box->ddr_bw_info.slice2[i].timer_cnt == 0) {
			sn_pri(tdev, SN_ERR,
			       "hwm: slice2 ddr%d axi wr timer_cnt:0.\n", i);
			ddr_bw[i] = 0;
		} else {
			timer = mail_box->ddr_bw_info.slice2[i].timer_cnt;
			count = mail_box->ddr_bw_info.slice2[i].wr_cnt;
			ddr_bw[i] = (count * DDR_CTRL_FREQ) / timer;
		}
	}

	return sprintf(buf, "%ld %ld %ld %ld\n", ddr_bw[0], ddr_bw[1],
		       ddr_bw[2], ddr_bw[3]);
}

static ssize_t ddrbw_s2_dfi_r_MBps_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	unsigned long count, timer, ddr_bw[DDR_CNT_SLICE];
	int i;

	for (i = 0; i < DDR_CNT_SLICE; i++) {
		if (mail_box->ddr_bw_info.slice2[i].timer_cnt == 0) {
			sn_pri(tdev, SN_ERR,
			       "hwm: slice2 ddr%d dfi rd timer_cnt:0.\n", i);
			ddr_bw[i] = 0;
		} else {
			timer = mail_box->ddr_bw_info.slice2[i].timer_cnt;
			count = mail_box->ddr_bw_info.slice2[i].rd_dfi_cnt;
			ddr_bw[i] = (count * 16 * DDR_CTRL_FREQ) / timer;
		}
	}

	return sprintf(buf, "%ld %ld %ld %ld\n", ddr_bw[0], ddr_bw[1],
		       ddr_bw[2], ddr_bw[3]);
}

static ssize_t ddrbw_s2_dfi_w_MBps_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	unsigned long count, timer, ddr_bw[DDR_CNT_SLICE];
	int i;

	for (i = 0; i < DDR_CNT_SLICE; i++) {
		if (mail_box->ddr_bw_info.slice2[i].timer_cnt == 0) {
			sn_pri(tdev, SN_ERR,
			       "hwm: slice2 ddr%d dfi wr timer_cnt:0.\n", i);
			ddr_bw[i] = 0;
		} else {
			timer = mail_box->ddr_bw_info.slice2[i].timer_cnt;
			count = mail_box->ddr_bw_info.slice2[i].wr_dfi_cnt;
			ddr_bw[i] = (count * 16 * DDR_CTRL_FREQ) / timer;
		}
	}

	return sprintf(buf, "%ld %ld %ld %ld\n", ddr_bw[0], ddr_bw[1],
		       ddr_bw[2], ddr_bw[3]);
}

static ssize_t uptime_s_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;

	return sprintf(buf, "%ld\n", mail_box->uptime_s);
}

static ssize_t version_information_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	int buf_len = 0;

	buf_len = sprintf(buf, "<<<Version Info>>>\n");
	buf_len += update_version(buf, "ZSP Version", mail_box->version_info.zsp_version);
	buf_len += update_version(buf, "SC Version", mail_box->version_info.sc_version);
	buf_len += update_version(buf, "eSecure Version", mail_box->version_info.esec_version);
	buf_len += update_version(buf, "PCIe FW Version", mail_box->version_info.pcie_version);
	buf_len += update_version(buf, "PCIe CTRL Patch Version", mail_box->version_info.pcie_ctrl_version);
	buf_len += update_version(buf, "PCIe PHY Patch A Version", mail_box->version_info.pcie_phy_a_version);
	return buf_len;
}

static ssize_t pcie_error_information_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;

	return sprintf(
		buf,
		"pcie uncorrected error counter =%d corrected error counter =%d\n",
		mail_box->error_info.pcie_error_counter & 0xFF,
		(mail_box->error_info.pcie_error_counter >> 16) & 0xFF);
}

static ssize_t axi_sram_error_information_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;

	return sprintf(buf,
		"THS 1 axi sram :error uncorrectable counter =%d, error correctable counter =%d \n"
		"THS 2 axi sram :error uncorrectable counter =%d, error correctable counter =%d \n",
		(mail_box->error_info.axi_sram_ecc_error_counter[0] >> 16) & 0xFFFF,
		(mail_box->error_info.axi_sram_ecc_error_counter[0]) & 0xFFFF,
		(mail_box->error_info.axi_sram_ecc_error_counter[1] >> 16) & 0xFFFF,
		(mail_box->error_info.axi_sram_ecc_error_counter[1]) & 0xFFFF);
}

static ssize_t ddr_error_information_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;

	return sprintf(
		buf,
		"ddr channel 0 :error uncorrectable counter =%d error correctable counter =%d \n \
ddr channel 1 :error uncorrectable counter =%d error correctable counter =%d \n \
ddr channel 2 :error uncorrectable counter =%d error correctable counter =%d \n \
ddr channel 3 :error uncorrectable counter =%d error correctable counter =%d \n \
ddr channel 4 :error uncorrectable counter =%d error correctable counter =%d \n \
ddr channel 5 :error uncorrectable counter =%d error correctable counter =%d \n \
ddr channel 6 :error uncorrectable counter =%d error correctable counter =%d \n \
ddr channel 7 :error uncorrectable counter =%d error correctable counter =%d \n ",
		(mail_box->error_info.ddr_ecc_error_counter[0] >> 16) & 0xFFFF,
		(mail_box->error_info.ddr_ecc_error_counter[0]) & 0xFFFF,
		(mail_box->error_info.ddr_ecc_error_counter[1] >> 16) & 0xFFFF,
		(mail_box->error_info.ddr_ecc_error_counter[1]) & 0xFFFF,
		(mail_box->error_info.ddr_ecc_error_counter[2] >> 16) & 0xFFFF,
		(mail_box->error_info.ddr_ecc_error_counter[2]) & 0xFFFF,
		(mail_box->error_info.ddr_ecc_error_counter[3] >> 16) & 0xFFFF,
		(mail_box->error_info.ddr_ecc_error_counter[3]) & 0xFFFF,
		(mail_box->error_info.ddr_ecc_error_counter[4] >> 16) & 0xFFFF,
		(mail_box->error_info.ddr_ecc_error_counter[4]) & 0xFFFF,
		(mail_box->error_info.ddr_ecc_error_counter[5] >> 16) & 0xFFFF,
		(mail_box->error_info.ddr_ecc_error_counter[5]) & 0xFFFF,
		(mail_box->error_info.ddr_ecc_error_counter[6] >> 16) & 0xFFFF,
		(mail_box->error_info.ddr_ecc_error_counter[6]) & 0xFFFF,
		(mail_box->error_info.ddr_ecc_error_counter[7] >> 16) & 0xFFFF,
		(mail_box->error_info.ddr_ecc_error_counter[7]) & 0xFFFF);
}

static struct pvt_process_corner pvt_corner_info[] = {
	{ "LVT NMOS PMOS", 1364, 1764, 2190, 0 },
	{ "LVT NMOS ", 219, 434, 691, 1 },
	{ "LVT PMOS", 60, 229, 507, 2 },
	{ "RVT NMOS PMOS", 1026, 1369, 1737, 3 },
	{ "RVT NMOS ", 56, 186, 397, 4 },
	{ "RVT PMOS", 11, 76, 254, 5 },
};

static ssize_t pvt_process_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	char process_val[3][6][10];
	unsigned char i, j;
	unsigned int count =
		sizeof(pvt_corner_info) / sizeof(pvt_corner_info[0]);
	for (i = 0; i < 3; i++) {
		for (j = 0; j < count; j++) {
			if ((mail_box->pvt_info.process[i][j] <=
			     pvt_corner_info[j].min))
				strcpy(process_val[i][j], "SS");
			else if ((mail_box->pvt_info.process[i][j] >
				  pvt_corner_info[j].min) &&
				 (mail_box->pvt_info.process[i][j] <=
				  pvt_corner_info[j].middle))
				strcpy(process_val[i][j], "TT");
			else if ((mail_box->pvt_info.process[i][j] >=
				  pvt_corner_info[j].middle))
				strcpy(process_val[i][j], "FF");
		}
	}

	return sprintf(buf, "<<<Process>>> \n \
<<< Sensor 0>>> \n \
LVT NMOS and PMOS : %s \n \
LVT NMOS : %s \n \
LVT PMOS : %s \n \
RVT NMOS and PMOS : %s \n \
RVT NMOS : %s \n \
RVT PMOS : %s \n \
<<< Sensor 1>>> \n \
LVT NMOS and PMOS : %s \n \
LVT NMOS : %s \n \
LVT PMOS : %s \n \
RVT NMOS and PMOS : %s \n \
RVT NMOS : %s \n \
RVT PMOS : %s \n \
<<< Sensor 2>>> \n \
LVT NMOS and PMOS : %s \n \
LVT NMOS : %s \n \
LVT PMOS : %s \n \
RVT NMOS and PMOS : %s \n \
RVT NMOS : %s \n \
RVT PMOS : %s \n ",
		       process_val[0][0], process_val[0][1], process_val[0][2],
		       process_val[0][3], process_val[0][4], process_val[0][5],
		       process_val[1][0], process_val[1][1], process_val[1][2],
		       process_val[1][3], process_val[1][4], process_val[1][5],
		       process_val[2][0], process_val[2][1], process_val[2][2],
		       process_val[2][3], process_val[2][4], process_val[2][5]);
}

static ssize_t pvt_process_sensor0_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	return sprintf(buf, "<<<Process Sensor0>>> \n \
LVT NMOS and PMOS (Typical code: TT(1764)/FF(2190)/SS(1364)): %d \n \
LVT NMOS (Typical code: TT(434)/FF(691)/SS(219)): %d \n \
LVT PMOS (Typical code: TT(229)/FF(507)/SS(60)): %d \n \
RVT NMOS and PMOS (Typical code: TT(1369)/FF(1737)/SS(1026)): %d \n \
RVT NMOS (Typical code: TT(186)/FF(397)/SS(56)): %d \n \
RVT PMOS (Typical code: TT(76)/FF(254)/SS(11)): %d \n ",
		       mail_box->pvt_info.process[0][0],
		       mail_box->pvt_info.process[0][1],
		       mail_box->pvt_info.process[0][2],
		       mail_box->pvt_info.process[0][3],
		       mail_box->pvt_info.process[0][4],
		       mail_box->pvt_info.process[0][5]);
}

static ssize_t pvt_process_sensor1_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	return sprintf(buf, "<<<Process Sensor1>>> \n \
LVT NMOS and PMOS (Typical code: TT(1764)/FF(2190)/SS(1364)): %d \n \
LVT NMOS (Typical code: TT(434)/FF(691)/SS(219)): %d \n \
LVT PMOS (Typical code: TT(229)/FF(507)/SS(60)): %d \n \
RVT NMOS and PMOS (Typical code: TT(1369)/FF(1737)/SS(1026)): %d \n \
RVT NMOS (Typical code: TT(186)/FF(397)/SS(56)): %d \n \
RVT PMOS (Typical code: TT(76)/FF(254)/SS(11)): %d \n ",
		       mail_box->pvt_info.process[1][0],
		       mail_box->pvt_info.process[1][1],
		       mail_box->pvt_info.process[1][2],
		       mail_box->pvt_info.process[1][3],
		       mail_box->pvt_info.process[1][4],
		       mail_box->pvt_info.process[1][5]);
}
static ssize_t pvt_process_sensor2_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	return sprintf(buf, "<<<Process Sensor2>>> \n \
LVT NMOS and PMOS (Typical code: TT(1764)/FF(2190)/SS(1364)): %d \n \
LVT NMOS (Typical code: TT(434)/FF(691)/SS(219)): %d \n \
LVT PMOS (Typical code: TT(229)/FF(507)/SS(60)): %d \n \
RVT NMOS and PMOS (Typical code: TT(1369)/FF(1737)/SS(1026)): %d \n \
RVT NMOS (Typical code: TT(186)/FF(397)/SS(56)): %d \n \
RVT PMOS (Typical code: TT(76)/FF(254)/SS(11)): %d \n ",
		       mail_box->pvt_info.process[2][0],
		       mail_box->pvt_info.process[2][1],
		       mail_box->pvt_info.process[2][2],
		       mail_box->pvt_info.process[2][3],
		       mail_box->pvt_info.process[2][4],
		       mail_box->pvt_info.process[2][5]);
}

static ssize_t pvt_temperature_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	int i;
	int count;

	count = sprintf(buf, "<<<Temperature>>>\n");
	for (i = 0; i < PVT_NUM; i++)
		count += sprintf(buf + count, " sensor%d=%d;\n", i, mail_box->pvt_info.temperature[i]);
	return count;
}

static ssize_t pvt_voltage_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	unsigned char i;
	unsigned long vol_val[4];
	// vol = adc *0.315 *(VREF/1.290)
	for (i = 0; i < 4; i++) {
		vol_val[i] = ((mail_box->pvt_info.voltage[i] * 315) *
			      ((PVT_VREF * 1000) / 1290));
		vol_val[i] = vol_val[i] / 1000000;
	}

	return sprintf(buf, "<<<Voltage>>>\n \
sensor0(0.75v)=%ld mV;\n \
sensor1(0.75v)=%ld mV;\n \
sensor2(0.85v)=%ld mV;\n \
sensor3(0.75v)=%ld mV;\n",vol_val[0], vol_val[1], vol_val[2], vol_val[3]);
}

static ssize_t board_temp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	int buf_len = 0;

	buf_len = sprintf(buf, "<<<Board temperature>>> \n \tBoard Temperature = %d\n", mail_box->board_temp);
	buf_len += sprintf(buf + strlen(buf), "\t VRM Temperature = %d\n", mail_box->vrm_temp.vrm_temperature);
	buf_len += sprintf(buf + strlen(buf), "\t VRM Max Temperature = %d\n", mail_box->vrm_temp.vrm_max_temperature);
	return buf_len;
}

static ssize_t board_power_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	unsigned int board_power = ((mail_box->board_vc.v3_pex_voltage * mail_box->board_vc.v3_pex_current) +
							   (mail_box->board_vc.v3_aux_voltage * mail_box->board_vc.v3_aux_current) +
							   (mail_box->board_vc.v12_pex_voltage * mail_box->board_vc.v12_pex_current))/1000;

	return sprintf(buf, " <<<Board power>>> \n Board Power = %d mW \n", board_power);
}

static ssize_t board_vc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	int buf_len = 0;

	buf_len = sprintf(buf, "<<<Board Voltage, Current>>>\n");
	buf_len += update_board_vc(buf, "3V PEX Voltage", "mV", mail_box->board_vc.v3_pex_voltage);
	buf_len += update_board_vc(buf, "3V AUX Voltage", "mV", mail_box->board_vc.v3_aux_voltage);
	buf_len += update_board_vc(buf, "12V PEX Voltage", "mV", mail_box->board_vc.v12_pex_voltage);
	buf_len += update_board_vc(buf, "3V PEX Current", "mA", mail_box->board_vc.v3_pex_current);
	buf_len += update_board_vc(buf, "3V AUX Current", "mA", mail_box->board_vc.v3_aux_current);
	buf_len += update_board_vc(buf, "12V PEX Current", "mA", mail_box->board_vc.v12_pex_current);
	buf_len += update_board_vc(buf, "VRM ASIC1 Voltage", "mV", mail_box->vrm_vc.asic1_voltage);
	buf_len += update_board_vc(buf, "VRM ASIC2 Voltage", "mV", mail_box->vrm_vc.asic2_voltage);
	buf_len += update_board_vc(buf, "VRM ASIC1 Current", "mA", mail_box->vrm_vc.asic1_current);
	buf_len += update_board_vc(buf, "VRM ASIC2 Current", "mA", mail_box->vrm_vc.asic2_current);
	buf_len += update_board_vc(buf, "ADC Voltage CH0", "mV", mail_box->adc_voltage.adc_voltage_ch0);
	buf_len += update_board_vc(buf, "ADC Voltage CH1", "mV", mail_box->adc_voltage.adc_voltage_ch1);
	buf_len += update_board_vc(buf, "ADC Voltage CH2", "mV", mail_box->adc_voltage.adc_voltage_ch2);
	buf_len += update_board_vc(buf, "ADC Voltage CH3", "mV", mail_box->adc_voltage.adc_voltage_ch3);
	buf_len += update_board_vc(buf, "ADC Voltage CH4", "mV", mail_box->adc_voltage.adc_voltage_ch4);
	buf_len += update_board_vc(buf, "ADC Voltage CH5", "mV", mail_box->adc_voltage.adc_voltage_ch5);
	buf_len += update_board_vc(buf, "ADC Voltage CH10", "mV", mail_box->adc_voltage.adc_voltage_ch10);

	return buf_len;
}

static ssize_t device_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;
	struct eeprom_data __iomem *device_info = &mail_box->device_info;

	return sprintf(buf, "<<<Device Info>>>\n Product name = %s\n Product revision = %s\n Product serial number = %s\n Part number = %s\n \
OEM ID = 0x%x \n PCIe vendor ID = 0x%x\n PCIe device ID = 0x%x\n PCIe sub vendor ID = 0x%x\n PCIe sub device ID = 0x%x\n Max power mode = %d\n",
					device_info->product_info.name, device_info->product_info.revision,
					device_info->product_info.serial_number, device_info->id_info.part_number,
					device_info->id_info.oem_id, ((device_info->id_info.pcie_info[0] << 8) | device_info->id_info.pcie_info[1]),
					((device_info->id_info.pcie_info[2] << 8) | device_info->id_info.pcie_info[3]), ((device_info->id_info.pcie_info[4] << 8) | device_info->id_info.pcie_info[5]),
					((device_info->id_info.pcie_info[6] << 8) | device_info->id_info.pcie_info[7]), device_info->id_info.max_power);
}

static ssize_t asic_num_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;

	return sprintf(buf, "<<<ASIC Number>>>\n ASIC Num = %d\n", mail_box->asic_num);
}

static ssize_t flash_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;

	/* If its 1, ASIC flash is actively used by SC */
	return sprintf(buf, "<<<Flash Status>>>\n Flash Status = %d\n", mail_box->flash_status);
}

static ssize_t sc_update_flag_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_d2f __iomem *mail_box = thwm->mbm_d2f;
	u32 val;

	if (count == 0)
		return 0;

	if (sscanf(buf, "%d", &val) != 1) {
		sn_pri(tdev, SN_ERR, "hwm: not in hex or decimal form.\n");
		return count;
	}

	if((val == 0) || (val == 1)) {
		mail_box->sc_update_flag = val;
	} else {
		sn_pri(tdev, SN_ERR, "hwm: Invalid sc update flag.\n");
		return count;
	}

	return count;
}

static ssize_t sc_update_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	struct sn_mail_box_f2d __iomem *mail_box = thwm->mbm_f2d;

	/* If its 1, SC firmware update is in progress */
	return sprintf(buf, "<<<SC Update Status>>>\n SC Update Status = %d\n", mail_box->sc_update_status);
}

static struct sn_mail_box_f2d __iomem * get_mail_box_f2d(struct device *dev)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	return thwm->mbm_f2d;

}

static ssize_t err_code_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sn_mail_box_f2d __iomem *mail_box = get_mail_box_f2d(dev);
	int buf_len = 0;

	buf_len = sprintf(buf, "ZSP error code = %d\n", mail_box->err_code & 0xFFFF);
	buf_len += sprintf(buf + strlen(buf), "DDR init retry count = %d\n", mail_box->err_code >> 16);
	return buf_len;
}

static ssize_t reset_encoder_fw_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct sn_misc_tdev *mtdev = dev_get_drvdata(dev);
	struct sn_tranx_t *tdev = mtdev->tdev;
	int ret;
	u32 val;

	if (count == 0)
		return 0;

	if (sscanf(buf, "%d", &val) != 1) {
		sn_pri(tdev, SN_ERR, "hwm: not in hex or decimal form.\n");
		return count;
	}

	if (val != 1) {
		sn_pri(tdev, SN_ERR, "hwm: Invalid encoder reset option.\n");
		return count;
	}

	if (val) {
		ret = xav1_soft_reset(tdev);
		if (ret) {
			sn_pri(tdev, SN_ERR, "hwm: Encoder soft reset failed.\n");
			return count;
		}
		ret = load_fps_subsystem(tdev);
		if (ret) {
			sn_pri(tdev, SN_ERR, "hwm: Encoder reset failed.\n");
		} else {
			sn_pri(tdev, SN_INF, "hwm: Encoder reset successful\n");
		}
    }

	return count;
}

static DEVICE_ATTR_RO(version_information);
static DEVICE_ATTR_RO(pcie_error_information);
static DEVICE_ATTR_RO(ddr_error_information);
static DEVICE_ATTR_RO(axi_sram_error_information);
static DEVICE_ATTR_RO(pvt_process);
static DEVICE_ATTR_RO(pvt_process_sensor0);
static DEVICE_ATTR_RO(pvt_process_sensor1);
static DEVICE_ATTR_RO(pvt_process_sensor2);
static DEVICE_ATTR_RO(pvt_voltage);
static DEVICE_ATTR_RO(pvt_temperature);
static DEVICE_ATTR_RO(uptime_s);
static DEVICE_ATTR_RO(ddrbw_s1_axi_r_MBps);
static DEVICE_ATTR_RO(ddrbw_s1_axi_w_MBps);
static DEVICE_ATTR_RO(ddrbw_s1_dfi_r_MBps);
static DEVICE_ATTR_RO(ddrbw_s1_dfi_w_MBps);
static DEVICE_ATTR_RO(ddrbw_s2_axi_r_MBps);
static DEVICE_ATTR_RO(ddrbw_s2_axi_w_MBps);
static DEVICE_ATTR_RO(ddrbw_s2_dfi_r_MBps);
static DEVICE_ATTR_RO(ddrbw_s2_dfi_w_MBps);
static DEVICE_ATTR_RO(board_temp);
static DEVICE_ATTR_RO(board_power);
static DEVICE_ATTR_RO(board_vc);
static DEVICE_ATTR_RO(device_info);
static DEVICE_ATTR_RO(asic_num);
static DEVICE_ATTR_WO(reset_zsp);
static DEVICE_ATTR_RO(flash_status);
static DEVICE_ATTR_WO(sc_update_flag);
static DEVICE_ATTR_RO(sc_update_status);
static DEVICE_ATTR_RO(err_code);
static DEVICE_ATTR_WO(reset_encoder_fw);

static struct attribute *sn_zsp_sysfs_entries[] = {
	&dev_attr_version_information.attr,
	&dev_attr_pcie_error_information.attr,
	&dev_attr_ddr_error_information.attr,
	&dev_attr_axi_sram_error_information.attr,
	&dev_attr_pvt_voltage.attr,
	&dev_attr_pvt_temperature.attr,
	&dev_attr_uptime_s.attr,
	&dev_attr_ddrbw_s1_axi_r_MBps.attr,
	&dev_attr_ddrbw_s1_axi_w_MBps.attr,
	&dev_attr_ddrbw_s1_dfi_r_MBps.attr,
	&dev_attr_ddrbw_s1_dfi_w_MBps.attr,
	&dev_attr_ddrbw_s2_axi_r_MBps.attr,
	&dev_attr_ddrbw_s2_axi_w_MBps.attr,
	&dev_attr_ddrbw_s2_dfi_r_MBps.attr,
	&dev_attr_ddrbw_s2_dfi_w_MBps.attr,
	&dev_attr_board_temp.attr,
	&dev_attr_board_power.attr,
	&dev_attr_board_vc.attr,
	&dev_attr_device_info.attr,
	&dev_attr_asic_num.attr,
	&dev_attr_err_code.attr,
	&dev_attr_reset_encoder_fw.attr,
	NULL
};
static struct attribute *sn_pf_zsp_sysfs_entries[] = {
	&dev_attr_pvt_process.attr,
	&dev_attr_pvt_process_sensor0.attr,
	&dev_attr_pvt_process_sensor1.attr,
	&dev_attr_pvt_process_sensor2.attr,
	&dev_attr_reset_zsp.attr,
	&dev_attr_flash_status.attr,
	&dev_attr_sc_update_flag.attr,
	&dev_attr_sc_update_status.attr,
	NULL
};

static struct attribute_group sn_zsp_attribute_group = {
	.name = NULL,
	.attrs = sn_zsp_sysfs_entries,
};

static struct attribute_group sn_pf_zsp_attribute_group = {
	.name = NULL,
	.attrs = sn_pf_zsp_sysfs_entries,
};

#define PCIE_CONFIG_BASE ((volatile resource_size_t)(S1_SYS_CON_OFF + 0x9D0))

static bool check_switch_ddr_ecc(struct sn_tranx_t *tdev)
{
	void __iomem *sram = tdev->bar2_virt + ZSP_SRAM_ADDR;
	u32 var_ext = readl(sram + CMD_EXT_ADDR);
	if ((tdev->ddr_ecc_flag == 1 && var_ext == DDR_ECC_ENABLED)
		|| (tdev->ddr_ecc_flag == 0 && var_ext == DDR_ECC_DISABLED))
		return false;
	return true;
}

int sn_hwm_init(struct sn_tranx_t *tdev)
{
	int ret;
	int boot_mode;
	struct hwm_t *thwm;
	u32 val = 0;
	void __iomem *sram = tdev->bar2_virt + ZSP_SRAM_ADDR;

	thwm = kzalloc(sizeof(struct hwm_t), GFP_KERNEL);
	if (!thwm) {
		sn_pri(tdev, SN_ERR, "hwm: alloc hwm_t failed.\n");
		goto out;
	}
	tdev->modules[SN_MODULE_HW_MONITOR] = thwm;
	thwm->tdev = tdev;

	if (IS_PF(tdev)) {
		boot_mode = readl(tdev->bar2_virt + BOOT_MODE_STUS);
		sn_pri(tdev, SN_INF, "hwm: zsp boot mode: %s.\n",
			(boot_mode == BOOT_MODE_NORMAL)?"normal":((boot_mode == BOOT_MODE_PCIE)?"PCIE":"debug"));

		if (boot_mode == BOOT_MODE_NORMAL) {
			if (check_firmware(thwm, CHECK_TIMEOUT)) {
				sn_pri(tdev, SN_ERR, "hwm: fw was not ready after reset, please reboot host\n");
				goto out_free_hwm;
			}
			if (check_switch_ddr_ecc(tdev)) {
				sn_pri(tdev, SN_INF, "hwm: ddr ecc flag changed, now reset ZSP\n");
				writel(0, sram + CMD_DIR_ADDR);
				writel(0, sram + CMD_RSN_ADDR);
				writel(0, sram + CMD_EXT_ADDR);
				if (reset_zsp(tdev))  {//reset_zsp
					sn_pri(tdev, SN_ERR,
						"hwm: reset zsp failed, please reboot host.\n");
					goto out_free_hwm;
				}
				// set DDR ECC flag
				set_ddr_ecc_flag(tdev);

				// check firmware again
				if (check_firmware(thwm, LOAD_TIMEOUT)) {
					sn_pri(tdev, SN_ERR, "hwm: fw was not ready after reset, please reboot host\n");
					goto out_free_hwm;
				}
			}
		} else if (boot_mode == BOOT_MODE_PCIE) {
			if(load_firmware(thwm))
				goto out_free_hwm;
		} else {
			sn_pri(tdev, SN_ERR,
				"hwm: zsp boot mode abnormal [debug mode].\n");
			goto out_free_hwm;
		}

		ret = sysfs_create_group(&tdev->misc_dev->this_device->kobj,
					 &sn_pf_zsp_attribute_group);
		if (ret) {
			sn_pri(tdev, SN_ERR,
			       "hwm: failed to create sysfs device attributes.\n");
			goto out_free_hwm;
		}

		thwm->zsp_soft_irq = tdev->msix_entries[ZSP_CPU_IRQ_INDEX].vector;
		thwm->vf_pf_irq = tdev->msix_entries[VF_PF_IRQ_INDEX].vector;
		if (!thwm->zsp_soft_irq || !thwm->vf_pf_irq) {
			sn_pri(tdev, SN_INF,
			       "hwm: get irq failed, zsp_soft_irq:%d vf_pf_irq:%d.\n",
			       thwm->zsp_soft_irq, thwm->vf_pf_irq);
			goto out_free_hwm;
		}

		ret = request_irq(thwm->zsp_soft_irq, zsp_softint_isr, IRQF_SHARED,
				  "zsp_softint", tdev);
		if (ret) {
			sn_pri(tdev, SN_ERR, "hwm: request zsp soft irq failed.\n");
			goto out_free_hwm;
		}

		ret = request_irq(thwm->vf_pf_irq, vf_to_pf_isr, IRQF_SHARED,
				  "vf_to_pf", tdev);
		if (ret) {
			sn_pri(tdev, SN_ERR, "hwm: request vf_to_pf irq failed.\n");
			goto out_free_zsp_softint;
		}

		spin_lock_init(&thwm->zsp_soft_lock);
		spin_lock_init(&thwm->pf_to_vf1_lock);
		spin_lock_init(&thwm->pf_to_vf2_lock);
		sema_init(&thwm->info_zsp_sem, 1);

		//soc set vip sram, ema=010, emaw=11, emas=0
		val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x388);
		sn_pri(tdev, SN_DBG, "hwm: read 0x388, val 0x%x \n", val);
		val = (val | 0x6);

		writel(val, (tdev->bar2_virt + TOP_SYS_CON_OFF + 0x388));
		val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x388);
		sn_pri(tdev, SN_DBG, "hwm: read 0x388, val 0x%x \n", val);

		val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x38c);
		sn_pri(tdev, SN_DBG, "hwm: read 0x38c, val 0x%x \n", val);
		val = (val | 0x6);

		writel(val, (tdev->bar2_virt + TOP_SYS_CON_OFF + 0x38c));
		val = readl(tdev->bar2_virt + TOP_SYS_CON_OFF + 0x38c);
		sn_pri(tdev, SN_DBG, "hwm: read 0x38c, val 0x%x \n", val);

	}

	thwm->mbm_f2d = tdev->bar4_virt + MAILBOX_BASE_ADDRESS;             /* size: 0x8000 */
	thwm->mbm_d2f = tdev->bar4_virt + MAILBOX_BASE_ADDRESS + MAILBOX_RESERVE_SIZE;

	ret = sysfs_create_group(&tdev->misc_dev->this_device->kobj,
					 &sn_zsp_attribute_group);
	if (ret) {
		sn_pri(tdev, SN_ERR,
		       "hwm: failed to create sysfs device attributes.\n");
		goto out_free_hwm;
	}
	sn_pri(tdev, SN_INF, "hwm: submodule inserted done.\n");
	sn_pri(tdev, SN_INF, "Serial Number: %s\n", ((struct sn_mail_box_f2d __iomem *) thwm->mbm_f2d)->device_info.product_info.serial_number);
	return 0;

out_free_zsp_softint:
	free_irq(thwm->zsp_soft_irq, (void *)tdev);
out_free_hwm:
	kfree(thwm);
	tdev->modules[SN_MODULE_HW_MONITOR] = NULL;
	sn_pri(tdev, SN_ERR, "hwm: transzsp probe failed.\n");
out:
	return -EFAULT;
}

void sn_hwm_release(struct sn_tranx_t *tdev)
{
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];

	if (IS_PF(tdev)) {
		if (BOOT_MODE_NORMAL != readl(tdev->bar2_virt + BOOT_MODE_STUS))
			if (reset_zsp(tdev)) //reset_zsp
				sn_pri(tdev, SN_DBG,
					"hwm: reset zsp failed, please reboot host.\n");

		free_irq(thwm->zsp_soft_irq, (void *)tdev);
		free_irq(thwm->vf_pf_irq, (void *)tdev);
		sysfs_remove_group(&tdev->misc_dev->this_device->kobj,
				   &sn_pf_zsp_attribute_group);
	}
	sysfs_remove_group(&tdev->misc_dev->this_device->kobj,
				   &sn_zsp_attribute_group);
	kfree(thwm);
	tdev->modules[SN_MODULE_HW_MONITOR] = NULL;
	sn_pri(tdev, SN_DBG, "hwm: remove module done.\n");
}

int mailbox_pf_send_msg(struct sn_tranx_t *tdev, u32 *data, int cnt, int mode)
{
	int ret = 0;
	struct mailbox_info __iomem *mailbox;
	struct hwm_t *thwm = tdev->modules[SN_MODULE_HW_MONITOR];
	spinlock_t *lock = NULL;
	u32 delay;
	u32 int_con_status_offset;
	u32 mail_box_offset;
	int i = 0;

	if (cnt <= 0 || cnt > 60) {
		sn_pri(tdev, SN_ERR, "%s:there is a faulty cnt\n", __func__);
		return -EFAULT;
	}

	if (tdev->vf_index != 0) {
		sn_pri(tdev, SN_ERR, "%s:this is not in pf\n", __func__);
		return -EFAULT;
	} else {
		if (mode == PF_TO_VF1) {
			int_con_status_offset = THS1_PF_VF_INT_CON_STUS;
			mail_box_offset = PF_TO_VF1_MAILBOX;
			lock = &thwm->pf_to_vf1_lock;
		} else if (mode == PF_TO_VF2) {
			int_con_status_offset = THS2_PF_VF_INT_CON_STUS;
			mail_box_offset = PF_TO_VF2_MAILBOX;
			lock = &thwm->pf_to_vf2_lock;
		} else {
			sn_pri(tdev, SN_ERR,
			       "%s error please input correct mode\n",
			       __func__);
			return -EFAULT;
		}
		spin_lock(lock);
		/* Wait for the last interrupt to finish processing */
		delay = 1000;
		while (delay--) {
			if (readl(tdev->bar2_virt + int_con_status_offset) == 0)
				break;
			usleep_range(1000, 1020);
		}
		if (readl(tdev->bar2_virt + int_con_status_offset) != 0) {
			sn_pri(tdev, SN_ERR, "%s: send msg to vf failed.\n",
			       __func__);
			ret = -EFAULT;
			goto out;
		}
		/* send data to pf, then trigger a pf interrupt */
		mailbox = tdev->bar2_virt + mail_box_offset;
		mailbox->event_id = mode | (cnt << 3);
		for (i = 0; i < cnt; i++)
			mailbox->data[i] = *(data + i);

		/* trigger a pf interrupt */
		writel(0x1, tdev->bar2_virt + int_con_status_offset);
		spin_unlock(lock);
		return ret;
	out:
		writel(0x0, tdev->bar2_virt + int_con_status_offset);
		spin_unlock(lock);
		return ret;
	}
}
