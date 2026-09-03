/* SPDX-License-Identifier: LGPL-3.0-or-later OR Apache-2.0 */

/*
 * (c) Copyright 2024 VeriSilicon Holdings Co., Ltd. All rights reserved.
 *
 * This file is dual-licensed; you may select either the GNU
 * Lesser General Public License version 3 or
 * Apache License, Version 2.0.
 *
 */

#ifndef __VPI_INT_H__
#define __VPI_INT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

typedef unsigned char u8;
typedef signed char i8;
typedef unsigned short u16;
typedef signed short i16;
typedef unsigned int u32;
typedef signed int i32;
// On Windows long is 32 bits. Since we've included stdint.h, use it.
typedef uint64_t u64;
typedef int64_t i64;
typedef size_t addr_t;

#ifdef __cplusplus
}
#endif

#endif
