// Copyright (c) 2020 XMOS LIMITED. This Software is subject to the terms of the
// XMOS Public License: Version 1

#include <platform.h>
#include <xs1.h>
#include <xcore/channel.h>

/* FreeRTOS headers */
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "queue.h"

/* Library headers */
#include "rtos_printf.h"
#include "device_control.h"
#include "src.h"

/* App headers */
#include "app_conf.h"
#include "platform/platform_init.h"
#include "platform/driver_instances.h"
#include "app_control/app_control.h"
#include "usb_support.h"
#include "usb_audio.h"
#include "vfe_pipeline/vfe_pipeline.h"
#include "ww_model_runner/ww_model_runner.h"
#include "fs_support.h"

#include "gpio_test/gpio_test.h"

static chanend_t c_other_tile = 0;

volatile int mic_from_usb = appconfMIC_SRC_DEFAULT;
volatile int aec_ref_source = appconfAEC_REF_DEFAULT;

void vfe_pipeline_input(void *input_app_data,
                        int32_t (*mic_audio_frame)[2],
                        int32_t (*ref_audio_frame)[2],
                        size_t frame_count)
{
    (void) input_app_data;

    static int flushed;
    while (!flushed) {
        size_t received;
        received = rtos_mic_array_rx(mic_array_ctx,
                                     mic_audio_frame,
                                     frame_count,
                                     0);
        if (received == 0) {
            rtos_mic_array_rx(mic_array_ctx,
                              mic_audio_frame,
                              frame_count,
                              portMAX_DELAY);
            flushed = 1;
        }
    }

    app_control_ap_handler(NULL, 0);

    /*
     * NOTE: ALWAYS receive the next frame from the PDM mics,
     * even if USB is the current mic source. The controls the
     * timing since usb_audio_recv() does not block and will
     * receive all zeros if no frame is available yet.
     */
    rtos_mic_array_rx(mic_array_ctx,
                      mic_audio_frame,
                      frame_count,
                      portMAX_DELAY);

#if appconfUSB_ENABLED
    int32_t (*usb_mic_audio_frame)[2] = NULL;

    if (mic_from_usb) {
        usb_mic_audio_frame = mic_audio_frame;
    }

    /*
     * As noted above, this does not block.
     */
    usb_audio_recv(intertile_ctx,
                   frame_count,
                   ref_audio_frame,
                   usb_mic_audio_frame);
#endif

#if appconfI2S_ENABLED
    if (!appconfUSB_ENABLED || aec_ref_source == appconfAEC_REF_I2S) {
        /* This shouldn't need to block given it shares a clock with the PDM mics */

        size_t rx_count =
        rtos_i2s_rx(i2s_ctx,
                    (int32_t*) ref_audio_frame,
                    frame_count,
                    portMAX_DELAY);
        xassert(rx_count == frame_count);
    }
#endif
}

int vfe_pipeline_output(void *output_app_data,
                        int32_t (*proc_audio_frame)[2],
                        int32_t (*mic_audio_frame)[2],
                        int32_t (*ref_audio_frame)[2],
                        size_t frame_count)
{
    (void) output_app_data;

#if appconfI2S_ENABLED
#if !appconfI2S_TDM_ENABLED
    rtos_i2s_tx(i2s_ctx,
                (int32_t*) proc_audio_frame,
                frame_count,
                portMAX_DELAY);
#else
    for (int i = 0; i < frame_count; i++) {

        int32_t tdm_output[6];

        tdm_output[0] = mic_audio_frame[i][0] | 0x1;
        tdm_output[1] = mic_audio_frame[i][1] | 0x1;
        tdm_output[2] = ref_audio_frame[i][0] & ~0x1;
        tdm_output[3] = ref_audio_frame[i][1] & ~0x1;
        tdm_output[4] = proc_audio_frame[i][0] & ~0x1;
        tdm_output[5] = proc_audio_frame[i][1] & ~0x1;

        rtos_i2s_tx(i2s_ctx,
                    tdm_output,
                    appconfI2S_AUDIO_SAMPLE_RATE / appconfAUDIO_PIPELINE_SAMPLE_RATE,
                    portMAX_DELAY);
    }
#endif
#endif

#if appconfUSB_ENABLED
    usb_audio_send(intertile_ctx,
                  frame_count,
                  proc_audio_frame,
                  ref_audio_frame,
                  mic_audio_frame);
#endif

#if appconfWW_ENABLED
    ww_audio_send(intertile_ctx,
                  frame_count,
                  proc_audio_frame);
#endif

#if appconfSPI_OUTPUT_ENABLED
    void spi_audio_send(rtos_intertile_t *intertile_ctx,
                        size_t frame_count,
                        int32_t (*processed_audio_frame)[2],
                        int32_t (*reference_audio_frame)[2],
                        int32_t (*raw_mic_audio_frame)[2]);

    spi_audio_send(intertile_ctx,
                   frame_count,
                   proc_audio_frame,
                   ref_audio_frame,
                   mic_audio_frame);
#endif

    return VFE_PIPELINE_FREE_FRAME;
}

RTOS_I2S_APP_SEND_FILTER_CALLBACK_ATTR
size_t i2s_send_upsample_cb(rtos_i2s_t *ctx, void *app_data, int32_t *i2s_frame, size_t i2s_frame_size, int32_t *send_buf, size_t samples_available)
{
    static int i;
    static int32_t src_data[2][SRC_FF3V_FIR_TAPS_PER_PHASE] __attribute__((aligned(8)));

    xassert(i2s_frame_size == 2);

    switch (i) {
    case 0:
        i = 1;
        if (samples_available >= 2) {
            i2s_frame[0] = src_us3_voice_input_sample(src_data[0], src_ff3v_fir_coefs[2], send_buf[0]);
            i2s_frame[1] = src_us3_voice_input_sample(src_data[1], src_ff3v_fir_coefs[2], send_buf[1]);
            return 2;
        } else {
            i2s_frame[0] = src_us3_voice_input_sample(src_data[0], src_ff3v_fir_coefs[2], 0);
            i2s_frame[1] = src_us3_voice_input_sample(src_data[1], src_ff3v_fir_coefs[2], 0);
            return 0;
        }
    case 1:
        i = 2;
        i2s_frame[0] = src_us3_voice_get_next_sample(src_data[0], src_ff3v_fir_coefs[1]);
        i2s_frame[1] = src_us3_voice_get_next_sample(src_data[1], src_ff3v_fir_coefs[1]);
        return 0;
    case 2:
        i = 0;
        i2s_frame[0] = src_us3_voice_get_next_sample(src_data[0], src_ff3v_fir_coefs[0]);
        i2s_frame[1] = src_us3_voice_get_next_sample(src_data[1], src_ff3v_fir_coefs[0]);
        return 0;
    default:
        xassert(0);
        return 0;
    }
}

RTOS_I2S_APP_RECEIVE_FILTER_CALLBACK_ATTR
size_t i2s_send_downsample_cb(rtos_i2s_t *ctx, void *app_data, int32_t *i2s_frame, size_t i2s_frame_size, int32_t *receive_buf, size_t sample_spaces_free)
{
    static int i;
    static int64_t sum[2];
    static int32_t src_data[2][SRC_FF3V_FIR_NUM_PHASES][SRC_FF3V_FIR_TAPS_PER_PHASE] __attribute__((aligned (8)));

    xassert(i2s_frame_size == 2);

    switch (i) {
    case 0:
        i = 1;
        sum[0] = src_ds3_voice_add_sample(0, src_data[0][0], src_ff3v_fir_coefs[0], i2s_frame[0]);
        sum[1] = src_ds3_voice_add_sample(0, src_data[1][0], src_ff3v_fir_coefs[0], i2s_frame[1]);
        return 0;
    case 1:
        i = 2;
        sum[0] = src_ds3_voice_add_sample(sum[0], src_data[0][1], src_ff3v_fir_coefs[1], i2s_frame[0]);
        sum[1] = src_ds3_voice_add_sample(sum[1], src_data[1][1], src_ff3v_fir_coefs[1], i2s_frame[1]);
        return 0;
    case 2:
        i = 0;
        if (sample_spaces_free >= 2) {
            receive_buf[0] = src_ds3_voice_add_final_sample(sum[0], src_data[0][2], src_ff3v_fir_coefs[2], i2s_frame[0]);
            receive_buf[1] = src_ds3_voice_add_final_sample(sum[1], src_data[1][2], src_ff3v_fir_coefs[2], i2s_frame[1]);
            return 2;
        } else {
            (void) src_ds3_voice_add_final_sample(sum[0], src_data[0][2], src_ff3v_fir_coefs[2], i2s_frame[0]);
            (void) src_ds3_voice_add_final_sample(sum[1], src_data[1][2], src_ff3v_fir_coefs[2], i2s_frame[1]);
            return 0;
        }
    default:
        xassert(0);
        return 0;
    }
}

void i2s_rate_conversion_enable(void)
{
#if !appconfI2S_TDM_ENABLED
    rtos_i2s_send_filter_cb_set(i2s_ctx, i2s_send_upsample_cb, NULL);
#endif
    rtos_i2s_receive_filter_cb_set(i2s_ctx, i2s_send_downsample_cb, NULL);
}

void vApplicationMallocFailedHook(void)
{
    rtos_printf("Malloc Failed on tile %d!\n", THIS_XCORE_TILE);
    xassert(0);
    for(;;);
}

static void mem_analysis(void)
{
	for (;;) {
//		rtos_printf("Tile[%d]:\n\tMinimum heap free: %d\n\tCurrent heap free: %d\n", THIS_XCORE_TILE, xPortGetMinimumEverFreeHeapSize(), xPortGetFreeHeapSize());
		vTaskDelay(pdMS_TO_TICKS(5000));
	}
}

#include "xcore_clock_control.h"
#include "low_power.h"


#include <xcore/hwtimer.h>

#include <platform.h>
#include <xscope.h>
#include <xccompat.h>
extern void xscope_lock_acquire(void);
extern void xscope_lock_release(void);

void nop_task(void *arg)
{
    while(1)
    {
        asm volatile("nop");
    }
}

#include "concurrency_support.h"

void reader_task(void *arg)
{
    mrsw_lock_t *ctx = (mrsw_lock_t *)arg;
    while(1)
    {
        rtos_printf("reader get\n");
        if (mrsw_lock_reader_get(ctx, portMAX_DELAY) == RTOS_OSAL_SUCCESS)
        {
            rtos_printf("\treader\n");
            vTaskDelay(100);
            rtos_printf("\treader dn\n");
            mrsw_lock_reader_put(ctx);
        }
        rtos_printf("reader wait\n");
        vTaskDelay(1);
    }
}

void writer_task(void *arg)
{
    mrsw_lock_t *ctx = (mrsw_lock_t *)arg;
    while(1)
    {
        rtos_printf("writer get\n");
        if (mrsw_lock_writer_get(ctx, portMAX_DELAY) == RTOS_OSAL_SUCCESS) {
            rtos_printf("\twriter\n");
            vTaskDelay(100);
            rtos_printf("\twriter dn\n");
            mrsw_lock_writer_put(ctx);
        }
        rtos_printf("writer wait\n");
        vTaskDelay(100);
    }
}


void startup_task(void *arg)
{
    rtos_printf("Startup task running from tile %d on core %d\n", THIS_XCORE_TILE, portGET_CORE_ID());


// test mrsw
while(1) {;}




    hwtimer_t tmr = hwtimer_alloc();


    for( int i=0; i<8; i++)
    {
        xTaskCreate((TaskFunction_t) nop_task,
                    "nop_task",
                    RTOS_THREAD_STACK_SIZE(nop_task),
                    NULL,
                    5,
                    NULL);
    }


    // rtos_printf("tile 0 id is 0x%x\ntile 1 id is 0x%x\ntile 1 id is 0x%x\n",
    //         TILE_ID(0),
    //         TILE_ID(1),
    //         get_local_tile_id()
    //     );
    init_tile_clock_divider();
#if ON_TILE(0)
    while(1)
    {
        rtos_printf("tile %d delay 1s\n", THIS_XCORE_TILE);
        vTaskDelay(pdMS_TO_TICKS(1000));
        rtos_printf("tile %d wakeup and chanend\n", THIS_XCORE_TILE);
        chan_out_byte(c_other_tile, 0xA5);

        // unsigned pre_div = 0;
        // unsigned mul = 0;
        // unsigned post_div = 0;
        // get_local_node_pll_ratio(&pre_div, &mul, &post_div);
        // rtos_printf("tile %d pll ratios\n\tpre_div = %d\n\tmul = %d\n\tpost_div = %d\n", THIS_XCORE_TILE, pre_div, mul, post_div);
        //
        // set_local_node_pll_ratio(1, 25, 1);
        //
        // get_local_node_pll_ratio(&pre_div, &mul, &post_div);
        // rtos_printf("tile %d pll ratios\n\tpre_div = %d\n\tmul = %d\n\tpost_div = %d\n", THIS_XCORE_TILE, pre_div, mul, post_div);
        //
        // vTaskDelay(pdMS_TO_TICKS(20000));
        // rtos_printf("tile %d disable local tile clock\n", THIS_XCORE_TILE);
        // disable_local_tile_processor_clock();
    }
#endif /* ON_TILE(0) */
#if ON_TILE(1)
    while(1)
    {
        rtos_printf("tile %d delay 5s\n", THIS_XCORE_TILE);
        vTaskDelay(pdMS_TO_TICKS(5000));
        // rtos_printf("tile %d reset links\n", THIS_XCORE_TILE);
        // dump_links(TILE_ID(0), TILE_ID(1));
        // vTaskDelay(pdMS_TO_TICKS(1000));
        // reset_local_links()
        rtos_printf("tile %d wakeup and chanend got %x\n", THIS_XCORE_TILE, chan_in_byte(c_other_tile));

        rtos_printf("power down\n");
        xscope_lock_acquire();
        power_down_from_this_tile();

        unsigned local_switch_freq = get_local_switch_clock();
        unsigned local_tile_freq = get_local_tile_processor_clock();
        unsigned local_vco = get_local_tile_vco_clock();
        // hwtimer_delay(tmr, 100000000);
        for(int i=0; i<20000000;i++)
        {
            asm volatile("nop");
        }
        power_up_from_this_tile();
        xscope_lock_release();
        rtos_printf("powered up\n");
        rtos_printf("during power down switch: %d tile: %d vco: %d\n", local_switch_freq, local_tile_freq, local_vco);

        local_switch_freq = get_local_switch_clock();
        local_tile_freq = get_local_tile_processor_clock();
        local_vco = get_local_tile_vco_clock();
        rtos_printf("during power up switch: %d tile: %d vco: %d\n", local_switch_freq, local_tile_freq, local_vco);


        // unsigned pre_div = 0;
        // unsigned mul = 0;
        // unsigned post_div = 0;
        // get_local_node_pll_ratio(&pre_div, &mul, &post_div);
        // rtos_printf("tile %d pll ratios\n\tpre_div = %d\n\tmul = %d\n\tpost_div = %d\n", THIS_XCORE_TILE, pre_div, mul, post_div);


        // rtos_printf("tile %d disable local tile clock\n", THIS_XCORE_TILE);
        // disable_local_tile_processor_clock();
        // rtos_printf("tile %d delay 3s\n", THIS_XCORE_TILE);
        // vTaskDelay(pdMS_TO_TICKS(3000));
    }
#endif /* ON_TILE(1) */

    platform_start();

#if ON_TILE(1)
    gpio_test(gpio_ctx_t0);
#endif

#if ON_TILE(AUDIO_HW_TILE_NO)
    app_control_ap_servicer_register();
    vfe_pipeline_init(NULL, NULL);
#endif

#if ON_TILE(FS_TILE_NO)
    rtos_fatfs_init(qspi_flash_ctx);
#endif

#if appconfWW_ENABLED && ON_TILE(WW_TILE_NO)
    ww_task_create(appconfWW_TASK_PRIORITY);
#endif

    mem_analysis();
    /*
     * TODO: Watchdog?
     */
}

void vApplicationMinimalIdleHook(void)
{
    rtos_printf("idle hook on tile %d core %d\n", THIS_XCORE_TILE, rtos_core_id_get());
    asm volatile("waiteu");
}

static void tile_common_init(chanend_t c)
{
    control_ret_t ctrl_ret;

    platform_init(c);
    c_other_tile = c;
    // chanend_free(c);

    ctrl_ret = app_control_init();
    xassert(ctrl_ret == CONTROL_SUCCESS);

#if appconfUSB_ENABLED && ON_TILE(USB_TILE_NO)
    usb_audio_init(intertile_ctx, appconfUSB_AUDIO_TASK_PRIORITY);
#endif

    xTaskCreate((TaskFunction_t) startup_task,
                "startup_task",
                RTOS_THREAD_STACK_SIZE(startup_task),
                NULL,
                appconfSTARTUP_TASK_PRIORITY,
                NULL);

    #if ON_TILE(0)
        mrsw_lock_t lock;

        mrsw_lock_create(&lock, NULL, MRSW_READER_PREFERRED);
        // rtos_printf("%d r create res:%x\n",THIS_XCORE_TILE, mrsw_lock_create(&lock, NULL, MRSW_READER_PREFERRED));
        // rtos_printf("%d r delete res:%x\n",THIS_XCORE_TILE, mrsw_lock_delete(&lock));
        // mrsw_lock_create(&lock, NULL, MRSW_WRITER_PREFERRED);
        // rtos_printf("%d w create res:%x\n",THIS_XCORE_TILE, mrsw_lock_create(&lock, NULL, MRSW_WRITER_PREFERRED));
        // rtos_printf("%d w delete res:%x\n",THIS_XCORE_TILE, mrsw_lock_delete(&lock));

        for( int i=0; i<2; i++)
        {
            xTaskCreate((TaskFunction_t) writer_task,
                        "writer_task",
                        RTOS_THREAD_STACK_SIZE(writer_task),
                        &lock,
                        5,
                        NULL);
        }

        for( int i=0; i<5; i++)
        {
            xTaskCreate((TaskFunction_t) reader_task,
                        "reader_task",
                        RTOS_THREAD_STACK_SIZE(reader_task),
                        &lock,
                        5,
                        NULL);
        }


    #endif

    rtos_printf("start scheduler on tile %d\n", THIS_XCORE_TILE);
    vTaskStartScheduler();
}

#if ON_TILE(0)
void main_tile0(chanend_t c0, chanend_t c1, chanend_t c2, chanend_t c3)
{
    (void) c0;
    (void) c2;
    (void) c3;

    tile_common_init(c1);
}
#endif

#if ON_TILE(1)
void main_tile1(chanend_t c0, chanend_t c1, chanend_t c2, chanend_t c3)
{
    (void) c1;
    (void) c2;
    (void) c3;

    tile_common_init(c0);
}
#endif
