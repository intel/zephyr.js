// Copyright (c) 2017, Intel Corporation.

// Test OCF client that works with the OCF server found in /test/test-ocfserver.js

console.log("Please setup /test/test-ocfserver.js first");
console.log("Test OCF client");

var ocf = require("ocf");
var assert = require("Assert.js");

var client = ocf.client;
assert(typeof client === "object" && !!client,
       "OCFObject: OCF client be defined");

function check_attribute(attribute, attributeName, type, attributeValue) {
    assert(typeof attribute === type && attribute !== null &&
           attribute === attributeValue,
           "OCFResource: client get " + attributeName +
           " as '" + attributeValue + "'");
}

var expectedDevice = {
    name: "TestOCFServer",
    coreSpecVersion: "2.0",
    dataModels: "2.5"
}

function onDevicefound(device) {
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

function onUpdate(resource) {
}

function onError(error) {
    if (error.deviceId) console.log("Device error: " + error.deviceId);
}

var FoundStringFlag = 1;
var FoundStateFlag = 0;
var FoundNumberFlag = 0;
var FoundInvalidFlag = 0;
function onfound1(resource) {
    if (FoundStringFlag) {
        client.retrieve(resource.deviceId).then(function(res) {
            assert(true, "OCFClient: retrieve resource");

            assert(typeof res.deviceId === "string" &&
                   !!res.deviceId &&
                   typeof res.resourcePath === "string" &&
                   !!res.resourcePath &&
                   typeof res.properties === "object" &&
                   !!res.properties,
                   "OCFclient: all resource information are defined");

            check_attribute(res.resourcePath, "resource.resourcePath",
                            "string", "/test/str");
            check_attribute(res.properties.str, "resource.properties.str",
                            "string", "testProperties");
        }).catch(function(error) {
            assert(false, "OCFClient: retrieve resource");
        });

        setTimeout(function() {
            client.getDeviceInfo(resource.deviceId).then(function(info) {
                assert(true, "OCFClient: get device information");
            }).catch(function(error) {
                assert(false, "OCFClient: get device information");
            });
        }, 1000);

        setTimeout(function() {
            client.getPlatformInfo(resource.deviceId).then(function(info) {
                assert(true, "OCFClient: get platform information");
            }).catch(function(error) {
                assert(false, "OCFClient: get platform information");
            });
        }, 2000);

        setTimeout(function() {
            FoundStringFlag = 0;
            FoundStateFlag = 1;
        }, 3000);
    }
}

function onfound2(resource) {
    if (FoundStateFlag) {
        client.retrieve(resource.deviceId).then(function(res) {
            check_attribute(res.resourcePath, "resource.resourcePath",
                            "string", "/test/state");
            check_attribute(res.properties.state, "resource.properties.state",
                            "boolean", true);
        }).catch(function(error) {
        });

        resource.properties.state = false;
        client.update(resource).then(function(res) {
        }).catch(function(error) {
        });

        client.retrieve(resource.deviceId).then(function(res) {
            assert(res.properties.state === false,
                   "OCFClient: update resource properties data");
        }).catch(function(error) {
            console.log("OCFClient: retrieve " + error.name);
        });

        FoundStateFlag = 0;
        FoundNumberFlag = 1;
    }
}

var testNum;
var NumberValue = 0;
function onfound3(resource) {
    if (FoundNumberFlag) {
        client.retrieve(resource.deviceId).then(function(res) {
            check_attribute(res.resourcePath, "resource.resourcePath",
                            "string", "/test/num");
            check_attribute(res.properties.num, "resource.properties.num",
                            "number", 0);
        }).catch(function(error) {
        });

        testNum = setInterval(function () {
            NumberValue = NumberValue + 1;

            client.retrieve(resource.deviceId).then(function(res) {
                if (NumberValue === 10) {
                    assert(res.properties.num === NumberValue,
                           "OCFClient: retrieve and update resource with 10 times");

                    clearInterval(testNum);
                }
            }).catch(function(error) {
            });
        }, 1000);

        FoundNumberFlag = 0;
        FoundInvalidFlag = 1;
    }
}

var FindInvalidResourceFlag = 1;
function onfound4(resource) {
    if (FoundInvalidFlag) {
        client.retrieve(resource.deviceId).then(function(res) {
            FindInvalidResourceFlag = 0;
        }).catch(function(error) {
        });

        FoundInvalidFlag = 0;
    }
}

client.on("devicefound", onDevicefound);
client.on("platformfound", onPlatformfound);
client.on("update", onUpdate);
client.on("error", onError);

ocf.start();

var ResourcesStringFlag = 0;
var ResourcesStateFlag = 0;
var ResourcesNumberFlag = 0;
client.findResources({resourceType:"core.str" },
                     onfound1).then(function(resource) {
    assert(true, "OCFClient: find OCF server resources with 'core.str'");

    ResourcesStringFlag = 1;
}).catch(function(error) {
    assert(false, "OCFClient: find OCF server resources with 'core.str'");
});

setTimeout(function() {
    client.findResources({resourceType:"core.state" },
                         onfound2).then(function(resource) {
        assert(true, "OCFClient: find OCF server resources with 'core.state'");

        ResourcesStateFlag = 1;
    }).catch(function(error) {
        assert(false, "OCFClient: find OCF server resources with 'core.state'");
    });
}, 4000);

setTimeout(function() {
    client.findResources({resourceType:"core.num" },
                         onfound3).then(function(resource) {
        assert(true, "OCFClient: find OCF server resources with 'core.num'");

        ResourcesNumberFlag = 1;
    }).catch(function(error) {
        assert(false, "OCFClient: find OCF server resources with 'core.num'");
    });
}, 5000);

setTimeout(function() {
    assert(ResourcesStringFlag && ResourcesStateFlag && ResourcesNumberFlag,
           "OCFClient: find multiple OCF server resources");
}, 17000);

setTimeout(function() {
    client.findResources({resourceType:"core.invalid" },
                         onfound4).then(function(resource) {
        FindInvalidResourceFlag = 0;
    }).catch(function(error) {
    });

    assert(FindInvalidResourceFlag, "OCFClient: find invalid OCF server resources");

    assert.result();
}, 18000);
