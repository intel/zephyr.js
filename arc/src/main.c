// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>

#include <string.h>
#include <device.h>
#include <init.h>
#include <adc.h>
#include <i2c.h>
#include <display/grove_lcd.h>

#include "zjs_common.h"
#include "zjs_ipm.h"

#define QUEUE_SIZE       10  // max incoming message can handle
#define SLEEP_TICKS       1  // 10ms sleep time in cpu ticks
#define UPDATE_INTERVAL 200  // 2sec interval in between notifications

// AIO
#define ADC_DEVICE_NAME "ADC_0"
#define ADC_BUFFER_SIZE 2

// I2C
#define MAX_I2C_BUS 1

// GROVE_LCD
#define MAX_BUFFER_SIZE 256

// IPM
static struct nano_sem arc_sem;
static struct zjs_ipm_message msg_queue[QUEUE_SIZE];
static struct zjs_ipm_message* end_of_queue_ptr = msg_queue + QUEUE_SIZE;

// AIO
static struct device* adc_dev;
static uint32_t pin_values[ARC_AIO_LEN] = {};
static uint32_t pin_last_values[ARC_AIO_LEN] = {};
static void *pin_user_data[ARC_AIO_LEN] = {};
static uint8_t pin_send_updates[ARC_AIO_LEN] = {};
static uint8_t seq_buffer[ADC_BUFFER_SIZE];

// I2C
static struct device *i2c_device[MAX_I2C_BUS];

// Grove_LCD
static struct device *glcd = NULL;
static char str[MAX_BUFFER_SIZE];

// add strnlen() support for security since it is missing
// in Zephyr's minimal libc implementation
size_t strnlen(const char *str, size_t max_len) {
    size_t len;
    for (len = 0; len < max_len; len++) {
        if (!*str)
            break;
        str++;
    }
    return len;
}

static int ipm_send_reply(struct zjs_ipm_message *msg) {
    return zjs_ipm_send(msg->id, msg);
}

static int ipm_send_error_reply(struct zjs_ipm_message *msg, uint32_t error_code) {
    msg->flags |= MSG_ERROR_FLAG;
    msg->error_code = error_code;
    PRINT("send error %lu\n", msg->error_code);
    return zjs_ipm_send(msg->id, msg);
}

static int ipm_send_updates(struct zjs_ipm_message *msg) {
    return zjs_ipm_send(msg->id, msg);
}

static uint32_t pin_read(uint8_t pin)
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
       PRINT("ADC device not found\n");
       return 0;
    }

    if (adc_read(adc_dev, &entry_table) != 0) {
        PRINT("couldn't read from pin %d\n", pin);
        return 0;
    }

    // read from buffer, not sure if byte order is important
    uint32_t raw_value = (uint32_t) seq_buffer[0]
                       | (uint32_t) seq_buffer[1] << 8;

    return raw_value;
}

static void queue_message(struct zjs_ipm_message* incoming_msg)
{
    struct zjs_ipm_message* msg = msg_queue;

    if (!incoming_msg) {
        return;
    }

    nano_isr_sem_take(&arc_sem, TICKS_UNLIMITED);
    while(msg && msg < end_of_queue_ptr) {
       if (msg->id == MSG_ID_DONE) {
           break;
       }
       msg++;
    }

    if (msg != end_of_queue_ptr) {
        // copy the message into our queue to be process in the mainloop
        memcpy(msg, incoming_msg, sizeof(struct zjs_ipm_message));
    } else {
        // running out of spaces, disgard message
        PRINT("skipping incoming message\n");
    }
    nano_isr_sem_give(&arc_sem);
}

static void ipm_msg_receive_callback(void *context, uint32_t id, volatile void *data)
{
    struct zjs_ipm_message *incoming_msg = (struct zjs_ipm_message*)(*(uintptr_t *)data);
    if (incoming_msg) {
        queue_message(incoming_msg);
    } else {
        PRINT("error: message is NULL\n");
    }
}

static void handle_aio(struct zjs_ipm_message* msg)
{
    uint32_t pin = msg->data.aio.pin;
    uint32_t reply_value = 0;
    uint32_t error_code = ERROR_IPM_NONE;

    if (pin < ARC_AIO_MIN || pin > ARC_AIO_MAX) {
        PRINT("pin #%lu out of range\n", pin);
        ipm_send_error_reply(msg, ERROR_IPM_INVALID_PARAMETER);
        return;
    }

    switch(msg->type) {
    case TYPE_AIO_OPEN:
        // NO OP - always success
        break;
    case TYPE_AIO_PIN_READ:
        reply_value = pin_read(pin);
        break;
    case TYPE_AIO_PIN_ABORT:
        // NO OP - always success
        break;
    case TYPE_AIO_PIN_CLOSE:
        // NO OP - always success
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
        PRINT("unsupported aio message type %lu\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error_reply(msg, error_code);
        return;
    }

    msg->data.aio.pin = pin;
    msg->data.aio.value = reply_value;
    ipm_send_reply(msg);
}

static void handle_i2c(struct zjs_ipm_message* msg)
{
    uint32_t error_code = ERROR_IPM_NONE;
    uint8_t msg_bus = msg->data.i2c.bus;

    switch(msg->type) {
    case TYPE_I2C_OPEN:
        if (msg_bus < MAX_I2C_BUS) {
            char bus[6];
            snprintf(bus, 6, "I2C_%i", msg_bus);
            i2c_device[msg_bus] = device_get_binding(bus);

            if (!i2c_device[msg_bus]) {
                PRINT("I2C bus %s not found.\n", bus);
                error_code = ERROR_IPM_OPERATION_FAILED;
            } else {
                /* TODO remove these hard coded numbers
                 * once the config API is made */
                union dev_config cfg;
                cfg.raw = 0;
                cfg.bits.use_10_bit_addr = 0;
                cfg.bits.speed = I2C_SPEED_STANDARD;
                cfg.bits.is_master_device = 1;

                if (i2c_configure(i2c_device[msg_bus], cfg.raw) != 0) {
                    PRINT("I2C bus %s configure failed.\n", bus);
                    error_code = ERROR_IPM_OPERATION_FAILED;
                }
            }
        } else {
            PRINT("I2C bus I2C_%s is not a valid I2C bus.\n", msg_bus);
            error_code = ERROR_IPM_OPERATION_FAILED;
        }
        break;
    case TYPE_I2C_WRITE:
        if (msg_bus < MAX_I2C_BUS) {
            // Write has to come after an Open I2C message
            if (i2c_device[msg_bus]) {
                if (i2c_write(i2c_device[msg_bus],
                              msg->data.i2c.data,
                              msg->data.i2c.length,
                              msg->data.i2c.address) != 0) {
                    PRINT("i2c_write failed!\n");
                    error_code = ERROR_IPM_OPERATION_FAILED;
                }
            }
            else {
                PRINT("no I2C device is ready yet\n");
                error_code = ERROR_IPM_OPERATION_FAILED;
            }
        }
        break;
    case TYPE_I2C_WRITE_BIT:
        PRINT("received TYPE_I2C_WRITE_BIT\n");
        break;
    case TYPE_I2C_READ:
        if (msg_bus < MAX_I2C_BUS) {
            // Read has to come after an Open I2C message
            if (i2c_device[msg_bus]) {

                int reply = i2c_read(i2c_device[msg_bus],
                                     msg->data.i2c.data,
                                     msg->data.i2c.length,
                                     msg->data.i2c.address);

                if (reply < 0) {
                    error_code = ERROR_IPM_OPERATION_FAILED;
                }
            }
            else {
                PRINT("No I2C device is ready yet\n");
                error_code = ERROR_IPM_OPERATION_FAILED;
            }
        }
        break;
    case TYPE_I2C_BURST_READ:
        if (msg_bus < MAX_I2C_BUS) {
            // Burst read has to come after an Open I2C message
            if (i2c_device[msg_bus]) {
               int reply = i2c_burst_read(i2c_device[msg_bus],
                                          msg->data.i2c.address,
                                          msg->data.i2c.register_addr,
                                          msg->data.i2c.data,
                                          msg->data.i2c.length);

                if (reply < 0) {
                    error_code = ERROR_IPM_OPERATION_FAILED;
                }
            }
            else {
                PRINT("No I2C device is ready yet\n");
                error_code = ERROR_IPM_OPERATION_FAILED;
            }
        }
        break;
    case TYPE_I2C_TRANSFER:
        PRINT("received TYPE_I2C_TRANSFER\n");
        break;

    default:
        PRINT("unsupported i2c message type %lu\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error_reply(msg, error_code);
        return;
    }

    ipm_send_reply(msg);
}

static void handle_glcd(struct zjs_ipm_message* msg)
{
    char *buffer;
    uint8_t r, g, b;
    uint32_t error_code = ERROR_IPM_NONE;

    if (msg->type != TYPE_GLCD_INIT && !glcd) {
        PRINT("Grove LCD device not found.\n");
        ipm_send_error_reply(msg, ERROR_IPM_OPERATION_FAILED);
        return;
    }

    switch(msg->type) {
    case TYPE_GLCD_INIT:
        if (!glcd) {
            /* Initialize the Grove LCD */
            glcd = device_get_binding(GROVE_LCD_NAME);

            if (!glcd) {
                error_code = ERROR_IPM_OPERATION_FAILED;
            } else {
                DBG_PRINT("Grove LCD initialized\n");
            }
        }
        break;
    case TYPE_GLCD_PRINT:
        /* print to LCD screen */
        buffer = msg->data.glcd.buffer;
        if (!buffer) {
            error_code = ERROR_IPM_INVALID_PARAMETER;
            PRINT("buffer not found\n");
        } else {
            snprintf(str, MAX_BUFFER_SIZE, "%s", buffer);
            glcd_print(glcd, str, strnlen(str, MAX_BUFFER_SIZE));
            DBG_PRINT("Grove LCD print: %s\n", str);
        }
        break;
    case TYPE_GLCD_CLEAR:
        glcd_clear(glcd);
        break;
    case TYPE_GLCD_SET_CURSOR_POS:
        glcd_cursor_pos_set(glcd, msg->data.glcd.col, msg->data.glcd.row);
        break;
    case TYPE_GLCD_SET_COLOR:
        r = msg->data.glcd.color_r;
        g = msg->data.glcd.color_g;
        b = msg->data.glcd.color_b;
        glcd_color_set(glcd, r, g, b);
        break;
    case TYPE_GLCD_SELECT_COLOR:
        glcd_color_select(glcd, msg->data.glcd.value);
        break;
    case TYPE_GLCD_SET_FUNCTION:
        glcd_function_set(glcd, msg->data.glcd.value);
        break;
    case TYPE_GLCD_GET_FUNCTION:
        msg->data.glcd.value = glcd_function_get(glcd);
        break;
    case TYPE_GLCD_SET_DISPLAY_STATE:
        glcd_display_state_set(glcd, msg->data.glcd.value);
        break;
    case TYPE_GLCD_GET_DISPLAY_STATE:
        msg->data.glcd.value = glcd_display_state_get(glcd);
        break;
    case TYPE_GLCD_SET_INPUT_STATE:
        glcd_input_state_set(glcd, msg->data.glcd.value);
        break;
    case TYPE_GLCD_GET_INPUT_STATE:
        msg->data.glcd.value = glcd_input_state_get(glcd);
        break;
    break;

    default:
        PRINT("unsupported grove lcd message type %lu\n", msg->type);
        error_code = ERROR_IPM_NOT_SUPPORTED;
    }

    if (error_code != ERROR_IPM_NONE) {
        ipm_send_error_reply(msg, error_code);
        return;
    }

    ipm_send_reply(msg);
}

static void process_messages()
{
    struct zjs_ipm_message* msg = msg_queue;

    while (msg && msg < end_of_queue_ptr) {
        // loop through all messages and process them
        if (msg->id != MSG_ID_DONE) {
            if (msg->id == MSG_ID_AIO) {
                handle_aio(msg);
            } else if (msg->id == MSG_ID_I2C) {
                handle_i2c(msg);
            } else if (msg->id == MSG_ID_GLCD) {
                handle_glcd(msg);
            } else {
                PRINT("unsupported ipm message id: %lu\n", msg->id);
                ipm_send_error_reply(msg, ERROR_IPM_NOT_SUPPORTED);
            }

            // done processing, marking it done
            msg->id = MSG_ID_DONE;
       } else {
            // no more messages
            break;
       }
       msg++;
    }
}

static void process_aio_updates()
{
    for (int i=0; i<=5; i++) {
        if (pin_send_updates[i]) {
            pin_values[i] = pin_read(ARC_AIO_MIN + i);
            if (pin_values[i] != pin_last_values[i]) {
                // send updates only if value has changed
                // so it doesn't flood the IPM channel
                struct zjs_ipm_message msg;
                msg.id = MSG_ID_AIO;
                msg.type = TYPE_AIO_PIN_EVENT_VALUE_CHANGE;
                msg.flags = 0;
                msg.user_data = pin_user_data[i];
                msg.data.aio.pin = ARC_AIO_MIN+i;
                msg.data.aio.value = pin_values[i];
                ipm_send_updates(&msg);
            }
            pin_last_values[i] = pin_values[i];
        }
    }
}

void main(void)
{
    PRINT("Sensor core running ZJS ARC support image\n");

    nano_sem_init(&arc_sem);
    nano_sem_give(&arc_sem);

    memset(msg_queue, 0, sizeof(struct zjs_ipm_message) * QUEUE_SIZE);

    zjs_ipm_init();
    zjs_ipm_register_callback(-1, ipm_msg_receive_callback); // MSG_ID ignored

    adc_dev = device_get_binding(ADC_DEVICE_NAME);
    adc_enable(adc_dev);

    int tick_count = 0;
    while (1) {
        process_messages();

        tick_count += SLEEP_TICKS;
        if (tick_count >= UPDATE_INTERVAL) {
            process_aio_updates();
            tick_count = 0;
        }

        task_sleep(SLEEP_TICKS);
    }

    adc_disable(adc_dev);
}
