// Copyright (c) 2017, Intel Corporation.

var statusNode = document.getElementById('status');
var connectBtn = document.getElementById('connect');
var updateBtn = document.getElementById('update');
var delayInput = document.getElementById('delay');

var device = null;

var packetsRead = 0;
var packetsWritten = 0;
var bytesRead = 0;
var bytesWritten = 0;

var connected = false;

function setClassDisplay(name, state) {
    var elements = document.getElementsByClassName(name);
    var len = elements.length;
    for (var i = 0; i < len; ++i) {
        elements[i].style.display = state;
    }
}

function setConnectionStatus(connect) {
    if (connect) {
        statusNode.innerHTML = "Connected";
        connectBtn.innerHTML = "Disconnect";
        setClassDisplay('con', 'block');
        setClassDisplay('con-inline', 'inline-block');
        setClassDisplay('discon', 'none');
        connected = true;
    }
    else {
        statusNode.innerHTML = 'Disconnected';
        connectBtn.innerHTML = "Connect";
        setClassDisplay('con', 'none');
        setClassDisplay('con-inline', 'none');
        setClassDisplay('discon', 'block');
        setClassDisplay('updated', 'none');
        connected = false;
    }
}

navigator.usb.addEventListener('connect', function (event) {
    autoConnect(event.device);
});

navigator.usb.addEventListener('disconnect', function (event) {
    setConnectionStatus(false);
});

async function autoConnect(dev) {
    if (dev.manufacturerName == 'Acme' || dev.manufacturerName == 'Intel') {
        try {
            statusNode.innerHTML = 'Device present. Connecting...';

            await dev.open();
            if (!dev.configuration) {
                await dev.selectConfiguration(1);
            }
            await dev.claimInterface(2);
            device = dev;
            await ctrlTransfer();

            // read configured delay value when safe
            setTimeout(async function () {
                await sendCommand('read delay');
                let maxlen = 64;

                var promise = device.transferIn(3, maxlen);
                promise.then(function (result) {
                    if (result.status == 'stall') {
                        console.warn('connection lost');
                        return;
                    }

                    setConnectionStatus(true);

                    var decoder = new TextDecoder("utf-8");
                    var s = decoder.decode(result.data);
                    delayInput.value = parseInt(s);
                });

                setTimeout(async function () {
                    if (!connected) {
                        statusNode.innerHTML = 'Device not in configuration mode';
                    }
                }, 2000);
            }, 250);
        }
        catch (e) {
            console.log("Exception:", e.message);
        }
    }
}

document.addEventListener('DOMContentLoaded', async () => {
    let devices = await navigator.usb.getDevices();
    devices.forEach(async dev => {
        autoConnect(dev);
    });
});

async function ctrlTransfer() {
	await device.controlTransferOut({
	    requestType: 'class',
	    recipient: 'interface',
	    request: 0x22,
	    value: 0x01,
	    index: 0x02
	});
}

async function sendCommand(command) {
    var encoder = new TextEncoder("utf-8");
    var buf = encoder.encode(command);
    await device.transferOut(2, buf);
}

async function updateConfiguration() {
    let command = 'set delay ' + delayInput.value;
    await sendCommand(command);
	statusNode.innerHTML = "Connected and Updated";
    setClassDisplay('updated', 'block');
}

updateBtn.addEventListener('click', updateConfiguration);

async function disconnect() {
    let devices = await navigator.usb.getDevices();
    devices.forEach(device => {
        device.close();
    });
}

document.getElementById('connect').addEventListener('click', async () => {
    if (connected) {
        setConnectionStatus(false);
        return;
    }
    try {
        // filter to show only Intel devices
	    let dev = await navigator.usb.requestDevice({
            filters: [{
                'vendorId': 0x8086
            }]
        });
        await autoConnect(dev);
    }
    catch (e) {
	    console.warn('no device selected');
    }
});
