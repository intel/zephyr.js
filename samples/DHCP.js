// Copyright (c) 2017, Intel Corporation.

var net_cfg = require('net-config');

console.log("starting dhcp");

net_cfg.dhcp(function(address, subnet, gateway) {
    console.log("address=" + address);
    console.log("subnet=" + subnet);
    console.log("gateway=" + gateway);
});
