// Copyright (c) 2016-2017, Intel Corporation.

// Sample client that works with the linux server found in iotivity-constrained
// This sample will find the resource, then periodically retrieve and update
// the resource

var ocf = require('ocf');
var client = ocf.client;

console.log("Started OCF client");

client.on('error', function(error) {
  if (error.deviceId)
    console.log("Error for device: " + error.deviceId);
});

function onupdate(resource) {
    console.log("Resource updated:");
    console.log("    deviceId: " + resource.deviceId);
    console.log("    resourcePath: " + resource.resourcePath);
    if (resource.properties != undefined) {
        console.log("Resource property 'state' is " + resource.properties.state);
    } else {
        console.log("resource.properties not found");
    }
}

client.on('update', onupdate);

function onfound(resource) {
    console.log("Resource found");
}

ocf.start();

client.findResources({ resourceType:"oic.r.light" }, onfound).then(function(resource) {
    console.log("findResources() was successful, deviceId=" + resource.deviceId);
}, function(error) {
    console.log("findResources() returned an error: " + error.name);
});
