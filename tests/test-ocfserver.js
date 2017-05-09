// Copyright (c) 2017, Intel Corporation.

console.log("Test OCF server");

var ocf = require("ocf");
var assert = require("Assert.js");

var bleAddress = "F1:E2:D3:C4:B5:A6";
ocf.setBleAddress(bleAddress);

var server = ocf.server;
assert(typeof server === "object" && !!server,
       "OCFObject: OCF server be defined");

ocf.device = {
    name: "TestOCFServer",
    coreSpecVersion: "2.0",
    dataModels: "2.5"
}

ocf.platform = {
    manufacturerName: "TestManufacturer",
    osVersion: "16.04",
    model: "Arduino101",
    manufacturerURL: "testurl.com",
    manufacturerDate: "11-11-2011",
    platformVersion: "5.0",
    firmwareVersion: "9.0",
    supportURL: "testurl.com/support"
}

var TestPropertiesStr = {
    str: "testProperties"
}

var TestPropertiesState = {
    state: true
}

var TestPropertiesNum = {
    num: 0
}

var TestPropertiesInvalid = {
    value: "invalid"
}

var ResourceInitStr = {
    resourcePath: "/test/str",
    resourceTypes: ["core.str"],
    interfaces: ["/oic/if/rw"],
    discoverable: true,
    observable: true,
    secure: true,
    slow: false,
    properties: TestPropertiesStr
}

var ResourceInitState = {
    resourcePath: "/test/state",
    resourceTypes: ["core.state"],
    interfaces: ["/oic/if/rw"],
    discoverable: true,
    observable: true,
    secure: true,
    slow: false,
    properties: TestPropertiesState
}

var ResourceInitNum = {
    resourcePath: "/test/num",
    resourceTypes: ["core.num"],
    interfaces: ["/oic/if/rw"],
    discoverable: true,
    observable: true,
    secure: true,
    slow: false,
    properties: TestPropertiesNum
}

var ResourceInitInvalid = {
    resourcePath: "/test/invalid",
    resourceTypes: ["core.invalid"],
    interfaces: ["/oic/if/rw"],
    discoverable: true,
    observable: true,
    secure: true,
    slow: false,
    properties: TestPropertiesInvalid
}

function check_attribute(attribute, attributeName, type, attributeValue) {
    assert(typeof attribute === type && attribute !== null &&
           attribute === attributeValue,
           "OCFResource: server register " + attributeName +
           " as '" + attributeValue + "'");
}

var retrieveFlage = true;
var RetrieveStrFlage = true;
var RetrieveStateFlage = true;
var RetrieveNumFlage = true;
var RetrieveInvalidFlage = true;
function onRetrieve(request, observe) {
    if (retrieveFlage) {
        assert(typeof request === "object" && !!request &&
               typeof observe === "boolean" && observe !== null,
               "OCFServer: callback for event 'retrieve'");

        assert(typeof request.target.resourcePath === "string" &&
               request.target.resourcePath !== undefined,
               "OCFServer: request target/destination resource");

        retrieveFlage = false;
    }

    console.log("on('retrieve'): request.target.resourcePath = " +
                request.target.resourcePath + " observe = " + observe);

    if (request.target.resourcePath === "/test/str" &&
        request.target.resourceTypes === "core.str") {
        console.log("retrieve properties.str = " + TestPropertiesStr.str);

        request.respond(TestPropertiesStr).then(function() {
            if (RetrieveStrFlage) {
                assert(true, "OCFServer: respond for event " +
                       "'retrieve' with 'string'");

                RetrieveStrFlage = false;
            }
        }).catch(function(error) {
            if (RetrieveStrFlage) {
                assert(false, "OCFServer: respond for event " +
                       "'retrieve' with 'string'");

                RetrieveStrFlage = false;
            }
        });
    } else if (request.target.resourcePath === "/test/state" &&
               request.target.resourceTypes === "core.state") {
        console.log("retrieve properties.state = " +
                    TestPropertiesState.state);

        request.respond(TestPropertiesState).then(function() {
            if (RetrieveStateFlage) {
                assert(true, "OCFServer: respond for event " +
                       "'retrieve' with 'boolean'");

                RetrieveStateFlage = false;
            }
        }).catch(function(error) {
            if (RetrieveStateFlage) {
                assert(false, "OCFServer: respond for event " +
                       "'retrieve' with 'boolean'");

                RetrieveStateFlage = false;
            }
        });
    } else if (request.target.resourcePath === "/test/num" &&
               request.target.resourceTypes === "core.num") {
        console.log("retrieve properties.num = " + TestPropertiesNum.num);

        request.respond(TestPropertiesNum).then(function() {
            if (RetrieveNumFlage) {
                assert(true, "OCFServer: respond for event " +
                       "'retrieve' with 'number'");

                RetrieveNumFlage = false;
            }
        }).catch(function(error) {
            if (RetrieveNumFlage) {
                assert(false, "OCFServer: respond for event " +
                       "'retrieve' with 'number'");

                RetrieveNumFlage = false;
            }
        });

        TestPropertiesNum.num = TestPropertiesNum.num + 1;
    } else if (request.target.resourcePath === "/test/invalid" &&
               request.target.resourceTypes === "core.invalid") {
        console.log("retrieve properties.value = " +
                    TestPropertiesInvalid.value);

        request.respond(TestPropertiesInvalid).then(function() {
            if (RetrieveInvalidFlage) {
                assert(true, "OCFServer: respond for event " +
                       "'retrieve' with 'invalid'");

                RetrieveInvalidFlage = false;
            }
        }).catch(function(error) {
            if (RetrieveInvalidFlage) {
                assert(false, "OCFServer: respond for event " +
                       "'retrieve' with 'invalid'");

                RetrieveInvalidFlage = false;
            }
        });
    } else {
        console.log("Resource requested does not exist");
    }

}

var updateFlage = true;
function onUpdate(request) {
    if (updateFlage) {
        assert(typeof request === "object" && !!request,
               "OCFServer: callback for event 'update'");

        assert(typeof request.resource.properties === "object" &&
               request.resource.properties !== undefined,
               "OCFServer: request source/origin resource");
    }

    console.log("on('update'): request.target.resourcePath = " +
                request.target.resourcePath);

    if (request.resource.properties) {
        if (TestPropertiesState.state !== undefined &&
            request.resource.properties.state !== undefined) {
            TestPropertiesState.state = request.resource.properties.state;
            respondData = TestPropertiesState;

            console.log("update properties.state = " + TestPropertiesState.state);
        }

        if (TestPropertiesNum.num !== undefined &&
            request.resource.properties.num !== undefined) {
            TestPropertiesNum.num = request.resource.properties.num;

            respondData = TestPropertiesNum;

            console.log("update properties.num = " + TestPropertiesNum.num);
        }
    }

    request.respond(respondData).then(function() {
        if (updateFlage) {
            assert(true, "OCFServer: respond for event 'update'");

            updateFlage = false;
        }
    }).catch(function(error) {
        if (updateFlage) {
            assert(false, "OCFServer: respond for event 'update'");

            updateFlage = false;
        }
    });
}

server.on("retrieve", onRetrieve);
server.on("update", onUpdate);

server.register(ResourceInitStr).then(function(resource) {
    check_attribute(resource.resourcePath, "resource.resourcePath",
                    "string", "/test/str");
    check_attribute(resource.properties.str, "resource.properties.str",
                    "string", "testProperties");

    assert(true, "OCFServer: register OCF server with 'string' property");
}).catch(function(error) {
    assert(false, "OCFServer: register OCF server with 'string' property");
});

server.register(ResourceInitState).then(function(resource) {
    check_attribute(resource.resourcePath, "resource.resourcePath",
                    "string", "/test/state");
    check_attribute(resource.properties.state, "resource.properties.state",
                    "boolean", true);

    assert(true, "OCFServer: register OCF server with 'boolean' property");
}).catch(function(error) {
    assert(false, "OCFServer: register OCF server with 'boolean' property");
});

server.register(ResourceInitNum).then(function(resource) {
    check_attribute(resource.resourcePath, "resource.resourcePath",
                    "string", "/test/num");
    check_attribute(resource.properties.num, "resource.properties.num",
                    "number", 0);

    assert(true, "OCFServer: register OCF server with 'number' property");
}).catch(function(error) {
    assert(false, "OCFServer: register OCF server with 'number' property");
});

ocf.start();

server.register(ResourceInitInvalid).then(function(resource) {
    console.log("OCFServer: register OCF server after starting");
}).catch(function(error) {
});
