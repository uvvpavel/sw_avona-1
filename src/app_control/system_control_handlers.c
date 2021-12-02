// Copyright (c) 2021 XMOS LIMITED. This Software is subject to the terms of the
// XMOS Public License: Version 1

#include <string.h>
#include "app_control.h"

#include "platform/driver_instances.h"
#include "spi/spi_interface.h"

#define APP_CONTROL_CMD_INTERRUPT_ENABLED       0x00
#define APP_CONTROL_CMD_SYSTEM_INTERRUPT_STATUS 0x01
#define APP_CONTROL_CMD_SPI_FRAME_COUNT         0x02

static device_control_servicer_t system_servicer;

static SemaphoreHandle_t gpo_mutex;
static EventGroupHandle_t interrupt_flags;
static rtos_gpio_port_id_t intn_port;
static const int intn_mask = 0x08;

static volatile int interrupt_out_enabled;

void app_control_system_interrupt_set(uint8_t status)
{
    xSemaphoreTake(gpo_mutex, portMAX_DELAY);
    {
        uint32_t gpo_val;

//        rtos_printf("Asserting interrupt %02x\n", status);
        xEventGroupSetBits(interrupt_flags, (EventBits_t) status);

        if (interrupt_out_enabled) {
            gpo_val = rtos_gpio_port_in(gpio_ctx_t0, intn_port);
            gpo_val &= ~intn_mask;
            rtos_gpio_port_out(gpio_ctx_t0, intn_port, gpo_val);
        }
    }
    xSemaphoreGive(gpo_mutex);
}

DEVICE_CONTROL_CALLBACK_ATTR
static control_ret_t system_read_cmd(control_resid_t resid, control_cmd_t cmd, uint8_t *payload, size_t payload_len, void *app_data)
{
    control_ret_t ret = CONTROL_SUCCESS;

    switch (cmd) {
    case CONTROL_CMD_SET_READ(APP_CONTROL_CMD_INTERRUPT_ENABLED):
        if (payload_len == sizeof(uint8_t)) {
            *((uint8_t *) payload) = interrupt_out_enabled;
        } else {
            ret = CONTROL_DATA_LENGTH_ERROR;
        }
        break;

    case CONTROL_CMD_SET_READ(APP_CONTROL_CMD_SYSTEM_INTERRUPT_STATUS):
        if (payload_len == sizeof(uint8_t)) {

            EventBits_t interrupt_status;

            xSemaphoreTake(gpo_mutex, portMAX_DELAY);
            {
                uint32_t gpo_val;

                interrupt_status = xEventGroupWaitBits(interrupt_flags,
                                                       0xFF,
                                                       pdTRUE,
                                                       pdFALSE,
                                                       0);

//                rtos_printf("Returning interrupt flags %02x\n", interrupt_status & 0xFF);

                if (interrupt_out_enabled) {
                    gpo_val = rtos_gpio_port_in(gpio_ctx_t0, intn_port);
                    gpo_val |= intn_mask;
                    rtos_gpio_port_out(gpio_ctx_t0, intn_port, gpo_val);
                }
            }
            xSemaphoreGive(gpo_mutex);

            *((uint8_t *) payload) = interrupt_status & 0xFF;
        } else {
            ret = CONTROL_DATA_LENGTH_ERROR;
        }
        break;

    case CONTROL_CMD_SET_READ(APP_CONTROL_CMD_SPI_FRAME_COUNT):
        if (payload_len == sizeof(uint32_t)) {
            *((uint32_t *) payload) = spi_audio_frames_available();
        } else {
            ret = CONTROL_DATA_LENGTH_ERROR;
        }
        break;

    default:
        memset(payload, 0, payload_len);
        ret = CONTROL_BAD_COMMAND;
    }

    return ret;
}

DEVICE_CONTROL_CALLBACK_ATTR
static control_ret_t system_write_cmd(control_resid_t resid, control_cmd_t cmd, const uint8_t *payload, size_t payload_len, void *app_data)
{
    control_ret_t ret = CONTROL_SUCCESS;

    switch (cmd) {

    case CONTROL_CMD_SET_WRITE(APP_CONTROL_CMD_INTERRUPT_ENABLED):
        if (payload_len == sizeof(uint8_t)) {
            xSemaphoreTake(gpo_mutex, portMAX_DELAY);
            {
                uint32_t gpo_val;
                gpo_val = rtos_gpio_port_in(gpio_ctx_t0, intn_port);

                interrupt_out_enabled = *((uint8_t *) payload);

                if (interrupt_out_enabled) {
                    rtos_printf("Interrupts enabled\n");
                    EventBits_t interrupt_status;
                    interrupt_status = xEventGroupGetBits(interrupt_flags);
                    interrupt_status &= 0xFF;
                    if (interrupt_status) {
                        rtos_printf("Asserting interrupt out\n");
                        gpo_val &= ~intn_mask;
                    } else {
                        rtos_printf("Deasserting interrupt out\n");
                        gpo_val |= intn_mask;
                    }
                } else {
                    rtos_printf("Interrupts disabled\n");
                    gpo_val |= intn_mask;
                }

                rtos_gpio_port_out(gpio_ctx_t0, intn_port, gpo_val);
            }
            xSemaphoreGive(gpo_mutex);
        } else {
            ret = CONTROL_DATA_LENGTH_ERROR;
        }
        break;

    default:
        ret = CONTROL_BAD_COMMAND;
    }

    return ret;
}

void app_control_system_handler(void *state, unsigned timeout)
{
    device_control_servicer_cmd_recv(&system_servicer, system_read_cmd, system_write_cmd, state, timeout);
}

static void system_control_task(void *arg)
{
    for (;;) {
        app_control_system_handler(NULL, portMAX_DELAY);
    }
}

void app_control_system_servicer_register(void)
{
    const control_resid_t resources[] = {APP_CONTROL_RESID_SYSTEM};
    control_ret_t dc_ret;

    intn_port = rtos_gpio_port(PORT_GPO);
    rtos_gpio_port_enable(gpio_ctx_t0, intn_port);
    rtos_gpio_port_out(gpio_ctx_t0, intn_port, intn_mask);

    gpo_mutex = xSemaphoreCreateMutex();
    interrupt_flags = xEventGroupCreate();

    rtos_printf("Will register the system servicer now\n");
    dc_ret = app_control_servicer_register(&system_servicer,
                                           resources, sizeof(resources));
    xassert(dc_ret == CONTROL_SUCCESS);
    rtos_printf("System servicer registered\n");

    xTaskCreate((TaskFunction_t) system_control_task, "system_control_task", portTASK_STACK_DEPTH(system_control_task), NULL, configMAX_PRIORITIES-1, NULL);
}
