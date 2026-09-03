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

#ifndef _XLNX_TRANSCODER_H_
#define _XLNX_TRANSCODER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xma.h>

#include "xlnx_decoder.h"
#include "xlnx_encoder.h"
#include "xlnx_scaler.h"
#include "xlnx_transcoder_constants.h"
#include <semaphore.h>

#define WAIT_ON_SEMA -1
#define MAX_BUF_SIZE 1
#define MAX_TRANS_PROC 10

typedef struct XlnxTranscoderProperties {
  XmaDecoderProperties xma_dec_props;
  XmaScalerProperties  xma_scal_props;
  XmaFilterProperties  xma_la_props[TRANSCODE_MAX_ABR_CHANNELS];
  XmaEncoderProperties xma_enc_props[TRANSCODE_MAX_ABR_CHANNELS];
} XlnxTranscoderProperties;

typedef struct {
  sem_t dec_que_empty;
  sem_t dec_que_full;
  sem_t scal_que_empty;
  sem_t scal_que_full;
  sem_t dec_pool_empty;
  sem_t dec_pool_full;
  sem_t dec_que_mutex;
  sem_t scal_que_mutex;
  sem_t dec_pool_mutex;
  sem_t enc_mutex;
  sem_t end_mutex;
  sem_t trans_state_mutex;
  sem_t fps_mutex;
  sem_t dec_enc_sta_mutex;
  sem_t transcode_done;
  sem_t exit_threads_mutex;
} XlnxTranscoderSemaphore;

typedef struct {
  XlnxTranscoderSemaphore trans_sem;
  XmaFrame*               decoder_frames[MAX_BUF_SIZE]; // the buffer
  XmaFrame*               scaler_frames[MAX_BUF_SIZE][SCAL_MAX_ABR_CHANNELS];
  int32_t                 curr_sess_channels[MAX_BUF_SIZE];
  int32_t                 dec_idx_arr[DEC_MAX_OUT_BUFFERS];
  uint32_t                dec_out_index;
  uint32_t                la_in_index;
  size_t                  dec_pool_in_ptr;
  size_t                  la_pool_out_ptr;
  size_t                  dec_que_frame_ptr; // number of items in the buffer
  size_t                  scal_in_que_frame_ptr;
  size_t                  scal_out_que_frame_ptr;
  size_t                  enc_que_frame_ptr;
  bool                    was_q_pressed;
  bool                    enc_exit_fr_thead;
} XlnxTranscoderQueue;

typedef struct XlnxLogHandle {
  XmaLogLevelType log_level;    /* -log */
  XmaLogType      log_location; /* -log-location */
  char*           log_file;
  XmaLogHandle    log;
  XmaHandle       handle;
  int             dev_index;
} XlnxLogHandle;

typedef struct XlnxTranscoderCtx {
  XlnxDecoderCtx      dec_ctx;
  XlnxScalerCtx       scal_ctx;
  XlnxEncoderCtx      enc_ctx[TRANSCODE_MAX_ABR_CHANNELS];
  XlnxAppTimeTracker  app_timer;
  XmaFrame            xma_app_frame[TRANSCODE_MAX_ABR_CHANNELS];
  size_t              num_frames;
  size_t              in_frame_cnt;
  size_t              out_frame_cnt;
  size_t              enc_frame_cnt;
  int32_t             loop_count;
  int32_t             num_enc_channels;
  int32_t             curr_sess_channels;
  int32_t             curr_session_channels;
  int32_t             num_scal_out;
  int32_t             num_scal_fullrate;
  int32_t             non_scal_channels;
  int32_t             enc_chan_idx;
  int32_t             enc_ch_id_non;
  int32_t             eos_count;
  int32_t             flush_mode;
  int32_t             transcoder_state;
  int32_t             dec_enc_state;
  XlnxTranscoderQueue trans_que;
  uint32_t            stream_number;
  int                 no_of_streams;
  XlnxLogHandle       trans_handle;
  XmaLogHandle        log;
  XmaHandle           handle;
  FILE*               fp;
  bool                exit_from_threads;
} XlnxTranscoderCtx;

int32_t xlnx_tran_device_init(XlnxTranscoderCtx* transcode_ctx, XlnxTranscoderProperties* transcode_props, bool load_xclbin);

int32_t xlnx_tran_session_create(XlnxTranscoderCtx* transcode_ctx, XlnxTranscoderProperties* transcode_props);

int32_t xlnx_tran_frame_process(XlnxTranscoderCtx* transcode_ctx);

#endif // _XLNX_TRANSCODER_H_
