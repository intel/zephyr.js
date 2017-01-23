// Copyright (c) 2016, Intel Corporation.
#ifdef BUILD_MODULE_FS

#include <zephyr.h>
#include <fs.h>

#include <string.h>

#include "zjs_callbacks.h"
#include "zjs_common.h"
#include "zjs_util.h"
#include "zjs_buffer.h"

#define FLAGS_READABLE      (1 << 0)
#define FLAGS_WRITEABLE     (1 << 1)
#define FLAGS_APPENDABLE    (1 << 2)
#define FLAGS_CREATE        (1 << 3)
#define FLAGS_NO_CREATE     (1 << 4)

#define MODE_R          (FLAGS_READABLE | FLAGS_NO_CREATE)
#define MODE_R_PLUS     (FLAGS_READABLE | FLAGS_WRITEABLE | FLAGS_NO_CREATE)
#define MODE_W          (FLAGS_WRITEABLE | FLAGS_CREATE)
#define MODE_WX         (FLAGS_WRITEABLE | FLAGS_NO_CREATE)
#define MODE_W_PLUS     (FLAGS_WRITEABLE | FLAGS_READABLE | FLAGS_CREATE)
#define MODE_WX_PLUS    (FLAGS_WRITEABLE | FLAGS_READABLE | FLAGS_NO_CREATE)
#define MODE_A          (FLAGS_WRITEABLE | FLAGS_APPENDABLE | FLAGS_CREATE)
#define MODE_AX         (FLAGS_WRITEABLE | FLAGS_APPENDABLE | FLAGS_NO_CREATE)
#define MODE_A_PLUS     (FLAGS_READABLE | FLAGS_APPENDABLE | FLAGS_CREATE)
#define MODE_AX_PLUS    (FLAGS_READABLE | FLAGS_APPENDABLE | FLAGS_NO_CREATE)

#define MAX_PATH_LENGTH 128

typedef struct file_handle_t {
    fs_file_t fp;
    uint16_t mode;
    jerry_value_t fd_val;
    int error;
} file_handle;

static file_handle* new_file(void)
{
    file_handle* handle = zjs_malloc(sizeof(file_handle));

    memset(handle, 0, sizeof(file_handle));

    return handle;
}

static int file_exists(const char *path)
{
    int res;
    struct fs_dirent entry;

    res = fs_stat(path, &entry);

    return !res;
}

static uint16_t get_mode(char* str)
{
    uint16_t mode = 0;
    if (strcmp(str, "r") == 0) {
        mode = MODE_R;
    } else if (strcmp(str, "r+") == 0) {
        mode = MODE_R_PLUS;
    } else if (strcmp(str, "w") == 0) {
        mode = MODE_W;
    } else if (strcmp(str, "wx") == 0) {
        mode = MODE_WX;
    } else if (strcmp(str, "w+") == 0) {
        mode = MODE_W_PLUS;
    } else if (strcmp(str, "wx+") == 0) {
        mode = MODE_WX_PLUS;
    } else if (strcmp(str, "a") == 0) {
        mode = MODE_A;
    } else if (strcmp(str, "ax") == 0) {
        mode = MODE_AX;
    } else if (strcmp(str, "a+") == 0) {
        mode = MODE_A_PLUS;
    } else if (strcmp(str, "ax+") == 0) {
        mode = MODE_AX_PLUS;
    }
    return mode;
}

static jerry_value_t is_file(const jerry_value_t function_obj,
                             const jerry_value_t this,
                             const jerry_value_t argv[],
                             const jerry_length_t argc)
{
    struct fs_dirent* entry;

    if (!jerry_get_object_native_handle(this, (uintptr_t*)&entry)) {
        ERR_PRINT("native handle not found\n");
        return jerry_create_boolean(false);
    }
    if (entry->type == FS_DIR_ENTRY_FILE) {
        return jerry_create_boolean(true);
    } else {
        return jerry_create_boolean(false);
    }
}

static jerry_value_t is_directory(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc)
{
    struct fs_dirent* entry;

    if (!jerry_get_object_native_handle(this, (uintptr_t*)&entry)) {
        ERR_PRINT("native handle not found\n");
        return jerry_create_boolean(false);
    }
    if (entry->type == FS_DIR_ENTRY_DIR) {
        return jerry_create_boolean(true);
    } else {
        return jerry_create_boolean(false);
    }
}

void free_stats(const uintptr_t native)
{
    struct zfs_dirent* entry = (struct zfs_dirent*)native;
    if (entry) {
        zjs_free(entry);
    }
}

static jerry_value_t create_stats_obj(struct fs_dirent* entry)
{

    jerry_value_t stats_obj = jerry_create_object();

    struct fs_dirent* new_entry = zjs_malloc(sizeof(struct fs_dirent));
    if (!new_entry) {
        return zjs_error("malloc failed");
    }
    memcpy(new_entry, entry, sizeof(struct fs_dirent));

    jerry_set_object_native_handle(stats_obj, (uintptr_t)new_entry, NULL);

    zjs_obj_add_function(stats_obj, is_file, "isFile");
    zjs_obj_add_function(stats_obj, is_directory, "isDirectory");

    return stats_obj;
}

static jerry_value_t zjs_open(const jerry_value_t function_obj,
                              const jerry_value_t this,
                              const jerry_value_t argv[],
                              const jerry_length_t argc,
                              uint8_t async)
{
    if (argc < 3 && async) {
        return zjs_error("not enough arguments");
    }
    if (!jerry_value_is_string(argv[0])) {
        return zjs_error("first parameter must be path string");
    }
    if (!jerry_value_is_string(argv[1])) {
        return zjs_error("second parameter must be flags string");
    }
    if (async) {
        if (!jerry_value_is_function(argv[argc - 1])) {
            return zjs_error("last parameter must be callback function");
        }
    }

    file_handle* handle = new_file();
    handle->fd_val = jerry_create_object();

    jerry_size_t size = MAX_PATH_LENGTH;
    char path[size];

    zjs_copy_jstring(argv[0], path, &size);
    if (!size) {
        return zjs_error("size mismatch");
    }

    size = 4;
    char mode[size];

    zjs_copy_jstring(argv[1], mode, &size);
    if (!size) {
        return zjs_error("size mismatch");
    }

    DBG_PRINT("Opening file: %s, mode: %s\n", path, mode);

    handle->mode = get_mode(mode);

    /*
     * TODO: Currently Zephyr has no concept of file permissions, when you open
     *       a file it is automatically readable and writable. This if block
     *       is here in case things change and we can throw an exception if a
     *       file is opened with improper permissions.
     */
    if (file_exists(path)) {
        if (handle->mode & FLAGS_NO_CREATE) {
            ERR_PRINT("Opening file %s in mode %s throws exception if file exists\n", path, mode);
            return zjs_error("file already exists");
        } else {
            DBG_PRINT("Opening existing file %s with mode %s\n", path, mode);
        }
    }

    handle->error = fs_open(&handle->fp, path);
    if (handle->error != 0) {
        ERR_PRINT("could not open file: %s, error=%d\n", path, handle->error);
        return zjs_error("could not open file");
    }

    jerry_set_object_native_handle(handle->fd_val, (uintptr_t)handle, NULL);

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        zjs_callback_id id = zjs_add_callback_once(argv[argc - 1],
                                                   this,
                                                   handle,
                                                   NULL);

        jerry_value_t args[2];

        args[0] = jerry_create_number(handle->error);
        args[1] = handle->fd_val;

        zjs_signal_callback(id, args, sizeof(jerry_value_t) * 2);

        return ZJS_UNDEFINED;
    }
#endif

    return handle->fd_val;
}

static jerry_value_t zjs_open_sync(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc) {
    return zjs_open(function_obj, this, argv, argc, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static jerry_value_t zjs_open_async(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc) {
    return zjs_open(function_obj, this, argv, argc, 1);
}
#endif

static jerry_value_t zjs_close(const jerry_value_t function_obj,
                               const jerry_value_t this,
                               const jerry_value_t argv[],
                               const jerry_length_t argc,
                               uint8_t async)
{
    file_handle* handle;

    if (!jerry_value_is_object(argv[0])) {
        return zjs_error("first parameter was not file descriptor");
    }
    if (async) {
        if (!jerry_value_is_function(argv[1])) {
            return zjs_error("second parameter must be callback function");
        }
    }
    if (!jerry_get_object_native_handle(argv[0], (uintptr_t*)&handle)) {
        return zjs_error("native handle not found");
    }

    handle->error = fs_close(&handle->fp);

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        zjs_callback_id id = zjs_add_callback_once(argv[1],
                                                   this,
                                                   handle,
                                                   NULL);

        jerry_value_t error = jerry_create_number(handle->error);

        zjs_signal_callback(id, &error, sizeof(jerry_value_t));
    }
#endif

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_close_sync(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc) {
    return zjs_close(function_obj, this, argv, argc, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static jerry_value_t zjs_close_async(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc) {
    return zjs_close(function_obj, this, argv, argc, 1);
}
#endif

static jerry_value_t zjs_unlink(const jerry_value_t function_obj,
                                const jerry_value_t this,
                                const jerry_value_t argv[],
                                const jerry_length_t argc,
                                uint8_t async)
{
    int ret = 0;

    if (!jerry_value_is_string(argv[0])) {
        return zjs_error("first parameter was not path");
    }
    if (async) {
        if (!jerry_value_is_function(argv[1])) {
            return zjs_error("second parameter was not function");
        }
    }

    jerry_size_t size = MAX_PATH_LENGTH;
    char path[size];

    zjs_copy_jstring(argv[0], path, &size);
    if (!size) {
        return zjs_error("size mismatch");
    }

    ret = fs_unlink(path);

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        zjs_callback_id id = zjs_add_callback_once(argv[1],
                                                   this,
                                                   NULL,
                                                   NULL);

        jerry_value_t error = jerry_create_number(ret);
        zjs_signal_callback(id, &error, sizeof(jerry_value_t));
    }
#endif

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_unlink_sync(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc) {
    return zjs_unlink(function_obj, this, argv, argc, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static jerry_value_t zjs_unlink_async(const jerry_value_t function_obj,
                                      const jerry_value_t this,
                                      const jerry_value_t argv[],
                                      const jerry_length_t argc) {
    return zjs_unlink(function_obj, this, argv, argc, 1);
}
#endif

static jerry_value_t zjs_read(const jerry_value_t function_obj,
                              const jerry_value_t this,
                              const jerry_value_t argv[],
                              const jerry_length_t argc,
                              uint8_t async)
{
    file_handle* handle;
    int err = 0;

    if (argc < 6 && async) {
        return zjs_error("not enough parameters");
    }

    if (!jerry_value_is_object(argv[0])) {
        return zjs_error("first parameter was not file descriptor");
    }
    if (!jerry_value_is_object(argv[1])) {
        return zjs_error("second parameter was not buffer");
    }
    if (!jerry_value_is_number(argv[2])) {
        return zjs_error("third parameter was not offset");
    }
    if (!jerry_value_is_number(argv[3])) {
        return zjs_error("fourth parameter was not length");
    }
    if (!jerry_value_is_number(argv[4])) {
        return zjs_error("fifth parameter was not position");
    }
    if (async) {
        if (!jerry_value_is_function(argv[5])) {
            return zjs_error("sixth parameter was not callback function");
        }
    }
    if (!jerry_get_object_native_handle(argv[0], (uintptr_t*)&handle)) {
        return zjs_error("native handle not found");
    }
    zjs_buffer_t* buffer = zjs_buffer_find(argv[1]);
    uint32_t offset = jerry_get_number_value(argv[2]);
    uint32_t length = jerry_get_number_value(argv[3]);
    uint32_t position = jerry_get_number_value(argv[4]);

    // if a position was specified, seek to it before writing

    if (fs_seek(&handle->fp, position, SEEK_SET) != 0) {
        return zjs_error("error seeking to position");
    }

    DBG_PRINT("reading into fp=%p, buffer=%p, offset=%lu, length=%lu\n", &handle->fp, buffer->buffer, offset, length);

    uint32_t ret = fs_read(&handle->fp, buffer->buffer + offset, length);

    if (ret != length) {
        ERR_PRINT("could not read %lu bytes, only %lu were read\n", length, ret);
        err = -1;
    }

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        jerry_value_t args[3];

        args[0] = jerry_create_number(err);
        args[1] = jerry_create_number(ret);
        args[2] = argv[1];

        zjs_callback_id id = zjs_add_callback_once(argv[5],
                                                   this,
                                                   NULL,
                                                   NULL);

        zjs_signal_callback(id, args, sizeof(jerry_value_t) * 3);

        return ZJS_UNDEFINED;
    }
#endif
    return jerry_create_number(ret);
}

static jerry_value_t zjs_read_sync(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc) {
    return zjs_read(function_obj, this, argv, argc, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static jerry_value_t zjs_read_async(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc) {
    return zjs_read(function_obj, this, argv, argc, 1);
}
#endif

static jerry_value_t zjs_write(const jerry_value_t function_obj,
                               const jerry_value_t this,
                               const jerry_value_t argv[],
                               const jerry_length_t argc,
                               uint8_t async)
{
    file_handle* handle;
    int position = 0;
    jerry_value_t js_cb = ZJS_UNDEFINED;

    if (argc < 5) {
        return zjs_error("not enough parameters");
    }

    if (!jerry_value_is_object(argv[0])) {
        return zjs_error("first parameter was not file descriptor");
    }
    if (!jerry_value_is_object(argv[1])) {
        return zjs_error("second parameter was not buffer");
    }
    if (!jerry_value_is_number(argv[2])) {
        return zjs_error("third parameter was not offset");
    }
    if (!jerry_value_is_number(argv[3])) {
        return zjs_error("fourth parameter was not length");
    }
    if (argc == 6) {
        if (!jerry_value_is_number(argv[4])) {
            return zjs_error("position parameter was not number");
        }
        if (async) {
            if (!jerry_value_is_function(argv[5])) {
                return zjs_error("sixth parameter was not callback function");
            }
            js_cb = argv[5];
        }
        position = jerry_get_number_value(argv[4]);
    } else {
#ifdef ZJS_FS_ASYNC_APIS
        if (async) {
            if (!jerry_value_is_function(argv[4])) {
                return zjs_error("fifth parameter was not callback function");
            }
            js_cb = argv[4];
        }
#endif
    }
    if (!jerry_get_object_native_handle(argv[0], (uintptr_t*)&handle)) {
        return zjs_error("native handle not found");
    }
    zjs_buffer_t* buffer = zjs_buffer_find(argv[1]);
    uint32_t offset = jerry_get_number_value(argv[2]);
    uint32_t length = jerry_get_number_value(argv[3]);

    // if a position was specified, seek to it before writing
    if (fs_seek(&handle->fp, position, SEEK_SET) != 0) {
        return zjs_error("error seeking to position\n");
    }

    DBG_PRINT("writing to fp=%p, buffer=%p, offset=%lu, length=%lu\n", &handle->fp, buffer->buffer, offset, length);

    uint32_t written = fs_write(&handle->fp, buffer->buffer + offset, length);

    if (written != length) {
        ERR_PRINT("could not write %lu bytes, only %lu were written\n", length, written);
    }

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        jerry_value_t args[3];

        args[0] = jerry_create_number(0);
        args[1] = jerry_create_number(written);
        args[2] = argv[1];

        zjs_callback_id id = zjs_add_callback_once(js_cb,
                                                   this,
                                                   NULL,
                                                   NULL);

        zjs_signal_callback(id, args, sizeof(jerry_value_t) * 3);

        return ZJS_UNDEFINED;
    }
#endif
    return jerry_create_number(written);
}

static jerry_value_t zjs_write_sync(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc) {
    return zjs_write(function_obj, this, argv, argc, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static jerry_value_t zjs_write_async(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc) {
    return zjs_write(function_obj, this, argv, argc, 1);
}
#endif

static jerry_value_t zjs_truncate(const jerry_value_t function_obj,
                                  const jerry_value_t this,
                                  const jerry_value_t argv[],
                                  const jerry_length_t argc,
                                  uint8_t async)
{
    fs_file_t fp;
    if (!jerry_value_is_number(argv[1])) {
        return zjs_error("second parameter must be length");
    }
    if (async) {
        if (!jerry_value_is_function(argv[2])) {
            return zjs_error("third paramter must be callback function");
        }
    }
    if (jerry_value_is_object(argv[0])) {
        file_handle* handle;
        if (!jerry_get_object_native_handle(argv[0], (uintptr_t*)&handle)) {
            return zjs_error("native handle not found");
        }
        fp = handle->fp;
    } else if (jerry_value_is_string(argv[0])) {
        jerry_size_t size = MAX_PATH_LENGTH;
        char path[size];

        zjs_copy_jstring(argv[0], path, &size);
        if (!size) {
            return zjs_error("size mismatch");
        }
        if (!fs_open(&fp, path)) {
            return zjs_error("error opening file for truncation");
        }
    } else {
        return zjs_error("first parameter must be either file descriptor or file path");
    }

    uint32_t length = jerry_get_number_value(argv[1]);

    if (!fs_truncate(&fp, length)) {
        return zjs_error("error calling fs_truncate()");
    }

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        zjs_callback_id id = zjs_add_callback_once(argv[2],
                                                   this,
                                                   NULL,
                                                   NULL);

        zjs_signal_callback(id, NULL, 0);
    }
#endif
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_truncate_sync(const jerry_value_t function_obj,
                                       const jerry_value_t this,
                                       const jerry_value_t argv[],
                                       const jerry_length_t argc) {
    return zjs_truncate(function_obj, this, argv, argc, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static jerry_value_t zjs_truncate_async(const jerry_value_t function_obj,
                                        const jerry_value_t this,
                                        const jerry_value_t argv[],
                                        const jerry_length_t argc) {
    return zjs_truncate(function_obj, this, argv, argc, 1);
}
#endif

static jerry_value_t zjs_mkdir(const jerry_value_t function_obj,
                               const jerry_value_t this,
                               const jerry_value_t argv[],
                               const jerry_length_t argc,
                               uint8_t async)
{
    jerry_value_t js_cb = ZJS_UNDEFINED;
    uint32_t mode = MODE_A_PLUS;
    jerry_size_t size;
    if (!jerry_value_is_string(argv[0])) {
        return zjs_error("first parameter was not path");
    }
    if (argc > 2) {
        if (async) {
            if (!jerry_value_is_function(argv[2])) {
                return zjs_error("third parameter must be callback function");
            } else {
                js_cb = argv[2];
            }
        }
        if (!jerry_value_is_string(argv[1])) {
            return zjs_error("second parameter must be mode string");
        } else {
            size = 4;
            char mode_str[size];

            zjs_copy_jstring(argv[1], mode_str, &size);
            if (!size) {
                return zjs_error("size mismatch");
            }

            mode = get_mode(mode_str);
        }
    } else {
        if (async) {
            if (!jerry_value_is_function(argv[1])) {
                return zjs_error("second parameter must be callback function");
            } else {
                js_cb = argv[1];
            }
        }
    }
    size = MAX_PATH_LENGTH;
    char path[size];

    zjs_copy_jstring(argv[0], path, &size);
    if (!size) {
        return zjs_error("size mismatch");
    }

    if (!fs_mkdir(path)) {
        return zjs_error("error creating directory");
    }
#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        zjs_callback_id id = zjs_add_callback_once(js_cb,
                                                   this,
                                                   NULL,
                                                   NULL);

        zjs_signal_callback(id, NULL, 0);
    }
#endif

    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_mkdir_sync(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc) {
    return zjs_mkdir(function_obj, this, argv, argc, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static jerry_value_t zjs_mkdir_async(const jerry_value_t function_obj,
                                     const jerry_value_t this,
                                     const jerry_value_t argv[],
                                     const jerry_length_t argc) {
    return zjs_mkdir(function_obj, this, argv, argc, 1);
}
#endif

static jerry_value_t zjs_readdir(const jerry_value_t function_obj,
                                 const jerry_value_t this,
                                 const jerry_value_t argv[],
                                 const jerry_length_t argc,
                                 uint8_t async)
{
    jerry_value_t array;

    if (!jerry_value_is_string(argv[0])) {
        return zjs_error("first parameter was not path");
    }
    jerry_size_t size = MAX_PATH_LENGTH;
    char path[size];

    zjs_copy_jstring(argv[0], path, &size);
    if (!size) {
        return zjs_error("size mismatch");
    }

    int num_files = 0;
    int res;
    fs_dir_t dp;
    static struct fs_dirent entry;

    res = fs_opendir(&dp, path);
    if (res) {
        return zjs_error("Error opening dir");
    }

    DBG_PRINT("Searching for files and sub directories in %s\n", path);
    for (;;) {
        res = fs_readdir(&dp, &entry);

        /* entry.name[0] == 0 means end-of-dir */
        if (res || entry.name[0] == 0) {
            break;
        }

        /* Delete file or sub directory */
        num_files++;

        DBG_PRINT("found file %s\n", entry.name);
    }

    fs_closedir(&dp);

    res = fs_opendir(&dp, path);
    if (res) {
        return zjs_error("Error opening dir");
    }

    DBG_PRINT("Adding files and sub directories in %s to array\n", path);

    array = jerry_create_array(num_files);

    uint32_t i;
    for (i = 0; i < num_files; ++i) {
        res = fs_readdir(&dp, &entry);

        /* entry.name[0] == 0 means end-of-dir */
        if (res || entry.name[0] == 0) {
            break;
        }

        jerry_value_t value = jerry_create_string(entry.name);

        jerry_set_property_by_index (array, i, value);
    }

    fs_closedir(&dp);

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        zjs_callback_id id = zjs_add_callback_once(argv[1],
                                               this,
                                               NULL,
                                               NULL);

        jerry_value_t args[2];

        args[0] = jerry_create_number(res);
        args[1] = array;

        zjs_signal_callback(id, args, sizeof(jerry_value_t) * 2);

        return ZJS_UNDEFINED;
    }
#endif
    return array;
}

static jerry_value_t zjs_readdir_sync(const jerry_value_t function_obj,
                                      const jerry_value_t this,
                                      const jerry_value_t argv[],
                                      const jerry_length_t argc) {
    return zjs_readdir(function_obj, this, argv, argc, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static jerry_value_t zjs_readdir_async(const jerry_value_t function_obj,
                                       const jerry_value_t this,
                                       const jerry_value_t argv[],
                                       const jerry_length_t argc) {
    return zjs_readdir(function_obj, this, argv, argc, 1);
}
#endif

static jerry_value_t zjs_stat(const jerry_value_t function_obj,
                              const jerry_value_t this,
                              const jerry_value_t argv[],
                              const jerry_length_t argc,
                              uint8_t async)
{
    if (!jerry_value_is_string(argv[0])) {
        return zjs_error("first parameter was not path");
    }
    if (async) {
        if (!jerry_value_is_function(argv[1])) {
            return zjs_error("second parameter was not callback function");
        }
    }
    jerry_size_t size = MAX_PATH_LENGTH;
    char path[size];

    zjs_copy_jstring(argv[0], path, &size);
    if (!size) {
        return zjs_error("size mismatch");
    }

    int ret;
    struct fs_dirent entry;

    ret = fs_stat(path, &entry);

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        jerry_value_t args[2];

        args[0] = jerry_create_number(ret);
        args[1] = create_stats_obj(&entry);

        zjs_callback_id id = zjs_add_callback_once(argv[1],
                                                   this,
                                                   NULL,
                                                   NULL);

        zjs_signal_callback(id, args, sizeof(jerry_value_t) * 2);

        return ZJS_UNDEFINED;
    }
#endif
    return create_stats_obj(&entry);
}

static jerry_value_t zjs_stat_sync(const jerry_value_t function_obj,
                                   const jerry_value_t this,
                                   const jerry_value_t argv[],
                                   const jerry_length_t argc) {
    return zjs_stat(function_obj, this, argv, argc, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static jerry_value_t zjs_stat_async(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc) {
    return zjs_stat(function_obj, this, argv, argc, 1);
}
#endif

static jerry_value_t zjs_write_file(const jerry_value_t function_obj,
                                    const jerry_value_t this,
                                    const jerry_value_t argv[],
                                    const jerry_length_t argc,
                                    uint8_t async)
{
    int error = 0;
    jerry_value_t js_cb = ZJS_UNDEFINED;
    char* data;
    uint32_t length;
    if (!jerry_value_is_string(argv[0])) {
        return zjs_error("first parameter must be file name");
    }
    if (jerry_value_is_string(argv[1])) {
        jerry_size_t jlen = jerry_get_string_size(argv[0]);
        data = zjs_malloc(jlen + 1);

        int wlen = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)data, jlen);
        if (jlen != wlen) {
            return zjs_error("size mismatch");
        }
        data[wlen] = '\0';
        length = wlen;
    } else if (jerry_value_is_object(argv[1])) {
        zjs_buffer_t* buffer = zjs_buffer_find(argv[1]);
        data = buffer->buffer;
        length = buffer->bufsize;
    } else {
        return zjs_error("second parameter must be string of buffer data");
    }
    // Options provided
    if (argc > 3) {
        if (!jerry_value_is_object(argv[2])) {
            return zjs_error("third parameter must be options object");
        }
        // Options object has no effect on Zephyr, 'mode' and 'flag' properties
        // are not used for opening a file, so their values are irrelevant
#ifdef ZJS_FS_ASYNC_APIS
        if (async) {
            if (!jerry_value_is_function(argv[3])) {
                return zjs_error("last parameter must be callback function");
            } else {
                js_cb = argv[3];
            }
        }
#endif
    } else if (async) {
        js_cb = argv[argc - 1];
    }
    jerry_size_t jlen = jerry_get_string_size(argv[0]);
    char path[jlen + 1];

    int wlen = jerry_string_to_char_buffer(argv[0], (jerry_char_t *)path, jlen);
    if (jlen != wlen) {
        return zjs_error("size mismatch");
    }
    path[wlen] = '\0';

    fs_file_t fp;
    error = fs_open(&fp, path);
    if (error != 0) {
        ERR_PRINT("error opening file, error=%d\n", error);
        goto Finished;
    }
    ssize_t written = fs_write(&fp, data, length);

    if (written != length) {
        ERR_PRINT("could not write %lu bytes, only %lu were written\n", length, written);
        error = -1;
    }

    error = fs_close(&fp);
    if (error != 0) {
        ERR_PRINT("error closing file\n");
    }

Finished:
#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        zjs_callback_id id = zjs_add_callback_once(js_cb,
                                                   this,
                                                   NULL,
                                                   NULL);

        jerry_value_t err = jerry_create_number(error);

        zjs_signal_callback(id, &err, sizeof(jerry_value_t));
    }
#endif
    return ZJS_UNDEFINED;
}

static jerry_value_t zjs_write_file_sync(const jerry_value_t function_obj,
                                         const jerry_value_t this,
                                         const jerry_value_t argv[],
                                         const jerry_length_t argc) {
    return zjs_write_file(function_obj, this, argv, argc, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static jerry_value_t zjs_write_file_async(const jerry_value_t function_obj,
                                          const jerry_value_t this,
                                          const jerry_value_t argv[],
                                          const jerry_length_t argc) {
    return zjs_write_file(function_obj, this, argv, argc, 1);
}
#endif

jerry_value_t zjs_fs_init()
{
    jerry_value_t fs = jerry_create_object();

    zjs_obj_add_function(fs, zjs_open_sync, "openSync");
    zjs_obj_add_function(fs, zjs_close_sync, "closeSync");
    zjs_obj_add_function(fs, zjs_unlink_sync, "unlinkSync");
    zjs_obj_add_function(fs, zjs_unlink_sync, "rmdirSync");
    zjs_obj_add_function(fs, zjs_read_sync, "readSync");
    zjs_obj_add_function(fs, zjs_write_sync, "writeSync");
    zjs_obj_add_function(fs, zjs_truncate_sync, "truncateSync");
    zjs_obj_add_function(fs, zjs_mkdir_sync, "mkdirSync");
    zjs_obj_add_function(fs, zjs_readdir_sync, "readdirSync");
    zjs_obj_add_function(fs, zjs_stat_sync, "statSync");
    zjs_obj_add_function(fs, zjs_write_file_sync, "writeFileSync");

#ifdef ZJS_FS_ASYNC_APIS
    zjs_obj_add_function(fs, zjs_open_async, "open");
    zjs_obj_add_function(fs, zjs_close_async, "close");
    zjs_obj_add_function(fs, zjs_unlink_async, "unlink");
    zjs_obj_add_function(fs, zjs_unlink_async, "rmdir");
    zjs_obj_add_function(fs, zjs_read_async, "read");
    zjs_obj_add_function(fs, zjs_write_async, "write");
    zjs_obj_add_function(fs, zjs_truncate_async, "truncate");
    zjs_obj_add_function(fs, zjs_mkdir_async, "mkdir");
    zjs_obj_add_function(fs, zjs_readdir_async, "readdir");
    zjs_obj_add_function(fs, zjs_stat_async, "stat");
    zjs_obj_add_function(fs, zjs_write_file_async, "writeFile");
#endif

    return fs;
}
#endif
