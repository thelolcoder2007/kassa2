/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2018 Verisilicon Inc.
 */

#ifndef _SN_HW_MONITOR_H_
#define _SN_HW_MONITOR_H_

#if !defined(MA35_UTILITIES)
#include <linux/types.h>

#include "common.h"
#endif

#define MAILBOX1_OFFSET		0x800
#define MAILBOX2_OFFSET		0x860

typedef enum {
	MBZ_FW_LOAD_IDLE,
	MBZ_FW_LOAD_IN_PROGRESS,
	MBZ_FW_LOAD_SUCCESS,
	MBZ_FW_LOAD_ERROR,
	MBZ_FW_LOAD_INVALID,
	MBZ_FW_LOAD_AUTH_FAIL
}mbz_ret_code;

struct ddr_bw_type {
	unsigned int timer_cnt;
	unsigned int rd_cnt;
	unsigned int wr_cnt;
	unsigned int rd_dfi_cnt;
	unsigned int wr_dfi_cnt;
};

struct ddr_bw {
	struct ddr_bw_type slice1[4];
	struct ddr_bw_type slice2[4];
};

#define PVT_P_NUM                6
#define PVT_NUM                  4
#define PVT_VREF                 (1291)  //1.291 V
#define PVT_DOTP25               (1000)
struct pvt_information {
	unsigned int process[PVT_NUM - 1][PVT_P_NUM];
	unsigned int voltage[PVT_NUM];
	signed int temperature[PVT_NUM];
};

struct pvt_process_corner {
	char Name[255];
	unsigned int min;
	unsigned int middle;
	unsigned int max;
	unsigned int index;
};

struct version_info{
	unsigned int zsp_version;
	unsigned int sc_version;
	unsigned int esec_version;
	unsigned int pcie_version;
	unsigned int pcie_ctrl_version;
	unsigned int pcie_phy_a_version;
};

struct board_vc_data{
	u16 v12_pex_current;
	u16 v3_aux_current;
	u16 v3_pex_current;
	u16 v12_pex_voltage;
	u16 v3_aux_voltage;
	u16 v3_pex_voltage;
};

struct product_info{
    u8 name[24];
    u8 revision[8];
    u8 serial_number[14];
    u8 reserved[2];
};

struct id_info{
    u8 part_number[24];
    unsigned int oem_id;
    u8 pcie_info[8];
    unsigned int max_power;
};

struct eeprom_data{
    struct product_info product_info;
    struct id_info id_info;
};

struct vrm_vc{
    u16 asic1_voltage;
    u16 asic2_voltage;
    u16 asic1_current;
    u16 asic2_current;
};

struct adc_v{
    u16 adc_voltage_ch0;
    u16 adc_voltage_ch1;
    u16 adc_voltage_ch2;
    u16 adc_voltage_ch3;
    u16 adc_voltage_ch4;
    u16 adc_voltage_ch5;
    u16 adc_voltage_ch10;
    u16 reserved;
};

struct vrm_temp{
    u8 vrm_max_temperature;
    u8 vrm_temperature;
    u8 reserved[2];
};

struct error_information {
	unsigned int
		pcie_error_counter; //   pcie_correctable_error_counter;  pcie_uncorrectable_error_counter;
	unsigned int ddr_ecc_error_counter
		[8]; //    ddr_correctable_error_counter;  ddr_uncorrectable_error_counter;
	unsigned int axi_sram_ecc_error_counter
		[2]; //    axi_sram_correctable_error_counter;  axi_sram_uncorrectable_error_counter;
};

/* firmware will update hardware information to MAILBOX register, so driver can get it.
 * [firmware ---> driver], reserve size is 0x164; THS1_VF_MAILBOX28~117
 */
struct sn_mail_box_f2d {
	struct version_info version_info;
	unsigned int event_ID;
	unsigned int process_status;
	struct ddr_bw ddr_bw_info;
	unsigned long uptime_s;
	struct pvt_information pvt_info;
	struct error_information error_info;
	unsigned int board_temp;
	struct board_vc_data board_vc;
	struct eeprom_data device_info;
	unsigned int asic_num;
	unsigned int flash_status;
	unsigned int sc_update_status;
	struct vrm_vc vrm_vc;
	struct adc_v adc_voltage;
	struct vrm_temp vrm_temp;
	unsigned int err_code;
	int mbz_update_status;
	/* Do not add/change anything above */
	int power_event_status;
};

struct mbz_fw_info {
	unsigned int fw_type;
	unsigned int fw_size;
};

/* it's a share memory between driver and firmware, the memory in MAILBOX register  ep side.
 * driver can send information to firmware. [ driver ---> firmware ]
 * , reserve size is0x164;THS2_VF_MAILBOX28~117
 */
struct sn_mail_box_d2f {
	unsigned long event_ID;
	unsigned long error_ID;
	unsigned int param[8];
	unsigned int sc_update_flag;
	struct mbz_fw_info mbz_fw_info;
	/* Do not add/change anything above */
};

#if !defined(MA35_UTILITIES)
int sn_hwm_init(struct sn_tranx_t *tdev);
void sn_hwm_release(struct sn_tranx_t *tdev);

int set_pf_vf_mode(struct sn_tranx_t *tdev, int mode);
int ma35_ip_power_config(struct sn_tranx_t *tdev, u32 power_event);
void ma35_kwork_power_setting(struct kthread_work* kwork);
void trigger_zsp_interrupt(struct sn_tranx_t *tdev);
int mailbox_pf_send_msg(struct sn_tranx_t *tdev, u32 *data, int cnt, int mode);
int load_fps_subsystem(struct sn_tranx_t *tdev);
#endif

#endif /* _SN_HW_MONITOR_H_ */
