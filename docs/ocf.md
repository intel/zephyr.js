ZJS API for OCF
==================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
ZJS provides OCF Server API's which allow communication using the OCF networking
protocol.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns an OCFObject
// var ocf = require('ocf');

dictionary ResourceInit {
    string resourcePath;      // OCF resource path
    string[] resourceTypes;   // List of resource types
    string[] interfaces;      // List of interfaces for resource types
    boolean discoverable;     // Is resource discoverable
    boolean observable;       // Is resource observable
    boolean secure;           // Is resource security enabled
    boolean slow;             // Is resource a slow reader
    object properties;        // Dictionary of resource properties
};

interface OCFResource {
    string resourcePath;      // Path for this resource
    object properties;        // Application specific resource properties
};

interface OCFRequest {
    OCFResource target;       // Target/destination resource
    OCFResource source;       // Source/origin resource
    Promise<void> respond(object data);
};

// OCFServer is an EventEmitter
interface OCFServer {
    Promise<OCFResource> register(ResourceInit init);
};

interface OCFObject {
    OCFServer server;         // OCF server object
};

// listener for onretrieve event
callback RetrieveCallback = void (OCFRequest request, boolean observe);
// listener for onupdate event
callback UpdateCallback = void (OCFRequest request);

```

API Documentation
-----------------

### setBleAddress
`void setBleAddress(string address);`

Sets the devices BLE MAC address. This function is in the global scope. It
is only defined on Zephyr boards with BLE capabilities (e.g. Arduino 101).

The `address` parameter should be a MAC address string in the format
`XX:XX:XX:XX:XX:XX` where each character is in HEX format (0-9, A-F).

### OCFServer.register
`Promise<OCFResource> register(ResourceInit init);`

Register a new resource with the server.

The `init` contains the resource initalization information.

Returns a promise which resolves to an `OCFResource`.

### OCFServer.on('retrieve', RetrieveCallback listener)

Register an `onretrieve` listener. This event will be emitted when a remote
client retrieves this servers resource(s).

### OCFServer.on('update', UpdateCallback listener)

Register an `onupdate` listener. This event will be emitted when a remote
client updates this servers resource(s).

### OCFRequest.respond
`Promise<void> respond(object data);`

Respond to an OCF `onretrieve` or `onupdate` event.

The `data` parameter should contain object property data for the resource. In
the case of an `onretrieve` event, `data` will be sent back to the client as
the retrieved property data.

Returns a promise which resolves successfully if there was no network error
sending out the data.

Sample Apps
-----------
* [OCF Server sample](../samples/OcfServer.js)
* [OCF Sensor sample](../samples/OcfSensorServer.js)
