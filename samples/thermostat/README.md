Example code of building a thermostat using ZJS
===

This folder contains the example code for building a ZJS based thermostat using the [Grove Temperature & Humidity Sensor]*. The example code `th02.js` serves as a template that reads the sensed temperature and humidity data from the sensor, then the example code `th02-ess.js` and `th02-ocf.js` expose the sensor data through Bluetooth [Environmental Sensing Service] (ESS) and [OCF] networking protocol respectively using the [ZJS communications API].

| Filename | Description |
|----------|-------------|
| [th02.js](./th02.js)     | Read the environmental data from the sensor and display the data on the serial console |
| [th02-ess.js](./th02-ess.js) | Expose the sensed environmental data through Bluetooth ESS |
| [th02-ocf.js](./th02-ocf.js) | Expose the sensed environmental data through through OCF |

[Grove Temperature & Humidity Sensor]: http://wiki.seeed.cc/Grove-TemptureAndHumidity_Sensor-High-Accuracy_AndMini-v1.0
[Environmental Sensing Service]: https://www.bluetooth.org/docman/handlers/downloaddoc.ashx?doc_id=294797
[OCF]: https://openconnectivity.org
[ZJS communications API]: https://github.com/intel/zephyr.js/blob/master/docs/API.md#communications
