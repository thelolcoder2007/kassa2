// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Xilinx Inc.
 */

#include "sn_perf.h"

#include <linux/version.h>
#include <linux/kfifo.h>
#include <linux/pagemap.h>

#if defined(TRACE_PERF)
#define TRACE trace_printk
#else
#define TRACE(...)
#endif

#define MAX_LOAD_ATTR_NUM 4

struct perf_node;

typedef struct {
  struct sn_tranx_t*                              tdev;
  struct perf_node*                               nodes[SN_PERF_ID_COUNT];
  struct kobject*                                 dir;
  STRUCT_KFIFO(sn_perf_event, SN_PERF_FIFO_DEPTH) fifo;
  wait_queue_head_t                               wait_queue;
  spinlock_t                                      fifo_lock;
  struct file*                                    filp;
} perf_dev;

typedef struct perf_node {
  char                name[20];
  SN_PERF_IP_ID       ipId;
  sn_perf_callback_fn callback;
  int                 loads[MAX_LOAD_ATTR_NUM];
  uint32_t            generations[MAX_LOAD_ATTR_NUM];

  perf_dev*        dev;
  struct kobject*  dir;
  struct kobj_attribute load_attrs[MAX_LOAD_ATTR_NUM];
  int load_attr_num;
} perf_node;

static ssize_t load_show(struct kobject* kobj, struct kobj_attribute* attr, char* buf) {
  unsigned index = attr->attr.name[4] - '0';
  perf_node* node = container_of(attr, perf_node, load_attrs[index]);
  return sysfs_emit(buf, "%u @%u\n", node->loads[index], node->generations[index]);
}

int sn_perf_init(struct sn_tranx_t* tdev) {
  perf_dev* dev = (perf_dev*) kzalloc(sizeof(*dev), GFP_KERNEL);
  if (!dev) {
    sn_pri(tdev, SN_ERR, "sn_perf_init: kmalloc failed\n");
    return -ENOMEM;
  }
  TRACE("alloc\n");
  tdev->modules[SN_MODULE_PERF] = dev;
  dev->tdev = tdev;
  INIT_KFIFO(dev->fifo);
  init_waitqueue_head(&dev->wait_queue);
  spin_lock_init(&dev->fifo_lock);
  dev->dir = kobject_create_and_add("perf", &tdev->misc_dev->this_device->kobj);
  if (!dev->dir) {
    sn_pri(tdev, SN_ERR, "sn_perf dir kobj\n");
    goto error;
  }
  TRACE("ok\n");
  return 0;
error:
  kfree(dev);
  return -EINVAL;
}

void sn_perf_release(struct sn_tranx_t* tdev) {
  perf_dev* dev = (perf_dev*) tdev->modules[SN_MODULE_PERF];
  unsigned i, j;
  for (i = 0; i < SN_PERF_ID_COUNT; ++i) {
    perf_node* node = dev->nodes[i];
    if (node) {
      if (node->ipId != SN_PERF_ID_ALPHACLOCK && node->ipId != SN_PERF_ID_LOOKAHEADCLOCK) {
        for (j = 0; j < node->load_attr_num; ++j) {
          sysfs_remove_file(node->dir, &(node->load_attrs[j].attr));
        }
        kobject_put(node->dir);
      }
      kfree(node);
    }
  }
  kobject_put(dev->dir);
  kfree(tdev->modules[SN_MODULE_PERF]);
}

#define LOAD_ATTR(ID) { .attr = { .name = "load" # ID, .mode = 0444 }, .show = load_show }

SnPerfHandle sn_perf_register(struct sn_tranx_t* tdev, const char* name, SN_PERF_IP_ID ipId, sn_perf_callback_fn callback, int num) {
  int i;
  perf_dev* dev = (perf_dev*) tdev->modules[SN_MODULE_PERF];
  perf_node* node = (perf_node*) kmalloc(sizeof(*node), GFP_KERNEL);
  perf_node tmp = { .load_attrs = { LOAD_ATTR(0), LOAD_ATTR(1), LOAD_ATTR(2), LOAD_ATTR(3) }};
  *node = tmp;
  node->load_attr_num = num;
  if (ipId != SN_PERF_ID_ALPHACLOCK && ipId != SN_PERF_ID_LOOKAHEADCLOCK) {
      node->dir = kobject_create_and_add(name, dev->dir);
      TRACE("NODE %s dir %p @ %p\n", name, node, node->dir);
    for (i = 0; i < num; ++i) {
      if (sysfs_create_file(node->dir, &(node->load_attrs[i].attr))) {
        sn_pri(tdev, SN_ERR, "sn_perf dir load\n");
      }
    }
  }
  node->ipId = ipId;
  node->callback = callback;
  node->dev = dev;
  dev->nodes[ipId] = node;
  return node;
}

void sn_perf_record(SnPerfHandle handle, sn_perf_event* event) {
  perf_node* node = (perf_node*) handle;
  perf_dev* dev = node->dev;
  if (!event || event->ipId >= SN_PERF_ID_COUNT) {
    return;
  }

  if (event->ipId >= SN_PERF_ID_ALPHACLOCK) {
    event->kernelTs = ktime_to_ns(ktime_get());
  }
  else {
    event->kernelTs = 0;
  }
 
  TRACE("EVENT\n");
  if (dev->filp) {
    TRACE("PUSHED\n");
    kfifo_in_spinlocked(&dev->fifo, event, 1, &dev->fifo_lock);
    wake_up_all(&dev->wait_queue);
  }
}

void sn_perf_load(SnPerfHandle handle, __u32 instance, __u32 load) {
  perf_node* node = (perf_node*) handle;
  TRACE("LOAD %u %u\n", instance, load);
  if (instance < node->load_attr_num) {
    node->loads[instance] = load;
    ++node->generations[instance];
  }
}

static int handle_cmd(perf_dev* dev, sn_perf_cmd __user* user) {
  int ret;
  sn_perf_cmd cmd;
  if ((ret = copy_from_user(&cmd, user, sizeof(cmd)))) {
    TRACE("COPYIN: %d\n", ret);
    return -EFAULT;
  }
  if (cmd.ipId >= SN_PERF_ID_COUNT || !dev->nodes[cmd.ipId] || !dev->nodes[cmd.ipId]->callback) {
    return -EINVAL;
  }
  return dev->nodes[cmd.ipId]->callback(dev->tdev, cmd.ipId, cmd.cmd, cmd.arg);
}

static int handle_poll(perf_dev* dev, sn_perf_poll __user* user) {
  int ret;
  sn_perf_poll poll;
  if ((ret = copy_from_user(&poll, user, sizeof(poll)))) {
    TRACE("COPYIN: %d\n", ret);
    return -EFAULT;
  }
  ret = wait_event_interruptible_timeout(dev->wait_queue, !kfifo_is_empty(&dev->fifo), poll.timeoutMs * HZ / 1000);
  if (ret < 0) {
    TRACE("ERR");
    return ret;
  }
  poll.timeoutMs = ret * 1000 / HZ;
  if (ret) {
    sn_perf_event __user* out = (sn_perf_event __user*) (user + 1);
    int i;
    sn_perf_event event;
    for (i = 0; i < poll.count; ++i) {
      if (!kfifo_get(&dev->fifo, &event)) {
        break;
      }
      if ((ret = copy_to_user(out + i, &event, sizeof(event)))) {
        TRACE("COPYOUT: %d\n", ret);
      }
    }
    poll.count = i;
  } else {
    poll.count = 0;
  }
  return copy_to_user(user, &poll, sizeof(poll));
}

long sn_perf_ioctl(struct file* filp, unsigned int cmd, unsigned long argp, struct sn_tranx_t* tdev) {
  perf_dev* dev = (perf_dev*) tdev->modules[SN_MODULE_PERF];
  void __user* arg = (void __user*) argp;
  if (!dev->filp) {
    TRACE("set filp\n");
    dev->filp = filp;
  } else if (filp != dev->filp) {
    TRACE("not owner\n");
    return -EINVAL;
  }
  switch (cmd) {
  case IOCTL_PERF_CMD:
    TRACE("cmd\n");
    return handle_cmd(dev, (sn_perf_cmd __user*) arg);
  case IOCTL_PERF_POLL:
    TRACE("poll\n");
    return handle_poll(dev, (sn_perf_poll __user*) arg);
  default:
    return -EINVAL;
  }
}

void sn_perf_close(struct sn_tranx_t* tdev, struct file* filp) {
  perf_dev* dev = (perf_dev*) tdev->modules[SN_MODULE_PERF];
  if (dev->filp == filp) {
    unsigned i;
    for (i = 0; i < SN_PERF_ID_COUNT; ++i) {
      if (dev->nodes[i] && dev->nodes[i]->callback) {
        dev->nodes[i]->callback(dev->tdev, i, SN_PERF_CMD_STOPALL, 0);
      }
    }
    TRACE("clear filp\n");
    dev->filp = NULL;
  }
}
