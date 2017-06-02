// Copyright (c) 2017, Intel Corporation.

// Sample client that works with OcfSensorServer.js

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
        console.log("Resource property 'sensor' is " + resource.properties.sensor);
    } else {
        console.log("resource.properties not found");
    }
}

client.on('update', onupdate);

var lightOn = true;

// TODO: Must save away the timer handle or else GC will destroy it after a few iterations
var t1 = null;

function onfound(resource) {
    t1 = setInterval(function() {
        client.retrieve(resource.deviceId, { observable: false }).then(function(res) {
            console.log("retrieve() was successful, deviceId=" + res.deviceId);
        }, function(error) {
            console.log("retrieve() returned an error: " + error.name);
        });
    }, 1000);
}

ocf.start();

client.findResources({ resourceType:"core.sensor" }, onfound).then(function(resource) {
    console.log("findResources() was successful, deviceId=" + resource.deviceId);
}, function(error) {
    console.log("findResources() returned an error: " + error.name);
});
