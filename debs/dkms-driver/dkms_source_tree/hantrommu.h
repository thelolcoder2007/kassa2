/*
 * Hantro Decoder device driver (kernel module)
*
* Copyright (C) 2020  VeriSilicon Microelectronics Co., Ltd.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
------------------------------------------------------------------------------*/

#ifndef _HANTROMMU_H_
#define _HANTROMMU_H_
#ifdef __FREERTOS__
#include "basetype.h"
#include "dev_common_freertos.h"
#elif defined(__linux__)
#include <linux/fs.h>
#include "common.h"
#endif

#define REGION_IN_START 0x0
#define REGION_IN_END 0x40000000
#define REGION_OUT_START 0x40000000
#define REGION_OUT_END 0x80000000
#define REGION_PRIVATE_START 0x80000000
#define REGION_PRIVATE_END 0xc0000000

#define REGION_IN_MMU_START 0x1000
#define REGION_IN_MMU_END 0x40002000
#define REGION_OUT_MMU_START 0x40002000
#define REGION_OUT_MMU_END 0x80001000
#define REGION_PRIVATE_MMU_START 0x80001000
#define REGION_PRIVATE_MMU_END 0xc0000000

enum MMUStatus {
	MMU_STATUS_OK = 0,

	MMU_STATUS_FALSE = -1,
	MMU_STATUS_INVALID_ARGUMENT = -2,
	MMU_STATUS_INVALID_OBJECT = -3,
	MMU_STATUS_OUT_OF_MEMORY = -4,
	MMU_STATUS_NOT_FOUND = -19,
};

struct addr_desc {
	void *virtual_address; /* buffer virtual address */
	size_t bus_address; /* buffer physical address */
	unsigned int size; /* physical size */
};

struct kernel_addr_desc {
	size_t bus_address; /* buffer virtual address */
	size_t mmu_bus_address; /* buffer physical address in MMU*/
	unsigned int size; /* physical size */
};

#define HANTRO_IOC_MMU 'm'

#define HANTRO_IOCS_MMU_MEM_MAP _IOWR(HANTRO_IOC_MMU, 1, struct addr_desc *)
#define HANTRO_IOCS_MMU_MEM_UNMAP _IOWR(HANTRO_IOC_MMU, 2, struct addr_desc *)
#define HANTRO_IOCS_MMU_FLUSH _IOWR(HANTRO_IOC_MMU, 3, unsigned int *)
#define HANTRO_IOC_MMU_MAXNR 3

/******************************************************************************/
/* MMU */
/******************************************************************************/

/* Init MMU, should be called in driver init function. */
enum MMUStatus MMUInit(volatile unsigned char *hwregs);
/* Clean up all data in MMU, should be called in driver cleanup function
   when rmmod driver*/
enum MMUStatus MMUCleanup(volatile unsigned char *hwregs[MAX_SUBSYS_NUM][2]);
/* The function should be called in driver realease function
   when driver exit unnormally */
enum MMUStatus MMURelease(void *filp, volatile unsigned char *hwregs);

enum MMUStatus MMUEnable(volatile unsigned char *hwregs[MAX_SUBSYS_NUM][2]);

/* Used in kernel to map buffer */
enum MMUStatus MMUKernelMemNodeMap(struct kernel_addr_desc *addr);
/* Used in kernel to unmap buffer */
enum MMUStatus MMUKernelMemNodeUnmap(struct kernel_addr_desc *addr);

long MMUIoctl(unsigned int cmd, void *filp, unsigned long arg,
	      volatile unsigned char *hwregs[MAX_SUBSYS_NUM][2]);

#endif
