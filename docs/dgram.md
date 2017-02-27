ZJS API for UDP datagram sockets
================================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Client Requirements](#requirements)
* [Sample Apps](#sample-apps)

Introduction
------------
The `dgram` API is based on a subset of the
[corresponding module](https://nodejs.org/api/dgram.html) in Node.js.
It allows you to send and receive UDP datagrams.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns a socket factory object
// var dgram = require('dgram');

[NoInterfaceObject]
interface dgram {
    DgramSocket createSocket(string udp4_or_udp6);
};

[NoInterfaceObject]
interface DgramSocket {
    void on(string event, RecvCallback cb);
    void bind(int port, string ip_addr);
    void send(Buffer buf, unsigned long offset, unsigned long len, int port, string ip_addr, [SendCallback cb]);
    void close();
};

callback RecvCallback = void (Buffer msg, RemoteInfo rinfo);
callback SendCallback = void (Error err);  // or undefined if no error


callback EventCallback = void (various);  // callback arg depends on event

dictionary RemoteInfo {
    string ip_addr;
    string family;
    int port;
};
```

API Documentation
-----------------
### dgram.createSocket

`DgramSocket createSocket(string type);`

Create a datagram socket of given type, which must be `'udp4'` or `'udp6'`.

### DgramSocket.on

`void on(string event, RecvCallback callback);`

Registers a callback. The `event` may be one of the following:

* `'message'` - a datagram received. `callback` receives a Buffer
  containing incoming datagram data and RemoteInfo dictionary with
  information about the source address.
* `'error'` - error occurred. `callback` receives an Error object.
  (In the current version, this callback is never called, but this
  will change in future versions.)

### DgramSocket.bind

`void bind(int port, string ip_addr);`

Bind socket to a local address and port. This is required operation for
server-side sockets, i.e. sockets which wait and receive data from other
network nodes. `ip_addr` must be a string representing an IP address of
a *local* network interface, or a "wildcard" address of `'0.0.0.0'` (IPv4)
or `'::'` (IPv6), in which case a socket will be bound to all local
interfaces. This module does not support domain name resolution, so only
IP addresses are allowed. At the time of writing, local interface
addresses are hardcoded to be: `'192.0.2.1'` (IPv4) and `'2001:db8::1'`
(IPv6) (but these will become configurable in the future).

### DgramSocket.send

`void send(Buffer buf, unsigned long offset, unsigned long len, int port, string ip_addr, [SendCallback cb]);`

Send data contained in a buffer to remote network node. A subset of
data in `buf` can be sent using `offset` and `len` parameters. To send
entire buffer, using values `0` and `buf.length` respectively. See
`bind()` method description for the format of `ip_addr`. An optional
callback may be provided, which will be called with the result of the send
operation: either NetworkError object in case of error, or `undefined`
on success.

### DgramSocket.close

`void close();`

Closes socket.

Sample Apps
-----------
* [IPv4 UDP echo server](../samples/UDPEchoServ4.js)
* [IPv6 UDP echo server with `offset` param to send()](../samples/UDPEchoServ6.js)
