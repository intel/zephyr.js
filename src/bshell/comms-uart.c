// Copyright (c) 2016-2017, Intel Corporation.

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

#include <arch/cpu.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
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

#ifndef CONFIG_USB_CDC_ACM
#include "webusb.h"
#endif

#include "comms.h"

#include "../zjs_util.h"

// TODO: remove max line limit / implement segmentation/reassembly protocol
#define MAX_LINE_LEN 90

extern void webusb_register_handlers();

extern void __stdout_hook_install(int(*fn)(int));

extern void ashell_init();
extern void ashell_process(char *buf, size_t len);

/**************************** UART CAPTURE **********************************/

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


struct comms_input
{
    int _unused;
    char line[MAX_LINE_LEN + 1];
};

static struct uart_context {
    struct device *dev;

    atomic_t state;
    volatile bool data_transmitted;

    struct comms_input *isr_data;
    uint8_t fifo_size;
    uint8_t max_fifo;
    uint32_t tail;
    char *buf;

    struct k_fifo avail_queue;
    struct k_fifo data_queue;
} uartc = {
    .dev = NULL,
    .state = 0,
    .data_transmitted = false,
    .isr_data = NULL,
    .fifo_size = 0,
    .max_fifo = 0,
    .tail = 0,
    .buf = NULL
};

/**************************** FIFO HANDLING **********************************/

struct comms_input *fifo_get_isr_buffer()
{
    void *data = k_fifo_get(&uartc.avail_queue, K_NO_WAIT);
    if (!data) {
        data = (void *)zjs_malloc(sizeof(struct comms_input));
        memset(data, '*', sizeof(struct comms_input));
        uartc.fifo_size++;
        if (uartc.fifo_size > uartc.max_fifo)
            uartc.max_fifo = uartc.fifo_size;
    }
    return (struct comms_input *) data;
}

void fifo_cache_clear()
{
    while(uartc.fifo_size > 0) {
        void *data = k_fifo_get(&uartc.avail_queue, K_NO_WAIT);
        if (!data)
            return;

        zjs_free(data);
        uartc.fifo_size--;
    }
    uartc.tail = 0;
    uartc.buf = NULL;
    uartc.isr_data = NULL;
}

void fifo_recycle_buffer(struct comms_input *data)
{
    if (uartc.fifo_size > 1) {
        zjs_free(data);
        uartc.fifo_size--;
        return;
    }
    k_fifo_put(&uartc.avail_queue, data);
}

/*************************** ACM OUTPUT *******************************/

/*
 * @brief Writes data into the uart and flushes it.
 * @param buf Buffer to write, not necessarily null-terminated.
 * @param len length of buffer
 */
void uart_send(const char *buf, size_t len)
{
    if (len <= 0)
        return;

    uart_irq_tx_enable(uartc.dev);
    uartc.data_transmitted = false;

    // uart_fifo_fill(uartc.dev, buf, len);
    int bytes = 0;
    while (bytes != len) {
        buf += bytes;
        len -= bytes;
        bytes = uart_fifo_fill(uartc.dev, (const uint8_t *)buf, len);
    }

    while (uartc.data_transmitted == false) {
        //k_sleep(100);
    }

    uart_irq_tx_disable(uartc.dev);
}

/**
 * @brief Output one character to UART ACM
 * @param i Character to output
 * @return success
 */
int uart_printch(int i)
{
    char c = (char) i;
    uart_send(&c, 1);
    return 1;
}

/*
 * @brief Writes null terminated string to UART and flushes it.
 * @param buf Buffer to write, null-terminated.
 */
void uart_print(const char *buf)
{
    uart_send(buf, strnlen(buf, MAX_LINE_LEN));
}

/**
 * @brief Provide console message implementation for the engine.
 * Uses the stdout hook based on uart_printch().
 */
void uart_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}

void uart_error(const char *format, ...)
{
    va_list args;
    uart_print("[Error] ");
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}

/**************************** UART INTERRUPT *********************************/

static void comms_interrupt_handler(struct device *dev)
{
    #define CTRL_START           0x00
    #define CTRL_END             0x1F

    char byte;

    uint32_t bytes_read = 0;
    uint32_t len = 0;

    atomic_set(&uartc.state, UART_IRQ_UPDATE);

    while (uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            atomic_set(&uartc.state, UART_RX_READY);

            /* We allocate a new buffer everytime we don't have a tail
            * the buffer might be recycled or not from a previous run.
            */
            if (uartc.tail == 0) {
                // DBG("[New]\n");
                uartc.isr_data = fifo_get_isr_buffer();
                uartc.buf = uartc.isr_data->line;
            }

            /* Read only until the end of the buffer
            * before i was using a ring buffer but was making things really
            * complicated for process side.
            */
            len = MAX_LINE_LEN - uartc.tail;
            bytes_read = uart_fifo_read(uartc.dev, uartc.buf, len);
            uartc.tail += bytes_read;

            /* We don't want to flush data too fast otherwise we would be allocating
            * but we want to flush as soon as we have processed the data on the task
            * so we don't queue too much and delay the system response.
            *
            * It is imperative to flush when we reach the limit of the buffer.
            *
            * If we are still fine in the cache limits, then we keep flushing every
            * time we get a byte.
            */
            bool flush = false;

            if (uartc.fifo_size == 1 || uartc.tail == MAX_LINE_LEN) {
                flush = true;
            } else {
                /* Check for line ends, to flush the data. The decoder / shell will probably
                 * sit for a bit in the data so it is better if we finish this buffer and send it.
                 */
                atomic_set(&uartc.state, UART_FIFO_READ);

                while (bytes_read-- > 0) {
                    byte = *(uartc.buf)++;
                    if (byte == '\r' || byte == '\n' ||
                        (byte >= CTRL_START && byte <= CTRL_END)) {
                        flush = true;
                        break;
                    }
                }
            }

            atomic_set(&uartc.state, UART_FIFO_READ_END);

            /* Happy to flush the data into the queue for processing */
            if (flush) {
                uartc.isr_data->line[uartc.tail] = 0;
                uartc.tail = 0;
                k_fifo_put(&uartc.data_queue, uartc.isr_data);
                uartc.isr_data = NULL;
            }
        }

        if (uart_irq_tx_ready(dev)) {
            atomic_set(&uartc.state, UART_TX_READY);
            uartc.data_transmitted = true;
        }
    }

    atomic_set(&uartc.state, UART_ISR_END);
}

/**************************** UART PROCESS ***********************************/

/**
 * Init UART/ACM to start getting input from user
 */

void uart_init()
{
    printk("Warning: The JavaScript terminal is in a different interface.\
            \nExamples:\n\tMac   /dev/cu.usbmodem\n\tLinux /dev/ttyACM0\n");

#ifdef CONFIG_USB_CDC_ACM
    uartc.dev = device_get_binding(CONFIG_CDC_ACM_PORT_NAME);
#else
    uartc.dev = device_get_binding(WEBUSB_SERIAL_PORT_NAME);
#endif

    if (!uartc.dev) {
        printf("Compatible USB device not found\n");
        return;
    }

#ifndef CONFIG_USB_CDC_ACM
    webusb_register_handlers();
#endif

    k_fifo_init(&uartc.data_queue);
    k_fifo_init(&uartc.avail_queue);

#ifdef CONFIG_UART_LINE_CTRL
    uint32_t dtr = 0;
    do {
        uart_line_ctrl_get(uartc.dev, LINE_CTRL_DTR, &dtr);
        k_sleep(1000);  // needed to allow the javascript in boot.cfg to run
    } while (!dtr);
#endif

    uart_irq_rx_disable(uartc.dev);
    uart_irq_tx_disable(uartc.dev);

    uart_irq_callback_set(uartc.dev, comms_interrupt_handler);

    /* Enable rx interrupts */
    uart_irq_rx_enable(uartc.dev);

    __stdout_hook_install(uart_printch);

    // Disable buffering on stdout since some parts write directly to uart fifo
    setbuf(stdout, NULL);

    atomic_set(&uartc.state, UART_INIT);

    ashell_init();
}

void uart_close(void)
{
    void *data = NULL;
    do {
        if (data != NULL)
            zjs_free(data);
        data = k_fifo_get(&uartc.avail_queue, K_NO_WAIT);
    } while (data);

    do {
        if (data != NULL)
            zjs_free(data);
        data = k_fifo_get(&uartc.data_queue, K_NO_WAIT);
    } while (data);

    uart_irq_rx_disable(uartc.dev);
    uart_irq_tx_disable(uartc.dev);

    atomic_set(&uartc.state, UART_CLOSE);
}

/*
 * Process user input
 */
void uart_process()
{
    static struct comms_input *data = NULL;

    atomic_set(&uartc.state, UART_WAITING);
    data = k_fifo_get(&uartc.data_queue, K_NO_WAIT);
    if (data) {
        size_t len = strnlen(data->line, MAX_LINE_LEN);
        ashell_process(data->line, len);
        fifo_recycle_buffer(data);
        data = NULL;
    } else {
        /* We clear the cache memory if there are no data transactions */
        if (uartc.tail == 0) {
            fifo_cache_clear();
        } else {
            /* Wait for a timeout and flush data if there was not a carriage return */
            if (atomic_get(&uartc.state) == UART_ISR_END && uartc.isr_data != NULL) {
                // DBG("Capturing buffer\n");
                uartc.isr_data->line[uartc.tail] = 0;
                uartc.tail = 0;
                uartc.isr_data = NULL;
                atomic_set(&uartc.state, UART_TASK_DATA_CAPTURE);
            }
        }
    }
}

/*************************** UART TRANSPORT *******************************/

static comms_driver_t uart_transport =
{
    .init = uart_init,
    .process = uart_process,
    .send = uart_send,
    .close = uart_close,
    .printch = uart_printch,
    .print = uart_print,
    .printf = uart_printf,
    .error = uart_error
};

comms_driver_t *comms_uart()
{
    return &uart_transport;
}
