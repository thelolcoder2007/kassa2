
// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 Xilinx Inc.
 *
 * This is xilinx scaler driver for Linux.
 * This file provide register operation and initialization,
 * like read/write a register or pull/push a batch of registers.
 */

// This is being used as a *reference* implementation should events be needed elsewhere
// They are *not* needed here but used as a PoC for Windows reference

// #define USE_EVENTS

#include <linux/pci.h>
#include <linux/pagemap.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/vmalloc.h>

#if defined(USE_EVENTS)
#include <linux/fdtable.h>
#include <linux/eventfd.h>
#endif

#include "common.h"
#include "transcoder.h"
#include "hw_monitor.h"
#include "regs.h"

#include "sn_perf.h"
#include "sn_osal.h"
#include "memory_osal.h"

#include "xav1_enc.h"

#if defined(TRACE_XAV1_ENC)
#define TRACE(...) trace_printk(__VA_ARGS__); udelay(500);
#else
#define TRACE(...)
#endif

#define XAV1_ENC_REGMAP_SIZE            (5*1024*1024) //5MB

#define XAV1_HOST_CONTROL_BASE          (0x400000) //(0x12900000 - 0x12500000)
#define XAV1_HOST_CONTROL_AP            (XAV1_HOST_CONTROL_BASE + 0x00)
#define XAV1_HOST_CONTROL_GIE           (XAV1_HOST_CONTROL_BASE + 0x04)
#define XAV1_HOST_CONTROL_ISR           (XAV1_HOST_CONTROL_BASE + 0x0C)

#define XAV1_MBOX_BASE                  (0x401000) //(0x12902000 - 0x12500000)
#define XAV1_MBOX0_DATA                 (XAV1_MBOX_BASE + 0 * 16)
#define XAV1_MBOX0_LAST                 (XAV1_MBOX0_DATA + 4)
#define XAV1_MBOX0_PTR                  (XAV1_MBOX0_LAST + 4)
#define XAV1_MBOX15_DATA                (XAV1_MBOX_BASE + 15 * 16)
#define XAV1_MBOX15_PTR                 (XAV1_MBOX15_DATA + 8)

#define XAV1_SAR_BASE                   (0x402000) //(0x12902000 - 0x12500000)
#define XAV1_SAR_CONTROL0_SET             (XAV1_SAR_BASE + 0x90)
#define XAV1_SAR_CONTROL0_CLR             (XAV1_SAR_BASE + 0x94)
#define XAV1_SAR_CONTROL0_VAL             (XAV1_SAR_BASE + 0x98)
#define XAV1_SAR_CONTROL1_SET             (XAV1_SAR_BASE + 0xD0)
#define XAV1_SAR_CONTROL1_CLR             (XAV1_SAR_BASE + 0xD4)
#define XAV1_SAR_CONTROL1_VAL             (XAV1_SAR_BASE + 0xD8)
#define XAV1_SAR_HOST_INTR_STATUS       (XAV1_SAR_BASE + 0x0A0)
#define XAV1_SAR_FPS_STATUS             (XAV1_SAR_BASE + 0x0A8)
#define XAV1_SAR_FPS_STATUS1            (XAV1_SAR_BASE + 0x0E0)
#define XAV1_SAR_FPS_VERSION            (XAV1_SAR_BASE + 0x0AC)

#define XAV1_SAR_ECC_COUNTS             (XAV1_SAR_BASE + 0x0B0)

#define XBIT_MASK(bit)                  ((u32)1<<(bit))

struct xav1_enc_core_desc {
    int slice;
    long base;
    int iosize;
    int irq;
    const char *name;
};

/*here config every core in all subsystems*/
//PF Mode
static struct xav1_enc_core_desc pf_core_array[]= {
    {0, S1_MB_0_AXI_LMB_OFF, XAV1_ENC_REGMAP_SIZE, 18, "xav1_enc_0"},
    {1, S2_MB_0_AXI_LMB_OFF, XAV1_ENC_REGMAP_SIZE, 28, "xav1_enc_1"},
};

//ONE VF Mode
static struct xav1_enc_core_desc onevf_core_array[]= {
    {0, ONE_VF_S1_MB_0_AXI_LMB_OFF, XAV1_ENC_REGMAP_SIZE, 18, "xav1_enc_0"},
    {1, ONE_VF_S2_MB_0_AXI_LMB_OFF, XAV1_ENC_REGMAP_SIZE, 28, "xav1_enc_1"},
};

struct core_cfg {
    int irq;
    unsigned long base_addr;
    volatile u8 *hwreg;
    u32 irq_recvd;
    u32 iosize;
    u32 reg_size;
    u32 *shadow;
    const char *name;
    int irq_count;
};

#define MAX_SLOTS_PER_SLICE 128 // DO *NOT* ADJUST THIS
#define MAX_PENDING_MSGS_PER_SLOT 32
#define MAX_WORDS_PER_MSG 4

#define CHANNEL_CONFIG_SIZE_WORDS 107

struct fifo_msg {
    u32 words[MAX_WORDS_PER_MSG];
};

struct slot {
    wait_queue_head_t wait_queue;
    STRUCT_KFIFO(struct fifo_msg, 512) fifo;
    spinlock_t fifo_lock; // TODO(jkuhn): might not need
    struct file *filp;
    int pendingDelete;

    uint32_t task_id; // assumption that all bitstream descriptors / recon come from same task!

    volatile uint8_t __iomem* h2fw_ring;
    uint32_t h2fw_write;

    volatile uint8_t __iomem* fw2h_ring;
    uint32_t fw2h_write;

    bool fatal_error;

#if defined(USE_EVENTS)
    struct file* eventFile;
#endif
};

#define LOG_RING_SIZE 65536

typedef struct log_ring {
  uint16_t read;
  void*    ringPtr;
  uint64_t physAddr;
} log_ring;

typedef struct log_work {
    struct work_struct work;
    struct sn_tranx_t* tdev;
    unsigned slice;
    unsigned cpu;
    uint16_t write;
    uint16_t watermark;
} log_work;

struct slice {
    int valid;
    int numChan;
    int nextAvailSlot;
    struct mutex lock;
    struct slot slots[MAX_SLOTS_PER_SLICE];
    struct log_ring logRings[4];
};

struct xav1_enc {
    struct core_cfg subsys[MAX_SLICE_NUM];
    spinlock_t slot_lock;
    void *regs_shadow;
    struct sn_tranx_t *tdev;
    struct slice slices[MAX_SLICE_NUM];
    SnPerfHandle perfHandles[SN_PERF_ID_COUNT];
    u32 mb_full[2];
    bool fatal_fw_error;
    struct timer_list ecc_counts_timer;
    uint32_t ecc_counts[2][8][2];

    bool log_disable;
    struct workqueue_struct* log_wq; // one per device should suffice
};

struct xav1_slot {
	__u32 slice; // slice in (always)
	__u32 slot; // slice out on alloc, in on free
};

static struct xav1_enc_core_desc* get_core_desc(struct xav1_enc *xav1e, int *num_cores)
{
    struct xav1_enc_core_desc *cp = NULL;
    int ncores = 0;

    switch(xav1e->tdev->pf_vf_mode)
    {
        case PF_MODE:
            if (xav1e->tdev->vf_index != PF_INDEX) {
                sn_pri(xav1e->tdev, SN_ERR, "%s,vf_index incorrect in PF_MODE\n", __func__);
            } else {
                ncores = sizeof(pf_core_array)/sizeof(pf_core_array[0]);
                cp     = pf_core_array;
            }
            break;

        case ONE_VF_MODE:
            if (xav1e->tdev->vf_index != VF1_INDEX) {
                sn_pri(xav1e->tdev, SN_ERR, "%s,vf_index incorrect in ONE_VF_MODE\n", __func__);
            } else {
                ncores = sizeof(onevf_core_array)/sizeof(onevf_core_array[0]);
                cp     = onevf_core_array;
            }
            break;

        default:
            sn_pri(xav1e->tdev, SN_ERR, "%s,incorrect pf_vf_mode (%d)\n", __func__, xav1e->tdev->pf_vf_mode);
            break;
    }

    *num_cores = ncores;
    return cp;
}


static int init_core_array(struct xav1_enc *xav1e)
{
    int i, irq;
    u32 total_io_size = 0;
    struct xav1_enc_core_desc* cd = NULL;
    int num_cores = 0;

    //get core description table for pf/vf
    cd = get_core_desc(xav1e, &num_cores);
    if (!num_cores) {
        sn_pri(xav1e->tdev, SN_ERR, "failed to get num_cores\n");
        return 0;
    }

    for (i = 0; i < num_cores; i++) {
        xav1e->subsys[i].base_addr = (cd+i)->base;
        xav1e->subsys[i].iosize    = (cd+i)->iosize;
        irq                        = (cd+i)->irq;
        if (irq != -1)
            xav1e->subsys[i].irq   = pci_irq_vector(xav1e->tdev->pdev, irq);

        total_io_size              += xav1e->subsys[i].iosize;
        xav1e->subsys[i].hwreg     = 0;
        xav1e->subsys[i].name      = (cd+i)->name;

        sn_pri(xav1e->tdev, SN_DBG, "xav1_enc:slice[%d]: base=%x  iosize=%d  tot_size=%d  core_irq id: %d  pci_irq: %d\n",
                i, xav1e->subsys[i].base_addr, xav1e->subsys[i].iosize, total_io_size,
                irq, xav1e->subsys[i].irq);
    }

    return total_io_size;
}

static int reserve_io(struct xav1_enc *xav1e)
{
    int i;

    for (i = 0; i < MAX_SLICE_NUM; i++) {
        if (xav1e->subsys[i].iosize) {
            sn_pri(xav1e->tdev, SN_DBG, "xav1_enc:slice[%d]: bar2_virt=0x%x  ipbase=0x%lx, iosize=%d\n", i,
                    xav1e->tdev->bar2_virt, xav1e->subsys[i].base_addr, xav1e->subsys[i].iosize);

            xav1e->subsys[i].hwreg = xav1e->tdev->bar2_virt + xav1e->subsys[i].base_addr;

            if (xav1e->subsys[i].hwreg == NULL) {
                sn_pri(xav1e->tdev, SN_ERR, "xav1_enc: failed to ioremap HW %d regs\n", i);
                return -EBUSY;
            }
            sn_pri(xav1e->tdev, SN_DBG, "xav1_enc:slice[%d]: hwreg(bar2_virt+base)=0x%x  reg[0]=0x%08x\n",
                i, xav1e->subsys[i].hwreg, readl(xav1e->subsys[i].hwreg));
        }
    }
    return 0;
}

static int check_slice(struct xav1_enc *xav1e, unsigned slice) {
    if (slice >= MAX_SLICE_NUM || !xav1e->subsys[slice].base_addr || !xav1e->slices[slice].valid) {
        TRACE("Invalid slice: %u\n", slice);
        return -1;
    }
    return 0;
}

static int can_send_msg(struct xav1_enc *Enc, struct xav1_msg *Msg) // must be under lock
{
    volatile u8* hwregs = Enc->subsys[Msg->slice].hwreg;
    int rawVal = readl(hwregs + XAV1_MBOX0_PTR);
    int writeIdx = (rawVal >> 8) & 127;
    int readIdx  = (rawVal >> 20) & 127;
    int avail = ((readIdx + 128 - writeIdx) & 127) + 128 * !!(rawVal & 1);
    if (avail < Msg->count) {
        TRACE("No room for message: %08x %d %d %d %d\n", rawVal, writeIdx, readIdx, avail, Msg->count);
        ++Enc->mb_full[Msg->slice];
        return -EAGAIN;
    }
    return 0;
}

static int send_msg_raw(struct xav1_enc *Enc, struct xav1_msg *Msg, int Lock, int SkipCheck)
{
    int i;
    struct slice* slice = Enc->slices + Msg->slice;
    volatile u8* hwregs = Enc->subsys[Msg->slice].hwreg;
    if (Lock) {
        int ret;
        if ((ret = mutex_lock_interruptible(&slice->lock)) < 0) {
            return ret;
        }
    }
    if (!SkipCheck && can_send_msg(Enc, Msg) < 0) {
        if (Lock) {
            mutex_unlock(&slice->lock);
        }
        return -EAGAIN;
    }
    for (i = 0; i < Msg->count - 1; ++i) {
        writel(Msg->content[i], hwregs + XAV1_MBOX0_DATA);
    }
    writel(Msg->content[i], hwregs + XAV1_MBOX0_LAST);
    if (Lock) {
        mutex_unlock(&slice->lock);
    }
    return 0;
}

static void patch_ring(struct xav1_enc* Enc, struct file* Fp, unsigned Slice, unsigned Slot, uint32_t Write, uint32_t Watermark, unsigned Dir) {
    struct slice* slice = Enc->slices + Slice;
    struct slot* slot = slice->slots + Slot;
    volatile uint8_t __iomem* ring = (Dir) ? slot->h2fw_ring : slot->fw2h_ring;
    volatile uint8_t __iomem* data = ring + 2 * sizeof(uint32_t);
    uint32_t lastWrite = (Dir) ? slot->h2fw_write : slot->fw2h_write;
    TRACE("SL%u RING%u: %016llx  W: %u [%u]  K: %u  R: %u\n", Slot, Dir, (uint64_t) ring, Write, lastWrite, Watermark, *((uint32_t*) ring));
    if (!Dir) {
        while (lastWrite != Write) {
            if  (lastWrite == Watermark) {
                lastWrite = 0;
            }
            switch (readb(data + lastWrite)) {
            case 0: { // FW2HOST_HW_BITSTREAM
                volatile uint64_t __iomem* patch = (volatile uint64_t __iomem*) (data + lastWrite + 16);
                writeq(sn_mem_osal_translate_mem(Enc->tdev, slot->task_id, readq(patch)), patch);
                lastWrite += 32;
                TRACE("*FW2HOST_HW_BITSTREAM\n");
                break;
            }
            case 1: // FW2HOST_FW_BITSTREAM
                lastWrite += 20 + readl(data + lastWrite + 8);
                TRACE("*FW2HOST_FW_BITSTREAM\n");
                break;
            case 2: // FW2HOST_METADATA_AV1
            case 3: { // FW2HOST_METADATA_VCE
                volatile uint64_t __iomem* patch = (volatile uint64_t __iomem*) (data + lastWrite + 8);
                uint64_t val = readq(patch);
                if (val != 1) {
                    writeq(sn_mem_osal_translate_mem(Enc->tdev, slot->task_id, val), patch);
                }
                val = readq(patch + 1);
                if (val != 1) {
                    writeq(sn_mem_osal_translate_mem(Enc->tdev, slot->task_id, val), patch + 1);
                }
                lastWrite += (readb(data + lastWrite) == 2) ? 168 : 128;
                TRACE("*FW2HOST_METADATA_*\n");
                break; }
            case 4: // FW2HOST_OP_STATS
                lastWrite += 48;
                TRACE("*FW2HOST_OP_STATS\n");
                break;
            case 5: // FW2HOST_LOOKAHEAD_STATS
                lastWrite += 2;
                TRACE("*FW2HOST_LOOKAHEAD_STATS\n");
                break;
            default: {
                pr_err("%s ***** UNKNOWN TYPE %u FATAL *****\n", KBUILD_MODNAME, readb(data + lastWrite));
                lastWrite = Write;
                break; }
            }
            TRACE("WRITE: %u of %u\n", lastWrite, Write);
        }
        slot->fw2h_write = lastWrite;
        return;
    }
    while (lastWrite != Write) {
        if  (lastWrite == Watermark) {
            lastWrite = 0;
        }
        switch (data[lastWrite]) {
        case 0: { // HOST2FW_VIDEOFRAME
            volatile uint64_t __iomem* patch = (volatile uint64_t __iomem*) &data[lastWrite] + 1;
            int i;
            for (i = 0; i < 6; ++i) {
                uint64_t val = readq(patch + i);
                if (val != 1) { // INVALID_ADDR
                    writeq(sn_mem_osal_translate_handle(Enc->tdev, Fp, val, 0), patch + i);
                    readq(patch + i);
                }
            }
            lastWrite += 104;
            TRACE("*HOST2FW_VIDEOFRAME\n");
            break; }
        case 1: // HOST2FW_DESCRIPTOR_STATUS
            TRACE("*HOST2FW_DESCRIPTOR_STATUS\n");
            lastWrite += 12;
            break;
        case 2: // HOST2FW_DESCRIPTOR_VUIPARAMS
            TRACE("*HOST2FW_DESCRIPTOR_VUIPARAMS\n");
            lastWrite += 12;
            break;
        case 3: // HOST2FW_DESCRIPTOR_DYNAMIC_PARAMS
            TRACE("*HOST2FW_DESCRIPTOR_DYNAMIC_PARAMS\n");
            lastWrite += 32;
            break;
        default:
            TRACE("***** UNKNOWN FATAL *****\n");
            lastWrite = Write;
            break;
        }
    }
    slot->h2fw_write = lastWrite;
}

static int ddr_patchin_check(struct xav1_enc* Enc, struct xav1_msg* Msg) { // Only patchable msg is MAILBOX_MSG_FW2HOST_RING_NOTIFY
    TRACE("RING_FW2HOST\n");
    patch_ring(Enc, 0, Msg->slice, (Msg->content[0] >> 10) & 255, Msg->content[1] & 0xffff, Msg->content[1] >> 16, 0);
    return 0;
}

struct logRingEntry {
    uint32_t timestamp;
    uint8_t  severity : 7;
    uint8_t  format : 1;
    uint8_t  moduleLen;
    uint16_t messageLen : 12;
};

static const char* logLevelText[] = {
    "FATAL",
    "ERROR",
    "WARN",
    "NOTICE",
    "INFO",
    "DEBUG"
};

typedef enum {
    FW_LOG_FATAL  = 0,
    FW_LOG_ERROR,
    FW_LOG_WARN,
    FW_LOG_NOTICE,
    FW_LOG_INFO,
    FW_LOG_DEBUG
} fwLogLevel;

static void log_handler(struct work_struct* Work) {
    log_work* work = (log_work*) Work;
    struct xav1_enc* enc = (struct xav1_enc*) work->tdev->modules[SN_MODULE_XAV1_ENC];
    log_ring* ring = enc->slices[work->slice].logRings + work->cpu;
    const char* data = ring->ringPtr + 2 * sizeof(uint32_t);
    struct logRingEntry entry;
    TRACE("RUNNING log work: %016llx[%d]->%016llx %d %d %d %d\n", (uint64_t) data, ring->read, (uint64_t) &entry, work->slice, work->cpu, work->write, work->watermark);
    while (ring->read != work->write) { // if we ever coalesce messages (at source or in driver)
        if (ring->read == work->watermark) {
            ring->read = 0;
        }
        memcpy(&entry, data + ring->read, sizeof(entry));
        if (!entry.format) {
            const char* module = data + ring->read + sizeof(entry);
            const char* msg = module + entry.moduleLen;
            pr_info("%s %u:%u:%u %10u %-6s [%s] %s\n", KBUILD_MODNAME, enc->tdev->node_index, work->slice, work->cpu, entry.timestamp, logLevelText[entry.severity],
                module, msg);
        } else {
            const char* module = data + ring->read + sizeof(entry);
            const char* msg = module + entry.moduleLen;
            unsigned char dbuf[8 * 64];
            unsigned char* dbufPtr = NULL;
            int len = entry.messageLen;
            int index = 0;
            pr_info("%s %u:%u:%u %10u %-6s [%s] #OPAQUE# %d\n", KBUILD_MODNAME, enc->tdev->node_index, work->slice, work->cpu, entry.timestamp,
                logLevelText[entry.severity], module, (len + 63) / 64);
            while (len) {
                char buf[64 * 3 + 1];
                int chunk = (len > 64) ? 64 : len;
                int i;
                if (!dbufPtr) {
                    int inputSize = (len > sizeof(dbuf)) ? sizeof(dbuf) : len;
                    dbufPtr = dbuf;
                    // probably misaligned, just use memcpy_fromio instead of ma35 version
                    memcpy_fromio(dbuf, msg, inputSize);
                    msg += inputSize;
                }
                for (i = 0; i < chunk; ++i) {
                    sprintf(buf + i * 3, " %02x", *(dbufPtr++));
                }
                if (dbufPtr - dbuf == sizeof(dbuf)) {
                    dbufPtr = NULL;
                }
                pr_info("%s %u:%u:%u %10u_%02d%s", KBUILD_MODNAME, enc->tdev->node_index, work->slice, work->cpu, entry.timestamp, index, buf);
                ++index;
                len -= chunk;
            }
        }
        ring->read += sizeof(entry) + entry.moduleLen + entry.messageLen;
        if (ring->read & 3) {
            ring->read = (ring->read + 4) & ~3;
        }
    }
    *(uint32_t*) ring->ringPtr = ring->read;
    kfree(work);
}

static void process_reserved_message(struct xav1_enc* Enc, int Slice, int SlotNum, uint32_t* Msg) {
    log_work* work;
    if ((Msg[0] & 255) != 5) { // only interested in notification, not acknowledgement
        return;
    }
    work = (log_work*) kmalloc(sizeof(log_work), GFP_NOWAIT);
    if (!work) {
        return;
    }
    INIT_WORK(&work->work, log_handler);
    work->tdev = Enc->tdev;
    work->slice = Slice;
    work->cpu = SlotNum - 252;
    work->write = Msg[1] & 0xffff;
    work->watermark = (Msg[1] >> 16) & 0xffff;
    queue_work(Enc->log_wq, &work->work);
}

static int __attribute__((no_sanitize("undefined"))) process_host_messages(struct xav1_enc *Enc, int Slice) // this ends up inlined to ISR
{
    // TODO(jkuhn) error handling and recovery
    volatile u8 *hwregs = Enc->subsys[Slice].hwreg;
    struct fifo_msg msg;
    u32 header;
    u8 cmd;
    int numWords;
    int slotNum;
    int arg;
    struct slot* slot;
    msg.words[0] = readl(hwregs + XAV1_MBOX15_DATA);
    header = msg.words[0];
    cmd = header & 0xff;
    numWords = (header >> 8) & 3;
    if (numWords) {
        int i;
        for (i = 1; i < numWords + 1; ++i) {
            msg.words[i] = readl(hwregs + XAV1_MBOX15_DATA);
        }
    }
    if (likely(cmd < 250 || cmd == 255)) {
        slotNum = (header >> 10) & 0xff;
        arg = (header >> 18);
        if (unlikely(slotNum >= MAX_SLOTS_PER_SLICE)) { // Only logging
            process_reserved_message(Enc, Slice, slotNum, msg.words);
            return 0;
        }
        TRACE("Got msg %d for %d + %d [%x]\n", cmd, slotNum, numWords, header);
        slot = Enc->slices[Slice].slots + slotNum;
        if (unlikely(!slot->filp)) {
            TRACE("DROPPING message for unallocated slot %u:%u\n", Slice, slotNum);
            return 0;
        }
        if (unlikely(slot->pendingDelete)) {
            if (likely(cmd == 255)) { // teardown
                void* filp;
                struct slice* slice = Enc->slices + Slice;
                unsigned long flags;
                spin_lock_irqsave(&Enc->slot_lock, flags);
                filp = slot->filp;
                slot->filp = NULL;
                ++slice->numChan;
                spin_unlock_irqrestore(&Enc->slot_lock, flags);
                TRACE("Freed slot: %u:%u [%u] for %p\n", Slice, slotNum, slice->numChan, filp);
                return 0;
            }
            TRACE("Ignoring message for slot %u:%u pending delete\n", Slice, slotNum);
            return 0;
        }
        if (unlikely(!kfifo_put(&slot->fifo, msg))) {
            slot->fatal_error = true;
            sn_pri(Enc->tdev, SN_ERR, "**FATAL** Full slot %u:%u\n", Slice, slotNum);
        } else {
            TRACE("SL%u MSG\n", slotNum);
            wake_up_all(&slot->wait_queue);
        }
        return 0;
    }
    switch (cmd) {
    case 252: { // FATAL ERROR
        int cpu = header >> 30;
        if (numWords) {
            int index = (header >> 10) & 0xfffff;
            int i;
            for (i = 1; i < numWords + 1; ++i) {
                printk(KERN_CRIT "[BT] %08x %d Device %s Slice %d Cpu %d\n", msg.words[i], index + i - 1, Enc->tdev->dev_name, Slice, cpu);
            }
        } else {
            int error = (header >> 10) & 0xfffff;
            Enc->fatal_fw_error = true;
            printk(KERN_CRIT "FATAL FPS ERROR: %08x Device %s Slice %d Cpu %d\n", error, Enc->tdev->dev_name, Slice, cpu);
        }
        return 0;
    }
    case 253: { // PERF LOAD
        u32 ipId = (header >> 11) & 127;
        u32 load = (header >> 18);
        if (ipId >= SN_PERF_ID_COUNT) {
          TRACE("INGORING UNKNOWN IP: %u", ipId);
          return 0;
        }
        sn_perf_load(Enc->perfHandles[ipId], Slice, load);
        return 0;
    }
    case 250: { // PERF SYNC
        sn_perf_event event;
        event.bits = header >> 10;
        if (event.ipId >= SN_PERF_ID_COUNT) {
          TRACE("INGORING UNKNOWN IP: %u", event.ipId);
          return 0;
        }

        event.kernelTsBefore = msg.words[1] | ((uint64_t) msg.words[2] << 32);
        event.hwTs = msg.words[3];

        sn_perf_record(Enc->perfHandles[event.ipId], &event);
        return 0;
    }
    case 254: { // PERF EVENT
        sn_perf_event event;
        event.bits = header >> 10;

        if (event.ipId >= SN_PERF_ID_COUNT) {
          TRACE("INGORING UNKNOWN IP: %u", event.ipId);
          return 0;
        }

        event.hwTs = msg.words[1];
        event.payload = msg.words[2];
        event.name = msg.words[3];

        sn_perf_record(Enc->perfHandles[event.ipId], &event);
        return 0;
    }
    }
    return 0; // Should not reach
}

static irqreturn_t __attribute__((no_sanitize("undefined"))) isr(int irq, void *dev_id)
{
    volatile u8 *hwregs;
    u32 irq_status;
    int slice;
    struct xav1_enc *xav1e = (struct xav1_enc *) dev_id;
    u32 int_shadow_status_off;
    u32 int_shadow_isr_off;
    //determine slice index from irq#
    if (likely(irq == xav1e->subsys[0].irq)) {
        slice = 0;
        int_shadow_status_off = S1_INT_SD_XENC_STUS;
        int_shadow_isr_off = S1_INT_SD_XENC_ISR;
    } else {
        slice = 1;
        int_shadow_status_off = S2_INT_SD_XENC_STUS;
        int_shadow_isr_off = S2_INT_SD_XENC_ISR;
    }
    hwregs = xav1e->subsys[slice].hwreg;

    /* Semaphore HOST interrupt status register read */
    irq_status = readl(hwregs + XAV1_SAR_HOST_INTR_STATUS);
    if (likely(irq_status & XBIT_MASK(29))) {
        process_host_messages(xav1e, slice);
    }
    return IRQ_HANDLED;
}

#if SUB_SYS_VCE != 1 /* assume legoVSI FPGA which doesn't have xav1 HW */
static void internal_configure_on(struct xav1_enc *xav1e)
{
    int i;
    u32 val;
    volatile u8 *base_addr=NULL;
    unsigned int loop;

    for (i = 0; i < MAX_SLICE_NUM; i++) {
        base_addr =  xav1e->tdev->bar2_virt + ((xav1e->tdev->pf_vf_mode == PF_MODE) ? pf_core_array[i].base : onevf_core_array[i].base);
		loop = 1000;
        val = readl(base_addr + XAV1_SAR_CONTROL0_CLR);
        // writel(val|0x1000,base_addr + THS0_POWER_CTRL_SET);
        // printk("%s %d, set reg base=0x%x, poll reg base=0x%x\n",__func__,__LINE__,(base_addr+THS0_POWER_CTRL_SET),(base_addr+THS0_POWER_CTRL_POLL));
        writel(0x1000,base_addr + XAV1_SAR_CONTROL0_CLR);

        val = readl(base_addr + XAV1_SAR_FPS_STATUS);
        while ((val&0x3000000) != 0x0) {
            val = readl(base_addr + XAV1_SAR_FPS_STATUS);
            msleep(1);
            loop--;
            if (!loop) {
                sn_pri(xav1e->tdev,SN_ERR,"xav1 internal power on configuration error!\n");
                break;
            }
        }
    }
}

static void internal_configure_off(struct xav1_enc *xav1e)
{
    int i;
    u32 val;
    volatile u8 *base_addr=NULL;
    unsigned int loop;

    for (i = 0; i < MAX_SLICE_NUM; i++) {
		loop = 1000;
        base_addr =  xav1e->tdev->bar2_virt + ((xav1e->tdev->pf_vf_mode == PF_MODE) ? pf_core_array[i].base : onevf_core_array[i].base);
        val = readl(base_addr + XAV1_SAR_CONTROL0_SET);
        writel(0x1000,base_addr + XAV1_SAR_CONTROL0_SET);

        val = readl(base_addr + XAV1_SAR_FPS_STATUS);
        while ((val&0x3000000) != 0x3000000) {
            val = readl(base_addr + XAV1_SAR_FPS_STATUS);
            msleep(1);
            loop--;
            if (!loop) {
                sn_pri(xav1e->tdev,SN_ERR,"xav1 internal power off  configuration error!\n");
                break;
            }
        }
    }
}
#endif

// reclaim all allocated slots and prevent new allocations
static int cancel_slot_allocation(struct xav1_enc* Enc, int SliceNo)
{
    unsigned long flags;
    if (SliceNo < 0 || SliceNo >= MAX_SLICE_NUM) {
        return -EINVAL;
    }
    spin_lock_irqsave(&Enc->slot_lock, flags);
    {
        struct slice* slice = Enc->slices + SliceNo;
        struct slot* slot = slice->slots;
        int j;
        slice->numChan = 0;
        for (j = 0; j < MAX_SLOTS_PER_SLICE; ++j, ++slot) {
            slot->filp = NULL;
        }
    }
    spin_unlock_irqrestore(&Enc->slot_lock, flags);
    return 0;
}

// allow allocations again after cancel_slot_allocation
static int begin_slot_allocation(struct xav1_enc* Enc, int SliceNo)
{
    unsigned long flags;
    if (SliceNo < 0 || SliceNo >= MAX_SLICE_NUM) {
        return -EINVAL;
    }
    spin_lock_irqsave(&Enc->slot_lock, flags);
    Enc->slices[SliceNo].numChan = MAX_SLOTS_PER_SLICE;
    spin_unlock_irqrestore(&Enc->slot_lock, flags);
    return 0;
 }

static int xav1_whole_subsys_soft_reset(struct xav1_enc *xav1e, int slice)
{
    volatile u8 *hwregs = NULL;
    if (slice < 0 || slice >= MAX_SLICE_NUM)
    {
        return -1;
    }
    hwregs = xav1e->subsys[slice].hwreg;

    sys_xav1_whole_subsys_soft_reset(xav1e->tdev, slice);
    cancel_slot_allocation(xav1e, slice);
    sys_xav1_whole_subsys_release_soft_reset(xav1e->tdev, slice);
    writel(1 << 29, hwregs + XAV1_SAR_CONTROL0_SET); // enable MBOX15 host interrupt
    begin_slot_allocation(xav1e, slice);
    return 0;
}

static void setup_log_rings(struct sn_tranx_t *tdev) {
    struct xav1_enc *xav1e = tdev->modules[SN_MODULE_XAV1_ENC];
    struct slice* slice = xav1e->slices;
    int i;
    for (i = 0; i < MAX_SLICE_NUM; ++i, ++slice) {
        int j;
        for (j = 0; j < 4; ++j) {
            struct xav1_msg msg;
            if (!slice->logRings[j].ringPtr) {
                slice->logRings[j].physAddr = sn_mem_osal_alloc_mem(tdev, LOG_RING_SIZE, NULL, 0, 1);
                slice->logRings[j].ringPtr = sn_mem_osal_translate_mmio(tdev, slice->logRings[j].physAddr);
            }
            slice->logRings[j].read = 0;
            msg.slice = i;
            msg.count = 3;
            msg.content[0] = 3 | (2 << 8) | ((252 + j) << 10) | (j << 18) | (FW_LOG_ERROR << 20);
            msg.content[1] = (uint32_t) slice->logRings[j].physAddr;
            msg.content[2] = slice->logRings[j].physAddr >> 32;
            while (send_msg_raw(xav1e, &msg, 1, 0) == -EAGAIN) {
                msleep(1);
            }
        }
    }
}

static void release_log_rings(struct sn_tranx_t *tdev)
{
    struct xav1_enc *xav1e = tdev->modules[SN_MODULE_XAV1_ENC];
    struct slice* slice = xav1e->slices;
    int i;
    for (i = 0; i < MAX_SLICE_NUM; ++i, ++slice) {
        int j;
        for (j = 0; j < ARRAY_SIZE(slice->logRings); ++j) {
            if (slice->logRings[j].ringPtr) {
                sn_mem_osal_free_mem(tdev, slice->logRings[j].physAddr, NULL);
            }
        }
    }
}

static int init_slices(struct xav1_enc* Enc)
{
    struct slice* slice = Enc->slices;
    int i;
    TRACE("init_slices\n");
    for (i = 0; i < MAX_SLICE_NUM; ++i, ++slice) {
        struct slot* slot = slice->slots;
        volatile u8 *hwregs = Enc->subsys[i].hwreg;
        int j;
        slice->numChan = MAX_SLOTS_PER_SLICE;
        slice->nextAvailSlot = 1;
        for (j = 0; j < MAX_SLOTS_PER_SLICE; ++j, ++slot) {
            slot->filp = NULL;
        }
        slice->valid = readl(hwregs + XAV1_MBOX0_PTR) != 0; // TODO(jkuhn): robust?
        writel(1 << 29, hwregs + XAV1_SAR_CONTROL0_SET); // enable MBOX15 host interrupt
        writel(0, hwregs + XAV1_MBOX15_PTR); // reset mailboxes
        writel(0, hwregs + XAV1_MBOX0_PTR); // reset mailboxes
        mutex_init(&slice->lock);
    }
    setup_log_rings(Enc->tdev);
    return 0;
}

static int xav1_enc_sync(struct sn_tranx_t* tdev, __u32 ipId, __u32 cmd, __u32 arg) {
    struct xav1_enc* xav1e = tdev->modules[SN_MODULE_XAV1_ENC];
    struct xav1_msg msg;
    struct slice* slice = xav1e->slices;
    uint64_t kernelTs = ktime_to_ns(ktime_get());
    int i;
    if (cmd != SN_PERF_CMD_SYNC) {
        return -EINVAL;
    }
    for (i = 0; i < MAX_SLICE_NUM; ++i, ++slice) {
        if (!slice->valid) {
            continue;
        }
        msg.slice = i;
        msg.count = 3;
        msg.content[0] = 250 | 2 << 8 | (ipId << 10) | (cmd << 17) | (arg << 19);
        msg.content[1] = kernelTs & 0xFFFFFFFF;
        msg.content[2] = kernelTs >> 32;

        send_msg_raw(xav1e, &msg, 1, 0); // TODO(jkuhn): spin???
        // TRACE("%u %u %u %u\n", i, ipId, cmd, arg);
    }
    return 0;
}

static int xav1_enc_perf(struct sn_tranx_t* tdev, __u32 ipId, __u32 cmd, __u32 arg) {
    struct xav1_enc* xav1e = tdev->modules[SN_MODULE_XAV1_ENC];
    struct xav1_msg msg;
    struct slice* slice = xav1e->slices;
    int i;
    if (cmd == SN_PERF_CMD_SYNC) {
        return -EINVAL;
    }
    for (i = 0; i < MAX_SLICE_NUM; ++i, ++slice) {
        if (!slice->valid) {
            continue;
        }
        msg.slice = i;
        msg.count = 1;
        msg.content[0] = 254 | (ipId << 10) | (cmd << 17) | (arg << 19);

        send_msg_raw(xav1e, &msg, 1, 0); // TODO(jkuhn): spin???
        // TRACE("%u %u %u %u\n", i, ipId, cmd, arg);
    }
    return 0;
}

static int alloc_slot(struct xav1_enc* Enc, sn_osal_work* work) {
    struct slice* slice;
    struct slot* slot;
    struct slot* end;
    struct xav1_slot slotInfo;
    unsigned long flags;
    signed int ret;

    if (work->cmd.numData < 1) {
        work->numResp = -1;
        ret = -ENOSPC;
        goto exit;
    }
    work->numResp = 1;
    slotInfo.slice = work->data[0];

    if (check_slice(Enc, slotInfo.slice)) {
        ret = -EINVAL;
        goto exit;
    }
    slice = Enc->slices + slotInfo.slice;
    spin_lock_irqsave(&Enc->slot_lock, flags);
    if (!slice->numChan) {
        spin_unlock_irqrestore(&Enc->slot_lock, flags);
        TRACE("No slots\n");
        ret = -ENOMEM;
        goto exit;
    }
    slot = slice->slots + slice->nextAvailSlot;
    end = slot;
    slotInfo.slot = slice->nextAvailSlot;
    do {
        if (!slot->filp) {
            slot->filp = work->filp;
            slot->pendingDelete = 0;
            slot->fatal_error = false;
            slice->nextAvailSlot = (slotInfo.slot + 1) % MAX_SLOTS_PER_SLICE;
            --slice->numChan;
            spin_unlock_irqrestore(&Enc->slot_lock, flags);
            init_waitqueue_head(&slot->wait_queue);
            spin_lock_init(&slot->fifo_lock);
            INIT_KFIFO(slot->fifo);
            TRACE("Allocated slot: %u:%u [%u] for %p\n", slotInfo.slice, slotInfo.slot, slice->numChan, work->filp);
            work->data[0] = slotInfo.slot;
            return 0;
        }
        if (++slotInfo.slot == MAX_SLOTS_PER_SLICE) {
            slotInfo.slot = 0;
            slot = slice->slots;
        } else {
            ++slot;
        }
    } while (slot != end);
    // should not be possible
    spin_unlock_irqrestore(&Enc->slot_lock, flags);
    printk(KERN_CRIT "No free slots (should be)\n");
    ret = -EINVAL;

exit:
    work->data[0] = ret;
    return ret;
}

static void free_slot(struct xav1_enc *Enc, sn_osal_work* work) {
    struct xav1_slot slotInfo;
    struct slice* slice;
    struct slot* slot;
    unsigned long flags;
    struct xav1_msg msg;
    int ret;

    if (work->numResp < 1) {
        work->numResp = -1;
        ret = -ENOSPC;
        goto exit;
    }
    work->numResp = 1;
    slotInfo.slice = work->data[0];
    slotInfo.slot = work->data[1];

    if (slotInfo.slot >= MAX_SLOTS_PER_SLICE || check_slice(Enc, slotInfo.slice)) {
        ret = -EINVAL;
        goto exit;
    }
    slice = Enc->slices + slotInfo.slice;
    slot = slice->slots + slotInfo.slot;
    msg.slice = slotInfo.slice;
    msg.count = 1;
    msg.content[0] = 255 | (slotInfo.slot << 10);
    spin_lock_irqsave(&Enc->slot_lock, flags);
    if (slot->filp != work->filp || slot->pendingDelete) {
        // A reset may have revoked ownership of this slot, or maybe we're dealing with an ill-behaved application.
        spin_unlock_irqrestore(&Enc->slot_lock, flags);
        TRACE("Skipping %p %u\n", slot->filp, slot->pendingDelete);
        ret = -EINVAL;
        goto exit;
    }
    slot->pendingDelete         = 1;
    spin_unlock_irqrestore(&Enc->slot_lock, flags);
    if (send_msg_raw(Enc, &msg, 1, 0) < 0) {
        spin_lock_irqsave(&Enc->slot_lock, flags);
        slot->pendingDelete = 0;
        spin_unlock_irqrestore(&Enc->slot_lock, flags);
        ret = -EAGAIN;
        goto exit;
    }
    work->data[0]       = 0;
    slot->pendingDelete = 1;
    return;
exit:
    work->data[0] = ret;
}

static void ddr_patchout_check(struct xav1_enc* Enc, struct file* Fp, struct xav1_msg* Msg) {
    switch (Msg->content[0] & 255) { // message type;
    case 128: { // MAILBOX_MSG_PROPOSE_CHANNEL
        uint64_t raw = (((uint64_t) Msg->content[2])) << 32 | Msg->content[1];
        volatile uint64_t __iomem* addr = (uint64_t*) sn_mem_osal_translate_mmio(Enc->tdev, raw);
        writeq(sn_mem_osal_translate_handle(Enc->tdev, Fp, readq(addr), 0), addr);
        readq(addr);
        TRACE("MSG_PROPOSE: %016llx\n", *addr);
        return;
    }
    case 129: { // MAILBOX_MSG_COMMIT_CHANNEL
        uint64_t raw = (((uint64_t) Msg->content[2])) << 32 | Msg->content[1];
        volatile uint32_t __iomem* addr = (volatile uint32_t __iomem*) sn_mem_osal_translate_mmio(Enc->tdev, raw);
        uint32_t count = readl(addr + CHANNEL_CONFIG_SIZE_WORDS);
        volatile struct ResourceInst {
            uint8_t  m_type;
            uint32_t m_index;
            uint64_t m_addr;
        } __iomem* inst = (volatile struct ResourceInst __iomem*) (addr + CHANNEL_CONFIG_SIZE_WORDS + 1);
        int i;
        TRACE("MSG_COMMIT\n");
        TRACE("Patching %d resources\n", count);
        for (i = 0; i < count; ++i, ++inst) {
            volatile uint64_t __iomem* resaddr = (volatile uint64_t __iomem*) &inst->m_addr;
            writeq(sn_mem_osal_translate_handle(Enc->tdev, Fp, readq(&inst->m_addr), 0), resaddr);
            readq(addr);
            TRACE("%3u %u %016llx\n", i, inst->m_type, inst->m_addr);
        }
        return;
    }
    case 131: { // MAILBOX_MSG_HOST2FW_RING_NOTIFY
        TRACE("SL%u MSG_HOST2FW\n", (Msg->content[0] >> 10) & 255);
        patch_ring(Enc, Fp, Msg->slice, (Msg->content[0] >> 10) & 255, Msg->content[1] & 0xffff, Msg->content[1] >> 16, 1);
        return;
    }
    case 197: { // MAILBOX_MSG_FUNCTIONAL_TEST_PTR
        uint64_t raw = (((uint64_t) Msg->content[2])) << 32 | Msg->content[1];
        uint64_t* addr = (uint64_t*) sn_mem_osal_translate_mmio(Enc->tdev, raw);
        uint32_t numPatch = Msg->content[0] >> 18;
        int i;
        for (i = 0; i < numPatch; ++i) {
            writeq(sn_mem_osal_translate_handle(Enc->tdev, Fp, readq(addr), 0), addr);
            readq(addr);
            ++addr;
        }
        return;
    }
    default:
        break;
    }
}

static int send_msg(struct xav1_enc* Enc, sn_osal_work* work) {
    struct xav1_msg msg;
    int ret;
    int i;
    if (work->numResp < 1) {
        work->numResp = -1;
        return -ENOSPC;
    }
    work->numResp = 1;
    msg.slice = work->data[0];
    msg.count = work->data[1];
    for (i = 0; i < work->data[1]; ++i) {
        msg.content[i] = work->data[i + 2];
    }
#if defined(TRACE_XAV1_ENC)
    for (; i < MAX_WORDS_PER_MSG; ++i) {
        msg.content[i] = 0;
    }
#endif
    TRACE("%08x %08x %08x %08x\n", msg.content[0], msg.content[1], msg.content[2], msg.content[3]);
    if (check_slice(Enc, msg.slice) || msg.count > 4) {
        TRACE("Invalid Message: slice: %d count: %d\n", msg.slice, msg.count);
        ret = -EINVAL;
    } else {
        struct slice* slice = Enc->slices + msg.slice;
        if ((ret = mutex_lock_interruptible(&slice->lock)) < 0) {
            return ret;
        }
        ret = can_send_msg(Enc, &msg);
        if (!ret) {
            ddr_patchout_check(Enc, work->filp, &msg);
            ret = send_msg_raw(Enc, &msg, 0, 1);
        }
        mutex_unlock(&slice->lock);
    }
    work->data[0] = ret;
    return ret;
}

static int wait_msg(struct xav1_enc* Enc, sn_osal_work* work) {
    struct xav1_wait_msg msg;
    struct slice* slice;
    struct slot* slot;
    int ret;
    int ret_time;

    if (work->numResp < 5) {
        work->numResp = -1;
    }
    msg.slice = work->data[0];
    msg.slot = work->data[1];
    msg.timeout_ms = work->data[2];

    if (check_slice(Enc, msg.slice)) {
        ret = -EINVAL;
        goto exit;
    }
    slice = Enc->slices + msg.slice;
    slot = slice->slots + msg.slot;
    if (slot->filp != work->filp) {;
        ret = -EINVAL;
        goto exit;
    }
    if (slot->fatal_error) {
        ret = -ENOTRECOVERABLE;
        goto exit;
    }
    ret_time = wait_event_interruptible_timeout(slot->wait_queue, !kfifo_is_empty(&slot->fifo), msg.timeout_ms * HZ / 1000);
    TRACE("SL%u WAITED: %d %d\n", msg.slot, msg.timeout_ms * HZ / 1000, ret_time);
    if (ret_time < 0) {
        ret = -EFAULT;
        goto exit;
    }
    work->data[0] = ret_time * 1000 / HZ;
    work->numResp = 1;
    if (ret_time) {
        int i;
        struct fifo_msg val;
        if (!kfifo_get(&slot->fifo, &val)) {
            TRACE("FIFO was empty!\n");
            ret = -EINVAL;
            goto exit;
        }
        for (i = 0; i < MAX_WORDS_PER_MSG; ++i) {
            work->data[i + 1] = val.words[i];
        }
        if ((val.words[0] & 255) == 132) { // MAILBOX_MSG_FW2HOST_RING_NOTIFY
            struct xav1_msg msg2;
            msg2.slice = msg.slice;
            msg2.content[0] = val.words[0];
            msg2.content[1] = val.words[1];
            msg2.content[2] = val.words[2];
            msg2.content[3] = val.words[3];
            ddr_patchin_check(Enc, &msg2);
        }
        work->numResp = 5;
    }
    return 0;

exit:
    work->numResp = 1;
    work->data[0] = ret;
    return ret;
}

static int set_ring_addrs(struct xav1_enc* Enc, sn_osal_work* work) {
    struct slice* slice;
    struct slot* slot;
    if (check_slice(Enc, work->data[0])) {
        return -EINVAL;
    }
    slice = Enc->slices + work->data[0];
    slot = slice->slots + work->data[1];
    if (slot->filp != work->filp || slot->pendingDelete) {
        TRACE("Skipping %p %u\n", slot->filp, slot->pendingDelete);
        return -EINVAL;
    }
    slot->h2fw_ring = sn_mem_osal_translate_mmio(Enc->tdev, *((uint64_t*) (work->data + 2)));
    slot->h2fw_write = 0;
    slot->fw2h_ring = sn_mem_osal_translate_mmio(Enc->tdev, *((uint64_t*) (work->data + 4)));
    slot->fw2h_write = 0;
    slot->task_id = sn_mem_osal_task_from_handle(*((uint64_t*) (work->data + 6)));
    TRACE("SL%u TASK: %u  H2FW: %016llx  FW2H: %016llx\n", work->data[1], slot->task_id, (uint64_t) slot->h2fw_ring, (uint64_t) slot->fw2h_ring);
    work->numResp = 0;
    return 0;
}

static void osal_encoder_handler(struct work_struct *workPtr)
{
    sn_osal_work* work       = (sn_osal_work*) workPtr;
    struct sn_tranx_t* tdev  = work->tdev;
    struct xav1_enc* xav1e   = tdev->modules[SN_MODULE_XAV1_ENC];
#if defined(USE_EVENTS)
    struct task_struct* task = work->task;
#endif
    long ret = 0;

    if (xav1e->fatal_fw_error) {
        if (work->numResp < 1) {
            work->numResp = -1;
        } else {
            work->numResp = 1;
            work->data[0] =-ENOTRECOVERABLE;
        }
        return;
    }
    switch(work->cmd.op_code)
    {
    case OSAL_ACCEL_CMD_ENC_ALLOC_SLOT:
        TRACE("ALLOC_SLOT\n");
        ret = alloc_slot(xav1e, work);
        break;
    case OSAL_ACCEL_CMD_ENC_FREE_SLOT:
        TRACE("FREE_SLOT\n");
        free_slot(xav1e, work);
        break;
    case OSAL_ACCEL_CMD_ENC_SEND_MSG:
        TRACE("SEND_MSG\n");
        ret = send_msg(xav1e, work);
        break;
    case OSAL_ACCEL_CMD_ENC_WAIT_MSG:
        TRACE("WAIT_MSG\n");
        ret = wait_msg(xav1e, work);
        break;
    case OSAL_ACCEL_CMD_ENC_SET_RING_ADDRS:
        TRACE("RING_ADDRS\n");
        ret = set_ring_addrs(xav1e, work);
        break;
    default:
        sn_pri(tdev, SN_ERR, "xav1_enc: %s, OSAL cmd:0x%x invalid.\n", __func__, work->cmd.op_code);
        if (work->numResp < 1) {
            work->numResp = -1;
        } else {
            work->numResp = 1;
            work->data[0] = 0xdeadbeef;
        }
        break;
    }
    sn_osal_finish_work(work);
}

static ssize_t enable_load_store(struct device* Dev, struct device_attribute* Attr, const char* Buf, size_t Count)
{
    struct sn_misc_tdev* mtdev = dev_get_drvdata(Dev);
    struct sn_tranx_t* tdev = mtdev->tdev;
    struct xav1_enc* xav1e = tdev->modules[SN_MODULE_XAV1_ENC];
    struct xav1_msg msg;
    if (Count != 2 || (Buf[0] != '0' && Buf[0] != '1')) {
        return -EINVAL;
    }
    msg.slice = 0; // send to both slices;
    msg.count = 1;
    msg.content[0] = 251 | ((Buf[0] - '0') << 10);
    send_msg_raw(xav1e, &msg, 1, 0);
    msg.slice = 1;
    send_msg_raw(xav1e, &msg, 1, 0);
    return Count;
}

static ssize_t mb_full_show(struct device* Dev, struct device_attribute* Attr, char* Buf)
{
    struct sn_misc_tdev* mtdev = dev_get_drvdata(Dev);
    struct sn_tranx_t* tdev = mtdev->tdev;
    struct xav1_enc* xav1e = tdev->modules[SN_MODULE_XAV1_ENC];
    return sysfs_emit(Buf, "%u %u\n", xav1e->mb_full[0], xav1e->mb_full[1]);
}

static ssize_t fps_ecc_counts_show(struct device* Dev, struct device_attribute* Attr, char* Buf)
{
    struct sn_misc_tdev* mtdev = dev_get_drvdata(Dev);
    struct sn_tranx_t* tdev = mtdev->tdev;
    struct xav1_enc* xav1e = tdev->modules[SN_MODULE_XAV1_ENC];
    int len = 0, slice, cpu;
    for (slice = 0; slice < 2; ++slice) {
        for (cpu = 0; cpu < 8; ++cpu) {
            len += sprintf(Buf + len, "S%uC%u%c  CO: %10u  UC: %10u\n", slice, cpu & 3, (cpu < 4) ? 'I' : 'D', xav1e->ecc_counts[slice][cpu][0], xav1e->ecc_counts[slice][cpu][1]);
        }
    }
    return len;
}

static int disable_logging(struct xav1_enc* Enc) {
    int i;
    for (i = 0; i < MAX_SLICE_NUM; ++i) {
        int j;
        for (j = 0; j < 4; ++j) {
            struct xav1_msg msg;
            msg.slice = i;
            msg.count = 1;
            msg.content[0] = 4 | ((252 + j) << 10) | (j << 18); // CANCEL
            while (send_msg_raw(Enc, &msg, 1, 0) == -EAGAIN) {
                msleep(1);
            }
        }
    }
    return 0;
}

static ssize_t kldisable_store(struct device* Dev, struct device_attribute* Attr, const char* Buf, size_t Count)
{
    struct sn_misc_tdev* mtdev = dev_get_drvdata(Dev);
    struct sn_tranx_t* tdev = mtdev->tdev;
    struct xav1_enc* xav1e = tdev->modules[SN_MODULE_XAV1_ENC];
    if (Count != 2 || (Buf[0] != '0' && Buf[0] != '1')) {
        return -EINVAL;
    }
    if (Buf[0] - '0' == xav1e->log_disable) {
        return Count;
    }
    xav1e->log_disable = Buf[0] - '0';
    if (!xav1e->log_disable) {
        sn_pri(tdev, SN_INF, "kl enabled\n");
        setup_log_rings(tdev);
    } else {
        sn_pri(tdev, SN_INF, "kl disabled\n");
        disable_logging(xav1e);
    }
    return Count;
}

static ssize_t kldisable_show(struct device* Dev, struct device_attribute* Attr, char* Buf)
{
    struct sn_misc_tdev* mtdev = dev_get_drvdata(Dev);
    struct sn_tranx_t* tdev = mtdev->tdev;
    struct xav1_enc* xav1e = tdev->modules[SN_MODULE_XAV1_ENC];
    return sysfs_emit(Buf, "%u\n", xav1e->log_disable);
}

static DEVICE_ATTR_WO(enable_load);
static DEVICE_ATTR_RO(mb_full);
static DEVICE_ATTR_RO(fps_ecc_counts);
static DEVICE_ATTR_RW(kldisable);

static struct attribute* trans_xav1_sysfs_entries[] = {
    &dev_attr_enable_load.attr, &dev_attr_mb_full.attr, &dev_attr_fps_ecc_counts.attr, &dev_attr_kldisable.attr, NULL
};

static struct attribute_group trans_xav1_attribute_group = {
    .name = NULL,
    .attrs = trans_xav1_sysfs_entries,
};

int xav1_get_register(struct sn_tranx_t *tdev, u32 slice_num, u32 reg)
{
    struct xav1_enc* xav1e = tdev->modules[SN_MODULE_XAV1_ENC];
    return readl(xav1e->subsys[slice_num].hwreg + reg);
}

int xav1_soft_reset(struct sn_tranx_t *tdev)
{
    struct xav1_enc* xav1e = tdev->modules[SN_MODULE_XAV1_ENC];

    if (xav1_whole_subsys_soft_reset(xav1e, 0) != 0) {
        return -1;
    }

    if (xav1_whole_subsys_soft_reset(xav1e, 1) !=0) {
        return -1;
    }

    return 0;
}

static void ecc_counts_timer(struct timer_list* timer)
{
    struct xav1_enc* xav1e = from_timer(xav1e, timer, ecc_counts_timer);
    int slice, cpu;
    for (slice = 0; slice < 2; ++slice) {
        volatile u8* hwregs = xav1e->subsys[slice].hwreg;
        for (cpu = 0; cpu < 8; ++cpu) {
            int rawVal = readl(hwregs + XAV1_SAR_ECC_COUNTS + cpu * 4);
            xav1e->ecc_counts[slice][cpu][0] = rawVal & ((1 << 20) - 1);
            xav1e->ecc_counts[slice][cpu][1] = rawVal >> 20;
        }
    }
    mod_timer(timer, timer->expires + 5 * HZ); // 5 seconds
}

int xav1_enc_init(struct sn_tranx_t *tdev)
{
    int i, result;
    struct xav1_enc *xav1e;
    int total_iosize;
    void *tmp_mem;

    xav1e = (struct xav1_enc *) kvzalloc(sizeof(*xav1e), GFP_KERNEL);
    if (!xav1e) {
        sn_pri(tdev, SN_ERR, "xav1_enc: kmalloc failed\n");
        return -ENOMEM;
    }
    tdev->modules[SN_MODULE_XAV1_ENC] = xav1e;
    xav1e->tdev = tdev;

    total_iosize = init_core_array(xav1e);
    if (!total_iosize) {
        sn_pri(xav1e->tdev, SN_ERR, "xav1_enc: total core count is zero\n");
        goto err_free_xav1e;
    }

#if SUB_SYS_VCE != 1 /* assume legoVSI FPGA which doesn't have xav1 HW */
    internal_configure_on(xav1e);
#endif

    spin_lock_init(&xav1e->slot_lock);

    result = sysfs_create_group(&tdev->misc_dev->this_device->kobj, &trans_xav1_attribute_group);
    if (result) {
        sn_pri(tdev, SN_ERR, "xav1: failed to create sysfs device attributes\n");
        goto err_free_xav1e;
    }

    result = reserve_io(xav1e);
    if(result < 0) {
        sn_pri(xav1e->tdev, SN_ERR, "xav1_enc: reserve io failed\n");
        goto err_free_xav1e;
    }

    tmp_mem = vzalloc(total_iosize);
    if (!tmp_mem) {
        sn_pri(xav1e->tdev, SN_ERR, "xav1_enc: malloc regs mem failed.\n");
        goto err_free_xav1e;
    }

    xav1e->regs_shadow = tmp_mem;
    for (i = 0; i < MAX_SLICE_NUM; i++) {
        if (xav1e->subsys[i].iosize) {
            xav1e->subsys[i].shadow = tmp_mem;
            tmp_mem += xav1e->subsys[i].iosize;
        }
    }

    for (i = 0; i < MAX_SLICE_NUM; i++) {
        if(xav1e->subsys[i].irq > 0) {
            result = request_irq(xav1e->subsys[i].irq, isr,
                                 IRQF_SHARED, xav1e->subsys[i].name, (void *)xav1e);
            if(result != 0) {
                if(result == -EINVAL) {
                    sn_pri(tdev, SN_ERR,
                        "xav1_enc: Bad IRQ:%d or handler\n",
                        xav1e->subsys[i].irq);
                } else if(result == -EBUSY) {
                    sn_pri(tdev, SN_ERR,
                        "xav1_enc: IRQ:%d busy\n",
                        xav1e->subsys[i].irq);
                }
                goto err_free_irq;
            } else {
                sn_pri(tdev, SN_INF, "xav1_enc:slice[%d]: pci_irq %d registered!\n", i, xav1e->subsys[i].irq);
            }
        } else
            sn_pri(tdev, SN_INF, "xav1_enc: IRQ:%d not in use!\n", i);
    }
    if (!(xav1e->log_wq = alloc_ordered_workqueue("wq_log", 0))) {
        goto err_free_irq;
    }
    init_slices(xav1e);
    xav1e->perfHandles[SN_PERF_ID_VPP] = sn_perf_register(tdev, "enc_vpp", SN_PERF_ID_VPP, xav1_enc_perf, 2);
    xav1e->perfHandles[SN_PERF_ID_ME] = sn_perf_register(tdev, "enc_me", SN_PERF_ID_ME, xav1_enc_perf, 2);
    xav1e->perfHandles[SN_PERF_ID_VCE] = sn_perf_register(tdev, "enc_vce", SN_PERF_ID_VCE, xav1_enc_perf, 2);
    xav1e->perfHandles[SN_PERF_ID_AV1] = sn_perf_register(tdev, "enc_av1", SN_PERF_ID_AV1, xav1_enc_perf, 2);
    xav1e->perfHandles[SN_PERF_ID_SYSTEMCPU] = sn_perf_register(tdev, "enc_systemcpu", SN_PERF_ID_SYSTEMCPU, xav1_enc_perf, 2);
    xav1e->perfHandles[SN_PERF_ID_INLINECPU] = sn_perf_register(tdev, "enc_inlinecpu", SN_PERF_ID_INLINECPU, xav1_enc_perf, 2);
    xav1e->perfHandles[SN_PERF_ID_LEGOCPU0] = sn_perf_register(tdev, "enc_legocpu0", SN_PERF_ID_LEGOCPU0, xav1_enc_perf, 2);
    xav1e->perfHandles[SN_PERF_ID_LEGOCPU1] = sn_perf_register(tdev, "enc_legocpu1", SN_PERF_ID_LEGOCPU1, xav1_enc_perf, 2);
    xav1e->perfHandles[SN_PERF_ID_CUTREE] = sn_perf_register(tdev, "enc_cutree", SN_PERF_ID_CUTREE, xav1_enc_perf, 2);
    xav1e->perfHandles[SN_PERF_ID_ALPHACLOCK] = sn_perf_register(tdev, "enc_alphaclock", SN_PERF_ID_ALPHACLOCK, xav1_enc_sync, 2);
    xav1e->perfHandles[SN_PERF_ID_LOOKAHEADCLOCK] = sn_perf_register(tdev, "enc_lookaheadclock", SN_PERF_ID_LOOKAHEADCLOCK, xav1_enc_sync, 2);

    /* load encoder firmware to FPS sub system */
    result = load_fps_subsystem(tdev);
    if(result) {
        goto err_free_irq;
    }

    xav1e->fatal_fw_error = false;

    timer_setup(&xav1e->ecc_counts_timer, ecc_counts_timer, 0);
    mod_timer(&xav1e->ecc_counts_timer, jiffies);
    sn_osal_register(tdev, "encoder", osal_encoder, osal_encoder_handler, NULL);
    sn_pri(tdev, SN_INF, "encoder: osal handler registered\n");

    sn_pri(tdev, SN_INF, "xav1_enc: module initialization done\n");
    return 0;

err_free_irq:
    for (i= 0; i < MAX_SLICE_NUM; i++) {
        if(xav1e->subsys[i].irq > 0)
          free_irq(xav1e->subsys[i].irq, (void *)xav1e);
    }
    vfree(xav1e->regs_shadow);

err_free_xav1e:
    kfree(xav1e);
    sn_pri(tdev, SN_ERR, "xav1_enc: module not inserted\n");
    return result;
}

void xav1_enc_release(struct sn_tranx_t *tdev)
{
    struct xav1_enc *xav1e = tdev->modules[SN_MODULE_XAV1_ENC];
    int i;

    del_timer_sync(&xav1e->ecc_counts_timer);
    /* free the IRQ */
    for (i = 0; i < MAX_SLICE_NUM; i++) {
        if(xav1e->subsys[i].irq > 0) {
            xav1_whole_subsys_soft_reset(xav1e, i);
            free_irq(xav1e->subsys[i].irq, (void *)xav1e);
        }
    }
#if SUB_SYS_VCE != 1
    internal_configure_off(xav1e);
#endif

    vfree(xav1e->regs_shadow);

    sysfs_remove_group(&tdev->misc_dev->this_device->kobj, &trans_xav1_attribute_group);
    destroy_workqueue(xav1e->log_wq);
    release_log_rings(tdev);
    kvfree(xav1e);

    sn_pri(tdev, SN_DBG, "xav1_enc: remove module done.\n");
}

long xav1_enc_ioctl(struct file *filp,
                    unsigned int cmd,
                    unsigned long argp,
                    struct sn_tranx_t *tdev)
{
    long ret;
    struct xav1_enc *xav1e = tdev->modules[SN_MODULE_XAV1_ENC];

    if (cmd != XAV1_ENC_IOCTL_TWO_SLICE_SOFT_RESET) {
        sn_pri(tdev, SN_ERR, "%s, cmd:0x%x error.\n", __func__, cmd);
        return -EINVAL;
    }
    del_timer_sync(&xav1e->ecc_counts_timer);
    if ((ret = xav1_whole_subsys_soft_reset(xav1e, 0))) {
        sn_pri(tdev, SN_ERR, "%s: s0 reset failed\n", __func__);
        return ret;
    }
    if ((ret = xav1_whole_subsys_soft_reset(xav1e, 1))) {
        sn_pri(tdev, SN_ERR, "%s: s1 reset failed\n", __func__);
        return ret;
    }
    if ((ret = load_fps_subsystem(tdev))) {
        sn_pri(tdev, SN_ERR, "fps firmware load failed\n");
        return ret;
    }
    timer_setup(&xav1e->ecc_counts_timer, ecc_counts_timer, 0);
    mod_timer(&xav1e->ecc_counts_timer, jiffies);

    xav1e->fatal_fw_error = false;
    setup_log_rings(tdev);
    return ret;
}

void xav1_close(struct sn_tranx_t *tdev, struct file *filp)
{
    struct xav1_enc *xav1e = tdev->modules[SN_MODULE_XAV1_ENC];
    int i;
    int k;
    int pending = 1;
    TRACE("xav1_close\n");
    for (k = 0; k < 500 && pending; ++k) {
        struct slice* slice = xav1e->slices;
        pending = 0;
        for (i = 0; i < MAX_SLICE_NUM; ++i, ++slice) {
            unsigned long flags;
            int j;
            struct slot* slot = slice->slots;
            spin_lock_irqsave(&xav1e->slot_lock, flags);
            for (j = 0; j < MAX_SLOTS_PER_SLICE; ++j, ++slot) {
                if (slot->filp == filp) {
                    if (!slot->pendingDelete) {
                        int retry = 1000;
                        slot->pendingDelete = 1;
                        ++pending;
                        {
                            struct xav1_msg msg;
                            msg.slice = i;
                            msg.count = 1;
                            msg.content[0] = 255 | (j << 10);
                            spin_unlock_irqrestore(&xav1e->slot_lock, flags);
                            // release slot_lock and take slice_lock instead
                            while (retry-- && send_msg_raw(xav1e, &msg, 1, 0) < 0) {
                                msleep(1);
                            }
                            spin_lock_irqsave(&xav1e->slot_lock, flags);
                        }
                        if (retry < 0) {
                            spin_unlock_irqrestore(&xav1e->slot_lock, flags);
                            xav1e->fatal_fw_error = true;
                            sn_pri(tdev, SN_ERR, "Firmware is unresponsive on slice %d\n", slice - xav1e->slices);
                            spin_lock_irqsave(&xav1e->slot_lock, flags);
                        }
                    } else {
                        ++pending;
                    }
                }
            }
            spin_unlock_irqrestore(&xav1e->slot_lock, flags);
        }
        if (pending) {
            // sn_pri(tdev, SN_ERR, "Waiting for firmware %u\n", k);
            msleep(1);
        }
    }
    if (pending) {
        xav1e->fatal_fw_error = true;
        sn_pri(tdev, SN_ERR, "**FATAL** Timeout waiting for slot teardown confirmation from firmware (close)\n");
    }
}
