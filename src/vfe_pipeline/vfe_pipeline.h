// Copyright (c) 2020, XMOS Ltd, All rights reserved

#ifndef VFE_PIPELINE_H_
#define VFE_PIPELINE_H_

#include "rtos/drivers/mic_array/api/rtos_mic_array.h"
#include "rtos/drivers/i2s/api/rtos_i2s_master.h"

#include "voice_front_end_settings.h"

#define VFE_PIPELINE_AUDIO_SAMPLE_RATE 16000
#define VFE_PIPELINE_AUDIO_FRAME_LENGTH VFE_FRAME_ADVANCE

void vfe_pipeline_init(
        rtos_mic_array_t *mic_array_ctx,
        rtos_i2s_master_t *i2s_master_ctx);

#endif /* VFE_PIPELINE_H_ */
