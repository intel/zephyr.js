ZJS API for File System
==================

* [Introduction](#introduction)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
ZJS provides File System API's which match Node.js' FS module. We describe them
here as there could potentially be minor differences. It should be noted that by
default the FS module only contains the synchronous Node.js API's. The
asynchronous API's can be compiled in by enabling the pre-processor define
`ZJS_FS_ASYNC_APIS`. They are compiled out because all of Zephyr's File
System API's are synchronous, so making the JavaScript API's asynchronous was
only adding ROM space.

On the Arduino 101 the flash file system uses SPI and the pins are shared with
IO10-13. For this reason you will not be able to use these GPIO pins at the
same time as the file system.

Available file modes:

`'r'` - Open file for only reading. An error will be thrown if the file does
not exist.

`'r+'` - Open a file for reading and writing. An error will be thrown if the
file does not exist.

`'w'` - Opens a file for writing. The file will be overwritten if it already
exists.

`'w+'` - Opens a file for writing and reading. The file will be overwritten if
it already exists.

`'a'` - Opens a file for appending. The write pointer will always seek
to the end of the file during a write.

`'a+'` - Opens a file for appending and reading. As with `'a'` the write
pointer will seek to the end for writes, but reads can be done from the
start of the file (read pointer saved across different read calls).

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns a FS object
// var fs = require('fs');

interface Stat {
    boolean isFile();
    boolean isDirectory();
};
```

API Documentation
-----------------

### FS.openSync
`object openSync(string path, string mode);`

Opens a file.

`path` is the name/path of the file to open.

`mode` is the mode to open the file in (r/w/a/r+/w+/a+).

Returns an object representing the file descriptor.

### FS.closeSync
`void closeSync(object fd);`

Closes a file.

`fd` is the descriptor returned from `openSync()`.

### FS.unlinkSync
`void unlinkSync(string path);`

Unlink (remove) a file from the file system.

`path` is the file to remove.

### FS.rmdirSync
`void rmdirSync(string path);`

Remove a directory from the file system.

`path` is the name of the directory.

### FS.writeSync
`number writeSync(object fd, [String|Buffer] data, number offset, number length, optional number position);`

Write bytes to an opened file.

`fd` is the file descriptor returned from `openSync()`.

`data` is either a string or buffer to write.

`offset` is the position in `data` to start writing from.

`length` is the number of bytes to write from `data`.

`position` is the offset from the beginning of the file where `data` should be
written. Default is 0.

Returns the number of bytes actually written (this may be different from `length`).

### FS.readSync
`number readSync(object fd, buffer data, number offset, number length, number position);`

Read bytes from a file.

`fd` is the file descriptor returned from `openSync()`.

`data` is a buffer where the data will be read into.

`offset` is the offset in `data` to start writing at.

`length` is the number of bytes to read.

`position` is the position in the file to start reading from.

Returns the number of bytes actually read. This may be different from `length`
if there was a read error or if the file had no more data left to read.

### FS.truncateSync
`void truncateSync(string path, number length);`

Truncate a file. If the length passed in is shorter than the existing file
length then the trailing file data will be lost.

`path` is the name of the file.

`length` is the new length of the file.

### FS.mkdirSync
`void mkdirSync(string path)`

Create a directory. If the directory already exists there is no effect.

`path` is the name of the directory.

### FS.readdirSync
`string[] readdirSync(string path);`

Read the contents of a directory.

`path` directory path to read.

Returns an array of filenames and directories found in `path`.

### FS.statSync
`Stat statSync(string path);`

Get stats about a file or directory.

`path` name of file or directory.

Returns a `Stat` object for that file or directory.

### FS.writeFileSync
`void writeFileSync(string file, [string|buffer] data);`

Open and write data to a file. This will replace the file if it already exists.

`file` is the name of the file to write to.

`data` is the bytes to write into the file.

Sample Apps
-----------
* [FS test](../tests/test-fs.js)
