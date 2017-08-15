ZJS API for Web Sockets
=======================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
The Web Socket API is modeled after Node.js' 'ws' module. This module only
supports the Web Socket server portion of the API.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns a WebSocket object
// var ws = require('ws');

interface WebSocket {
    WebSocketServer Server(Object options);
};

interface WebSocketServer: EventEmitter;

interface WebSocketConnection: EventEmitter {
    // WebSocketConnection methods
    void send(Buffer data, Boolean mask);
    void ping(Buffer data, Boolean mask);
    void pong(Buffer data, Boolean mask);
};
```

WebSocket API Documentation
---------------------------

### WebSocket.Server
`WebSocketServer Server(Object options)`

Create a Web Socket server object. Options object may contain:

WebSocketServer API Documentation
---------------------------------

WebSocketServer is [EventEmitter](./events.md) with the following events:

### Event: 'connection'

* `WebSocketConnection` `conn`

Emitted when a client has connected to the server. The argument to any
registered listener will be a `WebSocketConnection` object which can be used to
communicate with the client.
```
{
    port : Port to bind to
    backlog : Max number of concurrent connections
    clientTracking : enable client tracking
    maxPayload : set the max payload bytes per message
    acceptHandler : handler to call to accept/deny connections
}
```
The `acceptHandler` property sets a function handler to be called when there is
a new connection. The argument will be an array of sub-protocols (Strings) that
the client is requesting to use. To accept the connection, return one of these
strings from the handler.

Returns a `WebSocketServer` object.

WebSocketConnection API Documentation
-------------------------------------

WebSocketServer is [EventEmitter](./events.md) with the following events:

### Event: 'close'

Emitted when the web socket has closed.

### Event: 'error'

* `Error` `err`

Emitted when the web socket has an error. They type of error can be found in
the `err` object argument.

### Event: 'message'

* `Buffer` `data`

Emitted when the web socket has received data. The data will be contained in
the `data` Buffer argument.

### Event: 'ping'

* `Buffer` `data`

Emitted when the socket has received a ping. The ping's payload is contained in
the `data` argument.

### Event: 'pong'

* `Buffer` `data`

Emitted when the socket has received a pong. The pong's payload is contained in
the `data` argument.

### WebSocketConnection.send

`void send(Buffer data, Boolean mask)`

Send data to the other end of the web socket connection. The `data` parameter
should contain the data payload to send. The `mask` parameter says whether the
data payload should be masked.

### WebSocketConnection.ping

`void ping(Buffer data, Boolean mask)`

Send a ping to the other end of the web socket connection. The `data` parameter
should contain the data payload to send. The `mask` parameter says whether the
data payload should be masked.

### WebSocketConnection.pong

`void pong(Buffer data, Boolean mask)`

Send a pong to the other end of the web socket connection. The `data` parameter
should contain the data payload to send. The `mask` parameter says whether the
data payload should be masked.

Sample Apps
-----------
* [Web Socket Server sample](../samples/websockets/WebSocketServer.js)
* [Node Web Socket Client sample](../samples/websockets/NodeWebSocketClient.js)
