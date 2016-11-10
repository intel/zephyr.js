// Copyright (c) 2016, Intel Corporation.

// Sample server that works with the linux server found in iotivity-constrained
// This sample will find the resource and retrieve its device info

var client = require('ocf').client;

console.log("Started OCF client");

client.on('devicefound', function(device) {
  console.log("Device found:");
  console.log("    uuid: " + device.uuid);
  console.log("    url: " + device.url);
  console.log("    name: " + device.name);
});

client.on('error', function(error) {
  if (error.deviceId)
    console.log("Error for device: " + error.deviceId);
});

function found(resource) {
    console.log("Resource found: path=" + resource.resourcePath + " id=" + resource.deviceId);

    client.getDeviceInfo(resource.deviceId).then(function(info) {
        console.log("Got device info for: " + info.uuid);
    }).catch(function(error) {
        console.log("Error getting device info: " + error.name);
    });
}

client.findResources({ resourceType:"oic.r.light" }, found).then(function(resource) {
    console.log("findResources() was successful, deviceId=" + resource.deviceId);
}).catch(function(error) {
    console.log("findResources() returned an error: " + error.name);
});
