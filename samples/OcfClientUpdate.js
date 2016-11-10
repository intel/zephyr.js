// Copyright (c) 2016, Intel Corporation.

// Sample client that works with the linux server found in iotivity-constrained
// This sample will find the resource, then periodically retrieve and update
// the resource

var client = require('ocf').client;

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

var lightOn = true;

// TODO: Must save away the timer handle or else GC will destroy it after a few iterations
var t1 = null;

function onfound(resource) {
    t1 = setInterval(function() {
        lightOn = lightOn ? false : true;
        resource.state = lightOn;
        client.update(resource).then(function(resource) {
            console.log("update successful");
            client.retrieve(resource.deviceId, { observable: false }).then(function(res) {
                console.log("retrieve() was successful, deviceId=" + res.deviceId);
            }).catch(function(error) {
                console.log("retrieve() returned an error: " + error.name);
            });
        }).catch(function(error) {
            console.log("Error updating name='" + error.name + "' message='" +
                    error.message + "' " + "code=" + error.errorCode);
        });
    }, 1000);
}

client.findResources({ resourceType:"oic.r.light" }, onfound).then(function(resource) {
    console.log("findResources() was successful, deviceId=" + resource.deviceId);
}).catch(function(error) {
    console.log("findResources() returned an error: " + error.name);
});
