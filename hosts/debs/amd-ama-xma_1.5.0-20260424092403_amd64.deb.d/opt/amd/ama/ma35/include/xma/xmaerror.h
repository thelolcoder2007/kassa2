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

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * DOC:
 *  @def @XMA_SUCCESS - Normal return
 *
 *  @def @XMA_SEND_MORE_DATA - More data needed by kernel before receive
 *                             function can be called
 *
 *  @def @XMA_EOS - End of data stream
 *
 *  @def @XMA_TRY_AGAIN - No data is available. Wait and try again until
 *                        XMA_SUCCESS is returned, then data can be recieved.
 *
 *  @def @XMA_RESEND_AND_RECV - No data is available. Wait and try recieving
 *                              data again.
 *
 *  @def @XMA_ERROR - Unspecified error has occured. Check log files for more
 *                    info.
 *
 *  @def @XMA_ERROR_INVALID - Invalid or malformed data has been passed to a
 *                            function.
 *
 *  @def @XMA_ERROR_NO_DEV - No session could be created because there is no
 *                           free device.
 *
 *  @def @XMA_ERROR_TIMEOUT - Routine timed out.
 *
 *  @def @XMA_ERROR_NO_CHAN - No channels remain to be allocated on the kernel
 *
 *  @def @XMA_ERROR_BAD_ALLOC - Out of memory
 *
 *  @def @XMA_ERROR_OUT_OF_RANGE - It reports errors that are consequence of
 *                                 attempt to access elements out of defined
 *                                 range.
 *
 *  @def @XMA_ERROR_LENGTH_ERROR - It reports errors that result from attempts
 *                                 to exceed implementation defined length
 *                                 limits for some object.
*/

// clang-format off
//See recommended API flow diagram for more info
#define XMA_SUCCESS (0)         /**< Success */
#define XMA_SEND_MORE_DATA (1)  /**< More data needs to be sent before any
                                     output is available. */
#define XMA_EOS (2)             /**< End of stream has been reached. */
#define XMA_FLUSH_AGAIN (3)     /**< Not used. */
#define XMA_TRY_AGAIN (4)       /**< Either EOS has been sent, but no data is
                                     available; resend EOS, or internal buffers
                                     are full, wait and resend data. */
#define XMA_RESEND_AND_RECV (5) /**< Data is being processed, wait a few
                                     milliseconds and try recieving data
                                     again. */

#define XMA_ERROR (-1)              /**< Unknown error, check the log for more
                                         details */
#define XMA_ERROR_INVALID (-2)      /**< Invalid input */
#define XMA_ERROR_NO_DEV (-3)       /**< No device resource available */
#define XMA_ERROR_TIMEOUT (-4)      /**< Time allocated for call exceeded */
#define XMA_ERROR_NO_CHAN (-5)      /**< No more channels available on the
                                         kernel */
#define XMA_ERROR_BAD_ALLOC (-6)    /**< Out of memory */
#define XMA_ERROR_OUT_OF_RANGE (-7) /**< Out of range */
#define XMA_ERROR_LENGTH_ERROR (-8) /**< Length exceeds limits */
// clang-format on

#define XMA_ERROR_SUCCESS_MSG "XMA_SUCCESS: success"
#define XMA_ERROR_SEND_MORE_DATA_MSG "XMA_SEND_MORE_DATA: send more data"
#define XMA_ERROR_EOS_MSG "XMA_EOS: end of stream"
#define XMA_ERROR_FLUSH_AGAIN_MSG "XMA_FLUSH_AGAIN: flush again"
#define XMA_ERROR_TRY_AGAIN_MSG "XMA_TRY_AGAIN: try again"
#define XMA_ERROR_RESEND_AND_RECV_MSG "XMA_RESEND_AND_RECV: resend and receive"
#define XMA_ERROR_MSG "XMA_ERROR: error condition"
#define XMA_ERROR_INVALID_MSG "XMA_ERROR_INVALID: invalid input supplied"
#define XMA_ERROR_NO_DEV_MSG "XMA_ERROR_NO_DEV: no device resource available"
#define XMA_ERROR_TIMEOUT_MSG "XMA_ERROR_TIMEOUT: time alloted for call exceeded"
#define XMA_ERROR_NO_CHAN_MSG "XMA_ERROR_NO_CHAN: no more channels available on kernel"
#define XMA_ERROR_BAD_ALLOC_MSG "XMA_ERROR_BAD_ALLOC: out of memory"
#define XMA_ERROR_OUT_OF_RANGE_MSG "XMA_ERROR_OUT_OF_RANGE: out of range"
#define XMA_ERROR_LENGTH_ERROR_MSG "XMA_ERROR_LENGTH_ERROR: length exceeds limits"

/**
 * xma_perror() - Copy error message to buffer
 *
 * @err:  return code for which a string description is requested
 * @buff: string buffer to hold string description
 * @sz:   size of string buffer
 *
 * RETURN: pointer to buff populated with error string corresponding to err
*/
static inline char* xma_perror(int err, char* buff, size_t sz) {
  switch (err) {
  case XMA_SUCCESS:
    snprintf(buff, sz, XMA_ERROR_SUCCESS_MSG);
    break;
  case XMA_SEND_MORE_DATA:
    snprintf(buff, sz, XMA_ERROR_SEND_MORE_DATA_MSG);
    break;
  case XMA_EOS:
    snprintf(buff, sz, XMA_ERROR_EOS_MSG);
    break;
  case XMA_FLUSH_AGAIN:
    snprintf(buff, sz, XMA_ERROR_FLUSH_AGAIN_MSG);
    break;
  case XMA_TRY_AGAIN:
    snprintf(buff, sz, XMA_ERROR_TRY_AGAIN_MSG);
    break;
  case XMA_RESEND_AND_RECV:
    snprintf(buff, sz, XMA_ERROR_RESEND_AND_RECV_MSG);
    break;
  case XMA_ERROR:
    snprintf(buff, sz, XMA_ERROR_MSG);
    break;
  case XMA_ERROR_INVALID:
    snprintf(buff, sz, XMA_ERROR_INVALID_MSG);
    break;
  case XMA_ERROR_NO_DEV:
    snprintf(buff, sz, XMA_ERROR_NO_DEV_MSG);
    break;
  case XMA_ERROR_TIMEOUT:
    snprintf(buff, sz, XMA_ERROR_TIMEOUT_MSG);
    break;
  case XMA_ERROR_NO_CHAN:
    snprintf(buff, sz, XMA_ERROR_NO_CHAN_MSG);
    break;
  case XMA_ERROR_BAD_ALLOC:
    snprintf(buff, sz, XMA_ERROR_BAD_ALLOC_MSG);
    break;
  case XMA_ERROR_OUT_OF_RANGE:
    snprintf(buff, sz, XMA_ERROR_OUT_OF_RANGE_MSG);
    break;
  case XMA_ERROR_LENGTH_ERROR:
    snprintf(buff, sz, XMA_ERROR_LENGTH_ERROR_MSG);
    break;
  }
  return buff;
}

#ifdef __cplusplus
}
#endif
