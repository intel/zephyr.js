ZJS API for Network Configuration
=================================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [NetConfig API](#netconfig-api)
  * [Event: 'netup'](#event-netup)
  * [Event: 'netdown'](#event-netdown)
  * [net_cfg.setStaticIP(ip)](#net_cfgsetstaticipip)
  * [net_cfg.dhcp(callback)](#net_cfgdhcpcallback)
  * [net_cfg.setBleAddress(address)](#net_cfgsetbleaddressaddress)

Introduction
------------
ZJS provides network configuration API's allowing the application to set the
IP address, start DHCP, configure the Bluetooth MAC address, and be notified
when the network interface has come up or gone down.

If you include the netconfig module, you *must* either set a static IP address
or use DHCP. If you don't include the netconfig module, you will get default
static IP addresses of 192.0.2.1 and 2001:db8::1.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.  We have a short document explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).

<details>
<summary> Click to show/hide WebIDL</summary>
<pre>
// require returns a Net object
// var net_cfg = require('netconfig');<p><p>[ExternalInterface=(eventemitter, EventEmitter)]
interface NetConfig: EventEmitter {
    boolean setStaticIP(string ip);
    void dhcp(DHCPCallback callback);
    void setBleAddress(string address);
};<p>callback DHCPCallback = void (string address, string subnet, string gateway);
</pre>
</details>

NetConfig API
-------------
NetConfig is an [EventEmitter](./events.md) with the following events:

### Event: 'netup'

Emitted when the underlying network interface comes online. This becomes
important when using a networking client on 6lowpan, because you may need to
wait for a BLE connection before issuing any socket connections.

### Event: 'netdown'

Emitted when the networking interface goes offline.

### net_cfg.setStaticIP(ip)
* `ip` *string* This should be either an IPv4 or IPv6 string.
* Returns: true if the IP was successfully set and false if there was a problem setting the IP.

Set the device to use a static IP address.

An error will be returned if there was
a misconfiguration, e.g. setting an IPv6 address when IPv6 was not built.

### net_cfg.dhcp(callback)
* `callback` *DHCPCallback*

Start DHCP to obtain an IP address.

This `callback` event listener will be called when DHCP has
finished. The callback will contain 3 arguments: `address`,
`subnet`, and `gateway`.

### net_cfg.setBleAddress(address)
* `address` *string* The MAC address string in the format `XX:XX:XX:XX:XX:XX`, where each character is in HEX format (0-9, A-F).

Sets the device's BLE MAC address. This function is only defined on
Zephyr boards with BLE capabilities (e.g. Arduino 101).

Note: This function has be called when the JS is initially run when
loaded. This means no calling from within any callback functions like
setTimeout(), setInterval() and promises.  Also, If the image was built
with the `BLE_ADDR` flag, this API has no effect. Using the `BLE_ADDR`
hard codes the supplied address into the image which cannot be changed.
