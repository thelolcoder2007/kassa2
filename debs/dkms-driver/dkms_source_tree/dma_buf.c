//
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Copyright(C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include <linux/dma-buf.h>

#include "dma_buf.h"
#include "transcoder.h"
#include "memory_osal.h"

#if defined(TRACE_DMA_BUF)
#define TRACE(...) trace_printk(__VA_ARGS__); // msleep(1)
#else
#define TRACE(...)
#endif

static struct sg_table* map_dma_buf(struct dma_buf_attachment* attachment, enum dma_data_direction dir) {
  struct dma_buf* buf = attachment->dmabuf;
  struct sg_table* table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
  int ret;
  if (!table) {
    TRACE("table alloc failed\n");
    return ERR_PTR(-ENOMEM);
  }
  ret = sg_alloc_table(table, 1, GFP_KERNEL);
  if (ret) {
    TRACE("sg_alloc_table\n");
    kfree(table);
    return ERR_PTR(ret);
  }
  sg_dma_len(table->sgl) = buf->size;
  sg_dma_address(table->sgl) = (dma_addr_t) buf->priv;
  TRACE("map_dma_buf: %016llx for %lu\n", (uint64_t) buf->priv, buf->size);
  return table;
}

static void unmap_dma_buf(struct dma_buf_attachment* attachment, struct sg_table* table, enum dma_data_direction dir) {
  sg_free_table(table);
  kfree(table);
}

static void release(struct dma_buf* buf) {
  TRACE("release\n");
}

static const struct dma_buf_ops ops = {
  .map_dma_buf = map_dma_buf,
  .unmap_dma_buf = unmap_dma_buf,
  .release = release
};

long sn_dma_buf_ioctl(struct file* filp, unsigned cmd, unsigned long argp, struct sn_tranx_t* tdev) {
  void __user* arg = (void __user*) argp;
  struct sn_dma_buf dma_buf_cmd;
  int ret;
  uint64_t phys;
  void* addr;
  TRACE("ioctl\n");
  switch (cmd) {
  case IOCTL_DMA_BUF_WRAP:
    ret = copy_from_user(&dma_buf_cmd, arg, sizeof(dma_buf_cmd));
    if (ret) {
      TRACE("bad copy\n");
      return -EFAULT;
    }
    phys = sn_mem_osal_translate_handle(tdev, filp, dma_buf_cmd.address, dma_buf_cmd.size);
    if (phys == 0x1) {
      TRACE("bad handle/size\n");
      return -ENXIO;
    }
    addr = sn_mem_osal_translate_mmio(tdev, phys);
    if (!addr) {
      TRACE("Not MMIO\n");
      return -EINVAL;
    }
    {
      DEFINE_DMA_BUF_EXPORT_INFO(info);
      struct dma_buf* buf;
      info.exp_name = "ama";
      info.owner = THIS_MODULE;
      info.ops = &ops;
      info.size = dma_buf_cmd.size;
      info.flags = O_CLOEXEC;
      info.resv = NULL;
      info.priv = (void*) (tdev->bar4_base + phys);
      buf = dma_buf_export(&info);
      if (IS_ERR(buf)) {
        TRACE("Failed construction\n");
        return -EINVAL;
      }
      dma_buf_cmd.fd = dma_buf_fd(buf, O_CLOEXEC);
      ret = copy_to_user(arg, &dma_buf_cmd, sizeof(dma_buf_cmd));
      if (ret) {
        TRACE("bad return copy\n");
        dma_buf_put(buf);
      }
      return 0;
    }
  }
  return -EINVAL;
}

