// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 Xilinx Inc.
 *
 * This is xilinx scaler driver for Linux.
 * This file provide register operation and initialization,
 * like read/write a register or pull/push a batch of registers.
 */

#include <linux/pci.h>
#include <linux/pagemap.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/crc32.h>
#include <linux/vmalloc.h>
#include <linux/semaphore.h>

#include "common.h"
#include "transcoder.h"
#include "regs.h"

#include "sn_perf.h"

#include "sn_osal.h"
#include "memory_osal.h"

#include "xabr_scaler.h"

//------------------------Address Info-------------------
// 0x00 : Control signals
//        bit 0  - ap_start (Read/Write/COH)
//        bit 1  - ap_done (Read/COR)
//        bit 2  - ap_idle (Read)
//        bit 3  - ap_ready (Read)
//        bit 7  - auto_restart (Read/Write)
//        others - reserved
// 0x04 : Global Interrupt Enable Register
//        bit 0  - Global Interrupt Enable (Read/Write)
//        others - reserved
// 0x08 : IP Interrupt Enable Register (Read/Write)
//        bit 0  - Channel 0 (ap_done)
//        bit 1  - Channel 1 (ap_ready)
//        others - reserved
// 0x0c : IP Interrupt Status Register (Read/TOW)
//        bit 0  - Channel 0 (ap_done)
//        bit 1  - Channel 1 (ap_ready)
//        others - reserved
// 0x10 : Feature select
//        bit 0  - crc en_dis
//        bit 1  - wdt en_dis
//        others - wdt timer threshold value in cycles
// 0x14 : reserved
// 0x18 : Data signal of cmd_blk_V
//        bit 31~0 - cmd_blk_V[31:0] (Read/Write)
// 0x1c : Data signal of cmd_blk_V
//        bit 31~0 - cmd_blk_V[63:32] (Read/Write)
// 0x8c : reserved
// 0x90 : Data signal of error_flag_crc_V
//        bit 0  - error_flag_crc_V[0] (Read)
//        others - reserved
// 0x94 : Control signal of error_flag_crc_V
//        bit 0  - error_flag_crc_V_ap_vld (Read/COR)
//        others - reserved
// 0x98 : Data signal of error_flag_wdt_V
//        bit 0  - error_flag_wdt_V[0] (Read)
//        others - reserved
// 0x9c : Control signal of error_flag_wdt_V
//        bit 0  - error_flag_wdt_V_ap_vld (Read/COR)
//        others - reserved
// 0xb0 : Version Register
//        bit 31~0 - ABRScaler IP Version (Read)
// 0xb4 : Frame Cycle Count_0
//        bit 31~0 - cycle count (Read)
//.
//.
// 0xf4 : Frame Cycle Count_15
//        bit 31~0 - cycle count (Read)

#define XABR_SC_ADDR_AP_CTRL            0x00
#define XABR_SC_ADDR_GIE                0x04
#define XABR_SC_ADDR_IER                0x08
#define XABR_SC_ADDR_ISR                0x0c
#define XABR_SC_ADDR_CRC_WDT            0x10
#define XABR_SC_ADDR_CMD_BLK_ADDR_LO    0x18
#define XABR_SC_ADDR_CMD_BLK_ADDR_HI    0x1C
#define XABR_SC_RESET_PERF_COUNTER      0x30
#define XABR_SC_ADDR_DATA_CRC_ERROR     0x90
#define XABR_SC_ADDR_CTL_CRC_ERROR      0x94
#define XABR_SC_ADDR_DATA_WDT_ERROR     0x98
#define XABR_SC_ADDR_CTL_WDT_ERROR      0x9c
#define XABR_SC_ADDR_VERSION            0xb0
#define XABR_SC_ADDR_CYCLE_CNT_0        0xb4
#define XABR_SC_ADDR_CYCLE_CNT_15       0xf4
#define XABR_SC_REGMAP_SIZE             0xf8

#define MAX_CHANNELS                    16

#define XABR_CORE_IO_SIZE               (XABR_SC_REGMAP_SIZE)
#define XABR_DEC400_IO_SIZE             (1568*4)

#define XABR_WATCHDOG_TIMEOUT_SEC       5
#define XABR_RESERVE_CORE_TIMEOUT_SEC   2

typedef enum {
    OSAL_ACCL_CMD_SCALER_RUN = 1,
    OSAL_ACCL_CMD_SCALER_END
}xabr_osal_cmds;

typedef enum {
    XABR_CORE_HW = 0,
    XABR_CORE_DEC400,
    XABR_CORE_MAX
}xabr_core_type;

typedef enum {
    wd_ctl = 0,
    wd_data,
    crc_ctl,
    crc_data,
    ap_done,
    NUM_IRQ_FLAGS
}IrqFlags;

typedef enum {
    IRQ_SRC_INVALID = 0,
    IRQ_SRC_WDT,
    IRQ_SRC_CRC,
    IRQ_SRC_AP_DONE
}IrqSource;

struct xabr_core_desc {
    int slice;
    xabr_core_type type;
    long base;
    int iosize;
    int irq;
    const char *name;
};

/*config cores in xabr subsystem in 2 slices*/
//PF Mode
static struct xabr_core_desc pf_core_array[][XABR_CORE_MAX]= {
    {
        {0, XABR_CORE_HW,     S1_ABR_SCL_CORE_OFF,   XABR_CORE_IO_SIZE,   17, "xabr_scl_0"},
        {0, XABR_CORE_DEC400, S1_ABR_SCL_DEC400_OFF, XABR_DEC400_IO_SIZE, -1, "xabr_dec_0"},
    },
    {
        {1, XABR_CORE_HW,     S2_ABR_SCL_CORE_OFF,   XABR_CORE_IO_SIZE,   27, "xabr_scl_1"},
        {1, XABR_CORE_DEC400, S2_ABR_SCL_DEC400_OFF, XABR_DEC400_IO_SIZE, -1, "xabr_dec_1"},
    }
};

//One VF Mode
static struct xabr_core_desc onevf_core_array[][XABR_CORE_MAX]= {
    {
        {0, XABR_CORE_HW,     ONE_VF_S1_ABR_SCL_CORE_OFF,   XABR_CORE_IO_SIZE,   17, "xabr_scl_0"},
        {0, XABR_CORE_DEC400, ONE_VF_S1_ABR_SCL_DEC400_OFF, XABR_DEC400_IO_SIZE, -1, "xabr_dec_0"},
    },
    {
        {1, XABR_CORE_HW,     ONE_VF_S2_ABR_SCL_CORE_OFF,   XABR_CORE_IO_SIZE,   27, "xabr_scl_1"},
        {1, XABR_CORE_DEC400, ONE_VF_S2_ABR_SCL_DEC400_OFF, XABR_DEC400_IO_SIZE, -1, "xabr_dec_1"},
    }
};

struct core_cfg {
    int irq;
    unsigned long base_addr;
    volatile u8 *hwreg;
    u32 iosize;
    u32 *shadow;
    const char *name;
    int irq_recvd;
};

struct xabr_scaler {
    int num_cores;
    struct core_cfg subsys[MAX_SLICE_NUM][XABR_CORE_MAX];
    spinlock_t irq_lock;
    spinlock_t reserve_lock;
    wait_queue_head_t wait_queue;
    struct semaphore reserve_semaphore;
    void *regs_shadow;
    IrqFlags irq_flags[MAX_SLICE_NUM][NUM_IRQ_FLAGS];
    int isCoreLocked[MAX_SLICE_NUM];
    struct file *filp[MAX_SLICE_NUM];
    u32 cycle_cnt[MAX_SLICE_NUM][MAX_CHANNELS];
    struct sn_tranx_t *tdev;
    struct loading_info loading[MAX_SLICE_NUM];
    struct timer_list loading_timer;
    SnPerfHandle perf_handle;
    int perf_enable;
    uint64_t acc_cycle_cnt[MAX_SLICE_NUM];
};

////////////////////////////////////////////////////////////////////////////////
///////////////////////// DEC 400 functionality starts /////////////////////////
////////////////////////////////////////////////////////////////////////////////
// These are needed for DEC400 functionality
#define MAX_READ_STREAM  32
#define MAX_WRITE_STREAM 32

#define XABR_DECControl            (0x00800)
#define XABR_DECIntrAcknowledgeEx  (0x0081C)
#define XABR_DECIntrAcknowledgeEx2 (0x00820)
#define XABR_DECStatus             (0x00878)

#define TIMEOUT_COUNT       (50000)


// Const table for DEC400 default configuration, instead of generating/preparing for every frame/session.
// Will relook into this later
const uint32_t dec400_reg_map_defaults[] ={
    0x0880, 0x00000000, 0x0900, 0x00000000, 0x0a80, 0xffffffff, 0x0b80, 0xffffffff, 0x0b00, 0xffffffff, 0x0c00, 0xffffffff, 0x1080, 0xffffffff, 0x1100, 0xffffffff,
    0x0884, 0x00000000, 0x0904, 0x00000000, 0x0a84, 0xffffffff, 0x0b84, 0xffffffff, 0x0b04, 0xffffffff, 0x0c04, 0xffffffff, 0x1084, 0xffffffff, 0x1104, 0xffffffff,
    0x0888, 0x00000000, 0x0908, 0x00000000, 0x0a88, 0xffffffff, 0x0b88, 0xffffffff, 0x0b08, 0xffffffff, 0x0c08, 0xffffffff, 0x1088, 0xffffffff, 0x1108, 0xffffffff,
    0x088c, 0x00000000, 0x090c, 0x00000000, 0x0a8c, 0xffffffff, 0x0b8c, 0xffffffff, 0x0b0c, 0xffffffff, 0x0c0c, 0xffffffff, 0x108c, 0xffffffff, 0x110c, 0xffffffff,
    0x0890, 0x00000000, 0x0910, 0x00000000, 0x0a90, 0xffffffff, 0x0b90, 0xffffffff, 0x0b10, 0xffffffff, 0x0c10, 0xffffffff, 0x1090, 0xffffffff, 0x1110, 0xffffffff,
    0x0894, 0x00000000, 0x0914, 0x00000000, 0x0a94, 0xffffffff, 0x0b94, 0xffffffff, 0x0b14, 0xffffffff, 0x0c14, 0xffffffff, 0x1094, 0xffffffff, 0x1114, 0xffffffff,
    0x0898, 0x00000000, 0x0918, 0x00000000, 0x0a98, 0xffffffff, 0x0b98, 0xffffffff, 0x0b18, 0xffffffff, 0x0c18, 0xffffffff, 0x1098, 0xffffffff, 0x1118, 0xffffffff,
    0x089c, 0x00000000, 0x091c, 0x00000000, 0x0a9c, 0xffffffff, 0x0b9c, 0xffffffff, 0x0b1c, 0xffffffff, 0x0c1c, 0xffffffff, 0x109c, 0xffffffff, 0x111c, 0xffffffff,
    0x08a0, 0x00000000, 0x0920, 0x00000000, 0x0aa0, 0xffffffff, 0x0ba0, 0xffffffff, 0x0b20, 0xffffffff, 0x0c20, 0xffffffff, 0x10a0, 0xffffffff, 0x1120, 0xffffffff,
    0x08a4, 0x00000000, 0x0924, 0x00000000, 0x0aa4, 0xffffffff, 0x0ba4, 0xffffffff, 0x0b24, 0xffffffff, 0x0c24, 0xffffffff, 0x10a4, 0xffffffff, 0x1124, 0xffffffff,
    0x08a8, 0x00000000, 0x0928, 0x00000000, 0x0aa8, 0xffffffff, 0x0ba8, 0xffffffff, 0x0b28, 0xffffffff, 0x0c28, 0xffffffff, 0x10a8, 0xffffffff, 0x1128, 0xffffffff,
    0x08ac, 0x00000000, 0x092c, 0x00000000, 0x0aac, 0xffffffff, 0x0bac, 0xffffffff, 0x0b2c, 0xffffffff, 0x0c2c, 0xffffffff, 0x10ac, 0xffffffff, 0x112c, 0xffffffff,
    0x08b0, 0x00000000, 0x0930, 0x00000000, 0x0ab0, 0xffffffff, 0x0bb0, 0xffffffff, 0x0b30, 0xffffffff, 0x0c30, 0xffffffff, 0x10b0, 0xffffffff, 0x1130, 0xffffffff,
    0x08b4, 0x00000000, 0x0934, 0x00000000, 0x0ab4, 0xffffffff, 0x0bb4, 0xffffffff, 0x0b34, 0xffffffff, 0x0c34, 0xffffffff, 0x10b4, 0xffffffff, 0x1134, 0xffffffff,
    0x08b8, 0x00000000, 0x0938, 0x00000000, 0x0ab8, 0xffffffff, 0x0bb8, 0xffffffff, 0x0b38, 0xffffffff, 0x0c38, 0xffffffff, 0x10b8, 0xffffffff, 0x1138, 0xffffffff,
    0x08bc, 0x00000000, 0x093c, 0x00000000, 0x0abc, 0xffffffff, 0x0bbc, 0xffffffff, 0x0b3c, 0xffffffff, 0x0c3c, 0xffffffff, 0x10bc, 0xffffffff, 0x113c, 0xffffffff,
    0x08c0, 0x00000000, 0x0940, 0x00000000, 0x0ac0, 0xffffffff, 0x0bc0, 0xffffffff, 0x0b40, 0xffffffff, 0x0c40, 0xffffffff, 0x10c0, 0xffffffff, 0x1140, 0xffffffff,
    0x08c4, 0x00000000, 0x0944, 0x00000000, 0x0ac4, 0xffffffff, 0x0bc4, 0xffffffff, 0x0b44, 0xffffffff, 0x0c44, 0xffffffff, 0x10c4, 0xffffffff, 0x1144, 0xffffffff,
    0x08c8, 0x00000000, 0x0948, 0x00000000, 0x0ac8, 0xffffffff, 0x0bc8, 0xffffffff, 0x0b48, 0xffffffff, 0x0c48, 0xffffffff, 0x10c8, 0xffffffff, 0x1148, 0xffffffff,
    0x08cc, 0x00000000, 0x094c, 0x00000000, 0x0acc, 0xffffffff, 0x0bcc, 0xffffffff, 0x0b4c, 0xffffffff, 0x0c4c, 0xffffffff, 0x10cc, 0xffffffff, 0x114c, 0xffffffff,
    0x08d0, 0x00000000, 0x0950, 0x00000000, 0x0ad0, 0xffffffff, 0x0bd0, 0xffffffff, 0x0b50, 0xffffffff, 0x0c50, 0xffffffff, 0x10d0, 0xffffffff, 0x1150, 0xffffffff,
    0x08d4, 0x00000000, 0x0954, 0x00000000, 0x0ad4, 0xffffffff, 0x0bd4, 0xffffffff, 0x0b54, 0xffffffff, 0x0c54, 0xffffffff, 0x10d4, 0xffffffff, 0x1154, 0xffffffff,
    0x08d8, 0x00000000, 0x0958, 0x00000000, 0x0ad8, 0xffffffff, 0x0bd8, 0xffffffff, 0x0b58, 0xffffffff, 0x0c58, 0xffffffff, 0x10d8, 0xffffffff, 0x1158, 0xffffffff,
    0x08dc, 0x00000000, 0x095c, 0x00000000, 0x0adc, 0xffffffff, 0x0bdc, 0xffffffff, 0x0b5c, 0xffffffff, 0x0c5c, 0xffffffff, 0x10dc, 0xffffffff, 0x115c, 0xffffffff,
    0x08e0, 0x00000000, 0x0960, 0x00000000, 0x0ae0, 0xffffffff, 0x0be0, 0xffffffff, 0x0b60, 0xffffffff, 0x0c60, 0xffffffff, 0x10e0, 0xffffffff, 0x1160, 0xffffffff,
    0x08e4, 0x00000000, 0x0964, 0x00000000, 0x0ae4, 0xffffffff, 0x0be4, 0xffffffff, 0x0b64, 0xffffffff, 0x0c64, 0xffffffff, 0x10e4, 0xffffffff, 0x1164, 0xffffffff,
    0x08e8, 0x00000000, 0x0968, 0x00000000, 0x0ae8, 0xffffffff, 0x0be8, 0xffffffff, 0x0b68, 0xffffffff, 0x0c68, 0xffffffff, 0x10e8, 0xffffffff, 0x1168, 0xffffffff,
    0x08ec, 0x00000000, 0x096c, 0x00000000, 0x0aec, 0xffffffff, 0x0bec, 0xffffffff, 0x0b6c, 0xffffffff, 0x0c6c, 0xffffffff, 0x10ec, 0xffffffff, 0x116c, 0xffffffff,
    0x08f0, 0x00000000, 0x0970, 0x00000000, 0x0af0, 0xffffffff, 0x0bf0, 0xffffffff, 0x0b70, 0xffffffff, 0x0c70, 0xffffffff, 0x10f0, 0xffffffff, 0x1170, 0xffffffff,
    0x08f4, 0x00000000, 0x0974, 0x00000000, 0x0af4, 0xffffffff, 0x0bf4, 0xffffffff, 0x0b74, 0xffffffff, 0x0c74, 0xffffffff, 0x10f4, 0xffffffff, 0x1174, 0xffffffff,
    0x08f8, 0x00000000, 0x0978, 0x00000000, 0x0af8, 0xffffffff, 0x0bf8, 0xffffffff, 0x0b78, 0xffffffff, 0x0c78, 0xffffffff, 0x10f8, 0xffffffff, 0x1178, 0xffffffff,
    0x08fc, 0x00000000, 0x097c, 0x00000000, 0x0afc, 0xffffffff, 0x0bfc, 0xffffffff, 0x0b7c, 0xffffffff, 0x0c7c, 0xffffffff, 0x10fc, 0xffffffff, 0x117c, 0xffffffff,
    0x0980, 0x00000000, 0x0a00, 0x00000000, 0x0d80, 0xffffffff, 0x0e80, 0xffffffff, 0x0e00, 0xffffffff, 0x0f00, 0xffffffff, 0x1180, 0xffffffff, 0x1200, 0xffffffff,
    0x0984, 0x00000000, 0x0a04, 0x00000000, 0x0d84, 0xffffffff, 0x0e84, 0xffffffff, 0x0e04, 0xffffffff, 0x0f04, 0xffffffff, 0x1184, 0xffffffff, 0x1204, 0xffffffff,
    0x0988, 0x00000000, 0x0a08, 0x00000000, 0x0d88, 0xffffffff, 0x0e88, 0xffffffff, 0x0e08, 0xffffffff, 0x0f08, 0xffffffff, 0x1188, 0xffffffff, 0x1208, 0xffffffff,
    0x098c, 0x00000000, 0x0a0c, 0x00000000, 0x0d8c, 0xffffffff, 0x0e8c, 0xffffffff, 0x0e0c, 0xffffffff, 0x0f0c, 0xffffffff, 0x118c, 0xffffffff, 0x120c, 0xffffffff,
    0x0990, 0x00000000, 0x0a10, 0x00000000, 0x0d90, 0xffffffff, 0x0e90, 0xffffffff, 0x0e10, 0xffffffff, 0x0f10, 0xffffffff, 0x1190, 0xffffffff, 0x1210, 0xffffffff,
    0x0994, 0x00000000, 0x0a14, 0x00000000, 0x0d94, 0xffffffff, 0x0e94, 0xffffffff, 0x0e14, 0xffffffff, 0x0f14, 0xffffffff, 0x1194, 0xffffffff, 0x1214, 0xffffffff,
    0x0998, 0x00000000, 0x0a18, 0x00000000, 0x0d98, 0xffffffff, 0x0e98, 0xffffffff, 0x0e18, 0xffffffff, 0x0f18, 0xffffffff, 0x1198, 0xffffffff, 0x1218, 0xffffffff,
    0x099c, 0x00000000, 0x0a1c, 0x00000000, 0x0d9c, 0xffffffff, 0x0e9c, 0xffffffff, 0x0e1c, 0xffffffff, 0x0f1c, 0xffffffff, 0x119c, 0xffffffff, 0x121c, 0xffffffff,
    0x09a0, 0x00000000, 0x0a20, 0x00000000, 0x0da0, 0xffffffff, 0x0ea0, 0xffffffff, 0x0e20, 0xffffffff, 0x0f20, 0xffffffff, 0x11a0, 0xffffffff, 0x1220, 0xffffffff,
    0x09a4, 0x00000000, 0x0a24, 0x00000000, 0x0da4, 0xffffffff, 0x0ea4, 0xffffffff, 0x0e24, 0xffffffff, 0x0f24, 0xffffffff, 0x11a4, 0xffffffff, 0x1224, 0xffffffff,
    0x09a8, 0x00000000, 0x0a28, 0x00000000, 0x0da8, 0xffffffff, 0x0ea8, 0xffffffff, 0x0e28, 0xffffffff, 0x0f28, 0xffffffff, 0x11a8, 0xffffffff, 0x1228, 0xffffffff,
    0x09ac, 0x00000000, 0x0a2c, 0x00000000, 0x0dac, 0xffffffff, 0x0eac, 0xffffffff, 0x0e2c, 0xffffffff, 0x0f2c, 0xffffffff, 0x11ac, 0xffffffff, 0x122c, 0xffffffff,
    0x09b0, 0x00000000, 0x0a30, 0x00000000, 0x0db0, 0xffffffff, 0x0eb0, 0xffffffff, 0x0e30, 0xffffffff, 0x0f30, 0xffffffff, 0x11b0, 0xffffffff, 0x1230, 0xffffffff,
    0x09b4, 0x00000000, 0x0a34, 0x00000000, 0x0db4, 0xffffffff, 0x0eb4, 0xffffffff, 0x0e34, 0xffffffff, 0x0f34, 0xffffffff, 0x11b4, 0xffffffff, 0x1234, 0xffffffff,
    0x09b8, 0x00000000, 0x0a38, 0x00000000, 0x0db8, 0xffffffff, 0x0eb8, 0xffffffff, 0x0e38, 0xffffffff, 0x0f38, 0xffffffff, 0x11b8, 0xffffffff, 0x1238, 0xffffffff,
    0x09bc, 0x00000000, 0x0a3c, 0x00000000, 0x0dbc, 0xffffffff, 0x0ebc, 0xffffffff, 0x0e3c, 0xffffffff, 0x0f3c, 0xffffffff, 0x11bc, 0xffffffff, 0x123c, 0xffffffff,
    0x09c0, 0x00000000, 0x0a40, 0x00000000, 0x0dc0, 0xffffffff, 0x0ec0, 0xffffffff, 0x0e40, 0xffffffff, 0x0f40, 0xffffffff, 0x11c0, 0xffffffff, 0x1240, 0xffffffff,
    0x09c4, 0x00000000, 0x0a44, 0x00000000, 0x0dc4, 0xffffffff, 0x0ec4, 0xffffffff, 0x0e44, 0xffffffff, 0x0f44, 0xffffffff, 0x11c4, 0xffffffff, 0x1244, 0xffffffff,
    0x09c8, 0x00000000, 0x0a48, 0x00000000, 0x0dc8, 0xffffffff, 0x0ec8, 0xffffffff, 0x0e48, 0xffffffff, 0x0f48, 0xffffffff, 0x11c8, 0xffffffff, 0x1248, 0xffffffff,
    0x09cc, 0x00000000, 0x0a4c, 0x00000000, 0x0dcc, 0xffffffff, 0x0ecc, 0xffffffff, 0x0e4c, 0xffffffff, 0x0f4c, 0xffffffff, 0x11cc, 0xffffffff, 0x124c, 0xffffffff,
    0x09d0, 0x00000000, 0x0a50, 0x00000000, 0x0dd0, 0xffffffff, 0x0ed0, 0xffffffff, 0x0e50, 0xffffffff, 0x0f50, 0xffffffff, 0x11d0, 0xffffffff, 0x1250, 0xffffffff,
    0x09d4, 0x00000000, 0x0a54, 0x00000000, 0x0dd4, 0xffffffff, 0x0ed4, 0xffffffff, 0x0e54, 0xffffffff, 0x0f54, 0xffffffff, 0x11d4, 0xffffffff, 0x1254, 0xffffffff,
    0x09d8, 0x00000000, 0x0a58, 0x00000000, 0x0dd8, 0xffffffff, 0x0ed8, 0xffffffff, 0x0e58, 0xffffffff, 0x0f58, 0xffffffff, 0x11d8, 0xffffffff, 0x1258, 0xffffffff,
    0x09dc, 0x00000000, 0x0a5c, 0x00000000, 0x0ddc, 0xffffffff, 0x0edc, 0xffffffff, 0x0e5c, 0xffffffff, 0x0f5c, 0xffffffff, 0x11dc, 0xffffffff, 0x125c, 0xffffffff,
    0x09e0, 0x00000000, 0x0a60, 0x00000000, 0x0de0, 0xffffffff, 0x0ee0, 0xffffffff, 0x0e60, 0xffffffff, 0x0f60, 0xffffffff, 0x11e0, 0xffffffff, 0x1260, 0xffffffff,
    0x09e4, 0x00000000, 0x0a64, 0x00000000, 0x0de4, 0xffffffff, 0x0ee4, 0xffffffff, 0x0e64, 0xffffffff, 0x0f64, 0xffffffff, 0x11e4, 0xffffffff, 0x1264, 0xffffffff,
    0x09e8, 0x00000000, 0x0a68, 0x00000000, 0x0de8, 0xffffffff, 0x0ee8, 0xffffffff, 0x0e68, 0xffffffff, 0x0f68, 0xffffffff, 0x11e8, 0xffffffff, 0x1268, 0xffffffff,
    0x09ec, 0x00000000, 0x0a6c, 0x00000000, 0x0dec, 0xffffffff, 0x0eec, 0xffffffff, 0x0e6c, 0xffffffff, 0x0f6c, 0xffffffff, 0x11ec, 0xffffffff, 0x126c, 0xffffffff,
    0x09f0, 0x00000000, 0x0a70, 0x00000000, 0x0df0, 0xffffffff, 0x0ef0, 0xffffffff, 0x0e70, 0xffffffff, 0x0f70, 0xffffffff, 0x11f0, 0xffffffff, 0x1270, 0xffffffff,
    0x09f4, 0x00000000, 0x0a74, 0x00000000, 0x0df4, 0xffffffff, 0x0ef4, 0xffffffff, 0x0e74, 0xffffffff, 0x0f74, 0xffffffff, 0x11f4, 0xffffffff, 0x1274, 0xffffffff,
    0x09f8, 0x00000000, 0x0a78, 0x00000000, 0x0df8, 0xffffffff, 0x0ef8, 0xffffffff, 0x0e78, 0xffffffff, 0x0f78, 0xffffffff, 0x11f8, 0xffffffff, 0x1278, 0xffffffff,
    0x09fc, 0x00000000, 0x0a7c, 0x00000000, 0x0dfc, 0xffffffff, 0x0efc, 0xffffffff, 0x0e7c, 0xffffffff, 0x0f7c, 0xffffffff, 0x11fc, 0xffffffff, 0x127c, 0xffffffff
};

static long xabr_read_regs(struct xabr_scaler *tabr, struct core_desc *core)
{
    u32 i, reg_cnt;
    u32 idx  = core->id;
    u32 type = core->type;

    if (idx  >= MAX_SLICE_NUM              ||
        type >= XABR_CORE_MAX              ||
        !tabr->subsys[idx][type].base_addr ||
        !tabr->subsys[idx][type].hwreg) {
        sn_pri(tabr->tdev, SN_ERR, "xabr: %s failed core_id: [%d][%d] base_addr: %uld\n",
               __func__, idx,type,tabr->subsys[idx][type].base_addr);
        return -EINVAL;
    }
    reg_cnt = core->size / sizeof(struct reg_desc);
    /* read specific registers from hardware */
    for (i = 0; i < reg_cnt; i++) {
         u32 val = readl(tabr->subsys[idx][type].hwreg + ((struct reg_desc *)core->regs)[i].id);
         ((struct reg_desc *)core->regs)[i].val = val;
    }
    return 0;
}

static long xabr_write_regs(struct xabr_scaler *tabr, struct core_desc *core)
{
    //long ret;
    u32 i, reg_cnt;
    u32 idx  = core->id;
    u32 type = core->type;
    struct reg_desc *regs_info;

    if (idx  >= MAX_SLICE_NUM              ||
        type >= XABR_CORE_MAX              ||
        !tabr->subsys[idx][type].base_addr ||
        !tabr->subsys[idx][type].hwreg) {
        sn_pri(tabr->tdev, SN_ERR, "xabr: %s failed core_id: [%d][%d] base_addr: %uld\n",
               __func__, idx,type,tabr->subsys[idx][type].base_addr);
        return -EINVAL;
    }

    regs_info = (struct reg_desc *)tabr->subsys[idx][type].shadow;
    memcpy(regs_info, core->regs,  core->size);

    reg_cnt = core->size / sizeof(struct reg_desc);
    for (i = 0; i < reg_cnt; i++) {
        writel(regs_info[i].val,  tabr->subsys[idx][type].hwreg + regs_info[i].id);
    }
    return 0;
}

/*****************************************************************************
 * Disable all Read/Write Streams for DEC400
*****************************************************************************/
static int disable_dec400_all_streams(struct xabr_scaler *tabr, int slice_id, int core_id, void *reg_map_defaults, int regmap_defaults_size)
{
    int ret = 0;
    struct reg_desc reginfo;
    struct core_desc core;

    reginfo.id  = XABR_DECIntrAcknowledgeEx2;
    reginfo.val = -1;
    core = (struct core_desc){ .id = slice_id, .type = core_id, .regs = (u32*) &reginfo, .size = sizeof(reginfo), .reg_id = 0};
    xabr_read_regs(tabr, &core);

    reginfo.id  = XABR_DECIntrAcknowledgeEx;
    reginfo.val = -1;
    xabr_read_regs(tabr, &core);

    core = (struct core_desc){ .id = slice_id, .type = core_id, .regs = (u32*) reg_map_defaults, .size = regmap_defaults_size, .reg_id = 0};
    ret = xabr_write_regs(tabr, &core);

    return ret;
}

/*****************************************************************************
 * Flush DEC400 Stream
*****************************************************************************/
static int flush_dec400(struct xabr_scaler *tabr, int slice_id, int core_id)
{
    int ret;
    struct reg_desc reginfo;
    struct core_desc core;
    int timeout;

    reginfo.id  = XABR_DECStatus;
    reginfo.val = -1;
    core = (struct core_desc){ .id = slice_id, .type = core_id, .regs = (u32*) &reginfo, .size = sizeof(reginfo), .reg_id = 0};
    //check DEC400 idle
    timeout = TIMEOUT_COUNT;
    xabr_read_regs(tabr, &core);
    while((reginfo.val & 0x00000001) != 0x00000001) {
      usleep_range(100, 100);
      timeout--;
      if (timeout == 0) {
        printk("[%d]check idle timeout!\n", core_id);
        return -1;
      }
      xabr_read_regs(tabr, &core);
    }

    //enable flush
    reginfo.id  = XABR_DECControl;
    reginfo.val = -1;
    xabr_read_regs(tabr, &core);
    reginfo.val |= 1;
    ret = xabr_write_regs(tabr, &core);
    if(ret) {
        sn_pri(tabr->tdev, SN_ERR, "xsc_dec400_flush: failed to write regs\n");
        return ret;
    }

    // Write streams
    reginfo.id  = XABR_DECIntrAcknowledgeEx;
    reginfo.val = -1;
    //check DEC400 idle
    timeout = TIMEOUT_COUNT;
    xabr_read_regs(tabr, &core);
    while(reginfo.val != 0x00000000) {
      usleep_range(100, 100);
      timeout--;
      if (timeout == 0) {
        printk("[%d]XABR_DECIntrAcknowledgeEx timeout!\n", core_id);
        return -1;
      }
      xabr_read_regs(tabr, &core);
    }

    // Read streams
    reginfo.id  = XABR_DECIntrAcknowledgeEx2;
    reginfo.val = -1;
    //check DEC400 idle
    timeout = TIMEOUT_COUNT;
    xabr_read_regs(tabr, &core);
    while(reginfo.val != 0x00000000) {
      usleep_range(100, 100);
      timeout--;
      if (timeout == 0) {
        printk("[%d]XABR_DECIntrAcknowledgeEx2 timeout!\n", core_id);
        return -1;
      }
      xabr_read_regs(tabr, &core);
    }
    return 0;
}

static int enable_dec400(struct xabr_scaler *tabr, int slice_id, int core_id, void *reg_map, int regmap_size)
{
    int ret = 0;
    struct reg_desc reginfo;
    struct core_desc core;

    reginfo.id  = XABR_DECIntrAcknowledgeEx2;
    reginfo.val = -1;
    core = (struct core_desc){ .id = slice_id, .type = core_id, .regs = (u32*) &reginfo, .size = sizeof(reginfo), .reg_id = 0};
    xabr_read_regs(tabr, &core);

    reginfo.id  = XABR_DECIntrAcknowledgeEx;
    reginfo.val = -1;
    xabr_read_regs(tabr, &core);

    core = (struct core_desc){ .id = slice_id, .type = core_id, .regs = (u32*) reg_map, .size = regmap_size, .reg_id = 0};
    ret = xabr_write_regs(tabr, &core);

    return ret;
}

static __inline void do_patch_dec400(struct sn_tranx_t *tdev, struct file* filp, uint32_t *addr, uint32_t offset)
{
    struct reg_desc* reg_desc = (struct reg_desc*) (addr+offset/sizeof(uint32_t));
    uint64_t in_handle = ((uint64_t)reg_desc[1].val << 32) | reg_desc[0].val;

    // patch the buffer pointer if it is valid
    if(in_handle && in_handle != 0xFFFFFFFFFFFFFFFF)
    {
        uint64_t out_handle = sn_mem_osal_translate_handle(tdev, filp, in_handle, 0);
        reg_desc[0].val = out_handle;
        reg_desc[1].val = out_handle >> 32;
        // printk("in_handle 0x%llx\t", in_handle);
        // printk("out_handle 0x%llx", out_handle);
    }
    return;
}

static int patch_dec400_config_data(struct sn_tranx_t *tdev, struct file* filp, uint32_t *addr, uint32_t start_offset)
{
    int i;
    uint32_t offset = start_offset;
    // Read streams
    for(i=0; i<MAX_READ_STREAM; i++)
    {
        do_patch_dec400(tdev, filp, addr, offset+8);
        do_patch_dec400(tdev, filp, addr, offset+24);
        do_patch_dec400(tdev, filp, addr, offset+48);
        offset += 8*8;
    }
    // Write streams
    for(i=0; i<MAX_WRITE_STREAM; i++)
    {
        do_patch_dec400(tdev, filp, addr, offset+8);
        do_patch_dec400(tdev, filp, addr, offset+24);
        do_patch_dec400(tdev, filp, addr, offset+48);
        offset += 8*8;
    }

    return 0;
}
////////////////////////////////////////////////////////////////////////////////
////////////////////////// DEC 400 functionality ends //////////////////////////
////////////////////////////////////////////////////////////////////////////////

static struct xabr_core_desc* get_core_desc(struct xabr_scaler *tabr, int *num_cores)
{
    struct xabr_core_desc *cp = NULL;
    int ncores = 0;

    switch(tabr->tdev->pf_vf_mode)
    {
        case PF_MODE:
            if (tabr->tdev->vf_index != PF_INDEX) {
                sn_pri(tabr->tdev, SN_ERR, "(%s)vf_index incorrect in PF_MODE\n", __func__);
            } else {
                ncores = sizeof(pf_core_array)/sizeof(pf_core_array[0]);
                cp     = &pf_core_array[0][0];
            }
            break;

        case ONE_VF_MODE:
            if (tabr->tdev->vf_index != VF1_INDEX) {
                sn_pri(tabr->tdev, SN_ERR, "(%s)vf_index incorrect in ONE_VF_MODE\n", __func__);
            } else {
                ncores = sizeof(onevf_core_array)/sizeof(onevf_core_array[0]);
                cp     = &onevf_core_array[0][0];
            }
            break;

        default:
            sn_pri(tabr->tdev, SN_ERR, "(%s)incorrect pf_vf_mode (%d)\n", __func__, tabr->tdev->pf_vf_mode);
            break;
    }

    *num_cores = ncores;
    return cp;
}

static int init_core_array(struct xabr_scaler *tabr)
{
    int i, j, irq;
    u32 total_io_size = 0;
    struct xabr_core_desc *cd = NULL;
    int num_cores = 0;

    //get core description table for pf/vf
    cd = get_core_desc(tabr, &num_cores);
    if (!num_cores) {
        sn_pri(tabr->tdev, SN_ERR, "xabr: failed to get num_cores\n");
        return 0;
    }

    tabr->num_cores = num_cores;
    for (i = 0; i < MAX_SLICE_NUM; ++i) {
        for (j = 0; j < tabr->num_cores; j++) {

            tabr->subsys[i][j].base_addr = (cd+i*XABR_CORE_MAX+j)->base;
            tabr->subsys[i][j].iosize    = (cd+i*XABR_CORE_MAX+j)->iosize;
            irq                          = (cd+i*XABR_CORE_MAX+j)->irq;
            if (irq != -1)
                tabr->subsys[i][j].irq   = pci_irq_vector(tabr->tdev->pdev, irq);

            total_io_size               += tabr->subsys[i][j].iosize;
            tabr->subsys[i][j].hwreg     = 0;
            tabr->subsys[i][j].name      = (cd+i*XABR_CORE_MAX+j)->name;

            sn_pri(tabr->tdev, SN_DBG, "xabr:slice[%d][%d]: base=%x  iosize=%5d  tot_size=%5d  core_irq id: %4d  pci_irq: %4d\n",
                   i, j, tabr->subsys[i][j].base_addr, tabr->subsys[i][j].iosize, total_io_size,
                   irq, tabr->subsys[i][j].irq);
        }
    }

    return total_io_size;
}

static int reserve_io(struct xabr_scaler *tabr)
{
    int i, j;

    for (i = 0; i < MAX_SLICE_NUM; i++) {
        for (j = 0; j < tabr->num_cores; j++) {
            if (tabr->subsys[i][j].iosize) {
                sn_pri(tabr->tdev, SN_DBG, "xabr:slice[%d][%d]: bar2_virt=0x%x  ipbase=0x%lx, iosize=%d\n", i, j,
                       tabr->tdev->bar2_virt, tabr->subsys[i][j].base_addr, tabr->subsys[i][j].iosize);

                tabr->subsys[i][j].hwreg = tabr->tdev->bar2_virt + tabr->subsys[i][j].base_addr;

                if (tabr->subsys[i][j].hwreg == NULL) {
                    sn_pri(tabr->tdev, SN_ERR, "xabr:slice[%d][%d]: failed to ioremap HW %d regs\n", i,j,j);
                    return -EBUSY;
                }
                sn_pri(tabr->tdev, SN_DBG, "xabr:slice[%d][%d]: hwreg(bar2_virt+base)=0x%x  reg[0]=0x%08x\n",
                       i, j, tabr->subsys[i][j].hwreg, readl(tabr->subsys[i][j].hwreg));
            }
        }
    }
    return 0;
}

static void record_perf_event(struct xabr_scaler *tabr, u32 slice, SN_PERF_TYPE type)
{
    sn_perf_event event;

    event.slice     = slice;
    event.ipId      = SN_PERF_ID_XABR;
    event.unitId    = 0;
    event.type      = type;
    event.name      = 0;
    sn_perf_record(tabr->perf_handle, &event);
}

static int check_xabr_irq(struct xabr_scaler *tabr, u32 slice, u32 *irq_status)
{
    unsigned long flags;
    volatile u8 *hwregs;
    int i,rdy = 0;

    spin_lock_irqsave(&tabr->irq_lock, flags);

    if (tabr->subsys[slice][XABR_CORE_HW].irq_recvd) {
        /* reset the wait condition(s) */
        tabr->subsys[slice][XABR_CORE_HW].irq_recvd = 0;
        rdy = 1;

        hwregs = tabr->subsys[slice][XABR_CORE_HW].hwreg;
        /* Decode IRQ Source
            - check if irq was WDT error
            - check if irq was CRC error
            - check if irq was frame done
        */
        if((tabr->irq_flags[slice][wd_ctl]  & 0x01) && (tabr->irq_flags[slice][wd_data] & 0x01)) {
            irq_status[0] = IRQ_SRC_WDT;
        } else if((tabr->irq_flags[slice][crc_ctl]  & 0x01) && (tabr->irq_flags[slice][crc_data] & 0x01)) {
            irq_status[0] = IRQ_SRC_CRC;
        } else if(tabr->irq_flags[slice][ap_done] & 0x02) {
            irq_status[0] = IRQ_SRC_AP_DONE;
        } else {
            irq_status[0] = IRQ_SRC_INVALID;
        }

        //add cycle count
        for (i=0; i<MAX_CHANNELS; ++i) {
            irq_status[i+1] = tabr->cycle_cnt[slice][i];
            //accumulate cycle counts
            tabr->acc_cycle_cnt[slice] += tabr->cycle_cnt[slice][i];
        }

        //clear shadow irq flags
        for (i=0; i<NUM_IRQ_FLAGS; ++i) {
            tabr->irq_flags[slice][i] = 0;
        }

        //unmask interrupt for slice
        writel(0x01, hwregs + XABR_SC_ADDR_GIE);
    }

    spin_unlock_irqrestore(&tabr->irq_lock, flags);
    return rdy;
}

static long wait_xabr_ready(struct xabr_scaler *tabr,
                            u32 slice,
                            u32 timeout_sec,
                            u32 *irq_status)
{
    long ret;

    ret = wait_event_interruptible_timeout(tabr->wait_queue,
                                           check_xabr_irq(tabr, slice, irq_status),
                                           timeout_sec*HZ);

    if(!ret) {
        //timeout elapsed
        sn_pri(tabr->tdev, SN_ERR, "xabr: wait_for_done timed out\n");
    } else if (ret > 0) {
        ret = 1; //core ready
    } else { //-ERESTARTSYS
        sn_pri(tabr->tdev, SN_DBG,
            "xabr: wait_event_interruptible interrupted\n", ret);
    }
    return ret;
}

static irqreturn_t xabr_dec400_isr(int irq, void *dev_id)
{
    struct xabr_scaler *tabr = (struct xabr_scaler *) dev_id;

    sn_pri(tabr->tdev, SN_ERR,
            "xabr: xabr dec400 irq not supported yet\n");
    return IRQ_RETVAL(0);
}

static irqreturn_t xabr_scaler_isr(int irq, void *dev_id)
{
    unsigned long flags;
    unsigned int handled = 0;
    int j,slice = 0;
    volatile u8 *hwregs = NULL;
    int regid, idx;
    u32 int_shadow_isr_off = S1_INT_SD_XABR_ISR;
    u32 reg_read = 0;

    struct xabr_scaler *tabr = (struct xabr_scaler *) dev_id;

    spin_lock_irqsave(&tabr->irq_lock, flags);

    //detrmine slice index from irq#
    if(irq == tabr->subsys[0][XABR_CORE_HW].irq) {
        slice = 0;
        int_shadow_isr_off = S1_INT_SD_XABR_ISR;
    } else if(irq == tabr->subsys[1][XABR_CORE_HW].irq) {
        slice = 1;
        int_shadow_isr_off = S2_INT_SD_XABR_ISR;
    } else {
        sn_pri(tabr->tdev, SN_ERR, "xabr: error:: irq %d not supported \n", irq);
        goto end;
    }
    hwregs = tabr->subsys[slice][XABR_CORE_HW].hwreg;

    //zsp generates spurious interrupts for slice 1 that needs to be filtered here
    reg_read = readl(hwregs + XABR_SC_ADDR_ISR);
    if (reg_read != 1) {
        //sn_pri(tabr->tdev, SN_INF, "xabr: warn:: unknown IRQ_(%d)_src %d recvd on slice %d \n", irq, reg_read, slice);
        goto end;
    }
    //only ap_done is the valid irq source

    //read irq source registers into shadow regs
    tabr->irq_flags[slice][wd_ctl]   = readl(hwregs + XABR_SC_ADDR_CTL_WDT_ERROR);
    tabr->irq_flags[slice][wd_data]  = readl(hwregs + XABR_SC_ADDR_DATA_WDT_ERROR);
    tabr->irq_flags[slice][crc_ctl]  = readl(hwregs + XABR_SC_ADDR_CTL_CRC_ERROR);
    tabr->irq_flags[slice][crc_data] = readl(hwregs + XABR_SC_ADDR_DATA_CRC_ERROR);
    tabr->irq_flags[slice][ap_done]  = readl(hwregs + XABR_SC_ADDR_AP_CTRL);

    //read performance counters (skip unused registers in middle)
    idx = 0;
    for (j=0; j<MAX_CHANNELS+1; ++j) {
        regid = (XABR_SC_ADDR_CYCLE_CNT_0+j*4);
        if(regid == 0xcc) {//check for unused register in middle
            continue;
        }
        tabr->cycle_cnt[slice][idx]  = readl(hwregs + regid);
        ++idx;
    }
    handled++;
    tabr->subsys[slice][XABR_CORE_HW].irq_recvd = 1;
    //Mask interrupt line for the slice
    writel(0x00, hwregs + XABR_SC_ADDR_GIE);
    writel(0x01, hwregs + XABR_SC_RESET_PERF_COUNTER);
    //if done_irq clear the ISR
    if (tabr->irq_flags[slice][ap_done] & 0x02) {
        writel(0x01, hwregs + XABR_SC_ADDR_ISR);
    }

    //irq decoding done in bottom half
end:
    spin_unlock_irqrestore(&tabr->irq_lock, flags);
    if(handled)
        wake_up_interruptible_all(&tabr->wait_queue);

    return IRQ_RETVAL(handled);
}

static int get_core_idle(struct xabr_scaler *tabr, struct file *filp, u32 core_id, int *is_available)
{
    int ret = 0;
    int slice_mask = core_id; //core_id>>8 & 0xFF; //0: slice_0, 1: slice_1: 2: Any
    int num_slices = ((slice_mask > 1) ? 2 : 1);
    int slice_idx  = ((num_slices > 1) ? 0 : slice_mask);
    int i;

    spin_lock(&tabr->reserve_lock);
    for (i=0; i<num_slices; ++i) {
        if (!tabr->isCoreLocked[slice_idx]) {
            *is_available                 = slice_idx;
            tabr->isCoreLocked[slice_idx] = 1;
            tabr->filp[slice_idx]         = filp;
            ret = 1;
            break;
        }
        ++slice_idx;
    }
    spin_unlock(&tabr->reserve_lock);
    return ret;
}

static long xabr_reserve_core(struct xabr_scaler *tabr,
                              struct file *filp,
                              u32  core_id,
                              u32  timeout_sec,
                              int  *lock_status)
{
    long ret;

    ret = down_interruptible(&tabr->reserve_semaphore);
    if(ret) {
        // semaphore acquisition failed: -EINTR if interrupted by a signal, -ETIME if a timeout occurred
        sn_pri(tabr->tdev, SN_ERR, "xabr: failed to acquire semaphore (interrupted or timeout)\n");
        return ret;
    }
    ret = get_core_idle(tabr, filp, core_id, lock_status);
    if(!ret) {
        sn_pri(tabr->tdev, SN_ERR, "xabr: failed to reserve core\n");
        return ret;
    }
    if (tabr->perf_enable) {
        record_perf_event(tabr, *lock_status, SN_PERF_TYPE_START);
    }
    return ret;
}

static long xabr_release_core(struct xabr_scaler *tabr, u32 slice)
{
    if (slice >= MAX_SLICE_NUM) {
        sn_pri(tabr->tdev, SN_ERR, "xabr: %s invalid slice id %d\n",__func__, slice);
        return -EINVAL;
    }

    //update core status to idle and clear lock state
    spin_lock(&tabr->reserve_lock);
    tabr->isCoreLocked[slice] = 0;
    tabr->filp[slice] = NULL;
    spin_unlock(&tabr->reserve_lock);

    if (tabr->perf_enable) {
        record_perf_event(tabr, slice, SN_PERF_TYPE_STOP);
    }

    //signal waiting processes
    up(&tabr->reserve_semaphore);
    return 0;
}

static int xabr_subsys_reset_keep(struct xabr_scaler *tabr, u32 slice)
{
    u32 val;
    unsigned int loop=500;
    struct sys_ctrl_cfg_regs *psub;
    struct sys_ctrl_cfg_regs abr_sys_cfg[MAX_SLICE_NUM] = {
        [0] = {"xabr_0", S1_SYS_CON_OFF, SYS_CTL_XABR,   0x24, 0x1, 0x124, 0x1, 0x71c, 0x1, 0x2, 0x52c, 0x1, 0x20},
        [1] = {"xabr_1", S2_SYS_CON_OFF, SYS_CTL_XABR,   0x24, 0x1, 0x124, 0x1, 0x71c, 0x1, 0x2, 0x52c, 0x1, 0x20}
    };

    if (slice >= MAX_SLICE_NUM) {
        sn_pri(tabr->tdev, SN_ERR, "xabr: soft reset requested for slice_id 0x%x not supported\n", slice);
        return EINVAL;
    }
    psub = &abr_sys_cfg[slice];

    /* ---- Assert Soft Reset ----- */
    // adb_setting
    val = readl(tabr->tdev->bar2_virt + psub->slice_off + psub->adb_setting);
    writel(val&~(psub->adb_setting_clear_bits), tabr->tdev->bar2_virt + psub->slice_off + psub->adb_setting);

    //poll b5, until 0
    val = readl(tabr->tdev->bar2_virt + psub->slice_off + psub->adb_setting);
    while(val & psub->adb_setting_polling_bits) {
        --loop;
        msleep(1);
        val = readl(tabr->tdev->bar2_virt + psub->slice_off + psub->adb_setting);
        if (!loop) {
            sn_pri(tabr->tdev,SN_ERR,"%s: system controller soft reset error!\n",psub->name);
            break;
        }
    }
    if(!loop)
        return EFAULT;

    //clr b0 clk_ctrl
    val = readl(tabr->tdev->bar2_virt + psub->slice_off + psub->clock_ctrl);
    writel(val&~(psub->clock_ctrl_bits), tabr->tdev->bar2_virt + psub->slice_off + psub->clock_ctrl);

    //clr b0 sys_reset
    val = readl(tabr->tdev->bar2_virt + psub->slice_off + psub->sys_reset);
    writel(val&~(psub->sys_reset_bits), tabr->tdev->bar2_virt + psub->slice_off + psub->sys_reset);

    return 0;
}

static int xabr_subsys_reset_release(struct xabr_scaler *tabr, u32 slice)
{
    u32 val;
    unsigned int loop=500;
    struct sys_ctrl_cfg_regs *psub;
    struct sys_ctrl_cfg_regs abr_sys_cfg[MAX_SLICE_NUM] = {
        [0] = {"xabr_0", S1_SYS_CON_OFF, SYS_CTL_XABR,   0x24, 0x1, 0x124, 0x1, 0x71c, 0x1, 0x2, 0x52c, 0x1, 0x20},
        [1] = {"xabr_1", S2_SYS_CON_OFF, SYS_CTL_XABR,   0x24, 0x1, 0x124, 0x1, 0x71c, 0x1, 0x2, 0x52c, 0x1, 0x20}
    };

    if (slice >= MAX_SLICE_NUM) {
        sn_pri(tabr->tdev, SN_ERR, "xabr: soft reset requested for slice_id %d not supported\n", slice);
        return EINVAL;
    }
    psub = &abr_sys_cfg[slice];

    /* ---- Release Soft Reset ----- */
    //set b0 clk_ctrl
    val = readl(tabr->tdev->bar2_virt + psub->slice_off + psub->clock_ctrl);
    writel(val|psub->clock_ctrl_bits, tabr->tdev->bar2_virt + psub->slice_off + psub->clock_ctrl);

    //set b0 sys_reset
    val = readl(tabr->tdev->bar2_virt + psub->slice_off + psub->sys_reset);
    writel(val|psub->sys_reset_bits, tabr->tdev->bar2_virt + psub->slice_off + psub->sys_reset);

    // adb_setting
    val = readl(tabr->tdev->bar2_virt + psub->slice_off + psub->adb_setting);
    writel(val|psub->adb_setting_clear_bits, tabr->tdev->bar2_virt + psub->slice_off + psub->adb_setting);

    //poll b5, until 1
    loop = 500;
    val = readl(tabr->tdev->bar2_virt + psub->slice_off + psub->adb_setting);
    while((val & psub->adb_setting_polling_bits) == 0x0) {
        --loop;
        msleep(1);
        val = readl(tabr->tdev->bar2_virt + psub->slice_off + psub->adb_setting);
        if (!loop) {
            sn_pri(tabr->tdev,SN_ERR,"%s: system controller soft reset error!\n",psub->name);
            break;
        }
    }
    if(!loop)
        return EFAULT;

    //clear core reservation flags
    xabr_release_core(tabr, slice);

    //Re-enable the interrupts

    //XABR_CORE_HW enable crc, wdt
    writel(0x03,  tabr->subsys[slice][XABR_CORE_HW].hwreg + XABR_SC_ADDR_CRC_WDT);
    //XABR_CORE_HW enable interrupts
    writel(0x01,  tabr->subsys[slice][XABR_CORE_HW].hwreg + XABR_SC_ADDR_GIE);
    writel(0x01,  tabr->subsys[slice][XABR_CORE_HW].hwreg + XABR_SC_ADDR_IER);

    return 0;
}

static __inline void do_patch(struct sn_tranx_t *tdev, struct file* filp, volatile uint64_t *addr, uint32_t offset)
{
    addr[offset / 8] = sn_mem_osal_translate_handle(tdev, filp, addr[offset / 8], 0);
    return;
}

static int32_t patch_xabr_cmd_buffer(struct sn_tranx_t *tdev, struct file* filp, uint32_t *data)
{
    uint32_t channel;
    uint64_t desc_phy_addr     = (((uint64_t) data[1]) << 32) | data[0];
    uint32_t src_planes_all_ch = data[2];
    uint32_t dst_planes_all_ch = data[3];
    uint32_t ch_config_offset  = data[4];
    uint32_t ch_config_size    = data[5];
    uint32_t csc_coef_offset   = data[6];
    uint32_t h_coef_offset     = data[7];
    uint32_t v_coef_offset     = data[8];
    uint32_t src_buf_offset    = data[9];
    uint32_t dst_buf_offset    = data[10];
    uint32_t cmd_desc_size     = data[11];
    volatile uint32_t crc_val;
    struct xabr_scaler* tabr   = tdev->modules[SN_MODULE_XABR];

    volatile uint64_t* desc_ker_vir_addr = (uint64_t*) sn_mem_osal_translate_mmio(tdev, desc_phy_addr);
    uint32_t num_channels       = ((uint32_t*)desc_ker_vir_addr)[0];

    for(channel=0; channel<num_channels; channel++) {
        int32_t plane, src_num_planes, dst_num_planes;
        uint32_t src_offset, dst_offset;
        uint32_t offset = ch_config_offset + channel*ch_config_size;
        do_patch(tdev, filp, desc_ker_vir_addr, offset+h_coef_offset);
        do_patch(tdev, filp, desc_ker_vir_addr, offset+v_coef_offset);
        do_patch(tdev, filp, desc_ker_vir_addr, offset+csc_coef_offset);
        src_offset = offset + src_buf_offset;
        src_num_planes = (src_planes_all_ch>>2*channel)&0x3;
        for (plane=0; plane<src_num_planes; plane++) {
            do_patch(tdev, filp, desc_ker_vir_addr, src_offset);
            src_offset += 8;
        }
        dst_offset = offset + dst_buf_offset;
        dst_num_planes = (dst_planes_all_ch>>2*channel)&0x3;
        for (plane=0; plane<dst_num_planes; ++plane) {
            do_patch(tdev, filp, desc_ker_vir_addr, dst_offset);
            dst_offset += 8;
        }
    }

    // compute CRC and update in cmd buf
    crc_val = crc32(~0, (const void *)desc_ker_vir_addr, cmd_desc_size);
    ((volatile uint32_t*)desc_ker_vir_addr)[cmd_desc_size / 4] = crc_val;
    wmb(); //ensure crc write to dev memory is complete (pci writes being cached)

    //read back crc and compare
    if (crc_val != readl(desc_ker_vir_addr + cmd_desc_size / 8)) {
        sn_pri(tabr->tdev, SN_ERR, "crc mismatch in mmio region \n");
    }

    return 0;
}

static int xabr_exec_cmd_run(struct xabr_scaler *tabr, sn_osal_work* work)
{
    osal_command* command = &work->cmd;
    int lock_id = -1;
    int ret;
    struct reg_desc hwreg[4];
    struct core_desc core;
    bool decompress_en, out_compress_en;
    uint32_t dec400_reg_desc_size;
    uint32_t wd_timer;

    if (work->numResp < MAX_CHANNELS + 1 + 1) {
        return -EFAULT;
    }
    //validate command payload
    if (work->cmd.numData != (14 + (work->data[12] ? 2 : 0))) {
        sn_pri(tabr->tdev,SN_ERR,"xabr - start command payload size(%d words) not valid %d\n",command->numData, (14 + (work->data[12] ? 2 : 0)));
        return -EFAULT;
    }

    //default response - error
    work->numResp = 1;

    ret = xabr_reserve_core(tabr, work->filp, command->slice_id, XABR_RESERVE_CORE_TIMEOUT_SEC, &lock_id);
    if((ret <= 0 ) || (lock_id < 0)) {
        ret = 1;
        sn_pri(tabr->tdev,SN_ERR,"xabr - unable to reserve slice %d (%d)\n",command->slice_id, lock_id);
        work->data[0] = IRQ_SRC_INVALID;
        return ret;
    }

    //core is now locked - execute command
    sn_pri(tabr->tdev, SN_DBG, "xabr core %d locked, file pointer = %p. execute cmd %d\n", lock_id, work->filp, command->op_code);

    decompress_en        = work->data[12] & 0x1;
    out_compress_en      = (work->data[12] >> 1) & 0x1;
    dec400_reg_desc_size = work->data[13];

    // configure DEC400 if enabled
    if(decompress_en || out_compress_en) {
        uint64_t dec400_phy_addr = (((uint64_t) work->data[15]) << 32) | work->data[14];
        uint64_t* dec400_ker_vir_addr = (uint64_t*) sn_mem_osal_translate_mmio(tabr->tdev, dec400_phy_addr);
        patch_dec400_config_data(tabr->tdev, work->filp, (uint32_t *)dec400_ker_vir_addr, 80);
        ret = enable_dec400(tabr, lock_id, 1, (void*)dec400_ker_vir_addr, dec400_reg_desc_size);
        if(ret) {
            sn_pri(tabr->tdev,SN_ERR,"xabr:DEC400 - unable to enable DEC400\n");
            work->data[0] = IRQ_SRC_INVALID;
            return ret;
        }
    }

    //patch command buffer
    patch_xabr_cmd_buffer(tabr->tdev, work->filp, work->data);

    //write command block registers
    hwreg[0].id  = XABR_SC_ADDR_CMD_BLK_ADDR_LO;
    hwreg[0].val = work->data[0];

    hwreg[1].id  = XABR_SC_ADDR_CMD_BLK_ADDR_HI;
    hwreg[1].val = work->data[1];

    //Reload watchdog timer
    wd_timer     = ~0U; //880Hz clk = 1.136ns cycle * (0xFFFFFFFF<<2) = 1s timeout
    hwreg[2].id  = XABR_SC_ADDR_CRC_WDT;
    hwreg[2].val = (wd_timer << 2) | 0x03; //crc|wdt irq_en

    //start ip
    hwreg[3].id  = XABR_SC_ADDR_AP_CTRL;
    hwreg[3].val = 1;

    core.id     = lock_id;
    core.type   = XABR_CORE_HW;
    core.regs   = (u32 *)(hwreg);
    core.reg_id = 0;
    core.size   = sizeof(hwreg);
    ret = xabr_write_regs(tabr, &core);
    if(ret) {
        work->data[0] = IRQ_SRC_INVALID;
        sn_pri(tabr->tdev, SN_ERR, "xabr_cmd_run: failed to write regs\n");
        return ret;
    }

    //wait for completion
    sn_pri(tabr->tdev, SN_DBG, "xabr cmd executed. wait for completion\n");

    ret = wait_xabr_ready(tabr, lock_id, XABR_WATCHDOG_TIMEOUT_SEC, work->data);
    if(ret <= 0) {
        ret = 1;
        sn_pri(tabr->tdev, SN_ERR, "xabr_cmd_run: failed to execute wait\n");
        return ret;
    }

    work->data[MAX_CHANNELS + 1] = lock_id;
    work->numResp = MAX_CHANNELS+1+1;
    // response->header.slice_id = lock_id; // unused????
    sn_pri(tabr->tdev, SN_DBG, "xabr irq response %d\n", work->data[0]);

    if (out_compress_en) {
        flush_dec400(tabr, lock_id, 1);
    }
    if (decompress_en || out_compress_en) {
        disable_dec400_all_streams(tabr, lock_id, 1, (void*)dec400_reg_map_defaults, sizeof(dec400_reg_map_defaults));
    }
    //release the core
    sn_pri(tabr->tdev, SN_DBG, "xabr release core %d\n", lock_id);
    ret = xabr_release_core(tabr, lock_id);

    return ret;
}

static int xabr_soft_reset(struct xabr_scaler *tabr, u32 slice)
{
    xabr_subsys_reset_keep(tabr, slice);
    xabr_subsys_reset_release(tabr, slice);
    return 0;
}

static void osal_xabr_handler(struct work_struct* workPtr) {
    sn_osal_work* work = (sn_osal_work*) workPtr;
    struct sn_tranx_t *tdev  = work->tdev;
    struct xabr_scaler *tabr = tdev->modules[SN_MODULE_XABR];
    sn_pri(tdev, SN_DBG, "osal_xabr_handler(work_queue based): recvd cmd %d\n", work->cmd.op_code);
    switch(work->cmd.op_code) {
    case OSAL_ACCL_CMD_SCALER_RUN:
        if (xabr_exec_cmd_run(tabr, work)) {
            sn_pri(tdev, SN_ERR, "osal_xabr_handler: failed to execute run command\n");
        }
        break;
    // case 5555: // Used for OSAL test app
    //     work->numResp = 2;
    //     work->data[0] = 12345;
    //     work->data[1] = 67890;
    //     break;
    default:
        sn_pri(tdev, SN_ERR, "osal_xabr_handler: invalid cmd opcode %d\n", work->cmd.op_code);
        if (work->numResp > 0) {
            work->numResp = 1;
            work->data[0] = 0xdeadbeef;
        } else {
            work->numResp = -1;
        }
        break;
    }
    sn_osal_finish_work(work);
}

/*
 * Calculate the utilization of abr scaler in one second.
 * cycles_consumed/max_cycles is the utilization per slice.
 */
static void xabr_loading_timer_isr(struct timer_list *t)
{
    struct xabr_scaler *tabr = from_timer(tabr, t, loading_timer);
    const unsigned long max_cycles = 880*1e6; //880MHz clk
    uint32_t load;
    int i;

    for(i=0; i<MAX_SLICE_NUM; ++i) {
        tabr->loading[i].time_cnt_saved = tabr->acc_cycle_cnt[i];
        load = (tabr->loading[i].time_cnt_saved*100)/max_cycles;
        //cap load to 100
        load = (load > 100) ? 100 : load;
        sn_perf_load(tabr->perf_handle, i, load);
        tabr->acc_cycle_cnt[i] = 0;
    }
    //restart timer
    mod_timer(&tabr->loading_timer, t->expires + LOADING_TIME*HZ);
}

static int xabr_perf(struct sn_tranx_t* tdev, __u32 ipId, __u32 cmd, __u32 arg) {
    struct xabr_scaler* tabr = tdev->modules[SN_MODULE_XABR];

    tabr->perf_enable = (cmd == SN_PERF_CMD_START);
    return 0;
}

int xabr_scaler_init(struct sn_tranx_t *tdev)
{
    int i, j;
    int result = -1;
    struct xabr_scaler *tabr;
    int total_iosize;
    void *tmp_mem;
    const char *core_str[XABR_CORE_MAX] = {"core_scaler", "core_dec400"};

    tabr = (struct xabr_scaler *)kzalloc(sizeof(*tabr), GFP_KERNEL);
    if (!tabr) {
        sn_pri(tdev, SN_ERR, "xabr: kmalloc failed\n");
        return -ENOMEM;
    }

    tdev->modules[SN_MODULE_XABR] = tabr;
    tabr->tdev                    = tdev;
    total_iosize                  = init_core_array(tabr);
    if (!total_iosize) {
        sn_pri(tabr->tdev, SN_ERR, "xabr: total core count is zero\n");
        goto err_free_tabr;
    }

    init_waitqueue_head(&tabr->wait_queue);
    sema_init(&tabr->reserve_semaphore, tabr->num_cores);
    spin_lock_init(&tabr->irq_lock);
    spin_lock_init(&tabr->reserve_lock);

    result = reserve_io(tabr);
    if(result < 0) {
        sn_pri(tabr->tdev, SN_ERR, "xabr: reserve io failed\n");
        goto err_free_tabr;
    }

    tmp_mem = vzalloc(total_iosize);
    if (!tmp_mem) {
        sn_pri(tabr->tdev, SN_ERR, "xabr: malloc regs mem failed.\n");
        goto err_free_tabr;
    }

    tabr->regs_shadow = tmp_mem;
    for (i = 0; i < MAX_SLICE_NUM; i++) {
        for (j = 0; j < tabr->num_cores; ++j) {
            if (tabr->subsys[i][j].iosize) {
                tabr->subsys[i][j].shadow = tmp_mem;
                tmp_mem += tabr->subsys[i][j].iosize;
            }
        }
    }

    //register irq for all cores
    for (i = 0; i < MAX_SLICE_NUM; i++) {
        for (j = 0; j < tabr->num_cores; ++j) {
            if(tabr->subsys[i][j].irq > 0) {
                switch(j) {
                    case XABR_CORE_HW:
                        result = request_irq(tabr->subsys[i][j].irq, xabr_scaler_isr,
                                             IRQF_SHARED, tabr->subsys[i][j].name, (void *)tabr);
                        break;

                    case XABR_CORE_DEC400:
                        result = request_irq(tabr->subsys[i][j].irq, xabr_dec400_isr,
                                             IRQF_SHARED, tabr->subsys[i][j].name, (void *)tabr);
                        break;
                }
                if(result != 0) {
                    if(result == -EINVAL) {
                        sn_pri(tdev, SN_ERR, "xabr: Bad IRQ:%d or handler\n",
                               tabr->subsys[i][j].irq);
                    } else if(result == -EBUSY) {
                        sn_pri(tdev, SN_ERR, "xabr: IRQ:%d busy\n",
                               tabr->subsys[i][j].irq);
                    }
                    goto err_free_irq;
                } else {
                    sn_pri(tdev, SN_INF, "xabr:slice%d %s: pci_irq %d registered!\n",
                           i, core_str[j], tabr->subsys[i][j].irq);
                }
            } else {
                sn_pri(tdev, SN_INF, "xabr:slice%d %s: irq not in use!\n",
                       i,core_str[j]);
            }
        }
    }

    for (i= 0; i < MAX_SLICE_NUM; i++) {
        //XABR_CORE_HW enable crc, wdt
        writel(0x03,  tabr->subsys[i][XABR_CORE_HW].hwreg + XABR_SC_ADDR_CRC_WDT);
        //XABR_CORE_HW enable interrupts
        writel(0x01,  tabr->subsys[i][XABR_CORE_HW].hwreg + XABR_SC_ADDR_GIE);
        writel(0x01,  tabr->subsys[i][XABR_CORE_HW].hwreg + XABR_SC_ADDR_IER);

        tabr->isCoreLocked[i] = 0;
        tabr->filp[i] = NULL;
    }

    tabr->perf_enable = 0;
    tabr->perf_handle = sn_perf_register(tdev, "xabr", SN_PERF_ID_XABR, xabr_perf, 2);

    timer_setup(&tabr->loading_timer, xabr_loading_timer_isr, 0);
    mod_timer(&tabr->loading_timer, jiffies);

    sn_osal_register(tdev, "xabr", osal_scaler, osal_xabr_handler, NULL);
    sn_pri(tdev, SN_INF, "xabr: osal handler registered\n");

    sn_pri(tdev, SN_INF, "xabr: module initialization done\n");

    return 0;

err_free_irq:
    for (i= 0; i < MAX_SLICE_NUM; i++) {
        for (j = 0; j < tabr->num_cores; ++j) {
            if(tabr->subsys[i][j].irq > 0)
                free_irq(tabr->subsys[i][j].irq, (void *)tabr);
        }
    }
    vfree(tabr->regs_shadow);

err_free_tabr:
    kfree(tabr);
    tdev->modules[SN_MODULE_XABR] = NULL;
    sn_pri(tdev, SN_ERR, "xabr: module not inserted\n");
    return result;
}

void xabr_scaler_release(struct sn_tranx_t *tdev)
{
    struct xabr_scaler *tabr = tdev->modules[SN_MODULE_XABR];
    int i,j;

    del_timer_sync(&tabr->loading_timer);
    /* free the IRQ */
    for (i= 0; i < MAX_SLICE_NUM; i++) {
        for (j = 0; j < tabr->num_cores; ++j) {
            if(tabr->subsys[i][j].irq > 0)
                free_irq(tabr->subsys[i][j].irq, (void *)tabr);
        }
    }

    //release lock on all cores
    xabr_release_core(tabr, 0);
    xabr_release_core(tabr, 1);
    vfree(tabr->regs_shadow);
    kfree(tabr);
    tdev->modules[SN_MODULE_XABR] = NULL;

    sn_pri(tdev, SN_DBG, "xabr: remove module done.\n");
}

long xabr_scaler_ioctl(struct file *filp,
                       unsigned int cmd,
                       unsigned long argp,
                       struct sn_tranx_t *tdev)
{
    long ret = 0;
    struct xabr_scaler *tabr = tdev->modules[SN_MODULE_XABR];

    switch (cmd) {
        case XABR_IOCTL_TWO_SLICE_SOFT_RESET:
            ret = xabr_soft_reset(tabr, 0);
            ret |= xabr_soft_reset(tabr, 1);
            break;

        default:
            sn_pri(tdev, SN_ERR,
                "xabr: %s, cmd:0x%x error.\n", __func__, cmd);
            ret = -EINVAL;
    }
    return ret;
}

void xabr_close(struct sn_tranx_t *tdev, struct file *filp)
{
    struct xabr_scaler *tabr = tdev->modules[SN_MODULE_XABR];
    int i;
    for(i=0;i<MAX_SLICE_NUM;i++){
        if(tabr->filp[i] == filp) {
            xabr_release_core(tabr, i);
            sn_pri(tdev, SN_DBG, "xabr: close slice id = %d filp = %p\n", i, filp);
        }
    }
}
