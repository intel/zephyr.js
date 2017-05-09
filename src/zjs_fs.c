// Copyright (c) 2016-2017, Intel Corporation.

#ifdef BUILD_MODULE_FS

#include <zephyr.h>
#include <fs.h>

#include <string.h>

#include "zjs_callbacks.h"
#include "zjs_common.h"
#include "zjs_util.h"
#include "zjs_buffer.h"

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

/*
 * Bit mask of currently open FD's
 */
static uint32_t fd_used = 0;

typedef struct file_handle {
    fs_file_t fp;
    int fd;
    FileMode mode;
    int error;
    uint32_t rpos;
    struct file_handle *next;
} file_handle_t;

static file_handle_t *opened_handles = NULL;

static jerry_value_t invalid_args(void)
{
    return zjs_error("invalid arguments");
}

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

static uint16_t get_mode(char *str)
{
    uint16_t mode = 0;
    if (strcmp(str, "r") == 0) {
        mode = MODE_R;
    } else if (strcmp(str, "r+") == 0) {
        mode = MODE_R_PLUS;
    } else if (strcmp(str, "w") == 0) {
        mode = MODE_W;
    } else if (strcmp(str, "w+") == 0) {
        mode = MODE_W_PLUS;
    } else if (strcmp(str, "a") == 0) {
        mode = MODE_A;
    } else if (strcmp(str, "a+") == 0) {
        mode = MODE_A_PLUS;
    }
    return mode;
}

static ZJS_DECL_FUNC(is_file)
{
    struct fs_dirent *entry;

    if (!jerry_get_object_native_handle(this, (uintptr_t *)&entry)) {
        return zjs_error("native handle not found");
    }
    if (entry->type == FS_DIR_ENTRY_FILE) {
        return jerry_create_boolean(true);
    } else {
        return jerry_create_boolean(false);
    }
}

static ZJS_DECL_FUNC(is_directory)
{
    struct fs_dirent *entry;

    if (!jerry_get_object_native_handle(this, (uintptr_t *)&entry)) {
        return zjs_error("native handle not found");
    }
    if (entry->type == FS_DIR_ENTRY_DIR) {
        return jerry_create_boolean(true);
    } else {
        return jerry_create_boolean(false);
    }
}

static void free_stats(const uintptr_t native)
{
    struct zfs_dirent *entry = (struct zfs_dirent *)native;
    if (entry) {
        zjs_free(entry);
    }
}

static jerry_value_t create_stats_obj(struct fs_dirent *entry)
{

    jerry_value_t stats_obj = jerry_create_object();

    struct fs_dirent *new_entry = zjs_malloc(sizeof(struct fs_dirent));
    if (!new_entry) {
        return zjs_error("malloc failed");
    }
    memcpy(new_entry, entry, sizeof(struct fs_dirent));

    jerry_set_object_native_handle(stats_obj, (uintptr_t)new_entry, free_stats);

    zjs_obj_add_function(stats_obj, is_file, "isFile");
    zjs_obj_add_function(stats_obj, is_directory, "isDirectory");

    return stats_obj;
}

static ZJS_DECL_FUNC_ARGS(zjs_fs_open, uint8_t async)
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

    if ((m == MODE_R || m == MODE_R_PLUS)
        && !file_exists(path)) {
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

    handle->mode = m;
    handle->next = opened_handles;
    opened_handles = handle;

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        ZVAL err = jerry_create_number(handle->error);
        ZVAL fd_val = jerry_create_number(handle->fd);
        jerry_value_t args[] = {err, fd_val};

        zjs_callback_id id = zjs_add_callback_once(argv[2], this, handle, NULL);
        zjs_signal_callback(id, args, sizeof(args));
        return ZJS_UNDEFINED;
    }
#endif

    return jerry_create_number(handle->fd);
}

static ZJS_DECL_FUNC(zjs_fs_open_sync) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_open, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_open_async) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_open, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_close, uint8_t async)
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

        zjs_callback_id id = zjs_add_callback_once(argv[1],
                                                   this,
                                                   handle,
                                                   NULL);
        zjs_signal_callback(id, &error, sizeof(error));
    }
#endif

    ZJS_LIST_REMOVE(file_handle_t, opened_handles, handle);
    free_file(handle);

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_fs_close_sync) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_close, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_close_async) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_close, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_unlink, uint8_t async)
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

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        ZVAL error = jerry_create_number(ret);

        zjs_callback_id id = zjs_add_callback_once(argv[1],
                                                   this,
                                                   NULL,
                                                   NULL);
        zjs_signal_callback(id, &error, sizeof(error));
    }
#endif

    return ZJS_UNDEFINED;
}

static ZJS_DECL_FUNC(zjs_fs_unlink_sync) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_unlink, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_unlink_async) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_unlink, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_read, uint8_t async)
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
        if (fs_seek(&handle->fp, (uint32_t)position, SEEK_SET) != 0) {
            return zjs_error("error seeking to position");
        }
    }

    DBG_PRINT("reading into fp=%p, buffer=%p, offset=%lu, length=%lu\n",
              &handle->fp, buffer->buffer, (uint32_t)offset, (uint32_t)length);

    uint32_t ret = fs_read(&handle->fp, buffer->buffer + (uint32_t)offset,
                           (uint32_t)length);

    if (ret != (uint32_t)length) {
        DBG_PRINT("could not read %lu bytes, only %lu were read\n",
                  (uint32_t)length, ret);
        err = -1;
    }
    handle->rpos += ret;

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        ZVAL err_val = jerry_create_number(err);
        ZVAL ret_val = jerry_create_number(ret);
        jerry_value_t args[] = {err_val, ret_val, argv[1]};

        zjs_callback_id id = zjs_add_callback_once(argv[5],
                                                   this,
                                                   NULL,
                                                   NULL);
        zjs_signal_callback(id, args, sizeof(args));
        return ZJS_UNDEFINED;
    }
#endif
    return jerry_create_number(ret);
}

static ZJS_DECL_FUNC(zjs_fs_read_sync) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_read, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_read_async) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_read, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_write, uint8_t async)
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
    uint32_t offset = 0;
    uint32_t length = 0;
    uint32_t position = 0;
    uint8_t from_cur = 0;

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

    uint32_t written = fs_write(&handle->fp, buffer->buffer + offset, length);
    if (written != length) {
        DBG_PRINT("could not write %lu bytes, only %lu were written\n", length,
                  written);
    }

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        ZVAL err = jerry_create_number(0);
        ZVAL bytes = jerry_create_number(written);
        jerry_value_t args[] = {err, bytes, argv[1]};

        zjs_callback_id id = zjs_add_callback_once(argv[cbindex], this, NULL,
                                                   NULL);
        zjs_signal_callback(id, args, sizeof(args));
        return ZJS_UNDEFINED;
    }
#endif
    return jerry_create_number(written);
}

static ZJS_DECL_FUNC(zjs_fs_write_sync) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_write, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_write_async) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_write, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_truncate, uint8_t async)
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

static ZJS_DECL_FUNC(zjs_fs_truncate_sync) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_truncate, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_truncate_async) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_truncate, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_mkdir, uint8_t async)
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

static ZJS_DECL_FUNC(zjs_fs_mkdir_sync) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_mkdir, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_mkdir_async) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_mkdir, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_readdir, uint8_t async)
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
        ZVAL err = jerry_create_number(res);
        jerry_value_t args[] = {err, array};

        zjs_callback_id id = zjs_add_callback_once(argv[1],
                                               this,
                                               NULL,
                                               NULL);
        zjs_signal_callback(id, args, sizeof(args));
        return ZJS_UNDEFINED;
    }
#endif
    return array;
}

static ZJS_DECL_FUNC(zjs_fs_readdir_sync) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_readdir, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_readdir_async) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_readdir, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_stat, uint8_t async)
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

#ifdef ZJS_FS_ASYNC_APIS
    if (async) {
        ZVAL ret_val = jerry_create_number(ret);
        ZVAL stats = create_stats_obj(&entry);
        jerry_value_t args[] = {ret_val, stats};

        zjs_callback_id id = zjs_add_callback_once(argv[1],
                                                   this,
                                                   NULL,
                                                   NULL);
        zjs_signal_callback(id, args, sizeof(args));
        return ZJS_UNDEFINED;
    }
#endif
    return create_stats_obj(&entry);
}

static ZJS_DECL_FUNC(zjs_fs_stat_sync) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_stat, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_stat_async) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_stat, 1);
}
#endif

static ZJS_DECL_FUNC_ARGS(zjs_fs_write_file, uint8_t async)
{
    // args: filepath, data
    ZJS_VALIDATE_ARGS(Z_STRING, Z_BUFFER Z_STRING);

#ifdef ZJS_FS_ASYNC_APIS
    // async case adds callback arg
    if (async) {
        ZJS_VALIDATE_ARGS_OFFSET(2, Z_FUNCTION);
    }
#endif

    uint8_t is_buf = 0;
    jerry_size_t size;
    int error = 0;
    char *data = NULL;
    uint32_t length;
    if (jerry_value_is_string(argv[1])) {
        size = 256;
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

static ZJS_DECL_FUNC(zjs_fs_write_file_sync) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_write_file, 0);
}

#ifdef ZJS_FS_ASYNC_APIS
static ZJS_DECL_FUNC(zjs_fs_write_file_async) {
    return ZJS_CHAIN_FUNC_ARGS(zjs_fs_write_file, 1);
}
#endif

jerry_value_t zjs_fs_init()
{
    jerry_value_t fs = jerry_create_object();

    zjs_obj_add_function(fs, zjs_fs_open_sync, "openSync");
    zjs_obj_add_function(fs, zjs_fs_close_sync, "closeSync");
    zjs_obj_add_function(fs, zjs_fs_unlink_sync, "unlinkSync");
    zjs_obj_add_function(fs, zjs_fs_unlink_sync, "rmdirSync");
    zjs_obj_add_function(fs, zjs_fs_read_sync, "readSync");
    zjs_obj_add_function(fs, zjs_fs_write_sync, "writeSync");
    zjs_obj_add_function(fs, zjs_fs_truncate_sync, "truncateSync");
    zjs_obj_add_function(fs, zjs_fs_mkdir_sync, "mkdirSync");
    zjs_obj_add_function(fs, zjs_fs_readdir_sync, "readdirSync");
    zjs_obj_add_function(fs, zjs_fs_stat_sync, "statSync");
    zjs_obj_add_function(fs, zjs_fs_write_file_sync, "writeFileSync");

#ifdef ZJS_FS_ASYNC_APIS
    zjs_obj_add_function(fs, zjs_fs_open_async, "open");
    zjs_obj_add_function(fs, zjs_fs_close_async, "close");
    zjs_obj_add_function(fs, zjs_fs_unlink_async, "unlink");
    zjs_obj_add_function(fs, zjs_fs_unlink_async, "rmdir");
    zjs_obj_add_function(fs, zjs_fs_read_async, "read");
    zjs_obj_add_function(fs, zjs_fs_write_async, "write");
    zjs_obj_add_function(fs, zjs_fs_truncate_async, "truncate");
    zjs_obj_add_function(fs, zjs_fs_mkdir_async, "mkdir");
    zjs_obj_add_function(fs, zjs_fs_readdir_async, "readdir");
    zjs_obj_add_function(fs, zjs_fs_stat_async, "stat");
    zjs_obj_add_function(fs, zjs_fs_write_file_async, "writeFile");
#endif

    return fs;
}

void zjs_fs_cleanup()
{
    ZJS_LIST_FREE(file_handle_t, opened_handles, free_file);
}
#endif
