// Copyright (c) 2016-2017, Intel Corporation.

/**
 * @file
 * @brief Shell to keep the different states of the machine
 */

// C includes
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// Zephyr includes
#include <atomic.h>
#include <ctype.h>
#include <fs.h>
#include <malloc.h>
#include <misc/printk.h>
#include <zephyr/types.h>

// ZJS includes
#include "comms-shell.h"
#include "comms-uart.h"
#include "shell-state.h"

#include "ihex-handler.h"

#include "../zjs_util.h"

static const char comms_default_prompt[] =
    ANSI_FG_YELLOW "acm> " ANSI_FG_RESTORE;
static const char *comms_prompt = NULL;

void comms_set_prompt(const char *prompt)
{
    comms_prompt = prompt;
}

const char *comms_get_prompt()
{
    if (comms_prompt == NULL)
        return comms_default_prompt;
    return comms_prompt;
}

#ifndef CONFIG_SHELL_UPLOADER_DEBUG
#define DBG(...) { ; }
#else
#define DBG printk
#endif /* CONFIG_SHELL_UPLOADER_DEBUG */

#define MAX_LINE 90
#define MAX_ARGUMENT_SIZE 32
#define CMD_ECHO_OFF "echo off\n"

static char *shell_line = NULL;
static u8_t tail = 0;
static bool ashell_is_done = false;
static bool echo_mode = true;

static inline void cursor_forward(unsigned int count)
{
    for (int t = 0; t < count; t++)
        comms_print("\x1b[1C");
}

static inline void cursor_backward(unsigned int count)
{
    for (int t = 0; t < count; t++)
        comms_print("\x1b[1D");
}

static inline void cursor_save(void)
{
    comms_print("\x1b[s");
}

static inline void cursor_restore(void)
{
    comms_print("\x1b[u");
}

static void insert_char(char *pos, char c, u8_t end)
{
    char tmp;

    if (end == 0) {
        *pos = c;
        return;
    }

    tmp = *pos;
    *(pos++) = c;

    cursor_save();

    while (end-- > 0) {
        comms_write_buf(&tmp, 1);
        c = *pos;
        *(pos++) = tmp;
        tmp = c;
    }

    /* Move cursor back to right place */
    cursor_restore();
}

static void del_char(char *pos, u8_t end)
{
    comms_write_buf("\b", 1);

    if (end == 0) {
        comms_write_buf(" ", 1);
        comms_write_buf("\b", 1);
        return;
    }

    cursor_save();

    while (end-- > 0) {
        *pos = *(pos + 1);
        comms_write_buf((pos++), 1);
    }

    comms_write_buf(" ", 1);

    /* Move cursor back to right place */
    cursor_restore();
}

enum
{
    ESC_ESC,
    ESC_ANSI,
    ESC_ANSI_FIRST,
    ESC_ANSI_VAL,
    ESC_ANSI_VAL_2
};

static atomic_t esc_state;
static unsigned int ansi_val, ansi_val_2;
static u8_t cur, end;

static void handle_ansi(u8_t byte)
{
    if (atomic_test_and_clear_bit(&esc_state, ESC_ANSI_FIRST)) {
        if (!isdigit(byte)) {
            ansi_val = 1;
            goto ansi_cmd;
        }

        atomic_set_bit(&esc_state, ESC_ANSI_VAL);
        ansi_val = byte - '0';
        ansi_val_2 = 0;
        return;
    }

    if (atomic_test_bit(&esc_state, ESC_ANSI_VAL)) {
        if (isdigit(byte)) {
            if (atomic_test_bit(&esc_state, ESC_ANSI_VAL_2)) {
                ansi_val_2 *= 10;
                ansi_val_2 += byte - '0';
            } else {
                ansi_val *= 10;
                ansi_val += byte - '0';
            }
            return;
        }

        /* Multi value sequence, e.g. Esc[Line;ColumnH */
        if (byte == ';' &&
            !atomic_test_and_set_bit(&esc_state, ESC_ANSI_VAL_2)) {
            return;
        }

        atomic_clear_bit(&esc_state, ESC_ANSI_VAL);
        atomic_clear_bit(&esc_state, ESC_ANSI_VAL_2);
    }

ansi_cmd:
    switch (byte) {
    case ANSI_BACKWARD:
        if (ansi_val > cur) {
            break;
        }

        end += ansi_val;
        cur -= ansi_val;
        cursor_backward(ansi_val);
        break;
    case ANSI_FORWARD:
        if (ansi_val > end) {
            break;
        }

        end -= ansi_val;
        cur += ansi_val;
        cursor_forward(ansi_val);
        break;
    default:
        break;
    }

    atomic_clear_bit(&esc_state, ESC_ANSI);
}

/** @brief Get the number of arguments in a string
 *
 * @param str   Null terminated string
 * @param nsize Check for size boundaries
 * @return Returns the number of arguments on the string
 */

u32_t ashell_get_argc(const char *str, u32_t nsize)
{
    if (str == NULL || nsize == 0 || *str == '\0')
        return 0;

    u32_t size = 1;
    bool div = false;

    /* Skip the first spaces */
    while (nsize-- > 0 && *str != 0 && *str == ' ') {
        str++;
        if (size) {
            size = 0;
            div = true;
        }
    }

    while (nsize-- > 0 && *str++ != 0) {
        if (*str == ' ')
            div = true;

        if (div && *str != ' ' && *str != '\0') {
            div = false;
            size++;
        }
    }

    return size;
}

/** @brief Copy the next argument into the string
 *
 * @param str     Null terminated string
 * @param nsize   Checks line size boundaries.
 * @param str_arg Initialized destination for the argument. It always null
 *                  terminates the string
 * @param length  Returns length of the argument found.
 * @return 0      Pointer to where this argument finishes
 */

const char *
ashell_get_next_arg(const char *str, u32_t nsize, char *str_arg, u32_t *length)
{
    *length = 0;
    if (nsize == 0 || str == NULL || *str == '\0') {
        str_arg[0] = '\0';
        return 0;
    }

    /* Skip spaces */
    while (nsize > 0 && *str != 0 && *str == ' ') {
        str++;
        nsize--;
    }

    while (nsize-- > 0 && *str != ' ') {
        *str_arg++ = *str++;
        (*length)++;
        if (*str == 0)
            break;
    }

    *str_arg = '\0';
    return str;
}

/** @brief Safe function to copy the next argument into a destination argument
*     string
*
* @param str          Null terminated string
* @param nsize        Checks line size boundaries.
* @param str_arg      Initialized destination for the argument
* @param max_arg_size Limits the size of the variable to return
* @param length	   Returns length of the argument found.
* @return 0 Pointer to where this argument finishes
*/

const char *ashell_get_next_arg_s(const char *str, u32_t nsize, char *str_arg,
                                  const u32_t max_arg_size, u32_t *length)
{
    /* Check size and allocate for string termination */
    if (nsize >= max_arg_size) {
        nsize = max_arg_size - 1;
        DBG(" MAX \n");
    }

    return ashell_get_next_arg(str, nsize, str_arg, length);
}

/** @brief Check a buffer for a parameter
 * Parameters are single characters in a '-xyz' sequence
 *
 * @param str          Null terminated string
 * @param parameter    Token we are looking for
 * @return bool Returns if the parameter is on the string
 */

bool ashell_check_parameter(const char *buf, const char parameter)
{
    size_t t = 0;
    bool space = true;
    bool token = false;

    if (buf == NULL)
        return false;

    while (buf[t] != 0) {
        char byte = buf[t];
        if (space && byte == '-')
            token = true;

        if (byte == ' ') {
            space = true;
            token = false;
        } else {
            space = false;
        }

        if (token && byte == parameter) {
            return true;
        }
        t++;
    }
    return false;
}

/**
 * @brief Skips all the spaces until it finds the first character
 *
 * @param str   Null terminated string
 * @return 0	Returns NULL at the end of the string
 */

char *ashell_skip_spaces(char *str)
{
    if (str == NULL || *str == '\0') {
        return NULL;
    }

    /* Skip initial spaces if any */
    while (*str == ' ') {
        str++;
        if (*str == '\0') {
            return NULL;
        }
    }
    return str;
}

/**
 * @brief Returns the next token in the string
 *        It modifies the string, so it is destructive.
 *
 * @param str   Null terminated string
 * @return 0	Returns NULL at the end of the string
 */

char *ashell_get_token_arg(char *str)
{
    str = ashell_skip_spaces(str);
    if (str == NULL)
        return NULL;

    /* Skip until we find a space of end of line */
    while (*str != 0 && *str != ' ') {
        str++;
    }

    /* Reached end of string, no more tokens */
    if (*str == '\0') {
        return NULL;
    }

    /* Terminate current token */
    *str++ = '\0';

    /* Skip spaces to search for new token if any */
    return ashell_skip_spaces(str);
}

u32_t ashell_process_init()
{
    DBG("[SHELL] Init\n");
    return 0;
}

void ashell_process_error(u32_t error)
{
}

u32_t ashell_process_data(const char *buf, u32_t len)
{
    u32_t processed = 0;
    // printed Is used to make sure we don't re-print characters
    u8_t printed = cur;
    bool flush_line = false;
    if (shell_line == NULL) {
        DBG("[Process]%d\n", (int)len);
        DBG("[%s]\n", buf);
        shell_line = (char *)zjs_malloc(MAX_LINE);
        memset(shell_line, 0, MAX_LINE);
        tail = 0;
    }

    /* Don't send back the 'echo off' command */
    if (strequal(CMD_ECHO_OFF, buf)) {
        echo_mode = false;
    }

    while (len-- > 0) {
        processed++;
        u8_t byte = *buf++;

        if (tail == MAX_LINE) {
            DBG("Line size exceeded \n");
            tail = 0;
        }

        /* Handle ANSI escape mode */
        if (atomic_test_bit(&esc_state, ESC_ANSI)) {
            handle_ansi(byte);
            continue;
        }

        /* Handle escape mode */
        if (atomic_test_and_clear_bit(&esc_state, ESC_ESC)) {
            switch (byte) {
            case ANSI_ESC:
                atomic_set_bit(&esc_state, ESC_ANSI);
                atomic_set_bit(&esc_state, ESC_ANSI_FIRST);
                break;
            default:
                break;
            }
            continue;
        }

        /* Handle special control characters */
        if (!isprint(byte)) {
            switch (byte) {
            case ASCII_DEL:
            case ASCII_BKSP:
                if (cur > 0) {
                    del_char(&shell_line[--cur], end);
                }
                break;
            case ASCII_ESC:
                atomic_set_bit(&esc_state, ESC_ESC);
                break;
            case ASCII_CR:
                DBG("<CR>\n");
                flush_line = true;
                break;
            case ASCII_TAB:
                comms_write_buf("\t", 1);
                break;
            case ASCII_IF:
                flush_line = true;
                DBG("<IF>");
                break;
            default:
                if (echo_mode) {
                    printf("<CTRL> %u\n", byte);
                }
                flush_line = true;
                shell_line[cur++] = byte;
                break;
            }
        }
        // We have a newline, flush the current line
        if (flush_line) {
            DBG("Line %u %u \n", cur, end);
            shell_line[cur + end] = '\0';
            if (comms_get_echo_mode()) {
                comms_write_buf(shell_line + printed, cur - printed);
                comms_write_buf("\r\n", 2);
            }

            u32_t length = strnlen(shell_line, MAX_LINE);

            ashell_main_state(shell_line, length);

            printk("\n%s", system_get_prompt());
            comms_print("\r\n");
            comms_print(comms_get_prompt());

            cur = end = printed = 0;
            flush_line = false;
            if (ashell_is_done) {
                break;
            }
        } else if (isprint(byte)) {
            /* Ignore characters if there's no more buffer space */
            if (cur + end < MAX_LINE - 1) {
                insert_char(&shell_line[cur++], byte, end);
            } else {
                DBG("Max line\n");
            }
        }
    }
    // Once the data has been parsed, print it
    if (comms_get_echo_mode()) {
        comms_write_buf(shell_line + printed, cur - printed);
    }

    /* Done processing line */
    if (cur == 0 && end == 0 && shell_line != NULL) {
        DBG("[Free]\n");
        zjs_free(shell_line);
        shell_line = NULL;
    }
    return processed;
}

bool ashell_process_is_done()
{
    return ashell_is_done;
}

void comms_set_echo_mode(bool mode)
{
    echo_mode = mode;
}ashell_process_close

bool comms_get_echo_mode()
{
    return echo_mode;
}

u32_t ashell_process_finish()
{
    DBG("[SHELL CLOSE]\n");
    ihex_process_start();
    return 0;
}

void ashell_print_status()
{
    DBG("Shell Status\n");

    malloc_stats();

    if (shell_line != NULL) {
        printk("Line [%s]\n", shell_line);
    } else {
        printk("No data on serial\n");
    }
}

void ashell_process_close()
{
    ashell_is_done = true;do
}

static struct comms_cfg uart_cfg = {
    .init = ashell_process_init,
    .error = ashell_process_error,
    .done = ashell_process_is_done,
    .close = ashell_process_finish,
    .process = ashell_process_data
};

void ashell_process_start()
{
    ashell_is_done = false;
    comms_config = &uart_cfg;
}

#ifdef CONFIG_SHELL_UNIT_TESTS
struct shell_tests {
    char *str;
    u32_t size;
    u32_t result;
};

#define TEST_PARAMS(str, size, params) \
    {                                  \
        str, size, params              \
    }

struct shell_tests test[] = {
    TEST_PARAMS(" ", 2, 0),
    TEST_PARAMS("     ", 6, 0),
    /* Wrong string length */
    TEST_PARAMS(" ", 0, 0),
    TEST_PARAMS("hello world", 12, 2),
    TEST_PARAMS("hello", 6, 1),

    TEST_PARAMS("test1 ( )", 10, 3),
    TEST_PARAMS("test2 ( ) ", 8, 2),
    TEST_PARAMS("test3 (    )     ", sizeof("test2 (    )     "), 3),
    TEST_PARAMS(" test4", 7, 1),
    TEST_PARAMS("test5 ( ) ", sizeof("test5 ( ) "), 3),
    TEST_PARAMS("test6 ( ) ", 11, 3),
    TEST_PARAMS("test7 ", 7, 1),
    TEST_PARAMS("h  w", 5, 2),

    /* Buffer overflow */
    TEST_PARAMS("12345678901234567890123456789012345678901234567890123456789012345678901234567890", 32, 1),
    /* Cut the string */
    TEST_PARAMS(" test8 ", 8, 1),
    TEST_PARAMS(NULL, 0, 0)
};

struct shell_tests param_test[] = {
    TEST_PARAMS("-xyz", 't', 0),
    TEST_PARAMS("-xyz ", 't', 0),
    TEST_PARAMS("-xyz ", 'x', 1),
    TEST_PARAMS("test -xyz", 'y', 1),
    TEST_PARAMS(" test  -xyz", 'y', 1),
    TEST_PARAMS("  test  -xyz -x", 'x', 1),
    TEST_PARAMS("test  -xyz -x", 'a', 0),
    TEST_PARAMS(" test  -xyz -x", 'e', 0),
    TEST_PARAMS(" test  -xyz abc a ", 'a', 0),
    TEST_PARAMS(" test  -x abc a ", 'x', 1),
    TEST_PARAMS(" test  abc  -x a ", 'x', 1),
    TEST_PARAMS("", ' ', 0)
};

void shell_unit_test()
{
    u32_t t = 0;
    u32_t argc;
    char tmp[512];
    char *buf;
    char *next;

    while (t != sizeof(test) / sizeof(shell_tests)) {
        argc = ashell_get_argc(test[t].str, test[t].size);
        if (argc != test[t].result) {
            printf("Failed [%s] %d!=%d ", test[t].str, test[t].result, argc);
            return;
        }
        t++;
    }

    char arg[32];
    u32_t len;
    t = 0;

    while (t != sizeof(test) / sizeof(shell_tests)) {
        const char *line = test[t].str;
        argc = ashell_get_argc(line, test[t].size);
        printf("Test [%s] %d\n", line, argc);
        while (argc > 0) {
            line = ashell_get_next_arg(line, strnlen(line, 128), arg, &len);
            if (len != strnlen(arg, 32)) {
                printf("Failed [%s] %d!=%d ", arg, len, strnlen(arg, 32));
            }
            printf(" %d [%s]\n", test[t].result - argc, arg);
            argc--;
        }
        t++;
    }

    /* Destructive case, since get token will zero terminate the string */
    while (t != sizeof(test) / sizeof(shell_tests)) {
        if (test[t].str != NULL) {
            strncpy_s(tmp, test[t].str, sizeof(tmp));
            buf = tmp;
            argc = 0;

            printf("------------------------- \n");
            while (buf != NULL) {
                buf[test[t].size] = '\0';
                buf = ashell_skip_spaces(buf);
                if (buf == NULL) {
                    next = NULL;
                    printf("No arguments \n");
                } else {
                    next = ashell_get_token_arg(buf);
                    argc++;
                    printf(" %d [%s]\n", argc, buf);
                }

                buf = next;
            }

            if (argc != test[t].result) {
                printf(" Failed test %d\n", t);
            }
        }
        t++;
    }

    t = 0;
    while (t != sizeof(param_test) / sizeof(shell_tests)) {
        u32_t res = ashell_check_parameter(param_test[t].str,
                                           (char)param_test[t].size);
        if (res != param_test[t].result) {
            printf("Failed test (%s) %c\n", param_test[t].str,
                   (char)param_test[t].size);
        };
        t++;
    }
    printf("All tests were successful \n");
}
#endif  // CONFIG_SHELL_UNIT_TESTS
