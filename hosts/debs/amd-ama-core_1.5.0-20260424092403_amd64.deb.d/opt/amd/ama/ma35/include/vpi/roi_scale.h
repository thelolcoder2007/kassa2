// SPDX-License-Identifier: LGPL-3.0-or-later OR Apache-2.0 */
//
// (c) Copyright 2024 Advanced Micro Devices Inc. All rights reserved.
//
// This file is dual-licensed; you may select either the GNU
// Lesser General Public License version 3 or
// Apache License, Version 2.0.

#pragma once

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

void scale_roi(uint8_t* in_roi_map, uint8_t* out_roi_map, int inp_pic_width, int inp_pic_height, int out_pic_width, int out_pic_height);

#if defined(__cplusplus)
}
#endif
