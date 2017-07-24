ZJS API for Net
==================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
ZJS provides net (TCP) APIs which closely mimic the Node.js 'net' module. This
module allows you to create a TCP/IP server or client.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns a Net object
// var net = require('net');

interface Net {
    // create a server object
    Server createServer(callback onconnection);
    // Socket constructor, create a new Socket
    Socket Socket();
    Number isIP(input);
    Boolean isIPv4(input);
    Boolean isIPv6(input);
};

interface Socket {
    // Socket events
    onclose();
    onconnect();
    ondata(Buffer buf);
    onerror();
    ontimeout();
    // Socket methods
    void connect(Object options, callback onconnect);
    void pause();
    void resume();
    void setTimeout(Number timeout, callback ontimeout);
    void write(Buffer buf, callback writeDone);
    // Socket properties
    Number bufferSize;    // Size of read buffer
    Number bytesRead;     // Total bytes read for the socket
    Number bytesWritten;  // Total bytes written for the socket
    String localAddress;  // Sockets local IP
    Number localPort;     // Sockets local port
    String remoteAddress; // Remote IP address
    String remoteFamily;  // Remote IP family
    Number remotePort;    // Remote port
};

interface Server {
    // Server events
    onclose();
    onconnection(Socket sock);
    onerror();
    onlistening();
    // Server methods
    AddressInfo address();
    void close();
    void getConnections(callback);
    void listen(Object options, callback onlistening);
    // Server properties
    Boolean listening;      // true if the server is listening
    Number maxConnections;  // maximum number of connections
};

dictionary AddressOptions {
    Number port;          // Port the client should connect to (required)
    String host;          // Host the client should connect to
    String localAddress;  // Local address to bind to
    Number localPort;     // local port to bind to
    Number family;        // Version of IP stack, deafults to 4
}

dictionary AddressInfo {
    Number port;    // Server port
    String family;  // IPv4 or IPv6
    String address; // IP address for the server
}
```

API Documentation
-----------------

### Net.createServer
`Server createServer(callback onconnection)`

Create a TCP server that can accept client connections.

`onconnection` is an optional callback function that, if supplied, will be
registered as the the event listener for the `connection` event.

Returns a `Server` object.

### Net.Socket
`Socket Socket(void)`

Socket constructor. Calling `new Socket()` will return a new Socket object
that can be used to connect to a remote TCP server.

### Net.isIP
`Number isIP(input)`

Checks if the input is a valid IP address.

Returns 4 if the input is an IPv4 address, 6 if the input is an IPv6 address,
or 0 if the input is not an IP address.

### Net.isIPv4
`Boolean isIPv4(input)`

Checks if input is an IPv4 address.

Returns true if input is IPv4.

### Net.isIPv6
`Boolean isIPv6(input)`

Checks if input is an IPv6 address.

Returns true if input is IPv6.

### Socket.onclose Event
`void onclose(void)`

`close` event emitted when the socket has been closed, either by you or by
the remote end.

### Socket.onconnect Event
`void onconnect(void)`

`connect` event emitted when the socket has made a successful connection to a
remote TCP server.

### Socket.ondata Event
`void ondata(Buffer buf)`

`data` event emitted when the socket has received data. `buf` is a Buffer
containing the data received.

### Socket.onerror Event
`void onerror(void)`

`error` event emitted when there was an error on the socket (read/write/connect).

### Socket.ontimeout Event
`void ontimeout(void)`

`timeout` event emitted when the socket has timed out. This can only happen
if a timeout was set with `setTimeout`.

### Socket.connect
`void connect(AddressOptions options, callback onconnect)`

Connect to a remote TCP server.

`options` should describe the remote server your connecting to.

`onconnect` is optional and, if specified, will be added as the listener for the
`connect` event.

### Socket.pause
`void pause(void)`

Pause a socket from receiving data. `data` event will not be emitted until
`resume` is called.

### Socket.resume
`void resume(void)`

Allow a socket to resume receiving data after a call to `pause`.

### Socket.setTimeout
`void setTimeout(Number time, callback ontimeout)`

Set a socket timeout. This will start a timer on the socket which will expire
in `time` milliseconds if there has been no activity on the socket. The
`ontimeout` parameter, if specified, will register a listener for the
`timeout` event.

### Socket.write
`void write(Buffer buf, callback writeDone)`

Send data on the socket.

`buf` should contain the data you wish to send.

`writeDone` is optional and will be called once the data is written.

### Server.onclose Event
`void onclose(void)`

`close` event emitted when the server has closed. This only happens after a
server has called `close()` and all its connections have been closed. Calling
`close()` does not close all opened connections; that must be done manually.

### Server.onconnection Event
`void onconnection(Socket sock)`

`connection` event emitted when a client has connected to the server.

`sock` parameter is the socket for this new connection.

### Server.onerror Event
`void onerror(void)`

`error` event emitted when the server has had an error.

### Server.onlistening Event
`void onlistening(void)`

`listening` event emitted when the server has been bound, after calling
`server.listen()`.

### Server.address
`AddressInfo address(void)`

Returns an AddressInfo object for the server:

### Server.close
`void close(void)`

Signals the server to close. This will stop the server from accepting any new
connections but will keep any existing connections alive. Once all existing
connections have been closed the server will emit the `close` event.

### Server.getConnections(callback)
`void getConnections(callback)`

Get the number of connections to the server. The `callback` parameter should be
a function with `err` and `count` parameters.

### Server.listen
`void listen(Object options, callback onlistening)`

Start listening for connections. The `options` object supports the following
properties:
```
{
    port : Port to bind to
    host : IP to bind to
    backlog : Max number of concurrent connections
}
```

`onlistening` is optional and, if specified, will add a listener to the
`listening` event.

Sample Apps
-----------
* [IPv6 Server sample](../samples/TCPEchoServ6.js)
* [IPv6 Client sample](../samples/TCPClient6.js)
