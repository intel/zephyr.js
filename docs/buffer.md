ZJS API for Buffer
==================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
Buffer is a [node.js API]
(https://nodejs.org/dist/latest-v6.x/docs/api/buffer.html)
to read and write binary data accurately from JavaScript. ZJS supports a minimal
subset of this API that will be expanded as the need arises.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// Buffer is a global object constructor that is always available

[Constructor(unsigned long length)]
interface Buffer {
    unsigned char readUInt8(unsigned long offset);
    void writeUInt8(unsigned char value, unsigned long offset);
    unsigned short readUInt16BE(unsigned long offset);
    void writeUInt16BE(unsigned short value, unsigned long offset);
    unsigned short readUInt16LE(unsigned long offset);
    void writeUInt16LE(unsigned short value, unsigned long offset);
    unsigned long readUInt32BE(unsigned long offset);
    void writeUInt32BE(unsigned long value, unsigned long offset);
    unsigned long readUInt32LE(unsigned long offset);
    void writeUInt32LE(unsigned long value, unsigned long offset);
    string toString(string encoding);
    readonly attribute unsigned long length;
};
```

API Documentation
-----------------
### Buffer constructor

`Buffer(unsigned long length);`

The `length` argument specifies the length in bytes of the Buffer object.

### Buffer.readUInt family

```javascript
unsigned char readUInt8(unsigned long offset);
unsigned short readUInt16BE(unsigned long offset);
unsigned short readUInt16LE(unsigned long offset);
unsigned long readUInt32BE(unsigned long offset);
unsigned long readUInt32LE(unsigned long offset);
```

The `offset` should be provided but will be treated as 0 if not given. Returns
the value of the short or long located at the given offset. If the offset is
outside the bounds of the Buffer, returns an error.

The `BE` or `LE` refers to whether the bytes will be read in big-endian
(highest byte first) or little-endian (lowest byte first) order.

### Buffer.writeUInt family

```javascript
void writeUInt8(unsigned char value, unsigned long offset);
void writeUInt16BE(unsigned short value, unsigned long offset);
void writeUInt16LE(unsigned short value, unsigned long offset);
void writeUInt32BE(unsigned long value, unsigned long offset);
void writeUInt32LE(unsigned long value, unsigned long offset);
```

The `offset` should be provided but will be treated as 0 if not given. Writes
`value` to the buffer starting at the given offset. If the target area goes
outside the bounds of the Buffer, returns an error.

The `BE` or `LE` refers to whether the value will be written in big-endian
(highest byte first) or little-endian (lowest byte first) order.

### Buffer.toString

`string toString(string encoding);`

Currently, the only supported `encoding` is 'hex'. If 'hex' is given, returns
the contents of the Buffer encoded in hexadecimal digits. Otherwises, returns an
error.

Sample Apps
-----------
* [Buffer sample](../samples/Buffer.js)
* [WebBluetooth Demo](../samples/WebBluetoothDemo.js)
