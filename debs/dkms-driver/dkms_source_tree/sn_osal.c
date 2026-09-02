// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Xilinx Inc.
 */

#include "sn_osal.h"

#include <linux/version.h>
#include <linux/uaccess.h>

#include "memory_osal.h"

#if defined(TRACE_OSAL)
#define TRACE trace_printk
#else
#define TRACE(...)
#endif

#define SN_OSAL_MAGIC 0x88729311;
typedef struct osal_context {
  uint32_t                         magic; // determine valid
  struct list_head                 link;
  struct workqueue_struct*         wq;
  STRUCT_KFIFO(sn_osal_work*, 128) fifo; // no lock needed - one reader/one writer
  wait_queue_head_t                wait_queue;
  struct file*                     filp; // track owner
} osal_context;

typedef struct osal_accel {
  int                      registered;
  const char*              name;
  sn_osal_work_fn          workFn;
  sn_osal_close_fn         closeFn;
  uint32_t                 cmdId;
  // struct workqueue_struct* wq;
  struct list_head         contexts;
  spinlock_t               lock; // for context list
} osal_accel;

typedef struct {
  struct sn_tranx_t* tdev;
  struct osal_accel  accels[OSAL_COUNT];
} osal_dev;

int sn_osal_init(struct sn_tranx_t* tdev) {
  osal_dev* dev = (osal_dev*) kzalloc(sizeof(*dev), GFP_KERNEL);
  if (!dev) {
    sn_pri(tdev, SN_ERR, "sn_osal_init: kzalloc failed\n");
    return -ENOMEM;
  }
  tdev->modules[SN_MODULE_OSAL] = dev;
  dev->tdev = tdev;
  TRACE("init\n");
  return 0;
// error:
  kfree(dev);
  return -EINVAL;
}

void sn_osal_release(struct sn_tranx_t* tdev) {
  osal_dev* dev = (osal_dev*) tdev->modules[SN_MODULE_OSAL];
  kfree(dev);
}

int sn_osal_register(struct sn_tranx_t* tdev, const char* name, osal_accelerator accel, sn_osal_work_fn workFn, sn_osal_close_fn closeFn) {
  osal_dev* dev = (osal_dev*) tdev->modules[SN_MODULE_OSAL];
  osal_accel* entry = dev->accels + accel;
  if (entry->registered) {
    TRACE("%s already registered\n", name);
    return -1;
  }
  entry->registered = 1;
  entry->name = name;
  entry->workFn = workFn;
  entry->closeFn = closeFn;
  entry->cmdId = 5000;
  spin_lock_init(&entry->lock);
  INIT_LIST_HEAD(&entry->contexts);
  TRACE("Registered: %s\n", name);
  return 0;
}

void sn_osal_finish_work(sn_osal_work* work) {
  osal_context* context = (osal_context*) work->handle;
#if defined(TRACE_OSAL)
  osal_dev* dev = work->tdev->modules[SN_MODULE_OSAL];
  osal_accel* accel = dev->accels + work->accel;
  TRACE("work completed on %s\n", accel->name);
#endif
  kfifo_put(&context->fifo, work); // TODO: handle full
  wake_up_all(&context->wait_queue);
}

static int handle_create(osal_dev* dev, char __user* user, struct file* filp) {
  int ret;
  ma_create_ctx msg;
  osal_accel* accel;
  osal_context* context;
  if ((ret = copy_from_user(&msg, user, sizeof(msg)))) {
    TRACE("COPYIN: %d\n", ret);
    return -EFAULT;
  }
  if (msg.accel >= OSAL_COUNT || !(accel = dev->accels + msg.accel)->registered) {
    TRACE("accel %u not valid or not registered\n", msg.accel);
    return -1;
  }
  context = (osal_context*) kzalloc(sizeof(osal_context), GFP_KERNEL);
  if (!context) {
    return -ENOMEM;
  }
  TRACE("Creating: %p\n", context);
  msg.handle = (uintptr_t) context;
  if ((ret = copy_to_user(user, &msg, sizeof(msg)))) {
    TRACE("COPYOUT: %d\n", ret);
    kfree(context);
    return -EFAULT;
  }
  if (!(context->wq = alloc_ordered_workqueue("wq_%s", 0, accel->name))) {
    TRACE("%s cannot create workqueue\n", accel->name);
    kfree(context);
    return -1;
  }
  context->magic = SN_OSAL_MAGIC;
  INIT_LIST_HEAD(&context->link);
  INIT_KFIFO(context->fifo);
  init_waitqueue_head(&context->wait_queue);
  context->filp = filp;
  spin_lock(&accel->lock);
  list_add(&context->link, &accel->contexts);
  spin_unlock(&accel->lock);
  TRACE("Create done: %p\n", context);
  return ret;
}

static int handle_delete(osal_dev* dev, char __user* user, struct file* filp) {
  int ret;
  ma_delete_ctx msg;
  osal_accel* accel;
  osal_context* context;
  sn_osal_work* work;
  if ((ret = copy_from_user(&msg, user, sizeof(msg)))) {
    TRACE("COPYIN: %d\n", ret);
    return -EFAULT;
  }
  if (msg.accel >= OSAL_COUNT || !(accel = dev->accels + msg.accel)->registered) {
    TRACE("accel %u not valid or not registered\n", msg.accel);
    return -1;
  }
  context = (osal_context*) msg.handle;
  if (filp != context->filp) {
    return -EPERM;
  }
  TRACE("Deleting: %p\n", context);
  destroy_workqueue(context->wq);
  while (kfifo_get(&context->fifo, &work)) {
    TRACE("Freeing unread work: %p\n", work);
    kfree(work);
  }
  spin_lock(&accel->lock);
  list_del(&context->link);
  spin_unlock(&accel->lock);
  kfree(context);
  TRACE("Delete done\n");
  return 0;
}


static int do_patch(struct sn_tranx_t* tdev, sn_osal_work* work, uint32_t numPatches) {
  int j;
  uint32_t* patches = work->data + work->cmd.numData;
  for (j = 0; j < numPatches; ++j) {
    uint64_t newAddr;
    uint64_t addr;
    uint32_t* data = work->data + patches[j];
    if (patches[j] + 1 > work->cmd.numData) {
      TRACE("PATCH %u out of range %u\n", j, work->cmd.numData);
      return -1;
    }
    addr = (((uint64_t) data[1]) << 32) + data[0];
    newAddr = sn_mem_osal_translate_handle(tdev, work->filp, addr, 0);
    data[0] = newAddr;
    data[1] = newAddr >> 32;
    TRACE("Patched: %016llx to %016llx at word offset %u\n", addr, newAddr, (unsigned) (data - work->data));
  }
  return 0;
}

static int handle_submit(struct sn_tranx_t* tdev, char __user* user, struct file* filp) {
  osal_dev* dev = (osal_dev*) tdev->modules[SN_MODULE_OSAL];
  osal_submit submit;
  osal_accel* accel;
  osal_context* context;
  sn_osal_work* work;
  int ret;
  if ((ret = copy_from_user(&submit, user, sizeof(osal_submit)))) {
    TRACE("COPYIN: %d\n", ret);
    return -EFAULT;
  }
  if (submit.header.accel >= OSAL_COUNT || !(accel = dev->accels + submit.header.accel)->registered) {
    TRACE("accel %u not valid or not registered\n", submit.header.accel);
    return -EINVAL;
  }
  submit.maxNumResp = max(submit.maxNumResp, submit.cmd.numData + submit.numPatches);
  work = (sn_osal_work*) kmalloc(sizeof(sn_osal_work) + submit.maxNumResp * sizeof(uint32_t), GFP_KERNEL);
  work->tdev = tdev;
  work->filp = filp;
  work->task = current;
  work->handle = submit.header.handle;
#if defined(TRACE_OSAL)
  work->accel = submit.header.accel;
#endif
  work->cmdId = accel->cmdId;
  work->cmd = submit.cmd;
  work->numResp = submit.maxNumResp;

  if (submit.cmd.numData && (ret = copy_from_user(work->data, submit.cmdData, sizeof(uint32_t) * work->cmd.numData))) {
    TRACE("COPYIN data: %d\n", ret);
    goto error;
  }
  if (submit.numPatches && (ret = copy_from_user(work->data + submit.cmd.numData, submit.patches, sizeof(uint32_t) * submit.numPatches))) {
    TRACE("COPYIN patches: %d\n", ret);
    goto error;
  }
  context = (osal_context*) work->handle;
  if (filp != context->filp) {
    ret = -EPERM;
    goto error;
  }
  INIT_WORK(&work->work, accel->workFn);
  if (submit.numPatches && do_patch(tdev, work, submit.numPatches)) {
    TRACE("accel cmdId %u patching failed\n", accel->cmdId);
    ret = -EFAULT;
    goto error;
  }
  if ((ret = copy_to_user(user, &accel->cmdId, sizeof(accel->cmdId)))) {
    TRACE("accel cmdId %u copy failed\n", accel->cmdId);
    kfree(work);
    ret = -EFAULT;
    goto error;
  }

  TRACE("DISPATCHING accel: %u  cmdId: %u\n", work->accel, accel->cmdId);
  queue_work(context->wq, &work->work); // TODO (error cases?)
  ++accel->cmdId;
  return 0;
error:
  kfree(work);
  return ret;
}

static int handle_wait(osal_dev* dev, char __user* user, struct file* filp) {
  int ret, ret_val;
  osal_accel* accel;
  osal_context* context;
  sn_osal_work* work;
  osal_wait msg;
  if ((ret = copy_from_user(&msg, user, sizeof(msg)))) {
    TRACE("COPYIN: %d\n", ret);
    return -EFAULT;
  }
  if (msg.header.accel >= OSAL_COUNT || !(accel = dev->accels + msg.header.accel)->registered) {
    TRACE("accel %u not valid or not registered\n", msg.header.accel);
    return -EINVAL;
  }
  context = (osal_context*) msg.header.handle;
  if (filp != context->filp) {
    return -EPERM;
  }
  TRACE("WAITING: %u\n", msg.timeoutMs);
  ret = wait_event_interruptible_timeout(context->wait_queue, !kfifo_is_empty(&context->fifo), msg.timeoutMs * HZ / 1000);
  TRACE("DONE WAIT: %d (%u)\n", ret, ret * 1000 / HZ);
  if (ret < 0) {
    return ret;
  }
  msg.timeoutMs = ret * 1000 / HZ;
  if (copy_to_user(user + offsetof(osal_wait, timeoutMs), &msg.timeoutMs, sizeof(msg.timeoutMs))) {
    TRACE("msg.timeoutMs copy failed\n");
    return -1;
  }
  if (!ret) { // timeout
    return 0;
  }
  if (!kfifo_get(&context->fifo, &work)) {
    TRACE("fifo empty\n");
    return -1;
  }
  if (msg.numData < work->numResp) {
    TRACE("No room %d need %d\n", msg.numData, work->numResp);
    msg.numData = -1;
  }
  if (copy_to_user(user + offsetof(osal_wait, numData), &msg.numData, sizeof(msg.numData))) {
    TRACE("msg.numData copy failed\n");
    return -1;
  }
  if (msg.numData > 0 && (ret_val = copy_to_user(msg.respData, work->data, work->numResp * sizeof(uint32_t)))) {
    TRACE("response copy failed\n");
    kfree(work);
    return -EFAULT;
  }
  kfree(work);
  return 0;
}

long sn_osal_ioctl(struct file* filp, unsigned int cmd, unsigned long argp, struct sn_tranx_t* tdev) {
  osal_dev* dev = (osal_dev*) tdev->modules[SN_MODULE_OSAL];
  void __user* arg = (void __user*) argp;
  switch (cmd) {
  case IOCTL_MA_CREATE_CTX:
    TRACE("IOCTL_MA_CREATE_CTX\n");
    return handle_create(dev, arg, filp);
  case IOCTL_MA_DELETE_CTX:
    TRACE("IOCTL_MA_DELETE_CTX\n");
    return handle_delete(dev, arg, filp);
  case IOCTL_MA_SUBMIT_CMD:
    TRACE("IOCTL_MA_SUBMIT_CMD\n");
    return handle_submit(tdev, arg, filp);
  case IOCTL_MA_WAIT_RSP:
    TRACE("IOCTL_MA_WAIT_RSP\n");
    return handle_wait(dev, arg, filp);
  default:
    return -EINVAL;
  }
}

void sn_osal_close(struct sn_tranx_t* tdev, struct file* filp) {
  osal_dev* dev = (osal_dev*) tdev->modules[SN_MODULE_OSAL];
  osal_accel* accel = dev->accels;
  osal_context* context;
  osal_context* found;
  osal_context* tmp;
  int i;
  for (i = 0; i < OSAL_COUNT; ++i, ++accel) {
    if (accel->registered) {
      do {
        found = NULL;
        spin_lock(&accel->lock);
        list_for_each_entry_safe(context, tmp, &accel->contexts, link) {
          if (context->filp == filp) {
            found = context;
            break;
          }
        }
        if (found) {
          list_del(&found->link);
        }
        spin_unlock(&accel->lock);
        if (found) {
          sn_osal_work* work;
          TRACE("Purging abandoned context: %p\n", found);
          if (accel->closeFn) {
            accel->closeFn(tdev, filp);
          }
          destroy_workqueue(found->wq);
          while (kfifo_get(&found->fifo, &work)) {
            TRACE("Freeing unread work: %p\n", work);
            kfree(work);
          }
          kfree(found);
        }
      } while (found);
    }
  }
}
