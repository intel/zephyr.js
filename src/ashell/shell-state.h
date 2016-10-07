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

#ifndef __SHELL__STATE__H__
#define __SHELL__STATE__H__

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
enum
{
    kShellTransferRaw = (1 << 0),
    kShellTransferIhex = (1 << 1),
    kShellTransferSnapshot = (1 << 2),
    kShellCaptureRaw = (1 << 3),
    kShellEvalJavascript = (1 << 4),
};

/**
 * @brief Typedef callback for the command
 *
 * @param buf Raw buffer containing the rest of the parameters from the last command
 * @return RET_OK, RET_ERROR, RET_UNKNOWN
 */
typedef int32_t(*ashell_cmd)(char *buf);

struct ashell_cmd
{
    const char *cmd_name;
    const char *syntax;
    ashell_cmd cb;
};

/**
 * @brief Current state of the shell
 */
struct shell_state_config
{
    uint32_t state_flags;
};

/**
 * @brief Main callback from the UART containing the line in the buffer for processing.
 *
 * @param buf Zero terminated string, length of the buffer.
 * @return RET_OK, RET_ERROR, RET_UNKNOWN
 */
int32_t ashell_main_state(char *buf, uint32_t len);

int32_t ashell_help(char *buf);

#endif
