/*
 * Trace Recorder for Tracealyzer v989.878.767
 * Copyright 2021 Percepio AB
 * www.percepio.com
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Supporting functions for trace streaming, used by the "stream ports" 
 * for reading and writing data to the interface.
 */
 
#include <xscope.h>
#include <string.h>
#include "trcRecorder.h"

#if (TRC_USE_TRACEALYZER_RECORDER == 1)
#if (TRC_CFG_RECORDER_MODE == TRC_RECORDER_MODE_STREAMING)

int32_t readFromxScope(void* ptrData, uint32_t size, int32_t* ptrBytesRead)
{
	return 0;
}

uint32_t g_bytes_written[configNUM_CORES] = {0};

#define WRITE_BUFFER_SIZE 1*1024

#ifdef TRC_CFG_PLATFORM_LOCKLESS
volatile char 		g_write_buffer[configNUM_CORES][WRITE_BUFFER_SIZE]	= {{0}};
volatile uint32_t 	g_write_buffer_bytes[configNUM_CORES]				= {0};
#else
volatile char		g_write_buffer[WRITE_BUFFER_SIZE]	= {0};
volatile uint32_t	g_write_buffer_bytes 				= 0;
#endif

int32_t writeToxScope(void* ptrData, uint32_t size, int32_t* ptrBytesWritten)
{
	TRACE_ALLOC_CRITICAL_SECTION();
	TRACE_ENTER_CRITICAL_SECTION();

	unsigned core_id = TRC_GET_CURRENT_CORE();
#if 0
#ifdef TRC_CFG_PLATFORM_LOCKLESS
	if ((g_write_buffer_bytes[core_id] + size) >= WRITE_BUFFER_SIZE) {
		xscope_bytes(0, g_write_buffer_bytes[core_id], (unsigned char*)g_write_buffer[core_id]);
		
		g_write_buffer_bytes[core_id] = 0;
	}

	memcpy(&g_write_buffer[core_id][g_write_buffer_bytes[core_id]], ptrData, size);
	g_write_buffer_bytes[core_id] += size;
#else
	if ((g_write_buffer_bytes + size) >= WRITE_BUFFER_SIZE) {
		xscope_bytes(0, g_write_buffer_bytes, (unsigned char*)g_write_buffer);

		g_write_buffer_bytes = 0;
	}

	memcpy((void*)&g_write_buffer[g_write_buffer_bytes], ptrData, size);
	g_write_buffer_bytes += size;
#endif
#else
	/* Note: Write xScope byte data, while all data should get to the XTAG,
	we have no method of knowing if the host manages to read it before
	it is overwritten by new data. */
	xscope_bytes(0, size, (unsigned char*)ptrData);
#endif
	g_bytes_written[core_id] += size;

	TRACE_EXIT_CRITICAL_SECTION();
	
	/* Everything is written, there are no partial writes in xScope */
	*ptrBytesWritten = size;

	return 0;
}

#endif
#endif
