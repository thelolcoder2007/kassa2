// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Xilinx Inc.
 */

#include "memory_osal.h"

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/bitmap.h>

#include "transcoder.h"

#if defined(TRACE_OSAL_MEMORY)
#define TRACE(...) trace_printk(__VA_ARGS__)
#else
#define TRACE(...)
#endif

// #define TRACE_EVENT(...) trace_printk("_EVENT_ " __VA_ARGS__); msleep(1)
#define TRACE_EVENT(...)

#define MEM_PAGE_SIZE PAGE_SIZE
#define MAX_NUM_TASKS (1 << 10) // must be power of 2, max 1 << 15

typedef struct {
  struct rb_node addr_node;
  uint32_t used: 1;
  uint32_t region : 2;
  uint32_t start_page: 29;
  uint32_t num_pages;
} alloc_t;

#define NUM_BASE_ALLOCATIONS ((PAGE_SIZE) / sizeof(alloc_t))

typedef struct {
  struct file* filp;
  uint32_t     num_allocated;
  uint32_t     num_free; // free in array
  alloc_t*     allocations; // contiguous array of allocations (index used for handle)
  struct rb_root addr_root;
  struct mutex lock; // deal with multithreaded access to the same task
  int is_live; // try to handle malicious races
} task_t;

#define NUM_REGIONS 3 // slice0 / slice1 / mmio

#define DECLARE_REGION_PAGES(NAME, SADDR, EADDR) \
  static uint32_t NAME##_NUM_PAGES = ((EADDR) - (SADDR)) / MEM_PAGE_SIZE;

#define DECLARE_REGION_BITMAP(NAME, SADDR, EADDR) \
  DECLARE_BITMAP(NAME##_bits, ((EADDR) - (SADDR)) / MEM_PAGE_SIZE)

#define DO_REGION_INIT(NAME) \
  bitmap_zero(mem->NAME##_bits, NAME##_NUM_PAGES);

// #define REGION_MMIO_START   0x0 ## 1100 ## 0000ULL // Scaler does not seem to work with this address
// #define REGION_MMIO_START   0x0 ## 0800 ## 0000ULL
#define REGION_MMIO_START   0x0 ## 1180 ## 0000ULL
#define REGION_MMIO_END     0x0 ## 2000 ## 0000ULL

#define REGION_SLICE0_START 0x0 ## 2080 ## 0000ULL
#define REGION_SLICE0_END   0x0 ## e000 ## 0000ULL

#define REGION_SLICE1_START 0x2 ## 0000 ## 0000ULL
#define REGION_SLICE1_END   0x2 ## E000 ## 0000ULL

DECLARE_REGION_PAGES(mmio,   REGION_MMIO_START,   REGION_MMIO_END);
DECLARE_REGION_PAGES(slice0, REGION_SLICE0_START, REGION_SLICE0_END);
DECLARE_REGION_PAGES(slice1, REGION_SLICE1_START, REGION_SLICE1_END);

typedef struct {
  uint64_t       base_addr;
  unsigned long* bits;
  unsigned       num_bits;
  atomic_t       num_free;
  struct mutex   lock;
} region_t;

typedef struct {
  struct sn_tranx_t* dev;
  uint32_t           num_tasks;
  task_t             tasks[MAX_NUM_TASKS];
  struct mutex       taskid_lock;
  region_t           regions[NUM_REGIONS];
  DECLARE_BITMAP(task_bits, MAX_NUM_TASKS);
  DECLARE_REGION_BITMAP(slice0, REGION_SLICE0_START, REGION_SLICE0_END);
  DECLARE_REGION_BITMAP(slice1, REGION_SLICE1_START, REGION_SLICE1_END);
  DECLARE_REGION_BITMAP(mmio, REGION_MMIO_START, REGION_MMIO_END);
} memory_t;

// memory handles consist of (low to high)
// 32 bits of physical address offset // limits single allocation to 4GB
// 16 bits task-id (redundant here, but useful for transfers)
// 16 bits of slot offset (limits allocations to 32K/task);

typedef struct {
  union {
    struct {
      uint64_t offset : 32;
      uint64_t slot : 16;
      uint64_t task_id : 15;
      uint64_t is_handle: 1;
    };
    uint64_t value;
  };
} handle_t;

static_assert(sizeof(handle_t) == sizeof(uint64_t));

static inline int check_task_id(memory_t* mem, struct file* filp, int id) {
  int ret = (id < 0 || id >= MAX_NUM_TASKS || !test_bit(id, mem->task_bits) || mem->tasks[id].filp != filp) ? -1 : 0;
#if defined(TRACE_MEMORY)
  if (ret) {
    TRACE("*** Invalid task: %d\n", id);
  }
#endif
  return ret;
}

static inline int check_lock_task_id(memory_t* mem, struct file* filp, int id, task_t* task) {
  if (check_task_id(mem, filp, id)) {
    return -1;
  }
  mutex_lock(&task->lock);
  if (!task->is_live) {
    mutex_unlock(&task->lock);
    return -1;
  }
  return 0;
}

static int get_task_id(memory_t* mem, struct file* filp) {
  unsigned long bit;
  mutex_lock(&mem->taskid_lock);
  bit = find_first_zero_bit(mem->task_bits, MAX_NUM_TASKS);
  if (bit == MAX_NUM_TASKS) {
    TRACE("*** Out of tasks\n");
    mutex_unlock(&mem->taskid_lock);
    return -ENOMEM;
  }
  // TRACE("ID: %lu\n", bit);
  {
    task_t* task = mem->tasks + bit;
    if (!(task->allocations = krealloc_array(task->allocations, NUM_BASE_ALLOCATIONS, sizeof(alloc_t), GFP_KERNEL))) {
      mutex_unlock(&mem->taskid_lock);
      TRACE("*** Out of memory\n");
      return -ENOMEM;
    }
    memset(task->allocations, '\0', sizeof(alloc_t) * NUM_BASE_ALLOCATIONS);
    task->filp = filp;
    task->num_allocated = 0;
    task->num_free = NUM_BASE_ALLOCATIONS;
    task->addr_root.rb_node = NULL;
    task->is_live = 1;
    mutex_init(&task->lock);
  }
  set_bit(bit, mem->task_bits);
  ++mem->num_tasks;
  mutex_unlock(&mem->taskid_lock);
  TRACE_EVENT("T+ %lu\n", bit);
  return bit;
}

// Need to handle intentional races on some of these functions (double free of same task, etc.)

static uint32_t get_task_region(memory_t* mem, int id, uint32_t* offset_num, uint32_t* offset_den) {
  // don't bother locking, hence the atomics
  uint32_t r0 = atomic_read(&mem->regions[0].num_free);
  uint32_t r1 = atomic_read(&mem->regions[1].num_free);
  uint32_t region;
  int offset = 0;
  if (!r1) {
    region = 0;
  } else {
    uint64_t ratio = ((uint64_t) r0 << 32) / r1;
    // could precompute fixed ratio - not worth the trouble - maybe the optimizer will see it
    uint64_t ratio2 = ((uint64_t) slice0_NUM_PAGES << 32) / slice1_NUM_PAGES;
    region = (ratio <= ratio2);
    TRACE("ratio: %llu vs. %llu  Region: %u\n", ratio, ratio2, region);
  }
  if (id) {
    int scale = 32 - __builtin_clz(id);
    // Jim's stick-breaking interleaved allocation scheme
    offset = (MAX_NUM_TASKS >> scale) + (MAX_NUM_TASKS >> (scale - 1)) * (id ^ (1 << (scale - 1)));
  }
  TRACE("REGION %d  %u vs. %u  %u/%u\n", region, r0, r1, offset, MAX_NUM_TASKS);
  *offset_num = offset;
  *offset_den = MAX_NUM_TASKS;
  return region;
}

static handle_t INVALID_ADDR = { .offset = 1 };

static int get_pages(region_t* region, int num, int den, int npages) {
  int ret = -1;
  unsigned long start = region->num_bits * num / den;
  unsigned long pos = start;
  int wrap = 0;
  mutex_lock(&region->lock);
  if (npages > atomic_read(&region->num_free)) { // trivial OOM
    goto exit;
  }
  for (;;) {
    unsigned long range = (!wrap) ? start : region->num_bits;
    pos = bitmap_find_next_zero_area(region->bits, range, pos, npages, 0);
    if (pos < range) {
      // TRACE("FOUND: %ld\n", pos);
      bitmap_set(region->bits, pos, npages);
      atomic_sub(npages, &region->num_free);
      ret = pos;
      goto exit;
    }
    if (wrap) {
      TRACE_EVENT("NOFIT %u %u\n", region->num_bits, npages);
      goto exit;
    }
    wrap = 1;
    pos = 0;
  }
exit:
  mutex_unlock(&region->lock);
  return ret;
}

static void insert_allocation(memory_t* mem, struct rb_root* root, struct rb_node* node) {
  struct rb_node** new = &root->rb_node;
  struct rb_node* parent = NULL;
  alloc_t* alloc = container_of(node, alloc_t, addr_node);
  uint64_t addr = mem->regions[alloc->region].base_addr + alloc->start_page * MEM_PAGE_SIZE;
  while (*new) {
    alloc_t* newAlloc = container_of(*new, alloc_t, addr_node);
    uint64_t newAddr = mem->regions[newAlloc->region].base_addr + newAlloc->start_page * MEM_PAGE_SIZE;
    parent = *new;
    if (addr < newAddr) {
      new = &(*new)->rb_left;
    } else {
      new = &(*new)->rb_right;
    }
  }
  rb_link_node(node, parent, new);
  rb_insert_color(node, root);
}

static handle_t alloc_mem(memory_t* mem, struct file* filp, int id, uint32_t size, int mmio) {
  task_t* task;
  int slot = 0;
  task = mem->tasks + id;
  if (check_lock_task_id(mem, filp, id, task)) {
    return INVALID_ADDR;
  }
  // find available allocation slot
  if (!task->num_free) { // allocate another page
    char* oldAllocations = (char*) task->allocations;
    if (!(task->allocations = krealloc_array(task->allocations, task->num_allocated + NUM_BASE_ALLOCATIONS, sizeof(alloc_t), GFP_KERNEL))) {
      mutex_unlock(&task->lock);
      return INVALID_ADDR;
    }
    memset(task->allocations + task->num_allocated, '\0', sizeof(alloc_t) * NUM_BASE_ALLOCATIONS);
    task->num_free += NUM_BASE_ALLOCATIONS - 1;
    slot = task->num_allocated;
    // Oh no!  rb_node / rb_root need fixup on relocation!
    {
      int64_t adjust = (char*) task->allocations - oldAllocations;
      if (adjust) {
        if (task->addr_root.rb_node) {
          task->addr_root.rb_node = (struct rb_node*) ((char*) task->addr_root.rb_node + adjust);
        }
        for (slot = 0; slot < task->num_allocated; ++slot) {
          alloc_t* alloc = task->allocations + slot;
          if (rb_parent(&alloc->addr_node)) {
            alloc->addr_node.__rb_parent_color += adjust;
          }
          if (alloc->addr_node.rb_left) {
            alloc->addr_node.rb_left = (struct rb_node*) ((char*) alloc->addr_node.rb_left + adjust);
          }
          if (alloc->addr_node.rb_right) {
            alloc->addr_node.rb_right = (struct rb_node*) ((char*) alloc->addr_node.rb_right + adjust);
          }
        }
      }
    }
  } else {
    while (task->allocations[slot].used) {
      ++slot;
    }
    RB_CLEAR_NODE(&task->allocations[slot].addr_node);
    --task->num_free;
  }
  ++task->num_allocated;
  task->allocations[slot].used = 1;
  task->allocations[slot].num_pages = 0;
  {
    int npages = (size + MEM_PAGE_SIZE - 1) / MEM_PAGE_SIZE;
    int num;
    int den;
    int region_num = (mmio) ? 2 : get_task_region(mem, id, &num, &den);
    region_t* region = mem->regions + region_num;
    int offset = get_pages(region, num, den, npages);
    if (offset < 0) {
      TRACE_EVENT("X %u %d %d\n", id, region_num, npages);
      sn_pri(mem->dev, SN_ERR, "Failed allocation for region %u, size %llu\n", region_num, size);
      task->allocations[slot].used = 0;
      --task->num_allocated;
      ++task->num_free;
      mutex_unlock(&task->lock);
      return INVALID_ADDR;
    }
    TRACE("ALLOC TASK: %d  SLOT: %d  REG: %d  OFF: %d  LEN: %d  PHY: %016llx\n", id, slot, region_num, offset, npages, region->base_addr + offset * MEM_PAGE_SIZE);
    TRACE_EVENT("A %u %d %d %d\n", id, region_num, offset, npages);
    task->allocations[slot].region = region_num;
    task->allocations[slot].start_page = offset;
    task->allocations[slot].num_pages = npages;
    insert_allocation(mem, &task->addr_root, &task->allocations[slot].addr_node);
  }
  mutex_unlock(&task->lock);
  {
    handle_t handle = { .offset = 0, .slot = slot, .task_id = id, .is_handle = 1 };
    return handle;
  }
}

static void sn_release_pages(region_t* region, int offset, int npages) {
  mutex_lock(&region->lock);
  bitmap_clear(region->bits, offset, npages);
  atomic_add(npages, &region->num_free);
  mutex_unlock(&region->lock);
}

static handle_t translate_mem(memory_t* mem, int id, uint64_t address) {
  task_t* task = mem->tasks + id;
  struct rb_node* node;
  mutex_lock(&task->lock);
  node = task->addr_root.rb_node;
  while (node) {
    alloc_t* alloc = container_of(node, alloc_t, addr_node);
    uint64_t addr = mem->regions[alloc->region].base_addr + alloc->start_page * MEM_PAGE_SIZE;
    uint64_t end_addr = addr + alloc->num_pages * MEM_PAGE_SIZE;
    if (address < addr) {
      node = node->rb_left;
    } else if (address >= end_addr) {
      node = node->rb_right;
    } else {
      handle_t ret = { .offset = address - addr, .slot = alloc - task->allocations, .task_id = id, .is_handle = 1 };
      mutex_unlock(&task->lock);
      return ret;
    }
  }
  mutex_unlock(&task->lock);
  sn_pri(mem->dev, SN_ERR, "FAILED translate_mem: %016llx [%u]\n", address, id);
  dump_stack();
  return INVALID_ADDR;
}

static int free_mem(memory_t* mem, struct file* filp, int id, handle_t handle) {
  task_t* task;
  int slot;
  int ret = -1;
  if (check_task_id(mem, filp, id)) {
    return -EINVAL;
  }
  if (!handle.is_handle) {
    handle = translate_mem(mem, id, handle.value);
  }
  if (handle.offset || handle.task_id != id) {
    TRACE("Bad handle: %016llx\n", handle.value);
    return -EINVAL;
  }
  task = mem->tasks + id;
  mutex_lock(&task->lock);
  if (!task->is_live) {
    ret = -EINVAL;
    goto exit;
  }
  slot = handle.slot;
  if (slot > task->num_allocated + task->num_free || !task->allocations[slot].used) {
    TRACE("INVALID SLOT: %d\n", slot);
    goto exit;
  }
  {
    alloc_t* allocation = task->allocations + slot;
    TRACE_EVENT("F %u %d %d %d\n", id, allocation->region, allocation->start_page, allocation->num_pages);
    sn_release_pages(mem->regions + allocation->region, allocation->start_page, allocation->num_pages);
    TRACE("FREE TASK: %d  SLOT: %d  REG: %d  OFF: %d  LEN: %d  PHY: %016llx\n", id, slot, allocation->region, allocation->start_page, allocation->num_pages, mem->regions[allocation->region].base_addr + allocation->start_page * MEM_PAGE_SIZE);
    ++task->num_free;
    --task->num_allocated;
    rb_erase(&allocation->addr_node, &task->addr_root);
    allocation->used = 0;
    ret = 0;
  }
exit:
  mutex_unlock(&task->lock);
  return ret;
}

static uint64_t translate_handle(memory_t* mem, struct file* filp, handle_t handle, uint32_t size) {
  task_t* task;
  int slot;
  uint64_t ret;
  task = mem->tasks + handle.task_id;
  if (!handle.is_handle || check_lock_task_id(mem, filp, handle.task_id, task)) {
    return INVALID_ADDR.value;
  }
  slot = handle.slot;
  if (slot > task->num_allocated + task->num_free || !task->allocations[slot].used) {
    TRACE("INVALID SLOT: %d\n", slot);
    ret = INVALID_ADDR.value;
    goto exit;
  }
  {
    alloc_t* allocation = task->allocations + slot;
    if ((handle.offset + size) / MEM_PAGE_SIZE > allocation->num_pages) {
      TRACE("INVALID OFFSET: %u + %u\n", handle.offset, size);
      ret = INVALID_ADDR.value;
      goto exit;
    }
    ret = mem->regions[allocation->region].base_addr + allocation->start_page * MEM_PAGE_SIZE + handle.offset;
  }
exit:
  mutex_unlock(&task->lock);
  TRACE("translated: %016llx\n", ret);
  return ret;
}

static int free_task_id(memory_t* mem, struct file* filp, int id) {
  task_t* task = mem->tasks + id;
  if (check_task_id(mem, filp, id) || !task->is_live) {
    return -EINVAL;
  }
  // TODO(jkuhn): Handle intentional race (malicious) ?
  mutex_lock(&mem->taskid_lock);
  {
    alloc_t* alloc = task->allocations;
    for (; task->num_allocated; ++alloc) {
      if (!alloc->used) {
        continue;
      }
      TRACE("REAP SLOT %ld\n", alloc - task->allocations);
      sn_release_pages(mem->regions + alloc->region, alloc->start_page, alloc->num_pages);
      --task->num_allocated;
    }
    task->allocations = krealloc_array(task->allocations, 0, sizeof(alloc_t), GFP_KERNEL); // free
  }
  task->is_live = 0;
  clear_bit(id, mem->task_bits);
  --mem->num_tasks;
  mutex_unlock(&mem->taskid_lock);
  TRACE_EVENT("T- %u\n", id);
  return 0;
}

static ssize_t device_mem_info_show(struct device* device, struct device_attribute* attr, char *buf) {
	struct sn_tranx_t* dev = ((struct sn_misc_tdev*) dev_get_drvdata(device))->tdev;
  memory_t* mem = dev->modules[SN_MODULE_MEMORY_OSAL];
  region_t* region = mem->regions;
  char* start = buf;
  int i;

  for (i = 0; i < NUM_REGIONS; ++i, ++region) {
    int used = region->num_bits - atomic_read(&region->num_free);
    if (!region->bits) {
      continue;
    }
    buf += sprintf(buf, "%d\t%016llx\t%u\t%u\t%u%%\n", i, region->base_addr, used, region->num_bits, used * 100 / region->num_bits);
  }
  return buf - start;
}

static DEVICE_ATTR_RO(device_mem_info);

static struct attribute* sysfs_entries[] = { &dev_attr_device_mem_info.attr, NULL };
static struct attribute_group attribute_group = {
	.name = NULL,
	.attrs = sysfs_entries
};

int sn_mem_osal_init(struct sn_tranx_t* dev) {
  memory_t* mem = (memory_t*) kzalloc(sizeof(*mem), GFP_KERNEL);
  if (!mem) {
    sn_pri(dev, SN_ERR, "sn_perf_init: kmalloc failed\n");
    return -ENOMEM;
  }
  dev->modules[SN_MODULE_MEMORY_OSAL] = mem;
  mem->dev = dev;

  mutex_init(&mem->taskid_lock);
  bitmap_zero(mem->task_bits, MAX_NUM_TASKS);

  DO_REGION_INIT(slice0);
  mem->regions[0].base_addr = REGION_SLICE0_START;
  mem->regions[0].bits = mem->slice0_bits;
  mem->regions[0].num_bits = slice0_NUM_PAGES;
  atomic_set(&mem->regions[0].num_free, slice0_NUM_PAGES);
  mutex_init(&mem->regions[0].lock);

  DO_REGION_INIT(slice1);
  mem->regions[1].base_addr = REGION_SLICE1_START;
  mem->regions[1].bits = mem->slice1_bits;
  mem->regions[1].num_bits = slice1_NUM_PAGES;
  atomic_set(&mem->regions[1].num_free, slice1_NUM_PAGES);
  mutex_init(&mem->regions[1].lock);

  DO_REGION_INIT(mmio);
  mem->regions[2].base_addr = REGION_MMIO_START;
  mem->regions[2].bits = mem->mmio_bits;
  mem->regions[2].num_bits = mmio_NUM_PAGES;
  atomic_set(&mem->regions[2].num_free, mmio_NUM_PAGES);
  mutex_init(&mem->regions[2].lock);

  get_task_id(mem, NULL); // pre-allocate task-id 0 for use by kernel mmio mappings

  {
    int ret = sysfs_create_group(&dev->misc_dev->this_device->kobj, &attribute_group); // ignore return value for now
    (void) ret;
  }

  return 0;
}

void sn_mem_osal_release(struct sn_tranx_t* dev) {
  memory_t* mem = dev->modules[SN_MODULE_MEMORY_OSAL];
  sysfs_remove_group(&dev->misc_dev->this_device->kobj, &attribute_group);
  kfree(mem);
}

long sn_mem_osal_ioctl(struct file* filp, unsigned int cmd, unsigned long argp, struct sn_tranx_t* dev) {
  memory_t* mem = dev->modules[SN_MODULE_MEMORY_OSAL];
  void __user* arg = (void __user*) argp;
  switch (cmd) {
  case SN_TRANX_MEM_GET_TASKID: {
      int id = get_task_id(mem, filp);
      TRACE("SN_TRANX_MEM_GET_TASKID\n");
      if (id < 0) {
        sn_pri(dev, SN_ERR, "get_taskid: no tasks available\n");
        return id;
      }
      __put_user(id, (int*) arg);
      return 0;
    }
    break;
  case SN_TRANX_MEM_FREE_TASKID: {
      int id;
      int ret;
      TRACE("SN_TRANX_MEM_FREE_TASKID\n");
      if (__get_user(id, (int*) arg)) {
        return -EFAULT;
      }
      if ((ret = free_task_id(mem, filp, id)) < 0) {
        sn_pri(dev, SN_ERR, "free_taskid: invalid id\n");
        return ret;
      }
      __put_user(id, (int*) arg);
      return 0;
    }
    break;
  case SN_TRANX_MEM_ALLOC_HANDLE: {
      struct mem_info info;
      TRACE("SN_TRANX_MEM_ALLOC_HANDLE\n");
      if (copy_from_user(&info, arg, sizeof(info))) {
        return -EFAULT;
      }
      info.phy_addr = alloc_mem(mem, filp, info.task_id, info.size, info.fd).value;
      if (info.phy_addr == INVALID_ADDR.value) {
        return -EINVAL;
      }
      if (copy_to_user(arg, &info, sizeof(info))) {
        // can this actually fail? (same size/ptrs)
        return -EFAULT; // leak for now
      }
      return 0;
    }
    break;
	case SN_TRANX_MEM_FREE: {
      struct mem_info info;
      TRACE("SN_TRANX_MEM_FREE\n");
      if (copy_from_user(&info, arg, sizeof(info))) {
        return -EFAULT;
      }
      {
        handle_t handle = { .value = info.phy_addr };
        return free_mem(mem, filp, info.task_id, handle);
      }
    }
    break;
  default:
    return -EINVAL;
  }
}

void sn_mem_osal_close(struct sn_tranx_t* dev, struct file* filp) {
  memory_t* mem = dev->modules[SN_MODULE_MEMORY_OSAL];
  int pos = 1; // 0 is reserved and shouldn't be released
  TRACE("CLOSE\n");
  while (1) {
    pos = find_next_bit(mem->task_bits, MAX_NUM_TASKS, pos);
    if (pos == MAX_NUM_TASKS) {
      break;
    }
    TRACE("Check orphan task id: %d\n", pos);
    free_task_id(mem, filp, pos);
    ++pos;
  }
}

uint64_t sn_mem_osal_translate_handle(struct sn_tranx_t* dev, struct file* filp, uint64_t handle, uint32_t size) {
  memory_t* mem = dev->modules[SN_MODULE_MEMORY_OSAL];
  handle_t realHandle = { .value = handle };
  uint64_t addr;
  if (!handle) {
    return handle; // both the encoder and decoder pass null pointers as "empty"
  }
  addr = translate_handle(mem, filp, realHandle, size);
  if (addr == INVALID_ADDR.value) {
    pr_crit("OSAL: bad translation for handle: %016llx\n", handle);
    dump_stack();
  }
  TRACE("translate %016llx to %016llx\n", handle, addr);
  return addr;
}

uint64_t sn_mem_osal_alloc_mem(struct sn_tranx_t* dev, uint32_t size, struct file* filp, int id, int mmio) {
  memory_t* mem = dev->modules[SN_MODULE_MEMORY_OSAL];
  return translate_handle(mem, filp, alloc_mem(mem, filp, id, size, mmio), 0); // want physical address in kernel
}

int sn_mem_osal_free_mem(struct sn_tranx_t* tdev, uint64_t busaddr, struct file* filp) {
  memory_t* mem = tdev->modules[SN_MODULE_MEMORY_OSAL];
  handle_t handle = { .value = busaddr };
  return free_mem(mem, filp, handle.task_id, handle);
}

uint32_t sn_mem_osal_task_from_handle(uint64_t handle) {
  handle_t realHandle = { .value = handle };
  return realHandle.task_id;
}

uint64_t sn_mem_osal_translate_mem(struct sn_tranx_t* dev, uint32_t taskId, uint64_t address) {
  memory_t* mem = dev->modules[SN_MODULE_MEMORY_OSAL];
  return translate_mem(mem, taskId, address).value;
}
