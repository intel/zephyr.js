ZJS API for File System
==================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Class FS](#fs-api)
  * [fs.openSync(path, mode)](#fsopensyncpath-mode)
  * [fs.closeSync(fd)](#fsclosesyncfd)
  * [fs.unlinkSync(path)](#fsunlinksyncpath)
  * [fs.rmdirSync(path)](#fsrmdirsyncpath)
  * [fs.writeSync(fd, data, offset, length, [position])](#fswritesyncfd-data-offset-length-position)
  * [fs.readSync(fd, data, offset, length, [position])](#fsreadsyncfd-data-offset-length-position)
  * [fs.truncateSync(path, length)](#fstruncatesyncpath-length)
  * [fs.mkdirSync(path)](#fsmkdirsyncpath)
  * [fs.readdirSync(path)](#fsreaddirsyncpath)
  * [fs.statSync(path)](#fsstatsyncpath)
  * [writeFileSync(file, data)](#writefilesyncfile-data)
* [Class Stat](#stat-api)
  * [stat.isFile()](#statisfile)
  * [stat.isDirectory()](#statisdirectory)

Introduction
------------

ZJS provides File System APIs that match Node.js' FS module. We
describe them here to document any minor differences. It should be
noted that by default, the FS module only contains the synchronous
Node.js APIs. The asynchronous APIs can be compiled in by enabling the
pre-processor value `ZJS_FS_ASYNC_APIS`. The default is to leave them
out, because all of Zephyr's File System APIs are synchronous, so
making the JavaScript APIs asynchronous was only adding ROM space.

On the Arduino 101, the flash file system uses SPI, and the pins are shared with
IO10-13. For this reason, you will not be able to use these GPIO pins at the
same time as the file system.

Available file modes:

`'r'` - Open file for only reading. An error will be thrown if the file does
not exist.

`'r+'` - Open a file for reading and writing. An error will be thrown if the
file does not exist.

`'w'` - Open a file for writing. The file will be overwritten if it already
exists.

`'w+'` - Open a file for writing and reading. The file will be overwritten if
it already exists.

`'a'` - Opens a file for appending. The write pointer will always seek
to the end of the file during a write.

`'a+'` - Opens a file for appending and reading. The write
pointer will seek to the end for writes, but reads will be done from the
start of the file (the read pointer is saved across different read calls).

Web IDL
-------
This IDL provides an overview of the interface; see below for
documentation of specific API functions.  We have a short document
explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).

<details>
<summary>Click to show WebIDL</summary>
<pre>
// require returns a FS object
// var fs = require('fs');
[ReturnFromRequire, ExternalInterface=(Buffer)]
interface FS {
    FileDescriptor openSync(string path, FileMode mode);
    void closeSync(FileDescriptor fd);
    void unlinkSync(string path);
    void rmdirSync(string path);
    long writeSync(FileDescriptor fd, (string or Buffer) data, long offset,
                   long length, optional long position);
    long readSync(FileDescriptor fd, Buffer data, long offset,
                  long length, optional long position);
    void truncateSync(string path, long length);
    void mkdirSync(string path);
    sequence < string > readdirSync(string path);
    Stat statSync(string path);
    void writeFileSync(string file, (string or Buffer) data);
};<p>
// file descriptors are inherently platform specific, so we leave this
// as a placeholder
dictionary FileDescriptor {
    //string name;
};<p>interface Stat {
    boolean isFile();
    boolean isDirectory();
};<p>enum FileMode { "r", "w", "a", "r+", "w+", "a+" };</pre>
</details>

FS API
------

### fs.openSync(path, mode)
* `path` *string* The name and path of the file to open.
* `mode` *FileMode* The mode in which to open the file.
* Returns: an object representing the file descriptor.

Opens a file.

### fs.closeSync(fd)
* `fd` *FileDescriptor* The file descriptor for the file that will be closed.

Closes a file.

### fs.unlinkSync(path)
* `path` *string* The name and path of the file to remove.

Unlink (remove) a file from the file system.

### fs.rmdirSync(path)
* `path` *string* The name and path of the directory to be removed.

Remove a directory from the file system.

### fs.writeSync(fd, data, offset, length, [position])
* `fd` *FileDescriptor* The file descriptor returned from `openSync()`.
* `data` *string or Buffer* The data to write to 'fd'.
* `offset` *long* The position in 'data' from which to start writing.
* `length` *long* The number of bytes to write to 'fd' from 'data'.
* `position` *long* The offset from the beginning of the file where
  'data' should be written. The parameter is optional; the default value is 0.
* Returns: the number of bytes actually written (this may be different from 'length').

Write bytes to an opened file.

### fs.readSync(fd, data, offset, length, [position])
* `fd` *FileDescriptor* The file descriptor returned from 'openSync()'.
* `data` *Buffer* The buffer into which the data will be read.
* `offset` *long* The offset in 'data' at which to start writing.
* `length` *long* The number of bytes to read.
* `position` *long* The position in the file from which to start reading.
* Returns: the number of bytes actually read. This may be different from
'length' if there was a read error or if the file had no more data left to read.

Read bytes from a file.

### fs.truncateSync(path, length)
* `path` *string* The name and path of the file.
* `length` *long* The new length of the file.

Truncate a file. If the length passed in is shorter than the existing file
length, then the trailing file data will be lost.

### fs.mkdirSync(path)
* `path` *string* The name and path of the directory.

Create a directory. There is no effect if the directory already exists.

### fs.readdirSync(path)
* `path` *string* The name and path of the directory to read.
* Returns: an array of filenames and directories found in 'path'.

Read the contents of a directory.

### fs.statSync(path)
* `path` *string* The name and path of the file or directory.
* Returns: a 'Stat' object for the file or directory or undefined if the
file or directory does not exist.

Get stats about a file or directory.

### writeFileSync(file, data)
* `file` *string* The name of the file to which to write.
* `data` *string or Buffer* The data to write into the file.

Open and write data to a file. This will replace the file if it already exists.

Stat API
--------

### stat.isFile()
* Returns: true if the file descriptor is a file.

### stat.isDirectory()
* Returns: true if the file descriptor is a directory.

Sample Apps
-----------
* [FS test](../tests/test-fs.js)
