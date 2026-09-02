// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Xilinx Inc.
 */

#pragma once

#define SN_PERF_FIFO_DEPTH (4096)

// NOTE: Ordering matters
// IPs up to CUTREE (inclusive) correspond to units with access to a hardware clock
// IPs >= XABR do not have access to a hardware clock

#define SN_PERF_IP_ID_LIST \
	OP(VPP) \
	OP(ME) \
	OP(VCE) \
	OP(AV1) \
	OP(SYSTEMCPU) \
	OP(INLINECPU) \
	OP(LEGOCPU0) \
	OP(LEGOCPU1) \
	OP(CUTREE) \
	OP(ALPHACLOCK) \
	OP(LOOKAHEADCLOCK) \
	OP(XABR) \
	OP(VCD) \

#define OP(X) SN_PERF_ID_##X,

typedef enum {
	SN_PERF_IP_ID_LIST
	SN_PERF_ID_COUNT
} SN_PERF_IP_ID;
#undef OP

typedef enum {
	SN_PERF_TYPE_START,
	SN_PERF_TYPE_STOP,
	SN_PERF_TYPE_INSTANT,
	SN_PERF_TYPE_QUEUED,
	SN_PERF_TYPE_COUNT
} SN_PERF_TYPE;

typedef enum {
	SN_PERF_CMD_START,
	SN_PERF_CMD_STOP,
	SN_PERF_CMD_STOPALL,
	SN_PERF_CMD_SYNC,
} SN_PERF_CMD;

