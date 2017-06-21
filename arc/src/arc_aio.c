// Copyright (c) 2017, Intel Corporation.

// Zephyr includes
#include <zephyr.h>
#include <adc.h>

// ZJS includes
#include "arc_common.h"
#include "arc_aio.h"

// AIO thread
#define STACK_SIZE 1024
#define STACK_PRIORITY 7
static char __stack stack[STACK_SIZE];

// AIO
static struct device *adc_dev = NULL;
static atomic_t pin_enabled[ARC_AIO_LEN] = {};
static atomic_t pin_values[ARC_AIO_LEN] = {};
static atomic_t pin_last_values[ARC_AIO_LEN] = {};
static u8_t seq_buffer[ADC_BUFFER_SIZE];
static void *pin_user_data[ARC_AIO_LEN] = {};
static u8_t pin_send_updates[ARC_AIO_LEN] = {};

// AIO thread spawned to read from ADC
static void aio_read_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        for (int i = 0; i <= 5; i++) {
            if (pin_enabled[i]) {
                u32_t value = arc_pin_read(ARC_AIO_MIN + i);
                atomic_set(&pin_values[i], value);
                atomic_set(&pin_last_values[i], value);
            }
        }
    }
}

void arc_aio_init()
{
    adc_dev = device_get_binding(ADC_DEVICE_NAME);
    adc_enable(adc_dev);

    k_thread_spawn(stack, STACK_SIZE, aio_read_thread, NULL, NULL, NULL,
                   STACK_PRIORITY, 0, K_NO_WAIT);
}

void arc_aio_cleanup()
{
    adc_disable(adc_dev);
}

u32_t arc_pin_read(u8_t pin)
{
    struct adc_seq_entry entry = {
        .sampling_delay = 12,
        .channel_id = pin,
        .buffer = seq_buffer,
        .buffer_length = ADC_BUFFER_SIZE,
    };

    struct adc_seq_table entry_table = {
        .entries = &entry,
        .num_entries = 1,
    };

    if (!adc_dev) {
        ERR_PRINT("ADC device not found\n");
        return 0;
    }

    if (adc_read(adc_dev, &entry_table) != 0) {
        ERR_PRINT("couldn't read from pin %d\n", pin);
        return 0;
    }

    // read from buffer, not sure if byte order is important
    u32_t raw_value = (u32_t) seq_buffer[0]
                    | (u32_t) seq_buffer[1] << 8;

    return raw_value;
}

void arc_process_aio_updates()
{
    for (int i = 0; i <= 5; i++) {
        if (pin_send_updates[i] &&
            pin_values[i] != pin_last_values[i]) {
            // send updates only if value has changed
            // so it doesn't flood the IPM channel
            struct zjs_ipm_message msg;
            msg.id = MSG_ID_AIO;
            msg.type = TYPE_AIO_PIN_EVENT_VALUE_CHANGE;
            msg.flags = 0;
            msg.user_data = pin_user_data[i];
            msg.data.aio.pin = ARC_AIO_MIN + i;
            msg.data.aio.value = pin_values[i];
            ipm_send_msg(&msg);
        }
    }
}

void arc_handle_aio(struct zjs_ipm_message *msg)
{
    u32_t pin = msg->data.aio.pin;
    u32_t reply_value = 0;
    u32_t error_code = ERROR_IPM_NONE;

    if (pin < ARC_AIO_MIN || pin > ARC_AIO_MAX) {
        ERR_PRINT("pin #%u out of range\n", pin);
        ipm_send_error(msg, ERROR_IPM_INVALID_PARAMETER);
        return;
    }

    switch (msg->type) {
    case TYPE_AIO_OPEN:
        atomic_set(&pin_enabled[pin - ARC_AIO_MIN], 1);
        break;
    case TYPE_AIO_PIN_READ:
        reply_value = pin_last_values[pin - ARC_AIO_MIN];
        break;
    case TYPE_AIO_PIN_ABORT:
        // NO OP - always success
        break;
    case TYPE_AIO_PIN_CLOSE:
        atomic_set(&pin_enabled[pin - ARC_AIO_MIN], 0);
        break;
    case TYPE_AIO_PIN_SUBSCRIBE:
        pin_send_updates[pin - ARC_AIO_MIN] = 1;
        // save user data from subscribe request and return it in change msgs
        pin_user_data[pin - ARC_AIO_MIN] = msg->user_data;
        break;
    case TYPE_AIO_PIN_UNSUBSCRIBE:
        pin_send_updates[pin - ARC_AIO_MIN] = 0;
        pin_user_data[pin - ARC_AIO_MIN] = NULL;
        break;
    default:
        ERR_PRINT("unsupported aio message type %u\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error(msg, error_code);
        return;
    }

    msg->data.aio.pin = pin;
    msg->data.aio.value = reply_value;
    ipm_send_msg(msg);
}
