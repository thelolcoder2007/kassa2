// Copyright(C) 2022 - 2024 Advanced Micro Devices, Inc. All rights reserved.
//
// Copyright (C) 2022, Xilinx Inc - All rights reserved
//
// Licensed under the Apache License, Version 2.0 (the "License"). You may
// not use this file except in compliance with the License. A copy of the
// License is located at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.

#pragma once

#define XMA_MAX_PLANES 3
#define MAX_FRAME_W_H 7680
#define XMA_FRAME_W_ALIGN 256
#define XMA_FRAME_H_ALIGN 4
#define MAX_SCALER_OUTPUTS 16
#define MAX_MIO_FILTER_INPUTS 16
#define MAX_MIO_FILTER_OUTPUTS 16
#define XMA_MAX_LOGMSG_SIZE 4096
#define XMA_AV_NOPTS_VALUE ((int64_t) UINT64_C(0x8000000000000000))
#define XMA_DEFAULT_FRAMERATE \
  { 60, 1 }
