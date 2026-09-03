/* SPDX-License-Identifier: LGPL-3.0-or-later OR Apache-2.0 */

/*
 * (c) Copyright 2024 VeriSilicon Holdings Co., Ltd. All rights reserved.
 *
 * This file is dual-licensed; you may select either the GNU
 * Lesser General Public License version 3 or
 * Apache License, Version 2.0.
 *
 */

#ifndef __PCIE_EDMA_H__
#define __PCIE_EDMA_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef void * EDMA_HANDLE;

EDMA_HANDLE TRANS_EDMA_init(char * device);
void TRANS_EDMA_release(EDMA_HANDLE ehd);

int TRANS_EDMA_RC2EP_nonlink(EDMA_HANDLE ehd, u64 src_base, u64 dst_base, u32 size, i32 fd);
int TRANS_EDMA_EP2RC_nonlink(EDMA_HANDLE ehd, u64 src_base, u64 dst_base, u32 size, i32 fd);

#ifdef __cplusplus
}
#endif

#endif


