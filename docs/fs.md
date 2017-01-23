ZJS API for File System
==================

* [Introduction](#introduction)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
ZJS provides File System API's which match Node.js' FS module. We describe them here as there could
potentially be minor differences. It should be noted that by default the FS module only contains
the synchronous Node.js API's. The asynchronous API's can be compiled in by enabling the pre-processor
define `ZJS_FS_ASYNC_APIS`. They are compiled out because all of Zephyr's File System API's are
synchronous, so making the JavaScript API's asynchronous was only adding ROM space.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript

interface Stat {
    boolean isFile();
    boolean isDirectory();
};
```

API Documentation
-----------------

### openSync
`object openSync(string path, string mode);`

Opens a file.

`path` is the name/path of the file to open

`mode` is the mode to open the file in (w, r, w+ etc)

Returns an object representing the file descriptor

Note: On Zephyr, the underlying API's do not take a file mode. This API will do basic checking
to make sure the mode matches characteristics of the file i.e. whether it exists or not

### closeSync
`void closeSync(object fd);`

Closes a file.

`fd` is the descriptor returned from `openSync()`

### unlinkSync
`void unlinkSync(string path);`

Unlink (remove) a file from the file system

`path` is the name of the file or directory

### rmdirSync
`void rmdirSync(string path);`

Remove a directory from the file system

`path` is the name of the directory.

### writeSync
`number writeSync(object fd, [String|Buffer] data, number offset, number length, optional number position);`

Write bytes to an opened file.

`fd` is the file descriptor returned from `openSync()`

`data` is either a string or buffer to write

`offset` is the position in `data` to start writing from

`length` is the number of bytes to write from `data`

`position` is the offset from the beginning of the file where `data` should be written.

Returns the number of bytes actually written (this may be different from `length`).

### readSync
`number readSync(object fd, buffer data, number offset, number length, number position);`

Read bytes from a file.

`fd` is the file descriptor returned from `openSync()`

`data` is a buffer where the data will be read into

`offset` is the offset in `data` to start writing at

`length` is the number of bytes to read

`position` is the position in the file to start reading from

Returns the number of bytes actually read (this may be different from `length`)

### truncateSync
`void truncateSync(string path, number length);`

Truncate a file.

`path` is the name of the file

`length` is the new length of the file.

### mkdirSync
`void mkdirSync(string path)`

Create a directory

`path` is the name of the directory

### readdirSync
`string[] readdirSync(string path);`

Read the contents of a directory.

`path` directory path to read.

Returns an array of file names that were found in `path`

###  statSync
`Stat statSync(string path);`

Get stats about a file or directory.

`path` name of file or directory.

Returns a `Stat` object for that file or directory

### writeFileSync
`void writeFileSync(string file, [string|buffer] data);`

Open and write data to a file. This will replace the file if it already exists.

`file` is the name of the file to write to

`data` is the bytes to write into the file

Sample Apps
-----------
* [FS test](../tests/test-fs.js)
