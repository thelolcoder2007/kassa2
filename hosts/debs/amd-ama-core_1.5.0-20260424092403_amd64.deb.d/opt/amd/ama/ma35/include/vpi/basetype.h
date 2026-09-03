/* SPDX-License-Identifier: LGPL-3.0-or-later OR Apache-2.0 */

/*
 * (c) Copyright 2024 VeriSilicon Holdings Co., Ltd. All rights reserved.
 *
 * This file is dual-licensed; you may select either the GNU
 * Lesser General Public License version 3 or
 * Apache License, Version 2.0.
 *
 */

#ifndef BASE_TYPE_H
#define BASE_TYPE_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef TRANS_LOG_ENABLE
#include "trans_log.h"
#endif

// stdint.h defines just about all the types we need in a way that we can
// use them without concerns about machine architecture or OS. So use those.
typedef uint8_t u8;
typedef int8_t i8;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;
typedef uintptr_t addr_t;
typedef size_t    ptr_t;
typedef short Short;
typedef int Int;
typedef unsigned int UInt;

#ifndef NULL
#define NULL    0
#endif

#define HANTRO_OK 0
#define HANTRO_NOK 1

#define HANTRO_FALSE (0U)
#define HANTRO_TRUE (1U)

/* Don't define bool, true, and false in C++, except as a GNU extension. */
#ifndef __cplusplus
#define bool _Bool
#define true 1
#define false 0
#elif defined(__GNUC__) && !defined(__STRICT_ANSI__)
/* Define _Bool as a GNU extension. */
#define _Bool bool
#if __cplusplus < 201103L
/* For C++98, define bool, false, true as a GNU extension. */
#define bool bool
#define false false
#define true true
#endif
#endif

/* constant definitions */
#ifndef OK
#define OK 0
#endif

#ifndef NOK
#define NOK 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef NULL
#define NULL 0
#endif

#if 0
#undef assert
#undef ASSERT

#ifdef DEBUG
#define assert(expr)                                                   \
    if (!(expr)) {                                                     \
        SDKLOGC("assert failed, file: %s line: %d :: %s.\n", __FILE__, \
                __LINE__, #expr);                                      \
        abort();                                                       \
    }

#define ASSERT(expr)                                                   \
    if (!(expr)) {                                                     \
        SDKLOGC("assert failed, file: %s line: %d :: %s.\n", __FILE__, \
                __LINE__, #expr);                                      \
        abort();                                                       \
    }
#else
#define assert(expr)
#define ASSERT(expr)
#endif
#endif

/* ASSERT */
#ifndef NDEBUG
#define ASSERT(x) assert(x)
#else
#define ASSERT(x)
#endif

#define UNUSED(x) (void)(x)

#endif
