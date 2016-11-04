// Copyright (c) 2016, Intel Corporation.

// Sample server that works with the linux server found in iotivity-constrained
// This sample will find the resource and retrieve its platform info

var client = require('ocf').client;

console.log("Started OCF client");

client.on('platformfound', function(platform) {
    console.log("Platform found:");
    console.log("    id: " + platform.id);
    console.log("    Manufacturer Name: " + platform.manufacturerName);
});

client.on('error', function(error) {
  if (error.deviceId)
    console.log("Error for device: " + error.deviceId);
});

function onfound(resource) {
    console.log("Resource found: path=" + resource.resourcePath + " id=" + resource.deviceId);

    client.getPlatformInfo(resource.deviceId).then(function(info) {
        console.log("Got platform info for: " + info.id);
    }).catch(function(error) {
        console.log("Error getting platform info: " + error.name);
    });
}

client.findResources({ resourceType:"oic.r.light" }, onfound).then(function(resource) {
    console.log("findResources() was successful, deviceId=" + resource.deviceId);
}).catch(function(error) {
    console.log("findResources() returned an error: " + error.name);
});