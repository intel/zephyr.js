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

1. Install BLE adapter

  Plug in a USB BLE adapter, unless your system has it built in. We've used the
  [ASUS USB-BT400](http://a.co/0ZlqU5C) and the
  [Plugable BT4.0 micro adapter](http://a.co/diQXq2S) successfully.

2. Enable 6LoWPAN module

  Load the 6lowpan driver and enable it:

  ```
  modprobe bluetooth_6lowpan
  echo 1 > /sys/kernel/debug/bluetooth/6lowpan_enable
  ```

3. Make sure Bluetooth is on

  You can run this command to see if your Bluetooth is disabled by a hardware
  or software "radio kill switch":

  ```
  rfkill list bluetooth
  ```

  If you see that it is software blocked, you can fix it with this command:

  ```
  rfkill unblock bluetooth
  ```

4. Reset HCI

  ```
  hciconfig hci0
  hciconfig hci0 reset
  ```

5. Look for your Arduino 101 advertisement

  The following command should start listing BLE devices. Your device should
  appear as "Zephyr OCF Node" by default.

  ```
  hcitool lescan
  LE Scan ...
  F1:F9:50:21:43:4A Zephyr OCF Node
  ```

  Use Ctrl-C to stop the scan once your device is found.

  Note: If this fails with an input/output error, try running the reset command
  above again.

6. Connect Linux to your Arduino 101

  Next, echo "connect <device id> 2" to the file
  /sys/kernel/debug/bluetooth/6lowpan_control. For example:

  ```
  echo "connect F1:F9:50:21:43:4A 2" > /sys/kernel/debug/bluetooth/6lowpan_control
  ```

  Note: If you subsequently rebuild your app and reboot your device, you should
  be able to rerun just this command to reconnect. If it doesn't work, try going
  back to earlier commands. Sometimes, though, we seem to see that the Linux
  stack gets into a bad state and the only way we know to fix it is to reboot
  the Linux host.

6. Check for the Bluetooth network interface

  Check that you have a new network interface for this connection. It may take
  a few seconds for it to appear:

  ```
  ifconfig
  bt0       Link encap:UNSPEC  HWaddr 5C-F3-70-FF-FE-78-1D-72-00-00-00-00-00-00-00-00
            inet6 addr: fe80::5ef3:70ff:fe78:1d72/64 Scope:Link
            UP POINTOPOINT RUNNING MULTICAST  MTU:1280  Metric:1
            RX packets:6 errors:0 dropped:0 overruns:0 frame:0
            TX packets:28 errors:0 dropped:0 overruns:0 carrier:0
            collisions:0 txqueuelen:1
            RX bytes:167 (167.0 B)  TX bytes:1200 (1.2 KB)

  [eth0, lo, etc.]
  ```

  Note: If you fail to see this bt0 interface, try the connect command above
  again.

## Connecting an OCF client
Now that your Arduino 101 is connected, it is as if it's a regular OCF IP
device. You can now connect an OCF client. There are OCF client samples in the
[iot-rest-api-server](https://github.com/01org/iot-rest-api-server) repo.
```
git clone git@github.com:01org/iot-rest-api-server.git
npm i
node index.js
```

You can then do discovery from the browser or using one of the test scripts:

1. Browser method

  Open this URL in a browser: [http://localhost:8000/api/oic/res](http://localhost:8000/api/oic/res)

  You should see a JSON array in response, and find the /a/light resource listed
  at the end.

2. Test script method

  Use the oic-get script from the iot-rest-api-server test/ directory.

  Run resource discovery with the /res path:

  ```
  ./oic-get /res
  10.237.72.146:80 /res
  [{"di":"b492c1dd-fa51-47f1-a03c-d31495a0b99a","links":[{"href":"/oic/sec/doxm","
  rt":"oic.r.doxm","if":"oic.if.baseline"}]},{"di":"b492c1dd-fa51-47f1-a03c-d31495
  a0b99a","links":[{"href":"/oic/sec/pstat","rt":"oic.r.pstat","if":"oic.if.baseli
  ne"}]},{"di":"b492c1dd-fa51-47f1-a03c-d31495a0b99a","links":[{"href":"/oic/d","r
  t":"oic.wk.d","if":"oic.if.baseline"}]},{"di":"b492c1dd-fa51-47f1-a03c-d31495a0b
  99a","links":[{"href":"/oic/p","rt":"oic.wk.p","if":"oic.if.baseline"}]},{"di":"
  31bf6309-8ebf-4309-5fbf-630930c06309","links":[{"href":"/oic/p","rt":"oic.wk.p",
  "if":"oic.if.r"}]},{"di":"31bf6309-8ebf-4309-5fbf-630930c06309","links":[{"href"
  :"/oic/d","rt":"oic.d.zjs","if":"oic.if.r"}]},{"di":"31bf6309-8ebf-4309-5fbf-630
  930c06309","links":[{"href":"/a/light","rt":"core.light","if":"oic.if.rw"}]}]
  HTTP: 200
  ```

  Using the device UUID listed for the /a/light resource, query to retrieve
  details for this resource:

  ```
  ./oic-get /a/light?di=31bf6309-8ebf-4309-5fbf-630930c06309
  10.237.72.146:80 /a/light?di=31bf6309-8ebf-4309-5fbf-630930c06309
  {"properties":{"state":false,"power":10}}
  HTTP: 200
  ```

  Note the state is 'false', i.e. the light is off. Later, when the light is on:

  ```
  ./oic-get /a/light?di=31bf6309-8ebf-4309-5fbf-630930c06309
  10.237.72.146:80 /a/light?di=31bf6309-8ebf-4309-5fbf-630930c06309
  {"properties":{"state":true,"power":10}}
  HTTP: 200
  ```

  This is what it looks like when discovery and resource retrieval is working.
