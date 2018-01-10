ZJS API for Buffer
==================

* [Introduction](#introduction)
* [Class: Buffer](#buffer-api)
  * [new Buffer(array)](#new-bufferarray)
  * [new Buffer(size)](#new-buffersize)
  * [new Buffer(string)](#new-bufferstring)
  * [buf.copy(target[, targetStart, [sourceStart[, sourceEnd]]])](#bufcopytarget-targetstart-sourcestart-sourceend)
  * [buf.fill(value[, offset[, end[, encoding]]])](#buffillvalue-offset-end-encoding)
  * [buf.readUInt*(offset)](#bufreaduint-family)
  * [buf.toString([encoding])](#buftostringencoding)
  * [buf.write(string[, offset[, length[, encoding]]])](#bufwritestring-offset-length-encoding)
  * [buf.writeUInt*(value, offset)](#bufwriteuint-family)
* [Sample Apps](#sample-apps)

Introduction
------------
Buffer is a [Node.js API](https://nodejs.org/dist/latest-v8.x/docs/api/buffer.html)
to read and write binary data accurately from JavaScript. ZJS supports a minimal
subset of this API that will be expanded as the need arises.

Buffer API
----------
### new Buffer(array)
* `array` *integer[]* Array of octets to use as initial data.

The `array` argument should be an array of numbers that will be treated as
UInt8 integers. A new buffer object will be returned with the same size as the
array and initialized with its contents. If there is not enough available
memory, an error will be thrown.

### new Buffer(size)
* `size` *integer* Length in bytes of the new buffer.

The `size` argument specifies the length in bytes of the array that the Buffer
represents. If a negative length is passed, a 0-length Buffer will be returned.
If the size is too long and there is not enough available memory, an error will
be thrown.

### new Buffer(string)
* `string` *string* String to use as initial data.

The `string` argument will be treated as UTF8 and used to initialize the new
buffer. If there is not enough available memory, an error will be thrown.

### buf.copy(target[, targetStart, [sourceStart[, sourceEnd]]])
* `target` *Buffer* Buffer to receive the copied data.
* `targetStart` *integer* Offset to start writing at in the target buffer.
* `sourceStart` *integer* Offset to start reading from in the source buffer.
* `sourceEnd` *integer* Offset at which to stop reading from the source (not
inclusive).
* Returns: *integer* Number of bytes copied.

Copies data from this `buf` to `target`. Start offsets default to 0 and
`sourceEnd` defaults to the end of the source buffer. If there is not enough
room in the target, throws an error.

### buf.fill(value[, offset[, end[, encoding]]])
* `value` *string* | *Buffer* | *integer* Value to fill buffer with.
* `offset` *integer* Offset to start writing at in the buffer.
* `end` *integer* Offset at which to stop writing (not inclusive).
* `encoding` *string* Encoding type.
* Returns: *Buffer* A reference to `buf`.

Repeatedly copies bytes from the source number, buffer, or string, until the
buffer is filled from `offset` to `end` (which default to the beginning and end
of the buffer. Only default "utf8" encoding is accepted currently. Treats
numbers as four byte integers.

### buf.readUInt family

#### buf.readUInt8(offset)
#### buf.readUInt16BE(offset)
#### buf.readUInt16LE(offset)
#### buf.readUInt32BE(offset)
#### buf.readUInt32LE(offset)
* `offset` *integer* Number of bytes to skip before reading integer.
* Returns: *integer*

Reads 1, 2, or 4 bytes at `offset` as a big-endian (highest byte first) or
little-endian (lowest byte first) integer depending on the function version.
The `offset` should be provided but will be treated as 0 if not given. Returns
an error if the buffer is not big enough.

### buf.toString([encoding])
* `encoding` *string* Encoding to use.
* Returns: *string*

Currently, the supported encodings are 'utf8' (default), 'ascii', and 'hex'.
If 'ascii' is given, drops the high bit from every character and terminates at
'\0' byte if found. If 'hex' is given, returns the contents of the buffer
encoded in hexadecimal digits (two characters per byte). Otherwise, returns an
error.

### buf.write(string[, offset[, length[, encoding]]])
* `string` *string* String to write to buf.
* `offset` *integer* Offset to start writing at in the buffer.
* `length` *integer* Number of bytes to write.
* `encoding` *string* Encoding to use.
* Returns: *integer* Number of bytes written.

Writes bytes from `string` to buffer at `offset`, stopping after `length` bytes.
The default `offset` is 0 and default `length` is buffer length - `offset`. Only
'utf8' encoding is supported currently.

### buf.writeUInt family

#### writeUInt8(value, offset)
#### writeUInt16BE(value, offset)
#### writeUInt16LE(value, offset)
#### writeUInt32BE(value, offset)
#### writeUInt32LE(value, offset)
* `value` *integer* Number to write.
* `offset` *integer* Number of bytes to skip before writing value.
* Returns: *integer* `offset` plus the bytes written.

The `value` will be treated as 1, 2, or 4 byte big-endian (highest byte first)
or little-endian (lowest byte first) integer depending on the function version.
The `offset` should be provided but will be treated as 0 if not given. Returns
the new offset just beyond what was written to the buffer. If the target area
goes outside the bounds of the Buffer, returns an error.

Sample Apps
-----------
* [Buffer sample](../samples/Buffer.js)
* [WebBluetooth Demo](../samples/WebBluetoothDemo.js)
