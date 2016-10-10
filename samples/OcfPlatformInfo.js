// Copyright (c) 2016, Intel Corporation.

// Sample server that works with the linux server found in iotivity-constrained
// This sample will find the resource and retrieve its platform info

var client = require('ocf').client;

print("Started OCF client");

client.on('platformfound', function(platform) {
    print("Platform found:");
    print("    id: " + platform.id);
    print("    Manufacturer Name: " + platform.manufacturerName);
});

client.on('error', function(error) {
  if (error.deviceId)
    print("Error for device: " + error.deviceId);
});

function found(resource) {
    print("Resource found: path=" + resource.resourcePath + " id=" + resource.deviceId);

    client.getPlatformInfo(resource.deviceId).then(function(info) {
        print("Got platform info for: " + info.id);
    }).catch(function(error) {
        print("Error getting platform info: " + error.name);
    });
}

client.findResources({resourceType:"oic.r.light"}, found).then(function(resource) {
    print("findResources() was successful, deviceId=" + resource.deviceId);
}).catch(function(error) {
    print("findResources() returned an error: " + error.name);
});