// Copyright (c) 2016-2018, Intel Corporation.

// C includes
#include <stdlib.h>
#include <string.h>

#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <zephyr.h>
#endif

#ifndef ZJS_LINUX_BUILD
// ZJS includes
#include "zjs_ipm.h"
#include "zjs_zephyr_port.h"
#else
#include "zjs_linux_port.h"
#endif

#include "zjs_callbacks.h"
#include "zjs_modules.h"
#include "zjs_modules_gen.h"
#include "zjs_script.h"
#include "zjs_timers.h"
#include "zjs_util.h"
#include "jerryscript-ext/module.h"
#if defined(ZJS_ASHELL) || defined(ZJS_DYNAMIC_LOAD)
#include "file-utils.h"
#endif

struct routine_map {
    zjs_service_routine func;
    void *handle;
};

#ifdef ZJS_DYNAMIC_LOAD
static char *load_file;
#endif // ZJS_DYNAMIC_LOAD

static u8_t num_routines = 0;
struct routine_map svc_routine_map[NUM_SERVICE_ROUTINES];

/*****************************************************************
*   Real board JavaScript module resolver (ASHELL only currently)
******************************************************************/
#ifndef ZJS_LINUX_BUILD
#if defined(ZJS_ASHELL) || defined(ZJS_DYNAMIC_LOAD)
// Eval the JavaScript, and return the module.
static bool javascript_eval_code(const char *source_buffer, ssize_t size,
                                 jerry_value_t *ret_val)
{
    (*ret_val) = jerry_eval((jerry_char_t *)source_buffer, size, false);
    if (jerry_value_has_error_flag(*ret_val)) {
        ERR_PRINT("failed to evaluate JS\n");
        return false;
    }
    return true;
}
#endif  // defined(ZJS_ASHELL) || defined(ZJS_DYNAMIC_LOAD)

// Find the module on the filestystem
static bool load_js_module_fs(const jerry_value_t module_name,
                              jerry_value_t *result)
{
    // Currently searching the filesystem is only supported on arduino 101
#if defined(ZJS_ASHELL) || defined(ZJS_DYNAMIC_LOAD)
    jerry_size_t module_size = jerry_get_utf8_string_size(module_name) + 1;
    char module[module_size];
    zjs_copy_jstring(module_name, module, &module_size);
    ssize_t file_size = 0;

    // Attempt to read the file from the filesystem.
    char *req_file_buffer = read_file_alloc(module, &file_size);
    if (req_file_buffer == NULL) {
        return false;
    }

    // Eval the code from the file.
    bool ret = javascript_eval_code(req_file_buffer, file_size, result);
    zjs_free(req_file_buffer);
    return ret;
#else
    return false;
#endif  // defined(ZJS_ASHELL) || defined(ZJS_DYNAMIC_LOAD)

}
#else  // ZJS_LINUX_BUILD
/****************************************
*   Linux JavaScript module resolver
*****************************************/
// Find the module on the filestystem
static bool load_js_module_fs(const jerry_value_t module_name,
                              jerry_value_t *result)
{
    // Linux can pass in the script at runtime, so we have to read in/parse any
    // JS modules now rather than at compile time
#ifdef ZJS_SNAPSHOT_BUILD
    DBG_PRINT("Parser disabled, can't check FS for module\n");
    return false;
#endif
    jerry_size_t module_size = jerry_get_utf8_string_size(module_name) + 1;
    char module[module_size];
    zjs_copy_jstring(module_name, module, &module_size);
    jerry_size_t size = MAX_MODULE_STR_LEN;
    char full_path[size + 9];
    char *str = NULL;
    u32_t len;
    bool ret = false;
    sprintf(full_path, "modules/%s", module);
    full_path[size + 8] = '\0';

    if (zjs_read_script(full_path, &str, &len)) {
        return false;
    }

    (*result) = jerry_eval((jerry_char_t *)str, len, false);
    if (jerry_value_has_error_flag(*result)) {
        ERR_PRINT("failed to evaluate JS\n");
        ret = false;
    } else {
        ret = true;
    }
    zjs_free(str);
    return ret;
}
#endif  // !ZJS_LINUX_BUILD

// Try to find the module in the main JS file
static bool load_js_module_obj(const jerry_value_t module_name,
                               jerry_value_t *result)
{
    jerry_size_t module_size = jerry_get_utf8_string_size(module_name) + 1;
    char module[module_size];

    // Get the module name
    zjs_copy_jstring(module_name, module, &module_size);

    // Get the list of currently loaded modules
    ZVAL global_obj = jerry_get_global_object();
    ZVAL modules_obj = zjs_get_property(global_obj, "module");

    if (!jerry_value_is_object(modules_obj)) {
        return false;
    }

    ZVAL exports_obj = zjs_get_property(modules_obj, "exports");
    if (!jerry_value_is_object(exports_obj)) {
        return false;
    }

    // TODO: maybe we should just error if no .js extension here
    char mod_trim[module_size];
    strncpy(mod_trim, module, module_size);
    if (module_size > 3 && !strncmp(mod_trim + module_size - 3, ".js", 3)) {
        // strip the ".js"
        mod_trim[module_size - 3] = '\0';
    }

    (*result) = zjs_get_property(exports_obj, mod_trim);
    if (!jerry_value_is_object(*result)) {
        return false;
    }
    return true;
}

/****************************
*   Jerry module resolvers
*****************************/
static jerryx_module_resolver_t load_fs_resolver =
{
    NULL,
    load_js_module_fs
};

static jerryx_module_resolver_t load_js_resolver =
{
    NULL,
    load_js_module_obj
};

// These execute in order until a matching module is found
static const jerryx_module_resolver_t *resolvers[] =
{
    &jerryx_module_native_resolver,  // Check for a native module
    &load_js_resolver,               // Check for a JS module in the code
    &load_fs_resolver                // Check for a JS module on the FS
};

// native require handler
static ZJS_DECL_FUNC(native_require_handler)
{
    ZJS_VALIDATE_ARGS(Z_STRING);

    // Get the module name
    jerry_size_t module_size = jerry_get_utf8_string_size(argv[0]) + 1;
    if (module_size > MAX_MODULE_STR_LEN) {
        return RANGE_ERROR("module name too long");
    }

    char module[module_size];
    zjs_copy_jstring(argv[0], module, &module_size);

    // Try each of the resolvers to see if we can find the requested module
    jerry_value_t result = jerryx_module_resolve(argv[0], resolvers, 3);
    if (jerry_value_has_error_flag(result)) {
        DBG_PRINT("Couldn't load module %s\n", module);
        return NOTSUPPORTED_ERROR("Module not found");
    } else {
        DBG_PRINT("Module %s loaded\n", module);
    }
    return result;
}

// native eval handler
static ZJS_DECL_FUNC(native_eval_handler)
{
    return zjs_error("eval not supported");
}

// native print handler
static ZJS_DECL_FUNC(native_print_handler)
{
    if (argc < 1 || !jerry_value_is_string(argv[0]))
        return zjs_error("print: missing string argument");

    jerry_size_t size = 0;
    char *str = zjs_alloc_from_jstring(argv[0], &size);
    if (!str)
        return zjs_error("print: out of memory");

    ZJS_PRINT("%s\n", str);
    zjs_free(str);
    return ZJS_UNDEFINED;
}
void zjs_stop_js()
{
    zjs_modules_cleanup();
    zjs_remove_all_callbacks();
#ifdef CONFIG_BOARD_ARDUINO_101
#ifdef CONFIG_IPM
        zjs_ipm_free_callbacks();
#endif // CONFIG_IPM
#endif // CONFIG_BOARD_ARDUINO_101
    jerry_cleanup();
    jerry_init(JERRY_INIT_EMPTY);
    zjs_modules_init();
}

static ZJS_DECL_FUNC(stop_js_handler)
{
    zjs_stop_js();
    return ZJS_UNDEFINED;
}

#ifdef ZJS_LINUX_BUILD
static ZJS_DECL_FUNC(process_exit)
{
    ZJS_VALIDATE_ARGS(Z_OPTIONAL Z_NUMBER);

    int status = 0;

    if (argc > 0) {
        status = jerry_get_number_value(argv[0]);
    }
    ZJS_PRINT("Exiting with code=%d\n", status);
    exit(status);
}
#endif
#ifdef ZJS_DYNAMIC_LOAD
void zjs_modules_check_load_file(char *file)
{
    // No file waiting to load, just return
    if (load_file == NULL) {
        return;
    }

    zjs_stop_js();
    char *buf = NULL;
    size_t size;
    jerry_value_t parsed_code = 0;
    buf = read_file_alloc(load_file, &size);
    parsed_code = jerry_parse((const jerry_char_t *)buf, size, false);

    if (jerry_value_has_error_flag(parsed_code)) {
        ERR_PRINT("Error parsing JS\n");
    }

    zjs_free(buf);
    ZVAL ret_value = jerry_run(parsed_code);
    if (jerry_value_has_error_flag(ret_value)) {
        ERR_PRINT("Error running JS\n");
    }

    // Remove the load file so it doesn't load it again
    zjs_free(load_file);
    load_file = NULL;
    jerry_release_value(parsed_code);
}

static ZJS_DECL_FUNC(zjs_run_js)
{
    ZJS_VALIDATE_ARGS(Z_STRING);
    size_t len = MAX_FILE_NAME;
    size_t file_len;
    load_file = zjs_malloc(len);

    zjs_copy_jstring(argv[0], load_file, &file_len);

    if (!fs_exist(load_file)) {
        return ZJS_ERROR("File doesn't exist");
    }

    return ZJS_UNDEFINED;
}
#endif  // ZJS_DYNAMIC_LOAD

void zjs_modules_init()
{
    // Add module.exports to global namespace
    ZVAL global_obj = jerry_get_global_object();
    ZVAL modules_obj = zjs_create_object();
    ZVAL exports_obj = zjs_create_object();

    zjs_set_property(modules_obj, "exports", exports_obj);
    zjs_set_property(global_obj, "module", modules_obj);

    // Todo: find a better solution to disable eval() in JerryScript.
    // For now, just inject our eval() function in the global space
    zjs_obj_add_function(global_obj, "eval", native_eval_handler);
    zjs_obj_add_function(global_obj, "print", native_print_handler);
    zjs_obj_add_function(global_obj, "stopJS", stop_js_handler);
#ifdef ZJS_DYNAMIC_LOAD
    zjs_obj_add_function(global_obj, "runJS", zjs_run_js);
#endif // ZJS_DYNAMIC_LOAD

    // create the C handler for require JS call
    zjs_obj_add_function(global_obj, "require", native_require_handler);

#ifdef ZJS_LINUX_BUILD
    ZVAL process = zjs_create_object();
    zjs_obj_add_function(process, "exit", process_exit);
    zjs_set_property(global_obj, "process", process);
#endif

    // initialize callbacks early in case any init functions use them
    zjs_init_callbacks();
    // Load global modules
    int gbl_modcount = sizeof(zjs_global_array) / sizeof(gbl_module_t);
    for (int i = 0; i < gbl_modcount; i++) {
        gbl_module_t *mod = &zjs_global_array[i];
        mod->init();
    }
    // initialize fixed modules
    zjs_error_init();
    zjs_timers_init();
}

void zjs_modules_cleanup()
{
    // stop timers first to prevent further calls
    zjs_timers_cleanup();

    int gbl_modcount = sizeof(zjs_global_array) / sizeof(gbl_module_t);
    for (int i = 0; i < gbl_modcount; i++) {
        gbl_module_t *mod = &zjs_global_array[i];
        if (mod->cleanup) {
            mod->cleanup();
        }
    }
    // clean up fixed modules
    zjs_error_cleanup();

#ifdef ZJS_TRACE_MALLOC
    zjs_print_mem_stats();
#endif
}

void zjs_register_service_routine(void *handle, zjs_service_routine func)
{
    if (num_routines >= NUM_SERVICE_ROUTINES) {
        DBG_PRINT(("not enough space, increase NUM_SERVICE_ROUTINES\n"));
        return;
    }
    svc_routine_map[num_routines].handle = handle;
    svc_routine_map[num_routines].func = func;
    num_routines++;
    return;
}

void zjs_unregister_service_routine(zjs_service_routine func)
{
    for (int i = 0; i < num_routines; i++) {
        if (svc_routine_map[i].func == func) {
            svc_routine_map[i].handle = NULL;
            num_routines--;
            return;
        }
    }
    ERR_PRINT("Couldn't unregister routine for %p\n", func);
}

s32_t zjs_service_routines(void)
{
    s32_t wait = ZJS_TICKS_FOREVER;
    int i;
    for (i = 0; i < num_routines; ++i) {
        s32_t ret = svc_routine_map[i].func(svc_routine_map[i].handle);
        wait = (wait < ret) ? wait : ret;
    }
#ifdef ZJS_LINUX_BUILD
    if (wait == ZJS_TICKS_FOREVER) {
        return 0;
    }
#endif
    return wait;
}
