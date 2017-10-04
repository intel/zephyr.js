// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __TERM_CMD_H__
#define __TERM_CMD_H__

// We are not expecting to return the command line
#define RET_OK_NO_RET 1

// All went fine, print the shell prompt
#define RET_OK        0
#define RET_ERROR    -1
#define RET_UNKNOWN  -2

/**
 * @brief State of the shell to control the data flow
 * and transactions.
 */
enum {
    kShellTransferRaw = (1 << 0),
    kShellTransferIhex = (1 << 1),
    kShellTransferSnapshot = (1 << 2),
    kShellCaptureRaw = (1 << 3),
    kShellEvalJavascript = (1 << 4),
};

/**
 * @brief Typedef callback for the command
 *
 * @param buf Raw buffer containing the rest of the parameters from the last
 *              command
 * @return RET_OK, RET_ERROR, RET_UNKNOWN
 */
typedef s32_t (*ashell_cmd)(char *buf);

struct ashell_cmd {
    const char *cmd_name;
    const char *args;
    const char *syntax;
    ashell_cmd cb;
};

/**
 * @brief Current state of the shell
 */
struct shell_state_config {
    u32_t state_flags;
};

/**
 * @brief Main callback from the UART containing the line in the buffer for
 *           processing.
 *
 * @param buf Zero terminated string, length of the buffer.
 * @return RET_OK, RET_ERROR, RET_UNKNOWN
 */
s32_t ashell_main_state(char *buf, u32_t len);

/**
 * @brief Gets called when main loop starts and runs the JavaScript file found
 *          in boot.cfg.  If no file exists, it simply exits and ashell starts
 *          normally.
 */
void ashell_run_boot_cfg();

s32_t ashell_help(char *buf);

void comms_set_prompt(const char *prompt);

const char *comms_get_prompt();

void comms_set_echo_mode(bool echo_mode);
bool comms_get_echo_mode();

void terminal_start();

#endif  // __TERM_CMD_H__
