/*
 * Copyright 2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file
 * @brief UART-driven uploader
 *
 *  Reads a program from the uart using Intel HEX format.
 *
 * Designed to be used from Javascript or a ECMAScript object file.
 *
 * Hooks into the printk and fputc (for printf) modules. Poll driven.
 */

#include <nanokernel.h>

#include <arch/cpu.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <ctype.h>

#include <device.h>
#include <init.h>

#include <board.h>
#include <uart.h>
#include <toolchain.h>
#include <sections.h>
#include <atomic.h>

#include <misc/printk.h>

#include <fs.h>
#include "jerry-code.h"

#include "comms-uart.h"
#include "comms-shell.h"
#include "shell-state.h"

#include "ihex/kk_ihex_read.h"

#define FIVE_SECONDS    (5 * sys_clock_ticks_per_sec)
#define TEN_SECONDS     (10 * sys_clock_ticks_per_sec)

#ifndef CONFIG_IHEX_DEBUG
#define DBG(...) { ; }
#else
#define DBG printk
#endif /* CONFIG_IHEX_DEBUG */

extern void __stdout_hook_install(int(*fn)(int));

static const char banner[] = "Zephyr.js DEV MODE " __DATE__ " " __TIME__ "\r\n";

const char WRONG_TERMINAL_WARNING[] = "\n" \
    "Warning: The JavaScript terminal is in a different interface.\n" \
    "Examples:\n" \
    "\tMac   /dev/cu.usbmodem\n" \
    "\tLinux /dev/ttyACM0\n";

const char filename[] = "jerry.js";

// Jerryscript in green color

const char system_prompt[] = ANSI_FG_GREEN "shell> " ANSI_FG_RESTORE;
const char *system_get_prompt()
{
    return system_prompt;
}

#define MAX_LINE_LEN 80
#define FIFO_CACHE 2

/* Configuration of the callbacks to be called */
static struct comms_cfg_data comms_config = {
    /* Callback to be notified on connection status change */
    .cb_status = NULL,
    .interface = {
        .init_cb = NULL,
        .close_cb = NULL,
        .process_cb = NULL,
        .error_cb = NULL,
        .is_done = NULL
    },
    .print_state = NULL
};

struct comms_input
{
    int _unused;
    char line[MAX_LINE_LEN + 1];
};

static struct k_fifo avail_queue;
static struct k_fifo data_queue;
static bool uart_process_done = false;
static uint8_t fifo_size = 0;
static uint8_t max_fifo_size = 0;

atomic_t data_queue_count = 0;

uint32_t alloc_count = 0;
uint32_t free_count = 0;

static struct comms_input *isr_data = NULL;
static uint32_t tail = 0;
static char *buf;

struct comms_input *fifo_get_isr_buffer()
{
    void *data = k_fifo_get(&avail_queue, K_NO_WAIT);
    if (!data) {
        data = (void *)malloc(sizeof(struct comms_input));
        memset(data, '*', sizeof(struct comms_input));
        alloc_count++;
        fifo_size++;
        if (fifo_size > max_fifo_size)
            max_fifo_size = fifo_size;
    }
    return (struct comms_input *) data;
}

void fifo_cache_clear()
{
    while(fifo_size > 0) {
        void *data = k_fifo_get(&avail_queue, K_NO_WAIT);
        if (!data)
            return;

        free(data);
        fifo_size--;
        free_count++;
    }
    tail = 0;
    buf = NULL;
    isr_data = NULL;
}

void fifo_recycle_buffer(struct comms_input *data)
{
    if (fifo_size > 1) {
        free(data);
        fifo_size--;
        free_count++;
        return;
    }
    k_fifo_put(&avail_queue, data);
}

void comms_clear(void)
{
    void *data = NULL;
    do {
        if (data != NULL)
            free(data);
        data = k_fifo_get(&avail_queue, K_NO_WAIT);
    } while (data);

    do {
        if (data != NULL)
            free(data);
        data = k_fifo_get(&data_queue, K_NO_WAIT);
    } while (data);
}

/**************************** UART CAPTURE **********************************/

static struct device *dev_upload;

static volatile bool data_transmitted;

uint32_t bytes_received = 0;
uint32_t bytes_processed = 0;

atomic_t uart_state = 0;

enum
{
    UART_INIT,
    UART_TX_READY,
    UART_IRQ_UPDATE,
    UART_FIFO_WAIT,
    UART_RX_READY,
    UART_FIFO_READ,
    UART_FIFO_READ_END,
    UART_FIFO_READ_FLUSH,
    UART_FIFO_DATA_PROCESS,
    UART_RESET_HEAD,
    UART_POST_RESET,
    UART_ISR_END,
    UART_TASK_DATA_CAPTURE,
    UART_PROCESS_ENDED,
    UART_RESET_TAIL,
    UART_BUFFER_OVERFLOW,
    UART_BUFFER_PROCESS_OVERFLOW,
    UART_WAITING,
    UART_TIMEOUT,
    UART_CLOSE,
    UART_TERMINATED
};

int process_state = 0;

static void comms_interrupt_handler(struct device *dev)
{
    char byte;

    uint32_t bytes_read = 0;
    uint32_t len = 0;

    atomic_set(&uart_state, UART_IRQ_UPDATE);

    while (uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            atomic_set(&uart_state, UART_RX_READY);

            /* We allocate a new buffer everytime we don't have a tail
            * the buffer might be recycled or not from a previous run.
            */
            if (tail == 0) {
                DBG("[New]\n");
                isr_data = fifo_get_isr_buffer();
                buf = isr_data->line;
            }

            /* Read only until the end of the buffer
            * before i was using a ring buffer but was making things really
            * complicated for process side.
            */
            len = MAX_LINE_LEN - tail;
            bytes_read = uart_fifo_read(dev_upload, buf, len);
            bytes_received += bytes_read;
            tail += bytes_read;

            for (uint32_t t = 0; t<bytes_read; t++) {
                printk("%c",buf[t]);
            }

            /* We don't want to flush data too fast otherwise we would be allocating
            * but we want to flush as soon as we have processed the data on the task
            * so we don't queue too much and delay the system response.
            *
            * When the process has finished dealing with the data it signals this method
            * with a 'i am ready to continue' by changing uart_process_done.
            *
            * It is also imperative to flush when we reach the limit of the buffer.
            *
            * If we are still fine in the cache limits, then we keep flushing every
            * time we get a byte.
            */
            bool flush = false;

            if (fifo_size == 1 ||
                tail == MAX_LINE_LEN ||
                uart_process_done) {
                flush = true;
                uart_process_done = false;
            } else {
                /* Check for line ends, to flush the data. The decoder / shell will probably
                 * sit for a bit in the data so it is better if we finish this buffer and send it.
                 */
                atomic_set(&uart_state, UART_FIFO_READ);

                while (bytes_read-- > 0) {
                    byte = *buf++;
                    if (byte == '\r' || byte == '\n' ||
                        (byte >= CTRL_START && byte <= CTRL_END)) {
                        flush = true;
                        break;
                    }
                }
            }

            atomic_set(&uart_state, UART_FIFO_READ_END);

            /* Happy to flush the data into the queue for processing */
            if (flush) {
                isr_data->line[tail] = 0;
                tail = 0;
                atomic_inc(&data_queue_count);
                k_fifo_put(&data_queue, isr_data);
                isr_data = NULL;
            }
        }

        if (uart_irq_tx_ready(dev)) {
            atomic_set(&uart_state, UART_TX_READY);
            data_transmitted = true;
        }
    }

    atomic_set(&uart_state, UART_ISR_END);
}

/*************************** ACM OUTPUT *******************************/
/**
* @brief Output one character to UART ACM
*
* @param c Character to output
* @return success
*/

static int comms_out(int c)
{
    comms_writec((char)c);
    return 1;
}

/*
* @brief Writes data into the uart and flushes it.
*
* @param buf Buffer to write
* @param len length of buffer
*
* @todo Really dislike this wait here from the example
* will probably rewrite it later with a line queue
*/

void comms_write_buf(const char *buf, int len)
{
    if (len == 0)
        return;

    struct device *dev = dev_upload;
    uart_irq_tx_enable(dev);

    data_transmitted = false;
    uart_fifo_fill(dev, buf, len);

    while (data_transmitted == false);

    uart_irq_tx_disable(dev);
}

void comms_writec(char byte)
{
    comms_write_buf(&byte, 1);
}

void comms_print(const char *buf)
{
    comms_write_buf(buf, strnlen(buf, MAX_LINE_LEN * 4));
}

void comms_println(const char *buf)
{
    comms_write_buf(buf, strnlen(buf, MAX_LINE_LEN));
    comms_write_buf("\r\n", 2);
}

/**
* Provide console message implementation for the engine.
*/
void comms_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}

#ifdef CONFIG_UART_LINE_CTRL
uint32_t comms_get_baudrate(void)
{
    uint32_t baudrate;

    int ret = uart_line_ctrl_get(dev_upload, LINE_CTRL_BAUD_RATE, &baudrate);
    if (ret)
        printk("Fail baudrate %d\n", ret);
    else
        printk("Baudrate %d\n", (int)baudrate);

    return baudrate;
}
#endif

void comms_print_status()
{
    printk("******* SYSTEM STATE ********\n");
    if (atomic_get(&uart_state) == UART_INIT)
        printk(ANSI_FG_RED "JavaScript terminal not connected\n" ANSI_FG_RESTORE);

    if (comms_config.print_state != NULL)
        comms_config.print_state();

#ifdef CONFIG_UART_LINE_CTRL
    comms_get_baudrate();
#endif

    if (!data_transmitted)
        printk("[Data TX]\n");

    printk("[State] %d\n", (int)uart_state);
    printk("[Process State] %d\n", (int)process_state);

    printk("[Mem] Fifo %d Max Fifo %d Alloc %d Free %d \n",
        (int)fifo_size, (int)max_fifo_size, (int)alloc_count, (int)free_count);
    printk("[Usage] Max fifo usage %d bytes\n", (int)(max_fifo_size * sizeof(struct comms_input)));
    printk("[Queue size] %d\n", (int)data_queue_count);
    printk("[Data] Received %d Processed %d \n",
        (int)bytes_received, (int)bytes_processed);
}

void comms_runner()
{
    static struct comms_input *data = NULL;
    char *buf = NULL;
    uint32_t len = 0;

    DBG("[Listening]\n");
    __stdout_hook_install(comms_out);

    // Disable buffering on stdout since some parts write directly to uart fifo
    setbuf(stdout, NULL);

    ashell_help("");

    process_state = 0;

    while (1) {
        atomic_set(&uart_state, UART_INIT);
        if (comms_config.interface.init_cb != NULL) {
            DBG("[Init]\n");
            comms_config.interface.init_cb();
        }

        while (!comms_config.interface.is_done()) {
            atomic_set(&uart_state, UART_WAITING);
            process_state = 10;

            while (data == NULL) {
                DBG("[Wait]\n");
                data = k_fifo_get(&data_queue, FIVE_SECONDS);
                if (data) {
                    atomic_dec(&data_queue_count);
                    buf = data->line;
                    len = strnlen(buf, MAX_LINE_LEN);

                    DBG("[Data]\n");
                    DBG("%s\n", buf);
                } else {
                    /* We clear the cache memory if there are no data transactions */
                    if (tail == 0) {
                        fifo_cache_clear();
                    } else {
                        /* Wait for a timeout and flush data if there was not a carriage return */
                        if (atomic_get(&uart_state) == UART_ISR_END && isr_data != NULL) {
                            DBG("Capturing buffer\n");
                            process_state = 20;
                            isr_data->line[tail] = 0;
                            tail = 0;
                            data = isr_data;
                            buf = data->line;
                            isr_data = NULL;
                            atomic_set(&uart_state, UART_TASK_DATA_CAPTURE);
                        }
                    }
                }
            }

            process_state = 30;
            uint32_t processed = comms_config.interface.process_cb(buf, len);

            bytes_processed += processed;

            process_state = 40;
            if (comms_config.interface.is_done()) {
                len -= processed;
                // Moving the remaining data to be processed by the new callback
                memmove(buf, buf + processed, len);
                buf[len] = '\0';
                DBG("New buf [%s]\n", buf);
            } else {
                uart_process_done = true;
                DBG("[Recycle]\n");
                fifo_recycle_buffer(data);
                data = NULL;
            }

        }

        process_state = 50;
        atomic_set(&uart_state, UART_CLOSE);
        if (comms_config.interface.close_cb != NULL)
            comms_config.interface.close_cb();
    }

    // Not possible
    atomic_set(&uart_state, UART_TERMINATED);
}

/**
 * ACM TASK
 * Task running the shell in the ACM Uart port
 */

#ifndef ASHELL_MAIN
void main_development_shell(void);
#endif

void acm()
{
    ashell_process_start();

    printk(banner);

#ifndef ASHELL_MAIN
    printk(WRONG_TERMINAL_WARNING);
    main_development_shell();
#endif

    dev_upload = device_get_binding(CONFIG_CDC_ACM_PORT_NAME);

    if (!dev_upload) {
        printf("CDC [%s] ACM device not found\n", CONFIG_CDC_ACM_PORT_NAME);
        return;
    }

    k_fifo_init(&data_queue);
    k_fifo_init(&avail_queue);

#ifdef CONFIG_UART_LINE_CTRL
    uint32_t dtr = 0;
    int ret;

    while (1) {
        uart_line_ctrl_get(dev_upload, LINE_CTRL_DTR, &dtr);
        if (dtr)
            break;
    }

    /* They are optional, we use them to test the interrupt endpoint */
    ret = uart_line_ctrl_set(dev_upload, LINE_CTRL_DCD, 1);
    if (ret)
        printf("DCD Failed %d\n", ret);

    ret = uart_line_ctrl_set(dev_upload, LINE_CTRL_DSR, 1);
    if (ret)
        printf("DSR Failed %d\n", ret);

    /* 1000 msec = 1 sec */
    k_sleep(1000);

#endif

    uart_irq_rx_disable(dev_upload);
    uart_irq_tx_disable(dev_upload);

    uart_irq_callback_set(dev_upload, comms_interrupt_handler);
    comms_write_buf(banner, sizeof(banner));

    /* Enable rx interrupts */
    uart_irq_rx_enable(dev_upload);

    comms_runner();
}

/**************************** DEVICE **********************************/

void comms_uart_set_config(struct comms_cfg_data *config)
{
    memcpy(&comms_config, config, sizeof(struct comms_cfg_data));
}
