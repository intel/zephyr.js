// Copyright (c) 2016-2018, Intel Corporation.

/**
 * @file
 * @brief Shell to keep the different states of the machine
 */

// C includes
#include <ctype.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// Zephyr includes
#include "file-utils.h"
#include "jerryscript-port.h"
#include <atomic.h>
#include <misc/printk.h>
#include <misc/reboot.h>
#include <zephyr/types.h>
#ifdef CONFIG_BOARD_ARDUINO_101
#include <flash.h>
#endif

// ZJS includes
#include "term-cmd.h"
#include "term-ihex.h"
#include "term-uart.h"
#include "../zjs_util.h"

// JerryScript includes
#include "jerry-code.h"

#ifndef CONFIG_USB_CDC_ACM
#define CONFIG_IDE
#endif

#ifdef CONFIG_REBOOT
// TODO Waiting for patch https://gerrit.zephyrproject.org/r/#/c/3161/
#ifdef CONFIG_BOARD_ARDUINO_101
#include <qm_init.h>
#endif
#endif

/* Control characters */
/* http://www.physics.udel.edu/~watson/scen103/ascii.html */

/* Escape */
#define ASCII_ESC                0x1b
#define ASCII_DEL                0x7f
#define ASCII_BKSP               0x08

/* CTLR-X */
#define ASCII_CANCEL             0x18

/* CTRL-B Start of text */
#define ASCII_START_OF_TEXT      0x02

/* CTRL-C End of text */
#define ASCII_END_OF_TEXT        0x03

/* CTLR-Z */
#define ASCII_SUBSTITUTE         0x1A

/* CTLR-D End of transmission */
#define ASCII_END_OF_TRANS       0x04

#define ASCII_CR                 '\r'
#define ASCII_IF                 '\n'
#define ASCII_TAB                '\t'

/* ANSI escape sequences */
#define ANSI_ESC                 '['
#define ANSI_UP                  'A'
#define ANSI_DOWN                'B'
#define ANSI_FORWARD             'C'
#define ANSI_BACKWARD            'D'

/**
 * Ansi helpers
 * https://telepathy.freedesktop.org/doc/telepathy-glib/telepathy-glib-debug-ansi.html
 */

#define ANSI_FG_RED        "\x1b[31m"
#define ANSI_FG_GREEN      "\x1b[32m"
#define ANSI_FG_YELLOW     "\x1b[33m"
#define ANSI_FG_BLUE       "\x1b[34m"
#define ANSI_FG_MAGENTA    "\x1b[35m"
#define ANSI_FG_CYAN       "\x1b[36m"
#define ANSI_FG_WHITE      "\x1b[37m"
#define ANSI_FG_RESTORE    "\x1b[39;0m"
#define ANSI_FG_LIGHT_BLUE "\x1b[34;1m"
#define ANSI_CLEAR         "\x1b[2J\x1b[H"

/**
 * Contains the pointer to the memory where the code will be uploaded
 * using the stub interface at file_code.c
 */
static fs_file_t *file_code = NULL;

/* Configuration of the callbacks to be called */
static struct shell_state_config shell = {
    .state_flags = kShellTransferRaw
};

const char ERROR_NOT_ENOUGH_ARGUMENTS[] = "Not enough arguments\r\n";
const char ERROR_FILE_NOT_FOUND[] = "File not found\r\n";

const char MSG_FILE_SAVED[] =
     ANSI_FG_GREEN "Saving file. " ANSI_FG_RESTORE
     "run the 'run' command to see the result\r\n";

const char MSG_FILE_ABORTED[] = ANSI_FG_RED "Aborted!\r\n";
const char MSG_EXIT[] = ANSI_FG_GREEN "Back to shell!\r\n";

const char READY_FOR_RAW_DATA[] = "Ready for JavaScript. \r\n"
                                  "\tCtrl+Z to finish transfer.\r\n"
                                  "\tCtrl+X to cancel.\r\n";

const char MSG_IMMEDIATE_MODE[] = "Ready to evaluate JavaScript.\r\n"
                                  "\tCtrl+D to return to shell.\r\n";

const char *BUILD_TIMESTAMP = __DATE__ " " __TIME__ "\n";

#define CMD_TRANSFER "transfer"

#define READ_BUFFER_SIZE 80

#ifndef CONFIG_IHEX_UPLOADER_DEBUG
#define DBG(...) { ; }
#else
#define DBG printk
#endif /* CONFIG_IHEX_UPLOADER_DEBUG */

const char hex_prompt[] = "[HEX]\r\n";
const char raw_prompt[] = ANSI_FG_YELLOW "RAW> " ANSI_FG_RESTORE;
const char eval_prompt[] = ANSI_FG_GREEN "js> " ANSI_FG_RESTORE;
const char acm_prompt[] = ANSI_FG_YELLOW "acm> " ANSI_FG_RESTORE;
const char *comms_prompt = NULL;

void comms_set_prompt(const char *prompt)
{
    comms_prompt = prompt;
}

const char *comms_get_prompt()
{
    if (comms_prompt == NULL)
        return acm_prompt;
    return comms_prompt;
}

#define CMD_ECHO_OFF "echo off\n"

static bool terminal_done = false;
static bool echo_mode = true;

void comms_set_echo_mode(bool mode)
{
    echo_mode = mode;
}

bool comms_get_echo_mode()
{
    return echo_mode;
}

void comms_print(const char *buf)
{
    terminal->send(buf, strnlen(buf, MAX_LINE));
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

/**
 * @brief Skips all the spaces until it finds the first character
 *
 * @param str   Null terminated string
 * @return 0    Returns NULL at the end of the string
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
 * @return 0    Returns NULL at the end of the string
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
* @param length    Returns length of the argument found.
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

s32_t ashell_get_filename_buffer(const char *buf, char *destination)
{
    u32_t arg_len = 0;
    u32_t len = strnlen(buf, MAX_FILENAME_SIZE);
    if (len == 0)
        return RET_ERROR;

    if (buf[0] == '-')
        return 0;

    ashell_get_next_arg_s(buf, len, destination, MAX_FILENAME_SIZE, &arg_len);

    if (arg_len == 0) {
        *destination = '\0';
        comms_print(ERROR_NOT_ENOUGH_ARGUMENTS);
        return RET_ERROR;
    }

    return arg_len;
}

s32_t ashell_remove_file(char *buf)
{
    char filename[MAX_FILENAME_SIZE];
    if (ashell_get_filename_buffer(buf, filename) <= 0) {
        return RET_ERROR;
    }

    int res = fs_unlink(filename);
    if (!res)
        return RET_OK;

    printf("rm: cannot remove '%s': %d\n", filename, res);
    return RET_ERROR;
}

s32_t ashell_remove_dir(char *buf)
{
    return RET_OK;
}

s32_t ashell_make_dir(char *buf)
{
    return RET_OK;
}

s32_t ashell_disk_usage(char *buf)
{
    char filename[MAX_FILENAME_SIZE];
    if (ashell_get_filename_buffer(buf, filename) <= 0) {
        return RET_ERROR;
    }

    fs_file_t *file = fs_open_alloc(filename, "r");
    if (file == NULL) {
        comms_print(ERROR_FILE_NOT_FOUND);
        return RET_ERROR;
    }

    ssize_t size = fs_size(file);
    fs_close_alloc(file);
    printf("%5d %s\n", (unsigned int)size, filename);
    return RET_OK;
}

s32_t ashell_rename(char *buf)
{
    static struct fs_dirent entry;
    char path_org[MAX_FILENAME_SIZE];
    char path_dest[MAX_FILENAME_SIZE];

    if (ashell_get_filename_buffer(buf, path_org) > 0) {
        /* Check if file or directory */
        if (fs_stat(path_org, &entry)) {
            comms_printf("mv: cannot access '%s' no such file or directory\n",
                         path_org);
            return RET_ERROR;
        }
    }

    /* Tokenize and isolate the command */
    char *next = ashell_get_token_arg(buf);

    if (next == NULL) {
        comms_print(ERROR_NOT_ENOUGH_ARGUMENTS);
        return RET_ERROR;
    }

    ashell_get_token_arg(buf);

    if (ashell_get_filename_buffer(next, path_dest) > 0) {
        /* Check if file or directory */
        if (!fs_stat(path_dest, &entry)) {
            comms_printf("mv: cannot access '%s' file already exists\n",
                         path_dest);
            return RET_ERROR;
        }
        if (!fs_valid_filename(path_dest)) {
            return RET_ERROR;
        }
    }

    f_rename(path_org, path_dest);
    return RET_OK;
}

s32_t ashell_reboot(char *buf)
{
#ifdef CONFIG_REBOOT
// TODO Waiting for patch https://gerrit.zephyrproject.org/r/#/c/3161/
#ifdef CONFIG_BOARD_ARDUINO_101
    QM_SCSS_PMU->rstc |= QM_COLD_RESET;
#endif
#endif
    sys_reboot(SYS_REBOOT_COLD);
    return RET_OK;
}

s32_t ashell_list_dir(char *buf)
{
    char filename[MAX_FILENAME_SIZE];
    static struct fs_dirent entry;
    s32_t res;
    fs_dir_t dp;
    *filename = '\0';

    if (ashell_get_filename_buffer(buf, filename) > 0) {
        /* Check if file or directory */
        if (!fs_stat(filename, &entry)) {
            if (entry.type == FS_DIR_ENTRY_DIR) {
                ashell_disk_usage(filename);
                return RET_OK;
            }
        } else {
            printf("ls: cannot access %s: no such file or directory\n",
                   filename);
            return RET_ERROR;
        }
    }

    res = fs_opendir(&dp, filename);
    if (res) {
        printf("Error opening dir [%d]\n", (int)res);
        return res;
    }

    if (shell.state_flags & !kShellTransferIhex) {
        printf(ANSI_FG_LIGHT_BLUE "      .\n      ..\n" ANSI_FG_RESTORE);
    }

    char filesizeStr[16];

    for (;;) {
        res = fs_readdir(&dp, &entry);

        /* entry.name[0] == 0 means end-of-dir */
        if (res || entry.name[0] == 0) {
            break;
        }
        if (entry.type == FS_DIR_ENTRY_DIR) {
            printf(ANSI_FG_LIGHT_BLUE "%s\n" ANSI_FG_RESTORE, entry.name);
        } else {
            char *p = entry.name;
            for (; *p; ++p)
                *p = tolower((int)*p);

            int filesizeStrLen = snprintf(filesizeStr, 16, "%d",
                                          (int)entry.size);
            terminal->send(filesizeStr, filesizeStrLen);
            terminal->send("\t", 1);
            comms_print(entry.name);
            terminal->send("\r\n", 2);
        }
    }

    fs_closedir(&dp);
    return RET_OK;
}

s32_t ashell_print_file(char *buf)
{
    char filename[MAX_FILENAME_SIZE];
    char data[READ_BUFFER_SIZE];
    fs_file_t *file;
    ssize_t count;

    if (ashell_get_filename_buffer(buf, filename) <= 0) {
        return RET_ERROR;
    }

    file = fs_open_alloc(filename, "r");

    /* Error getting an id for our data storage */
    if (!file) {
        comms_print(ERROR_FILE_NOT_FOUND);
        return RET_ERROR;
    }

    ssize_t size = fs_size(file);
    if (size == 0) {
        comms_print("Empty file\r\n");
        fs_close_alloc(file);
        return RET_OK;
    }

    fs_seek(file, 0, SEEK_SET);

    int lineStart = 0;
    do {
        count = fs_read(file, data, READ_BUFFER_SIZE);
        for (int t = 0; t < count; t++) {
            if (data[t] == '\n' || data[t] == '\r') {
                int strLen = t - lineStart;
                terminal->send(&data[lineStart], strLen);
                terminal->send("\r\n", 2);
                lineStart = t + 1;
            }
        }
        // If we have data left that doesn't end in a newline, print it.
        if (lineStart < count) {
            int strLen = count - lineStart;
            terminal->send(&data[lineStart], strLen);
        }
        // Reset the line start
        lineStart = 0;
    } while (count > 0);

    terminal->send("\r\n", 2);
    fs_close_alloc(file);
    return RET_OK;
}

s32_t ashell_parse_javascript(char *buf)
{
    char filename[MAX_FILENAME_SIZE];
    if (ashell_get_filename_buffer(buf, filename) <= 0) {
        return RET_ERROR;
    }

    javascript_parse_code(filename);
    return RET_OK;
}

s32_t ashell_run_javascript(char *buf)
{
    char filename[MAX_FILENAME_SIZE];
    if (ashell_get_filename_buffer(buf, filename) <= 0) {
        return RET_ERROR;
    }
    if (shell.state_flags & kShellTransferIhex) {
        comms_print("[RUN]\n");
    }
    javascript_run_code(filename);
    return RET_OK;
}

s32_t ashell_start_raw_capture(char *filename)
{
    file_code = fs_open_alloc(filename, "w+");

    /* Error getting an id for our data storage */
    if (!file_code) {
        return RET_ERROR;
    }
    return RET_OK;
}

s32_t ashell_close_capture()
{
    return fs_close_alloc(file_code);
}

s32_t ashell_discard_capture()
{
    fs_close_alloc(file_code);
    // TODO ashell_remove_file(file_code);
    return RET_OK;
}

s32_t ashell_eval_javascript(const char *buf, u32_t len)
{
    const char *src = buf;

    while (len > 0) {
        u8_t byte = *buf++;
        if (!isprint(byte)) {
            switch (byte) {
            case ASCII_END_OF_TRANS:
            case ASCII_SUBSTITUTE:
            case ASCII_END_OF_TEXT:
            case ASCII_CANCEL:
                comms_print(MSG_EXIT);
                shell.state_flags &= ~kShellEvalJavascript;
                comms_set_prompt(NULL);
                return RET_OK;
            }
        }
        len--;
    }

    javascript_eval_code(src, strnlen(src, MAX_BUFFER_SIZE));
    return RET_OK;
}

s32_t ashell_raw_capture(const char *buf, u32_t len)
{
    u8_t eol = '\n';

    while (len > 0) {
        u8_t byte = *buf++;
        if (!isprint(byte)) {
            switch (byte) {
            case ASCII_END_OF_TRANS:
            case ASCII_SUBSTITUTE:
                comms_print(MSG_FILE_SAVED);
                shell.state_flags &= ~kShellCaptureRaw;
                comms_set_prompt(NULL);
                ashell_close_capture();
                return RET_OK;
                break;
            case ASCII_END_OF_TEXT:
            case ASCII_CANCEL:
                comms_print(MSG_FILE_ABORTED);
                shell.state_flags &= ~kShellCaptureRaw;
                comms_set_prompt(NULL);
                ashell_discard_capture();
                return RET_OK;
            case ASCII_CR:
            case ASCII_IF:
                comms_print("\r\n");
                break;
            default:
                terminal->send(buf, 1);
            }
        } else {
            size_t written = fs_write(file_code, &byte, 1);
            if (written == 0) {
                return RET_ERROR;
            }
            DBG("%c", byte);
        }
        len--;
    }

    fs_write(file_code, &eol, 1);
    return RET_OK_NO_RET;
}

s32_t ashell_set_echo_mode(char *buf)
{
    if (strequal(buf, "on")) {
        comms_print("echo_on");
        comms_set_echo_mode(true);
    } else if (strequal(buf, "off")) {
        comms_print("echo_off");
        comms_set_echo_mode(false);
    }
    return RET_OK;
}

s32_t ashell_read_data(char *buf)
{
    if (shell.state_flags & kShellTransferIhex) {
        terminal_done = true;
        return RET_OK;
    }

    if (!fs_valid_filename(buf)) {
        return RET_ERROR;
    }

    char filename[MAX_FILENAME_SIZE];
    if (shell.state_flags & kShellTransferRaw) {
        if (ashell_get_filename_buffer(buf, filename) <= 0) {
            return RET_ERROR;
        }

        if (comms_get_echo_mode()) {
            comms_print(ANSI_CLEAR);
            comms_printf("Saving to '%s'\r\n", filename);
            comms_print(READY_FOR_RAW_DATA);
            comms_set_prompt(raw_prompt);
        }
        shell.state_flags |= kShellCaptureRaw;
        ashell_start_raw_capture(filename);
    }
    return RET_OK;
}

s32_t ashell_js_immediate_mode(char *buf)
{
    shell.state_flags |= kShellEvalJavascript;
    comms_print(ANSI_CLEAR);
    comms_print(MSG_IMMEDIATE_MODE);
    comms_set_prompt(eval_prompt);
    return RET_OK;
}

s32_t ashell_set_transfer_state(char *buf)
{
    char *next;
    if (buf == 0) {
        comms_print(ERROR_NOT_ENOUGH_ARGUMENTS);
        return RET_ERROR;
    }
    next = ashell_get_token_arg(buf);
    comms_print(buf);

    if (strequal(buf, "raw")) {
        comms_set_prompt(NULL);
        shell.state_flags |= kShellTransferRaw;
        shell.state_flags &= ~kShellTransferIhex;
        return RET_OK;
    }

    if (strequal(buf, "ihex")) {
        comms_set_prompt(hex_prompt);
        shell.state_flags |= kShellTransferIhex;
        shell.state_flags &= ~kShellTransferRaw;
        return RET_OK;
    }
    return RET_UNKNOWN;
}

s32_t ashell_set_state(char *buf)
{
    if (buf == 0) {
        comms_print(ERROR_NOT_ENOUGH_ARGUMENTS);
        return RET_ERROR;
    }

    char *next = ashell_get_token_arg(buf);
    if (strequal(buf, CMD_TRANSFER)) {
        return ashell_set_transfer_state(next);
    }

    return RET_UNKNOWN;
}

s32_t ashell_get_state(char *buf)
{
    if (buf == 0) {
        comms_print(ERROR_NOT_ENOUGH_ARGUMENTS);
        return RET_ERROR;
    }

    ashell_get_token_arg(buf);
    if (strequal(buf, CMD_TRANSFER)) {
        DBG("Flags %lu\n", shell.state_flags);

        if (shell.state_flags & kShellTransferRaw)
            comms_print("Raw\r\n");

        if (shell.state_flags & kShellTransferIhex)
            comms_print("Ihex\r\n");

        return RET_OK;
    }
    return RET_UNKNOWN;
}

s32_t ashell_at(char *buf)
{
    comms_print("OK\r\n\r\n");
    return RET_OK;
}

s32_t ashell_clear(char *buf)
{
    comms_print(ANSI_CLEAR);
    return RET_OK;
}

#ifdef CONFIG_BOARD_ARDUINO_101
s32_t ashell_erase_flash(char *buf)
{
    struct device *flash_dev;
    flash_dev = device_get_binding(CONFIG_SPI_FLASH_W25QXXDV_DRV_NAME);

    if (!flash_dev) {
        comms_print("SPI flash driver was not found!\n\r\n");
        return RET_ERROR;
    }

    flash_write_protection_set(flash_dev, false);
    if (flash_erase(flash_dev, 0, CONFIG_SPI_FLASH_W25QXXDV_FLASH_SIZE) != 0) {
        comms_print("Flash erase failed\n\r\n");
        return RET_ERROR;
    }
    flash_write_protection_set(flash_dev, true);

    comms_print("Flash erase succeeded, rebooting...\n\r\n");
    ashell_reboot(NULL);
    return RET_OK;
}
#endif

s32_t ashell_stop_javascript(char *buf)
{
    javascript_stop();
    return RET_OK;
}

s32_t ashell_check_control(const char *buf, u32_t len)
{
    while (len > 0) {
        u8_t byte = *buf++;
        if (!isprint(byte)) {
            switch (byte) {
            case ASCII_SUBSTITUTE:
                DBG("<CTRL + Z>");
                break;

            case ASCII_END_OF_TRANS:
                DBG("<CTRL + D>");
                break;
            }
        }
        len--;
    }
    return RET_OK;
}

s32_t ashell_set_bootcfg(char *buf)
{
    if (!fs_exist(buf)) {
        comms_print("File passed to cfg doesn't exist\n\r\n");
        return RET_ERROR;
    }

    fs_file_t *file = fs_open_alloc("boot.cfg", "w+");
    if (!file) {
        comms_print("Failed to create boot.cfg file\r\n");
        return RET_ERROR;
    }

    ssize_t written = fs_write(file, BUILD_TIMESTAMP, strlen(BUILD_TIMESTAMP));
    written += fs_write(file, buf, strlen(buf));
    if (written <= 0) {
        comms_print("Failed to write boot.cfg file\r\n");
    }

    fs_close_alloc(file);
    return RET_OK;
}

#ifdef CONFIG_IDE
// skip listing this command in the IDE by setting description string empty
#define IDE_SKIP(str) ""
#else
#define IDE_SKIP(str) str
#endif

static const struct ashell_cmd commands[] = {
    // CMD    ARGS      DESCRIPTION  IMPL
    {"help",  "",       "Display this help information", ashell_help},
    {"eval",  "",       "Evaluate JavaScript in real time",
                          ashell_js_immediate_mode},
    {"load",  "FILE",   IDE_SKIP("Save the input text into a file"),
                          ashell_read_data},
    {"run",   "FILE",   "Run the JavaScript program in the file",
                          ashell_run_javascript},
    {"parse", "FILE",   IDE_SKIP("Check if the JS syntax is correct"),
                          ashell_parse_javascript},
    {"stop",  "",       "Stop current JavaScript execution",
                          ashell_stop_javascript},
    {"ls",    "",       "List all files", ashell_list_dir},
    {"cat",   "FILE",   "Print the contents of a file", ashell_print_file},
    {"du",    "FILE",   IDE_SKIP("Estimate file space usage"),
                          ashell_disk_usage},
    {"rm",    "FILE",   "Remove file or directory", ashell_remove_file},
    {"mv",    "F1 F2",  "Move file F1 to destination F2", ashell_rename},
    {"clear", "",       "Clear the terminal screen", ashell_clear},
#ifdef CONFIG_BOARD_ARDUINO_101
    {"erase", "",       "Erase flash file system and reboot",
                          ashell_erase_flash},
#endif
    {"boot",  "FILE",   "Set the file that should run at boot",
                          ashell_set_bootcfg},
    {"reboot", "",      "Reboot the device", ashell_reboot},

    // undocumented commands used by IDE
    {"echo",  "on/off", "", ashell_set_echo_mode},
    {"set",   "",       "", ashell_set_state},
    {"get",   "",       "", ashell_get_state},
};

#define ASHELL_COMMANDS_COUNT (sizeof(commands) / sizeof(*commands))

s32_t ashell_help(char *buf)
{
    comms_print("Welcome to the ZJS 'A Shell' interface!\r\n\r\n");
    comms_print("Command list:\r\n");
    for (u32_t t = 0; t < ASHELL_COMMANDS_COUNT; t++) {
        // skip commands with empty description
        if (!commands[t].syntax[0]) {
            continue;
        }
        char buf[40];
        snprintf(buf, 40, " %s\t%s\t", commands[t].cmd_name, commands[t].args);
        comms_print(buf);
        comms_print(commands[t].syntax);
        terminal->send("\r\n", 2);
    }
    return RET_OK;
}

void ashell_run_boot_cfg()
{
    char filename[MAX_FILENAME_SIZE];

    if (fs_get_boot_cfg_filename(BUILD_TIMESTAMP, filename) == 0) {
        ashell_run_javascript(filename);
    }
}

s32_t ashell_main_state(char *buf, u32_t len)
{
    /* Raw line to be evaluated by JS */
    if (shell.state_flags & kShellEvalJavascript) {
        return ashell_eval_javascript(buf, len);
    }

    /* Capture data into the raw buffer */
    if (shell.state_flags & kShellCaptureRaw) {
        return ashell_raw_capture(buf, len);
    }

    /* Special characters check for ESC, cancel and commands */
    DBG("[BOF]%s[EOF]", buf);
    ashell_check_control(buf, len);

    u32_t argc = ashell_get_argc(buf, len);
    DBG("[ARGS %u]\n", argc);

    if (argc == 0)
        return 0;

    /* Null terminate again, protect the castle */
    buf[len] = '\0';
    buf = ashell_skip_spaces(buf);
    if (buf == NULL)
        return 0;

    /* Tokenize and isolate the command */
    char *next = ashell_get_token_arg(buf);

    /* Begin command */
    if (shell.state_flags & kShellTransferIhex) {
        comms_print("[BCMD]\n");
    }

    for (u8_t t = 0; t < ASHELL_COMMANDS_COUNT; t++) {
        if (strequal(commands[t].cmd_name, buf)) {
            s32_t res = commands[t].cb(next);
            /* End command */
            if (shell.state_flags & kShellTransferIhex) {
                comms_print("[ECMD]\n");
            }
            return res;
        }
    }

    /* Shell didn't recognize the command */
    if (shell.state_flags & kShellTransferIhex) {
        comms_print("[ERRCMD]\n");
    } else {
        comms_printf("%s: command not found. \r\n", buf);
        comms_print("Type 'help' for available commands.\r\n");
    }
    return RET_UNKNOWN;
}

/*************************** Former comms-ashell *****************************/

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
        terminal->send(&tmp, 1);
        c = *pos;
        *(pos++) = tmp;
        tmp = c;
    }

    /* Move cursor back to right place */
    cursor_restore();
}

static void del_char(char *pos, u8_t end)
{
    terminal->send("\b", 1);

    if (end == 0) {
        terminal->send(" ", 1);
        terminal->send("\b", 1);
        return;
    }

    cursor_save();

    while (end-- > 0) {
        *pos = *(pos + 1);
        terminal->send((pos++), 1);
    }

    terminal->send(" ", 1);

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

u32_t terminal_init()
{
    DBG("[SHELL] Init\n");
    ashell_run_boot_cfg();

    return 0;
}

void terminal_error(u32_t error)
{
}

u32_t terminal_process(const char *buf, u32_t len)
{
    static char *shell_line = NULL;
    static u8_t tail = 0;
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
                terminal->send("\t", 1);
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
            if (echo_mode) {
                terminal->send(shell_line + printed, cur - printed);
                terminal->send("\r\n", 2);
            }

            u32_t length = strnlen(shell_line, MAX_LINE);

            ashell_main_state(shell_line, length);

            comms_print("\r\n");
            comms_print(comms_get_prompt());

            cur = end = printed = 0;
            flush_line = false;
            if (terminal_done) {
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
    if (echo_mode) {
        terminal->send(shell_line + printed, cur - printed);
    }

    /* Done processing line */
    if (cur == 0 && end == 0 && shell_line != NULL) {
        DBG("[Free]\n");
        zjs_free(shell_line);
        shell_line = NULL;
    }
    return processed;
}

bool terminal_process_done()
{
    return terminal_done;
}

u32_t terminal_close()
{
    DBG("[SHELL CLOSE]\n");
    ihex_process_start();
    return 0;
}

static struct terminal_config terminal_cfg = {
    .init = terminal_init,
    .error = terminal_error,
    .done = terminal_process_done,
    .close = terminal_close,
    .process = terminal_process,
    .send = uart_write_buf
};

void terminal_start()
{
    terminal_done = false;
    terminal = &terminal_cfg;
}
