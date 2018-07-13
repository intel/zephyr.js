ZJS API for Net
===============

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Class: Net](#net-api)
  * [net.createServer([onconnection])](#netcreateserveronconnection)
  * [net.Socket()](#netsocket)
  * [net.isIP(input)](#netisipinput)
  * [Net.isIPv4(input)](#netisipv4input)
  * [Net.isIPv6(input)](#netisipv6input)
* [Class: Socket](#socket-api)
  * [Event: 'close'](#event-close)
  * [Event: 'connect'](#event-connect)
  * [Event: 'data'](#event-data)
  * [Event: 'error'](#event-error)
  * [Event: 'timeout'](#event-timeout)
  * [Socket.connect(options, [onconnect])](#socketconnectoptions-onconnect)
  * [Socket.pause()](#socketpause)
  * [Socket.resume()](#socketresume)
  * [Socket.setTimeout(time, ontimeout)](#socketsettimeouttime-ontimeout)
  * [Socket.write(buf, [writeDone])](#socketwritebuf-writedone)
* [Class: Server](#server-api)
  * [Event: 'close'](#event-close)
  * [Event: 'connection'](#event-connection)
  * [Event: 'error'](#event-error)
  * [Event: 'listening'](#event-listening)
  * [Server.address](#serveraddress)
  * [Server.close()](#serverclose)
  * [Server.getConnections(onconnection)](#servergetconnectionsonconnection)
  * [Server.listen(options, [onlistening])](#serverlistenoptions-onlistening)
* [Sample Apps](#sample-apps)

Introduction
------------
ZJS provides net (TCP) APIs that closely mimic the Node.js 'net'
module, which allows you to create a TCP/IP server or client.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.  We also have a short document explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).
<details>
<summary> Click to show/hide WebIDL</summary>
<pre>
// require returns a Net object
// var net = require('net');
[ReturnFromRequire,ExternalCallback=(ListenerCallback)]
interface Net {
    Server createServer(optional ListenerCallback onconnection);
    Socket Socket();
    long isIP(string input);
    boolean isIPv4(string input);
    boolean isIPv6(string input);
};<p>
[ExternalInterface=(EventEmitter),ExternalInterface=(Buffer),ExternalCallback=(ListenerCallback)]
interface Socket: EventEmitter {
    // Socket methods
    void connect(object options, optional ListenerCallback onconnect);
    void pause();
    void resume();
    void setTimeout(long timeout, ListenerCallback ontimeout);
    void write(Buffer buf, optional ListenerCallback writeDone);
    // Socket properties
    attribute long bufferSize;    // Size of read buffer
    attribute long bytesRead;     // Total bytes read for the socket
    attribute long bytesWritten;  // Total bytes written for the socket
    attribute string localAddress;  // Sockets local IP
    attribute long localPort;     // Sockets local port
    attribute string remoteAddress; // Remote IP address
    attribute string remoteFamily;  // Remote IP family
    attribute long remotePort;    // Remote port
};<p>[ExternalInterface=(EventEmitter),ExternalCallback=(ListenerCallback)]
interface Server: EventEmitter {
    // Server methods
    AddressInfo address();
    void close();
    void getConnections(ListenerCallback onconnection);
    void listen(object options, optional ListenerCallback onlistening);
    // Server properties
    attribute boolean listening;      // true if the server is listening
    attribute long maxConnections;  // maximum number of connections
};<p>dictionary AddressOptions {
    long port;          // Port the client should connect to (required)
    string host;          // Host the client should connect to
    string localAddress;  // Local address to bind to
    long localPort;     // local port to bind to
    long family;        // Version of IP stack, deafults to 4
};<p>dictionary AddressInfo {
    long port;    // Server port
    string family;  // IPv4 or IPv6
    string address; // IP address for the server
};
</pre>
</details>

Net API
-------

### net.createServer([onconnection])
* `onconnection` *callback* The (optional) callback function registered as the the event listener for the `connection` event.
* Returns: a `Server` object.

Create a TCP server that can accept client connections.

### net.Socket()
* Returns: a new Socket object that can be used to connect to a remote TCP server.

Socket constructor.

### net.isIP(input)
* `input` *string*
* Returns: 4 if the input is an IPv4 address, 6 if the input is an IPv6 address,
or 0 if the input is not an IP address.

Checks if the input is a valid IP address.

### Net.isIPv4(input)
* `input` *string*
* Returns: true if input is IPv4, false otherwise.

Checks if input is an IPv4 address.

### Net.isIPv6(input)
* `input` *string*
* Returns: true if input is IPv6, false otherwise.

Checks if input is an IPv6 address.

Socket API
----------

Socket is an [EventEmitter](./events.md) with the following events:

### Event: 'close'

Emitted when the socket is closed, either by you or the remote end.

### Event: 'connect'

Emitted when the socket has made a successful connection to a remote TCP server.

### Event: 'data'

* `Buffer` `buf`

Emitted when the socket has received data. `buf` is a Buffer containg the data
received.

### Event: 'error'

Emitted when there was an error on the socket during read, write, or connect.

### Event: 'timeout'

Emitted only when a timeout set with `setTimeout` expires.

### Socket.connect(options, [onconnect])
* `options` *AddressOptions* Describes the remote server being connected to.
* `onconnect` *ListenerCallback* Optional callback added as the listener for the
`connect` event.

Connect to a remote TCP server.

### Socket.pause()

Pause a socket from receiving data. `data` event will not be emitted until
`Socket.resume` is called.

### Socket.resume()

Allow a socket to resume receiving data after a call to `Socket.pause`.

### Socket.setTimeout(time, ontimeout)
* `time` *long*
* `ontimeout` *ListenerCallback* Optional callback registered as a listener for the `timeout` event.

Set a socket timeout. This will start a timer on the socket that will expire
in `time` milliseconds if there has been no activity on the socket.

### Socket.write(buf, [writeDone])
* `buf` *Buffer* `buf` Contains the data to be written.
* `writeDone` *ListenerCallback* Optional function called once the data is written.

Send data on the socket.

Server API
----------

Server is an [EventEmitter](./events.md) with the following events:

### Event: 'close'

Emitted when the server has closed. This will only happen after a server has
called `close()` and all its connections have been closed. Calling `close()`
does not close all opened connections; that must be done manually.

### Event: 'connection'

* `Socket` `sock`

Emitted when when a client has connected to the server. `sock` is the Socket
representing the new connection.

### Event: 'error'

Emitted when the server has had an error.

### Event: 'listening'

Emitted when the server has been bound, after calling `server.listen()`.

### Server.address
`AddressInfo address(void)`

Returns an AddressInfo object for the server:

### Server.close()

Signals the server to close. This will stop the server from accepting any new
connections but will keep any existing connections alive. Once all existing
connections have been closed the server will emit the `close` event.

### Server.getConnections(onconnection)
* `onconnection` *ListenerCallback* Should be a function with `err` and `count` parameters.

Get the number of connections to the server.

### Server.listen(options, [onlistening])
* `options` *object*
* `onlistening` *ListenerCallback* Optional function added to the `listening` event.

Start listening for connections. The `options` object supports the following
properties:
```
{
    port : Port to bind to
    host : IP to bind to
    backlog : Max number of concurrent connections
}
```

Sample Apps
-----------
* [IPv6 Server sample](../samples/TCPEchoServ6.js)
* [IPv6 Client sample](../samples/TCPClient6.js)
