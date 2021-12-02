// Copyright (c) 2020 XMOS LIMITED. This Software is subject to the terms of the
// XMOS Public License: Version 1

#ifndef SPI_INTERFACE_H_
#define SPI_INTERFACE_H_

#include <stdlib.h>
#include <stdint.h>

#include "rtos/drivers/spi/api/rtos_spi_slave.h"

size_t spi_audio_frames_available();

void spi_audio_send(rtos_intertile_t *intertile_ctx,
                    size_t frame_count,
                    int32_t (*processed_audio_frame)[2],
                    int32_t (*reference_audio_frame)[2],
                    int32_t (*raw_mic_audio_frame)[2]);


void spi_slave_start_cb(rtos_spi_slave_t *ctx, void *app_data);
void spi_slave_xfer_done_cb(rtos_spi_slave_t *ctx, void *app_data);


#endif /* SPI_INTERFACE_H_ */
