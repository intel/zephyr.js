ZJS API for OCF
===============

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Class: OCF](#ocf-api)
  * [ocf.start()](#ocfstart)
* [OCFServer-supported Events](#ocfserver-supported-events)
* [Class: OCFServer](#ocfserver-api)
  * [server.register(init)](#serverregisterinit)
* [Class: Request](#request-api)
  * [request.respond(data)](#requestresponddata)
* [OCFClient-supported Events](#ocfclient-supported-events)
* [Class: OCFClient](#ocfclient-api)
  * [client.findResources(options, [listener])](#clientfindresourcesoptions-listener)
  * [client.retrieve(deviceId, options)](#clientretrievedeviceid-options)
  * [client.update(resource)](#clientupdateresource)
  * [client.getPlatformInfo(deviceId)](#clientgetplatforminfodeviceid)
  * [client.getDeviceInfo(deviceId)](#clientgetdeviceinfodeviceid)
* [OCFServer Samples](#ocfserver-samples)
* [OCFClient Samples](#ocfclient-samples)

Introduction
------------
ZJS provides OCF Server API's which allow communication using the OCF networking
protocol.

The OCF object is the top level object containing either OCF Server,
OCF Client, or both, as well as device and platform information.

The OCF device and platform objects can be set up after requiring 'ocf'. An
example of this can be found in [OCF Server sample](../samples/OcfServer.js).

Web IDL
-------
This IDL provides an overview of the interface; see below for
documentation of specific API functions.  We have a short document
explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).

<details>
<summary>Click to show WebIDL</summary>
<pre>
// require returns an OCFObject
// var ocf = require('ocf');
<p>[ReturnFromRequire]
interface OCFObject {
    attribute OCFServer server;         // OCF server object
    attribute OCFClient client;         // OCF client object
    attribute Platform platform;        // OCF platform info
    attribute Device device;            // OCF device info
    void start();
};<p>dictionary Platform {
    string id;
    string osVersion;
    string model;
    string manufacturerName;
    string manufacturerURL;
    string manufacturerDate;
    string platformVersion;
    string firmwareVersion;
    string supportURL;
};<p>dictionary Device {
    string uuid;
    string name;
    string dataModels;
    string coreSpecVersion;
};<p>///////////////////////////////////////////
// OCF Server
///////////////////////////////////////////<p>[ExternalInterface=(eventemitter,
EventEmitter)]
interface OCFServer: EventEmitter {
    Promise<OCFResource> register(ResourceInit init);
};<p>dictionary ResourceInit {
    string resourcePath;      // OCF resource path
    string[] resourceTypes;   // List of resource types
    string[] interfaces;      // List of interfaces for resource types
    boolean discoverable;     // Is resource discoverable
    boolean observable;       // Is resource observable
    boolean secure;           // Is resource security enabled
    boolean slow;             // Is resource a slow reader
    object properties;        // Dictionary of resource properties
};<p>dictionary Resource {
    string resourcePath;      // Path for this resource
    object properties;        // Application specific resource properties
};<p>interface Request {
    attribute OCFResource target;       // Target/destination resource
    attribute OCFResource source;       // Source/origin resource
    attribute object data;              // resource representation
    Promise<void> respond(object data);
};<p>///////////////////////////////////////////
// OCF Client
///////////////////////////////////////////<p>[ExternalInterface=(eventemitter,
EventEmitter)]
interface OCFClient: EventEmitter {
    Promise<Resource> findResources(ClientOptions options, optional FoundListener listener);
    Promise<Resource> retrieve(string deviceId, object options);
    Promise<Resource> update(Resource resource);
    Promise<Platform> getPlatformInfo(string deviceId);
    Promise<Device> getDeviceInfo(string deviceId);
};<p>dictionary ClientOptions {
    string deviceId;
    string resourceType;
    string resourcePath;
};<p>callback FoundListener = void (ClientResource resource);
dictionary ClientResource {
    string deviceId;
    string resourceType;
    string resourcePath;
};
</pre>
</details>

OCF API
-------
The properties are registered to the system (and available during discovery)
once either `OCFServer.registerResource()` or `OCFClient.findResources()`
is called.

### ocf.start()

Start the OCF stack (iotivity-constrained). This should be called after all
resources have been registered. Any calls to `registerResource` after `start`
will have no effect.

OCFServer-supported Events
--------------------------
An OCFServer is an [EventEmitter](./events.md) with the following events:

### Event: 'retrieve'

* `Request` `request`
* `boolean` `observe`

Emitted when a remote client retrieves this server's resource(s).

### Event: 'update'

* `Request` `request`

Emitted when a remote client updates this server's resource(s).

OCFServer API
--------------
### server.register(init)
* `init` *ResourceInit* Contains the resource initialization information.
* Returns: a promise which resolves to an `OCFResource`.

Register a new resource with the server.

Request API
-----------
### request.respond(data)
* `data` *object* Should contain object property data for the resource. In
the case of an `onretrieve` event, `data` will be sent back to the client as
the retrieved property data.
* Returns: a promise which resolves successfully if there was no network error
from sending out the data.

Respond to an OCF `retrieve` or `update` event.

OCFClient-supported Events
--------------------------
An OCFClient is an [EventEmitter](./events.md) with the following events:

### Event: 'devicefound'

* `Device` `device`

Emitted when a device is found during `getDeviceInfo()`.

### Event: 'platformfound'

* `Platform` `platform`

Emitted when a platform is found during `getPlatformInfo()`.

### Event: 'resourcefound'

* `Resource` `resource`

Emitted when a resource is found during `findResources()`.

### Event: 'update'

* `Resource` `update`

Emitted when a resource is updated.

OCFClient API
-------------
### client.findResources(options, [listener])
* `options` *ClientOptions* Should contain a filter of resource options. Only
resources matching these options will be found.
* `listener` *FoundListener* An optional event-listener callback. This
callback will be called if a resource is found (`onfound` event).
* Returns: a promise which resolves to a `ClientResource` containing the resource properties if a resource is found.

Find remote resources matching `options` filter.

### client.retrieve(deviceId, options)
* `deviceId` *string* The device ID of the resource you are retrieving.
This ID must match a resource which has been found with `findResources()`.
* `options` *object * Contains flag information for this GET request (e.g., `observable=true`).

Retrieve (GET) a remote resource.

### client.update(resource)
* `resource` *Resource* Should contain a `deviceId` for the resource to
update. The `properties` parameter will be sent to the resource and updated.
* Returns: a promise which resolves to a resource `Resource` containing the
updated properties.

Update remote resource properties.

### client.getPlatformInfo(deviceId)
* `deviceId` *string* The `deviceId` parameter should be the ID for a resource found with `findResources()`.
* Returns: a promise which resolves to a `Platform` containing the platform
information for the resource.

Get `Platform` information for a resource.

### client.getDeviceInfo(deviceId)
* `deviceId` *string* The ID for a resource found with `findResources()`.
* Returns: a promise which resolves to a `Device` containing the device
information for the resource.

Get `Device` information for a resource.

OCFServer Samples
--------------
* [OCF Server sample](../samples/OcfServer.js)
* [OCF Sensor Server](../samples/OcfSensorServer.js)

OCFClient Samples
--------------
* [OCF Client sample](../samples/OcfClient.js)
* [OCF Sensor Client](../samples/OcfSensorClient.js)
