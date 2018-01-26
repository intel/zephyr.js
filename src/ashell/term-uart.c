// Copyright (c) 2016-2018, Intel Corporation.

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

// C includes
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Zephyr includes
#include <device.h>
#include <init.h>

#include <atomic.h>
#include <board.h>
#include <toolchain.h>
#include <uart.h>

#include <fs.h>
#include <misc/printk.h>

// JerryScript includes
#include "jerry-code.h"

// ZJS includes
#include "term-cmd.h"
#include "term-uart.h"

#ifndef CONFIG_USB_CDC_ACM
#include "webusb_serial.h"
#endif

#include "ihex/kk_ihex_read.h"

#include "../zjs_util.h"

#define FIVE_SECONDS    (5 * sys_clock_ticks_per_sec)
#define TEN_SECONDS     (10 * sys_clock_ticks_per_sec)

#define CTRL_START 0x00
#define CTRL_END   0x1F

#ifndef CONFIG_IHEX_DEBUG
#define DBG(...) { ; }
#else
#define DBG printk
#endif /* CONFIG_IHEX_DEBUG */

extern void __stdout_hook_install(int (*fn)(int));
extern void webusb_register_handlers();

static const char banner[] = "Zephyr.js DEV MODE " __DATE__ " " __TIME__ "\r\n";

#define FIFO_CACHE 2

/* Configuration of the callbacks to be called */
struct terminal_config *terminal = NULL;

struct uart_input {
    int _unused;
    char line[MAX_LINE + 1];
};

static struct k_fifo avail_queue;
static struct k_fifo data_queue;
static bool uart_process_done = false;
static u8_t fifo_size = 0;
static u8_t max_fifo_size = 0;

atomic_t data_queue_count = 0;

u32_t alloc_count = 0;
u32_t free_count = 0;

static struct uart_input *isr_data = NULL;
static u32_t tail = 0;
static char *buf;
static u32_t dtr = 0;

struct uart_input *fifo_get_isr_buffer()
{
    void *data = k_fifo_get(&avail_queue, K_NO_WAIT);
    if (!data) {
        data = (void *)zjs_malloc(sizeof(struct uart_input));
        memset(data, '*', sizeof(struct uart_input));
        alloc_count++;
        fifo_size++;
        if (fifo_size > max_fifo_size)
            max_fifo_size = fifo_size;
    }
    return (struct uart_input *)data;
}

void fifo_cache_clear()
{
    while (fifo_size > 0) {
        void *data = k_fifo_get(&avail_queue, K_NO_WAIT);
        if (!data)
            return;

        zjs_free(data);
        fifo_size--;
        free_count++;
    }
    tail = 0;
    buf = NULL;
    isr_data = NULL;
}

void fifo_recycle_buffer(struct uart_input *data)
{
    if (fifo_size > 1) {
        zjs_free(data);
        fifo_size--;
        free_count++;
        return;
    }
    k_fifo_put(&avail_queue, data);
}

/**************************** UART CAPTURE **********************************/

static struct device *dev_upload;

static volatile bool data_transmitted;

u32_t bytes_received = 0;
u32_t bytes_processed = 0;

atomic_t uart_state = 0;

enum {
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

static void uart_interrupt_handler(struct device *dev)
{
    char byte;

    u32_t bytes_read = 0;
    u32_t len = 0;

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
            len = MAX_LINE - tail;
            bytes_read = uart_fifo_read(dev_upload, buf, len);
            bytes_received += bytes_read;
            tail += bytes_read;

            /* We don't want to flush data too fast otherwise we would be
            * allocating but we want to flush as soon as we have processed
            * the data on the task so we don't queue too much and delay the
            * system response.
            *
            * When the process has finished dealing with the data it signals
            * this method with a 'i am ready to continue' by changing
            * uart_process_done.
            *
            * It is also imperative to flush when we reach the limit of the
            * buffer.
            *
            * If we are still fine in the cache limits, then we keep flushing
            * every time we get a byte.
            */
            bool flush = false;

            if (fifo_size == 1 || tail == MAX_LINE || uart_process_done) {
                flush = true;
                uart_process_done = false;
            } else {
                /* Check for line ends, to flush the data. The decoder / shell
                 * will probably sit for a bit in the data so it is better if
                 * we finish this buffer and send it.
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

/*
* @brief Writes data into the uart and flushes it.
*
* @param buf Buffer to write
* @param len length of buffer
*
* @todo Really dislike this wait here from the example
* will probably rewrite it later with a line queue
*/

void uart_write_buf(const char *buf, int len)
{
    if (len <= 0)
        return;

    uart_irq_tx_enable(dev_upload);
    int sent = 0;

    while (len > 0) {
        data_transmitted = false;
        // FIXME: docs indicate this function should only be called from ISR
        //   context; check on that in Zephyr code/samples and our usage
        sent = uart_fifo_fill(dev_upload, (const u8_t *)buf, len);

        if (sent) {
            // wait for data
            while (data_transmitted == false) {}
        }

        len -= sent;
        buf += sent;
    }

    uart_irq_tx_disable(dev_upload);
}

/**
* @brief Output one character to UART ACM
*
* @param c Character to output
* @return success
*/

static int uart_out(int c)
{
    static char buf[80];
    static int size = 0;
    char ch = (char)c;
    buf[size++] = ch;
    if (ch == '\n' || size == 80) {
        terminal->send(buf, size);
        size = 0;
    }
    return 1;
}

u32_t uart_get_baudrate(void)
{
    u32_t baudrate;

    int ret = uart_line_ctrl_get(dev_upload, LINE_CTRL_BAUD_RATE, &baudrate);
    if (ret)
        printk("Fail baudrate %d\n", ret);
    else
        printk("Baudrate %d\n", (int)baudrate);

    return baudrate;
}

static void uart_ready()
{
    // Need a short wait after uart_line_ctrl_get success
    k_sleep(1000);

    uart_irq_rx_disable(dev_upload);
    uart_irq_tx_disable(dev_upload);

    uart_irq_callback_set(dev_upload, uart_interrupt_handler);
    terminal->send(banner, sizeof(banner));

    /* Enable rx interrupts */
    uart_irq_rx_enable(dev_upload);
    DBG("[Listening]\n");

    __stdout_hook_install(uart_out);

    // Disable buffering on stdout since some parts write directly to uart fifo
    setbuf(stdout, NULL);

    ashell_help("");
    comms_print(comms_get_prompt());
    process_state = 0;

    atomic_set(&uart_state, UART_INIT);

    DBG("[Init]\n");
    terminal->init();
}

static bool check_uart_connection()
{
    // Try to connect to the device
    uart_line_ctrl_get(dev_upload, LINE_CTRL_DTR, &dtr);
    if (dtr) {
        uart_ready();
        return true;
    }
    return false;
}

/*
 * Process user input
 */
void uart_process()
{
    if (!dtr) {
        if (!check_uart_connection()) {
            // No connection yet, bail out
            return;
        }
    }

    static struct uart_input *data = NULL;
    char *buf = NULL;
    u32_t len = 0;
    atomic_set(&uart_state, UART_INIT);

    while (!terminal->done()) {
        atomic_set(&uart_state, UART_WAITING);
        data = k_fifo_get(&data_queue, K_NO_WAIT);
        if (data) {
            atomic_dec(&data_queue_count);
            buf = data->line;
            len = strnlen(buf, MAX_LINE);

            terminal->process(buf, len);
            uart_process_done = true;
            DBG("[Recycle]\n");
            fifo_recycle_buffer(data);
            data = NULL;

            DBG("[Data]\n");
            DBG("%s\n", buf);
        } else {
            /* We clear the cache memory if there are no data transactions */
            if (tail == 0) {
                fifo_cache_clear();
            } else {
                /* Wait for a timeout and flush data if there was not a carriage
                 * return */
                if (atomic_get(&uart_state) == UART_ISR_END &&
                    isr_data != NULL) {
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
            // We have nothing to do, bail out of the while.
            return;
        }
    }
    atomic_set(&uart_state, UART_CLOSE);
    terminal->close();
}

/**
 * Ashell initialization
 * Init UART/ACM to start getting input from user
 */

void uart_init()
{
    terminal_start();

    printk(banner);
    printk("Warning: The JavaScript terminal is in a different interface.\
            \nExamples:\n\tMac   /dev/cu.usbmodem\n\tLinux /dev/ttyACM0\n");

#ifdef CONFIG_USB_CDC_ACM
    dev_upload = device_get_binding(CONFIG_CDC_ACM_PORT_NAME);
#else
    dev_upload = device_get_binding(WEBUSB_SERIAL_PORT_NAME);
#endif

    if (!dev_upload) {
        printf("Compatible USB device not found\n");
        return;
    }

#ifndef CONFIG_USB_CDC_ACM
    webusb_register_handlers();
#endif

    k_fifo_init(&data_queue);
    k_fifo_init(&avail_queue);
}

void zjs_ashell_init()
{
    uart_init();
}

void zjs_ashell_process()
{
    uart_process();
}
