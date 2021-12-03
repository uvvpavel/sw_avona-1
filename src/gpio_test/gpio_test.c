// Copyright 2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <platform.h>

#include "FreeRTOS.h"

#include "platform/app_pll_ctrl.h"
#include "gpio_test/gpio_test.h"

#if XVF3610_Q60A
#define BUTTON_MUTE_BITMASK 0x10
#define BUTTON_BTN_BITMASK  0x20
#define BUTTON_IP_2_BITMASK 0x40
#define BUTTON_IP_3_BITMASK 0x80
#define GPIO_BITMASK    (BUTTON_MUTE_BITMASK | BUTTON_BTN_BITMASK | BUTTON_IP_2_BITMASK | BUTTON_IP_3_BITMASK)
#define GPIO_PORT       PORT_GPI
#define GPIO_USE_INTERRUPT  0

static int mute_status = -1;

#elif XCOREAI_EXPLORER
#define BUTTON_0_BITMASK    0x01
#define BUTTON_1_BITMASK    0x02
#define GPIO_BITMASK    (BUTTON_0_BITMASK | BUTTON_1_BITMASK)
#define GPIO_PORT       PORT_BUTTONS
#define GPIO_USE_INTERRUPT  1

#elif OSPREY_BOARD
#define BUTTON_0_BITMASK    0x04
#define GPIO_BITMASK    (BUTTON_0_BITMASK)
#define GPIO_PORT       PORT_BUTTON
#define GPIO_USE_INTERRUPT  1

#else
#define GPIO_PORT       0
#define GPIO_USE_INTERRUPT 1
#endif

#if GPIO_USE_INTERRUPT
RTOS_GPIO_ISR_CALLBACK_ATTR
static void gpio_callback(rtos_gpio_t *ctx, void *app_data, rtos_gpio_port_id_t port_id, uint32_t value)
{
    TaskHandle_t task = app_data;
    BaseType_t yield_required = pdFALSE;

    value &= GPIO_BITMASK;

    xTaskNotifyFromISR(task, value, eSetValueWithOverwrite, &yield_required);

    portYIELD_FROM_ISR(yield_required);
}
#endif

static void gpio_handler(rtos_gpio_t *gpio_ctx)
{
    uint32_t gpio_val;

    const rtos_gpio_port_id_t gpio_port = rtos_gpio_port(GPIO_PORT);

    rtos_gpio_port_enable(gpio_ctx, gpio_port);

#if XVF3610_Q60A
    /* Enable pullups on the GPI port since some pins are floating */
//    rtos_gpio_port_pull_up(gpio_ctx, gpio_port);
#endif

#if GPIO_USE_INTERRUPT
    /*
     * On the 3610/Voice reference design board, the SPI MISO and I2C 1 bit ports
     * overlap the 8 bit GPI port. Unfortunately, changing values on these 1 bit
     * ports trigger the GPI interrupt which is unacceptable. Disabling this for now.
     */
    rtos_gpio_isr_callback_set(gpio_ctx, gpio_port, gpio_callback, xTaskGetCurrentTaskHandle());
    rtos_gpio_interrupt_enable(gpio_ctx, gpio_port);
#else
    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t last_gpio_val = 0;
#endif

    for (;;) {
#if GPIO_USE_INTERRUPT
        xTaskNotifyWait(
                0x00000000UL,    /* Don't clear notification bits on entry */
                0xFFFFFFFFUL,    /* Reset full notification value on exit */
                &gpio_val,       /* Pass out notification value into value */
                portMAX_DELAY ); /* Wait indefinitely until next notification */
#else
        vTaskDelayUntil(&last_wake_time, 25);
        gpio_val = rtos_gpio_port_in(gpio_ctx, gpio_port);
        gpio_val &= GPIO_BITMASK;
        if (gpio_val == last_gpio_val) {
            continue;
        }
        last_gpio_val = gpio_val;
#endif

#if XVF3610_Q60A
        if (((gpio_val & BUTTON_MUTE_BITMASK) != 0) && (mute_status != 1)) {
            rtos_printf("Mute active\n");
            mute_status = 1;
        } else if (((gpio_val & BUTTON_MUTE_BITMASK) == 0) && (mute_status != 0)) {
            rtos_printf("Mute inactive\n");
            mute_status = 0;
        }

        if ((gpio_val & BUTTON_BTN_BITMASK) == 0) {
            rtos_printf("Button pressed\n");
        }
#elif XCOREAI_EXPLORER
        extern volatile int mic_from_usb;
        extern volatile int aec_ref_source;
        if (( gpio_val & BUTTON_0_BITMASK ) == 0) {
            mic_from_usb = !mic_from_usb;
            rtos_printf("Microphone from USB: %s\n", mic_from_usb ? "true" : "false");
        } else if (( gpio_val & BUTTON_1_BITMASK ) == 0) {
            aec_ref_source = !aec_ref_source;
            rtos_printf("AEC reference source: %s\n", aec_ref_source == appconfAEC_REF_I2S ? "I2S" : "USB");
        }
#elif OSPREY_BOARD
        extern volatile int mic_from_usb;
        if (( gpio_val & BUTTON_0_BITMASK ) == 0) {
            mic_from_usb = !mic_from_usb;
            rtos_printf("Microphone from USB: %s\n", mic_from_usb ? "true" : "false");
        }
#endif
    }
}

void gpio_test(rtos_gpio_t *gpio_ctx)
{
    if (GPIO_PORT != 0) {
        xTaskCreate((TaskFunction_t) gpio_handler,
                    "gpio_handler",
                    RTOS_THREAD_STACK_SIZE(gpio_handler),
                    gpio_ctx,
                    configMAX_PRIORITIES-2,
                    NULL);
    }
}
