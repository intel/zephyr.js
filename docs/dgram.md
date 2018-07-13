ZJS API for UDP datagram sockets
================================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Dgram API](#dgram-api)
  * [dgram.createSocket(type)](#dgramcreatesockettype)
* [DgramSocket API](#dgramsocket-api)
  * [DgramSocket.on(event, callback)](#dgramsocketonevent-callback)
  * [DgramSocket.bind(port, ip_addr)](#dgramsocketbindport-ip_addr)
  * [DgramSocket.send(buf, offset, len, port, ip_addr, [cb])](#dgramsocketsendbuf-offset-len-port-ip_addr-cb)
  * [DgramSocket.close](#dgramsocketclose)
* [Sample Apps](#sample-apps)

Introduction
------------
The `dgram` API is based on a subset of the
[corresponding module](https://nodejs.org/api/dgram.html) in Node.js.
It allows you to send and receive UDP datagrams.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.  We also have a short document explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).
<details>
<summary> Click to show/hide WebIDL</summary>
<pre>
// require returns a socket factory object
// var dgram = require('dgram');
[ReturnFromRequire]
interface Dgram {
    DgramSocket createSocket(string udp4_or_udp6);
};<p>
[ExternalInterface=(Buffer)]
interface DgramSocket {
    void on(string event, RecvCallback cb);
    void bind(long port, string ip_addr);
    void send(Buffer buf, unsigned long offset, unsigned long len, long port,
              string ip_addr, optional SendCallback cb);
    void close();
};<p>callback RecvCallback = void (Buffer msg, RemoteInfo rinfo);
callback SendCallback = void (Error err);  // or undefined if no error
callback EventCallback = void (any... args);  // callback args depend on event<p>dictionary RemoteInfo {
    string ip_addr;
    string family;
    long port;
};
</pre>
</details>

Dgram API
---------
### dgram.createSocket(type)
* `type` *string* Must be `'udp4'` or `'udp6'`.
* Returns: DgramSocket object.

Create a datagram socket of the given type.

DgramSocket API
---------------
### DgramSocket.on(event, callback)
* `event` *string*
* `callback` *RecvCallback*

Registers a callback. The `event` may be one of the following:

* `'message'` - a datagram received. `callback` receives a Buffer
  containing incoming datagram data and RemoteInfo dictionary with
  information about the source address.
* `'error'` - error occurred. `callback` receives an Error object.
  (In the current version, this callback is never called, but this
  will change in future versions.)

### DgramSocket.bind(port, ip_addr)
* `port` *long*
* `ip_addr` *string* `ip_addr` A string representing an IP address of
a *local* network interface, or a "wildcard" address of `'0.0.0.0'` (IPv4)
or `'::'` (IPv6), in which case a socket will be bound to all local
interfaces.

Bind socket to a local address and port. This is a required operation for
server-side sockets, i.e., sockets that wait and receive data from other
network nodes.  This module does not support domain name resolution, so only
IP addresses are allowed. At the time of writing, local interface
addresses are hardcoded to be: `'192.0.2.1'` (IPv4) and `'2001:db8::1'`
(IPv6), but these will become configurable in the future.

### DgramSocket.send(buf, offset, len, port, ip_addr, [cb])
* `buf` *Buffer*
* `offset` *unsigned long*
* `len` *unsigned long*
* `port` *long*
* `ip_addr` *string*
* `cb` *SendCallback* Optional.

Send data contained in a buffer to remote network node. A subset of
data in `buf` can be sent using `offset` and `len` parameters. To send
the entire buffer, use values `0` and `buf.length` respectively. See
the `bind()`-method description for the format of `ip_addr`. An optional
callback may be provided, which will be called with the result of the send
operation: either a NetworkError object in the case of error, or `undefined`
on success.

### DgramSocket.close()

Closes socket.

Sample Apps
-----------
* [IPv4 UDP echo server](../samples/UDPEchoServ4.js)
* [IPv6 UDP echo server with `offset` param to send()](../samples/UDPEchoServ6.js)
