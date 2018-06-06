ZJS API for Web Sockets
=======================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [WebSocket API](#websocket-api)
  * [ws.Server(options)](#wsserveroptions)
* [WebSocketServer API](#websocketserver-api)
  * [Event: 'connection'](#event-connection)
* [WebSocket API](#websocket-api)
  * [Event: 'close'](#event-close)
  * [Event: 'error'](#event-error)
  * [Event: 'message'](#event-message)
  * [Event: 'ping'](#event-ping)
  * [Event: 'pong'](#event-pong)
* [WebSocketConnection API](#websocketconnection-api)
  * [webSocketConnection.send(data, mask)](#websocketconnectionsenddata-mask)
  * [webSocketConnection.ping(data, mask)](#websocketconnectionpingdata-mask)
  * [webSocketConnection.pong(data, mask)](#websocketconnectionpongdata-mask)
* [Sample Apps](#sample-apps)

Introduction
------------
The Web Socket API is modeled after Node.js' 'ws' module. This module only
supports the Web Socket server portion of that API.

Web IDL
-------
This IDL provides an overview of the interface; see below for
documentation of specific API functions.  We have a short document
explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).

<details>
<summary>Click to show WebIDL</summary>
<pre>// require returns a WebSocket object
// var ws = require('ws');<p><p>[ReturnFromRequire]
interface WebSocket {
    WebSocketServer Server(Object options);
};<p>interface WebSocketServer: EventEmitter;<p>[ExternalInterface=(buffer,Buffer)]
interface WebSocketConnection: EventEmitter {
    void send(Buffer data, boolean mask);
    void ping(Buffer data, boolean mask);
    void pong(Buffer data, boolean mask);
};</pre>
</details>

WebSocket API
-------------

### ws.Server(options)
* `options` *Object*
* Returns: a WebSocketServer object.

Create a Web Socket server object. Options object may contain:

WebSocketServer API
-------------------

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

WebSocket API
-------------

WebSocketServer is an [EventEmitter](./events.md) with the following events:

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


WebSocketConnection API
-----------------------

### webSocketConnection.send(data, mask)
* `data` *Buffer* The data payload to send.
* `mask` *boolean* Describes whether the data payload should be masked.

Send data to the other end of the web socket connection.

### webSocketConnection.ping(data, mask)
* `data` *Buffer* Contains the data payload to send.
* `mask` *boolean* Describes whether the data payload should be masked.

Send a ping to the other end of the web socket connection.

### webSocketConnection.pong(data, mask)
* `data` *Buffer* The data payload to send.
* `mask` *boolean* Describes whether the data payload should be masked.

Send a pong to the other end of the web socket connection.

Sample Apps
-----------
* [Web Socket Server sample](../samples/websockets/WebSocketServer.js)
* [Node Web Socket Client sample](../samples/websockets/NodeWebSocketClient.js)
