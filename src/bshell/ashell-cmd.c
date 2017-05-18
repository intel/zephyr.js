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
#include <jerry-port.h>
#include "ashell-def.h"
#include "file-utils.h"
#include "script-handler.h"

#ifdef CONFIG_REBOOT
//TODO Waiting for patch https://gerrit.zephyrproject.org/r/#/c/3161/
#ifdef CONFIG_BOARD_ARDUINO_101
#include <qm_init.h>
#endif
#endif

/**
 * @brief State of the shell to control the data flow
 * and transactions.
 */
enum
{
    kShellCommandMode = (1 << 0),
    kShellEvalJavascript = (1 << 1),
    kShellCaptureRaw = (1 << 2),
};

#define TEST_FLAG(flag) ((shell.flags) & (flag))
#define RESET_FLAG(flag) ((shell.flags) &= ~(flag))
#define SET_FLAG(flag)   ((shell.flags) |= (flag))

const char *BUILD_TIMESTAMP = __DATE__ " " __TIME__ "\n";

static const char ashell_default_prompt[] = ANSI_FG_YELLOW "$ " ANSI_FG_RESTORE;
static const char raw_prompt[] = ANSI_FG_YELLOW "RAW> " ANSI_FG_RESTORE;
static const char eval_prompt[] = ANSI_FG_GREEN "js> " ANSI_FG_RESTORE;
static const char *ashell_prompt = ashell_default_prompt;

static struct ashell_context {
    uint32_t flags; // shell command context represented with flags
    fs_file_t *fp;  // pointer faked as file pointer (see file_utils.c)
} shell;

static inline void ashell_reset_prompt()
{
    ashell_prompt = ashell_default_prompt;
    SET_FLAG(kShellCommandMode);
}

static inline void ashell_set_prompt_raw()
{
    ashell_prompt = raw_prompt;
}

static inline void ashell_set_prompt_eval()
{
    ashell_prompt = eval_prompt;
}

void ashell_send_prompt()
{
    if (ECHO_MODE()) {
        PRINT(ashell_prompt);
    }
}

/** @brief Copy the next argument into the string
 *
 * @param str     Input: command line.
 * @param tsize   Input: max size for token buffer.
 * @param token   Output: the resulting next token, null-terminated.
 * @param len     Length of the found token.
 * @return        Pointer after this token in the command line.
 */
char *ashell_next_token(char *str, size_t tsize,
                              char *token, size_t *len)
{
    /* Skip leading spaces */
    for(*token = '\0', *len = 0;
        str && *str != 0 && (*str == ' ' || *str == '\t');
        str++);

    /* Copy until first space or end of string */
    for(; tsize-- > 0 && str && *str != ' ' && *str != '\t' && *str != '\0';
        *(token++) = *str++, (*len)++);

    *token = '\0';
    return str;
}

/** @brief Check a buffer for a parameter
 * Parameters are single characters in a '-xyz' sequence
 *
 * @param buf          Null terminated string
 * @param option       Token we are looking for
 * @return bool        Returns if the parameter is on the string
 */
bool ashell_check_cmdline_option(const char *buf, const char option)
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

        if (token && byte == option) {
            return true;
        }
        t++;
    }
    return false;
}
/* =============================================================== */

size_t ashell_get_filename(char **buf, char *dest)
{
    size_t arg_len = 0;
    size_t len = strnlen(*buf, MAX_FILENAME_SIZE);
    if (len == 0 || !*buf || (*buf)[0] == '-')
        return 0;

    INFO("[DBG] next_token input: %s\r\n", *buf);
    *buf = ashell_next_token(*buf, len, dest, &arg_len);
    INFO("[DBG] next_token output: %s, len: %d.\r\n", dest, arg_len);
    INFO("[DBG] remaining command line: %s\r\n", *buf);

    if (arg_len == 0 || arg_len > MAX_FILENAME_SIZE) {
        *dest = '\0';
        ERROR("Expected file name in 8.3 format.\r\n");
        return 0;
    }

    return arg_len;
}

void ashell_clear()
{
    PRINT("\x1b[2J\x1b[H");
}

/**
 * Save consecutive lines to a file specified by the argument until Ctrl+D.
 */
void ashell_save(char *args)  // Former ashell_read_data()
{
    char filename[MAX_FILENAME_SIZE];
    if (ashell_get_filename(&args, filename) <= 0) {
        return;
    }

    if (!fs_valid_filename(filename)) {
        return;
    }

    if (!(shell.fp = fs_open_alloc(filename, "w+"))) {
        return;
    }

    if (ECHO_MODE()) {
        PRINTF("Saving to '%s'.\r\n", filename);
        PRINT("Press Ctrl+Z to finish, Ctrl+X to cancel.\r\n");
    }

    SET_FLAG(kShellCaptureRaw);
    ashell_set_prompt_raw();
    return;
}

void ashell_print_file(char *buf)
{
    char filename[MAX_FILENAME_SIZE];
    char data[ASHELL_MAX_LINE_LEN];
    size_t count;
    size_t line = 1;

    // Show not printing
    bool hidden = ashell_check_cmdline_option(buf, 'v');
    bool lines = ashell_check_cmdline_option(buf, 'n');

    if (ashell_get_filename(&buf, filename) <= 0) {
        return;
    }

    fs_file_t *file = fs_open_alloc(filename, "r");
    if (!file) {
        ERROR("Cannot open file %s.\r\n", filename);
        return;
    }

    size_t size = fs_size(file);
    if (size == 0) {
        PRINT("Empty file.\r\n");
        fs_close_alloc(file);
        return;
    }

    fs_seek(file, 0, SEEK_SET);
    if (lines)
        PRINTF("%5d  ", line++);

    do {
        count = fs_read(file, data, 4);
        for (int t = 0; t < count; t++) {
            uint8_t byte = data[t];
            if (byte == '\n' || byte == '\r') {
                SEND("\r\n", 2);
                if (lines)
                    PRINTF("%5d  ", line++);

            } else {
                if (hidden && !isprint(byte)) {
                    PRINTF("(%x)", byte);
                } else
                    PRINTCH(byte);
            }
        }
    } while (count > 0);

    SEND("\r\n", 2);
    fs_close_alloc(file);
}

// ashell_js_immediate_mode
void ashell_eval_js(char *input)
{
    PRINT("Evaluate JavaScript. Press Ctrl+D to return to shell.\r\n");
    SET_FLAG(kShellEvalJavascript);
    ashell_set_prompt_eval();
}

void ashell_eval_capture(const char *buf)
{
    const char *src = buf;
    uint32_t len = strnlen(buf, ASHELL_MAX_LINE_LEN);

    while (len > 0) {
        uint8_t byte = *buf++;
        if (!isprint(byte)) {
            switch (byte) {
            case ASCII_END_OF_TRANS:
            // case ASCII_SUBSTITUTE:
            // case ASCII_END_OF_TEXT:
            // case ASCII_CANCEL:
                RESET_FLAG(kShellEvalJavascript);
                ashell_reset_prompt();
                return;
            }
        }
        len--;
    }

    javascript_eval(src, strnlen(src, len));
}

// ashell_parse_javascript
void ashell_parse_js(char *buf)
{
    char filename[MAX_FILENAME_SIZE];
    if (ashell_get_filename(&buf, filename) <= 0) {
        return;
    }

    javascript_parse(filename);
}

// ashell_run_javascript
void ashell_run_js(char *buf)
{
    char filename[MAX_FILENAME_SIZE];
    if (ashell_get_filename(&buf, filename) <= 0) {
        return;
    }
    javascript_run(filename);
}

// ashell_stop_javascript()
void ashell_stop_js(char *buf)
{
    javascript_stop();
}

// Former ashell_set_bootcfg
void ashell_bootfile(char *file_name)
{
    if (!fs_exist(file_name)) {
        ERROR("File %s doesn't exist.\r\n", file_name);
        return;
    }

    fs_file_t *file = fs_open_alloc("boot.cfg", "w+");
    if (!file) {
        ERROR("Failed to create boot.cfg file.\r\n");
        return;
    }

    size_t written = fs_write(file, BUILD_TIMESTAMP, strlen(BUILD_TIMESTAMP));
    written += fs_write(file, file_name, strlen(file_name));
    if (written <= 0) {
        ERROR("Failed to write boot.cfg file.\r\n");
    }

    fs_close_alloc(file);
    return;
}

void ashell_file_size(char *buf)
{
    char filename[MAX_FILENAME_SIZE];
    if (ashell_get_filename(&buf, filename) <= 0) {
        return;
    }

    fs_file_t *file = fs_open_alloc(filename, "r");
    if (file == NULL) {
        ERROR("File %s not found.", filename);
        return;
    }

    size_t size = fs_size(file);
    fs_close_alloc(file);
    PRINTF("%5d %s\n", (unsigned int)size, filename);
}

void ashell_list_dir(char *buf)
{
    char filename[MAX_FILENAME_SIZE];
    static struct fs_dirent entry;
    int32_t res;
    fs_dir_t dp;

    *filename = '\0';
    if (ashell_get_filename(&buf, filename) > 0) {
        if (!fs_stat(filename, &entry)) {
            if (entry.type == FS_DIR_ENTRY_DIR) {
                ashell_file_size(filename);
                return;
            }
        } else {
            PRINTF("Cannot access %s.\n", filename);
            return;
        }
    }

    res = fs_opendir(&dp, filename);
    if (res) {
        PRINTF("Error opening dir [%d].\n", (int)res);
        return;
    }

    PRINTF(ANSI_FG_LIGHT_BLUE "      .\n      ..\n" ANSI_FG_RESTORE);

    for (;;) {
        res = fs_readdir(&dp, &entry);

        /* entry.name[0] == 0 means end-of-dir */
        if (res || entry.name[0] == 0) {
            break;
        }
        if (entry.type == FS_DIR_ENTRY_DIR) {
            PRINTF(ANSI_FG_LIGHT_BLUE "%s\n" ANSI_FG_RESTORE, entry.name);
        } else {
            char *p = entry.name;
            for (; *p; ++p)
                *p = tolower((int)*p);

            PRINTF("%5u %s\n", (unsigned int)entry.size, entry.name);
        }
    }

    fs_closedir(&dp);
}

void ashell_remove_file(char *buf)
{
    char filename[MAX_FILENAME_SIZE];
    if (ashell_get_filename(&buf, filename) <= 0) {
        return;
    }

    if (fs_unlink(filename)) {
        ERROR("Cannot remove %s.\r\n", filename);
    }
}

void ashell_move_file(char *args)
{
    static struct fs_dirent entry;
    char path_org[MAX_FILENAME_SIZE];
    char path_dest[MAX_FILENAME_SIZE];

    if (ashell_get_filename(&args, path_org) > 0) {
        /* Check if file or directory */
        if (fs_stat(path_org, &entry)) {
            PRINTF("mv: cannot access '%s' no such file or directory\n", path_org);
            return;
        }
    }
    if (ashell_get_filename(&args, path_dest) > 0) {
        /* Check if file or directory */
        if (!fs_stat(path_dest, &entry)) {
            ERROR("File %s already exists.\r\n", path_dest);
            return;
        }
        if (!fs_valid_filename(path_dest)) {
            ERROR("Invalid filename %s.\r\n", path_dest);
            return;
        }
    }

    f_rename(path_org, path_dest);
}

static inline void ashell_close_capture()
{
    fs_close_alloc(shell.fp);
    shell.fp = NULL;
}

static inline void ashell_discard_capture()
{
    fs_close_alloc(shell.fp);
    shell.fp = NULL;
}

/**
 * Called in 'save' (raw transfer) mode for each line.
 */
bool ashell_raw_capture(const char *input)
{
    uint8_t eol = '\n';
    uint32_t len = strnlen(input, ASHELL_MAX_LINE_LEN);

    while (len > 0) {
        uint8_t byte = *input++;
        if (!isprint(byte)) {
            switch (byte) {
            case ASCII_END_OF_TRANS:
            case ASCII_SUBSTITUTE:
                if (ECHO_MODE()) {
                    PRINT("File saved.\r\n");
                }
                RESET_FLAG(kShellCaptureRaw);
                ashell_reset_prompt();
                ashell_close_capture();
                return true;
            case ASCII_END_OF_TEXT:
            case ASCII_CANCEL:
                if (ECHO_MODE()) {
                    PRINT("Aborted.\r\n");
                }
                RESET_FLAG(kShellCaptureRaw);
                ashell_reset_prompt();
                ashell_discard_capture();
                return true;
            case ASCII_CR:
            case ASCII_NL:
                if (ECHO_MODE()) {
                    PRINT("\r\n");
                }
                break;
            default:
                if (ECHO_MODE()) {
                    PRINTF("%c", byte);
                }
                break;
            }
        } else {
            size_t written = fs_write(shell.fp, &byte, 1);
            if (written == 0) {
                return false;
            }
        }
        len--;
    }

    fs_write(shell.fp, &eol, 1);
    return true;
}

void ashell_reboot(char *buf)
{
    PRINT("Rebooting now!\r\n");

#ifdef CONFIG_REBOOT
    //TODO Waiting for patch https://gerrit.zephyrproject.org/r/#/c/3161/
    #ifdef CONFIG_BOARD_ARDUINO_101
    QM_SCSS_PMU->rstc |= QM_COLD_RESET;
    #endif
#endif
    sys_reboot(SYS_REBOOT_COLD);
}

/**
 * @brief ashell command, handler and help definitions
 */
void ashell_help();

static const struct
{
    const char *cmd;
    void (*handler)(char *);
    const char *help;
} commands[] = {
    { "help",   ashell_help,         "This help" },
    { "clear",  ashell_clear,        "Clear the terminal screen" },
    { "save",   ashell_save,         "<FILE> Save input to file" },
    { "parse",  ashell_parse_js,     "<FILE> Parse JavaScript file" },
    { "run",    ashell_run_js,       "<FILE> Run JavaScript file" },
    { "stop",   ashell_stop_js,      "Stop current JavaScript execution" },
    { "bootf",  ashell_bootfile,     "<FILE> Set JavaScript file run at boot" },
    { "eval",   ashell_eval_js,      "Evaluate JavaScript" },
    { "ls",     ashell_list_dir,     "List directory contents" },
    { "cat",    ashell_print_file,   "<FILE> [-nv] Print file" },
    { "fsize",  ashell_file_size,    "<FILE> Print file size" },
    { "rm",     ashell_remove_file,  "<FILE> Remove file" },
    { "mv",     ashell_move_file,    "<SOURCE> <DEST> Rename file" },
    // { "mkdir",  ashell_make_dir,     "[DIR] Create directory" },
    // { "rmdir",  ashell_remove_dir,   "[DIR] Remove empty directory" },
    { "reboot", ashell_reboot,       "Reboot the device" }
};

#define ASHELL_CMD_COUNT (sizeof(commands)/sizeof(*commands))

void ashell_help()
{
    PRINT("Commands:\r\n");
    for (uint32_t i = 0; i < ASHELL_CMD_COUNT; i++) {
        PRINTF("%8s %s\r\n", commands[i].cmd, commands[i].help);
    }
}

/**
 * Parse a single command line.
 */
void ashell_process_line(char *line, size_t len)
{
    INFO("[DBG] process_line: '%s'.\r\n", line);

    if (!line || !*line) {
        ashell_send_prompt();
        return;
    }

    if (TEST_FLAG(kShellEvalJavascript)) {
        INFO("[DBG] Eval JS\r\n");
        ashell_eval_capture(line);  // evaluate raw line
        ashell_send_prompt();
        return;
    }

    if (TEST_FLAG(kShellCaptureRaw)) {
        INFO("[DBG] Capture file\r\n");
        ashell_raw_capture(line);  // save line in file
        ashell_send_prompt();
        return;
    }

    /* Regular command and argument(s) */
    static char token[ASHELL_MAX_CMD_LEN + 1];
    size_t token_len;
    line = ashell_next_token(line, len, token, &token_len);
    for (uint8_t i = 0; i < ASHELL_CMD_COUNT; i++) {
        if (!strncmp(commands[i].cmd, token, ASHELL_MAX_CMD_LEN)) {
            commands[i].handler(line);
            ashell_send_prompt();
            return;
        }
    }
    ERROR("Command %s not found.\r\n", token);
    // ERROR("Type 'help' for available commands.\r\n");
    ashell_send_prompt();
}

// =====================================================
/**
 * Run the configured JavaScript file after reboot.
 * ashell_run_boot_cfg()
 */
void ashell_bootstrap_js()
{
    fs_file_t *file;
    if (!(file = fs_open_alloc("boot.cfg", "r")))
        return;

    size_t count;
    size_t tssize = strlen(BUILD_TIMESTAMP);
    size_t size = fs_size(file);
    // Check if there is something after the timestamp
    if (size > tssize) {
        char ts[tssize];
        count = fs_read(file, ts, tssize);
        if (count == tssize && strncmp(ts, BUILD_TIMESTAMP, tssize) == 0) {
            size_t filenamesize = size - tssize;
            char filename[filenamesize + 1];
            count = fs_read(file, filename, filenamesize);

            if (count > 0) {
                filename[filenamesize] = '\0';
                ashell_run_js(filename);
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

#if 0
/**
 *  Return the first relevant control character in the line, or 0 if none found.
 */
int8_t ashell_check_control_chars(const char *line)
{
    uint32_t len = ASHELL_MAX_LINE_LEN;
    for (uint32_t byte = *buf++; byte && len; byte = *buf++, len--) {
        if (!isprint(byte)) {
            switch (byte) {
                case ASCII_SUBSTITUTE:  // Ctrl+Z
                    return byte;
                case ASCII_END_OF_TRANS:  // Ctrl+D
                    return byte;
            }
        }
    }
    return 0;
}
#endif
