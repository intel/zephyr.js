// Copyright (c) 2017, Intel Corporation.

// Test OCF client that works with the OCF server found in /test/test-ocfserver.js

console.log("Please setup /test/test-ocfserver.js first");
console.log("Test OCF client");

var client = require("ocf").client;
var assert = require("Assert.js");

assert(typeof client === "object" && !!client,
       "OCFObject: OCF client be defined");

function check_attribute(attribute, attributeName, type, attributeValue) {
    assert(typeof attribute === type && !!attribute &&
           attribute === attributeValue,
           "OCFResource: client get " + attributeName +
           " as '" + attributeValue + "'");
}

var propertiesNum1 = 0;
var propertiesNum2 = 0;
function onUpdate(resource) {
    if (resource.properties != undefined) {
        propertiesNum2 = resource.properties.num;
    } else {
        console.log("resource.properties not found");
    }

    if (updateFlage) {
        propertiesNum1 = resource.properties.num - 1;
        updateFlage =  false;
    }
}

var expectedDevice = {
    name: "TestOCFServer",
    coreSpecVersion: "2.0",
    dataModels: "2.5"
}

function onDevicefound(device) {
    if (deviceFlage) {
        assert(typeof device.uuid === "string" && !!device.uuid &&
               typeof device.name === "string" && !!device.name &&
               device.name === expectedDevice.name &&
               typeof device.dataModels === "string" &&
               !!device.dataModels &&
               device.dataModels === expectedDevice.dataModels &&
               typeof device.coreSpecVersion === "string" &&
               !!device.coreSpecVersion &&
               device.coreSpecVersion === expectedDevice.coreSpecVersion,
               "OCFObject: all device information are defined");
    }
}

var expectedPlatform = {
    manufacturerName: "TestManufacturer",
    osVersion: "16.04",
    model: "Arduino101",
    manufacturerURL: "testurl.com",
    manufacturerDate: "11-11-2011",
    platformVersion: "5.0",
    firmwareVersion: "9.0",
    supportURL: "testurl.com/support"
}

function onPlatformfound(platform) {
    if (platformFlage) {
        assert(typeof platform.id === "string" && !!platform.id &&
               typeof platform.manufacturerName === "string" &&
               !!platform.manufacturerName &&
               platform.manufacturerName === expectedPlatform.manufacturerName &&
               typeof platform.osVersion === "string" &&
               !!platform.osVersion &&
               platform.osVersion === expectedPlatform.osVersion &&
               typeof platform.model === "string" && !!platform.model &&
               platform.model === expectedPlatform.model &&
               typeof platform.manufacturerURL === "string" &&
               !!platform.manufacturerURL &&
               platform.manufacturerURL === expectedPlatform.manufacturerURL &&
               typeof platform.manufacturerDate === "string" &&
               !!platform.manufacturerDate &&
               platform.manufacturerDate === expectedPlatform.manufacturerDate &&
               typeof platform.platformVersion === "string" &&
               !!platform.platformVersion &&
               platform.platformVersion === expectedPlatform.platformVersion &&
               typeof platform.firmwareVersion === "string" &&
               !!platform.firmwareVersion &&
               platform.firmwareVersion === expectedPlatform.firmwareVersion &&
               typeof platform.supportURL === "string" &&
               !!platform.supportURL &&
               platform.supportURL === expectedPlatform.supportURL,
               "OCFObject: all platform information are defined");
    }
}

function onError(error) {
    if (error.deviceId) console.log("Device error: " + error.deviceId);
}

client.on("update", onUpdate);
client.on("devicefound", onDevicefound);
client.on("platformfound", onPlatformfound);
client.on("error", onError);

var timer = null;
var retrieveFlage = true;
var deviceFlage = true;
var platformFlage = true;
var updateFlage = true;
var timeNum = 0;
function onfound(resource) {
    timer = setInterval(function() {
        client.retrieve(resource.deviceId).then(function(resource) {
            if (retrieveFlage) {
                assert(true, "OCFClient: retrieve resource");

                assert(typeof resource.deviceId === "string" &&
                       !!resource.deviceId &&
                       typeof resource.resourcePath === "string" &&
                       !!resource.resourcePath &&
                       typeof resource.properties === "object" &&
                       !!resource.properties,
                       "OCFclient: all resource information are defined");

                check_attribute(resource.resourcePath, "resource.resourcePath",
                                "string", "/test/OCFserver");
                check_attribute(resource.properties.str, "resource.properties.str",
                                "string", "testProperties");
                check_attribute(resource.properties.state, "resource.properties.state",
                                "boolean", true);
                check_attribute(resource.properties.num, "resource.properties.num",
                                "number", propertiesNum1 + 1);

                retrieveFlage = false;
            }

            timeNum = timeNum + 1;
            if (timeNum === 10) {
                assert((propertiesNum2 - propertiesNum1) === 10,
                       "OCFClient: retrieve and update resource with 10 times");

                clearInterval(timer);
            }
        }).catch(function(error) {
            if (retrieveFlage) {
                assert(false, "OCFClient: retrieve resource");

                retrieveFlage = false;
                clearInterval(timer);
            }
        });
    }, 1000);

    setTimeout(function() {
        client.getDeviceInfo(resource.deviceId).then(function(info) {
            if (deviceFlage) {
                assert(true, "OCFClient: get device information");

                deviceFlage = false;
            }
        }).catch(function(error) {
            if (deviceFlage) {
                assert(false, "OCFClient: get device information");

                deviceFlage = false;
            }
        });
    }, 12000);

    setTimeout(function() {
        client.getPlatformInfo(resource.deviceId).then(function(info) {
            if (platformFlage) {
                assert(true, "OCFClient: get platform information");

                platformFlage = false;
            }
        }).catch(function(error) {
            if (platformFlage) {
                assert(false, "OCFClient: get platform information");

                platformFlage = false;
            }
        });
    }, 13000);

    setTimeout(function() {
        resource.properties.state = false;

        client.update(resource).then(function(resource) {
            client.retrieve(resource.deviceId).then(function(res) {
                assert(res.properties.state === false,
                       "OCFClient: update resource properties data");
            }).catch(function(error) {
                console.log("OCFClient: retrieve " + error.name);
            });
        }).catch(function(error) {
            console.log("OCFClient: update " + error.name);
        });
    }, 14000);

    setTimeout(function() {
        assert.result();
    }, 15000);
}

client.findResources({ resourceType:"core.test" }, onfound).then(function(resource) {
    assert(true, "OCFClient: find OCF server resources");
}).catch(function(error) {
    assert(false, "OCFClient: find OCF server resources");
});
