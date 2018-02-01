// Copyright (c) 2016-2018, Intel Corporation.

#ifdef BUILD_MODULE_FS

// C includes
#include <string.h>

// Zephyr includes
#include <fs.h>
#include <zephyr.h>

// ZJS includes
#include "zjs_buffer.h"
#include "zjs_callbacks.h"
#include "zjs_common.h"
#include "zjs_util.h"

typedef enum {
    // Open for reading, file must already exist
    MODE_R,
    // Open for reading/writing, file must already exist
    MODE_R_PLUS,
    // Open file for writing, file will be created or overwritten
    MODE_W,
    // Open file for reading/writing, file will be created or overwritten
    MODE_W_PLUS,
    // Open file for appending, file is created if it does not exist
    MODE_A,
    // Open a file for appending/reading, file is created if it does not exist
    MODE_A_PLUS
} FileMode;

#define MAX_PATH_LENGTH 128

#define BIT_SET(a, i) a |= 1 << i
#define BIT_CLR(a, i) a &= ~(1 << i)
#define IS_SET(a, i) (a >> i) & 1

#define invalid_args() zjs_error("invalid arguments")

/*
 * Bit mask of currently open FD's
 */
static u32_t fd_used = 0;

typedef struct file_handle {
    fs_file_t fp;
    int fd;
    FileMode mode;
    int error;
    u32_t rpos;
    struct file_handle *next;
} file_handle_t;

static file_handle_t *opened_handles = NULL;

static void free_stats(void *native)
{
    struct zfs_dirent *entry = (struct zfs_dirent *)native;
    if (entry) {
        zjs_free(entry);
    }
}

static const jerry_object_native_info_t stats_type_info = {
   .free_cb = free_stats
};

static file_handle_t *find_file(int fd)
{
    return ZJS_LIST_FIND(file_handle_t, opened_handles, fd, fd);
}

static file_handle_t *new_file(void)
{
    int fd = 0;
    while (IS_SET(fd_used, fd)) {
        fd++;
        if (fd > 31) {
            return NULL;
        }
    }

    file_handle_t *handle = zjs_malloc(sizeof(file_handle_t));
    if (!handle) {
        return NULL;
    }

    memset(handle, 0, sizeof(file_handle_t));

    handle->fd = fd;
    BIT_SET(fd_used, fd);

    return handle;
}

static void free_file(file_handle_t *f)
{
    BIT_CLR(fd_used, f->fd);
    fs_close(&f->fp);
    zjs_free(f);
}

static int file_exists(const char *path)
{
    int res;
    struct fs_dirent entry;

    res = fs_stat(path, &entry);

    return !res;
}

static u16_t get_mode(char *str)
{
    u16_t mode = 0;
    if (strequal(str, "r")) {
        mode = MODE_R;
    } else if (strequal(str, "r+")) {
        mode = MODE_R_PLUS;
    } else if (strequal(str, "w")) {
        mode = MODE_W;
    } else if (strequal(str, "w+")) {
        mode = MODE_W_PLUS;
    } else if (strequal(str, "a")) {
        mode = MODE_A;
    } else if (strequal(str, "a+")) {
        mode = MODE_A_PLUS;
    }
    return mode;
}

static ZJS_DECL_FUNC(is_file)
{
    ZJS_GET_HANDLE(this, struct fs_dirent, entry, stats_type_info);
    if (entry->type == FS_DIR_ENTRY_FILE) {
        return jerry_create_boolean(true);
    } else {
        return jerry_create_boolean(false);
    }
}

static ZJS_DECL_FUNC(is_directory)
{
    ZJS_GET_HANDLE(this, struct fs_dirent, entry, stats_type_info);
    if (entry->type == FS_DIR_ENTRY_DIR) {
        return jerry_create_boolean(true);
    } else {
        return jerry_create_boolean(false);
    }
}

static jerry_value_t create_stats_obj(struct fs_dirent *entry)
{

    jerry_value_t stats_obj = zjs_create_object();

    struct fs_dirent *new_entry = zjs_malloc(sizeof(struct fs_dirent));
    if (!new_entry) {
        return zjs_error_context("malloc failed", 0, 0);
    }
    *new_entry = *entry;

    jerry_set_object_native_pointer(stats_obj, new_entry, &stats_type_info);

    zjs_obj_add_function(stats_obj, "isFile", is_file);
    zjs_obj_add_function(stats_obj, "isDirectory", is_directory);
    zjs_obj_add_number(stats_obj, "size", (double)entry->size);

    return stats_obj;
}

static ZJS_DECL_FUNC_ARGS(zjs_fs_open, u8_t async)
{
    // NOTE: what we call mode below is actually 'flags' in Node docs, argv[1];
    //   we don't support mode (optional argv[2])
    // args: filepath, flags
    ZJS_VALIDATE_ARGS(Z_STRING, Z_STRING);

#ifdef ZJS_FS_ASYNC_APIS
    // async case add callback arg
    if (async) {
        ZJS_VALIDATE_ARGS_OFFSET(2, Z_FUNCTION);
    }
#endif

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

    FileMode m = get_mode(mode);

    if ((m == MODE_R || m == MODE_R_PLUS) && !file_exists(path)) {
        return zjs_error("file doesn't exist");
    }

    file_handle_t *handle = new_file();
    if (!handle) {
        return zjs_error("malloc failed");
    }

    handle->error = fs_open(&handle->fp, path);
    if (handle->error != 0) {
        ERR_PRINT("could not open file: %s, error=%d\n", path, handle->error);
        zjs_free(handle);
        return zjs_error("could not open file");
    }

    // w and w+ overwrite the existing file
    if ((m == MODE_W) || (m == MODE_W_PLUS)) {
        if (fs_truncate(&handle->fp, 0) != 0) {
            ERR_PRINT("could not truncate file: %s\n", path);
        }
    }

    handle->mode = m;
    handle->next = opened_handles;
    opened_handles = handle;

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        ZVAL err = jerry_create_number(handle->error);
        ZVAL fd_val = jerry_create_number(handle->fd);
        jerry_value_t args[] = { err, fd_val };

        zjs_callback_id id = zjs_add_callback_once(argv[2], this, handle, NULL);
        zjs_signal_callback(id, args, sizeof(args));
        return ZJS_UNDEFINED;
    }
#endif

    return jerry_create_number(handle->fd);
}

static ZJS_DECL_FUNC(zjs_fs_open_sync)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_open, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_open_async)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_open, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_close, u8_t async)
{
    // args: file descriptor
    ZJS_VALIDATE_ARGS(Z_NUMBER);
#ifdef ZJS_FS_ASYNC_APIS
    // async case adds callback arg
    if (async) {
        ZJS_VALIDATE_ARGS_OFFSET(1, Z_FUNCTION);
    }
#endif

    file_handle_t *handle;
    handle = find_file((int)jerry_get_number_value(argv[0]));
    if (!handle) {
        return zjs_error("file not found");
    }

    handle->error = fs_close(&handle->fp);

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        ZVAL error = jerry_create_number(handle->error);

        zjs_callback_id id = zjs_add_callback_once(argv[1], this, handle, NULL);
        zjs_signal_callback(id, &error, sizeof(error));
    }
#endif

    ZJS_LIST_REMOVE(file_handle_t, opened_handles, handle);
    free_file(handle);

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_fs_close_sync)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_close, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_close_async)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_close, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_unlink, u8_t async)
{
    // args: filename
    ZJS_VALIDATE_ARGS(Z_STRING);

#ifdef ZJS_FS_ASYNC_APIS
    // async case adds callback arg
    if (async) {
        ZJS_VALIDATE_ARGS_OFFSET(1, Z_FUNCTION);
    }
#endif

    int ret = 0;
    jerry_size_t size = MAX_PATH_LENGTH;
    char path[size];

    zjs_copy_jstring(argv[0], path, &size);
    if (!size) {
        return zjs_error("size mismatch");
    }

    ret = fs_unlink(path);
    if (ret != 0) {
        DBG_PRINT("failed to unlink %s with error [%d]\n", path, ret);
        return ZJS_UNDEFINED;
    }

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        ZVAL error = jerry_create_number(ret);

        zjs_callback_id id = zjs_add_callback_once(argv[1], this, NULL, NULL);
        zjs_signal_callback(id, &error, sizeof(error));
    }
#endif

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_fs_unlink_sync)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_unlink, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_unlink_async)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_unlink, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_read, u8_t async)
{
    // args: file descriptor, buffer, offset, length, position
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_BUFFER, Z_NUMBER, Z_NUMBER, Z_NUMBER Z_NULL);
#ifdef ZJS_FS_ASYNC_APIS
    // async case adds callback
    if (async) {
        ZJS_VALIDATE_ARGS_OFFSET(5, Z_FUNCTION);
    }
#endif

    file_handle_t *handle;
    int err = 0;

    handle = find_file((int)jerry_get_number_value(argv[0]));
    if (!handle) {
        return zjs_error("file not found");
    }

    if (handle->mode == MODE_W || handle->mode == MODE_A) {
        return zjs_error("file is not open for reading");
    }

    zjs_buffer_t *buffer = zjs_buffer_find(argv[1]);
    double offset = jerry_get_number_value(argv[2]);
    double length = jerry_get_number_value(argv[3]);

    if (offset < 0 || length < 0) {
        return invalid_args();
    }
    if (offset >= buffer->bufsize) {
        return zjs_error("offset overflows buffer");
    }
    if (offset + length > buffer->bufsize) {
        return zjs_error("offset + length overflows buffer");
    }

    // if mode == a+
    if (handle->mode == MODE_A_PLUS) {
        // mode is a+, seek to read position
        if (fs_seek(&handle->fp, handle->rpos, SEEK_SET) != 0) {
            return zjs_error("error seeking to position");
        }
    }
    if (jerry_value_is_number(argv[4])) {
        // if position was a number, set as the new read position
        double position = jerry_get_number_value(argv[4]);
        if (position < 0) {
            return invalid_args();
        }
        handle->rpos = position;
        // if a position was specified, seek to it before reading
        if (fs_seek(&handle->fp, (u32_t)position, SEEK_SET) != 0) {
            return zjs_error("error seeking to position");
        }
    }

    DBG_PRINT("reading into fp=%p, buffer=%p, offset=%lu, length=%lu\n",
              &handle->fp, buffer->buffer, (u32_t)offset, (u32_t)length);

    u32_t ret = fs_read(&handle->fp, buffer->buffer + (u32_t)offset,
                        (u32_t)length);

    if (ret != (u32_t)length) {
        DBG_PRINT("could not read %lu bytes, only %lu were read\n",
                  (u32_t)length, ret);
        err = -1;
    }
    handle->rpos += ret;

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        ZVAL err_val = jerry_create_number(err);
        ZVAL ret_val = jerry_create_number(ret);
        jerry_value_t args[] = { err_val, ret_val, argv[1] };

        zjs_callback_id id = zjs_add_callback_once(argv[5], this, NULL, NULL);
        zjs_signal_callback(id, args, sizeof(args));
        return ZJS_UNDEFINED;
    }
#endif
    return jerry_create_number(ret);
}

static ZJS_DECL_FUNC(zjs_fs_read_sync)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_read, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_read_async)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_read, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_write, u8_t async)
{
    // args: file descriptor, buffer[, offset[, length[, position]]]
    ZJS_VALIDATE_ARGS_OPTCOUNT(optcount, Z_NUMBER, Z_BUFFER,
                               Z_OPTIONAL Z_NUMBER, Z_OPTIONAL Z_NUMBER,
                               Z_OPTIONAL Z_NUMBER);
// NOTE: Borrowing the optional parameters from Node 7.x, beyond 6.10 LTS
//         which we're currently targeting.
#ifdef ZJS_FS_ASYNC_APIS
    // async case adds callback arg
    int cbindex = 2 + optcount;
    if (async) {
        ZJS_VALIDATE_ARGS_OFFSET(cbindex, Z_FUNCTION);
    }
#endif

    file_handle_t *handle;
    u32_t offset = 0;
    u32_t length = 0;
    u32_t position = 0;
    u8_t from_cur = 0;

    handle = find_file((int)jerry_get_number_value(argv[0]));
    if (!handle) {
        return zjs_error("file not found");
    }

    if (handle->mode == MODE_R) {
        return zjs_error("file is not open for writing");
    }

    zjs_buffer_t *buffer = zjs_buffer_find(argv[1]);

    switch (optcount) {
    case 3:
        position = jerry_get_number_value(argv[4]);
    case 2:
        length = jerry_get_number_value(argv[3]);
    case 1:
        offset = jerry_get_number_value(argv[2]);
    default:
        break;
    }

    if (offset && !length) {
        length = buffer->bufsize - offset;
    } else if (!length) {
        length = buffer->bufsize;
    }

    if (offset >= buffer->bufsize) {
        return zjs_error("offset overflows buffer");
    }
    if (offset + length > buffer->bufsize) {
        return zjs_error("offset + length overflows buffer");
    }

    if (handle->mode == MODE_A || handle->mode == MODE_A_PLUS) {
        // if in append mode, seek to end (ignoring position parameter)
        if (fs_seek(&handle->fp, 0, SEEK_END) != 0) {
            return zjs_error("error seeking start");
        }
    } else if (!from_cur) {
        // if a position was specified, seek to it before writing
        if (fs_seek(&handle->fp, position, SEEK_SET) != 0) {
            return zjs_error("error seeking to position\n");
        }
    }

    DBG_PRINT("writing to fp=%p, buffer=%p, offset=%lu, length=%lu\n",
              &handle->fp, buffer->buffer, offset, length);

    u32_t written = fs_write(&handle->fp, buffer->buffer + offset, length);
    if (written != length) {
        DBG_PRINT("could not write %lu bytes, only %lu were written\n", length,
                  written);
    }

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        ZVAL err = jerry_create_number(0);
        ZVAL bytes = jerry_create_number(written);
        jerry_value_t args[] = { err, bytes, argv[1] };

        zjs_callback_id id = zjs_add_callback_once(argv[cbindex], this, NULL,
                                                   NULL);
        zjs_signal_callback(id, args, sizeof(args));
        return ZJS_UNDEFINED;
    }
#endif
    return jerry_create_number(written);
}

static ZJS_DECL_FUNC(zjs_fs_write_sync)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_write, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_write_async)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_write, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_truncate, u8_t async)
{
    // args: file descriptor or string path, length
    ZJS_VALIDATE_ARGS(Z_OBJECT Z_STRING, Z_NUMBER);

#ifdef ZJS_FS_ASYNC_APIS
    // async case adds callback arg
    if (async) {
        ZJS_VALIDATE_ARGS_OFFSET(2, Z_FUNCTION);
    }
#endif
    fs_file_t fp;
    if (jerry_value_is_number(argv[0])) {
        file_handle_t *handle;
        handle = find_file((int)jerry_get_number_value(argv[0]));
        if (!handle) {
            return zjs_error("file not found");
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
        return invalid_args();
    }

    u32_t length = jerry_get_number_value(argv[1]);

    if (fs_truncate(&fp, length) != 0) {
        return zjs_error("error calling fs_truncate()");
    }

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        zjs_callback_id id = zjs_add_callback_once(argv[2], this, NULL, NULL);
        zjs_signal_callback(id, NULL, 0);
    }
#endif
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_fs_truncate_sync)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_truncate, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_truncate_async)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_truncate, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_mkdir, u8_t async)
{
    // args: dirpath
    ZJS_VALIDATE_ARGS(Z_STRING);

#ifdef ZJS_FS_ASYNC_APIS
    // async case adds callback arg
    if (async) {
        ZJS_VALIDATE_ARGS_OFFSET(1, Z_FUNCTION);
    }
#endif

    jerry_size_t size;

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
        zjs_callback_id id = zjs_add_callback_once(argv[1], this, NULL, NULL);
        zjs_signal_callback(id, NULL, 0);
    }
#endif

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_fs_mkdir_sync)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_mkdir, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_mkdir_async)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_mkdir, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_readdir, u8_t async)
{
    // args: dirpath
    ZJS_VALIDATE_ARGS(Z_STRING);

#ifdef ZJS_FS_ASYNC_APIS
    // async case adds callback arg
    if (async) {
        ZJS_VALIDATE_ARGS_OFFSET(1, Z_FUNCTION);
    }
#endif

    jerry_value_t array;
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

    u32_t i;
    for (i = 0; i < num_files; ++i) {
        res = fs_readdir(&dp, &entry);

        /* entry.name[0] == 0 means end-of-dir */
        if (res || entry.name[0] == 0) {
            break;
        }

        jerry_value_t value = jerry_create_string(entry.name);

        jerry_set_property_by_index(array, i, value);
    }

    fs_closedir(&dp);

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        ZVAL err = jerry_create_number(res);
        jerry_value_t args[] = { err, array };

        zjs_callback_id id = zjs_add_callback_once(argv[1], this, NULL, NULL);
        zjs_signal_callback(id, args, sizeof(args));
        return ZJS_UNDEFINED;
    }
#endif
    return array;
}

static ZJS_DECL_FUNC(zjs_fs_readdir_sync)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_readdir, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_readdir_async)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_readdir, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_stat, u8_t async)
{
    // args: filepath
    ZJS_VALIDATE_ARGS(Z_STRING);

#ifdef ZJS_FS_ASYNC_APIS
    // async case adds callback arg
    if (async) {
        ZJS_VALIDATE_ARGS_OFFSET(1, Z_FUNCTION);
    }
#endif

    jerry_size_t size = MAX_PATH_LENGTH;
    char path[size];

    zjs_copy_jstring(argv[0], path, &size);
    if (!size) {
        return zjs_error("size mismatch");
    }

    int ret;
    struct fs_dirent entry;

    ret = fs_stat(path, &entry);
    if (ret != 0) {
        // TODO: Decide what to do with FStat if the file doesn't exists,
        // the current work-around is to return undefined value.
        return ZJS_UNDEFINED;
    }

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        if (ret != 0) {
            jerry_value_t args[] = { ZJS_UNDEFINED };
            zjs_callback_id id = zjs_add_callback_once(argv[1], this, NULL, NULL);
            zjs_signal_callback(id, args, sizeof(args));
        } else {
            ZVAL ret_val = jerry_create_number(ret);
            ZVAL stats = create_stats_obj(&entry);
            jerry_value_t args[] = { ret_val, stats };

            zjs_callback_id id = zjs_add_callback_once(argv[1], this, NULL, NULL);
            zjs_signal_callback(id, args, sizeof(args));
        }
        return ZJS_UNDEFINED;
    }
#endif
    return create_stats_obj(&entry);
}

static ZJS_DECL_FUNC(zjs_fs_stat_sync)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_stat, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_stat_async)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_stat, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_write_file, u8_t async)
{
    // args: filepath, data
    ZJS_VALIDATE_ARGS(Z_STRING, Z_BUFFER Z_STRING);

#ifdef ZJS_FS_ASYNC_APIS
    // async case adds callback arg
    if (async) {
        ZJS_VALIDATE_ARGS_OFFSET(2, Z_FUNCTION);
    }
#endif

    u8_t is_buf = 0;
    jerry_size_t size;
    int error = 0;
    char *data = NULL;
    u32_t length;
    if (jerry_value_is_string(argv[1])) {
        size = 0;
        data = zjs_alloc_from_jstring(argv[1], &size);
        length = size;
    } else {
        zjs_buffer_t *buffer = zjs_buffer_find(argv[1]);
        data = buffer->buffer;
        length = buffer->bufsize;
        is_buf = 1;
    }

    size = 32;
    char *path = zjs_alloc_from_jstring(argv[0], &size);
    if (!path) {
        if (data && !is_buf) {
            zjs_free(data);
        }
        return zjs_error("path string too long\n");
    }

    fs_file_t fp;
    error = fs_open(&fp, path);
    if (error != 0) {
        ERR_PRINT("error opening file, error=%d\n", error);
        goto Finished;
    }
    if (fs_truncate(&fp, 0) != 0) {
        ERR_PRINT("could not truncate file: %s\n", path);
        goto Finished;
    }

    ssize_t written = fs_write(&fp, data, length);

    if (written != length) {
        ERR_PRINT("could not write %u bytes, only %u were written\n",
                  (unsigned int)length, (unsigned int)written);
        error = -1;
    }

    error = fs_close(&fp);
    if (error != 0) {
        ERR_PRINT("error closing file\n");
    }

Finished:
#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        ZVAL err = jerry_create_number(error);

        zjs_callback_id id = zjs_add_callback_once(argv[2], this, NULL, NULL);
        zjs_signal_callback(id, &err, sizeof(err));
    }
#endif
    if (data && !is_buf) {
        zjs_free(data);
    }
    zjs_free(path);
    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_fs_write_file_sync)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_write_file, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_write_file_async)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_write_file, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_read_file, u8_t async)
{
    // args: filepath, data
    ZJS_VALIDATE_ARGS(Z_STRING);

#ifdef ZJS_FS_ASYNC_APIS
    // async case adds callback arg
    if (async) {
        ZJS_VALIDATE_ARGS_OFFSET(1, Z_FUNCTION);
    }
#endif
    jerry_value_t buffer = ZJS_UNDEFINED;
    jerry_size_t size = 32;
    char *path = zjs_alloc_from_jstring(argv[0], &size);
    if (!path) {
        return zjs_error("path string too long");
    }
    struct fs_dirent entry;
    int ret = fs_stat(path, &entry);
    if (ret != 0) {
        ERR_PRINT("error getting stats on file, error=%d\n", ret);
        goto Finished;
    }
    zjs_buffer_t *buf_handle;
    buffer = zjs_buffer_create(entry.size, &buf_handle);

    fs_file_t fp;
    ret = fs_open(&fp, path);
    if (ret != 0) {
        ERR_PRINT("error opening file, error=%d\n", ret);
        goto Finished;
    }
    if (fs_seek(&fp, 0, SEEK_SET) != 0) {
        return zjs_error("error seeking to position");
    }

    int len = fs_read(&fp, buf_handle->buffer, buf_handle->bufsize);
    if (len != buf_handle->bufsize) {
        ERR_PRINT("read length was incorrect\n");
    }
Finished:
#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        ZVAL err = jerry_create_number(ret);
        jerry_value_t args[] = { err, buffer };

        zjs_callback_id id = zjs_add_callback_once(argv[1], this, NULL, NULL);
        zjs_signal_callback(id, args, sizeof(args));
    }
    return ZJS_UNDEFINED;
#endif

    return buffer;
}

static ZJS_DECL_FUNC(zjs_fs_read_file_sync)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_read_file, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_read_file_async)
{
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_read_file, 1);
}
#endif

static void zjs_fs_cleanup(void *native)
{
    ZJS_LIST_FREE(file_handle_t, opened_handles, free_file);
}

static const jerry_object_native_info_t fs_module_type_info = {
    .free_cb = zjs_fs_cleanup
};

static jerry_value_t zjs_fs_init()
{
    jerry_value_t fs = zjs_create_object();

    zjs_obj_add_function(fs, "openSync", zjs_fs_open_sync);
    zjs_obj_add_function(fs, "closeSync", zjs_fs_close_sync);
    zjs_obj_add_function(fs, "unlinkSync", zjs_fs_unlink_sync);
    zjs_obj_add_function(fs, "rmdirSync", zjs_fs_unlink_sync);
    zjs_obj_add_function(fs, "readSync", zjs_fs_read_sync);
    zjs_obj_add_function(fs, "writeSync", zjs_fs_write_sync);
    zjs_obj_add_function(fs, "truncateSync", zjs_fs_truncate_sync);
    zjs_obj_add_function(fs, "mkdirSync", zjs_fs_mkdir_sync);
    zjs_obj_add_function(fs, "readdirSync", zjs_fs_readdir_sync);
    zjs_obj_add_function(fs, "statSync", zjs_fs_stat_sync);
    zjs_obj_add_function(fs, "writeFileSync", zjs_fs_write_file_sync);
    zjs_obj_add_function(fs, "readFileSync", zjs_fs_read_file_sync);

#ifdef ZJS_FS_ASYNC_APIS
    zjs_obj_add_function(fs, "open", zjs_fs_open_async);
    zjs_obj_add_function(fs, "close", zjs_fs_close_async);
    zjs_obj_add_function(fs, "unlink", zjs_fs_unlink_async);
    zjs_obj_add_function(fs, "rmdir", zjs_fs_unlink_async);
    zjs_obj_add_function(fs, "read", zjs_fs_read_async);
    zjs_obj_add_function(fs, "write", zjs_fs_write_async);
    zjs_obj_add_function(fs, "truncate", zjs_fs_truncate_async);
    zjs_obj_add_function(fs, "mkdir", zjs_fs_mkdir_async);
    zjs_obj_add_function(fs, "readdir", zjs_fs_readdir_async);
    zjs_obj_add_function(fs, "stat", zjs_fs_stat_async);
    zjs_obj_add_function(fs, "writeFile", zjs_fs_write_file_async);
    zjs_obj_add_function(fs, "readFile", zjs_fs_read_file_async);
#endif
    // Set up cleanup function for when the object gets freed
    jerry_set_object_native_pointer(fs, NULL, &fs_module_type_info);
    return fs;
}

JERRYX_NATIVE_MODULE(fs, zjs_fs_init)
#endif
