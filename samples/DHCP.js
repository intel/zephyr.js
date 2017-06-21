// Copyright (c) 2017, Intel Corporation.

var dhcp = require('dhcp');

console.log("starting dhcp");

dhcp.start(function(address, subnet, gateway) {
    console.log("address=" + address);
    console.log("subet=" + subnet);
    console.log("gateway=" + gateway);
});
