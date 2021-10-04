/*
 * Trace Recorder for Tracealyzer v989.878.767
 * Copyright 2021 Percepio AB
 * www.percepio.com
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * The interface definitions for trace streaming ("stream ports").
 * This "stream port" sets up the recorder to use XMOS xScope as a streaming channel.
 */

#ifndef TRC_STREAMING_PORT_H
#define TRC_STREAMING_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

int32_t readFromxScope(void* ptrData, uint32_t size, int32_t* ptrBytesRead);
int32_t writeToxScope(void* ptrData, uint32_t size, int32_t* ptrBytesWritten);


#define TRC_STREAM_PORT_ALLOCATE_FIELDS()
#define TRC_STREAM_PORT_INIT()

#define TRC_STREAM_PORT_USE_INTERNAL_BUFFER 0
  
#define TRC_STREAM_PORT_WRITE_DATA(_ptrData, _size, _ptrBytesWritten) writeToxScope(_ptrData, _size, _ptrBytesWritten)

#define TRC_STREAM_PORT_READ_DATA(_ptrData, _size, _ptrBytesRead) readFromxScope(_ptrData, _size, _ptrBytesRead)

#ifdef __cplusplus
}
#endif

#endif /* TRC_STREAMING_PORT_H */
