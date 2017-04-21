// Copyright (c) 2016-2017, Intel Corporation.

/**
 * @file
 * @brief Shell to keep the different states of the machine
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <atomic.h>
#include <misc/printk.h>
#include <misc/reboot.h>
#include <ctype.h>
#include "jerryscript-port.h"
#include "file-utils.h"

#include "comms-uart.h"
#include "comms-shell.h"
#include "shell-state.h"

#include "ihex-handler.h"
#include "jerry-code.h"

#ifndef CONFIG_USB_CDC_ACM
#define CONFIG_IDE
#endif

#ifdef CONFIG_REBOOT
//TODO Waiting for patch https://gerrit.zephyrproject.org/r/#/c/3161/
#ifdef CONFIG_BOARD_ARDUINO_101
#include <qm_init.h>
#endif
#endif

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

const char READY_FOR_RAW_DATA[] =
    "Ready for JavaScript. \r\n" \
    "\tCtrl+Z to finish transfer.\r\n" \
    "\tCtrl+X to cancel.\r\n";

const char MSG_IMMEDIATE_MODE[] =
    "Ready to evaluate JavaScript.\r\n" \
    "\tCtrl+D to return to shell.\r\n";

const char hex_prompt[] = "[HEX]\r\n";
const char raw_prompt[] = ANSI_FG_YELLOW "RAW> " ANSI_FG_RESTORE;
const char eval_prompt[] = ANSI_FG_GREEN "js> " ANSI_FG_RESTORE;
const char *BUILD_TIMESTAMP = __DATE__ " " __TIME__ "\n";

#define CMD_TRANSFER "transfer"

#define READ_BUFFER_SIZE 80

#ifndef CONFIG_IHEX_UPLOADER_DEBUG
#define DBG(...) { ; }
#else
#define DBG printk
#endif /* CONFIG_IHEX_UPLOADER_DEBUG */

int32_t ashell_get_filename_buffer(const char *buf, char *destination)
{
    uint32_t arg_len = 0;
    uint32_t len = strnlen(buf, MAX_FILENAME_SIZE);
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

int32_t ashell_remove_file(char *buf)
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

int32_t ashell_remove_dir(char *buf)
{
    return RET_OK;
}

int32_t ashell_make_dir(char *buf)
{
    return RET_OK;
}

int32_t ashell_disk_usage(char *buf)
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

int32_t ashell_rename(char *buf)
{
    static struct fs_dirent entry;
    char path_org[MAX_FILENAME_SIZE];
    char path_dest[MAX_FILENAME_SIZE];

    if (ashell_get_filename_buffer(buf, path_org) > 0) {
        /* Check if file or directory */
        if (fs_stat(path_org, &entry)) {
            comms_printf("mv: cannot access '%s' no such file or directory\n", path_org);
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
            comms_printf("mv: cannot access '%s' file already exists\n", path_dest);
            return RET_ERROR;
        }
        if (!fs_valid_filename(path_dest)) {
            return RET_ERROR;
        }
    }

    f_rename(path_org, path_dest);
    return RET_OK;
}

int32_t ashell_error(char *buf)
{
    printk("[Error](%s)\n", buf);
    jerry_port_log(JERRY_LOG_LEVEL_ERROR, "stderr test (%s)\n", buf);
    return 0;
}

int32_t ashell_reboot(char *buf)
{
#ifdef CONFIG_REBOOT
    //TODO Waiting for patch https://gerrit.zephyrproject.org/r/#/c/3161/
    #ifdef CONFIG_BOARD_ARDUINO_101
    QM_SCSS_PMU->rstc |= QM_COLD_RESET;
    #endif
#endif
    sys_reboot(SYS_REBOOT_COLD);
    return RET_OK;
}

int32_t ashell_list_dir(char *buf)
{
    char filename[MAX_FILENAME_SIZE];
    static struct fs_dirent entry;
    int32_t res;
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
            printf("ls: cannot access %s: no such file or directory\n", filename);
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

            int filesizeStrLen = snprintf(filesizeStr, 16, "%d", (int)entry.size);
            comms_write_buf(filesizeStr, filesizeStrLen);
            comms_write_buf("\t", 1);
            comms_print(entry.name);
            comms_write_buf("\r\n", 2);
        }
    }

    fs_closedir(&dp);
    return RET_OK;
}

int32_t ashell_print_file(char *buf)
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
                comms_write_buf(&data[lineStart], strLen);
                comms_write_buf("\r\n", 2);
                lineStart = t + 1;
            }
        }
        // If we have data left that doesn't end in a newline, print it.
        if (lineStart < count)
        {
            int strLen = count - lineStart;
            comms_write_buf(&data[lineStart], strLen);
        }
        // Reset the line start
        lineStart = 0;
    } while (count > 0);

    comms_write_buf("\r\n", 2);
    fs_close_alloc(file);
    return RET_OK;
}

int32_t ashell_parse_javascript(char *buf)
{
    char filename[MAX_FILENAME_SIZE];
    if (ashell_get_filename_buffer(buf, filename) <= 0) {
        return RET_ERROR;
    }

    javascript_parse_code(filename);
    return RET_OK;
}

int32_t ashell_run_javascript(char *buf)
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

int32_t ashell_start_raw_capture(char *filename)
{
    file_code = fs_open_alloc(filename, "w+");

    /* Error getting an id for our data storage */
    if (!file_code) {
        return RET_ERROR;
    }
    return RET_OK;
}

int32_t ashell_close_capture()
{
    return fs_close_alloc(file_code);
}

int32_t ashell_discard_capture()
{
    fs_close_alloc(file_code);
    //TODO ashell_remove_file(file_code);
    return RET_OK;
}

int32_t ashell_eval_javascript(const char *buf, uint32_t len)
{
    const char *src = buf;

    while (len > 0) {
        uint8_t byte = *buf++;
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

int32_t ashell_raw_capture(const char *buf, uint32_t len)
{
    uint8_t eol = '\n';

    while (len > 0) {
        uint8_t byte = *buf++;
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
                comms_write_buf(buf, 1);
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

int32_t ashell_set_echo_mode(char *buf)
{
    if (!strcmp("on", buf)) {
        comms_print("echo_on");
        comms_set_echo_mode(true);
    } else if (!strcmp("off", buf)) {
        comms_print("echo_off");
        comms_set_echo_mode(false);
    }
    return RET_OK;
}

int32_t ashell_read_data(char *buf)
{
    if (shell.state_flags & kShellTransferIhex) {
        ashell_process_close();
    }
    else {
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
    }
    return RET_OK;
}

int32_t ashell_js_immediate_mode(char *buf)
{
    shell.state_flags |= kShellEvalJavascript;
    comms_print(ANSI_CLEAR);
    comms_print(MSG_IMMEDIATE_MODE);
    comms_set_prompt(eval_prompt);
    return RET_OK;
}

int32_t ashell_set_transfer_state(char *buf)
{
    char *next;
    if (buf == 0) {
        comms_print(ERROR_NOT_ENOUGH_ARGUMENTS);
        return RET_ERROR;
    }
    next = ashell_get_token_arg(buf);
    comms_print(buf);

    if (!strcmp("raw", buf)) {
        comms_set_prompt(NULL);
        shell.state_flags |= kShellTransferRaw;
        shell.state_flags &= ~kShellTransferIhex;
        return RET_OK;
    }

    if (!strcmp("ihex", buf)) {
        comms_set_prompt(hex_prompt);
        shell.state_flags |= kShellTransferIhex;
        shell.state_flags &= ~kShellTransferRaw;
        return RET_OK;
    }
    return RET_UNKNOWN;
}

int32_t ashell_set_state(char *buf)
{
    if (buf == 0) {
        comms_print(ERROR_NOT_ENOUGH_ARGUMENTS);
        return RET_ERROR;
    }

    char *next = ashell_get_token_arg(buf);
    if (!strcmp(CMD_TRANSFER, buf)) {
        return ashell_set_transfer_state(next);
    }

    return RET_UNKNOWN;
}

int32_t ashell_get_state(char *buf)
{
    if (buf == 0) {
        comms_print(ERROR_NOT_ENOUGH_ARGUMENTS);
        return RET_ERROR;
    }

    ashell_get_token_arg(buf);
    if (!strcmp(CMD_TRANSFER, buf)) {
        DBG("Flags %lu\n", shell.state_flags);

        if (shell.state_flags & kShellTransferRaw)
            comms_print("Raw\r\n");

        if (shell.state_flags & kShellTransferIhex)
            comms_print("Ihex\r\n");

        return RET_OK;
    }
    return RET_UNKNOWN;
}

int32_t ashell_at(char *buf)
{
    comms_print("OK\r\n\r\n");
    return RET_OK;
}

int32_t ashell_clear(char *buf)
{
    comms_print(ANSI_CLEAR);
    return RET_OK;
}

int32_t ashell_stop_javascript(char *buf)
{
    javascript_stop();
    return RET_OK;
}

int32_t ashell_check_control(const char *buf, uint32_t len)
{
    while (len > 0) {
        uint8_t byte = *buf++;
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

int32_t ashell_set_bootcfg(char *buf)
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

static const struct ashell_cmd commands[] =
{
    // CMD    ARGS      DESCRIPTION  IMPL
    {"help",  "",       "This help", ashell_help},
    {"eval",  "",       "Evaluate JavaScript in real time",
                          ashell_js_immediate_mode},
    {"load",  "FILE",   IDE_SKIP("Saves the input text into a file"),
                          ashell_read_data},
    {"run",   "FILE",   "Runs the JavaScript program in the file",
                          ashell_run_javascript},
    {"parse", "FILE",   IDE_SKIP("Check if the JS syntax is correct"),
                          ashell_parse_javascript},
    {"stop",  "",       "Stops current JavaScript execution",
                          ashell_stop_javascript},
    {"ls",    "",       "List all files", ashell_list_dir},
    {"cat",   "FILE",   "Print the file contents of a file", ashell_print_file},
    {"du",    "FILE",   IDE_SKIP("Estimate file space usage"),
                          ashell_disk_usage},
    {"rm",    "FILE",   "Remove file or directory", ashell_remove_file},
    {"mv",    "F1 F2",  "Move file F1 to destination F2", ashell_rename},
    {"clear", "",       "Clear the terminal screen", ashell_clear},
    {"boot",  "FILE",   "Set the file that should run at boot",
                          ashell_set_bootcfg},
    {"reboot", "",      "Reboots the device", ashell_reboot},

    // undocumented commands used by IDE
    {"error", "",       "", ashell_error},
    {"echo",  "on/off", "", ashell_set_echo_mode},
    {"set",   "",       "", ashell_set_state},
    {"get",   "",       "", ashell_get_state},
};

#define ASHELL_COMMANDS_COUNT (sizeof(commands)/sizeof(*commands))

int32_t ashell_help(char *buf)
{
    comms_print("'A Shell' bash\r\n\r\n");
    comms_print("Commands list:\r\n");
    for (uint32_t t = 0; t < ASHELL_COMMANDS_COUNT; t++) {
        // skip commands with empty description
        if (!commands[t].syntax[0]) {
            continue;
        }
        char buf[40];
        snprintf(buf, 40, " %s\t%s\t", commands[t].cmd_name, commands[t].args);
        comms_print(buf);
        comms_print(commands[t].syntax);
        comms_write_buf("\r\n", 2);
    }
    return RET_OK;
}

void ashell_run_boot_cfg()
{
    fs_file_t *file;
    size_t count;
    file = fs_open_alloc("boot.cfg", "r");
    // Failed to open boot.cfg
    if (!file) {
        return;
    }

    size_t tssize = strlen(BUILD_TIMESTAMP);
    ssize_t size = fs_size(file);
    // Check that there is something after the timestamp
    if (size > tssize) {
        char ts[tssize];
        count = fs_read(file, ts, tssize);
        if (count == tssize && strncmp(ts, BUILD_TIMESTAMP, tssize) == 0) {
            size_t filenamesize = size - tssize;
            char filename[filenamesize + 1];
            count = fs_read(file, filename, filenamesize);

            if (count > 0) {
                filename[filenamesize] = '\0';
                ashell_run_javascript(filename);
            }
        }
        else {
            // This is a newly flashed board, delete boot.cfg
            ashell_remove_file("boot.cfg");
        }
    }
    else {
        // boot.cfg is invalid, remove it
        ashell_remove_file("boot.cfg");
    }
    fs_close_alloc(file);
}

int32_t ashell_main_state(char *buf, uint32_t len)
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

    uint32_t argc = ashell_get_argc(buf, len);
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

    for (uint8_t t = 0; t < ASHELL_COMMANDS_COUNT; t++) {
        if (!strcmp(commands[t].cmd_name, buf)) {
            int32_t res = commands[t].cb(next);
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
