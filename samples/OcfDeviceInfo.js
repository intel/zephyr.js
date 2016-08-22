// Copyright (c) 2016, Intel Corporation.

// Sample server that works with the linux server found in iotivity-constrained
// This sample will find the resource and retrieve its device info

var client = require('ocf').client;

print("Started OCF client");

client.on('devicefound', function(device) {
  print("Device found:");
  print("    uuid: " + device.uuid);
  print("    url: " + device.url);
  print("    name: " + device.name);
});

client.on('error', function(error) {
  if (error.deviceId)
    print("Error for device: " + error.deviceId);
});

function found(resource) {
    print("Resource found: path=" + resource.resourcePath + " id=" + resource.deviceId);

    client.getDeviceInfo(resource.deviceId).then(function(info) {
        print("Got device info for: " + info.uuid);
    }).catch(function(error) {
        print("Error getting device info: " + error.name);
    });
}

client.findResources({resourceType:"oic.r.light"}, found).then(function(resource) {
    print("findResources() was successful, deviceId=" + resource.deviceId);
}).catch(function(error) {
    print("findResources() returned an error: " + error.name);
});
