ZJS API for Bluetooth Low Energy (BLE)
======================================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Client Requirements](#client-requirements)
* [Sample Apps](#sample-apps)

Introduction
------------
The BLE API is based off the [bleno API](https://github.com/sandeepmistry/bleno). Bluetooth Low Energy (aka
[Bluetooth LE: Broadcast](https://www.bluetooth.com/what-is-bluetooth-technology/how-it-works/le-broadcast)) is a power-friendly version of Bluetooth
intended for IoT devices. It provides the ability to support low-bandwidth data
services to nearby devices.

A note about UUIDs. The BLE standard lets you have full 128-bit UUIDs or short
16-bit UUIDs (4 hex chars). We only support the short ones so far.

Based on the bleno API, these UUIDs are specified as a hexadecimal string, so
"2901" really means 0x2901. *NOTE: This seems like a bad practice and we should
perhaps require these numbers to be specified as "0x2901" instead, or else
treat them like decimals.*

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns a BLE object
// var ble = require('ble');

[NoInterfaceObject]
interface BLE: EventEmitter {
    void disconnect(string address);
    void startAdvertising(string name, string[] uuids, string url);
    void stopAdvertising();
    void setServices(PrimaryService services[]);
    PrimaryService PrimaryService(PrimaryServiceInit init);
    Characteristic Characteristic(CharacteristicInit init);
    Descriptor Descriptor(DescriptorInit init);
};

dictionary PrimaryServiceInit {
    string uuid;
    Characteristic[] characteristics;
};

dictionary CharacteristicInit {
    string uuid;
    string[] properties;                // 'read', 'write', 'notify'
    Descriptor[] descriptors;
    ReadCallback onReadRequest;         // optional
    WriteCallback onWriteRequest;       // optional
    SubscribeCallback onSubscribe;      // optional
    UnsubscribeCallback onUnsubscribe;  // optional
    NotifyCallback onNotify;            // optional
};

callback ReadCallback = void (unsigned long offset, FulfillReadCallback);
callback WriteCallback = void (Buffer data, unsigned long offset,
                               boolean withoutResponse, FulfillWriteCallback);
callback SubscribeCallback = void (unsigned long maxValueSize,
                                   FulfillSubscribeCallback);
callback FulfillReadCallback = void (CharacteristicResult result, Buffer data);
callback FulfillWriteCallback = void (CharacteristicResult result);
callback FulfillSubscribeCallback = void (Buffer data);

dictionary DescriptorInit {
    string uuid;
    string value;
};

[NoInterfaceObject]
interface Characteristic {
    attribute ReadCallback onReadRequest;
    attribute WriteCallback onWriteRequest;
    attribute SubscribeCallback onSubscribe;
    attribute UnsubscribeCallback onUnsubscribe;
    attribute NotifyCallback onNotify;
    unsigned long RESULT_SUCCESS;
    unsigned long RESULT_INVALID_OFFSET;
    unsigned long RESULT_INVALID_ATTRIBUTE_LENGTH;
    unsigned long RESULT_UNLIKELY_ERROR;
};
```

API Documentation
-----------------
BLE is an [EventEmitter](./events.md) with the following events:

### Event: 'accept'

* `string` `clientAddress`

Emitted when a BLE client has connected. `clientAddress` is a unique BLE address
for the client in colon-separated format (e.g. 01:23:45:67:89:AB).

### Event: 'advertisingStart'

* `int` `status`

Emitted when BLE services have begun to be advertised. The `status` will be 0
for success, otherwise for an error.

### Event: 'disconnect'

* `string` `clientAddress`

Emitted when a BLE client has disconnected. `clientAddress` will be the same as
one previously sent with the 'accept' event.

### Event: 'stateChange'

* `string` `newState`

Emitted with 'poweredOn' when the BLE stack is ready to be used. No other states
are supported at this time.

### BLE.disconnect

`void disconnect(string address);`

Disconnect the remote client.

The `address` is the address of the connected client.

### BLE.startAdvertising

`void startAdvertising(string name, string[] uuids, string url);`

The `name` is limited to 26 characters and will be advertised as the device
name to nearby BLE devices.

The `uuids` array may contain at most 7 16-bit UUIDs (four hex digits each).
These UUIDs identify available services to nearby BLE devices.

The `url` is optional and limited to around 24 characters (slightly more
if part of the URL is able to be [encoded](https://github.com/google/eddystone/tree/master/eddystone-url). If provided,
this will be used to create a physical web advertisement that will direct users
to the given URL. At that URL they might be able to interact with the
advertising device somehow.

### BLE.stopAdvertising

`void stopAdvertising();`

Currently does nothing.

### BLE.setServices

`void setServices(PrimaryService[]);`

Pass an array of PrimaryService objects to set up the services that are
implemented by your app.

### BLE.PrimaryService constructor

`PrimaryService(PrimaryServiceInit init);`

The `init` object should contain a `uuid` field with a 16-bit service UUID (4
hex chars) and a `characteristics` field with an array of Characteristic
objects.

### BLE.Characteristic constructor

`Characteristic(CharacteristicInit init);`

The `init` object should contain:
* `uuid` field with a 16-bit characteristic UUID (4 hex chars)
* `properties` field with an array of strings that may include 'read', 'write',
  and 'notify', depending on what is supported
* `descriptors` field with an array of Descriptor objects

It may also contain these optional callback fields:
* `onReadRequest` function(offset, callback(result, data))
  * Called when the client is requesting to read data from the characteristic.
  * See below for common argument definitions
* `onWriteRequest` function(data, offset, withoutResponse, callback(result))
  * Called when the client is requesting to write data to the characteristic.
  * `withoutResponse` is true if the client doesn't want a response
    * *TODO: verify this*
* `onSubscribe` function(maxValueSize, callback(data))
  * Called when a client signs up to receive notify events when the
      characteristic changes.
  * `maxValueSize` is the maximum data size the client wants to receive.
* `onUnsubscribe` function()
  * *NOTE: Never actually called currently.*
* `onNotify` function()
  * *NOTE: Never actually called currently.*

Explanation of common arguments to the above functions:
* `offset` is a 0-based integer index into the data the characteristic
    represents.
* `result` is one of these values defined in the Characteristic object.
  * RESULT_SUCCESS
  * RESULT_INVALID_OFFSET
  * RESULT_INVALID_ATTRIBUTE_LENGTH
  * RESULT_UNLIKELY_ERROR
* `data` is a [Buffer](./buffer.md) object.

### BLE.Descriptor constructor

`Descriptor(DescriptorInit init);`

The `init` object should contain:
* `uuid` field with a 16-bit descriptor UUID (4 hex chars)
  * Defined descriptors are listed here in [Bluetooth Specifications](https://www.bluetooth.com/specifications/gatt/descriptors)
* `value` field with a string supplying the defined information
  * *NOTE: Values can also be Buffer objects, but that's not currently supported.*

Client Requirements
-------------------
You can use any device that has BLE support to connect to the Arduino 101 when
running any of the BLE apps or demos. We've successfully tested the following
setup:

* Update the Bluetooth firmware, follow instructions [here](https://wiki.zephyrproject.org/view/Arduino_101#Bluetooth_firmware_for_the_Arduino_101)
* Any Android device running Android 6.0 Marshmallow or higher (We use Nexus 5/5X, iOS devices not tested)
* Let the x86 core use 256K of flash space with ROM=256 as described [here](https://github.com/intel/zephyr.js#getting-more-space-on-your-arduino-101)

For the WebBluetooth Demo which supports the Physical Web, you'll need:
* Chromium version 50.0 or higher
* Make sure Bluetooth is on and Location Services is enabled
* Go to chrome://flags and enable the #enable-web-bluetooth flag

Sample Apps
-----------
* [BLE with multiple services](../samples/BLE.js)
* [WebBluetooth Demo](../samples/WebBluetoothDemo.js)
* [WebBluetooth Demo with Grove LCD](../samples/WebBluetoothGroveLcdDemo.js)
* [Heartrate Demo with Grove LCD](../samples/HeartRateDemo.js)
