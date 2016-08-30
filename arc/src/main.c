// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>

#include <device.h>
#include <init.h>
#include <adc.h>
#include <i2c.h>

#include "zjs_common.h"
#include "zjs_ipm.h"

/* specify delay between greetings (in ms); compute equivalent in ticks */

#define SLEEPTIME  1100
#define SLEEPTICKS (SLEEPTIME * sys_clock_ticks_per_sec / 800)

#define ADC_DEVICE_NAME "ADC_0"
#define MAX_I2C_BUS 1
#define BUFFER_SIZE 4

static struct device* adc_dev;
static uint32_t pin_values[ARC_AIO_LEN] = {};
static uint32_t pin_send_updates[ARC_AIO_LEN] = {};

static uint8_t seq_buffer[BUFFER_SIZE];

/* I2C variables */
static struct device *i2c_device[MAX_I2C_BUS];
static struct zjs_i2c_ipm_message *i2cMsg;
static bool dont_sleep = false;

int ipm_send_msg(uint32_t id, bool block, uint32_t type, uint32_t pin, uint32_t value) {
    if (id == MSG_ID_AIO) {
        struct zjs_ipm_message msg;
        msg.block = block;
        msg.type = type;
        msg.pin = pin;
        msg.value = value;
        return zjs_ipm_send(id, &msg, sizeof(msg));
    } else if (id == MSG_ID_I2C) {
        struct zjs_i2c_ipm_message_reply msg;
        msg.block = block;
        msg.type = type;
        return zjs_ipm_send(id, &msg, sizeof(msg));
    }
    // Invalid message id
    return -1;
}

uint32_t handle_i2c_message(struct zjs_i2c_ipm_message *msg) {
    uint32_t reply_type = 0;

    if (msg->type == TYPE_I2C_OPEN) {
        if (msg->bus < MAX_I2C_BUS) {
            char bus[6];
            snprintf(bus, 6, "I2C_%i", msg->bus);
            i2c_device[msg->bus] = device_get_binding(bus);

            if (!i2c_device[msg->bus]) {
                PRINT("I2C bus %s not found.\n", bus);
                reply_type = TYPE_I2C_OPEN_FAIL;
            } else {
                /* TODO remove these hard coded numbers
                 * once the config API is made */
                union dev_config cfg;
                cfg.raw = 0;
                cfg.bits.use_10_bit_addr = 0;
                cfg.bits.speed = I2C_SPEED_STANDARD;
                cfg.bits.is_master_device = 1;

                if (i2c_configure(i2c_device[msg->bus], cfg.raw) != 0) {
                    PRINT("I2C bus %s configure failed.\n", bus);
                    reply_type = TYPE_I2C_OPEN_FAIL;
                } else {
                    // Skip the pin sleep; slows I2C transmission greatly
                    dont_sleep = true;
                    reply_type = TYPE_I2C_OPEN_SUCCESS;
                }
            }
        } else {
            PRINT("I2C bus I2C_%s is not a valid I2C bus.\n", msg->bus);
            reply_type = TYPE_I2C_OPEN_FAIL;
        }
    } else if (msg->type == TYPE_I2C_WRITE) {
        if (msg->bus < MAX_I2C_BUS) {
            // Write has to come after an Open I2C message
            if (i2c_device[msg->bus]) {
                if (i2c_write(i2c_device[msg->bus], msg->data, msg->length, msg->address) != 0) {
                    PRINT("i2c_write failed!\n");
                    reply_type =  TYPE_I2C_WRITE_FAIL;
                } else {
                    reply_type =  TYPE_I2C_WRITE_SUCCESS;
                }
            }
            else {
                PRINT("No I2C device is ready yet\n");
                reply_type =  TYPE_I2C_WRITE_FAIL;
            }
        }
    } else if (msg->type == TYPE_I2C_WRITE_BIT) {
        PRINT("RECEIVED TYPE_I2C_WRITE_BIT\n");
        reply_type =  TYPE_I2C_WRITE_BIT_SUCCESS;
    } else if (msg->type == TYPE_I2C_READ) {
        PRINT("RECEIVED TYPE_I2C_READ\n");
        reply_type = TYPE_I2C_READ_SUCCESS;
    } else if (msg->type == TYPE_I2C_TRANSFER) {
        PRINT("RECEIVED TYPE_I2C_TRANSFER\n");
        reply_type = TYPE_I2C_TRANSFER_SUCCESS;
    } else {
        PRINT("I2C - Unsupported message type %d\n", msg->type);
    }

    return ipm_send_msg(MSG_ID_I2C, msg->block, reply_type, 0, 0);
}

uint32_t pin_read(uint8_t pin)
{
    uint8_t* buf = seq_buffer;

    struct adc_seq_entry entry = {
        .sampling_delay = 12,
        .channel_id = pin,
        .buffer = seq_buffer,
        .buffer_length = BUFFER_SIZE,
    };

    struct adc_seq_table entry_table = {
        .entries = &entry,
        .num_entries = 1,
    };

    if (!adc_dev) {
       PRINT("ARC - ADC device not found\n");
       return 0;
    }

    if (adc_read(adc_dev, &entry_table) != 0) {
        PRINT("ARC - couldn't read from pin %d\n", pin);
        return 0;
    }

    uint32_t raw_value = *((uint32_t *) buf);
    return raw_value;
}

void ipm_msg_receive_callback(void *context, uint32_t id, volatile void *data)
{
    uint32_t reply_type = 0;
    uint32_t reply_value = 0;

    if (id == MSG_ID_AIO) {
        struct zjs_ipm_message *msg = (struct zjs_ipm_message*) data;
        uint32_t pin = msg->pin;

        bool invalid = pin < ARC_AIO_MIN || pin > ARC_AIO_MAX;

        if (msg->type == TYPE_AIO_OPEN) {
            if (invalid) {
                PRINT("ARC - pin #%d out of range\n", pin);
                reply_type = TYPE_AIO_OPEN_FAIL;
            } else {
                reply_type = TYPE_AIO_OPEN_SUCCESS;
            }
        } else if (msg->type == TYPE_AIO_PIN_READ) {
            if (invalid) {
                PRINT("ARC - pin #%d out of range\n", pin);
                reply_type = TYPE_AIO_PIN_READ_FAIL;
            }

            reply_type = TYPE_AIO_PIN_READ_SUCCESS;
            // FIXME: inside the interrupt, cannot read from the ADC pins, only
            //   from the main loop thread
            reply_value = pin_values[pin - ARC_AIO_MIN];
        } else if (msg->type == TYPE_AIO_PIN_ABORT) {
            PRINT("ARC - AIO abort() not supported\n");
            reply_type = TYPE_AIO_PIN_ABORT_SUCCESS;
        } else if (msg->type == TYPE_AIO_PIN_CLOSE) {
            if (invalid) {
                PRINT("ARC - pin #%d out of range\n", pin);
                reply_type = TYPE_AIO_PIN_CLOSE_FAIL;
            } else {
                PRINT("ARC - AIO pin #%d is closed\n", pin);
                reply_type = TYPE_AIO_PIN_CLOSE_SUCCESS;
            }
        } else if (msg->type == TYPE_AIO_PIN_SUBSCRIBE) {
            if (invalid) {
                PRINT("ARC - pin #%d out of range\n", pin);
                reply_type = TYPE_AIO_PIN_SUBSCRIBE_FAIL;
            } else {
                pin_send_updates[pin - ARC_AIO_MIN] = 1;
                reply_type = TYPE_AIO_PIN_SUBSCRIBE_SUCCESS;
            }
        } else if (msg->type == TYPE_AIO_PIN_UNSUBSCRIBE) {
            if (invalid) {
                PRINT("ARC - pin #%d out of range\n", pin);
                reply_type = TYPE_AIO_PIN_UNSUBSCRIBE_FAIL;
            } else {
                pin_send_updates[pin - ARC_AIO_MIN] = 0;
                reply_type = TYPE_AIO_PIN_UNSUBSCRIBE_SUCCESS;
            }
        } else {
            PRINT("ARC - Unsupported message id %d\n", id);
        }

        ipm_send_msg(MSG_ID_AIO, msg->block, reply_type, pin, reply_value);
    } else if (id == MSG_ID_I2C) {
        if (i2cMsg) {
            // Currently we should be blocked until the current I2C message has
            // been handled
            PRINT("ARC - current i2c msg not sent, not ready for another\n");
        } else {
            // Save the the message pointer to handle it once the main loop
            // comes around
            i2cMsg = (struct zjs_i2c_ipm_message*) data;
        }
    }
}

#ifdef CONFIG_MICROKERNEL
void mainloop(void)
#else
void main(void)
#endif
{
    PRINT("ARC -------------------------------------- \n");
    PRINT("ARC - AIO processor from sensor core (ARC)!\n");

    zjs_ipm_init();
    zjs_ipm_register_callback(MSG_ID_AIO, ipm_msg_receive_callback);

    adc_dev = device_get_binding(ADC_DEVICE_NAME);
    adc_enable(adc_dev);

    while (1) {
        /*
         * mainloop just reads all the values from all ADC pins
         * and stores them in the array
         */

        // If we have an I2C message waiting to be sent, send it
        if (i2cMsg) {
            handle_i2c_message(i2cMsg);
            i2cMsg = NULL;
        }

        // If we are doing I2C skip checking pins
        if (!dont_sleep) {
            for (int i=0; i<=5; i++) {
                pin_values[i] = pin_read(ARC_AIO_MIN + i);
                if (pin_send_updates[i]) {
                    ipm_send_msg(MSG_ID_AIO, 0, TYPE_AIO_PIN_EVENT_VALUE_CHANGE,
                                 ARC_AIO_MIN + i, pin_values[i]);
                    task_sleep(10);
                }

                /* FIX BUG
                 * read value will read the old value
                 * from the previous pin when switching pins
                 * too fast, adding a sleep fix this issue
                 */
                task_sleep(1);
            }
            task_sleep(SLEEPTICKS);
        }
        else {
            // TODO: Figure out why we need this sleep for the main loop to work
            task_sleep(1);
        }
    }

    adc_disable(adc_dev);
}
