/*
 * Trace Recorder for Tracealyzer v989.878.767
 * Copyright 2021 Percepio AB
 * www.percepio.com
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Supporting functions for trace streaming, used by the "stream ports" 
 * for reading and writing data to the interface.
 *
 * Note that this stream port is more complex than the typical case, since
 * the J-Link interface uses a separate RAM buffer in SEGGER_RTT.c, instead
 * of the default buffer included in the recorder core. The other stream ports 
 * offer more typical examples of how to define a custom streaming interface.
 */
 
#include "trcRecorder.h"
#include <xscope.h>

#if (TRC_USE_TRACEALYZER_RECORDER == 1)
#if (TRC_CFG_RECORDER_MODE == TRC_RECORDER_MODE_STREAMING)

int32_t readFromRTT(void* ptrData, uint32_t size, int32_t* ptrBytesRead)
{
	uint32_t bytesRead = 0; 
	
	if (SEGGER_RTT_HASDATA(TRC_CFG_RTT_DOWN_BUFFER_INDEX))
	{
		bytesRead = SEGGER_RTT_Read((TRC_CFG_RTT_DOWN_BUFFER_INDEX), (char*)ptrData, size);
	
		if (ptrBytesRead != NULL)
			*ptrBytesRead = (int32_t)bytesRead;

	}

	return 0;
}

extern int g_tz_tile;
static int write_count = 1;
static char buffer[2][128];
static uint32_t bytes_in_buffer[2] = {0, 0};
static int time_last = 0;

int32_t writeToRTT(void* ptrData, uint32_t size, int32_t* ptrBytesWritten)
{
	//if ((bytes_in_buffer[g_tz_tile] + size) < 128) {
	//	memcpy((char*)&buffer[g_tz_tile][bytes_in_buffer[g_tz_tile]], ptrData, size);
	//	bytes_in_buffer[g_tz_tile] += size;
	//} else {
		//xscope_bytes(g_tz_tile, bytes_in_buffer[g_tz_tile], (unsigned char*)buffer[g_tz_tile]);
		//bytes_in_buffer[g_tz_tile] = 0;
		//memcpy((char*)&buffer[g_tz_tile][bytes_in_buffer[g_tz_tile]], ptrData, size);
	//}

	if ((time_last + 200) < xscope_gettime()) {
		for (volatile int i = 0; i < 200; i++);
		time_last = xscope_gettime();
	}

	xscope_bytes(g_tz_tile, size, (unsigned char*)ptrData);
	
	*ptrBytesWritten = size;
	
	//uint32_t bytesWritten = SEGGER_RTT_Write((TRC_CFG_RTT_UP_BUFFER_INDEX), (const char*)ptrData, size);
	
	//if (ptrBytesWritten != NULL)
	//	*ptrBytesWritten = (int32_t)bytesWritten;

	return 0;
}

#endif
#endif
