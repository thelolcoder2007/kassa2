//
// Copyright(C) 2022 - 2025 Advanced Micro Devices, Inc. All rights reserved.
//
//

#pragma once

#if defined(KERNEL) // kernel build
#include <stdint.h>
#include <stddef.h> // for size_t
#endif

//* Enumeration of possible accelerators

#define OSAL_ACCEL_LIST \
  OP(decoder) \
  OP(scaler) \
  OP(gpu2d) \
  OP(ml) \
  OP(encoder)

#define OP(X) osal_##X,
typedef enum osal_accelerator { OSAL_ACCEL_LIST OSAL_COUNT } osal_accelerator;
#undef OP

//* Enumeration of possible allocation flags
typedef enum osal_alloc_flag { osal_host, osal_device, osal_device_mmio } osal_alloc_flag;

//* Header for messages between user space and kernel
typedef struct osal_command {
  union {
    struct {
      uint32_t op_code  : 16;
      uint32_t slice_id : 2;
      uint32_t priority : 1;
    };
    uint32_t cmd;
  };
  uint32_t numData;
} osal_command;
