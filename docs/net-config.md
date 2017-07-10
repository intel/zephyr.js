ZJS API for Network Configuration
==================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
ZJS provides network configuration API's allowing the application to set the
IP address, start DHCP, configure the Bluetooth MAC address, and be notified
when the network interface has come up or gone down.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns a Net object
// var net_cfg = require('net-config');

interface NetConfig {
    // net-config events
    EventListener onup(void);
    EventListener ondown(void);
    // set a static IP
    Boolean setStaticIP(String ip);
    // start DHCP
    void dhcp(DHCPCallback callback);
    // set the BLE MAC address
    void setBleAddress(String address);
};

callback DHCPCallback = void (String address, String subnet, String gateway);
```

API Documentation
-----------------
### NetConfig.onup Event
`void onup(void);`

This event is emitted when the underlying network interface comes online. This
becomes important when using a networking client on 6lowpan, because you may
need to wait for a BLE connection before issuing any socket connections.

### NetConfig.onup Event
`void ondown(void);`

The couterpart to `ondown`. This is issued when the networking interface goes
offline.

### NetConfig.setStaticIP
`Boolean setStaticIP(String ip)`

Set the device to use a static IP address.

`ip` should be either an IPv4 or IPv6 string.

This returns a true if the IP was successfully set. It will return false if
there was a problem setting that IP. An error will be returned if there was
a misconfiguration, e.g. setting an IPv6 address when IPv6 was not built.

### NetConfig.dhcp
`void dhcp(DHCPCallback callback)`

Start DHCP to obtain an IP address.

`callback` should be a `DHCPCallback` type. This event listener will be called
when DHCP has finished. The callback will contain 3 arguments: `address`,
`subnet`, and `gateway`.

### NetConfig.setBleAddress
`void setBleAddress(string address);`

Sets the device's BLE MAC address. This function is only defined on
Zephyr boards with BLE capabilities (e.g. Arduino 101).

The `address` parameter should be a MAC address string in the format
`XX:XX:XX:XX:XX:XX` where each character is in HEX format (0-9, A-F).

Note: If the image was built with the `BLE_ADDR` flag, this API has
no effect. Using the `BLE_ADDR` hard codes the supplied address into
the image which cannot be changed.
