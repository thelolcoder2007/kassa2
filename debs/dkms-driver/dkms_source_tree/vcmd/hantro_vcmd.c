// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 VeriSilicon Holdings Co., Ltd.
 *
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <linux/pci.h>
#include <linux/uaccess.h>
#include <linux/ioport.h>
#include <asm/irq.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <asm-generic/io.h>

#include "common.h"
#include "vcmdswhwregisters.h"
#include "bidirect_list.h"
#include "vcmdswhwregisters.h"
#include "hantrovcmd.h"
#include "transcoder.h"
#include "vc8000d.h"
#include "vc8000e.h"
#include "sn_osal.h"
#include "memory_osal.h"
#include "sn_perf.h"

/* VCMD */
#define OPCODE_WREG (0x01 << 27)
#define OPCODE_END (0x02 << 27)
#define OPCODE_NOP (0x03 << 27)
#define OPCODE_RREG (0x16 << 27)
#define OPCODE_INT (0x18 << 27)
#define OPCODE_JMP (0x19 << 27)
#define OPCODE_STALL (0x09 << 27)
#define OPCODE_CLRINT (0x1a << 27)
#define OPCODE_JMP_RDY0 (0x19 << 27)
#define OPCODE_JMP_RDY1 ((0x19 << 27) | (1 << 26))
#define JMP_IE_1 (1 << 25)
#define JMP_RDY_1 (1 << 26)

#define VCMD_REGS_IOSIZE (ASIC_VCMD_SWREG_AMOUNT * 4)
#define INVALID_MODULE 0xFFFF

/********variables declaration related with race condition**********/
#define CMDBUF_MAX_SIZE (512 * 4 * 4)

#define CMDBUF_POOL_TOTAL_SIZE (2 * 1024 * 1024) //approximately=128x(320x240)=128x2k=128x8kbyte=1Mbytes
#define TOTAL_DISCRETE_CMDBUF_NUM (CMDBUF_POOL_TOTAL_SIZE / CMDBUF_MAX_SIZE)
#define CMDBUF_REGS_TOTAL_SIZE (11 * 1024 * 1024 - CMDBUF_POOL_TOTAL_SIZE * 3)
#define VCMD_REGISTER_SIZE (128 * 4)
#define VCMD_TIMEOUT (10 * HZ)

#define RESERVED_EP_MEM_SIZE 0x400000                                  // dma link table
#define MAP_SIZE (CMDBUF_POOL_TOTAL_SIZE * 3 + CMDBUF_REGS_TOTAL_SIZE) // just ends up being 11MB

/* May be unified in next step. */
struct vcmd_config {
  unsigned long       vcmd_base_addr;
  u32                 vcmd_iosize;
  int                 vcmd_irq;
  u32                 sub_module_type;         /*input vc8000e=0,IM=1,vc8000d=2,jpege=3, jpegd=4*/
  u16                 submodule_main_addr;     // in byte
  u16                 submodule_dec400_addr;   //if submodule addr == 0xffff, this submodule does not exist.// in byte
  u16                 submodule_L2Cache_addr;  // in byte
  u16                 submodule_MMU_addr[2];   // in byte
  u16                 submodule_axife_addr[2]; // in byte
  char*               vcmd_name;
  int                 slice;
  enum SYS_CON_SUBSYS subsys;
};

/*for all vcmds, the core info should be listed here for subsequent use*/
static struct vcmd_config pf_vcmd_core_array[] = {
#if VCMD_ENABLE_VC8000E
#if SUB_SYS_VCE /* vc8000e configuration */
#if S1_VCE
    {S1_VC8000E_OFF, VCMD_REGS_IOSIZE, S1_VCE_IRQ, VCMD_TYPE_ENCODER, VCE_VC8000E_CORE_OFF, VCE_DEC400_CORE_OFF, INVALID_MODULE, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s1_vcmd_vce", 0},

    {S1_VCE_IM_OFF, VCMD_REGS_IOSIZE, S1_IM_IRQ, VCMD_TYPE_CUTREE, VCE_IM_CORE_OFF, INVALID_MODULE, INVALID_MODULE, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s1_vcmd_im", 0},
#endif
#if S2_VCE
    {S2_VC8000E_OFF, VCMD_REGS_IOSIZE, S2_VCE_IRQ, VCMD_TYPE_ENCODER, VCE_VC8000E_CORE_OFF, VCE_DEC400_CORE_OFF, INVALID_MODULE, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s2_vcmd_vce", 1},

    {S2_VCE_IM_OFF, VCMD_REGS_IOSIZE, S2_IM_IRQ, VCMD_TYPE_CUTREE, VCE_IM_CORE_OFF, INVALID_MODULE, INVALID_MODULE, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s2_vcmd_im", 1},
#endif
#endif
#endif

#if VCMD_ENABLE_VC8000D
#if SUB_SYS_VCD /* vc8000d configuration */
#if S1_VCD_A
    {S1_VC8000D_A_OFF, VCMD_REGS_IOSIZE, S1_VCD_A_IRQ, VCMD_TYPE_DECODER, VCD_VC8000D_CORE_OFF, VCD_DEC400_CORE_OFF, VCD_L2CACHE_CORE_OFF,
        {INVALID_MODULE, INVALID_MODULE}, {INVALID_MODULE, INVALID_MODULE}, "s1_vcmd_vcd_a", 0},
#endif
#if S1_VCD_B
    {S1_VC8000D_B_OFF, VCMD_REGS_IOSIZE, S1_VCD_B_IRQ, VCMD_TYPE_DECODER, VCD_VC8000D_CORE_OFF, INVALID_MODULE, VCD_L2CACHE_CORE_OFF, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s1_vcmd_vcd_b", 0},
#endif

#if S2_VCD_A
    {S2_VC8000D_A_OFF, VCMD_REGS_IOSIZE, S2_VCD_A_IRQ, VCMD_TYPE_DECODER, VCD_VC8000D_CORE_OFF, VCD_DEC400_CORE_OFF, VCD_L2CACHE_CORE_OFF,
        {INVALID_MODULE, INVALID_MODULE}, {INVALID_MODULE, INVALID_MODULE}, "s2_vcmd_vcd_a", 1},
#endif
#if S2_VCD_B
    {S2_VC8000D_B_OFF, VCMD_REGS_IOSIZE, S2_VCD_B_IRQ, VCMD_TYPE_DECODER, VCD_VC8000D_CORE_OFF, INVALID_MODULE, VCD_L2CACHE_CORE_OFF, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s2_vcmd_vcd_b", 1},
#endif
#endif
#endif
};

static struct vcmd_config vf1_vcmd_core_array[] = {
#if VCMD_ENABLE_VC8000E
#if SUB_SYS_VCE /* vc8000e configuration */
#if S1_VCE
    {VF_VC8000E_OFF, VCMD_REGS_IOSIZE, S1_VCE_IRQ, VCMD_TYPE_ENCODER, VCE_VC8000E_CORE_OFF, VCE_DEC400_CORE_OFF, INVALID_MODULE, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s1_vcmd_vce", 0},

    {VF_VCE_IM_OFF, VCMD_REGS_IOSIZE, S1_IM_IRQ, VCMD_TYPE_CUTREE, VCE_IM_CORE_OFF, INVALID_MODULE, INVALID_MODULE, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s1_vcmd_im", 0},
#endif
#endif
#endif

#if VCMD_ENABLE_VC8000D
#if SUB_SYS_VCD /* vc8000d configuration */
#if S1_VCD_A
    {VF_VC8000D_A_OFF, VCMD_REGS_IOSIZE, S1_VCD_A_IRQ, VCMD_TYPE_DECODER, VCD_VC8000D_CORE_OFF, VCD_DEC400_CORE_OFF, VCD_L2CACHE_CORE_OFF,
        {INVALID_MODULE, INVALID_MODULE}, {INVALID_MODULE, INVALID_MODULE}, "s1_vcmd_vcd_a", 0},
#endif
#if S1_VCD_B
    {VF_VC8000D_B_OFF, VCMD_REGS_IOSIZE, S1_VCD_B_IRQ, VCMD_TYPE_DECODER, VCD_VC8000D_CORE_OFF, INVALID_MODULE, VCD_L2CACHE_CORE_OFF, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s1_vcmd_vcd_b", 0},
#endif
#endif
#endif
};

static struct vcmd_config vf2_vcmd_core_array[] = {
#if VCMD_ENABLE_VC8000E
#if SUB_SYS_VCE /* vc8000e configuration */
#if S2_VCE
    {VF_VC8000E_OFF, VCMD_REGS_IOSIZE, S2_VCE_IRQ, VCMD_TYPE_ENCODER, VCE_VC8000E_CORE_OFF, VCE_DEC400_CORE_OFF, INVALID_MODULE, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s2_vcmd_vce", 1},

    {VF_VCE_IM_OFF, VCMD_REGS_IOSIZE, S2_IM_IRQ, VCMD_TYPE_CUTREE, VCE_IM_CORE_OFF, INVALID_MODULE, INVALID_MODULE, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s2_vcmd_im", 1},
#endif
#endif
#endif

#if VCMD_ENABLE_VC8000D
#if SUB_SYS_VCD /* vc8000d configuration */
#if S2_VCD_A
    {VF_VC8000D_A_OFF, VCMD_REGS_IOSIZE, S2_VCD_A_IRQ, VCMD_TYPE_DECODER, VCD_VC8000D_CORE_OFF, VCD_DEC400_CORE_OFF, VCD_L2CACHE_CORE_OFF,
        {INVALID_MODULE, INVALID_MODULE}, {INVALID_MODULE, INVALID_MODULE}, "s2_vcmd_vcd_a", 1},
#endif
#if S2_VCD_B
    {VF_VC8000D_B_OFF, VCMD_REGS_IOSIZE, S2_VCD_B_IRQ, VCMD_TYPE_DECODER, VCD_VC8000D_CORE_OFF, INVALID_MODULE, VCD_L2CACHE_CORE_OFF, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s2_vcmd_vcd_b", 1},
#endif
#endif
#endif
};

static struct vcmd_config vf_vcmd_core_array[] = {
#if VCMD_ENABLE_VC8000E
#if SUB_SYS_VCE /* vc8000e configuration */
#if S1_VCE
    {ONE_VF_S1_VC8000E_OFF, VCMD_REGS_IOSIZE, S1_VCE_IRQ, VCMD_TYPE_ENCODER, VCE_VC8000E_CORE_OFF, VCE_DEC400_CORE_OFF, INVALID_MODULE, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s1_vcmd_vce", 0},

    {ONE_VF_S1_VCE_IM_OFF, VCMD_REGS_IOSIZE, S1_IM_IRQ, VCMD_TYPE_CUTREE, VCE_IM_CORE_OFF, INVALID_MODULE, INVALID_MODULE, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s1_vcmd_im", 0},
#endif
#if S2_VCE
    {ONE_VF_S2_VC8000E_OFF, VCMD_REGS_IOSIZE, S2_VCE_IRQ, VCMD_TYPE_ENCODER, VCE_VC8000E_CORE_OFF, VCE_DEC400_CORE_OFF, INVALID_MODULE, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s2_vcmd_vce", 1},

    {ONE_VF_S2_VCE_IM_OFF, VCMD_REGS_IOSIZE, S2_IM_IRQ, VCMD_TYPE_CUTREE, VCE_IM_CORE_OFF, INVALID_MODULE, INVALID_MODULE, {INVALID_MODULE, INVALID_MODULE},
        {INVALID_MODULE, INVALID_MODULE}, "s2_vcmd_im", 1},
#endif
#endif
#endif

#if VCMD_ENABLE_VC8000D
#if SUB_SYS_VCD /* vc8000d configuration */
#if S1_VCD_A
    {ONE_VF_S1_VC8000D_A_OFF, VCMD_REGS_IOSIZE, S1_VCD_A_IRQ, VCMD_TYPE_DECODER, VCD_VC8000D_CORE_OFF, VCD_DEC400_CORE_OFF, VCD_L2CACHE_CORE_OFF,
        {INVALID_MODULE, INVALID_MODULE}, {INVALID_MODULE, INVALID_MODULE}, "s1_vcmd_vcd_a", 0},
#endif
#if S1_VCD_B
    {ONE_VF_S1_VC8000D_B_OFF, VCMD_REGS_IOSIZE, S1_VCD_B_IRQ, VCMD_TYPE_DECODER, VCD_VC8000D_CORE_OFF, INVALID_MODULE, VCD_L2CACHE_CORE_OFF,
        {INVALID_MODULE, INVALID_MODULE}, {INVALID_MODULE, INVALID_MODULE}, "s1_vcmd_vcd_b", 0},
#endif

#if S2_VCD_A
    {ONE_VF_S2_VC8000D_A_OFF, VCMD_REGS_IOSIZE, S2_VCD_A_IRQ, VCMD_TYPE_DECODER, VCD_VC8000D_CORE_OFF, VCD_DEC400_CORE_OFF, VCD_L2CACHE_CORE_OFF,
        {INVALID_MODULE, INVALID_MODULE}, {INVALID_MODULE, INVALID_MODULE}, "s2_vcmd_vcd_a", 1},
#endif
#if S2_VCD_B
    {ONE_VF_S2_VC8000D_B_OFF, VCMD_REGS_IOSIZE, S2_VCD_B_IRQ, VCMD_TYPE_DECODER, VCD_VC8000D_CORE_OFF, INVALID_MODULE, VCD_L2CACHE_CORE_OFF,
        {INVALID_MODULE, INVALID_MODULE}, {INVALID_MODULE, INVALID_MODULE}, "s2_vcmd_vcd_b", 1},
#endif
#endif
#endif
};

/*these size need to be modified according to hw config.*/
#define MAX_SAME_MODULE_TYPE_CORE_NUMBER 4

#define VCMD_ENCODER_REGISTER_SIZE (ENCODER_REGISTER_SIZE * 4)
#define VCMD_DECODER_REGISTER_SIZE (VCD_VC8000D_REGS_CNT * 4)
#define VCMD_IM_REGISTER_SIZE (IM_REGISTER_SIZE * 4)
#define VCMD_JPEG_ENCODER_REGISTER_SIZE (JPEG_ENCODER_REGISTER_SIZE * 4)
#define VCMD_JPEG_DECODER_REGISTER_SIZE (VCD_VC8000D_REGS_CNT * 4)

#define MAX_VCMD_NUMBER (MAX_VCMD_TYPE * MAX_SAME_MODULE_TYPE_CORE_NUMBER) //
#define HW_WORK_STATE_PEND 3
#define MAX_CMDBUF_INT_NUMBER 1
#define INT_MIN_SUM_OF_IMAGE_SIZE (4096 * 2160 * MAX_SAME_MODULE_TYPE_CORE_NUMBER * MAX_CMDBUF_INT_NUMBER)
#define MAX_PROCESS_CORE_NUMBER 4 * 8
#define PROCESS_MAX_VIDEO_SIZE (4096 * 2160 * MAX_SAME_MODULE_TYPE_CORE_NUMBER * MAX_PROCESS_CORE_NUMBER)
#define PROCESS_MAX_JPEG_SIZE (2147483648) //32768*32768*2
#define PROCESS_MAX_SUM_OF_IMAGE_SIZE (PROCESS_MAX_VIDEO_SIZE > PROCESS_MAX_JPEG_SIZE ? PROCESS_MAX_VIDEO_SIZE : PROCESS_MAX_JPEG_SIZE)

static struct pci_dev* g_vcmd_dev = NULL; /* PCI device structure. */

struct noncache_mem {
  u32 __iomem* virtualAddress;
  dma_addr_t __iomem busAddress;
  u32        size;
  u16        cmdbuf_id;
};

struct process_manager_obj {
  struct file*      filp;
  u64               total_exe_time;
  spinlock_t        spinlock;
  wait_queue_head_t wait_queue;
};

struct cmdbuf_obj {
  u32          module_type;           //current CMDBUF type: input vc8000e=0,IM=1,vc8000d=2,jpege=3, jpegd=4
  u32          priority;              //current CMDBUFpriority: normal=0, high=1
  u64          executing_time;        //current CMDBUFexecuting_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
  u32          cmdbuf_size;           //current CMDBUF size
  u32 __iomem* cmdbuf_virtualAddress; //current CMDBUF start virtual address.
  u64 __iomem  cmdbuf_busAddress;     //current CMDBUF start physical address.
  u32 __iomem* status_virtualAddress; //current status CMDBUF start virtual address.
  u64 __iomem  status_busAddress;     //current status CMDBUF start physical address.
  u32          status_size;           //current status CMDBUF size
  u32          executing_status;      //current CMDBUF executing status.
  struct file* filp;                  //file pointer in the same process.
  u16          core_id;               //which vcmd core is used.
  u16          cmdbuf_id; //used to manage CMDBUF in driver.It is a handle to identify cmdbuf.also is an interrupt vector.position in pool,same as status position.
  u8           cmdbuf_data_loaded; //0 means sw has not copied data into this CMDBUF; 1 means sw has copied data into this CMDBUF
  u8           cmdbuf_data_linked; //0 :not linked, 1:linked.
  u8           cmdbuf_run_done; //if 0,waiting for CMDBUF finish; if 1, op code in CMDBUF has finished one by one. HANTRO_VCMD_IOCH_WAIT_CMDBUF will check this variable.
  u8           cmdbuf_need_remove;          // if 0, not need to remove CMDBUF; 1 CMDBUF can be removed if it is not the last CMDBUF;
  u32          waited;                      // if 0, the cmd buf hasn't been waited, otherwise, has been waited.
  u8           has_end_cmdbuf;              //if 1, the last opcode is end opCode.
  u8           no_normal_int_cmdbuf;        //if 1, JMP will not send normal interrupt.
  u32 __iomem* patch_cmdbuf_virtualAddress; //current patch cmd buffer start virtual address.
  u64 __iomem  patch_cmdbuf_busAddress;     //current patch cmd buffer start physical address.
};

struct hantrovcmd_dev {
  struct vcmd_config  vcmd_core_cfg; //config of each core,such as base addr, irq,etc
  u32                 core_id;       //vcmd core id for driver and sw internal use, in the vcmd_core_array struct sequence
  u32                 sw_cmdbuf_rdy_num;
  spinlock_t*         spinlock;
  wait_queue_head_t*  wait_queue;
  wait_queue_head_t*  wait_abort_queue;
  bi_list             list_manager;
  void __iomem*       hwregs; /* IO mem base */
  u32                 reg_mirror[ASIC_VCMD_SWREG_AMOUNT];
  u32                 duration_without_int; //number of cmdbufs without interrupt.
  u8                  working_state;
  u64                 total_exe_time;
  u16                 status_cmdbuf_id;            //used for analyse configuration in cwl.
  u32                 hw_version_id;               /*megvii 0x43421001, later 0x43421102*/
  u32 __iomem*        vcmd_reg_mem_virtualAddress; //start virtual address of vcmd registers memory of  CMDBUF.
  u64 __iomem         vcmd_reg_mem_busAddress;     //start physical address of vcmd registers memory of  CMDBUF.
  u32                 vcmd_reg_mem_size;           // size of vcmd registers memory of CMDBUF.
  u32                 is_valid;
  void*               vcmd_dev; // the vcmd device(a hardware transcoder)
  SnPerfHandle        perf_handle;
  struct timer_list   loading_timer;
  struct loading_info loading[4];
  u16                 triger_cmdbuf_id;
};

static void vcd_loading_timer_isr(struct timer_list *t)
{
  struct hantrovcmd_dev *dev = from_timer(dev, t, loading_timer);
  uint32_t load;
  int i = 0;
  // calculate loading
  for(i=0; i<4; i++){
    dev->loading[i].time_cnt_saved = dev->loading[i].time_cnt;
    dev->loading[i].time_cnt = 0;
    dev->loading[i].total_time = LOADING_TIME * 1000000;
    load = 100*dev->loading[i].time_cnt_saved/dev->loading[i].total_time;
    load = (load > 100) ? 100 : load;
    // core_id "0","1","2","3" correspond to "DecA slice0","DecA slice1","DecB slice0","DecB slice1" respectively.
    // The display order is "DecA slice0", "DecB slice0", "DecA slice1", "DecB slice1".
    if(i==1){
      sn_perf_load(dev->perf_handle, 2, load);
    }else if(i==2){
      sn_perf_load(dev->perf_handle, 1, load);
    }else{
      sn_perf_load(dev->perf_handle, i, load);
    }
  }
  // restart timer
  mod_timer(&dev->loading_timer, t->expires + LOADING_TIME*HZ);
}

struct vcmd_dev {
  struct vcmd_config*    vcmd_active_core_array;
  struct hantrovcmd_dev* hantrovcmd_data;
  u64 __iomem            g_vcmd_base_hdwr;      /* PCI base register address (Hardware address) */
  u64 __iomem            g_vcmd_base_ddr_hw;    /* DDR base address (memalloc) */
  void __iomem*          g_vcmd_base_hdwr_virt; /*DDR virtual address (memalloc) */
  void __iomem*          g_vcmd_base_ddr_virt;  /*DDR virtual address (memalloc) */
  u32                    g_vcmd_base_len;       /* Base register address Length */
  u64                    base_ddr_addr;         /*pcie address need to substract this value then can be put to register*/
  struct noncache_mem    vcmd_buf_mem_pool;
  struct noncache_mem    vcmd_status_buf_mem_pool;
  struct noncache_mem    vcmd_registers_mem_pool;
  struct noncache_mem    vcmd_cmbbuf_patch_pool;
  uint64_t               osal_mem_handle;
  u16                    vcmd_position[MAX_VCMD_TYPE];
  int                    vcmd_type_core_num[MAX_VCMD_TYPE];
  bi_list_node*          global_cmdbuf_node[TOTAL_DISCRETE_CMDBUF_NUM];

  bi_list                global_process_manager;
  u16                    cmdbuf_used[TOTAL_DISCRETE_CMDBUF_NUM];
  u16                    cmdbuf_used_pos;
  u16                    cmdbuf_used_residual;
  struct hantrovcmd_dev* vcmd_manager[MAX_VCMD_TYPE][MAX_VCMD_NUMBER];

  spinlock_t        owner_lock_vcmd[MAX_VCMD_NUMBER];
  wait_queue_head_t wait_queue_vcmd[MAX_VCMD_NUMBER];
  wait_queue_head_t abort_queue_vcmd[MAX_VCMD_NUMBER];
  wait_queue_head_t mc_wait_queue; //mc wait queue, used in wait_cmdbuf_ready with ANY_CMDBUF_ID.

  struct semaphore  vcmd_reserve_cmdbuf_sem[MAX_VCMD_TYPE]; //for reserve
  struct semaphore  vcmd_reserve_resource;
  wait_queue_head_t vcmd_cmdbuf_memory_wait;                //hw_queue can be used for reserve cmdbuf memory
  spinlock_t        vcmd_cmdbuf_alloc_lock;
  spinlock_t        vcmd_process_manager_lock;

  int software_triger_abort;

  u32 total_vcmd_core_num;
  u32 total_vcmd_vc8000d_core_num;
  u32 total_vcmd_vc8000e_core_num;
  void *tdev; //To use for logging
};

struct vcmd_vc8000d_config {
	u32 dec_path;
	u32 start_core_id;
	u32 max_core_id;
	u32 step;
	u32 max_core_num;
};

static struct vcmd_vc8000d_config decpath_array[] = {
	{ DEC_PATH_A, 0, 2, 2, 2},
	{ DEC_PATH_B, 1, 3, 2, 2},
	{ DEC_PATH_AB, 0, 3, 1, 4},
};

#define VCMD_HW_ID 0x4342

#define EXECUTING_CMDBUF_ID_ADDR 26
#define VCMD_EXE_CMDBUF_COUNT 3

#define WORKING_STATE_IDLE 0
#define WORKING_STATE_WORKING 1
#define CMDBUF_EXE_STATUS_OK 0
#define CMDBUF_EXE_STATUS_CMDERR 1
#define CMDBUF_EXE_STATUS_BUSERR 2

/* here's all the must remember stuff */
static int         vcmd_reserve_IO(struct sn_tranx_t* tdev);
static void        vcmd_release_IO(struct sn_tranx_t* tdev);
static void        vcmd_reset_asic(struct hantrovcmd_dev* dev, u32 core_count);
static void        vcmd_reset_current_asic(struct hantrovcmd_dev* dev);
static int         allocate_cmdbuf(struct file* filp, struct sn_tranx_t* tdev, struct noncache_mem* new_cmdbuf_addr, struct noncache_mem* new_status_cmdbuf_addr,
            struct noncache_mem* new_patch_cmdbuf_addr, struct exchange_parameter* param);
static void        vcmd_link_cmdbuf(struct hantrovcmd_dev* dev, bi_list_node* last_linked_cmdbuf_node);
static void        vcmd_start(struct hantrovcmd_dev* dev, bi_list_node* first_linked_cmdbuf_node);
static irqreturn_t hantrovcmd_isr(int irq, void* dev_id);
static void        vcmd_reset_asic_ex(struct hantrovcmd_dev* dev, u32 index);

/**********************************************************************************************************\
*cmdbuf object management
\***********************************************************************************************************/
static struct cmdbuf_obj* create_cmdbuf_obj(void) {
  struct cmdbuf_obj* cmdbuf_obj = NULL;

  cmdbuf_obj = kzalloc(sizeof(struct cmdbuf_obj), GFP_KERNEL);
  if (cmdbuf_obj == NULL) {
    printk(KERN_ERR "vmalloc for cmdbuf_obj fail!\n");
    return cmdbuf_obj;
  }
  memset(cmdbuf_obj, 0, sizeof(struct cmdbuf_obj));

  return cmdbuf_obj;
}

static void free_cmdbuf_obj(struct cmdbuf_obj* cmdbuf_obj) {
  if (cmdbuf_obj == NULL) {
    printk(KERN_INFO "remove_cmdbuf_obj NULL\n");
    return;
  }

  //free current cmdbuf_obj
  kfree(cmdbuf_obj);
}

static void free_cmdbuf_mem(struct vcmd_dev* vcmd_dev, u16 cmdbuf_id) {
  unsigned long flags;

  spin_lock_irqsave(&vcmd_dev->vcmd_cmdbuf_alloc_lock, flags);
  vcmd_dev->cmdbuf_used[cmdbuf_id] = 0;
  vcmd_dev->cmdbuf_used_residual += 1;
  spin_unlock_irqrestore(&vcmd_dev->vcmd_cmdbuf_alloc_lock, flags);
  wake_up_interruptible_all(&vcmd_dev->vcmd_cmdbuf_memory_wait);
}

static int init_allocate_cmdbuf(
    struct vcmd_dev* vcmd_dev, struct noncache_mem* new_cmdbuf_addr, struct noncache_mem* new_status_cmdbuf_addr, struct noncache_mem* new_patch_cmdbuf_addr) {
  while (1) {
    if (vcmd_dev->cmdbuf_used[vcmd_dev->cmdbuf_used_pos] == 0 && (vcmd_dev->global_cmdbuf_node[vcmd_dev->cmdbuf_used_pos] == NULL)) {
      vcmd_dev->cmdbuf_used[vcmd_dev->cmdbuf_used_pos] = 1;
      vcmd_dev->cmdbuf_used_residual -= 1;
      new_cmdbuf_addr->virtualAddress        = vcmd_dev->vcmd_buf_mem_pool.virtualAddress + vcmd_dev->cmdbuf_used_pos * CMDBUF_MAX_SIZE / 4;
      new_cmdbuf_addr->busAddress            = vcmd_dev->vcmd_buf_mem_pool.busAddress + vcmd_dev->cmdbuf_used_pos * CMDBUF_MAX_SIZE;
      new_cmdbuf_addr->size                  = CMDBUF_MAX_SIZE;
      new_cmdbuf_addr->cmdbuf_id             = vcmd_dev->cmdbuf_used_pos;
      new_status_cmdbuf_addr->virtualAddress = vcmd_dev->vcmd_status_buf_mem_pool.virtualAddress + vcmd_dev->cmdbuf_used_pos * CMDBUF_MAX_SIZE / 4;
      new_status_cmdbuf_addr->busAddress     = vcmd_dev->vcmd_status_buf_mem_pool.busAddress + vcmd_dev->cmdbuf_used_pos * CMDBUF_MAX_SIZE;
      new_status_cmdbuf_addr->size           = CMDBUF_MAX_SIZE;
      new_status_cmdbuf_addr->cmdbuf_id      = vcmd_dev->cmdbuf_used_pos;
      vcmd_dev->cmdbuf_used_pos++;
      if (vcmd_dev->cmdbuf_used_pos >= TOTAL_DISCRETE_CMDBUF_NUM)
        vcmd_dev->cmdbuf_used_pos = 0;
      return 1;
    } else {
      vcmd_dev->cmdbuf_used_pos++;
      if (vcmd_dev->cmdbuf_used_pos >= TOTAL_DISCRETE_CMDBUF_NUM)
        vcmd_dev->cmdbuf_used_pos = 0;
    }
  }
  return 0;
}

static bi_list_node* create_cmdbuf_node(struct file* filp, struct sn_tranx_t* tdev, struct exchange_parameter* para) {
  struct vcmd_dev*    vcmd_dev     = tdev->modules[SN_MODULE_VCMD];
  bi_list_node*       current_node = NULL;
  struct cmdbuf_obj*  cmdbuf_obj   = NULL;
  struct noncache_mem new_cmdbuf_addr;
  struct noncache_mem new_status_cmdbuf_addr;
  struct noncache_mem new_patch_cmdbuf_addr;

  if (filp == NULL) {
    if (0 == init_allocate_cmdbuf(vcmd_dev, &new_cmdbuf_addr, &new_status_cmdbuf_addr, &new_patch_cmdbuf_addr)) {
      sn_pri(tdev, SN_ERR, "init allocate cmdbuf failed\n");
      return NULL;
    }
  } else {
    if (wait_event_interruptible_timeout(vcmd_dev->vcmd_cmdbuf_memory_wait, allocate_cmdbuf(filp, tdev, &new_cmdbuf_addr, &new_status_cmdbuf_addr, &new_patch_cmdbuf_addr, para), VCMD_TIMEOUT) <= 0) {
      sn_pri(tdev, SN_INF, "create_cmdbuf_node: wait event interruptible timeout\n");
      return NULL;
    }
  }

  cmdbuf_obj = create_cmdbuf_obj();
  if (cmdbuf_obj == NULL) {
    sn_pri(tdev, SN_ERR, "create_cmdbuf_obj fail!\n");
    free_cmdbuf_mem(vcmd_dev, new_cmdbuf_addr.cmdbuf_id);
    return NULL;
  }
  cmdbuf_obj->cmdbuf_busAddress           = new_cmdbuf_addr.busAddress;
  cmdbuf_obj->cmdbuf_virtualAddress       = new_cmdbuf_addr.virtualAddress;
  cmdbuf_obj->cmdbuf_size                 = new_cmdbuf_addr.size;
  cmdbuf_obj->cmdbuf_id                   = new_cmdbuf_addr.cmdbuf_id;
  cmdbuf_obj->status_busAddress           = new_status_cmdbuf_addr.busAddress;
  cmdbuf_obj->status_virtualAddress       = new_status_cmdbuf_addr.virtualAddress;
  cmdbuf_obj->status_size                 = new_status_cmdbuf_addr.size;
  cmdbuf_obj->patch_cmdbuf_busAddress     = new_patch_cmdbuf_addr.busAddress;
  cmdbuf_obj->patch_cmdbuf_virtualAddress = new_patch_cmdbuf_addr.virtualAddress;
  current_node                            = bi_list_create_node();
  if (current_node == NULL) {
    sn_pri(tdev, SN_ERR, "bi_list_create_node fail!\n");
    free_cmdbuf_mem(vcmd_dev, new_cmdbuf_addr.cmdbuf_id);
    free_cmdbuf_obj(cmdbuf_obj);
    return NULL;
  }
  current_node->data     = (void*) cmdbuf_obj;
  current_node->next     = NULL;
  current_node->previous = NULL;

  return current_node;
}

static void free_cmdbuf_node(struct vcmd_dev* vcmd_dev, bi_list_node* cmdbuf_node) {
  struct cmdbuf_obj* cmdbuf_obj = NULL;

  if (cmdbuf_node == NULL) {
    printk(KERN_INFO "remove_cmdbuf_node NULL\n");
    return;
  }
  cmdbuf_obj = (struct cmdbuf_obj*) cmdbuf_node->data;
  free_cmdbuf_mem(vcmd_dev, cmdbuf_obj->cmdbuf_id);
  free_cmdbuf_obj(cmdbuf_obj);
  bi_list_free_node(cmdbuf_node);
}

//just remove, not free the node.
static bi_list_node* remove_cmdbuf_node_from_list(bi_list* list, bi_list_node* cmdbuf_node) {
  if (cmdbuf_node == NULL) {
    printk(KERN_INFO "remove_cmdbuf_node_from_list  NULL\n");
    return NULL;
  }
  if (cmdbuf_node->next) {
    bi_list_remove_node(list, cmdbuf_node);
    return cmdbuf_node;
  } else {
    //the last one, should not be removed.
    return NULL;
  }
}

//calculate executing_time of each vcmd
static u64 calculate_executing_time_after_node(bi_list_node* exe_cmdbuf_node) {
  u64                time_run_all    = 0;
  struct cmdbuf_obj* cmdbuf_obj_temp = NULL;

  while (1) {
    if (exe_cmdbuf_node == NULL)
      break;
    cmdbuf_obj_temp = (struct cmdbuf_obj*) exe_cmdbuf_node->data;
    time_run_all += cmdbuf_obj_temp->executing_time;
    exe_cmdbuf_node = exe_cmdbuf_node->next;
  }

  return time_run_all;
}

static u64 calculate_executing_time_after_node_high_priority(bi_list_node* exe_cmdbuf_node) {
  u64                time_run_all    = 0;
  struct cmdbuf_obj* cmdbuf_obj_temp = NULL;

  if (exe_cmdbuf_node == NULL)
    return time_run_all;

  cmdbuf_obj_temp = (struct cmdbuf_obj*) exe_cmdbuf_node->data;
  time_run_all += cmdbuf_obj_temp->executing_time;
  exe_cmdbuf_node = exe_cmdbuf_node->next;
  while (1) {
    if (exe_cmdbuf_node == NULL)
      break;

    cmdbuf_obj_temp = (struct cmdbuf_obj*) exe_cmdbuf_node->data;
    if (cmdbuf_obj_temp->priority == CMDBUF_PRIORITY_NORMAL)
      break;

    time_run_all += cmdbuf_obj_temp->executing_time;
    exe_cmdbuf_node = exe_cmdbuf_node->next;
  }
  return time_run_all;
}

/**********************************************************************************************************\
*cmdbuf pool management
\***********************************************************************************************************/
static int allocate_cmdbuf(struct file* filp, struct sn_tranx_t* tdev, struct noncache_mem* new_cmdbuf_addr, struct noncache_mem* new_status_cmdbuf_addr,
    struct noncache_mem* new_patch_cmdbuf_addr, struct exchange_parameter* param) {
  struct vcmd_dev* vcmd_dev = tdev->modules[SN_MODULE_VCMD];
  unsigned long    flags;
  int pos = 0;

  //there is one cmdbuf at least
  while (1) {

    spin_lock_irqsave(&vcmd_dev->vcmd_cmdbuf_alloc_lock, flags);
    if (vcmd_dev->cmdbuf_used_residual == 0) {
      spin_unlock_irqrestore(&vcmd_dev->vcmd_cmdbuf_alloc_lock, flags);
      //no empty cmdbuf
      return 0;
    }

    if (vcmd_dev->cmdbuf_used[vcmd_dev->cmdbuf_used_pos] == 0 && (vcmd_dev->global_cmdbuf_node[vcmd_dev->cmdbuf_used_pos] == NULL)) {
      vcmd_dev->cmdbuf_used[vcmd_dev->cmdbuf_used_pos] = 1;
      pos= vcmd_dev->cmdbuf_used_pos;
      vcmd_dev->cmdbuf_used_pos++;
      if (vcmd_dev->cmdbuf_used_pos >= TOTAL_DISCRETE_CMDBUF_NUM)
        vcmd_dev->cmdbuf_used_pos = 0;
      vcmd_dev->cmdbuf_used_residual -= 1;
      spin_unlock_irqrestore(&vcmd_dev->vcmd_cmdbuf_alloc_lock, flags);

      new_cmdbuf_addr->virtualAddress        = vcmd_dev->vcmd_buf_mem_pool.virtualAddress + pos * CMDBUF_MAX_SIZE / 4;
      new_cmdbuf_addr->busAddress            = vcmd_dev->vcmd_buf_mem_pool.busAddress + pos * CMDBUF_MAX_SIZE;
      new_cmdbuf_addr->size                  = CMDBUF_MAX_SIZE;
      new_cmdbuf_addr->cmdbuf_id             = pos;
      new_status_cmdbuf_addr->busAddress     = sn_mem_osal_translate_handle(tdev, filp, param->statusbuf_addr, 0);
      new_status_cmdbuf_addr->virtualAddress = sn_mem_osal_translate_mmio(tdev, new_status_cmdbuf_addr->busAddress);
      new_status_cmdbuf_addr->size           = CMDBUF_MAX_SIZE;
      new_status_cmdbuf_addr->cmdbuf_id      = pos;
      new_patch_cmdbuf_addr->virtualAddress  = vcmd_dev->vcmd_cmbbuf_patch_pool.virtualAddress + pos*CMDBUF_MAX_SIZE/4;
      new_patch_cmdbuf_addr->busAddress      = vcmd_dev->vcmd_cmbbuf_patch_pool.busAddress + pos*CMDBUF_MAX_SIZE;
      new_patch_cmdbuf_addr->size            = CMDBUF_MAX_SIZE;
      return 1;
    } else {
      vcmd_dev->cmdbuf_used_pos++;
      if (vcmd_dev->cmdbuf_used_pos >= TOTAL_DISCRETE_CMDBUF_NUM)
        vcmd_dev->cmdbuf_used_pos = 0;
    }
    spin_unlock_irqrestore(&vcmd_dev->vcmd_cmdbuf_alloc_lock, flags);
  }
  return 0;
}

static bi_list_node* get_cmdbuf_node_in_list_by_addr(struct vcmd_dev* vcmd_dev, size_t cmdbuf_addr, bi_list* list) {
  bi_list_node*      new_cmdbuf_node = NULL;
  struct cmdbuf_obj* cmdbuf_obj      = NULL;

  new_cmdbuf_node = list->head;
  while (1) {
    if (new_cmdbuf_node == NULL)
      return NULL;

    cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
    if (((cmdbuf_obj->cmdbuf_busAddress - vcmd_dev->base_ddr_addr) <= cmdbuf_addr) &&
        (((cmdbuf_obj->cmdbuf_busAddress - vcmd_dev->base_ddr_addr + cmdbuf_obj->cmdbuf_size) > cmdbuf_addr))) {
      return new_cmdbuf_node;
    }
    new_cmdbuf_node = new_cmdbuf_node->next;
  }
  return NULL;
}

static int wait_abort_rdy(struct hantrovcmd_dev* dev) {
  return dev->working_state == WORKING_STATE_IDLE;
}

static int init_select_vcmd(struct vcmd_dev* vcmd_dev, bi_list_node* new_cmdbuf_node, int index) {
  struct cmdbuf_obj*     cmdbuf_obj = NULL;
  bi_list*               list       = NULL;
  struct hantrovcmd_dev* dev        = NULL;
  unsigned long          flags      = 0;

  cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
  if (index != vcmd_dev->vcmd_position[cmdbuf_obj->module_type]) {
    printk(KERN_ERR "index %d not equals to vcmd_position %d for type %d\n", index, vcmd_dev->vcmd_position[cmdbuf_obj->module_type], cmdbuf_obj->module_type);
    return -1;
  }
  dev  = vcmd_dev->vcmd_manager[cmdbuf_obj->module_type][vcmd_dev->vcmd_position[cmdbuf_obj->module_type]];
  list = &dev->list_manager;
  spin_lock_irqsave(dev->spinlock, flags);
  if (list->tail == NULL) {
    // the list is empty, only for the first node
    bi_list_insert_node_tail(list, new_cmdbuf_node);
    spin_unlock_irqrestore(dev->spinlock, flags);
    vcmd_dev->vcmd_position[cmdbuf_obj->module_type]++;
    if (vcmd_dev->vcmd_position[cmdbuf_obj->module_type] >= vcmd_dev->vcmd_type_core_num[cmdbuf_obj->module_type])
      vcmd_dev->vcmd_position[cmdbuf_obj->module_type] = 0;
    cmdbuf_obj->core_id = dev->core_id;
    return 0;
  } else {
    printk(KERN_ERR "At initialization, module %d index %d list not empty\n", cmdbuf_obj->module_type, index);
    return -1;
  }
}

static int select_vcmd(struct vcmd_dev* vcmd_dev, bi_list_node* new_cmdbuf_node, u16 core_id) {
  struct cmdbuf_obj*     cmdbuf_obj        = NULL;
  bi_list*               list              = NULL;
  struct hantrovcmd_dev* dev               = NULL;
  int                    counter           = 0;
  unsigned long          flags             = 0;
  cmdbuf_obj                               = (struct cmdbuf_obj*) new_cmdbuf_node->data;

  // look for target dev with specified core_id
  while(1){
    dev  = vcmd_dev->vcmd_manager[cmdbuf_obj->module_type][vcmd_dev->vcmd_position[cmdbuf_obj->module_type]];

    vcmd_dev->vcmd_position[cmdbuf_obj->module_type]++;
    if (vcmd_dev->vcmd_position[cmdbuf_obj->module_type] >= vcmd_dev->vcmd_type_core_num[cmdbuf_obj->module_type])
      vcmd_dev->vcmd_position[cmdbuf_obj->module_type] = 0;

    if(dev->core_id == core_id){
      list = &dev->list_manager;
      spin_lock_irqsave(dev->spinlock, flags);
      bi_list_insert_node_tail(list, new_cmdbuf_node);
      spin_unlock_irqrestore(dev->spinlock, flags);
      cmdbuf_obj->core_id = dev->core_id;
      return 0;
    }
    counter++;
    if (counter >= vcmd_dev->vcmd_type_core_num[cmdbuf_obj->module_type]){
      printk(KERN_ERR "can't find dev with core_id %d !!\n", core_id);
      break;
    }
  }
  return -1;
}

static int select_vcmd_auto(struct vcmd_dev* vcmd_dev, bi_list_node* new_cmdbuf_node, u16 dec_path) {
  struct cmdbuf_obj*     cmdbuf_obj        = NULL;
  bi_list_node*          curr_cmdbuf_node  = NULL;
  bi_list*               list              = NULL;
  struct hantrovcmd_dev* dev               = NULL;
  struct hantrovcmd_dev* smallest_dev      = NULL;
  u64                    executing_time    = 0xffffffffffffffff;
  int                    counter           = 0;
  unsigned long          flags             = 0;
  u32                    hw_rdy_cmdbuf_num = 0;
  size_t                 exe_cmdbuf_addr   = 0;
  struct cmdbuf_obj*     cmdbuf_obj_temp   = NULL;
  u32                    cmdbuf_id         = 0;
  cmdbuf_obj                               = (struct cmdbuf_obj*) new_cmdbuf_node->data;
  //there is an empty vcmd to be used
  while (1) {
    dev  = vcmd_dev->vcmd_manager[cmdbuf_obj->module_type][vcmd_dev->vcmd_position[cmdbuf_obj->module_type]];
    list = &dev->list_manager;
    spin_lock_irqsave(dev->spinlock, flags);
    if (list->tail == NULL) {
      // the list is empty, only for the first node
      bi_list_insert_node_tail(list, new_cmdbuf_node);
      spin_unlock_irqrestore(dev->spinlock, flags);
      vcmd_dev->vcmd_position[cmdbuf_obj->module_type]+=decpath_array[dec_path].step;
      if (vcmd_dev->vcmd_position[cmdbuf_obj->module_type] > decpath_array[dec_path].max_core_id)
        vcmd_dev->vcmd_position[cmdbuf_obj->module_type] = decpath_array[dec_path].start_core_id;
      cmdbuf_obj->core_id = dev->core_id;
      return 0;
    } else {
      spin_unlock_irqrestore(dev->spinlock, flags);
      vcmd_dev->vcmd_position[cmdbuf_obj->module_type]+=decpath_array[dec_path].step;
      if (vcmd_dev->vcmd_position[cmdbuf_obj->module_type] > decpath_array[dec_path].max_core_id)
        vcmd_dev->vcmd_position[cmdbuf_obj->module_type] = decpath_array[dec_path].start_core_id;
      counter++;
    }
    if (counter >= decpath_array[dec_path].max_core_num)
      break;
  }
  //there is a vcmd which tail node -> cmdbuf_run_done == 1. It means this vcmd has nothing to do, so we select it.
  counter = 0;
  while (1) {
    dev  = vcmd_dev->vcmd_manager[cmdbuf_obj->module_type][vcmd_dev->vcmd_position[cmdbuf_obj->module_type]];
    list = &dev->list_manager;
    spin_lock_irqsave(dev->spinlock, flags);
    curr_cmdbuf_node = list->tail;
    if (curr_cmdbuf_node == NULL) {
      bi_list_insert_node_tail(list, new_cmdbuf_node);
      spin_unlock_irqrestore(dev->spinlock, flags);
      vcmd_dev->vcmd_position[cmdbuf_obj->module_type]+=decpath_array[dec_path].step;
      if (vcmd_dev->vcmd_position[cmdbuf_obj->module_type] > decpath_array[dec_path].max_core_id)
        vcmd_dev->vcmd_position[cmdbuf_obj->module_type] = decpath_array[dec_path].start_core_id;
      cmdbuf_obj->core_id = dev->core_id;
      return 0;
    }
    cmdbuf_obj_temp = (struct cmdbuf_obj*) curr_cmdbuf_node->data;
    if (cmdbuf_obj_temp->cmdbuf_run_done == 1) {
      bi_list_insert_node_tail(list, new_cmdbuf_node);
      spin_unlock_irqrestore(dev->spinlock, flags);
      vcmd_dev->vcmd_position[cmdbuf_obj->module_type]+=decpath_array[dec_path].step;
      if (vcmd_dev->vcmd_position[cmdbuf_obj->module_type] > decpath_array[dec_path].max_core_id)
        vcmd_dev->vcmd_position[cmdbuf_obj->module_type] = decpath_array[dec_path].start_core_id;
      cmdbuf_obj->core_id = dev->core_id;
      return 0;
    } else {
      spin_unlock_irqrestore(dev->spinlock, flags);
      vcmd_dev->vcmd_position[cmdbuf_obj->module_type]+=decpath_array[dec_path].step;
      if (vcmd_dev->vcmd_position[cmdbuf_obj->module_type] > decpath_array[dec_path].max_core_id)
        vcmd_dev->vcmd_position[cmdbuf_obj->module_type] = decpath_array[dec_path].start_core_id;
      counter++;
    }
    if (counter >= decpath_array[dec_path].max_core_num)
      break;
  }

  //another case, tail = executing node, and vcmd=pend state (finish but not generate interrupt)
  counter = 0;
  while (1) {
    dev  = vcmd_dev->vcmd_manager[cmdbuf_obj->module_type][vcmd_dev->vcmd_position[cmdbuf_obj->module_type]];
    list = &dev->list_manager;
    //read executing cmdbuf address
    if (dev->hw_version_id <= HW_ID_1_0_C)
      hw_rdy_cmdbuf_num = vcmd_get_register_value(dev->hwregs, dev->reg_mirror, HWIF_VCMD_EXE_CMDBUF_COUNT);
    else {
      hw_rdy_cmdbuf_num = *(dev->vcmd_reg_mem_virtualAddress + VCMD_EXE_CMDBUF_COUNT);
      if (hw_rdy_cmdbuf_num != dev->sw_cmdbuf_rdy_num)
        hw_rdy_cmdbuf_num += 1;
    }
    spin_lock_irqsave(dev->spinlock, flags);
    curr_cmdbuf_node = list->tail;
    if (curr_cmdbuf_node == NULL) {
      bi_list_insert_node_tail(list, new_cmdbuf_node);
      spin_unlock_irqrestore(dev->spinlock, flags);
      vcmd_dev->vcmd_position[cmdbuf_obj->module_type]+=decpath_array[dec_path].step;
      if (vcmd_dev->vcmd_position[cmdbuf_obj->module_type] > decpath_array[dec_path].max_core_id)
        vcmd_dev->vcmd_position[cmdbuf_obj->module_type] = decpath_array[dec_path].start_core_id;
      cmdbuf_obj->core_id = dev->core_id;
      return 0;
    }

    if ((dev->sw_cmdbuf_rdy_num == hw_rdy_cmdbuf_num)) {
      bi_list_insert_node_tail(list, new_cmdbuf_node);
      spin_unlock_irqrestore(dev->spinlock, flags);
      vcmd_dev->vcmd_position[cmdbuf_obj->module_type]+=decpath_array[dec_path].step;
      if (vcmd_dev->vcmd_position[cmdbuf_obj->module_type] > decpath_array[dec_path].max_core_id)
        vcmd_dev->vcmd_position[cmdbuf_obj->module_type] = decpath_array[dec_path].start_core_id;
      cmdbuf_obj->core_id = dev->core_id;
      return 0;
    } else {
      spin_unlock_irqrestore(dev->spinlock, flags);
      vcmd_dev->vcmd_position[cmdbuf_obj->module_type]+=decpath_array[dec_path].step;
      if (vcmd_dev->vcmd_position[cmdbuf_obj->module_type] > decpath_array[dec_path].max_core_id)
        vcmd_dev->vcmd_position[cmdbuf_obj->module_type] = decpath_array[dec_path].start_core_id;
      counter++;
    }
    if (counter >= decpath_array[dec_path].max_core_num)
      break;
  }

  //there is no idle vcmd,if low priority,calculate exe time, select the least one.
  // or if high priority, calculate the exe time, select the least one and abort it.
  if (cmdbuf_obj->priority == CMDBUF_PRIORITY_NORMAL) {

    counter = 0;
    //calculate total execute time of all devices
    while (1) {
      dev = vcmd_dev->vcmd_manager[cmdbuf_obj->module_type][vcmd_dev->vcmd_position[cmdbuf_obj->module_type]];
      //read executing cmdbuf address
      if (dev->hw_version_id <= HW_ID_1_0_C) {
        exe_cmdbuf_addr = VCMDGetAddrRegisterValue(dev->hwregs, dev->reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR);
        list            = &dev->list_manager;
        spin_lock_irqsave(dev->spinlock, flags);
        //get the executing cmdbuf node.
        curr_cmdbuf_node = get_cmdbuf_node_in_list_by_addr(vcmd_dev, exe_cmdbuf_addr, list);

        //calculate total execute time of this device
        dev->total_exe_time = calculate_executing_time_after_node(curr_cmdbuf_node);
        spin_unlock_irqrestore(dev->spinlock, flags);
      } else {
        //cmdbuf_id = vcmd_get_register_value((const void *)dev->hwregs,dev->reg_mirror,HWIF_VCMD_CMDBUF_EXECUTING_ID);
        cmdbuf_id = *(dev->vcmd_reg_mem_virtualAddress + EXECUTING_CMDBUF_ID_ADDR + 1);
        spin_lock_irqsave(dev->spinlock, flags);
        if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM || cmdbuf_id == 0) {
          printk(KERN_ERR "CMDBUF_PRIORITY_NORMAL : cmdbuf_id greater than the ceiling !!\n");
          spin_unlock_irqrestore(dev->spinlock, flags);
          return -1;
        }
        //get the executing cmdbuf node.
        curr_cmdbuf_node = vcmd_dev->global_cmdbuf_node[cmdbuf_id];
        if (curr_cmdbuf_node == NULL) {
          list             = &dev->list_manager;
          curr_cmdbuf_node = list->head;
          while (1) {
            if (curr_cmdbuf_node == NULL)
              break;
            cmdbuf_obj_temp = (struct cmdbuf_obj*) curr_cmdbuf_node->data;
            if (cmdbuf_obj_temp->cmdbuf_data_linked && cmdbuf_obj_temp->cmdbuf_run_done == 0)
              break;
            curr_cmdbuf_node = curr_cmdbuf_node->next;
          }
        }

        //calculate total execute time of this device
        dev->total_exe_time = calculate_executing_time_after_node(curr_cmdbuf_node);
        spin_unlock_irqrestore(dev->spinlock, flags);
      }
      vcmd_dev->vcmd_position[cmdbuf_obj->module_type]+=decpath_array[dec_path].step;
      if (vcmd_dev->vcmd_position[cmdbuf_obj->module_type] > decpath_array[dec_path].max_core_id)
        vcmd_dev->vcmd_position[cmdbuf_obj->module_type] = decpath_array[dec_path].start_core_id;
      counter++;
      if (counter >= decpath_array[dec_path].max_core_num)
        break;
    }
    //find the device with the least total_exe_time.
    counter        = 0;
    executing_time = 0xffffffffffffffff;
    while (1) {
      dev = vcmd_dev->vcmd_manager[cmdbuf_obj->module_type][vcmd_dev->vcmd_position[cmdbuf_obj->module_type]];
      if (dev->total_exe_time <= executing_time) {
        executing_time = dev->total_exe_time;
        smallest_dev   = dev;
      }
      vcmd_dev->vcmd_position[cmdbuf_obj->module_type]+=decpath_array[dec_path].step;
      if (vcmd_dev->vcmd_position[cmdbuf_obj->module_type] > decpath_array[dec_path].max_core_id)
        vcmd_dev->vcmd_position[cmdbuf_obj->module_type] = decpath_array[dec_path].start_core_id;
      counter++;
      if (counter >= decpath_array[dec_path].max_core_num)
        break;
    }
    //insert list
    list = &smallest_dev->list_manager;
    spin_lock_irqsave(smallest_dev->spinlock, flags);
    bi_list_insert_node_tail(list, new_cmdbuf_node);
    spin_unlock_irqrestore(smallest_dev->spinlock, flags);
    cmdbuf_obj->core_id = smallest_dev->core_id;
    return 0;
  } else {
    //CMDBUF_PRIORITY_HIGH
    counter = 0;
    //calculate total execute time of all devices
    while (1) {
      dev = vcmd_dev->vcmd_manager[cmdbuf_obj->module_type][vcmd_dev->vcmd_position[cmdbuf_obj->module_type]];
      if (dev->hw_version_id <= HW_ID_1_0_C) {
        //read executing cmdbuf address
        exe_cmdbuf_addr = VCMDGetAddrRegisterValue(dev->hwregs, dev->reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR);
        list            = &dev->list_manager;
        spin_lock_irqsave(dev->spinlock, flags);
        //get the executing cmdbuf node.
        curr_cmdbuf_node = get_cmdbuf_node_in_list_by_addr(vcmd_dev, exe_cmdbuf_addr, list);

        //calculate total execute time of this device
        dev->total_exe_time = calculate_executing_time_after_node_high_priority(curr_cmdbuf_node);
        spin_unlock_irqrestore(dev->spinlock, flags);
      } else {
        cmdbuf_id = *(dev->vcmd_reg_mem_virtualAddress + EXECUTING_CMDBUF_ID_ADDR);
        spin_lock_irqsave(dev->spinlock, flags);
        if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM || cmdbuf_id == 0) {
          printk(KERN_ERR "cmdbuf_id greater than the ceiling !!\n");
          spin_unlock_irqrestore(dev->spinlock, flags);
          return -1;
        }
        //get the executing cmdbuf node.
        curr_cmdbuf_node = vcmd_dev->global_cmdbuf_node[cmdbuf_id];
        if (curr_cmdbuf_node == NULL) {
          list             = &dev->list_manager;
          curr_cmdbuf_node = list->head;
          while (1) {
            if (curr_cmdbuf_node == NULL)
              break;
            cmdbuf_obj_temp = (struct cmdbuf_obj*) curr_cmdbuf_node->data;
            if (cmdbuf_obj_temp->cmdbuf_data_linked && cmdbuf_obj_temp->cmdbuf_run_done == 0)
              break;
            curr_cmdbuf_node = curr_cmdbuf_node->next;
          }
        }

        //calculate total execute time of this device
        dev->total_exe_time = calculate_executing_time_after_node(curr_cmdbuf_node);
        spin_unlock_irqrestore(dev->spinlock, flags);
      }
      vcmd_dev->vcmd_position[cmdbuf_obj->module_type]+=decpath_array[dec_path].step;
      if (vcmd_dev->vcmd_position[cmdbuf_obj->module_type] > decpath_array[dec_path].max_core_id)
        vcmd_dev->vcmd_position[cmdbuf_obj->module_type] = decpath_array[dec_path].start_core_id;
      counter++;
      if (counter >= decpath_array[dec_path].max_core_num)
        break;
    }
    //find the smallest device.
    counter        = 0;
    executing_time = 0xffffffffffffffff;
    while (1) {
      dev = vcmd_dev->vcmd_manager[cmdbuf_obj->module_type][vcmd_dev->vcmd_position[cmdbuf_obj->module_type]];
      if (dev->total_exe_time <= executing_time) {
        executing_time = dev->total_exe_time;
        smallest_dev   = dev;
      }
      vcmd_dev->vcmd_position[cmdbuf_obj->module_type]+=decpath_array[dec_path].step;
      if (vcmd_dev->vcmd_position[cmdbuf_obj->module_type] > decpath_array[dec_path].max_core_id)
        vcmd_dev->vcmd_position[cmdbuf_obj->module_type] = decpath_array[dec_path].start_core_id;
      counter++;
      if (counter >= decpath_array[dec_path].max_core_num)
        break;
    }
    //abort the vcmd and wait
    vcmd_write_register_value(smallest_dev->hwregs, smallest_dev->reg_mirror, HWIF_VCMD_START_TRIGGER, 0);
    vcmd_dev->software_triger_abort = 1;
    if (wait_event_interruptible_timeout(*smallest_dev->wait_abort_queue, wait_abort_rdy(smallest_dev), VCMD_TIMEOUT) <= 0) {
      vcmd_dev->software_triger_abort = 0;
      printk(KERN_INFO "select_vcmd_auto: wait event interruptible timeout\n");
      return -ERESTARTSYS;
    }
    vcmd_dev->software_triger_abort = 0;
    //need to select inserting position again because hw maybe have run to the next node.
    spin_lock_irqsave(smallest_dev->spinlock, flags);
    curr_cmdbuf_node = smallest_dev->list_manager.head;
    while (1) {
      //if list is empty or tail,insert to tail
      if (curr_cmdbuf_node == NULL)
        break;
      cmdbuf_obj_temp = (struct cmdbuf_obj*) curr_cmdbuf_node->data;
      //if find the first node which priority is normal, insert node prior to  the node
      if ((cmdbuf_obj_temp->priority == CMDBUF_PRIORITY_NORMAL) && (cmdbuf_obj_temp->cmdbuf_run_done == 0))
        break;
      curr_cmdbuf_node = curr_cmdbuf_node->next;
    }
    bi_list_insert_node_before(list, curr_cmdbuf_node, new_cmdbuf_node);
    cmdbuf_obj->core_id = smallest_dev->core_id;
    spin_unlock_irqrestore(smallest_dev->spinlock, flags);

    return 0;
  }
  return 0;
}

static long reserve_cmdbuf(struct file* filp, struct sn_tranx_t* tdev, struct exchange_parameter* input_para) {
  struct vcmd_dev* vcmd_dev = tdev->modules[SN_MODULE_VCMD];

  bi_list_node*      new_cmdbuf_node = NULL;
  struct cmdbuf_obj* cmdbuf_obj      = NULL;
  input_para->cmdbuf_id              = 0;
  if (input_para->cmdbuf_size > CMDBUF_MAX_SIZE) {
    return -1;
  }

#if 0
  if (filp) {
    if (down_interruptible(&vcmd_dev->vcmd_reserve_resource))
      return -ERESTARTSYS;
  }
#endif

  sn_pri(tdev, SN_DBG, "reserve cmdbuf filp %p\n", (void*) filp);
  new_cmdbuf_node = create_cmdbuf_node(filp, tdev, input_para);
  if (new_cmdbuf_node == NULL) {
    sn_pri(tdev, SN_ERR, "create cmfbuf node failed.\n");
    return -1;
  }

  cmdbuf_obj                 = (struct cmdbuf_obj*) new_cmdbuf_node->data;
  cmdbuf_obj->module_type    = input_para->module_type;
  cmdbuf_obj->priority       = input_para->priority;
  cmdbuf_obj->executing_time = input_para->executing_time;
  cmdbuf_obj->cmdbuf_size    = CMDBUF_MAX_SIZE;
  input_para->cmdbuf_size    = CMDBUF_MAX_SIZE;
  cmdbuf_obj->filp           = filp;

  input_para->cmdbuf_id                               = cmdbuf_obj->cmdbuf_id;
  vcmd_dev->global_cmdbuf_node[input_para->cmdbuf_id] = new_cmdbuf_node;

  return 0;
}

static long release_cmdbuf(struct file* filp, struct sn_tranx_t* tdev, u16 cmdbuf_id) {
  struct vcmd_dev*       vcmd_dev         = tdev->modules[SN_MODULE_VCMD];
  struct cmdbuf_obj*     cmdbuf_obj       = NULL;
  bi_list_node*          last_cmdbuf_node = NULL;
  bi_list_node*          new_cmdbuf_node  = NULL;
  bi_list*               list             = NULL;
  u32                    module_type;
  unsigned long flags;
  struct hantrovcmd_dev* dev = NULL;

  new_cmdbuf_node = vcmd_dev->global_cmdbuf_node[cmdbuf_id];
  if (new_cmdbuf_node == NULL) {
    //should not happen
    sn_pri(tdev, SN_ERR, "hantrovcmd: ERROR cmdbuf_id =%d!\n", cmdbuf_id);
    return -1;
  }

  cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
  if (cmdbuf_obj->filp != filp) {
    //should not happen
    sn_pri(tdev, SN_ERR, "hantrovcmd: ERROR cmdbuf_id =%d!!\n", cmdbuf_id);
    return -1;
  }
  module_type = cmdbuf_obj->module_type;
  if (down_interruptible(&vcmd_dev->vcmd_reserve_cmdbuf_sem[module_type])) {
    sn_pri(tdev, SN_INF, "release_cmdbuf: down interruptible\n");
    return -ERESTARTSYS;
  }

  dev = &vcmd_dev->hantrovcmd_data[cmdbuf_obj->core_id];

  spin_lock_irqsave(dev->spinlock, flags);
  list                           = &dev->list_manager;
  cmdbuf_obj->cmdbuf_need_remove = 1;
  last_cmdbuf_node               = new_cmdbuf_node->previous;
  while (1) {
    //remove current node
    cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
    if (cmdbuf_obj->cmdbuf_need_remove == 1) {
      new_cmdbuf_node = remove_cmdbuf_node_from_list(list, new_cmdbuf_node);
      if (new_cmdbuf_node) {
        vcmd_dev->global_cmdbuf_node[cmdbuf_obj->cmdbuf_id] = NULL;
        free_cmdbuf_node(vcmd_dev, new_cmdbuf_node);
      }
    }
    if (last_cmdbuf_node == NULL)
      break;
    new_cmdbuf_node  = last_cmdbuf_node;
    last_cmdbuf_node = new_cmdbuf_node->previous;
  }
  spin_unlock_irqrestore(dev->spinlock, flags);
  up(&vcmd_dev->vcmd_reserve_cmdbuf_sem[module_type]);

#if 0
  if (filp)
    up(&vcmd_dev->vcmd_reserve_resource);
#endif
  return 0;
}
static long release_cmdbuf_node(struct vcmd_dev* vcmd_dev, bi_list* list, bi_list_node* cmdbuf_node) {
  bi_list_node*      new_cmdbuf_node = NULL;
  struct cmdbuf_obj* cmdbuf_obj      = NULL;

  /*get cmdbuf object according to cmdbuf_id*/
  new_cmdbuf_node = cmdbuf_node;
  if (new_cmdbuf_node == NULL)
    return -1;
  new_cmdbuf_node = remove_cmdbuf_node_from_list(list, new_cmdbuf_node);
  if (new_cmdbuf_node) {
    cmdbuf_obj                                          = (struct cmdbuf_obj*) new_cmdbuf_node->data;
    vcmd_dev->global_cmdbuf_node[cmdbuf_obj->cmdbuf_id] = NULL;
    free_cmdbuf_node(vcmd_dev, new_cmdbuf_node);
    return 0;
  }
  return 1;
}

static long release_cmdbuf_node_cleanup(struct vcmd_dev* vcmd_dev, bi_list* list) {
  bi_list_node*      new_cmdbuf_node = NULL;
  struct cmdbuf_obj* cmdbuf_obj      = NULL;

  while (1) {
    new_cmdbuf_node = list->head;
    if (new_cmdbuf_node == NULL)
      return 0;
    bi_list_remove_node(list, new_cmdbuf_node);
    cmdbuf_obj                                          = (struct cmdbuf_obj*) new_cmdbuf_node->data;
    vcmd_dev->global_cmdbuf_node[cmdbuf_obj->cmdbuf_id] = NULL;
    free_cmdbuf_node(vcmd_dev, new_cmdbuf_node);
  }
  return 0;
}

static bi_list_node* find_last_linked_cmdbuf(bi_list_node* current_node) {
  bi_list_node*      new_cmdbuf_node = current_node;
  bi_list_node*      last_cmdbuf_node;
  struct cmdbuf_obj* cmdbuf_obj = NULL;

  if (current_node == NULL)
    return NULL;
  last_cmdbuf_node = new_cmdbuf_node;
  new_cmdbuf_node  = new_cmdbuf_node->previous;
  while (1) {
    if (new_cmdbuf_node == NULL)
      return last_cmdbuf_node;
    cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
    if (cmdbuf_obj->cmdbuf_data_linked) {
      return new_cmdbuf_node;
    }
    last_cmdbuf_node = new_cmdbuf_node;
    new_cmdbuf_node  = new_cmdbuf_node->previous;
  }
  return NULL;
}

#define DDR_INTERLEAVE_SIZE 4096
#define NEXT_MULTIPLE(value, n) (((value) + (n) -1) & ~((n) -1))
static int ddr_buffer_sync(u32 volatile __iomem* virtual_address, size_t bus_address, u32 buf_size, u32 interleave_size) {
  size_t       last_address = 0;
  u32          offset;
  u32          size;
  u32          i;
  volatile u32 tmp_value;
  ;

  for (i = 0; i < buf_size;) {
    if (bus_address % interleave_size) {
      last_address = NEXT_MULTIPLE(bus_address, interleave_size);
      offset       = last_address - bus_address;
    } else {
      offset = interleave_size;
    }
    size = min(offset, buf_size - i);
    virtual_address += size / sizeof(u32);
    bus_address += size;
    i += size;
    tmp_value = readl(virtual_address - 1);
  }

  return 0;
}

#define DECODER_REG_ADDR_MSB_LSB_POS 800
#define DECODER_REG_CACHE_ADDR_SET_MIN 180
#define DECODER_REG_CACHE_ADDR_SET_MAX 285

static __inline uint64_t do_patch(struct sn_tranx_t* tdev, struct file* filp, uint64_t handle) {
  uint64_t addr;
  addr = sn_mem_osal_translate_handle(tdev, filp, handle, 0);
  return addr;
}

static int patch_dec_cmd_buffer(struct sn_tranx_t* tdev, struct file* filp, struct cmdbuf_obj* obj, u32 *cmd_buf, u32 *patch_buf) {
  u32 i;
  u64 addr_handle;
  u64 phy_addr;
  u64 addr_msb;
  u8  tmp = 0;
  u32 patch_handle;
  u8  handle_flag = 0, msb_lsb_flag = 0;
  u16 first_part_pos = 0;
  u32 patch_cnt;
  u32 offset;

  patch_cnt = (*patch_buf - 1)/2;
  for (i = 0; i < patch_cnt; i++) {
    offset         = *(patch_buf + i*2 + 1);
    patch_handle   = *(patch_buf + i*2 + 2);
    handle_flag    = (patch_handle >> 24) & 1;
    msb_lsb_flag   = (patch_handle >> 16) & 0xff;
    first_part_pos = patch_handle & 0xffff;

    if (handle_flag) {
      if (msb_lsb_flag == OSAL_ADDR_MSB) {
        addr_msb                                       = (u64) * (cmd_buf + offset);
        addr_handle                                    = (addr_msb << 32) | *(cmd_buf + first_part_pos);
        phy_addr                                       = do_patch(tdev, filp, addr_handle);
        *(cmd_buf + offset)         = (phy_addr >> 32) & 0xFFFFFFFF;
        *(cmd_buf + first_part_pos) = phy_addr & 0xFFFFFFFF;
      } else if (msb_lsb_flag == OSAL_ADDR_LSB) {
        addr_msb                                       = (u64) * (cmd_buf + first_part_pos);
        addr_handle                                    = (addr_msb << 32) | *(cmd_buf + offset);
        phy_addr                                       = do_patch(tdev, filp, addr_handle);
        *(cmd_buf + offset)              = phy_addr & 0xFFFFFFFF;
        *(cmd_buf + first_part_pos) = (phy_addr >> 32) & 0xFFFFFFFF;
      } else if (msb_lsb_flag == OSAL_ADDR_MSB_SHIFT) {
        addr_msb                                       = (u64) * (cmd_buf + offset);
        addr_handle                                    = (addr_msb << 28) | ((*(cmd_buf + first_part_pos) >> 4) & 0xFFFFFFF);
        tmp                                            = *(cmd_buf + first_part_pos) & 0xF;
        addr_handle                                    = addr_handle << 4;
        phy_addr                                       = do_patch(tdev, filp, addr_handle);
        phy_addr                                       = phy_addr >> 4;
        *(cmd_buf + offset)         = (phy_addr >> 28) & 0xFFFFFFFF;
        *(cmd_buf + first_part_pos) = ((phy_addr & 0xFFFFFFF) << 4) | tmp;
      }
      if( phy_addr == 1){
        return 1;
      }
    }
  }
  return 0;
}

/******************************************************************************/
static int check_cmdbuf_irq(struct hantrovcmd_dev* dev, struct cmdbuf_obj* cmdbuf_obj, u32* irq_status_ret) {

  int           rdy = 0;
  unsigned long flags;
  spin_lock_irqsave(dev->spinlock, flags);
  if (cmdbuf_obj->cmdbuf_run_done) {
    rdy             = 1;
    *irq_status_ret = cmdbuf_obj->executing_status; //need to decide how to assign this variable
  }
  spin_unlock_irqrestore(dev->spinlock, flags);
  return rdy;
}

static long link_and_run_cmdbuf(struct file* filp, struct sn_tranx_t* tdev, struct exchange_parameter* input_para, u32 *cmd_buf, u32 *patch_buf, u32 *ppu_buf) {
  struct cmdbuf_obj*     cmdbuf_obj      = NULL;
  bi_list_node*          new_cmdbuf_node = NULL;
  bi_list_node*          last_cmdbuf_node;
  u32 __iomem*           jmp_addr = NULL;
  u32                    opCode;
  u32                    tempOpcode;
  u16                    tempCmdbufId;
  u32                    record_last_cmdbuf_rdy_num;
  unsigned long          flags;
  int                    return_value;
  u16                    cmdbuf_id = input_para->cmdbuf_id;
  struct vcmd_dev*       vcmd_dev  = NULL;
  struct hantrovcmd_dev* dev       = NULL;
  u32 offset;
  u32 data_size = 0;
  u8 *ppu_buf_temp;
  int ret = 0;
  vcmd_dev = tdev->modules[SN_MODULE_VCMD];

  new_cmdbuf_node = vcmd_dev->global_cmdbuf_node[cmdbuf_id];
  if (new_cmdbuf_node == NULL) {
    //should not happen
    sn_pri(tdev, SN_ERR, "link_and_run_cmdbuf: ERROR cmdbuf_id=%d!\n", cmdbuf_id);
    return -1;
  }
  cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
  if (cmdbuf_obj->filp != filp) {
    //should not happen
    sn_pri(tdev, SN_ERR, "link_and_run_cmdbuf: ERROR cmdbuf_id=%d!!\n", cmdbuf_id);
    return -1;
  }
  cmdbuf_obj->cmdbuf_data_loaded = 1;
  cmdbuf_obj->cmdbuf_size        = input_para->cmdbuf_size;
  cmdbuf_obj->waited             = 0;

  if (down_interruptible(&vcmd_dev->vcmd_reserve_cmdbuf_sem[cmdbuf_obj->module_type])){
    sn_pri(tdev, SN_INF, "link_and_run_cmdbuf: down interruptible\n");
    return -ERESTARTSYS;
  }

  if (filp) {
    if (input_para->core_id == 255) {
      return_value = select_vcmd_auto(vcmd_dev, new_cmdbuf_node, input_para->dec_path);
      input_para->core_id = cmdbuf_obj->core_id;
    }
    else {
      return_value = select_vcmd(vcmd_dev, new_cmdbuf_node, input_para->core_id);
    }
  } else {
    return_value = init_select_vcmd(vcmd_dev, new_cmdbuf_node, input_para->core_id);
  }
  if (return_value) {
    ret = return_value;
    goto end;
  }
  sn_pri(tdev, SN_DBG, "select_vcmd core_id %d\n", input_para->core_id);

  if (filp) {
    if (!cmd_buf || !patch_buf) {
      sn_pri(tdev, SN_ERR, "link_and_run_cmdbuf: ERROR cmdbuf_addr/patchbuf_addr!!\n");
      ret = -1;
      goto end;
    }
    // core_id: coreA 0|2 coreB: 1|3, default ppu set is for coreA
    if ((input_para->core_id == 1 || input_para->core_id == 3) && input_para->dec_path == DEC_PATH_AB) {
      if (!ppu_buf) {
        sn_pri(tdev, SN_ERR, "link_and_run_cmdbuf: ERROR ppubuf_addr!!\n");
        ret = -1;
        goto end;
      }
      if (input_para->ppubuf_size) {
        ppu_buf_temp = (u8*)ppu_buf;
        offset = *((u32 *)ppu_buf_temp);
        data_size = *((u32 *)ppu_buf_temp + 1);
        memcpy(((u8*)cmd_buf+offset), ((u8*)ppu_buf_temp+8), data_size);
        ppu_buf_temp += data_size + 8;

        offset = *((u32 *)ppu_buf_temp);
        data_size = *((u32 *)ppu_buf_temp + 1);
        memcpy(((u8*)cmd_buf+offset), ((u8*)ppu_buf_temp+8), data_size);
      }
    }
    if (patch_dec_cmd_buffer(tdev, filp, cmdbuf_obj, cmd_buf, patch_buf)) {
      sn_pri(tdev, SN_ERR, "patch_dec_cmd_buffer: INVALID ADDR.\n");
      ret = -1;
      goto end;
    }
    memcpy_toio(cmdbuf_obj->cmdbuf_virtualAddress, cmd_buf, cmdbuf_obj->cmdbuf_size);
  }
#ifdef VCMD_DEBUG_INTERNAL
  {
    u32 i, inst = 0, size = 0;
    sn_pri(tdev, SN_INF, "vcmd link, current cmdbuf content\n");
    for (i = 0; i < cmdbuf_obj->cmdbuf_size / 4; i++) {
      if (i == inst) {
        PrintInstr(i, *(cmdbuf_obj->cmdbuf_virtualAddress + i), &size);
        inst += size;
      } else {
        sn_pri(tdev, SN_INF, "current cmdbuf data %d = 0x%x\n", i, *(cmdbuf_obj->cmdbuf_virtualAddress + i));
      }
    }
  }
#endif
  //test nop and end opcode, then assign value.
  cmdbuf_obj->has_end_cmdbuf       = 0; //0: has jmp opcode,1 has end code
  cmdbuf_obj->no_normal_int_cmdbuf = 0; //0: interrupt when JMP,1 not interrupt when JMP
  jmp_addr                         = cmdbuf_obj->cmdbuf_virtualAddress + (cmdbuf_obj->cmdbuf_size / 4);
  opCode = tempOpcode = *(jmp_addr - 4);
  opCode >>= 27;
  opCode <<= 27;
  //we can't identify END opcode or JMP opcode, so we don't support END opcode in control sw and driver.
  if (opCode == OPCODE_JMP) {
    //jmp
    opCode = tempOpcode;
    opCode &= 0x02000000;
    if (opCode == JMP_IE_1) {
      cmdbuf_obj->no_normal_int_cmdbuf = 0;
    } else {
      cmdbuf_obj->no_normal_int_cmdbuf = 1;
    }
  } else {
    //not support other opcode
    sn_pri(tdev, SN_ERR, "err opCode %d\n", opCode);
    ret = -1;
    goto end;
  }
  tempCmdbufId = *(jmp_addr - 1);
  if (tempCmdbufId == ANY_CMDBUF_ID)
    *(jmp_addr - 1) = (u32) (cmdbuf_id);
  else if (tempCmdbufId != cmdbuf_id) {
    sn_pri(tdev, SN_ERR, "cmdbuf_id %d mismatch with expected cmdbuf_id %d\n", tempCmdbufId, cmdbuf_id);
    ret = -1;
    goto end;
  }

  dev                 = &vcmd_dev->hantrovcmd_data[cmdbuf_obj->core_id];
  //set ddr address for vcmd registers copy.
  if (dev->hw_version_id > HW_ID_1_0_C) {
    //read vcmd executing register into ddr memory.
    //now core id is got and output ddr address of vcmd register can be filled in.
    //each core has its own fixed output ddr address of vcmd registers.
    jmp_addr = cmdbuf_obj->cmdbuf_virtualAddress;
    if (sizeof(size_t) == 8) {
      *(jmp_addr + 2) = (u32) ((u64) (dev->vcmd_reg_mem_busAddress + (EXECUTING_CMDBUF_ID_ADDR + 1) * 4) >> 32);
    } else {
      *(jmp_addr + 2) = 0;
    }
    *(jmp_addr + 1) = (u32) ((dev->vcmd_reg_mem_busAddress + (EXECUTING_CMDBUF_ID_ADDR + 1) * 4));

    jmp_addr = cmdbuf_obj->cmdbuf_virtualAddress + (cmdbuf_obj->cmdbuf_size / 4);
    //read vcmd all registers into ddr memory.
    //now core id is got and output ddr address of vcmd registers can be filled in.
    //each core has its own fixed output ddr address of vcmd registers.
    if (sizeof(size_t) == 8) {
      *(jmp_addr - 6) = (u32) ((u64) dev->vcmd_reg_mem_busAddress >> 32);
    } else {
      *(jmp_addr - 6) = 0;
    }
    *(jmp_addr - 7) = (u32) (dev->vcmd_reg_mem_busAddress);
  }
  //start to link and/or run
  spin_lock_irqsave(dev->spinlock, flags);
  last_cmdbuf_node           = find_last_linked_cmdbuf(new_cmdbuf_node);
  record_last_cmdbuf_rdy_num = dev->sw_cmdbuf_rdy_num;
  vcmd_link_cmdbuf(dev, last_cmdbuf_node);
  if (dev->working_state == WORKING_STATE_IDLE) {
    //run
    vcmd_start(dev, last_cmdbuf_node);
  } else {
    //just update cmdbuf ready number
    if (record_last_cmdbuf_rdy_num != dev->sw_cmdbuf_rdy_num)
      vcmd_write_register_value(dev->hwregs, dev->reg_mirror, HWIF_VCMD_RDY_CMDBUF_COUNT, dev->sw_cmdbuf_rdy_num);
  }
  spin_unlock_irqrestore(dev->spinlock, flags);

end:
  up(&vcmd_dev->vcmd_reserve_cmdbuf_sem[cmdbuf_obj->module_type]);

  return ret;
}

/******************************************************************************/
static int check_mc_cmdbuf_irq(struct file* filp, struct vcmd_dev* vcmd_dev, struct cmdbuf_obj* cmdbuf_obj, u32* irq_status_ret) {
  int                    k;
  bi_list_node*          new_cmdbuf_node = NULL;
  struct hantrovcmd_dev* dev             = NULL;

  for (k = 0; k < TOTAL_DISCRETE_CMDBUF_NUM; k++) {
    new_cmdbuf_node = vcmd_dev->global_cmdbuf_node[k];
    if (new_cmdbuf_node == NULL)
      continue;

    cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
    if (!cmdbuf_obj || cmdbuf_obj->filp != filp)
      continue;

    dev = &vcmd_dev->hantrovcmd_data[cmdbuf_obj->core_id];
    if (check_cmdbuf_irq(dev, cmdbuf_obj, irq_status_ret) == 1) {
      /* Return cmdbuf_id when ANY_CMDBUF_ID is used. */
      if (!cmdbuf_obj->waited) {
        *irq_status_ret    = cmdbuf_obj->cmdbuf_id;
        cmdbuf_obj->waited = 1;
        return 1;
      }
    }
  }

  return 0;
}

static unsigned int wait_cmdbuf_ready(struct file* filp, struct vcmd_dev* vcmd_dev, u16 cmdbuf_id, u32* irq_status_ret) {

  struct cmdbuf_obj*     cmdbuf_obj      = NULL;
  bi_list_node*          new_cmdbuf_node = NULL;
  struct hantrovcmd_dev* dev             = NULL;

  if (cmdbuf_id != ANY_CMDBUF_ID) {
    new_cmdbuf_node = vcmd_dev->global_cmdbuf_node[cmdbuf_id];
    if (new_cmdbuf_node == NULL) {
      //should not happen
      printk(KERN_ERR "wait_cmdbuf_ready: ERROR cmdbuf_id=%d!\n", cmdbuf_id);
      return -1;
    }
    cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
    if (cmdbuf_obj->filp != filp) {
      //should not happen
      printk(KERN_ERR "wait_cmdbuf_ready: ERROR cmdbuf_id=%d!!\n", cmdbuf_id);
      return -1;
    }
    dev = &vcmd_dev->hantrovcmd_data[cmdbuf_obj->core_id];

    if (wait_event_interruptible_timeout(*dev->wait_queue, check_cmdbuf_irq(dev, cmdbuf_obj, irq_status_ret), VCMD_TIMEOUT) <= 0) {
      printk(KERN_INFO "vcmd_wait_queue_0 interrupted\n");
      return -ERESTARTSYS;
    }
    return 0;
  } else {
    if (check_mc_cmdbuf_irq(filp, vcmd_dev, cmdbuf_obj, irq_status_ret))
      return 0;
    if (wait_event_interruptible_timeout(vcmd_dev->mc_wait_queue, check_mc_cmdbuf_irq(filp, vcmd_dev, cmdbuf_obj, irq_status_ret), VCMD_TIMEOUT) <= 0) {
      printk(KERN_INFO "multicore wait queue interrupted\n");
      return -ERESTARTSYS;
    }
    return 0;
  }
}

long hantrovcmd_ioctl(struct file* filp, unsigned int cmd, unsigned long arg, struct sn_tranx_t* tdev) {
  return -ENOTTY;
}

int hantrovcmd_open(struct sn_tranx_t* tdev, struct file* filp) {
  int              result   = 0;
  struct vcmd_dev* vcmd_dev = NULL;

  vcmd_dev = (struct vcmd_dev*) tdev->modules[SN_MODULE_VCMD];
  if (vcmd_dev == NULL) {
    sn_pri(tdev, SN_ERR, "VCMD device NULL\n");
    return -1;
  }
  sn_pri(tdev, SN_DBG, "dev opened\n");
  return result;
}

int hantrovcmd_release(struct sn_tranx_t* tdev, struct file* filp) {
  struct vcmd_dev*       vcmd_dev           = tdev->modules[SN_MODULE_VCMD];
  u32                    core_id            = 0;
  u32                    release_cmdbuf_num = 0;
  bi_list_node*          new_cmdbuf_node    = NULL;
  bi_list_node*          next_cmdbuf_node   = NULL;
  struct cmdbuf_obj*     cmdbuf_obj_temp    = NULL;
  struct hantrovcmd_dev* dev                = vcmd_dev->hantrovcmd_data;

  unsigned long flags;
  long          retVal = 0;

  sn_pri(tdev, SN_DBG, "hantrovcmd_release %p\n", (void*) filp);
  sn_pri(tdev, SN_DBG, "dev closed\n");

  for (core_id = 0; core_id < vcmd_dev->total_vcmd_core_num; core_id++) {
    if (dev == NULL || !dev[core_id].is_valid)
      continue;
    if (down_timeout(&vcmd_dev->vcmd_reserve_cmdbuf_sem[dev[core_id].vcmd_core_cfg.sub_module_type], VCMD_TIMEOUT)) {
      sn_pri(tdev, SN_INF, "hantrovcmd_release: down timeout\n");
      return -ERESTARTSYS;
    }
    spin_lock_irqsave(dev[core_id].spinlock, flags);
    new_cmdbuf_node = dev[core_id].list_manager.head;
    while (1) {
      if (new_cmdbuf_node == NULL)
        break;
      cmdbuf_obj_temp  = (struct cmdbuf_obj*) new_cmdbuf_node->data;
      next_cmdbuf_node = new_cmdbuf_node->next;
      if (dev[core_id].hwregs && (cmdbuf_obj_temp->filp == filp)) {
        if (cmdbuf_obj_temp->cmdbuf_run_done) {
          cmdbuf_obj_temp->cmdbuf_need_remove = 1;
          retVal                              = release_cmdbuf_node(vcmd_dev, &dev[core_id].list_manager, new_cmdbuf_node);
        } else if (cmdbuf_obj_temp->cmdbuf_data_linked == 0) {
          cmdbuf_obj_temp->cmdbuf_data_linked = 1;
          cmdbuf_obj_temp->cmdbuf_run_done    = 1;
          cmdbuf_obj_temp->cmdbuf_need_remove = 1;
          retVal                              = release_cmdbuf_node(vcmd_dev, &dev[core_id].list_manager, new_cmdbuf_node);
        } else if (cmdbuf_obj_temp->cmdbuf_data_linked == 1 && dev[core_id].working_state == WORKING_STATE_IDLE) {
          cmdbuf_obj_temp->cmdbuf_run_done    = 1;
          cmdbuf_obj_temp->cmdbuf_need_remove = 1;
          retVal                              = release_cmdbuf_node(vcmd_dev, &dev[core_id].list_manager, new_cmdbuf_node);
        } else if (cmdbuf_obj_temp->cmdbuf_data_linked == 1 && dev[core_id].working_state == WORKING_STATE_WORKING) {
          int ret;
          //abort the vcmd and wait
          vcmd_dev->software_triger_abort = 1;
          dev[core_id].triger_cmdbuf_id   = cmdbuf_obj_temp->cmdbuf_id;
          sn_pri(tdev, SN_DBG, "software abort triger start, dev %p, cmdbuf id %d\n", &dev[core_id], cmdbuf_obj_temp->cmdbuf_id);
          spin_unlock_irqrestore(dev[core_id].spinlock, flags);
          ret = wait_event_interruptible_timeout(*dev[core_id].wait_queue, cmdbuf_obj_temp->cmdbuf_run_done, VCMD_TIMEOUT);
          if (ret == 0) {
            up(&vcmd_dev->vcmd_reserve_cmdbuf_sem[dev[core_id].vcmd_core_cfg.sub_module_type]);
            sn_pri(tdev, SN_INF, "software triger timeout\n");
            vcmd_dev->software_triger_abort = 0;
            return -ERESTARTSYS;
          }
          sn_pri(tdev, SN_DBG, "software abort finish\n");
          spin_lock_irqsave(dev[core_id].spinlock, flags);
          cmdbuf_obj_temp->cmdbuf_run_done    = 1;
          cmdbuf_obj_temp->cmdbuf_need_remove = 1;
          retVal                              = release_cmdbuf_node(vcmd_dev, &dev[core_id].list_manager, new_cmdbuf_node);
          vcmd_dev->software_triger_abort     = 0;
        }
        release_cmdbuf_num++;
        sn_pri(tdev, SN_DBG, "release reserved cmdbuf\n");
      }
      new_cmdbuf_node = next_cmdbuf_node;
    }
    spin_unlock_irqrestore(dev[core_id].spinlock, flags);

    if (release_cmdbuf_num)
      wake_up_interruptible_all(&vcmd_dev->vcmd_cmdbuf_memory_wait);
    up(&vcmd_dev->vcmd_reserve_cmdbuf_sem[dev[core_id].vcmd_core_cfg.sub_module_type]);
  }

  {
    // release the unlink cmdbuf
    u16 n;
    u8 start_num;

    release_cmdbuf_num = 0;
    start_num = vcmd_dev->total_vcmd_core_num + 1;   // reserved cmd buf: 0 and core_num count for hw
    for (n = start_num; n < TOTAL_DISCRETE_CMDBUF_NUM; n++) {
      new_cmdbuf_node = vcmd_dev->global_cmdbuf_node[n];
      if (new_cmdbuf_node) {
        cmdbuf_obj_temp = (struct cmdbuf_obj*) new_cmdbuf_node->data;
        if (cmdbuf_obj_temp && cmdbuf_obj_temp->filp == filp) {
          if (cmdbuf_obj_temp->cmdbuf_data_loaded == 0) {
            vcmd_dev->global_cmdbuf_node[n] = NULL;
            free_cmdbuf_node(vcmd_dev, new_cmdbuf_node);
            release_cmdbuf_num++;
          }
        }
      }

    }
    if (release_cmdbuf_num)
      wake_up_interruptible_all(&vcmd_dev->vcmd_cmdbuf_memory_wait);
  }

  return 0;
}

static u32 vcmd_pool_release(struct vcmd_dev* vcmd_dev) {
  struct sn_tranx_t* tdev = (struct sn_tranx_t*)vcmd_dev->tdev;
  /* Release osal memeory */
  if (vcmd_dev->g_vcmd_base_ddr_hw)
    return sn_mem_osal_free_mem(tdev, vcmd_dev->g_vcmd_base_ddr_hw, NULL);

  return 0;
}

/*------------------------------------------------------------------------------
 Function name   : vcmd_pcie_init
 Description     : Initialize PCI Hw access

 Return type     : int
 ------------------------------------------------------------------------------*/
static int vcmd_pcie_init(struct sn_tranx_t* tdev) {
  struct vcmd_dev* vcmd_dev = NULL;
  vcmd_dev                  = tdev->modules[SN_MODULE_VCMD];

  g_vcmd_dev                     = tdev->pdev;
  vcmd_dev->base_ddr_addr        = 0;
  vcmd_dev->g_vcmd_base_ddr_hw   = sn_mem_osal_alloc_mem(tdev, MAP_SIZE, NULL, 0, 1);
  vcmd_dev->osal_mem_handle      = sn_mem_osal_translate_mem(tdev, 0, vcmd_dev->g_vcmd_base_ddr_hw);
  vcmd_dev->g_vcmd_base_ddr_virt = sn_mem_osal_translate_mmio(tdev, vcmd_dev->g_vcmd_base_ddr_hw);
  sn_pri(tdev, SN_INF, "osal_mem_handle=0x%llx\n", vcmd_dev->osal_mem_handle);

  vcmd_dev->vcmd_buf_mem_pool.busAddress     = vcmd_dev->g_vcmd_base_ddr_hw;
  vcmd_dev->vcmd_buf_mem_pool.size           = CMDBUF_POOL_TOTAL_SIZE;
  vcmd_dev->vcmd_buf_mem_pool.virtualAddress = vcmd_dev->g_vcmd_base_ddr_virt;

  vcmd_dev->vcmd_status_buf_mem_pool.busAddress     = vcmd_dev->g_vcmd_base_ddr_hw + CMDBUF_POOL_TOTAL_SIZE;
  vcmd_dev->vcmd_status_buf_mem_pool.size           = CMDBUF_POOL_TOTAL_SIZE;
  vcmd_dev->vcmd_status_buf_mem_pool.virtualAddress = vcmd_dev->g_vcmd_base_ddr_virt + CMDBUF_POOL_TOTAL_SIZE;

  vcmd_dev->vcmd_cmbbuf_patch_pool.busAddress     = vcmd_dev->g_vcmd_base_ddr_hw + CMDBUF_POOL_TOTAL_SIZE * 2;
  vcmd_dev->vcmd_cmbbuf_patch_pool.size           = CMDBUF_POOL_TOTAL_SIZE;
  vcmd_dev->vcmd_cmbbuf_patch_pool.virtualAddress = vcmd_dev->g_vcmd_base_ddr_virt + CMDBUF_POOL_TOTAL_SIZE * 2;

  vcmd_dev->vcmd_registers_mem_pool.busAddress     = vcmd_dev->g_vcmd_base_ddr_hw + CMDBUF_POOL_TOTAL_SIZE * 3;
  vcmd_dev->vcmd_registers_mem_pool.size           = CMDBUF_REGS_TOTAL_SIZE;
  vcmd_dev->vcmd_registers_mem_pool.virtualAddress = vcmd_dev->g_vcmd_base_ddr_virt + CMDBUF_POOL_TOTAL_SIZE * 3;

  return 0;
}

static void vcmd_link_cmdbuf(struct hantrovcmd_dev* dev, bi_list_node* last_linked_cmdbuf_node) {
  bi_list_node*      new_cmdbuf_node  = NULL;
  bi_list_node*      next_cmdbuf_node = NULL;
  struct cmdbuf_obj* cmdbuf_obj       = NULL;
  struct cmdbuf_obj* next_cmdbuf_obj  = NULL;
  u32 __iomem*       jmp_addr         = NULL;
  u32                operation_code;
  u32                checked_value;
  u32 __iomem*       checked_vir_address;
  size_t             checked_bus_address;
  size_t             jmp_bus_addr;
  struct vcmd_dev*   vcmd_dev = (struct vcmd_dev*) dev->vcmd_dev;
  volatile u32 tmp = 0; // Read back last written value to ensure mmio sync (volatile just to be sure!!)

  new_cmdbuf_node = last_linked_cmdbuf_node;
  //for the first cmdbuf.
  if (new_cmdbuf_node != NULL) {
    cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
    if ((cmdbuf_obj->cmdbuf_data_linked == 0)) {
      dev->sw_cmdbuf_rdy_num++;
      cmdbuf_obj->cmdbuf_data_linked = 1;
      dev->duration_without_int      = 0;
      if (cmdbuf_obj->has_end_cmdbuf == 0) {
        if (cmdbuf_obj->no_normal_int_cmdbuf == 1) {
          dev->duration_without_int = cmdbuf_obj->executing_time;
          //maybe nop is modified, so write back.
          if (dev->duration_without_int >= INT_MIN_SUM_OF_IMAGE_SIZE) {
            jmp_addr                  = cmdbuf_obj->cmdbuf_virtualAddress + (cmdbuf_obj->cmdbuf_size / 4);
            jmp_bus_addr              = cmdbuf_obj->cmdbuf_busAddress + cmdbuf_obj->cmdbuf_size;
            operation_code            = *(jmp_addr - 4);
            operation_code            = JMP_IE_1 | operation_code;
            *(jmp_addr - 4)           = operation_code;
            dev->duration_without_int = 0;
            ddr_buffer_sync(jmp_addr - 4, jmp_bus_addr - 16, sizeof(u32), DDR_INTERLEAVE_SIZE);
            tmp = readl((u32 volatile __iomem*)jmp_addr - 4);
          }
        }
      }
    }
  }
  while (1) {
    if (new_cmdbuf_node == NULL)
      break;
    if (new_cmdbuf_node->next == NULL)
      break;
    next_cmdbuf_node = new_cmdbuf_node->next;
    cmdbuf_obj       = (struct cmdbuf_obj*) new_cmdbuf_node->data;
    next_cmdbuf_obj  = (struct cmdbuf_obj*) next_cmdbuf_node->data;
    if (cmdbuf_obj->has_end_cmdbuf == 0) {
      //need to link, current cmdbuf link to next cmdbuf
      jmp_addr     = cmdbuf_obj->cmdbuf_virtualAddress + (cmdbuf_obj->cmdbuf_size / 4);
      jmp_bus_addr = cmdbuf_obj->cmdbuf_busAddress + cmdbuf_obj->cmdbuf_size;
      if (dev->hw_version_id > HW_ID_1_0_C) {
        //set next cmdbuf id
        *(jmp_addr - 1) = next_cmdbuf_obj->cmdbuf_id;
      }

      if (sizeof(size_t) == 8) {
        *(jmp_addr - 2) = (u32) ((u64) (next_cmdbuf_obj->cmdbuf_busAddress - vcmd_dev->base_ddr_addr) >> 32);
      } else {
        *(jmp_addr - 2) = 0;
      }
      *(jmp_addr - 3) = (u32) (next_cmdbuf_obj->cmdbuf_busAddress - vcmd_dev->base_ddr_addr);

      operation_code = *(jmp_addr - 4);
      operation_code >>= 16;
      operation_code <<= 16;
      *(jmp_addr - 4) = checked_value     = (u32) (operation_code | JMP_RDY_1 | ((next_cmdbuf_obj->cmdbuf_size + 7) / 8));
      tmp = readl((u32 volatile __iomem*)jmp_addr - 4);
      checked_vir_address                 = jmp_addr - 4;
      checked_bus_address                 = jmp_bus_addr - 16;
      next_cmdbuf_obj->cmdbuf_data_linked = 1;
      dev->sw_cmdbuf_rdy_num++;
      //modify nop code of next cmdbuf
      if (next_cmdbuf_obj->has_end_cmdbuf == 0) {
        if (next_cmdbuf_obj->no_normal_int_cmdbuf == 1) {
          dev->duration_without_int += next_cmdbuf_obj->executing_time;

          //maybe we see the modified nop before abort, so need to write back.
          if (dev->duration_without_int >= INT_MIN_SUM_OF_IMAGE_SIZE) {
            jmp_addr        = next_cmdbuf_obj->cmdbuf_virtualAddress + (next_cmdbuf_obj->cmdbuf_size / 4);
            operation_code  = *(jmp_addr - 4);
            operation_code  = JMP_IE_1 | operation_code;
            *(jmp_addr - 4) = checked_value = operation_code;
            tmp = readl((u32 volatile __iomem*)jmp_addr - 4);
            checked_vir_address             = jmp_addr - 4;
            checked_bus_address             = jmp_bus_addr - 16;
            dev->duration_without_int       = 0;
          }
        }
      } else {
        dev->duration_without_int = 0;
      }
#ifdef VCMD_DEBUG_INTERNAL
      {
        u32 i;
        printk(KERN_INFO "vcmd link, last cmdbuf content\n");
        for (i = cmdbuf_obj->cmdbuf_size / 4 - 8; i < cmdbuf_obj->cmdbuf_size / 4; i++) {
          printk(KERN_INFO "current linked cmdbuf data %d =0x%x\n", i, *(cmdbuf_obj->cmdbuf_virtualAddress + i));
        }
      }
#endif
    } else
      break;

    ddr_buffer_sync(checked_vir_address, checked_bus_address, sizeof(u32), DDR_INTERLEAVE_SIZE);

    new_cmdbuf_node = new_cmdbuf_node->next;
  }
  sn_pri(vcmd_dev->tdev, SN_DBG, "vcmd link done last written value = %u\n", tmp);
  return;
}
static void vcmd_delink_cmdbuf(struct hantrovcmd_dev* dev, bi_list_node* last_linked_cmdbuf_node) {
  bi_list_node*      new_cmdbuf_node = NULL;
  struct cmdbuf_obj* cmdbuf_obj      = NULL;

  new_cmdbuf_node = last_linked_cmdbuf_node;
  while (1) {
    if (new_cmdbuf_node == NULL)
      break;
    cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
    if (cmdbuf_obj->cmdbuf_data_linked) {
      cmdbuf_obj->cmdbuf_data_linked = 0;
    } else
      break;
    new_cmdbuf_node = new_cmdbuf_node->next;
  }
  dev->sw_cmdbuf_rdy_num = 0;
}

static void vcmd_start(struct hantrovcmd_dev* dev, bi_list_node* first_linked_cmdbuf_node) {
  struct cmdbuf_obj* cmdbuf_obj = NULL;
  struct vcmd_dev*   vcmd_dev   = (struct vcmd_dev*) dev->vcmd_dev;

  if (dev->working_state == WORKING_STATE_IDLE) {
    if ((first_linked_cmdbuf_node != NULL) && dev->sw_cmdbuf_rdy_num) {
      cmdbuf_obj = (struct cmdbuf_obj*) first_linked_cmdbuf_node->data;
      //0x40
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_AXI_CLK_GATE_DISABLE, 0);
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_MASTER_OUT_CLK_GATE_DISABLE, 0); //this bit should be set 1 only when need to reset dec400
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_CORE_CLK_GATE_DISABLE, 0);
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_ABORT_MODE, 0);
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_RESET_CORE, 0);
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_RESET_ALL, 0);
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_START_TRIGGER, 0);
      //0x48
      if (dev->hw_version_id <= HW_ID_1_0_C)
        vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_INTCMD_EN, 0xffff);
      else {
        vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_JMPP_EN, 1);
        vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_JMPD_EN, 1);
      }
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_RESET_EN, 1);
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_ABORT_EN, 1);
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_CMDERR_EN, 1);
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_TIMEOUT_EN, 0);
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_BUSERR_EN, 1);
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_ENDCMD_EN, 1);
      //0x4c
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_TIMEOUT_EN, 1);
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_TIMEOUT_CYCLES, 0x1dcd6500);
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR, (u32) (cmdbuf_obj->cmdbuf_busAddress - vcmd_dev->base_ddr_addr));
      if (sizeof(size_t) == 8) {
        vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR_MSB, (u32) ((u64) (cmdbuf_obj->cmdbuf_busAddress - vcmd_dev->base_ddr_addr) >> 32));
      } else {
        vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR_MSB, 0);
      }
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_EXE_CMDBUF_LENGTH, (u32) ((cmdbuf_obj->cmdbuf_size + 7) / 8));
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_RDY_CMDBUF_COUNT, dev->sw_cmdbuf_rdy_num);
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_MAX_BURST_LEN, 0x10);
      if (dev->hw_version_id > HW_ID_1_0_C) {
        vcmd_write_register_value(dev->hwregs, dev->reg_mirror, HWIF_VCMD_CMDBUF_EXECUTING_ID, (u32) cmdbuf_obj->cmdbuf_id);
      }
      vcmd_write_reg(dev->hwregs, 0x40, dev->reg_mirror[0x40 / 4]);
      vcmd_write_reg(dev->hwregs, 0x44, vcmd_read_reg(dev->hwregs, 0x44));
      vcmd_write_reg(dev->hwregs, 0x48, dev->reg_mirror[0x48 / 4]);
      vcmd_write_reg(dev->hwregs, 0x4c, dev->reg_mirror[0x4c / 4]);
      vcmd_write_reg(dev->hwregs, 0x50, dev->reg_mirror[0x50 / 4]);
      vcmd_write_reg(dev->hwregs, 0x54, dev->reg_mirror[0x54 / 4]);
      vcmd_write_reg(dev->hwregs, 0x58, dev->reg_mirror[0x58 / 4]);
      vcmd_write_reg(dev->hwregs, 0x5c, dev->reg_mirror[0x5c / 4]);
      vcmd_write_reg(dev->hwregs, 0x60, dev->reg_mirror[0x60 / 4]);
      vcmd_write_reg(dev->hwregs, 0x64, 0xffffffff); //not interrupt cpu

      dev->working_state = WORKING_STATE_WORKING;
      //start
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_MASTER_OUT_CLK_GATE_DISABLE, 1); //this bit should be set 1 only when need to reset dec400
      vcmd_set_register_mirror_value(dev->reg_mirror, HWIF_VCMD_START_TRIGGER, 1);
      vcmd_write_reg(dev->hwregs, 0x40, dev->reg_mirror[0x40 / 4]);
    }
  }
}

static void create_read_all_registers_cmdbuf(struct vcmd_dev* vcmd_dev, struct exchange_parameter* input_para) {
  u32 register_range[] = {
      VCMD_ENCODER_REGISTER_SIZE, VCMD_IM_REGISTER_SIZE, VCMD_DECODER_REGISTER_SIZE, VCMD_JPEG_ENCODER_REGISTER_SIZE, VCMD_JPEG_DECODER_REGISTER_SIZE};
  u32   counter_cmdbuf_size  = 0;
  u32 __iomem*  set_base_addr = vcmd_dev->vcmd_buf_mem_pool.virtualAddress + input_para->cmdbuf_id * CMDBUF_MAX_SIZE / 4;
  ptr_t status_base_phy_addr = vcmd_dev->vcmd_status_buf_mem_pool.busAddress + input_para->cmdbuf_id * CMDBUF_MAX_SIZE +
      (vcmd_dev->vcmd_manager[input_para->module_type][input_para->core_id]->vcmd_core_cfg.submodule_main_addr / 2 + 0);
  u32 offset_inc        = 0;
  u32 offset_inc_dec400 = 0;
  if (vcmd_dev->vcmd_manager[input_para->module_type][input_para->core_id]->hw_version_id > HW_ID_1_0_C) {
    printk(KERN_INFO "vc8000_vcmd_driver:create cmdbuf data when hw_version_id = 0x%x\n",
        vcmd_dev->vcmd_manager[input_para->module_type][input_para->core_id]->hw_version_id);

    //read vcmd executing cmdbuf id registers to ddr for balancing core load.
    *(set_base_addr + 0) = (OPCODE_RREG) | (1 << 16) | (EXECUTING_CMDBUF_ID_ADDR * 4);
    counter_cmdbuf_size += 4;
    *(set_base_addr + 1) = (u32) 0; //will be changed in link stage
    counter_cmdbuf_size += 4;
    *(set_base_addr + 2) = (u32) 0; //will be changed in link stage
    counter_cmdbuf_size += 4;
    //alignment
    *(set_base_addr + 3) = 0;
    counter_cmdbuf_size += 4;

    //read main IP all registers
    *(set_base_addr + 4) = (OPCODE_RREG) | ((register_range[input_para->module_type] / 4) << 16) |
        (vcmd_dev->vcmd_manager[input_para->module_type][input_para->core_id]->vcmd_core_cfg.submodule_main_addr + 0);
    counter_cmdbuf_size += 4;
    *(set_base_addr + 5) = (u32) (status_base_phy_addr - vcmd_dev->base_ddr_addr);
    counter_cmdbuf_size += 4;
    if (sizeof(size_t) == 8) {
      *(set_base_addr + 6) = (u32) ((u64) (status_base_phy_addr - vcmd_dev->base_ddr_addr) >> 32);
    } else {
      *(set_base_addr + 6) = 0;
    }

    counter_cmdbuf_size += 4;
    //alignment
    *(set_base_addr + 7) = 0;
    counter_cmdbuf_size += 4;
    if (vcmd_dev->vcmd_manager[input_para->module_type][input_para->core_id]->vcmd_core_cfg.submodule_L2Cache_addr != 0xffff) {
      offset_inc           = 4;
      status_base_phy_addr = vcmd_dev->vcmd_status_buf_mem_pool.busAddress + input_para->cmdbuf_id * CMDBUF_MAX_SIZE +
          (vcmd_dev->vcmd_manager[input_para->module_type][input_para->core_id]->vcmd_core_cfg.submodule_L2Cache_addr / 2 + 0);
      //read L2cache IP first register
      *(set_base_addr + 8) = (OPCODE_RREG) | (1 << 16) | (vcmd_dev->vcmd_manager[input_para->module_type][input_para->core_id]->vcmd_core_cfg.submodule_L2Cache_addr + 0);
      counter_cmdbuf_size += 4;
      *(set_base_addr + 9) = (u32) (status_base_phy_addr - vcmd_dev->base_ddr_addr);
      counter_cmdbuf_size += 4;
      if (sizeof(size_t) == 8) {
        *(set_base_addr + 10) = (u32) ((u64) (status_base_phy_addr - vcmd_dev->base_ddr_addr) >> 32);
      } else {
        *(set_base_addr + 10) = 0;
      }

      counter_cmdbuf_size += 4;
      //alignment
      *(set_base_addr + 11) = 0;
      counter_cmdbuf_size += 4;
    }
    if (vcmd_dev->vcmd_manager[input_para->module_type][input_para->core_id]->vcmd_core_cfg.submodule_dec400_addr != 0xffff) {
      //read dec400 register
      offset_inc_dec400    = 4;
      status_base_phy_addr = vcmd_dev->vcmd_status_buf_mem_pool.busAddress + input_para->cmdbuf_id * CMDBUF_MAX_SIZE +
          (vcmd_dev->vcmd_manager[input_para->module_type][input_para->core_id]->vcmd_core_cfg.submodule_dec400_addr / 2 + 0);
      //read DEC400 IP first register
      *(set_base_addr + 8 + offset_inc) = (OPCODE_RREG) | (0x2b << 16) |
          (vcmd_dev->vcmd_manager[input_para->module_type][input_para->core_id]->vcmd_core_cfg.submodule_dec400_addr + 0);
      counter_cmdbuf_size += 4;

      *(set_base_addr + 9 + offset_inc) = (u32) (status_base_phy_addr - vcmd_dev->base_ddr_addr);

      counter_cmdbuf_size += 4;
      if (sizeof(size_t) == 8) {
        *(set_base_addr + 10 + offset_inc) = (u32) ((u64) (status_base_phy_addr - vcmd_dev->base_ddr_addr) >> 32);
      } else {
        *(set_base_addr + 10 + offset_inc) = 0;
      }

      counter_cmdbuf_size += 4;
      //alignment
      *(set_base_addr + 11 + offset_inc) = 0;
      counter_cmdbuf_size += 4;
    }

    //read vcmd registers to ddr
    *(set_base_addr + 8 + offset_inc + offset_inc_dec400) = (OPCODE_RREG) | (27 << 16) | (0);
    counter_cmdbuf_size += 4;
    *(set_base_addr + 9 + offset_inc + offset_inc_dec400) = (u32) 0; //will be changed in link stage
    counter_cmdbuf_size += 4;
    *(set_base_addr + 10 + offset_inc + offset_inc_dec400) = (u32) 0; //will be changed in link stage
    counter_cmdbuf_size += 4;
    //alignment
    *(set_base_addr + 11 + offset_inc + offset_inc_dec400) = 0;
    counter_cmdbuf_size += 4;
    //JMP RDY = 0
    *(set_base_addr + 12 + offset_inc + offset_inc_dec400) = (OPCODE_JMP_RDY0) | 0 | JMP_IE_1 | 0;
    counter_cmdbuf_size += 4;
    *(set_base_addr + 13 + offset_inc + offset_inc_dec400) = 0;
    counter_cmdbuf_size += 4;
    *(set_base_addr + 14 + offset_inc + offset_inc_dec400) = 0;
    counter_cmdbuf_size += 4;
    *(set_base_addr + 15 + offset_inc + offset_inc_dec400) = input_para->cmdbuf_id;
    //don't add the last alignment DWORD in order to  identify END command or JMP command.
    //counter_cmdbuf_size += 4;
    input_para->cmdbuf_size = (16 + offset_inc + offset_inc_dec400) * 4;
  } else {
    printk(KERN_INFO "vc8000_vcmd_driver:create cmdbuf data when hw_version_id = 0x%x\n",
        vcmd_dev->vcmd_manager[input_para->module_type][input_para->core_id]->hw_version_id);
    //read all registers
    *(set_base_addr + 0) = (OPCODE_RREG) | ((register_range[input_para->module_type] / 4) << 16) |
        (vcmd_dev->vcmd_manager[input_para->module_type][input_para->core_id]->vcmd_core_cfg.submodule_main_addr + 0);
    counter_cmdbuf_size += 4;

    *(set_base_addr + 1) = (u32) (status_base_phy_addr - vcmd_dev->base_ddr_addr);

    counter_cmdbuf_size += 4;
    if (sizeof(size_t) == 8) {
      *(set_base_addr + 2) = (u32) ((u64) (status_base_phy_addr - vcmd_dev->base_ddr_addr) >> 32);
    } else {
      *(set_base_addr + 2) = 0;
    }

    counter_cmdbuf_size += 4;
    //alignment
    *(set_base_addr + 3) = 0;
    counter_cmdbuf_size += 4;

    //JMP RDY = 0
    *(set_base_addr + 4) = (OPCODE_JMP_RDY0) | 0 | JMP_IE_1 | 0;
    counter_cmdbuf_size += 4;
    *(set_base_addr + 5) = 0;
    counter_cmdbuf_size += 4;
    *(set_base_addr + 6) = 0;
    counter_cmdbuf_size += 4;
    *(set_base_addr + 7) = input_para->cmdbuf_id;
    //don't add the last alignment DWORD in order to  identify END command or JMP command.
    //counter_cmdbuf_size += 4;
    input_para->cmdbuf_size = 8 * 4;
  }
}
static void read_main_module_all_registers(struct sn_tranx_t* tdev, u32 main_module_type) {
  int                       ret;
  struct exchange_parameter input_para;
  u32                       irq_status_ret = 0;
  u32 __iomem*              status_base_virt_addr;
  struct vcmd_dev*          vcmd_dev = NULL;
  int                       i, type_core_num;

  vcmd_dev      = tdev->modules[SN_MODULE_VCMD];
  type_core_num = vcmd_dev->vcmd_type_core_num[main_module_type];
  for (i = 0; i < type_core_num; i++) {
    input_para.executing_time                                     = 0;
    input_para.priority                                           = CMDBUF_PRIORITY_NORMAL;
    input_para.module_type                                        = main_module_type;
    input_para.cmdbuf_size                                        = 0;
    input_para.core_id                                            = i;
    ret                                                           = reserve_cmdbuf(NULL, tdev, &input_para);
    vcmd_dev->vcmd_manager[main_module_type][i]->status_cmdbuf_id = input_para.cmdbuf_id;
    create_read_all_registers_cmdbuf(vcmd_dev, &input_para);
    link_and_run_cmdbuf(NULL, tdev, &input_para, NULL, NULL, NULL);
    wait_cmdbuf_ready(NULL, vcmd_dev, input_para.cmdbuf_id, &irq_status_ret);
    status_base_virt_addr = vcmd_dev->vcmd_status_buf_mem_pool.virtualAddress + input_para.cmdbuf_id * CMDBUF_MAX_SIZE / 4 +
        (vcmd_dev->vcmd_manager[input_para.module_type][i]->vcmd_core_cfg.submodule_main_addr / 2 / 4 + 0);
  }
}

static struct vcmd_config* get_total_subsys_num(struct sn_tranx_t* tdev, u32* total_subsys_num) {
  struct vcmd_dev*    vcmd_dev           = tdev->modules[SN_MODULE_VCMD];
  struct vcmd_config* ret_subsys_pointer = NULL;
  u8                  pf_vf_mode         = tdev->pf_vf_mode;
  u8                  i;
  switch (pf_vf_mode) {
  case PF_MODE:
    if (tdev->vf_index == PF_INDEX) {
      *total_subsys_num  = sizeof(pf_vcmd_core_array) / sizeof(pf_vcmd_core_array[0]);
      ret_subsys_pointer = pf_vcmd_core_array;
    } else {
      sn_pri(tdev, SN_ERR, "%s,vf_index incorrect in PF_MODE\n", __func__);
    }
    break;
  case TWO_VF_MODE:
    if (tdev->vf_index == VF2_INDEX) {
      *total_subsys_num  = sizeof(vf2_vcmd_core_array) / sizeof(vf2_vcmd_core_array[0]);
      ret_subsys_pointer = vf2_vcmd_core_array;
    } else if (tdev->vf_index == VF1_INDEX) {
      *total_subsys_num  = sizeof(vf1_vcmd_core_array) / sizeof(vf1_vcmd_core_array[0]);
      ret_subsys_pointer = vf1_vcmd_core_array;
    } else {
      sn_pri(tdev, SN_ERR, "%s,vf_index incorrect in TWO_VF_MODE\n", __func__);
    }
    break;
  case ONE_VF_MODE:
    if (tdev->vf_index == VF1_INDEX) {
      *total_subsys_num  = sizeof(vf_vcmd_core_array) / sizeof(vf_vcmd_core_array[0]);
      ret_subsys_pointer = vf_vcmd_core_array;
    } else {
      sn_pri(tdev, SN_ERR, "%s,vf_index incorrect in ONE_VF_MODE\n", __func__);
    }
    break;
  default:
    sn_pri(tdev, SN_ERR, "%s,vf_max_count incorrect\n", __func__);
    break;
  }
  if (ret_subsys_pointer) {
    for (i = 0; i < *total_subsys_num; i++) {
      if (ret_subsys_pointer[i].sub_module_type == VCMD_TYPE_ENCODER || ret_subsys_pointer[i].sub_module_type == VCMD_TYPE_CUTREE) {
        vcmd_dev->total_vcmd_vc8000e_core_num++;
      } else if (ret_subsys_pointer[i].sub_module_type == VCMD_TYPE_DECODER) {
        vcmd_dev->total_vcmd_vc8000d_core_num++;
      }
    }
    sn_pri(tdev, SN_INF, "vcmd vc8000e core num %d, vc8000d core num %d\n", vcmd_dev->total_vcmd_vc8000e_core_num, vcmd_dev->total_vcmd_vc8000d_core_num);
  }
  return ret_subsys_pointer;
}

static void osal_decoder_close(struct sn_tranx_t* tdev, struct file* filp){
  return;
}

#define WORDCOUNT(X) (((X) + sizeof(uint32_t) - 1) / sizeof(uint32_t))

static void osal_decoder_handler(struct work_struct* workPtr) {
  sn_osal_work*      work     = (sn_osal_work*) workPtr;
  osal_command*      command  = &work->cmd;
  struct file*       filp     = NULL;
  int                ret;
  struct sn_tranx_t* tdev = work->tdev;
  int                i;
  struct vcmd_dev*   vcmd_dev = tdev->modules[SN_MODULE_VCMD];
  filp                        = work->filp;
  if (filp == NULL) {
    sn_pri(tdev, SN_ERR, "osal_decoder_handler, filp is NULL\n");
    return;
  }
    sn_pri(tdev, SN_DBG, "vcmd_osal_handler(work_queue based): recvd cmd %d\n", command->op_code);
    switch (command->op_code) {
    case OSAL_ACCL_CMD_VCMD_GET_NUMCORES: {
      u16 dec_path = work->data[1];
      sn_pri(tdev, SN_DBG, "OSAL_ACCL_CMD_VCMD_GET_NUMCORES\n");
      if (work->numResp < 1) {
        work->numResp = -1;
        break;
      }
      work->data[0] = decpath_array[dec_path].max_core_num;
      work->numResp = 1;
      break;
    }

    case OSAL_ACCL_CMD_VCMD_GET_HWIOSIZE: {
      struct regsize_desc regsize_desc;
      u16                 module_type = work->data[0];
      sn_pri(tdev, SN_DBG, "OSAL_ACCL_CMD_VCMD_GET_HWIOSIZE\n");
      if (work->numResp < 2) {
        work->numResp = -1;
        break;
      }
      work->data[0] = 0;
      if (module_type == 2) {
        regsize_desc.id   = work->data[1];
        regsize_desc.type = work->data[2];
        ret               = vc8000d_get_hw_iosize(tdev->modules[SN_MODULE_VC8000D], &regsize_desc);
        if (ret == 0)
          work->data[0] = regsize_desc.size;
      } else {
        ret = 0;
      }
      work->data[1] = ret; //return value
      work->numResp = 2;
      break;
    }

    case OSAL_ACCL_CMD_VCMD_GET_CONFIGINFO: {
      u16                    module_type   = work->data[0];
      u16                    dec_path      = work->data[1];
      struct hantrovcmd_dev* vcmd        = vcmd_dev->vcmd_manager[module_type][decpath_array[dec_path].start_core_id];
      unsigned numWords = WORDCOUNT(sizeof(struct config_parameter)) + 2;
      struct config_parameter* config = (struct config_parameter*) (work->data + 2);
      sn_pri(tdev, SN_DBG, "OSAL_ACCL_CMD_VCMD_GET_CONFIGINFO\n");
      if (work->numResp < numWords) {
        work->numResp = -1;
        break;
      }
      work->numResp = numWords;

      if (vcmd_dev->vcmd_type_core_num[module_type]) {
        config->submodule_main_addr     = vcmd->vcmd_core_cfg.submodule_main_addr;
        config->submodule_dec400_addr   = vcmd->vcmd_core_cfg.submodule_dec400_addr;
        config->submodule_L2Cache_addr  = vcmd->vcmd_core_cfg.submodule_L2Cache_addr;
        config->submodule_MMU_addr[0]   = vcmd->vcmd_core_cfg.submodule_MMU_addr[0];
        config->submodule_MMU_addr[1]   = vcmd->vcmd_core_cfg.submodule_MMU_addr[1];
        config->submodule_axife_addr[0] = vcmd->vcmd_core_cfg.submodule_axife_addr[0];
        config->submodule_axife_addr[1] = vcmd->vcmd_core_cfg.submodule_axife_addr[1];
        config->config_status_cmdbuf_id = vcmd->status_cmdbuf_id;
        config->vcmd_hw_version_id      = vcmd->hw_version_id;
        config->vcmd_core_num           = decpath_array[dec_path].max_core_num;
      } else {
        config->submodule_main_addr                  = 0xffff;
        config->submodule_dec400_addr                = 0xffff;
        config->submodule_L2Cache_addr               = 0xffff;
        config->submodule_MMU_addr[0]                = 0xffff;
        config->submodule_MMU_addr[1]                = 0xffff;
        config->submodule_axife_addr[0]              = 0xffff;
        config->submodule_axife_addr[1]              = 0xffff;
        config->config_status_cmdbuf_id              = 0;
        config->vcmd_hw_version_id                   = HW_ID_1_0_C;
        config->vcmd_core_num                        = decpath_array[dec_path].max_core_num;
      }
      work->data[0] = 0; //return value
      break;
    }

    case OSAL_ACCL_CMD_VCMD_GET_BUFINFO: {
      unsigned numWords = WORDCOUNT(sizeof(struct cmdbuf_mem_parameter)) + 1;
      struct cmdbuf_mem_parameter* params = (struct cmdbuf_mem_parameter*) (work->data + 1);
      sn_pri(tdev, SN_DBG, "OSAL_ACCL_CMD_VCMD_GET_BUFINFO\n");
      if (work->numResp < numWords) {
        work->numResp = -1;
        break;
      }
      work->numResp = numWords;

      params->cmdbuf_unit_size           = CMDBUF_MAX_SIZE;
      params->status_cmdbuf_unit_size    = CMDBUF_MAX_SIZE;
      params->cmdbuf_total_size          = CMDBUF_POOL_TOTAL_SIZE;
      params->status_cmdbuf_total_size   = CMDBUF_POOL_TOTAL_SIZE;
      params->cmdbuf_patch_total_size    = CMDBUF_POOL_TOTAL_SIZE;
      work->data[0] = 0; //return value
      break;
    }

    case OSAL_ACCL_CMD_VCMD_GET_REG_INFO: {
      u16 module_type = work->data[0];
      u16 reg_cnt = work->data[1];
			u16 dec_path = work->data[2];
      struct hantrovcmd_dev *vcmd = vcmd_dev->vcmd_manager[module_type][decpath_array[dec_path].start_core_id];
      u32 *status_va;
      u16 offset;
      if (work->numResp < reg_cnt + 1) {
        work->numResp = -1;
        break;
      }

      status_va = vcmd_dev->vcmd_status_buf_mem_pool.virtualAddress + vcmd->status_cmdbuf_id*CMDBUF_MAX_SIZE/4 + vcmd->vcmd_core_cfg.submodule_main_addr/2/4;

      work->data[0] = 1;

      for (i = 0; i < reg_cnt; i++) {
        offset = work->data[3 + i];
        work->data[i + 1] = *(status_va + offset);
      }

      work->numResp = reg_cnt + 1;
      break;
    }


    case OSAL_ACCL_CMD_VCMD_CMDBUF_PROCESS: {
      unsigned numWords = WORDCOUNT(sizeof(struct exchange_parameter));
      struct exchange_parameter* input_para = (struct exchange_parameter*) work->data;
      u32                       cmd_buf_used = input_para->cmdbuf_size;
      u32                       *cmd_buf = work->data + numWords + 4;
      u32                       *patch_buf = cmd_buf + work->data[numWords + 2];
      u32                       *ppu_buf = patch_buf + work->data[numWords + 3];
      u32                       irq_status_ret = 0;
      u64                       handle;
      u16                       core_id = input_para->core_id;
      ktime_t                   tv_s;
      ktime_t                   tv_e;
      tv_s = ktime_get();

      sn_pri(tdev, SN_DBG, "OSAL_ACCL_CMD_VCMD_CMDBUF_PROCESS\n");

      if (work->numResp < 8) {
        work->numResp = -1;
        break;
      }
      // receive handle to get the task_id
      handle = (uint64_t) work->data[numWords] << 32;
      handle |= work->data[numWords + 1];

      ret                       = reserve_cmdbuf(filp, tdev, input_para);
      work->data[0] = ret;
      if (ret) {
        sn_pri(tdev, SN_ERR, "reserve_cmdbuf failed! ret=%x\n", ret);
        work->numResp = 1;
        break;
      }

      input_para->cmdbuf_size = cmd_buf_used;
      if (down_interruptible(&vcmd_dev->vcmd_reserve_resource)) {
        sn_pri(tdev, SN_ERR, "link_and_run_cmdbuf: down interruptible failed!\n");
        work->data[0]  = -ERESTARTSYS;
        work->numResp = 1;
        goto release_cmdbuf;
      }
      ret = link_and_run_cmdbuf(filp, tdev, input_para, cmd_buf, patch_buf, ppu_buf);
      core_id = input_para->core_id;
      if (ret) {
        up(&vcmd_dev->vcmd_reserve_resource);
        sn_pri(tdev, SN_ERR, "link_and_run_cmdbuf failed! ret=%x\n", ret);
        work->data[0] = ret;
        work->numResp = 1;
        goto release_cmdbuf;
      }

      ret = wait_cmdbuf_ready(filp, vcmd_dev, input_para->cmdbuf_id, &irq_status_ret);
      up(&vcmd_dev->vcmd_reserve_resource);
      if (ret == 0) {
        u32                task_id;
        u32 __iomem*       status_base_virt_addr;
        u32                strm_addr_lsb;
        u64                strm_addr;
        struct cmdbuf_obj* cmdbuf_obj      = NULL;
        bi_list_node*      new_cmdbuf_node = NULL;
        u16 start_core_id = decpath_array[input_para->dec_path].start_core_id;

        new_cmdbuf_node       = vcmd_dev->global_cmdbuf_node[input_para->cmdbuf_id];
        cmdbuf_obj            = (struct cmdbuf_obj*) new_cmdbuf_node->data;
        status_base_virt_addr = cmdbuf_obj->status_virtualAddress + (vcmd_dev->vcmd_manager[2][start_core_id]->vcmd_core_cfg.submodule_main_addr / 2 / 4 + 0);

        task_id           = sn_mem_osal_task_from_handle(handle);
        strm_addr_lsb     = *(status_base_virt_addr + 3);
        strm_addr         = *(status_base_virt_addr + 2);
        strm_addr         = (strm_addr << 32) | strm_addr_lsb;
        handle            = sn_mem_osal_translate_mem(tdev, task_id, strm_addr);
        if (handle != 1) {
          work->data[1] = *(status_base_virt_addr + 1);
          work->data[2] = (handle >> 32) & 0xffffffff; //*(status_base_virt_addr+2);
          work->data[3] = handle & 0xffffffff;         //*(status_base_virt_addr+3);
          work->data[4] = *(status_base_virt_addr + 4);
          work->data[5] = *(status_base_virt_addr + 5);
          work->data[6] = cmdbuf_obj->core_id;
          work->data[0] = 0; //return value
          work->numResp = 8;
        } else {
          work->numResp = 1;
          work->data[0] = 1;
        }
      } else {
          work->numResp = 1;
          work->data[0] = 1;
      }

release_cmdbuf:
      ret               = release_cmdbuf(filp, tdev, input_para->cmdbuf_id);
      if (ret != 0) {
        sn_pri(tdev, SN_ERR, "release_cmdbuf failed! ret=%x\n", ret);
      }

      tv_e = ktime_get();
      vcmd_dev->hantrovcmd_data->loading[core_id].time_cnt += ktime_to_us(ktime_sub(tv_e, tv_s));

      break;
    }

    case OSAL_ACCL_CMD_VCMD_CMDBUF_RESERVE: {
      struct exchange_parameter* input_para = (struct exchange_parameter*) work->data;

      sn_pri(tdev, SN_DBG, "OSAL_ACCL_CMD_VCMD_CMDBUF_RESERVE\n");
      if (work->numResp < 2) {
        work->numResp = -1;
        break;
      }

      work->numResp = 2;

      ret                       = reserve_cmdbuf(filp, tdev, input_para);
      if (ret) {
        sn_pri(tdev, SN_ERR, "OSAL_ACCL_CMD_VCMD_CMDBUF_RESERVE failed! ret=%x\n", ret);
      }
      work->data[0] = ret; //return value
      work->data[1] = input_para->cmdbuf_id;
      break;
    }

    case OSAL_ACCL_CMD_VCMD_CMDBUF_LINK_RUN: {
      // struct exchange_parameter
      // +0: 64 bit handle HI/LO
      // +2: num cmd words
      // +3: num patch words
      // +4: cmd buffer
      // patch buffer
      // ppu buffer
      unsigned numWords = WORDCOUNT(sizeof(struct exchange_parameter));
      struct exchange_parameter* input_para = (struct exchange_parameter*) work->data;
      u32                       *cmd_buf = work->data + numWords + 4;
      u32                       *patch_buf = cmd_buf + work->data[numWords + 2];
      u32                       *ppu_buf = patch_buf + work->data[numWords + 3];
      u32                       irq_status_ret = 0;
      u64                       handle;
      u16                       core_id = input_para->core_id;
      ktime_t                   tv_s;
      ktime_t                   tv_e;
      tv_s = ktime_get();

      sn_pri(tdev, SN_DBG, "OSAL_ACCL_CMD_VCMD_CMDBUF_LINK_RUN\n");

      if (work->numResp < 8) {
        work->numResp = -1;
        break;
      }

      // receive handle to get the task_id
      handle = (uint64_t) work->data[numWords] << 32;
      handle |= work->data[numWords + 1];

      if (down_interruptible(&vcmd_dev->vcmd_reserve_resource)) {
        sn_pri(tdev, SN_ERR, "OSAL_ACCL_CMD_VCMD_CMDBUF_LINK_RUN failed!\n");
        work->data[0]  = -ERESTARTSYS;
        work->numResp = 1;
        break;
      }
      ret = link_and_run_cmdbuf(filp, tdev, input_para, cmd_buf, patch_buf, ppu_buf);
      core_id = input_para->core_id;
      if (ret) {
        up(&vcmd_dev->vcmd_reserve_resource);
        sn_pri(tdev, SN_ERR, "OSAL_ACCL_CMD_VCMD_CMDBUF_LINK_RUN failed! ret=%x\n", ret);
        work->data[0] = ret;
        work->numResp = 1;
        break;
      }

      ret = wait_cmdbuf_ready(filp, vcmd_dev, input_para->cmdbuf_id, &irq_status_ret);
      up(&vcmd_dev->vcmd_reserve_resource);
      if (ret == 0) {
        u32                task_id;
        u32 __iomem*       status_base_virt_addr;
        u32                strm_addr_lsb;
        u64                strm_addr;
        struct cmdbuf_obj* cmdbuf_obj      = NULL;
        bi_list_node*      new_cmdbuf_node = NULL;
        u16 start_core_id = decpath_array[input_para->dec_path].start_core_id;
        work->data[1] = input_para->cmdbuf_id;

        new_cmdbuf_node       = vcmd_dev->global_cmdbuf_node[input_para->cmdbuf_id];
        cmdbuf_obj            = (struct cmdbuf_obj*) new_cmdbuf_node->data;
        status_base_virt_addr = cmdbuf_obj->status_virtualAddress + (vcmd_dev->vcmd_manager[2][start_core_id]->vcmd_core_cfg.submodule_main_addr / 2 / 4 + 0);

        task_id           = sn_mem_osal_task_from_handle(handle);
        strm_addr_lsb     = *(status_base_virt_addr + 3);
        strm_addr         = *(status_base_virt_addr + 2);
        strm_addr         = (strm_addr << 32) | strm_addr_lsb;
        handle            = sn_mem_osal_translate_mem(tdev, task_id, strm_addr);
        if (handle != 1) {
          work->data[2] = *(status_base_virt_addr + 1);
          work->data[3] = (handle >> 32) & 0xffffffff; //*(status_base_virt_addr+2);
          work->data[4] = handle & 0xffffffff;         //*(status_base_virt_addr+3);
          work->data[5] = *(status_base_virt_addr + 4);
          work->data[6] = *(status_base_virt_addr + 5);
          work->data[7] = cmdbuf_obj->core_id;
          work->data[0] = 0; //return value
          work->numResp = 8;
        } else {
          work->numResp = 1;
          work->data[0] = 1;
        }
      } else {
          work->numResp = 1;
          work->data[0] = 1;
      }

      tv_e = ktime_get();
      vcmd_dev->hantrovcmd_data->loading[core_id].time_cnt += ktime_to_us(ktime_sub(tv_e, tv_s));

      break;
    }

    case OSAL_ACCL_CMD_VCMD_CMDBUF_RELEASE: {
      u16 cmdbuf_id;

      sn_pri(tdev, SN_DBG, "OSAL_ACCL_CMD_VCMD_CMDBUF_RELEASE\n");
      if (work->numResp < 1) {
        work->numResp = -1;
        break;
      }
      work->numResp = 1;
      cmdbuf_id     = work->data[0];
      sn_pri(tdev, SN_DBG, "release_cmdbuf\n");
      ret               = release_cmdbuf(filp, tdev, cmdbuf_id);
      work->data[0] = ret;
      if (ret != 0) {
        sn_pri(tdev, SN_ERR, "release_cmdbuf failed! ret=%x\n", ret);
        work->data[0] = 0;
      }
      break;
    }

    case OSAL_ACCL_CMD_VCMD_CMDBUF_POLLING: {
      u16 core_id;

      sn_pri(tdev, SN_DBG, "OSAL_ACCL_CMD_VCMD_CMDBUF_POLLING\n");
      if (work->numResp < 1) {
        work->numResp = -1;
        break;
      }
      work->numResp = 1;
      core_id       = work->data[0];
      if (core_id >= vcmd_dev->total_vcmd_core_num) {
        sn_pri(tdev, SN_ERR, "core_id is not valid=%x\n", core_id);
        work->data[0] = 1;
        break;
      }
      hantrovcmd_isr(core_id, &vcmd_dev->hantrovcmd_data[core_id]);
      work->data[0] = 0; //return value
      break;
    }

    default:
      sn_pri(tdev, SN_ERR, "vcmd_osal_handler: invalid cmd opcode %d\n", command->op_code);
      if (work->numResp < 1) {
        work->numResp = -1;
        break;
      }
      work->numResp     = 1;
      work->data[0] = 0xdeadbeef;
      break;
  }
  sn_osal_finish_work(work);
}

static int get_vcmd_index_by_slice_subsys(struct sn_tranx_t* tdev, int slice_index, u32 sub_module_type, u32 isIM, u32* index) {
  int                 i               = 0;
  struct vcmd_dev*    vcmd_dev        = NULL;
  struct vcmd_config* vcmd_core_array = NULL;

  vcmd_dev        = tdev->modules[SN_MODULE_VCMD];
  vcmd_core_array = vcmd_dev->vcmd_active_core_array;

  if (vcmd_core_array == NULL) {
    sn_pri(tdev, SN_ERR, "%s,vcmd_core_array is NULL\n", __func__);
    return -1;
  }

  for (i = 0; i < vcmd_dev->total_vcmd_core_num; i++) {
    if (((vcmd_core_array + i)->slice == slice_index) && ((vcmd_core_array + i)->subsys == sub_module_type)) {
      break;
    }
  }

  if (i == vcmd_dev->total_vcmd_core_num) {
    sn_pri(tdev, SN_ERR, "vc8000_vcmd_driver: vcmd submodule not found!\n");
    return -1;
  }

  if (isIM)
    *index = (i + 1);
  else
    *index = i;

  return 0;
}

extern int hantrovcmd_init_ex(struct sn_tranx_t* tdev, int slice_index, u32 sub_module_type, u32 isIM);

int hantrovcmd_init_ex(struct sn_tranx_t* tdev, int slice_index, u32 sub_module_type, u32 isIM) {
  int                    result;
  u32                    index           = -1;
  struct vcmd_dev*       vcmd_dev        = NULL;
  struct hantrovcmd_dev* hantrovcmd_data = NULL;
  struct vcmd_config*    vcmd_core_array = NULL;

  vcmd_dev = tdev->modules[SN_MODULE_VCMD];
  if (vcmd_dev == NULL) {
    sn_pri(tdev, SN_INF, "vc8000_vcmd_driver: vcmd_dev NULL!\n");
    return -1;
  }

  hantrovcmd_data = vcmd_dev->hantrovcmd_data;

  result = get_vcmd_index_by_slice_subsys(tdev, slice_index, sub_module_type, isIM, (u32*) &index);
  if (result < 0)
    return -1;

  sn_pri(tdev, SN_INF, "vc8000_vcmd_driver: index %d\n", index);

  vcmd_core_array = vcmd_dev->vcmd_active_core_array;

  sn_pri(tdev, SN_DBG, "vcmd_name: %s\n", (vcmd_core_array + index)->vcmd_name);
  sn_pri(tdev, SN_DBG, "vcmd_base_addr: 0x%lx\n", (vcmd_core_array + index)->vcmd_base_addr);
  sn_pri(tdev, SN_DBG, "vcmd_iosize: 0x%x\n", (vcmd_core_array + index)->vcmd_iosize);
  sn_pri(tdev, SN_DBG, "vcmd_irq: 0x%x\n", (vcmd_core_array + index)->vcmd_irq);
  sn_pri(tdev, SN_DBG, "sub_module_type: 0x%x\n", (vcmd_core_array + index)->sub_module_type);
  sn_pri(tdev, SN_DBG, "submodule_main_addr: 0x%x\n", (vcmd_core_array + index)->submodule_main_addr);
  sn_pri(tdev, SN_DBG, "submodule_dec400_addr: 0x%x\n", (vcmd_core_array + index)->submodule_dec400_addr);
  sn_pri(tdev, SN_DBG, "submodule_L2Cache_addr: 0x%x\n", (vcmd_core_array + index)->submodule_L2Cache_addr);
  sn_pri(tdev, SN_DBG, "submodule_MMU_addr: 0x%x\n\n", (vcmd_core_array + index)->submodule_MMU_addr[0]);

  hantrovcmd_data[index].working_state     = WORKING_STATE_IDLE;
  hantrovcmd_data[index].sw_cmdbuf_rdy_num = 1;

  vcmd_reset_asic_ex(hantrovcmd_data, index);

  /*read all registers for each type of module for analyzing configuration in cwl*/
  sn_pri(tdev, SN_INF, "vc8000_vcmd: vcmd_core_array[%d].sub_module_type = %d\n", index, (vcmd_core_array + index)->sub_module_type);

  sn_pri(tdev, SN_INF, "vc8000_vcmd_driver: module initialization done\n");
  return 0;
}

int hantrovcmd_init(struct sn_tranx_t* tdev) {
  int                    i, k;
  int                    result;
  u32                    core_type, core_index;
  struct vcmd_config*    vcmd_core_array = NULL;
  struct vcmd_dev*       vcmd_dev        = NULL;
  struct hantrovcmd_dev* hantrovcmd_data = NULL;

  vcmd_dev = kzalloc(sizeof(struct vcmd_dev), GFP_KERNEL);
  if (vcmd_dev == NULL)
    goto err;
  tdev->modules[SN_MODULE_VCMD] = vcmd_dev;
  vcmd_dev->tdev                = tdev;

  vcmd_core_array = get_total_subsys_num(tdev, &(vcmd_dev->total_vcmd_core_num));
  if (vcmd_core_array == NULL) {
    sn_pri(tdev, SN_ERR, "%s,vcmd_core_array is NULL\n", __func__);
    goto err;
  }
  vcmd_dev->vcmd_active_core_array = kzalloc(sizeof(struct vcmd_config) * vcmd_dev->total_vcmd_core_num, GFP_KERNEL);
  if (vcmd_dev->vcmd_active_core_array == NULL)
    goto err;
  memcpy(vcmd_dev->vcmd_active_core_array, vcmd_core_array, sizeof(struct vcmd_config) * vcmd_dev->total_vcmd_core_num);

  vcmd_core_array = vcmd_dev->vcmd_active_core_array;
  for (i = 0; i < vcmd_dev->total_vcmd_core_num; i++) {
    (vcmd_core_array + i)->subsys = get_subsys_config_index((vcmd_core_array + i)->vcmd_base_addr);
    switch ((vcmd_core_array + i)->subsys) {
    case SYS_CTL_VCDA:
    case SYS_CTL_VCDB:
      break;
    default:
      break;
    }
  }
  result = vcmd_pcie_init(tdev);
  if (result)
    goto err;

  for (i = 0; i < vcmd_dev->total_vcmd_core_num; i++) {
    if ((vcmd_core_array + i)->vcmd_irq != -1)
      (vcmd_core_array + i)->vcmd_irq = tdev->msix_entries[(vcmd_core_array + i)->vcmd_irq].vector;
  }

  hantrovcmd_data = kzalloc(sizeof(struct hantrovcmd_dev) * vcmd_dev->total_vcmd_core_num, GFP_KERNEL);
  if (hantrovcmd_data == NULL)
    goto err1;
  vcmd_dev->hantrovcmd_data = hantrovcmd_data;

  for (k = 0; k < MAX_VCMD_TYPE; k++) {
    vcmd_dev->vcmd_type_core_num[k] = 0;
    vcmd_dev->vcmd_position[k]      = 0;
    for (i = 0; i < MAX_VCMD_NUMBER; i++)
      vcmd_dev->vcmd_manager[k][i] = NULL;
  }

  init_bi_list(&vcmd_dev->global_process_manager);

  for (i = 0; i < vcmd_dev->total_vcmd_core_num; i++) {
    if (!slice_accessible(tdev, (vcmd_core_array + i)->slice))
      continue;
    core_type  = (vcmd_core_array + i)->sub_module_type;
    core_index = vcmd_dev->vcmd_type_core_num[core_type];
    memcpy(&hantrovcmd_data[i].vcmd_core_cfg, (vcmd_core_array + i), sizeof(struct vcmd_config));
    hantrovcmd_data[i].hwregs                     = NULL;
    hantrovcmd_data[i].core_id                    = i;
    hantrovcmd_data[i].working_state              = WORKING_STATE_IDLE;
    hantrovcmd_data[i].sw_cmdbuf_rdy_num          = 0;
    hantrovcmd_data[i].spinlock                   = &vcmd_dev->owner_lock_vcmd[i];
    hantrovcmd_data[i].wait_queue                 = &vcmd_dev->wait_queue_vcmd[i];
    hantrovcmd_data[i].wait_abort_queue           = &vcmd_dev->abort_queue_vcmd[i];
    hantrovcmd_data[i].duration_without_int       = 0;
    vcmd_dev->vcmd_manager[core_type][core_index] = &hantrovcmd_data[i];
    vcmd_dev->vcmd_type_core_num[core_type]++;
    hantrovcmd_data[i].vcmd_reg_mem_busAddress     = vcmd_dev->vcmd_registers_mem_pool.busAddress + i * VCMD_REGISTER_SIZE - vcmd_dev->base_ddr_addr;
    hantrovcmd_data[i].vcmd_reg_mem_virtualAddress = vcmd_dev->vcmd_registers_mem_pool.virtualAddress + i * VCMD_REGISTER_SIZE / 4;
    hantrovcmd_data[i].vcmd_reg_mem_size           = VCMD_REGISTER_SIZE;

    spin_lock_init(&vcmd_dev->owner_lock_vcmd[i]);
    init_waitqueue_head(&vcmd_dev->wait_queue_vcmd[i]);
    init_waitqueue_head(&vcmd_dev->abort_queue_vcmd[i]);
    init_bi_list(&hantrovcmd_data[i].list_manager);

    memset_io(hantrovcmd_data[i].vcmd_reg_mem_virtualAddress, 0, VCMD_REGISTER_SIZE);
  }

  init_waitqueue_head(&vcmd_dev->mc_wait_queue);
  init_waitqueue_head(&vcmd_dev->vcmd_cmdbuf_memory_wait);
  spin_lock_init(&vcmd_dev->vcmd_cmdbuf_alloc_lock);
  spin_lock_init(&vcmd_dev->vcmd_process_manager_lock);

  result = vcmd_reserve_IO(tdev);
  if (result < 0)
    goto err2;

  for (i = 0; i < vcmd_dev->total_vcmd_core_num; i++) {
    sn_pri(tdev, SN_INF, "vcmd_base_addr: 0x%lx\n", (vcmd_core_array + i)->vcmd_base_addr);
    sn_pri(tdev, SN_INF, "vcmd_iosize: 0x%x\n", (vcmd_core_array + i)->vcmd_iosize);
    sn_pri(tdev, SN_INF, "vcmd_irq: 0x%x\n", (vcmd_core_array + i)->vcmd_irq);
    sn_pri(tdev, SN_INF, "sub_module_type: 0x%x\n", (vcmd_core_array + i)->sub_module_type);
    sn_pri(tdev, SN_INF, "submodule_main_addr: 0x%x\n", (vcmd_core_array + i)->submodule_main_addr);
    sn_pri(tdev, SN_INF, "submodule_dec400_addr: 0x%x\n", (vcmd_core_array + i)->submodule_dec400_addr);
    sn_pri(tdev, SN_INF, "submodule_L2Cache_addr: 0x%x\n", (vcmd_core_array + i)->submodule_L2Cache_addr);
    sn_pri(tdev, SN_INF, "submodule_MMU_addr: 0x%x\n\n", (vcmd_core_array + i)->submodule_MMU_addr[0]);
  }

  vcmd_reset_asic(hantrovcmd_data, vcmd_dev->total_vcmd_core_num);

  /* get the IRQ line */
  for (i = 0; i < vcmd_dev->total_vcmd_core_num; i++) {
    if (hantrovcmd_data[i].hwregs == NULL)
      continue;
    if (hantrovcmd_data[i].vcmd_core_cfg.vcmd_irq != -1) {
      result = request_irq(hantrovcmd_data[i].vcmd_core_cfg.vcmd_irq, hantrovcmd_isr, IRQF_SHARED, (vcmd_core_array + i)->vcmd_name, (void*) &hantrovcmd_data[i]);
      if (result == -EINVAL) {
        sn_pri(tdev, SN_ERR, "vc8000_vcmd_driver: Bad vcmd_irq number or handler. core_id=%d\n", i);
        vcmd_release_IO(tdev);
        goto err3;
      } else if (result == -EBUSY) {
        sn_pri(tdev, SN_ERR, "vc8000_vcmd_driver: IRQ <%d> busy, change your config. core_id=%d\n", hantrovcmd_data[i].vcmd_core_cfg.vcmd_irq, i);
        vcmd_release_IO(tdev);
        goto err3;
      } else
        sn_pri(tdev, SN_ERR, "vc8000_vcmd_driver: IRQ <%d>  request ok! core_id=%d\n", hantrovcmd_data[i].vcmd_core_cfg.vcmd_irq, i);
    } else
      sn_pri(tdev, SN_INF, "vc8000_vcmd_driver: IRQ not in use!\n");
    hantrovcmd_data[i].vcmd_dev = (void*) vcmd_dev;
  }

  //cmdbuf pool allocation
  //for cmdbuf management
  vcmd_dev->cmdbuf_used_pos = 0;
  for (k = 0; k < TOTAL_DISCRETE_CMDBUF_NUM; k++) {
    vcmd_dev->cmdbuf_used[k]        = 0;
    vcmd_dev->global_cmdbuf_node[k] = NULL;
  }
  vcmd_dev->cmdbuf_used_residual = TOTAL_DISCRETE_CMDBUF_NUM;
  //cmdbuf_used[0] not be used, because int vector must non-zero
  vcmd_dev->cmdbuf_used_pos = 1;
  vcmd_dev->cmdbuf_used[0]  = 1;
  vcmd_dev->cmdbuf_used_residual -= 1;

  for (i = 0; i < MAX_VCMD_TYPE; i++) {
    if (vcmd_dev->vcmd_type_core_num[i] == 0)
      continue;
    sema_init(&vcmd_dev->vcmd_reserve_cmdbuf_sem[i], 1);
  }

  sema_init(&vcmd_dev->vcmd_reserve_resource, vcmd_dev->total_vcmd_core_num);

  /*read all registers for each type of module for analyzing configuration in cwl*/
  for (i = 0; i < MAX_VCMD_TYPE; i++) {
    if (vcmd_dev->vcmd_type_core_num[i] == 0)
      continue;
    /*read registers once for each module type*/
    read_main_module_all_registers(tdev, i);
  }

  sn_osal_register(tdev, "decoder", osal_decoder, osal_decoder_handler, osal_decoder_close);
  sn_pri(tdev, SN_INF, "decoder: osal handler registered\n");

  vcmd_dev->hantrovcmd_data->perf_handle = sn_perf_register(tdev, "vcd", SN_PERF_ID_VCD, NULL, 4);
  timer_setup(&vcmd_dev->hantrovcmd_data->loading_timer, vcd_loading_timer_isr, 0);
  mod_timer(&vcmd_dev->hantrovcmd_data->loading_timer, jiffies);

  return 0;

err3:
  vcmd_release_IO(tdev);
err2:
  if (hantrovcmd_data != NULL)
    vfree(hantrovcmd_data);
err1:
  vcmd_pool_release(vcmd_dev);
err:
  sn_pri(tdev, SN_INF, "vc8000_vcmd_driver: module not inserted\n");
  return result;
}

void hantrovcmd_cleanup(struct sn_tranx_t* tdev) {
  int                    i = 0;
  u32                    result;
  struct vcmd_dev*       vcmd_dev        = NULL;
  struct hantrovcmd_dev* hantrovcmd_data = NULL;

  vcmd_dev = tdev->modules[SN_MODULE_VCMD];
  if (vcmd_dev == NULL) {
    sn_pri(tdev, SN_INF, "vc8000_vcmd_driver: vcmd_dev NULL!\n");
    return;
  }

  hantrovcmd_data = vcmd_dev->hantrovcmd_data;

  del_timer_sync(&hantrovcmd_data->loading_timer);

  for (i = 0; i < vcmd_dev->total_vcmd_core_num; i++) {
    if (hantrovcmd_data[i].hwregs == NULL)
      continue;
    //disable interrupt at first
    vcmd_write_reg(hantrovcmd_data[i].hwregs, VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
    //disable HW
    vcmd_write_reg(hantrovcmd_data[i].hwregs, VCMD_REGISTER_CONTROL_OFFSET, 0x0000);
    //read status register
    result = vcmd_read_reg(hantrovcmd_data[i].hwregs, VCMD_REGISTER_INT_STATUS_OFFSET);
    //clean status register
    vcmd_write_reg(hantrovcmd_data[i].hwregs, VCMD_REGISTER_INT_STATUS_OFFSET, result);

    /* free the vcmd IRQ */
    if (hantrovcmd_data[i].vcmd_core_cfg.vcmd_irq != -1)
      free_irq(hantrovcmd_data[i].vcmd_core_cfg.vcmd_irq, (void*) &hantrovcmd_data[i]);

    release_cmdbuf_node_cleanup(vcmd_dev, &hantrovcmd_data[i].list_manager);
  }

  vcmd_release_IO(tdev);
  vcmd_pool_release(vcmd_dev);

  if (vcmd_dev->hantrovcmd_data) {
    kfree(vcmd_dev->hantrovcmd_data);
  }
  if (vcmd_dev->vcmd_active_core_array) {
    kfree(vcmd_dev->vcmd_active_core_array);
  }
  kfree(vcmd_dev);

  sn_pri(tdev, SN_INF, "vc8000_vcmd_driver: module removed\n");
}

static int vcmd_reserve_IO(struct sn_tranx_t* tdev) {
  u32                    hwid;
  int                    i;
  u32                    found_hw        = 0;
  struct vcmd_dev*       dev             = NULL;
  struct hantrovcmd_dev* hantrovcmd_data = NULL;

  dev             = tdev->modules[SN_MODULE_VCMD];
  hantrovcmd_data = dev->hantrovcmd_data;

  dev->g_vcmd_base_hdwr = pci_resource_start(tdev->pdev, 2);
  for (i = 0; i < dev->total_vcmd_core_num; i++) {
    if (!slice_accessible(tdev, hantrovcmd_data[i].vcmd_core_cfg.slice))
      continue;
    hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr += dev->g_vcmd_base_hdwr;
    sn_pri(tdev, SN_INF, "%s hantrovcmd_data[%d].vcmd_core_cfg.vcmd_base_addr = 0x%lx\n", tdev->dev_name, i, dev->hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr);
    hantrovcmd_data[i].hwregs = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
    hantrovcmd_data[i].hwregs = ioremap(hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr, hantrovcmd_data[i].vcmd_core_cfg.vcmd_iosize);
#else
    hantrovcmd_data[i].hwregs = ioremap_nocache(hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr, hantrovcmd_data[i].vcmd_core_cfg.vcmd_iosize);
#endif
    if (hantrovcmd_data[i].hwregs == NULL) {
      sn_pri(tdev, SN_INF, "hantrovcmd: failed to ioremap HW regs\n");
      continue;
    }

    /*read hwid and check validness and store it*/
    hwid = (u32) ioread32(hantrovcmd_data[i].hwregs);
    sn_pri(tdev, SN_INF, "hwid=0x%08x\n", hwid);
    hantrovcmd_data[i].hw_version_id = hwid;

    /* check for vcmd HW ID */
    if (((hwid >> 16) & 0xFFFF) != VCMD_HW_ID) {
      sn_pri(tdev, SN_INF, "hantrovcmd: HW not found at 0x%llx\n", (long long unsigned int) hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr);
      iounmap(hantrovcmd_data[i].hwregs);
      hantrovcmd_data[i].hwregs = NULL;
      continue;
    }

    found_hw                    = 1;
    hantrovcmd_data[i].is_valid = 1;
    sn_pri(tdev, SN_INF, "hantrovcmd: HW at base <0x%llx> with ID <0x%08x>\n", (long long unsigned int) hantrovcmd_data[i].vcmd_core_cfg.vcmd_base_addr, hwid);
  }

  if (found_hw == 0) {
    sn_pri(tdev, SN_ERR, "hantrovcmd: NO ANY HW found!!\n");
    return -1;
  }

  return 0;
}

static void vcmd_release_IO(struct sn_tranx_t* tdev) {
  u32                    i;
  struct vcmd_dev*       dev             = NULL;
  struct hantrovcmd_dev* hantrovcmd_data = NULL;

  dev             = tdev->modules[SN_MODULE_VCMD];
  hantrovcmd_data = dev->hantrovcmd_data;
  for (i = 0; i < dev->total_vcmd_core_num; i++) {
    if (hantrovcmd_data[i].hwregs) {
      iounmap(hantrovcmd_data[i].hwregs);
      hantrovcmd_data[i].hwregs = NULL;
    }
  }
}

static int get_cmdbuf_node(struct hantrovcmd_dev* dev, u32 cmdbuf_id, bi_list_node** cmdbuf_node, int done) {
  bi_list_node*      new_cmdbuf_node = NULL;
  size_t             exe_cmdbuf_busAddress;
  struct cmdbuf_obj* cmdbuf_obj = NULL;
  struct vcmd_dev*   vcmd_dev   = (struct vcmd_dev*) dev->vcmd_dev;

  new_cmdbuf_node    = dev->list_manager.head;
  dev->working_state = WORKING_STATE_IDLE;

  *cmdbuf_node = NULL;
  if (dev->hw_version_id > HW_ID_1_0_C) {
    new_cmdbuf_node = vcmd_dev->global_cmdbuf_node[cmdbuf_id];
    if (new_cmdbuf_node == NULL) {
      printk(KERN_ERR "%s-%d error cmdbuf_id=%d!!\n", __func__, __LINE__, cmdbuf_id);
      return -EFAULT;
    }
  } else {
    if (!done)
      exe_cmdbuf_busAddress = VCMDGetAddrRegisterValue(dev->hwregs, dev->reg_mirror, HWIF_VCMD_EXECUTING_CMD_ADDR);

    //find the cmderror cmdbuf
    while (1) {
      if (new_cmdbuf_node == NULL) {
        printk(KERN_ERR "%s-%d error cmdbuf_id=%d!!\n", __func__, __LINE__, cmdbuf_id);
        return -EFAULT;
      }
      cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
      if (!done) {
        if ((((cmdbuf_obj->cmdbuf_busAddress - vcmd_dev->base_ddr_addr) <= exe_cmdbuf_busAddress) &&
                (((cmdbuf_obj->cmdbuf_busAddress - vcmd_dev->base_ddr_addr + cmdbuf_obj->cmdbuf_size) > exe_cmdbuf_busAddress))) &&
            (cmdbuf_obj->cmdbuf_run_done == 0))
          break;
      } else {
        if ((cmdbuf_obj->has_end_cmdbuf == 1) && (cmdbuf_obj->cmdbuf_run_done == 0))
          break;
      }
      new_cmdbuf_node = new_cmdbuf_node->next;
    }
  }

  *cmdbuf_node = new_cmdbuf_node;

  return 0;
}

static irqreturn_t hantrovcmd_isr(int irq, void* dev_id) {
  unsigned int           handled    = 0;
  struct hantrovcmd_dev* dev        = (struct hantrovcmd_dev*) dev_id;
  u32                    irq_status = 0;
  unsigned long          flags;
  bi_list_node*          new_cmdbuf_node      = NULL;
  bi_list_node*          base_cmdbuf_node     = NULL;
  struct cmdbuf_obj*     cmdbuf_obj           = NULL;
  u32                    cmdbuf_processed_num = 0;
  u32                    cmdbuf_id            = 0;
  struct vcmd_dev*       vcmd_dev             = (struct vcmd_dev*) dev->vcmd_dev;

  /*If core is not reserved by any user, but irq is received, just clean it*/
  spin_lock_irqsave(dev->spinlock, flags);

  if (dev->list_manager.head == NULL) {
    irq_status = vcmd_read_reg(dev->hwregs, VCMD_REGISTER_INT_STATUS_OFFSET);
    vcmd_write_reg(dev->hwregs, VCMD_REGISTER_INT_STATUS_OFFSET, irq_status);
    spin_unlock_irqrestore(dev->spinlock, flags);
    return IRQ_HANDLED;
  }

  irq_status = vcmd_read_reg(dev->hwregs, VCMD_REGISTER_INT_STATUS_OFFSET);
#ifdef VCMD_DEBUG_INTERNAL
  {
    u32 i, fordebug;
    for (i = 0; i < ASIC_VCMD_SWREG_AMOUNT; i++) {
      fordebug = vcmd_read_reg(dev->hwregs, i * 4);
      printk(KERN_INFO "vcmd register %d:0x%x\n", i, fordebug);
    }
  }
#endif

  if (!irq_status) {
    spin_unlock_irqrestore(dev->spinlock, flags);
    return IRQ_HANDLED;
  }

  vcmd_write_reg(dev->hwregs, VCMD_REGISTER_INT_STATUS_OFFSET, irq_status);

  dev->reg_mirror[VCMD_REGISTER_INT_STATUS_OFFSET / 4] = irq_status;

  if ((dev->hw_version_id > HW_ID_1_0_C) && (irq_status & 0x3f)) {
    //if error,read from register directly.
    cmdbuf_id = vcmd_get_register_value(dev->hwregs, dev->reg_mirror, HWIF_VCMD_CMDBUF_EXECUTING_ID);
    if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM) {
      printk(KERN_ERR "cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM, hantrovcmd_isr error cmdbuf_id greater than the ceiling !!\n");
      spin_unlock_irqrestore(dev->spinlock, flags);
      return IRQ_HANDLED;
    }
  } else if ((dev->hw_version_id > HW_ID_1_0_C)) {
    //read cmdbuf id from ddr
#ifdef VCMD_DEBUG_INTERNAL
    {
      u32 i, fordebug;
      printk(KERN_INFO "ddr vcmd register phy_addr=0x%x\n", dev->vcmd_reg_mem_busAddress);
      printk(KERN_INFO "ddr vcmd register virt_addr=0x%x\n", dev->vcmd_reg_mem_virtualAddress);
      for (i = 0; i < ASIC_VCMD_SWREG_AMOUNT; i++) {
        fordebug = *(dev->vcmd_reg_mem_virtualAddress + i);
        printk(KERN_INFO "ddr vcmd register %d:0x%x\n", i, fordebug);
      }
    }
#endif

    cmdbuf_id = *(dev->vcmd_reg_mem_virtualAddress + EXECUTING_CMDBUF_ID_ADDR);
    if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM) {
      printk(KERN_ERR "cmdbuf_id: %d, hantrovcmd_isr error cmdbuf_id greater than the ceiling !!\n", cmdbuf_id);
      spin_unlock_irqrestore(dev->spinlock, flags);
      return IRQ_HANDLED;
    }
  }

  if (vcmd_get_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_RESET)) {
    //reset error,all cmdbuf that is not  done will be run again.
    sn_pri(vcmd_dev->tdev, SN_INF, "hantrovcmd_isr : HWIF_VCMD_IRQ_RESET: cmdbuf_id = %d\n", cmdbuf_id);
    new_cmdbuf_node    = dev->list_manager.head;
    dev->working_state = WORKING_STATE_IDLE;
    //find the first run_done=0
    while (1) {
      if (new_cmdbuf_node == NULL)
        break;
      cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
      if ((cmdbuf_obj->cmdbuf_run_done == 0))
        break;
      new_cmdbuf_node = new_cmdbuf_node->next;
    }
    base_cmdbuf_node = new_cmdbuf_node;
    vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
    vcmd_link_cmdbuf(dev, base_cmdbuf_node);
    if (dev->sw_cmdbuf_rdy_num != 0) {
      //restart new command
      vcmd_start(dev, base_cmdbuf_node);
    }
    handled++;
    spin_unlock_irqrestore(dev->spinlock, flags);
    return IRQ_HANDLED;
  }
  if (vcmd_get_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_ABORT)) {
    sn_pri(vcmd_dev->tdev, SN_INF, "hantrovcmd_isr : HWIF_VCMD_IRQ_ABORT: cmdbuf_id = %d\n", cmdbuf_id);
    if (get_cmdbuf_node(dev, cmdbuf_id, &new_cmdbuf_node, 0)) {
      spin_unlock_irqrestore(dev->spinlock, flags);
      wake_up_interruptible_all(dev->wait_abort_queue);
      return IRQ_HANDLED;
    }
    base_cmdbuf_node = new_cmdbuf_node;
    // this cmdbuf and cmdbufs prior to itself, run_done = 1
    while (1) {
      if (new_cmdbuf_node == NULL)
        break;
      cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
      if ((cmdbuf_obj->cmdbuf_run_done == 0)) {
        cmdbuf_obj->cmdbuf_run_done  = 1;
        cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
        cmdbuf_processed_num++;
      } else
        break;
      new_cmdbuf_node = new_cmdbuf_node->previous;
    }
    base_cmdbuf_node = base_cmdbuf_node->next;
    vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
    if (vcmd_dev->software_triger_abort == 0) {
      //for QCFE
      vcmd_link_cmdbuf(dev, base_cmdbuf_node);
      if (dev->sw_cmdbuf_rdy_num != 0) {
        //restart new command
        vcmd_start(dev, base_cmdbuf_node);
      }
    }
    spin_unlock_irqrestore(dev->spinlock, flags);
    if (cmdbuf_processed_num)
      wake_up_interruptible_all(dev->wait_queue);
    //to let high priority cmdbuf be inserted
    wake_up_interruptible_all(dev->wait_abort_queue);
    wake_up_interruptible_all(&vcmd_dev->mc_wait_queue);
    handled++;
    return IRQ_HANDLED;
  }
  if (vcmd_get_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_BUSERR)) {
    sn_pri(vcmd_dev->tdev, SN_INF, "hantrovcmd_isr : HWIF_VCMD_IRQ_BUSERR: cmdbuf_id = %d\n", cmdbuf_id);
    if (get_cmdbuf_node(dev, cmdbuf_id, &new_cmdbuf_node, 0)) {
      printk(KERN_ERR "HWIF_VCMD_IRQ_BUSERR: hantrovcmd_isr error cmdbuf_id !!\n");
      spin_unlock_irqrestore(dev->spinlock, flags);
      return IRQ_HANDLED;
    }
    base_cmdbuf_node = new_cmdbuf_node;
    // this cmdbuf and cmdbufs prior to itself, run_done = 1
    while (1) {
      if (new_cmdbuf_node == NULL)
        break;
      cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
      if ((cmdbuf_obj->cmdbuf_run_done == 0)) {
        cmdbuf_obj->cmdbuf_run_done  = 1;
        cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
        cmdbuf_processed_num++;
      } else
        break;
      new_cmdbuf_node = new_cmdbuf_node->previous;
    }
    new_cmdbuf_node = base_cmdbuf_node;
    if (new_cmdbuf_node != NULL) {
      cmdbuf_obj                   = (struct cmdbuf_obj*) new_cmdbuf_node->data;
      cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_BUSERR;
    }
    base_cmdbuf_node = base_cmdbuf_node->next;
    vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
    vcmd_link_cmdbuf(dev, base_cmdbuf_node);
    if (dev->sw_cmdbuf_rdy_num != 0) {
      //restart new command
      vcmd_start(dev, base_cmdbuf_node);
    }
    spin_unlock_irqrestore(dev->spinlock, flags);
    if (cmdbuf_processed_num)
      wake_up_interruptible_all(dev->wait_queue);
    handled++;
    wake_up_interruptible_all(&vcmd_dev->mc_wait_queue);
    return IRQ_HANDLED;
  }
  if (vcmd_get_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_TIMEOUT)) {
    sn_pri(vcmd_dev->tdev, SN_INF, "hantrovcmd_isr : HWIF_VCMD_IRQ_TIMEOUT: cmdbuf_id = %d\n", cmdbuf_id);
    if (get_cmdbuf_node(dev, cmdbuf_id, &new_cmdbuf_node, 0)) {
      printk(KERN_ERR "HWIF_VCMD_IRQ_TIMEOUT: hantrovcmd_isr error cmdbuf_id !!\n");
      spin_unlock_irqrestore(dev->spinlock, flags);
      return IRQ_HANDLED;
    }
    base_cmdbuf_node = new_cmdbuf_node;
    new_cmdbuf_node  = new_cmdbuf_node->previous;
    // this cmdbuf and cmdbufs prior to itself, run_done = 1
    while (1) {
      if (new_cmdbuf_node == NULL)
        break;
      cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
      if ((cmdbuf_obj->cmdbuf_run_done == 0)) {
        cmdbuf_obj->cmdbuf_run_done  = 1;
        cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
        cmdbuf_processed_num++;
      } else
        break;
      new_cmdbuf_node = new_cmdbuf_node->previous;
    }
    vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
    vcmd_link_cmdbuf(dev, base_cmdbuf_node);
    if (dev->sw_cmdbuf_rdy_num != 0) {
      //reset
      vcmd_reset_current_asic(dev);
      //restart new command
      vcmd_start(dev, base_cmdbuf_node);
    }
    spin_unlock_irqrestore(dev->spinlock, flags);
    if (cmdbuf_processed_num)
      wake_up_interruptible_all(dev->wait_queue);
    handled++;
    wake_up_interruptible_all(&vcmd_dev->mc_wait_queue);
    return IRQ_HANDLED;
  }
  if (vcmd_get_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_CMDERR)) {
    sn_pri(vcmd_dev->tdev, SN_INF, "hantrovcmd_isr : HWIF_VCMD_IRQ_CMDERR: cmdbuf_id = %d\n", cmdbuf_id);
    if (get_cmdbuf_node(dev, cmdbuf_id, &new_cmdbuf_node, 0)) {
      spin_unlock_irqrestore(dev->spinlock, flags);
      wake_up_interruptible_all(dev->wait_abort_queue);
      return IRQ_HANDLED;
    }
    base_cmdbuf_node = new_cmdbuf_node;
    // this cmdbuf and cmdbufs prior to itself, run_done = 1
    while (1) {
      if (new_cmdbuf_node == NULL)
        break;
      cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
      if ((cmdbuf_obj->cmdbuf_run_done == 0)) {
        cmdbuf_obj->cmdbuf_run_done  = 1;
        cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
        cmdbuf_processed_num++;
      } else
        break;
      new_cmdbuf_node = new_cmdbuf_node->previous;
    }
    new_cmdbuf_node = base_cmdbuf_node;
    if (new_cmdbuf_node != NULL) {
      cmdbuf_obj                   = (struct cmdbuf_obj*) new_cmdbuf_node->data;
      cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_CMDERR; //cmderr
    }
    base_cmdbuf_node = base_cmdbuf_node->next;
    vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
    vcmd_link_cmdbuf(dev, base_cmdbuf_node);
    if (dev->sw_cmdbuf_rdy_num != 0) {
      //restart new command
      vcmd_start(dev, base_cmdbuf_node);
    }
    spin_unlock_irqrestore(dev->spinlock, flags);
    if (cmdbuf_processed_num)
      wake_up_interruptible_all(dev->wait_queue);
    handled++;
    wake_up_interruptible_all(&vcmd_dev->mc_wait_queue);
    return IRQ_HANDLED;
  }

  if (vcmd_get_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_ENDCMD)) {
    sn_pri(vcmd_dev->tdev, SN_INF, "hantrovcmd_isr : HWIF_VCMD_IRQ_ENDCMD: cmdbuf_id = %d\n", cmdbuf_id);
    if (get_cmdbuf_node(dev, cmdbuf_id, &new_cmdbuf_node, 1)) {
      printk(KERN_ERR "HWIF_VCMD_IRQ_ENDCMD: hantrovcmd_isr error cmdbuf_id !!\n");
      spin_unlock_irqrestore(dev->spinlock, flags);
      return IRQ_HANDLED;
    }
    base_cmdbuf_node = new_cmdbuf_node;
    // this cmdbuf and cmdbufs prior to itself, run_done = 1
    while (1) {
      if (new_cmdbuf_node == NULL)
        break;
      cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
      if ((cmdbuf_obj->cmdbuf_run_done == 0)) {
        cmdbuf_obj->cmdbuf_run_done  = 1;
        cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
        cmdbuf_processed_num++;
      } else
        break;
      new_cmdbuf_node = new_cmdbuf_node->previous;
    }
    base_cmdbuf_node = base_cmdbuf_node->next;
    vcmd_delink_cmdbuf(dev, base_cmdbuf_node);
    vcmd_link_cmdbuf(dev, base_cmdbuf_node);
    if (dev->sw_cmdbuf_rdy_num != 0) {
      //restart new command
      vcmd_start(dev, base_cmdbuf_node);
    }
    spin_unlock_irqrestore(dev->spinlock, flags);
    if (cmdbuf_processed_num)
      wake_up_interruptible_all(dev->wait_queue);
    handled++;
    wake_up_interruptible_all(&vcmd_dev->mc_wait_queue);
    return IRQ_HANDLED;
  }
  if (dev->hw_version_id <= HW_ID_1_0_C)
    cmdbuf_id = vcmd_get_register_mirror_value(dev->reg_mirror, HWIF_VCMD_IRQ_INTCMD);
  if (cmdbuf_id) {
    if (dev->hw_version_id <= HW_ID_1_0_C) {
      if (cmdbuf_id >= TOTAL_DISCRETE_CMDBUF_NUM) {
        printk(KERN_ERR "hantrovcmd_isr error cmdbuf_id greater than the ceiling !!\n");
        spin_unlock_irqrestore(dev->spinlock, flags);
        return IRQ_HANDLED;
      }
    }
    new_cmdbuf_node = vcmd_dev->global_cmdbuf_node[cmdbuf_id];
    if (new_cmdbuf_node == NULL) {
      printk(KERN_ERR "hantrovcmd_isr error cmdbuf_id %d!!, softabort %d\n", cmdbuf_id, vcmd_dev->software_triger_abort);
      spin_unlock_irqrestore(dev->spinlock, flags);
      return IRQ_HANDLED;
    }
    // interrupt cmdbuf and cmdbufs prior to itself, run_done = 1
    while (1) {
      if (new_cmdbuf_node == NULL)
        break;
      cmdbuf_obj = (struct cmdbuf_obj*) new_cmdbuf_node->data;
      if (cmdbuf_obj == NULL)
        break;
      if (cmdbuf_obj->cmdbuf_run_done == 0) {
        cmdbuf_obj->cmdbuf_run_done  = 1;
        cmdbuf_obj->executing_status = CMDBUF_EXE_STATUS_OK;
        cmdbuf_processed_num++;
      } else
        break;
      new_cmdbuf_node = new_cmdbuf_node->previous;
    }
    handled++;
  }

  spin_unlock_irqrestore(dev->spinlock, flags);
  if (cmdbuf_processed_num) {
    wake_up_interruptible_all(dev->wait_queue);
    wake_up_all(dev->wait_queue);
  }
  if (!handled) {
    printk(KERN_INFO "IRQ received, but not hantro's!\n");
  }
  wake_up_interruptible_all(&vcmd_dev->mc_wait_queue);
  return IRQ_HANDLED;
}

static void vcmd_reset_asic_ex(struct hantrovcmd_dev* dev, u32 index) {
  int i;
  u32 result;

  if (dev[index].hwregs != NULL) {
    //disable interrupt at first
    vcmd_write_reg(dev[index].hwregs, VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
    //reset all
    // vcmd_write_reg((const void *)dev[n].hwregs,VCMD_REGISTER_CONTROL_OFFSET,0x0002);
    vcmd_write_reg(dev[index].hwregs, VCMD_REGISTER_CONTROL_OFFSET, 0x0004);
    msleep(10);
    //read status register
    result = vcmd_read_reg(dev[index].hwregs, VCMD_REGISTER_INT_STATUS_OFFSET);
    //clean status register
    vcmd_write_reg(dev[index].hwregs, VCMD_REGISTER_INT_STATUS_OFFSET, result);

    //set all register 0
    for (i = VCMD_REGISTER_CONTROL_OFFSET; i < dev[index].vcmd_core_cfg.vcmd_iosize; i += 4) {
      vcmd_write_reg(dev[index].hwregs, i, 0x0000);
    }

    //enable all interrupt
    vcmd_write_reg(dev[index].hwregs, VCMD_REGISTER_INT_CTL_OFFSET, 0xffffffff);
  }
}

void vcmd_reset_asic(struct hantrovcmd_dev* dev, u32 count) {
  int i, n;
  u32 result;

  for (n = 0; n < count; n++) {
    if (dev[n].hwregs != NULL) {
      //disable interrupt at first
      vcmd_write_reg(dev[n].hwregs, VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
      //reset all
      vcmd_write_reg(dev[n].hwregs, VCMD_REGISTER_CONTROL_OFFSET, 0x0004);
      msleep(10);
      //read status register
      result = vcmd_read_reg(dev[n].hwregs, VCMD_REGISTER_INT_STATUS_OFFSET);
      //clean status register
      vcmd_write_reg(dev[n].hwregs, VCMD_REGISTER_INT_STATUS_OFFSET, result);

      //set all register 0
      for (i = VCMD_REGISTER_CONTROL_OFFSET; i < dev[n].vcmd_core_cfg.vcmd_iosize; i += 4) {
        vcmd_write_reg(dev[n].hwregs, i, 0x0000);
      }

      //enable all interrupt
      vcmd_write_reg(dev[n].hwregs, VCMD_REGISTER_INT_CTL_OFFSET, 0xffffffff);
    }
  }
}

static void vcmd_reset_current_asic(struct hantrovcmd_dev* dev) {
  u32 result;

  if (dev->hwregs != NULL) {
    //disable interrupt at first
    vcmd_write_reg(dev->hwregs, VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
    //reset all
    vcmd_write_reg(dev->hwregs, VCMD_REGISTER_CONTROL_OFFSET, 0x0004);
    msleep(10);
    //read status register
    result = vcmd_read_reg(dev->hwregs, VCMD_REGISTER_INT_STATUS_OFFSET);
    //clean status register
    vcmd_write_reg(dev->hwregs, VCMD_REGISTER_INT_STATUS_OFFSET, result);
  }
}
