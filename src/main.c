// Copyright (c) 2016-2017, Intel Corporation.

// C includes
#include <string.h>

#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include "zjs_zephyr_port.h"
#include <zephyr.h>
#if defined(BUILD_MODULE_BLE) || (CONFIG_NET_L2_BT)
#include <bluetooth/bluetooth.h>
#ifdef CONFIG_NET_L2_BT
#include <bluetooth/storage.h>
#include <gatt/ipss.h>
#include "zjs_net_config.h"
#endif
#endif
#else
#include "zjs_linux_port.h"
#endif  // ZJS_LINUX_BUILD
#include "zjs_script.h"
#include "zjs_util.h"
#if defined (ZJS_ASHELL) || defined (ZJS_DYNAMIC_LOAD)
#include <gpio.h>
#include "zjs_board.h"
#include "ashell/file-utils.h"
#ifdef ZJS_ASHELL
#include "ashell/ashell.h"
#endif // ZJS_ASHELL
#endif // defined (ZJS_ASHELL) || defined (ZJS_DYNAMIC_LOAD)

// JerryScript includes
#include "jerryscript.h"

// Platform agnostic modules/headers
#include "zjs_callbacks.h"
#include "zjs_error.h"
#include "zjs_modules.h"
#ifdef BUILD_MODULE_SENSOR
#include "zjs_sensor.h"
#endif
#include "zjs_timers.h"
#ifdef BUILD_MODULE_OCF
#include "zjs_ocf_common.h"
#endif
#ifdef BUILD_MODULE_BLE
#include "zjs_ble.h"
#endif
#ifdef ZJS_LINUX_BUILD
#include "zjs_unit_tests.h"
#endif
#ifdef CONFIG_BOARD_ARDUINO_101
#include "zjs_ipm.h"
#endif

#define ZJS_MAX_PRINT_SIZE 512

#ifdef ZJS_SNAPSHOT_BUILD
const uint32_t snapshot_bytecode[] = {
#include "zjs_snapshot_gen.h"
};
const size_t snapshot_len = sizeof(snapshot_bytecode);
#else
const char script_jscode[] = {
#include "zjs_script_gen.h"
};
#endif

#ifdef ZJS_ASHELL
static bool ashell_mode = false;
#endif

#ifdef ZJS_DEBUGGER
static bool start_debug_server = false;
static uint16_t debug_port = 5001;
#endif

#ifdef ZJS_LINUX_BUILD
// enabled if --noexit is passed to jslinux
static u8_t no_exit = 0;
// if > 0, jslinux will exit after this many milliseconds
static u32_t exit_after = 0;
static struct timespec exit_timer;

u8_t process_cmd_line(int argc, char *argv[])
{
    int i;
    for (i = 0; i < argc; ++i) {
        if (!strncmp(argv[i], "--unittest", 10)) {
            // run unit tests
            zjs_run_unit_tests();
        } else if (!strncmp(argv[i], "--debugger", 10)) {
#ifdef ZJS_DEBUGGER
            // run in debugger
            start_debug_server = true;
#else
            ERR_PRINT("Debugger disabled, rebuild with DEBUGGER=on");
            return 0;
#endif
        } else if (!strncmp(argv[i], "--noexit", 8)) {
            no_exit = 1;
        } else if (!strncmp(argv[i], "-t", 2)) {
            if (i == argc - 1) {
                // no time argument, return error
                ERR_PRINT("no time argument given after '-t'\n");
                return 0;
            } else {
                char *str_time = argv[i + 1];
                exit_after = atoi(str_time);
                ZJS_PRINT("jslinux will terminate after %u milliseconds\n",
                          (unsigned int)exit_after);
                clock_gettime(CLOCK_MONOTONIC, &exit_timer);
            }
        }
    }
    return 1;
}
#else
#ifndef CONFIG_NET_APP_AUTO_INIT
#ifdef BUILD_MODULE_BLE
extern void ble_bt_ready(int err);
#endif
#endif
#endif

#ifdef ZJS_ASHELL
static bool config_mode_detected()
{
#ifdef IDE_GPIO_PIN
    char devname[20];
    int pin;

    ZVAL pin_val = jerry_create_number(IDE_GPIO_PIN);
    int rval = zjs_board_find_pin(pin_val, devname, &pin);
    if (rval == FIND_PIN_INVALID) {
        ERR_PRINT("bad pin argument");
        return false;
    }

    struct device *gpiodev = device_get_binding(devname);
    if (!gpiodev) {
        ERR_PRINT("GPIO device not found\n");
        return false;
    }

    int flags = 0;
    flags |= GPIO_POL_INV | GPIO_DIR_IN;
    rval = gpio_pin_configure(gpiodev, pin, flags);
    if (rval) {
        ERR_PRINT("invalid GPIO pin %d\n", pin);
        return false;
    }

    u32_t value;
    rval = gpio_pin_read(gpiodev, pin, &value);
    if (rval) {
        ERR_PRINT("gpio read failed\n");
        return false;
    }

    return (value == 1);
#else
    // always enter IDE mode when IDE_GPIO_PIN is not specified
    DBG_PRINT("IDE_GPIO_PIN not set\n");
    return true;
#endif
}
#endif

#ifndef ZJS_LINUX_BUILD
void main(void)
#else
int main(int argc, char *argv[])
#endif
{
#ifndef ZJS_SNAPSHOT_BUILD
    char *file_name = NULL;
    size_t file_name_len = 0;
#ifdef ZJS_LINUX_BUILD
    char *script = NULL;
    if (argc < 2) {
        ZJS_PRINT("usage: jslinux [--unittests] [path/to/file.js]\n");
        return 1;
    }

    file_name = argv[1];
    file_name_len = strlen(argv[1]);
#elif defined ZJS_ASHELL || defined ZJS_DYNAMIC_LOAD
    char *script = NULL;
#else
    const char *script = NULL;
#endif
    jerry_value_t code_eval;
    u32_t script_len = 0;
#endif
#ifndef ZJS_LINUX_BUILD
    DBG_PRINT("Main Thread ID: %p\n", (void *)k_current_get());
    zjs_loop_init();
#endif
    jerry_value_t result;

    // print newline here to make it easier to find
    // the beginning of the program
    ZJS_PRINT("\n");

    jerry_init(JERRY_INIT_EMPTY);
    // initialize modules
    zjs_modules_init();

#ifdef BUILD_MODULE_OCF
    zjs_register_service_routine(NULL, main_poll_routine);
#endif

#ifdef ZJS_ASHELL
if (config_mode_detected()) {
    // go into IDE mode if connected GPIO button is pressed
    ashell_mode = true;
} else {
    // boot to cfg file if found
    char filename[MAX_FILENAME_SIZE];
    if (fs_get_boot_cfg_filename(NULL, filename) == 0) {
        // read JS stored in filesystem
        fs_file_t *js_file = fs_open_alloc(filename, "r");
        if (!js_file) {
            ZJS_PRINT("\nFile %s not found on filesystem, \
                      please boot into IDE mode, exiting!\n",
                      filename);
            goto error;
        }
        script_len = fs_size(js_file);
        script = zjs_malloc(script_len + 1);
        ssize_t count = fs_read(js_file, script, script_len);
        if (script_len != count) {
            ZJS_PRINT("\nfailed to read JS file\n");
            zjs_free(script);
            goto error;
        }
        script[script_len] = '\0';
        ZJS_PRINT("JS boot config found, booting JS %s...\n\n\n", filename);
    } else {
        // boot cfg file not found
        ZJS_PRINT("\nNo JS found, please boot into IDE mode, exiting!\n");
        goto error;
    }
}
#endif

#ifndef ZJS_SNAPSHOT_BUILD
#ifdef ZJS_LINUX_BUILD
    if (argc > 1) {
        if (process_cmd_line(argc - 1, argv + 1) == 0) {
            ERR_PRINT("command line options error\n");
            goto error;
        }
        if (zjs_read_script(argv[1], &script, &script_len)) {
            ERR_PRINT("could not read script file %s\n", argv[1]);
            goto error;
        }
    } else
    // slightly tricky: reuse next section as else clause
#endif
    {
#ifdef ZJS_LINUX_BUILD
        script = zjs_malloc(script_len + 1);
        memcpy(script, script_jscode, script_len);
        script[script_len] = '\0';
#else
#ifndef ZJS_ASHELL
        script_len = strnlen(script_jscode, MAX_SCRIPT_SIZE);
        script = script_jscode;
#endif
#endif
        if (script_len == MAX_SCRIPT_SIZE) {
            ERR_PRINT("Script size too large! Increase MAX_SCRIPT_SIZE.\n");
            goto error;
        }
    }
#endif

#ifdef ZJS_DEBUGGER
if (start_debug_server) {
    jerry_debugger_init(debug_port);
}
#endif

#ifndef ZJS_SNAPSHOT_BUILD
    code_eval = jerry_parse_named_resource((jerry_char_t *)file_name,
                                           file_name_len,
                                           (jerry_char_t *)script,
                                           script_len,
                                           false);

    if (jerry_value_has_error_flag(code_eval)) {
        DBG_PRINT("Error parsing JS\n");
        zjs_print_error_message(code_eval, ZJS_UNDEFINED);
        goto error;
    }
#endif

#ifdef ZJS_LINUX_BUILD
    zjs_free(script);
#endif

#ifdef ZJS_SNAPSHOT_BUILD
    result = jerry_exec_snapshot(snapshot_bytecode, snapshot_len, false);
#else
#ifdef ZJS_DEBUGGER
    ZJS_PRINT("Debugger mode: connect using jerry-client-ws.py\n");
#endif
    result = jerry_run(code_eval);
#endif

    if (jerry_value_has_error_flag(result)) {
        DBG_PRINT("Error running JS\n");
        zjs_print_error_message(result, ZJS_UNDEFINED);
        goto error;
    }

#ifndef ZJS_LINUX_BUILD
#ifndef ZJS_ASHELL  // Ashell will call bt_enable when module is loaded

#ifndef CONFIG_NET_APP_AUTO_INIT  // net_app will call bt_enable() itself
    int err = 0;
#ifdef BUILD_MODULE_BLE
    err = bt_enable(ble_bt_ready);
#elif CONFIG_NET_L2_BT
    err = bt_enable(NULL);
#endif
    if (err) {
       ERR_PRINT("Failed to enable Bluetooth, error %d\n", err);
       goto error;
    }
#endif

#ifdef CONFIG_NET_L2_BT
    ipss_init();
    ipss_advertise();
    net_ble_enabled = 1;
#endif
#endif
#endif

    // NOTE: don't use ZVAL on these because we want to release them early, so
    //   they don't stick around for the lifetime of the app
#ifndef ZJS_SNAPSHOT_BUILD
    jerry_release_value(code_eval);
#endif

    jerry_release_value(result);

#ifdef ZJS_LINUX_BUILD
    u8_t last_serviced = 1;
#endif
#ifdef ZJS_ASHELL
    if (ashell_mode) {
        DBG_PRINT("Config mode detected, booting into the IDE...\n\n\n");
        zjs_ashell_init();
    }
#endif
    while (1) {
#ifdef ZJS_DYNAMIC_LOAD
	// Check if we should load a new JS file
        zjs_modules_check_load_file();
#endif
#ifdef ZJS_ASHELL
        if (ashell_mode) {
            zjs_ashell_process();
        }
#endif
        s32_t wait_time = ZJS_TICKS_FOREVER;
        u8_t serviced = 0;

        // callback cannot return a wait time
        if (zjs_service_callbacks()) {
            // when this was only called at the end, if a callback created a
            //   timer, it would think there were no timers and block forever
            // FIXME: need to consider the chicken and egg problems here
            serviced = 1;
        }
#ifdef ZJS_LINUX_BUILD
	// FIXME - reverted patch #1542 to old timer implementation
        u64_t wait = zjs_timers_process_events();
        if (wait != ZJS_TICKS_FOREVER) {
            serviced = 1;
            wait_time = (wait < wait_time) ? wait : wait_time;
        }
        wait = zjs_service_routines();
#else
        u64_t wait = zjs_service_routines();
#endif
        if (wait != ZJS_TICKS_FOREVER) {
            serviced = 1;
            wait_time = (wait < wait_time) ? wait : wait_time;
        }
        // callback cannot return a wait time
        if (zjs_service_callbacks()) {
            serviced = 1;
        }

#ifdef BUILD_MODULE_PROMISE
        // run queued jobs for promises
        result = jerry_run_all_enqueued_jobs();
        if (jerry_value_has_error_flag(result))
        {
            DBG_PRINT("Error running JS in promise jobqueue\n");
            zjs_print_error_message(result, ZJS_UNDEFINED);
            goto error;
        }
#endif

#ifndef ZJS_LINUX_BUILD
        zjs_loop_block(wait_time);
#endif
#ifdef ZJS_LINUX_BUILD
        if (!no_exit) {
            // if the last and current loop had no pending "events" (timers or
            // callbacks) and --autoexit is enabled the program will terminate
            if (last_serviced == 0 && serviced == 0) {
                ZJS_PRINT("\njslinux: no more timers or callbacks found, exiting!\n");
                ZJS_PRINT("   * to run your script indefinitely, use --noexit\n");
                ZJS_PRINT("   * to run your script for a set timeout, use -t <ms>\n");
                return 0;
            }
        }
        if (exit_after != 0) {
            // an exit timeout was passed in
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            u32_t elapsed = (1000 * (now.tv_sec - exit_timer.tv_sec)) +
                ((now.tv_nsec / 1000000) - (exit_timer.tv_nsec / 1000000));
            if (elapsed >= exit_after) {
                ZJS_PRINT("%u milliseconds have passed, exiting!\n",
                          (unsigned int)elapsed);
                return 0;
            }
        }
        last_serviced = serviced;
#endif
    }
error:
#ifdef ZJS_LINUX_BUILD
    return 1;
#else
    return;
#endif
}
