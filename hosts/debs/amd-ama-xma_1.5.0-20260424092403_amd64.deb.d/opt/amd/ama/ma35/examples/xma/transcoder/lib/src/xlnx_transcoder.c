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

#include "xlnx_transcoder.h"
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include <sys/syscall.h>

int signal_caught;

/**
 * xlnx_tran_signal_handler: Signal handler function
 *
 * @param signum: Signal number
 */
void xlnx_tran_signal_handler(int32_t signum) {
  switch (signum) {
  case SIGTERM:
  case SIGINT:
  case SIGABRT:
  case SIGHUP:
  case SIGQUIT:
    signal_caught = TRANSCODE_APP_STOP;
    break;
  }
}

/**
 * xlnx_tran_set_signal_handler: Signal handler initialization.
 *
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
static int32_t xlnx_tran_set_signal_handler() {
  signal_caught = 0;
  struct sigaction action;
  action.sa_handler = xlnx_tran_signal_handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGHUP, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGQUIT, &action, NULL);
  return XMA_APP_SUCCESS;
}

/**
 * xlnx_tran_update_num_channels: Update num channels for the current session.
 *
 * @param transcode_ctx: Transcoder context
 */
static void xlnx_tran_update_num_channels(XlnxTranscoderCtx* transcode_ctx) {

  if (transcode_ctx->curr_sess_channels != transcode_ctx->num_enc_channels) {
    transcode_ctx->curr_sess_channels = transcode_ctx->num_enc_channels;
  } else {
    transcode_ctx->curr_sess_channels = transcode_ctx->num_scal_fullrate + (transcode_ctx->num_enc_channels - transcode_ctx->num_scal_out);
  }

  return;
}

/**
 * xlnx_tran_get_enc_input: Gets enc input frame pointer
 * @param transcode_ctx The transcode ctx
 * @return The enc input frame pointer
 */
static XmaFrame* xlnx_tran_get_enc_input(XlnxTranscoderCtx* transcode_ctx, int enc_chan_idx) {
  transcode_ctx->enc_chan_idx        = enc_chan_idx;
  int                  scal_out_idx  = enc_chan_idx;
  XmaFrame*            enc_in_frame  = NULL;
  XlnxTranscoderQueue* transcode_que = &transcode_ctx->trans_que;
  XlnxEncoderCtx*      enc_ctx       = &transcode_ctx->enc_ctx[transcode_ctx->enc_chan_idx];

  if (transcode_ctx->non_scal_channels) {
    scal_out_idx = enc_chan_idx - 1;
  }
  if ((transcode_ctx->enc_chan_idx == 0) && transcode_ctx->non_scal_channels) {
    enc_ctx->enc_in_frame = transcode_que->decoder_frames[transcode_que->enc_que_frame_ptr];
    enc_in_frame          = enc_ctx->enc_in_frame;
  } else {
    enc_ctx->enc_in_frame = transcode_que->scaler_frames[transcode_que->enc_que_frame_ptr][scal_out_idx];
    enc_in_frame          = enc_ctx->enc_in_frame;
  }

  return enc_in_frame;
}

/**
 * xlnx_tran_session_create: Creates transcoder session
 *
 * @param transcode_ctx: Transcoder context
 * @param transcode_props: Transcoder properties
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
int32_t xlnx_tran_session_create(XlnxTranscoderCtx* transcode_ctx, XlnxTranscoderProperties* transcode_props) {
  int32_t ret                           = XMA_APP_ERROR;
  transcode_props->xma_dec_props.handle = transcode_ctx->trans_handle.handle;
  transcode_ctx->dec_ctx.log            = transcode_ctx->trans_handle.log;
  ret                                   = xlnx_dec_session(&transcode_ctx->dec_ctx, &transcode_props->xma_dec_props);
  if (ret != XMA_APP_SUCCESS) {
    xma_logmsg(transcode_ctx->trans_handle.log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Error in decoder session create \n");
    return XMA_APP_ERROR;
  }
  xma_logmsg(transcode_ctx->dec_ctx.log, XMA_INFO_LOG, XLNX_TRANSCODER_APP_MODULE, "Decoder session creation successful \n");
  transcode_ctx->scal_ctx.log = transcode_ctx->trans_handle.log;
  if (transcode_ctx->num_scal_out) {
    transcode_props->xma_scal_props.handle = transcode_ctx->trans_handle.handle;
    ret                                    = xlnx_scal_session(&transcode_ctx->scal_ctx, &transcode_props->xma_scal_props, transcode_ctx->handle);

    if (ret != XMA_APP_SUCCESS) {
      xma_logmsg(transcode_ctx->scal_ctx.log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Error in scaler session create \n");
      return XMA_APP_ERROR;
    }
    xma_logmsg(transcode_ctx->scal_ctx.log, XMA_INFO_LOG, XLNX_TRANSCODER_APP_MODULE, "Scaler session creation successful \n");
  }
  for (int32_t i = 0; i < transcode_ctx->num_enc_channels; i++) {
    transcode_props->xma_enc_props[i].handle = transcode_ctx->trans_handle.handle;
    transcode_ctx->enc_ctx[i].log            = transcode_ctx->trans_handle.log;
    ret                                      = xlnx_enc_session(&transcode_ctx->enc_ctx[i], &transcode_props->xma_enc_props[i]);
    if (ret != XMA_APP_SUCCESS) {
      xma_logmsg(transcode_ctx->trans_handle.log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Error in encoder session create \n");
      return XMA_APP_ERROR;
    }
  }
  xma_logmsg(transcode_ctx->trans_handle.log, XMA_INFO_LOG, XLNX_TRANSCODER_APP_MODULE, "Encoder session creation successful \n");

  return ret;
}

/**
 * xlnx_tran_print_total_fps: Print the total performance of the transcoder.
 *
 * @param transcode_ctx: Transcoder context
 * @return None
 */
static void xlnx_tran_print_total_fps(XlnxTranscoderCtx* transcode_ctx) {
  double time_taken         = xlnx_utils_get_total_time(&transcode_ctx->app_timer);
  size_t num_frames_encoded = min(transcode_ctx->out_frame_cnt, transcode_ctx->num_frames);
  fprintf(stderr, "\nFrames Transcoded: %zu, Time Elapsed: %.03lf\r\n", num_frames_encoded, time_taken);
  fprintf(stderr, "Total FPS: %.03lf\r\n", num_frames_encoded / time_taken);
}

/**
 * xlnx_tran_print_segment_fps: Calculate and print fps for every second
 *
 * @param transcode_ctx: Transcoder context
 * @return None
 */
static void xlnx_tran_print_segment_fps(XlnxTranscoderCtx* transcode_ctx) {
  double segment_time = xlnx_utils_get_segment_time(&transcode_ctx->app_timer);
  if (segment_time < 1) {
    return;
  }
  sem_wait(&transcode_ctx->trans_que.trans_sem.fps_mutex);
  fprintf(stderr, "\r Fps thread id = %lu Stream Number = %d  Frame =%5zu Average FPS = %.03f Current FPS = %.03f\r", syscall(SYS_gettid), transcode_ctx->stream_number,
      transcode_ctx->out_frame_cnt, (float) transcode_ctx->out_frame_cnt / xlnx_utils_get_total_time(&transcode_ctx->app_timer),
      (transcode_ctx->out_frame_cnt - transcode_ctx->app_timer.last_displayed_frame) / segment_time);
  fprintf(stderr, "\n");
  fflush(stderr);
  transcode_ctx->app_timer.last_displayed_frame = transcode_ctx->out_frame_cnt;
  sem_post(&transcode_ctx->trans_que.trans_sem.fps_mutex);
  xlnx_utils_set_segment_time(&transcode_ctx->app_timer);

  return;
}

/**
 * xlnx_tran_wait_on_sema: Wait on Semaphore
 *
 * @param transcode_sem: Semaphore
 * @return status
 */
static int32_t xlnx_tran_wait_on_sema(sem_t* transcode_sem) {
  struct timespec sem_timeout;
  int32_t         sem_status;
  clock_gettime(CLOCK_REALTIME, &sem_timeout);
  sem_timeout.tv_sec += 1;
  sem_status = sem_timedwait(transcode_sem, &sem_timeout);
  if (sem_status == WAIT_ON_SEMA) {
    if (errno == EINTR || errno == ETIMEDOUT || errno == EINVAL) {
      return WAIT_ON_SEMA;
    }
  }
  return 0;
}
/**
 * decoder_thread: Decode's encoded data
 *
 * @param transcode_data: Transcoder context
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
void* decoder_thread(void* transcode_data) {
  XlnxTranscoderCtx*       transcode_ctx  = transcode_data;
  XlnxDecoderCtx*          dec_ctx        = &transcode_ctx->dec_ctx;
  XlnxTranscoderQueue*     transcode_que  = &transcode_ctx->trans_que;
  XlnxTranscoderSemaphore* transcode_sem  = &transcode_ctx->trans_que.trans_sem;
  int32_t                  ret            = XMA_APP_ERROR;
  int32_t                  ret_read_frame = XMA_APP_ERROR;
  int                      dec_frame_ptr  = 0;
  int                      do_not_read    = 0;
  bool                     first_read     = true;
  dec_ctx->is_use_push_thread             = transcode_ctx->dec_ctx.dec_props.stream_model;

  do {
    if ((xlnx_utils_was_q_pressed()) == TRANSCODE_APP_STOP) {
      transcode_que->was_q_pressed = true;
    }
    do {
      if (!do_not_read) {
        if ((ret_read_frame = xlnx_dec_read_frame(dec_ctx)) == XMA_APP_ERROR) {
          xma_logmsg(transcode_ctx->trans_handle.log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Error in reading input frame \n");
          sem_wait(&transcode_sem->exit_threads_mutex);
          transcode_ctx->exit_from_threads = true;
          sem_post(&transcode_sem->exit_threads_mutex);
          pthread_exit(0);
        }
      }
      if (first_read) {
        first_read = false;
        xlnx_utils_start_tracking_time(&transcode_ctx->app_timer);
        sem_post(&transcode_sem->fps_mutex);
      }
      if (ret_read_frame == DEC_INPUT_EOF) {
        if (transcode_ctx->loop_count-- > 0) {
          lseek(transcode_ctx->dec_ctx.in_file, 0, SEEK_SET);
        }
      }
      if (((ret_read_frame == DEC_INPUT_EOF) && (!xlnx_dec_get_input_size(&transcode_ctx->dec_ctx)) && transcode_ctx->loop_count <= -1) ||
          dec_ctx->dec_out_fr_cnt >= transcode_ctx->num_frames || transcode_que->was_q_pressed || signal_caught || transcode_ctx->exit_from_threads) {
        do_not_read = 1;
        if ((ret = xlnx_dec_send_null_frame(dec_ctx)) <= XMA_APP_ERROR) {
          xma_logmsg(transcode_ctx->trans_handle.log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Error sending null frame to the decoder\n");
          sem_wait(&transcode_sem->exit_threads_mutex);
          transcode_ctx->exit_from_threads = true;
          sem_post(&transcode_sem->exit_threads_mutex);
          pthread_exit(0);
        }
      } else {
        if ((ret = xlnx_dec_send_frame(dec_ctx)) <= XMA_APP_ERROR) {
          xma_logmsg(transcode_ctx->trans_handle.log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Error in sending frame to decoder \n");
          sem_wait(&transcode_sem->exit_threads_mutex);
          transcode_ctx->exit_from_threads = true;
          sem_post(&transcode_sem->exit_threads_mutex);
          pthread_exit(0);
        } else {
          /* Input frame has been incremented only if the decoder
                       consumes the frame that is read. Read frame doesn't
                       necessarily read one frame at a time */
          if (ret == XMA_APP_SUCCESS || ret == XMA_SEND_MORE_DATA) {
            transcode_ctx->in_frame_cnt++;
            xma_logmsg(transcode_ctx->trans_handle.log, XMA_DEBUG_LOG, XLNX_TRANSCODER_APP_MODULE, "\n Dec_th_id = %lu Dec_ip_fr_cnt = %ld", syscall(SYS_gettid),
                transcode_ctx->in_frame_cnt);
          } else if (ret == XMA_TRY_AGAIN) {
            ret = XMA_SUCCESS;
          }
        }
      }
      if ((ret == XMA_SUCCESS) || (ret == XMA_FLUSH_AGAIN)) {
        dec_ctx->out_frame = xma_frame_alloc(transcode_ctx->handle, &dec_ctx->device_frame_props, true);
        if ((ret = xlnx_dec_recv_frame(dec_ctx)) <= XMA_APP_ERROR) {
          xma_logmsg(transcode_ctx->trans_handle.log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Error in getting decoder output \n");
          sem_wait(&transcode_sem->exit_threads_mutex);
          transcode_ctx->exit_from_threads = true;
          sem_post(&transcode_sem->exit_threads_mutex);
          pthread_exit(0);
        }
        if (ret != XMA_SUCCESS) {
          xma_frame_free(dec_ctx->out_frame);
          dec_ctx->out_frame = NULL;
        }
      }
    } while ((ret == XMA_RESEND_AND_RECV) || (ret == XMA_SEND_MORE_DATA));

    if (ret == XMA_EOS) {
      xma_frame_free(dec_ctx->out_frame);
      dec_ctx->out_frame = NULL;
      if (dec_ctx->dec_out_fr_cnt < transcode_ctx->num_frames || transcode_ctx->exit_from_threads) {
        sem_wait(&transcode_sem->trans_state_mutex);
        transcode_ctx->transcoder_state = TRANSCODE_SCAL_FLUSH;
        sem_post(&transcode_sem->trans_state_mutex);
        sem_wait(&transcode_sem->dec_enc_sta_mutex);
        transcode_ctx->dec_enc_state = TRANSCODE_ENC_FLUSH;
        sem_post(&transcode_sem->dec_enc_sta_mutex);
      }
      break;
    }
    if (dec_ctx->dec_out_fr_cnt < transcode_ctx->num_frames && !transcode_ctx->exit_from_threads) {
      if (ret == XMA_APP_SUCCESS) {
        if (xlnx_tran_wait_on_sema(&transcode_sem->dec_que_empty) == WAIT_ON_SEMA) {
          continue;
        }
        sem_wait(&transcode_sem->dec_que_mutex);
        dec_frame_ptr = transcode_que->dec_que_frame_ptr;
        /* dec_ctx->out_frame used by decoder for next frame and
                transcode_que->decoder_frames[dec_frame_ptr] will be used by scaler */
        transcode_que->decoder_frames[dec_frame_ptr] = dec_ctx->out_frame;
        dec_frame_ptr                                = (dec_frame_ptr + 1) % MAX_BUF_SIZE;
        transcode_que->dec_que_frame_ptr             = dec_frame_ptr;
        dec_ctx->dec_out_fr_cnt++;
        xma_logmsg(transcode_ctx->trans_handle.log, XMA_DEBUG_LOG, XLNX_TRANSCODER_APP_MODULE, "\n Dec_th_id = %lu Dec_out_fr_cnt = %d", syscall(SYS_gettid),
            dec_ctx->dec_out_fr_cnt);
        sem_post(&transcode_sem->dec_que_mutex);
        sem_post(&transcode_sem->dec_que_full);
      }
    } else {
      xma_frame_free(dec_ctx->out_frame);
      dec_ctx->out_frame = NULL;
      if (transcode_ctx->transcoder_state != TRANSCODE_DONE && !transcode_ctx->exit_from_threads) {
        if (xlnx_tran_wait_on_sema(&transcode_sem->transcode_done) == WAIT_ON_SEMA) {
          continue;
        }
      }
    }
  } while (1);
  return NULL;
}

/**
 * scaler_thread: Scale to different resolution of frames
 *
 * @param transcode_data: Transcoder context
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
void* scaler_thread(void* transcode_data) {
  XlnxTranscoderCtx*       transcode_ctx      = transcode_data;
  XlnxScalerCtx*           scal_ctx           = &transcode_ctx->scal_ctx;
  XlnxTranscoderQueue*     transcode_que      = &transcode_ctx->trans_que;
  XlnxTranscoderSemaphore* transcode_sem      = &transcode_ctx->trans_que.trans_sem;
  int32_t                  ret                = XMA_APP_ERROR;
  int                      scal_in_frame_ptr  = 0;
  int                      scal_out_frame_ptr = 0;
  if (transcode_ctx->non_scal_channels) {
    return NULL;
  }
  do {
    scal_ctx->in_frame = NULL;
    sem_wait(&transcode_sem->exit_threads_mutex);
    scal_ctx->exit_from_threads = transcode_ctx->exit_from_threads;
    sem_post(&transcode_sem->exit_threads_mutex);

    if (transcode_ctx->transcoder_state != TRANSCODE_SCAL_FLUSH && scal_ctx->scal_input_cnt < transcode_ctx->num_frames && !scal_ctx->exit_from_threads) {
      if (xlnx_tran_wait_on_sema(&transcode_sem->dec_que_full) == WAIT_ON_SEMA) {
        continue;
      }
      sem_wait(&transcode_sem->dec_que_mutex);
      scal_in_frame_ptr = transcode_que->scal_in_que_frame_ptr;
      scal_ctx->scal_input_cnt++;
      xma_logmsg(transcode_ctx->trans_handle.log, XMA_DEBUG_LOG, XLNX_TRANSCODER_APP_MODULE, "\n Scl_th_id = %lu Scal_ip_fr_cnt = %d", syscall(SYS_gettid),
          scal_ctx->scal_input_cnt);
      scal_ctx->in_frame                   = transcode_que->decoder_frames[scal_in_frame_ptr];
      scal_in_frame_ptr                    = (scal_in_frame_ptr + 1) % MAX_BUF_SIZE;
      transcode_que->scal_in_que_frame_ptr = scal_in_frame_ptr;
      sem_post(&transcode_sem->dec_que_mutex);
      sem_post(&transcode_sem->dec_que_empty);
    } else {
      scal_in_frame_ptr = transcode_que->scal_in_que_frame_ptr;
      scal_ctx->scal_input_cnt++;
      if (!scal_ctx->exit_from_threads && scal_ctx->scal_input_cnt <= transcode_ctx->dec_ctx.dec_out_fr_cnt) {
        scal_ctx->in_frame                               = transcode_que->decoder_frames[scal_in_frame_ptr];
        transcode_que->decoder_frames[scal_in_frame_ptr] = NULL;
        scal_in_frame_ptr                                = (scal_in_frame_ptr + 1) % MAX_BUF_SIZE;
        transcode_que->scal_in_que_frame_ptr             = scal_in_frame_ptr;
      }
      if (!scal_ctx->in_frame) {
        scal_ctx->in_frame = NULL;
      } else {
        xma_logmsg(transcode_ctx->trans_handle.log, XMA_DEBUG_LOG, XLNX_TRANSCODER_APP_MODULE, "\n Scl_th_id = %lu Scl_ip_fr_cnt = %d", syscall(SYS_gettid),
            scal_ctx->scal_input_cnt);
      }
    }
    if ((ret = xlnx_scal_process_frame(scal_ctx, transcode_ctx->handle)) <= XMA_APP_ERROR) {
      xma_logmsg(transcode_ctx->trans_handle.log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Failed in scaler frame process \n");
      sem_wait(&transcode_sem->exit_threads_mutex);
      transcode_ctx->exit_from_threads = true;
      sem_post(&transcode_sem->exit_threads_mutex);
      pthread_exit(0);
    } else if ((ret == XMA_SEND_MORE_DATA)) {
      continue;
    }
    if (scal_ctx->scal_input_cnt <= transcode_ctx->num_frames && !scal_ctx->exit_from_threads) {
      if (ret == XMA_APP_SUCCESS && transcode_ctx->scal_ctx.scal_frame_cnt < transcode_ctx->dec_ctx.dec_out_fr_cnt) {
        if (xlnx_tran_wait_on_sema(&transcode_sem->scal_que_empty) == WAIT_ON_SEMA) {
          continue;
        }
        sem_wait(&transcode_sem->scal_que_mutex);
        transcode_ctx->scal_ctx.scal_frame_cnt++;
        xma_logmsg(transcode_ctx->trans_handle.log, XMA_DEBUG_LOG, XLNX_TRANSCODER_APP_MODULE, "\n Scl_th_id = %lu Scl_out_fr_cnt = %ld", syscall(SYS_gettid),
            transcode_ctx->scal_ctx.scal_frame_cnt);
        xlnx_tran_update_num_channels(transcode_ctx);
        scal_out_frame_ptr = transcode_que->scal_out_que_frame_ptr;
        for (int out_fr_no = 0; out_fr_no < scal_ctx->scal_props.nb_outputs; out_fr_no++) {
          transcode_que->scaler_frames[scal_out_frame_ptr][out_fr_no] = scal_ctx->out_frame[out_fr_no];
        }
        scal_out_frame_ptr                    = (scal_out_frame_ptr + 1) % MAX_BUF_SIZE;
        transcode_que->scal_out_que_frame_ptr = scal_out_frame_ptr;
        memmove(&transcode_que->curr_sess_channels[scal_out_frame_ptr], &transcode_ctx->curr_sess_channels, sizeof(int32_t));
        sem_post(&transcode_sem->scal_que_mutex);
        sem_post(&transcode_sem->scal_que_full);
      }
    }
    if (ret == XMA_EOS) {
      if (scal_ctx->scal_input_cnt < transcode_ctx->num_frames || scal_ctx->exit_from_threads) {
        sem_wait(&transcode_sem->trans_state_mutex);
        transcode_ctx->transcoder_state = TRANSCODE_ENC_FLUSH;
        sem_post(&transcode_sem->trans_state_mutex);
      }
      transcode_ctx->curr_sess_channels = transcode_ctx->num_enc_channels;
      break;
    }
  } while (1);

  return NULL;
}

/**
 * encoder_thread: Encode frames into coded bitstream
 *
 * @param transcode_data: Transcoder context
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
void* encoder_thread(void* transcode_data) {
  XlnxTranscoderCtx*       transcode_ctx    = transcode_data;
  XlnxEncoderCtx*          enc_ctx          = &transcode_ctx->enc_ctx[0];
  XlnxTranscoderSemaphore* transcode_sem    = &transcode_ctx->trans_que.trans_sem;
  XlnxTranscoderQueue*     transcode_que    = &transcode_ctx->trans_que;
  int32_t                  enc_out_size     = 0;
  int32_t                  ret              = XMA_APP_ERROR;
  int                      transcoder_state = 0;
  int                      enc_channel_no   = 0;
  int                      enc_frame_ptr    = 0;
  int                      dec_enc_state    = 0;
  uint32_t                 dec_read_finish  = 0;
  uint32_t                 scl_read_finish  = 0;
  int                      enc_chan_index   = 0;
  transcode_que->enc_que_frame_ptr          = 0;
  int32_t curr_sess_channels                = 1;
  int     try_again                         = 0;
  do {
read_la_frame:
    sem_wait(&transcode_sem->trans_state_mutex);
    transcoder_state = transcode_ctx->transcoder_state;
    sem_post(&transcode_sem->trans_state_mutex);

    if (transcoder_state == TRANSCODE_ENC_FLUSH) {
      sem_wait(&transcode_sem->scal_que_mutex);
      if (transcode_ctx->enc_frame_cnt == transcode_ctx->scal_ctx.scal_frame_cnt) {
        scl_read_finish = 1;
      }
      sem_post(&transcode_sem->scal_que_mutex);
    }
    sem_wait(&transcode_sem->dec_enc_sta_mutex);
    dec_enc_state = transcode_ctx->dec_enc_state;
    sem_post(&transcode_sem->dec_enc_sta_mutex);

    if (dec_enc_state == TRANSCODE_ENC_FLUSH) {
      if (transcode_ctx->enc_frame_cnt == transcode_ctx->dec_ctx.dec_out_fr_cnt) {
        dec_read_finish = 1;
      }
    }

    sem_wait(&transcode_sem->exit_threads_mutex);
    enc_ctx->exit_from_threads = transcode_ctx->exit_from_threads;
    sem_post(&transcode_sem->exit_threads_mutex);

    if (transcode_ctx->enc_frame_cnt >= transcode_ctx->num_frames || enc_ctx->exit_from_threads) {
      if (transcode_ctx->non_scal_channels) {
        dec_read_finish = 1;
      } else {
        scl_read_finish = 1;
      }
    }

    enc_channel_no = 0;
    if (transcode_ctx->non_scal_channels && !dec_read_finish) {
      if (xlnx_tran_wait_on_sema(&transcode_sem->dec_que_full) == WAIT_ON_SEMA) {
        if (transcode_ctx->exit_from_threads == true) {
          pthread_exit(0);
        }
        continue;
      }
      sem_wait(&transcode_sem->dec_que_mutex);
      enc_ctx[enc_chan_index].enc_in_frame = xlnx_tran_get_enc_input(transcode_ctx, enc_channel_no);
      transcode_ctx->enc_frame_cnt++;
      sem_post(&transcode_sem->dec_que_mutex);
      sem_post(&transcode_sem->dec_que_empty);
      enc_channel_no = enc_channel_no + 1;
    } else if (transcode_ctx->non_scal_channels && dec_read_finish) {
      enc_ctx[enc_chan_index].enc_in_frame = NULL;
    }
    if (!scl_read_finish && transcode_ctx->num_scal_out) {
      if (xlnx_tran_wait_on_sema(&transcode_sem->scal_que_full) == WAIT_ON_SEMA) {
        if (transcode_ctx->exit_from_threads == true) {
          pthread_exit(0);
        }
        continue;
      }
      sem_wait(&transcode_sem->scal_que_mutex);
      memmove(&curr_sess_channels, &transcode_que->curr_sess_channels[transcode_que->enc_que_frame_ptr], sizeof(int32_t));
      for (int enc_chan_idx = enc_channel_no; enc_chan_idx < curr_sess_channels; enc_chan_idx++) {
        enc_ctx[enc_chan_idx].enc_in_frame = xlnx_tran_get_enc_input(transcode_ctx, enc_chan_idx);
      }
      transcode_ctx->enc_frame_cnt++;
      xma_logmsg(transcode_ctx->trans_handle.log, XMA_DEBUG_LOG, XLNX_TRANSCODER_APP_MODULE, "\n Enc_th_id = %lu Enc_ip_fr_cnt = %ld", syscall(SYS_gettid),
          transcode_ctx->enc_frame_cnt);
      enc_frame_ptr                    = transcode_que->enc_que_frame_ptr;
      enc_frame_ptr                    = (enc_frame_ptr + 1) % MAX_BUF_SIZE;
      transcode_que->enc_que_frame_ptr = enc_frame_ptr;
      sem_post(&transcode_sem->scal_que_mutex);
      sem_post(&transcode_sem->scal_que_empty);
    } else {
      if (transcoder_state == TRANSCODE_ENC_FLUSH) {
        curr_sess_channels = transcode_ctx->num_enc_channels;
      }
      for (int enc_chan_idx = enc_channel_no; enc_chan_idx < curr_sess_channels; enc_chan_idx++) {
        enc_ctx[enc_chan_idx].enc_in_frame = NULL;
      }
    }
    enc_chan_index = -1;
    do {
      enc_chan_index++;
      if (enc_chan_index > curr_sess_channels) {
        goto read_la_frame;
      }

      if (!enc_ctx[enc_chan_index].flush_frame_sent) {
        do {
          if ((ret = xlnx_enc_process_frame(&enc_ctx[enc_chan_index], &enc_out_size, transcode_ctx->handle)) <= XMA_APP_ERROR) {
            xma_logmsg(transcode_ctx->trans_handle.log, XMA_ERROR_LOG, XLNX_TRANSCODER_APP_MODULE, "Failed in encoder frame process \n");
            transcode_ctx->exit_from_threads = true;
            pthread_exit(0);
          }
          if (((ret == XMA_TRY_AGAIN) || (ret == XMA_SUCCESS)) && enc_out_size && !enc_ctx->exit_from_threads) {
            try_again = (ret == XMA_TRY_AGAIN);
            ret       = write(enc_ctx[enc_chan_index].out_file, enc_ctx[enc_chan_index].xma_out_buffer->data.buffer, enc_out_size);
            sem_wait(&transcode_sem->fps_mutex);
            if (!enc_chan_index) {
              xma_logmsg(transcode_ctx->trans_handle.log, XMA_DEBUG_LOG, XLNX_TRANSCODER_APP_MODULE, "\n Enc_th_id = %lu Enc_out_fr_cnt = %ld", syscall(SYS_gettid),
                  transcode_ctx->out_frame_cnt);
              transcode_ctx->out_frame_cnt++;
            }
            sem_post(&transcode_sem->fps_mutex);
            xma_data_buffer_free(enc_ctx[enc_chan_index].xma_out_buffer);
            enc_ctx[enc_chan_index].xma_out_buffer = NULL;
            sem_post(&transcode_sem->fps_mutex);
            enc_out_size = 0;
            if (try_again) {
              ret = XMA_TRY_AGAIN;
            }
          }
          if ((ret == XMA_EOS)) {
            enc_ctx[enc_chan_index].flush_frame_sent = true;
            xma_data_buffer_free(enc_ctx[enc_chan_index].xma_out_buffer);
            enc_ctx[enc_chan_index].xma_out_buffer = NULL;
            sem_wait(&transcode_sem->end_mutex);
            transcode_ctx->eos_count++;
            if (transcode_ctx->eos_count == transcode_ctx->num_enc_channels) {
              transcode_ctx->transcoder_state = TRANSCODE_DONE;
              sem_post(&transcode_sem->transcode_done);
              sem_post(&transcode_sem->end_mutex);
              break;
            }
            sem_post(&transcode_sem->end_mutex);
          }
        } while (ret == XMA_TRY_AGAIN);
      }
    } while (enc_chan_index < (curr_sess_channels - 1));

    if (transcode_ctx->transcoder_state == TRANSCODE_DONE) {
      break;
    }
  } while (1);

  return NULL;
}

/**
 * fps_thread: Measure fps while processing output
 *
 * @param transcode_data: Transcoder context
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
void* fps_thread(void* transcode_data) {
  do {
    XlnxTranscoderCtx* transcode_ctx = transcode_data;
    if (transcode_ctx->exit_from_threads) {
      break;
    }
    XlnxTranscoderSemaphore* trans_sema = &transcode_ctx->trans_que.trans_sem;
    xlnx_tran_print_segment_fps(transcode_ctx);
    sem_wait(&trans_sema->end_mutex);
    if (transcode_ctx->transcoder_state == TRANSCODE_DONE)
      break;
    sem_post(&trans_sema->end_mutex);
  } while (1);

  return NULL;
}

/**
 * master_thread:  Create thread per processing engine
 *
 * @param transcode_data: Transcoder context
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
void* master_thread(void* transcode_data) {
  XlnxTranscoderCtx*       transcode_ctx = transcode_data;
  pthread_t*               dec_thread    = calloc(transcode_ctx->no_of_streams, sizeof(pthread_t));
  pthread_t*               scal_thread   = calloc(transcode_ctx->no_of_streams, sizeof(pthread_t));
  pthread_t*               enc_thread    = calloc(transcode_ctx->no_of_streams, sizeof(pthread_t));
  pthread_t*               fp_thread     = calloc(transcode_ctx->no_of_streams, sizeof(pthread_t));
  XlnxTranscoderSemaphore* transcode_sem;
  for (int i = 0; i < transcode_ctx->no_of_streams; i++) {
    transcode_sem = &transcode_ctx[i].trans_que.trans_sem;
    sem_init(&transcode_sem->dec_que_empty, 0, MAX_BUF_SIZE);
    sem_init(&transcode_sem->dec_que_full, 0, 0);
    sem_init(&transcode_sem->dec_pool_empty, 0, MAX_BUF_SIZE);
    sem_init(&transcode_sem->dec_pool_full, 0, 0);
    sem_init(&transcode_sem->scal_que_empty, 0, MAX_BUF_SIZE);
    sem_init(&transcode_sem->scal_que_full, 0, 0);
    sem_init(&transcode_sem->dec_que_mutex, 0, 1);
    sem_init(&transcode_sem->dec_pool_mutex, 0, 1);
    sem_init(&transcode_sem->scal_que_mutex, 0, 1);
    sem_init(&transcode_sem->trans_state_mutex, 0, 1);
    sem_init(&transcode_sem->dec_enc_sta_mutex, 0, 1);
    sem_init(&transcode_sem->end_mutex, 0, 1);
    sem_init(&transcode_sem->transcode_done, 0, 0);
    sem_init(&transcode_sem->enc_mutex, 0, 0);
    sem_init(&transcode_sem->fps_mutex, 0, 0);
    sem_init(&transcode_sem->exit_threads_mutex, 0, 1);
  }

  for (int i = 0; i < transcode_ctx->no_of_streams; i++) {
    xlnx_utils_start_tracking_time(&transcode_ctx[i].app_timer);
    pthread_create(&dec_thread[i], NULL, decoder_thread, &transcode_ctx[i]);
    pthread_create(&scal_thread[i], NULL, scaler_thread, &transcode_ctx[i]);
    pthread_create(&enc_thread[i], NULL, encoder_thread, &transcode_ctx[i]);
    pthread_create(&fp_thread[i], NULL, fps_thread, &transcode_ctx[i]);
  }

  for (int i = 0; i < transcode_ctx->no_of_streams; i++) {
    pthread_join(dec_thread[i], NULL);
    pthread_join(scal_thread[i], NULL);
    pthread_join(enc_thread[i], NULL);
    pthread_join(fp_thread[i], NULL);
    xlnx_tran_print_total_fps(&transcode_ctx[i]);
  }

  free(dec_thread);
  free(scal_thread);
  free(enc_thread);
  free(fp_thread);

  return NULL;
}

/**
 * xlnx_tran_frame_process: Process a frame through a state in transcoder
 * pipeline
 *
 * @param transcode_ctx: Transcoder context
 * @return XMA_APP_SUCCESS or XMA_APP_ERROR
 */
int32_t xlnx_tran_frame_process(XlnxTranscoderCtx transcode_ctx[]) {
  pthread_t manager_thread;
  /* Setting signal handler for transcoder */
  xlnx_tran_set_signal_handler();
  xlnx_utils_set_non_blocking(1);
  pthread_create(&manager_thread, NULL, master_thread, transcode_ctx);
  pthread_join(manager_thread, NULL);
  xlnx_utils_set_non_blocking(0);
  return XMA_APP_SUCCESS;
}
