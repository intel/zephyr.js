# OCF over BLE
This document describes the steps to build and run an OCF server over BLE on
the Arduino 101. This setup requires a Linux desktop (or similar) with a BLE
adapter and 6LoWPAN support. The 6LoWPAN module is standard on Ubuntu and
should work with a 4.0 compliant BT/BLE adapter.

## Prerequisites
* Arduino 101 Board
* BLE adapter (a BT 4.0 compliant adapter)
* Linux Desktop
* Latest ZJS branch

## Initial Setup
Please follow the ZJS initial setup in the main [README](../README.md)

## Building for A101
Building the OCF server sample is the same as any other:
```
make BOARD=arduino_101 JS=samples/OcfServer.js
```

## Flashing the A101
Flash the Arduino 101 as you would any other script:
```
make dfu
```

## Linux Setup
These steps are for making the initial BLE connection to the Arduino 101. The
Arduino 101 must have been flashed with the OCF server sample and reset so it
is advertising over BLE. The following steps will create that connection which
will show up as a IP interface on the Linux box:
(Note: You will likely have to run these commands as root)

1. Enable 6LoWPAN module

  ```
  modprobe bluetooth_6lowpan
  echo 1 > /sys/kernel/debug/bluetooth/6lowpan_enable
  ```

2. Reset HCI

  ```
  hciconfig hci0
  hciconfig hci0 reset
  ```

3. Look for your Arduino 101 advertisment

  ```
  hcitool lescan
  # This should start listing BLE devices
  # Your device should appear as "Zephyr OCF node"
  # e.g.
  LE Scan ...
  F1:F9:50:21:43:4A Zephyr OCF node
  # Use Ctrl-C to stop the scan once your device is found
  ```

4. Connect Linux to your Arduino 101

  ```
  echo "connect <device id> 2" > /sys/kernel/debug/bluetooth/6lowpan_control
  # e.g.
  echo "connect F1:F9:50:21:43:4A 2" > /sys/kernel/debug/bluetooth/6lowpan_control
  ```

5. Check that you have a new network interface for this connection

  ```
  ifconfig
  # You should see a bt0 interface
  # e.g.
  bt0       Link encap:UNSPEC  HWaddr 5C-F3-70-FF-FE-78-1D-72-00-00-00-00-00-00-00-00
            inet6 addr: fe80::5ef3:70ff:fe78:1d72/64 Scope:Link
            UP POINTOPOINT RUNNING MULTICAST  MTU:1280  Metric:1
            RX packets:6 errors:0 dropped:0 overruns:0 frame:0
            TX packets:28 errors:0 dropped:0 overruns:0 carrier:0
            collisions:0 txqueuelen:1 
            RX bytes:167 (167.0 B)  TX bytes:1200 (1.2 KB)

  ```

## Connecting an OCF client
Now that your Arduino 101 is connected, it is as if its a regular OCF IP device. You
can now connect an OCF client. There are example OCF client samples in the
[iot-rest-api-server](https://github.com/01org/iot-rest-api-server) repo.
```
git clone git@github.com:01org/iot-rest-api-server.git
npm i
node index.js
```

You can then do discovery a) from the browser or b) using one of the test scripts

a) http://localhost:8000/api/oic/res

You should see the /a/light resource there

b) Using oic-get script under test/

```
$ ./oic-get /res
10.237.72.146:80 /res
[{"di":"b492c1dd-fa51-47f1-a03c-d31495a0b99a","links":[{"href":"/oic/sec/doxm","rt":"oic.r.doxm","if":"oic.if.baseline"}]},{"di":"b492c1dd-fa51-47f1-a03c-d31495a0b99a","links":[{"href":"/oic/sec/pstat","rt":"oic.r.pstat","if":"oic.if.baseline"}]},{"di":"b492c1dd-fa51-47f1-a03c-d31495a0b99a","links":[{"href":"/oic/d","rt":"oic.wk.d","if":"oic.if.baseline"}]},{"di":"b492c1dd-fa51-47f1-a03c-d31495a0b99a","links":[{"href":"/oic/p","rt":"oic.wk.p","if":"oic.if.baseline"}]},{"di":"31bf6309-8ebf-4309-5fbf-630930c06309","links":[{"href":"/oic/p","rt":"oic.wk.p","if":"oic.if.r"}]},{"di":"31bf6309-8ebf-4309-5fbf-630930c06309","links":[{"href":"/oic/d","rt":"oic.d.zjs","if":"oic.if.r"}]},{"di":"31bf6309-8ebf-4309-5fbf-630930c06309","links":[{"href":"/a/light","rt":"core.light","if":"oic.if.rw"}]}]
HTTP: 200
$ ./oic-get /a/light?di=31bf6309-8ebf-4309-5fbf-630930c06309
10.237.72.146:80 /a/light?di=31bf6309-8ebf-4309-5fbf-630930c06309
{"properties":{"state":false,"power":10}}
HTTP: 200
$ ./oic-get /a/light?di=31bf6309-8ebf-4309-5fbf-630930c06309
10.237.72.146:80 /a/light?di=31bf6309-8ebf-4309-5fbf-630930c06309
{"properties":{"state":true,"power":10}}
HTTP: 200
$ ./oic-get /a/light?di=31bf6309-8ebf-4309-5fbf-630930c06309
10.237.72.146:80 /a/light?di=31bf6309-8ebf-4309-5fbf-630930c06309
{"properties":{"state":false,"power":10}}
HTTP: 200
$ ./oic-get /a/light?di=31bf6309-8ebf-4309-5fbf-630930c06309
10.237.72.146:80 /a/light?di=31bf6309-8ebf-4309-5fbf-630930c06309
{"properties":{"state":true,"power":10}}
HTTP: 200
 
As you can see, the discovery and retrieve resources works.
```
