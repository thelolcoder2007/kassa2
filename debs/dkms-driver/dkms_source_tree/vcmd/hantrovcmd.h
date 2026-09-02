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

#ifndef _VC8000_VCMD_DRIVER_H_
#define _VC8000_VCMD_DRIVER_H_
#ifdef __FREERTOS__
#include "basetype.h"
#include "dev_common_freertos.h"
#elif defined(__linux__)
#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */
#endif

#ifdef __FREERTOS__
//addr_t has been defined in basetype.h //Now the FreeRTOS mem need to support 64bit env
#elif defined(__linux__)
#undef ptr_t
#define ptr_t PTR_T_KERNEL
typedef size_t ptr_t;
#endif

/*priority support*/

#define MAX_CMDBUF_PRIORITY_TYPE 2 //0:normal priority,1:high priority

#define CMDBUF_PRIORITY_NORMAL 0
#define CMDBUF_PRIORITY_HIGH 1

long hantrovcmd_ioctl(struct file* filp, unsigned int cmd, unsigned long arg, struct sn_tranx_t* tdev);
int  hantrovcmd_init(struct sn_tranx_t* tdev);
void hantrovcmd_cleanup(struct sn_tranx_t* tdev);
int  hantrovcmd_open(struct sn_tranx_t* tdev, struct file* filp);
int  hantrovcmd_release(struct sn_tranx_t* tdev, struct file* filp);
int  hantrovcmd_init_ex(struct sn_tranx_t *tdev, int slice_index, u32 sub_module_type, u32 isIM);

#endif /* !_VC8000_VCMD_DRIVER_H_ */
