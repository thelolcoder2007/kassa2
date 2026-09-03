/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2020 Verisilicon Inc.
 */

#ifndef __TRANSCODER_H__
#define __TRANSCODER_H__

 /**
	* This file largely serves two purposes. (1) It defines data structures and enumeration values used
	* by the driver, and by clients of the driver to interact with it. (2) It defines the ioctl values
	* used to interact with the driver. For the latter, it defines macros which in turn use _IOR, _IOW,
	* and _IORW. These are defined in <linux/ioctl.h>.
	*
	* It is a classic problem that the driver code needs to interact with the kernel, and thus use kernel
	* types; but the client using the driver wants to work with user space types. See
	* https://lwn.net/Articles/113349/ for one discussion of this. For now we are not going to use
	* __KERNEL__ as a discriminator, but instead rely on __linux__ vs. _WIN32. This means Linux client
	* code is using the kernel types. Hopefully all are in sync and this makes no difference.
	*/
#if !defined(__MICROBLAZE__) && defined(__linux__)
#include <linux/types.h>
#include <linux/ioctl.h>
#endif
#if defined(_WIN32)
#include <stdint.h>
typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int8_t   __s8;
typedef int16_t  __s16;
typedef int32_t  __s32;
typedef int64_t  __s64;
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum TRANS_HDMA_DIR {
	RC2EP = 0,
	EP2RC = 1,
};

enum TRANS_HDMA_INT {
	POLL_EN = 0,
	INTE_EN = 1,
};

/* pcie bar region description */
struct bar_info {
	__u32 bar_num;
	__u64 bar_addr;
	__u32 bar_size;
};

/* hdma info description */
struct trans_pcie_hdma {
	__u32 size; /* transfer size */
	__u64 sar; /* source address 64 bits */
	__u64 dar; /* destination address 64 bits */
	__u32 dir; /* direction: RC2EP or RC2EP ,need rename to dir */
	__s32 fd; /* fd: owner of the ep address */
};

struct reg_desc {
	__u32 id; /* register index, starting from 0 */
	__u32 val; /* value */
};

struct mem_info {
	__s32 task_id; /* task id */
	__u64 phy_addr; /* physics address */    // alloc(out), free(in)
	__u32 size;                              // alloc only (and optional validation on free?)
	__s32 fd; /*device handle*/            // THIS IS UNNECESSARY (flags for OSAL?)
};

struct release_addr_t {
	__u32 pc_l; /* pc low val */
	__u32 pc_h; /* pc high val */
};

/* ep memory used information */
struct mem_used_info {
	__u32 s1_used;
	__u32 s1_free;
	__u32 s1_blk_used;
	__u32 s2_used;
	__u32 s2_free;
	__u32 s2_blk_used;
};

/* for vc8000d */
enum vcd_core_type {
	/* Decoder */
	HW_VC8000D = 0,
	HW_VC8000DJ,
	HW_BIGOCEAN,
	HW_VCMD,
	HW_MMU,
	HW_MMU_WR,
	HW_DEC400,
	HW_L2CACHE,
	HW_SHAPER,
	/* Encoder*/
	/* Auxiliary IPs */
	HW_NOC,
	HW_AXIFE,
	HW_AFBC,
	HW_CORE_MAX /* max number of cores supported */
};

enum vcd_codec_type {
	VC8000D_CLIENT_TYPE_H264_DEC = 1,
	VC8000D_CLIENT_TYPE_MPEG4_DEC,
	VC8000D_CLIENT_TYPE_JPEG_DEC,
	VC8000D_CLIENT_TYPE_PP,
	VC8000D_CLIENT_TYPE_VC1_DEC,
	VC8000D_CLIENT_TYPE_MPEG2_DEC,
	VC8000D_CLIENT_TYPE_VP6_DEC,
	VC8000D_CLIENT_TYPE_AVS_DEC,
	VC8000D_CLIENT_TYPE_RV_DEC,
	VC8000D_CLIENT_TYPE_VP8_DEC,
	VC8000D_CLIENT_TYPE_VP9_DEC,
	VC8000D_CLIENT_TYPE_HEVC_DEC,
	VC8000D_CLIENT_TYPE_ST_PP,
	VC8000D_CLIENT_TYPE_H264_MAIN10,
	VC8000D_CLIENT_TYPE_AVS2_DEC,
	VC8000D_CLIENT_TYPE_AV1_DEC,
	VC8000D_CLIENT_TYPE_BO_AV1_DEC,
	VC8000D_CLIENT_TYPE_MAX
};

#define CoreType vcd_core_type
enum MISC_IP_ID {
	F1_ID,
	F2_ID,
	F3_ID,
};
struct core_desc {
	__u32 id; /* id of the subsystem */
	__u32 type; /* type of core to be written */
	__u32 *regs; /* pointer to user registers */
	__u32 size; /* size of register space */
	__u32 reg_id; /* id of register to be read/written */
};

struct ip_desc {
	__u8 ip_id;
	struct core_desc core;
};

struct regsize_desc {
	__u32 slice; /* id of the slice */
	__u32 id; /* id of the subsystem */
	__u32 type; /* type of core to be written */
	__u32 size; /* iosize of the core */
};

struct core_param {
	__u32 slice; /* id of the slice */
	__u32 id; /* id of the subsystem */
	__u32 type; /* type of core to be written */
	__u32 size; /* iosize of the core */
	__u32 asic_id; /* asic id of the core */
};

struct subsys_desc {
	__u32 subsys_num; /* total subsystems count */
	__u32 subsys_vcmd_num; /* subsystems with vcmd */
};

struct axife_cfg {
	__u8 axi_rd_chn_num;
	__u8 axi_wr_chn_num;
	__u8 axi_rd_burst_length;
	__u8 axi_wr_burst_length;
	__u8 fe_mode;
	__u32 id;
};

struct apbfilter_cfg {
	__u32 nbr_mask_regs;
	__u32 mask_reg_offset;
	__u32 page_sel_addr;
	__u8 num_mode;
	__u8 mask_bits_per_reg;
	__u32 id; /* id of the subsystem */
	__u32 type; /* type of core to be written */
	__u32 has_apbfilter;
};

#define HW_ID_1_0_C 0x43421001
#define HW_ID_1_1_2 0x43421102

#define ANY_CMDBUF_ID 0xFFFF

#ifndef ASIC_SWREG_AMOUNT
#define ASIC_SWREG_AMOUNT 543 //from encswhwregister.h
#endif

/*initialized device nums */
#define INIT_DEVICE_CNT	4

/*these size need to be modified according to hw config.*/
#define ENCODER_REGISTER_SIZE ASIC_SWREG_AMOUNT
#define IM_REGISTER_SIZE ASIC_SWREG_AMOUNT
#define JPEG_ENCODER_REGISTER_SIZE ASIC_SWREG_AMOUNT

#define DEC400_REGISTER_SIZE 1600
#define MMU_REGISTER_SIZE 500
#define L2CACHE_REGISTER_SIZE 500
#define AXIFE_REGISTER_SIZE 500

/*module_type support*/

#define OSAL_ADDR_MSB             0
#define OSAL_ADDR_LSB             1
#define OSAL_ADDR_MSB_SHIFT       2

enum vcmd_module_type {
	VCMD_TYPE_ENCODER = 0,
	VCMD_TYPE_CUTREE,
	VCMD_TYPE_DECODER,
	VCMD_TYPE_JPEG_ENCODER,
	VCMD_TYPE_JPEG_DECODER,
	MAX_VCMD_TYPE
};

struct cmdbuf_mem_parameter {
	__u32 *virt_cmdbuf_addr;
	__u64 phy_cmdbuf_addr; //cmdbuf pool base physical address
	__u32 mmu_phy_cmdbuf_addr; //cmdbuf pool base mmu mapping address
	__u32 cmdbuf_total_size; //cmdbuf pool total size in bytes.
	__u16 cmdbuf_unit_size; //one cmdbuf size in bytes. all cmdbuf have same size.
	__u32 *virt_status_cmdbuf_addr;
	__u64 phy_status_cmdbuf_addr; //status cmdbuf pool base physical address
	__u32 mmu_phy_status_cmdbuf_addr; //status cmdbuf pool base mmu mapping address
	__u32 status_cmdbuf_total_size; //status cmdbuf pool total size in bytes.
	__u16 status_cmdbuf_unit_size; //one status cmdbuf size in bytes. all status cmdbuf have same size.
	__u64 phy_cmdbuf_patch_addr;  // cmdbuf patch pool base physical address
	__u32 cmdbuf_patch_total_size;  // cmdbuf patch pool total size in bytes
	__u32 *virt_cmdbuf_patch_addr;
	__u64 base_ddr_addr; //for pcie interface, hw can only access phy_cmdbuf_addr-pcie_base_ddr_addr.
		//for other interface, this value should be 0?
};

struct config_parameter {
	__u16 module_type; //input vc8000e=0,cutree=1,vc8000d=2,jpege=3, jpegd=4
	__u16 vcmd_core_num; //output, how many vcmd cores are there with corresponding module_type.
	__u16 submodule_main_addr; //output,if submodule addr == 0xffff, this submodule does not exist.
	__u16 submodule_dec400_addr; //output ,if submodule addr == 0xffff, this submodule does not exist.
	__u16 submodule_L2Cache_addr; //output,if submodule addr == 0xffff, this submodule does not exist.
	__u16 submodule_MMU_addr
		[2]; //output,if submodule addr == 0xffff, this submodule does not exist.
	__u16 submodule_axife_addr
		[2]; //output,if submodule addr == 0xffff, this submodule does not exist.
	__u16 config_status_cmdbuf_id; // output , this status comdbuf save the all register values read in driver init.//used for analyse configuration in cwl.
	__u32 vcmd_hw_version_id;
};

/*need to consider how many memory should be allocated for status.*/
struct exchange_parameter {
	__u64 executing_time; //input ;executing_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
	__u64 cmdbuf_addr; //input for cmdbuf,patchbuf - must convert to phy addr
	__u64 statusbuf_addr; //input for statusbuf - must convert to phy addr
	__u16 module_type; //input input vc8000e=0,IM=1,vc8000d=2, jpege=3, jpegd=4
	__u16 cmdbuf_size; //input, reserve is not used; link and run is input.
	__u16 priority; //input,normal=0, high/live=1
	__u16 cmdbuf_id; //output, it is unique in driver.
	__u16 core_id; //just used for polling.
	__u16 dec_path; // DEC_PATH_A or DEC_PATH_B or both
	__u16 ppubuf_size;
};

struct core_reserve_param {
	__u32 format; //codec format
	__u32 dec_path; //decoder path: path a, path b, path a+b
};
/* end for vc8000d */

enum { DEC_PATH_A = 0, DEC_PATH_B = 1, DEC_PATH_AB = 2, DEC_PATH_MAX };

/* for vc8000e */
enum {
	CORE_VC8000E = 0,
	CORE_VC8000EJ = 1,
	CORE_CUTREE = 2,
	CORE_DEC400 = 3,
	CORE_MMU = 4,
	CORE_L2CACHE = 5,
	CORE_AXIFE = 6,
	CORE_APBFT = 7,
	CORE_MMU_1 = 8,
	CORE_AXIFE_1 = 9,
	CORE_MAX
};
//#define CORE_MAX  (CORE_MMU)

struct subsys_core_info {
	__u32 type_info;
	__u64 offset[CORE_MAX];
	__u64 regSize[CORE_MAX];
	__s32 irq[CORE_MAX];
	char irq_name[CORE_MAX][32];
};

struct core_wait_out {
	__u32 job_id[4];
	__u32 irq_status[4];
	__u32 irq_num;
};
/* end for vc8000e */

/* for xav1 */
struct xav1_msg {
	__u16 slice;
	__u16 count;
	__u32 content[4];
};

struct xav1_wait_msg {
	__u16 slice;
	__u16 slot;
	__u32 content[4];
	__u32 timeout_ms;
};

/* end for xav1 */

/* sn_perf */

#include "sn_perf_types.h"

typedef struct {
	__u32 ipId : 7;
	__u32 cmd : 2;
	__u32 arg : 13;
} sn_perf_cmd;

typedef struct {
	union {
		struct {
			__u32 slice : 1;
			__u32 ipId : 7;
			__u32 unitId : 5;
			__u32 type : 2;
		};
		__u32 bits;
	};
	__u32 hwTs;
	__u32 payload;
	union {
		__u32 name;
		char namec[4];
	};
	__u64 kernelTsBefore;
	__u64 kernelTs;
} sn_perf_event;

typedef struct {
	__u32 count;
	__u32 timeoutMs;
	sn_perf_event events[];
} sn_perf_poll;

/* end for sn_perf */

// osal_queues
#if defined(USE_OSAL)
#include <common/osaltypes.h>
#endif

/* pcie submodule ioctl commands */
#define IOCTL_CMD_PCIE_MINNR 0x00
#define IOCTL_CMD_PCIE_MAXNR 0x01
#define SN_TRANX_GET_BARADDR _IOWR('k', 0x00, struct bar_info *)
#define SN_TRANX_SW_DDR_MAPPING _IOWR('k', 0x01, __u32 *)

/* hdma submodule ioctl commands */
#define IOCTL_CMD_HDMA_MINNR 0x02
#define IOCTL_CMD_HDMA_MAXNR 0x03
#define SN_TRANX_HDMA_TRANX_VIR _IOWR('k', 0x02, struct trans_pcie_hdma *)

/* memory submodule ioctl commands */
#define IOCTL_CMD_MEM_MINNR 0x05
#define IOCTL_CMD_MEM_MAXNR 0x0a
#define SN_TRANX_MEM_GET_UTIL _IOWR('k', 0x05, struct mem_used_info *)
#define SN_TRANX_MEM_FREE _IOWR('k', 0x07, struct mem_info *)
#define SN_TRANX_MEM_GET_TASKID _IOWR('k', 0x08, __s32 *)
#define SN_TRANX_MEM_FREE_TASKID _IOWR('k', 0x09, __s32 *)
#define SN_TRANX_MEM_ALLOC_HANDLE _IOWR('k', 0x0a, struct mem_info *)

/* vc8000e submodule ioctl commands */
#define HANTRO_IOC_SOFT_RESET _IO('k', 0x19)

/* vc8000d submodule ioctl commands */
#define IOCTL_CMD_VCD_MINNR 0x20
#define IOCTL_CMD_VCD_MAXNR 0x37
#define HANTRODEC_IOC_SOFT_RESET _IO('k', 0x35)

/* xabr submodule ioctl commands */
#define XABR_IOCTL_SOFT_RESET _IOWR('k', 0x65, __u32 *)
#define XABR_IOCTL_TWO_SLICE_SOFT_RESET _IO('k', 0x68)

/* xav1 enc submodule ioctl commands */
#define XAV1_ENC_IOCTL_TWO_SLICE_SOFT_RESET _IO('k', 0x70)

/* riscv submodule ioctl commands */
#define IOCTL_CMD_RISCV_IP_MINNR 0x80
#define IOCTL_CMD_RISCV_IP_MAXNR 0x89
#define RISCV_0_IOCS_POWER_ON _IOWR('k', 0x80, struct ip_desc *)
#define RISCV_0_IOCS_POWER_OFF _IOWR('k', 0x81, struct ip_desc *)
#define RISCV_0_IOCS_SOFT_RESET _IOWR('k', 0x82, struct ip_desc *)
#define RISCV_0_IOCS_RESET_REALEASE _IOWR('k', 0x83, struct ip_desc *)
#define RISCV_0_IOCS_RELEASE_ADDR _IOWR('k', 0x84, struct ip_desc *)
#define RISCV_1_IOCS_POWER_ON _IOWR('k', 0x85, struct ip_desc *)
#define RISCV_1_IOCS_POWER_OFF _IOWR('k', 0x86, struct ip_desc *)
#define RISCV_1_IOCS_SOFT_RESET _IOWR('k', 0x87, struct ip_desc *)
#define RISCV_1_IOCS_RESET_REALEASE _IOWR('k', 0x88, struct ip_desc *)
#define RISCV_1_IOCS_RELEASE_ADDR _IOWR('k', 0x89, struct ip_desc *)

/* gpu submodule ioctl commands */
#define IOCTL_CMD_GPU_MINNR 0x90
#define IOCTL_CMD_GPU_MAXNR          0x9f
#define IOCTL_GCHAL_INTERFACE            _IOWR('k', 0x99, void *)
#define IOCTL_GCHAL_PROFILER_INTERFACE   _IOWR('k', 0x9a, void *)
#define IOCTL_GCHAL_TERMINATE            _IOWR('k', 0x9b, void *)
#define IOCTL_GCHAL_GPU2D_SOFT_RESET     _IO('k', 0x90)
#define IOCTL_GCHAL_VIP_SOFT_RESET       _IO('k', 0x91)


/* perfcollector submodule */
#define IOCTL_CMD_PERF_MINNR 0xa0
#define IOCTL_CMD_PERF_MAXNR 0xa1
#define IOCTL_PERF_CMD _IOW('k', 0xa0, sn_perf_cmd *)
#define IOCTL_PERF_POLL _IOWR('k', 0xa1, sn_perf_poll *)

/* osal queues submodule */
typedef enum {
		OSAL_ACCL_CMD_VCMD_GET_CONFIGINFO = 1,
		OSAL_ACCL_CMD_VCMD_GET_BUFINFO,
		OSAL_ACCL_CMD_VCMD_CMDBUF_GET_POOL_SIZE,
		OSAL_ACCL_CMD_VCMD_CMDBUF_SET_POOL_BASE,
		OSAL_ACCL_CMD_VCMD_CMDBUF_PROCESS,
		OSAL_ACCL_CMD_VCMD_CMDBUF_RESERVE,
		OSAL_ACCL_CMD_VCMD_CMDBUF_LINK_RUN,
		OSAL_ACCL_CMD_VCMD_CMDBUF_RELEASE,
		OSAL_ACCL_CMD_VCMD_CMDBUF_POLLING,
		OSAL_ACCL_CMD_VCMD_GET_NUMCORES,
		OSAL_ACCL_CMD_VCMD_GET_HWIOSIZE,
		OSAL_ACCL_CMD_VCMD_GET_REG_INFO,
		OSAL_ACCL_CMD_VCMD_END
} vcmd_osal_cmds;

typedef enum {
		OSAL_ACCEL_CMD_ENC_SEND_MSG = 0,
		OSAL_ACCEL_CMD_ENC_ALLOC_SLOT,
		OSAL_ACCEL_CMD_ENC_FREE_SLOT,
		OSAL_ACCEL_CMD_ENC_WAIT_MSG,
		OSAL_ACCEL_CMD_ENC_SET_RING_ADDRS,
} osal_cmds_enc;

#define OSAL_ACCEL_CMD_GLOBAL_ASSOCIATE_EVENT 255

typedef enum {
		OSAL_ACCL_CMD_GPU_RUN = 1,
		OSAL_ACCL_CMD_GPU_PROFILER_RUN,
		OSAL_ACCL_CMD_GPU_END
} gpu_osal_cmds;

#if defined(USE_OSAL)

typedef struct osal_ioctl_header {
	osal_accelerator accel;
	uint64_t         handle;
} osal_ioctl_header;

typedef osal_ioctl_header ma_create_ctx;
typedef osal_ioctl_header ma_delete_ctx;

typedef struct osal_submit {
	osal_ioctl_header header;
	osal_command      cmd;
	const uint32_t*   cmdData;
	uint32_t          numPatches;
	const uint32_t*   patches;
	uint32_t          maxNumResp;
} osal_submit;

typedef struct osal_wait {
	osal_ioctl_header header;
	uint32_t          cmdId;
	uint32_t          timeoutMs;
	int32_t           numData;
	uint32_t*         respData;
} osal_wait;

#define IOCTL_MA_CREATE_CTX _IOWR('k', 0xb0, ma_create_ctx*)
#define IOCTL_MA_DELETE_CTX _IOWR('k', 0xb1, ma_delete_ctx*)
#define IOCTL_MA_SUBMIT_CMD _IOWR('k', 0xb2, osal_submit*)
#define IOCTL_MA_WAIT_RSP   _IOWR('k', 0xb3, osal_wait*)
#define IOCTL_CMD_MA_MINNR 0xb0
#define IOCTL_CMD_MA_MAXNR 0xb3

#endif

/* global module */
#define IOCTL_CMD_GLOBAL_MINNR 0xc0
#define IOCTL_CMD_GLOBAL_MAXNR 0xc0
#define IOCTL_ALL_IP_SOFT_RESET _IO('k', 0xc0)

/* dma_buf */
#define IOCTL_DMA_BUF_MINNR 0xd0
#define IOCTL_DMA_BUF_MAXNR 0xd0

struct sn_dma_buf {
	__u64 address; // MMIO handle of address to wrap
	__u32 size;
	int   fd; // output fd
};

#define IOCTL_DMA_BUF_WRAP _IOWR('k', 0xd0, struct sn_dma_buf*)

#define TRANS_MAXNR 0xd0

#ifdef __cplusplus
}
#endif

#endif /*  __TRANSCODER_H__*/
