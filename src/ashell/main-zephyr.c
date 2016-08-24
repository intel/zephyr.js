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

#include <zephyr.h>
#include <misc/shell.h>
#include <misc/printk.h>
#include <string.h>

#include "jerry-api.h"
#include "acm-uart.h"

static int shell_cmd_version(int argc, char *argv[])
{
    uint32_t version = sys_kernel_version_get();
    printk("Jerryscript API %d.%d\n", JERRY_API_MAJOR_VERSION, JERRY_API_MINOR_VERSION);
    printk("Zephyr version %d.%d.%d\n", (int)SYS_KERNEL_VER_MAJOR(version),
        (int)SYS_KERNEL_VER_MINOR(version),
        (int)SYS_KERNEL_VER_PATCHLEVEL(version));
    return 0;
}

static int shell_clear(int argc, char *argv[])
{
    printk(ANSI_CLEAR);
    acm_clear();
    return 0;
}

static int shell_status(int argc, char *argv[])
{
    acm_print_status();
    return 0;
}

#define SHELL_COMMAND(name,cmd) { name, cmd }

static const struct shell_cmd commands[] =
{
  SHELL_COMMAND("version", shell_cmd_version),
  SHELL_COMMAND("clear", shell_clear),
  SHELL_COMMAND("status", shell_status),
  SHELL_COMMAND(NULL, NULL)
};

#ifdef ASHELL_MAIN
void main(void)
#else
void main_development_shell(void)
#endif
{
    shell_init(system_get_prompt(), commands);
    /* Don't call jerry_cleanup() here, as shell_init() returns after setting
       up background task to process shell input, and that task calls
       shell_cmd_handler(), etc. as callbacks. This processing happens in
       the infinite loop, so JerryScript doesn't need to be de-initialized. */
}
