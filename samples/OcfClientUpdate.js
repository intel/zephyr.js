// Copyright (c) 2016, Intel Corporation.

// Sample client that works with the linux server found in iotivity-constrained
// This sample will find the resource, then periodically retrieve and update
// the resource

var client = require('ocf').client;

print("Started OCF client");

client.on('error', function(error) {
  if (error.deviceId)
    print("Error for device: " + error.deviceId);
});

function onupdate(resource) {
    print("Resource updated:");
    print("    deviceId: " + resource.deviceId);
    print("    resourcePath: " + resource.resourcePath);
    if (resource.properties != undefined) {
        print("Resource property 'state' is " + resource.properties.state);
    } else {
        print("resource.properties not found");
    }
}

client.on('update', onupdate);

var light_state = true;

function found(resource) {
    setInterval(function() {
        light_state = (light_state) ? false : true;
        resource.state = light_state;
        client.update(resource).then(function(resource) {
            print("update successful");
            client.retrieve(resource.deviceId, {observable: false}).then(function(res) {
                print("retrieve() was successful, deviceId=" + res.deviceId);
            }).catch(function(error) {
                print("retrieve() returned an error: " + error.name);
            });
        }).catch(function(error) {
            print("Error updating name='" + error.name + "' message='" + error.message + "' " + "code=" + error.errorCode);
        });
    }, 1000);
}

client.findResources({resourceType:"oic.r.light"}, found).then(function(resource) {
    print("findResources() was successful, deviceId=" + resource.deviceId);
}).catch(function(error) {
    print("findResources() returned an error: " + error.name);
});

