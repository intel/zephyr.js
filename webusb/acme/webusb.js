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
    console.log('setting', name, state);
    var elements = document.getElementsByClassName(name);
    var len = elements.length;
    for (var i = 0; i < len; ++i) {
        elements[i].style.display = state;
    }
}

async function autoConnect(dev) {
    if (dev.manufacturerName == 'Acme') {
        try {
            await dev.open();
            if (!dev.configuration) {
                await dev.selectConfiguration(1);
            }
            await dev.claimInterface(2);
            device = dev;
            await ctrlTransfer();
	        statusNode.innerHTML = "Connected";
            connectBtn.innerHTML = "Disconnect";

            setClassDisplay('con', 'block');
            setClassDisplay('con-inline', 'inline-block');
            setClassDisplay('discon', 'none');
            connected = true;

            // read configured delay value when safe
            console.log('about to set timeout');
            setTimeout(async function () {
                console.log('done set timeout');
                await sendCommand('read delay');
                let maxlen = 64;
                console.log('receiving');
                let result = await device.transferIn(3, maxlen);
                console.log('received', typeof(result));
                console.log('received', result);

                var decoder = new TextDecoder("utf-8");
                var s = decoder.decode(result.data);
                delayInput.value = parseInt(s);
                console.log('updated value: ', delayInput.value);
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
	console.log('control transfer...');
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
    console.log('command:', command);

    await sendCommand(command);
	statusNode.innerHTML = "Connected and Updated";
}

updateBtn.addEventListener('click', updateConfiguration);

async function disconnect() {
    let devices = await navigator.usb.getDevices();
    devices.forEach(device => {
            device.close();
    });
    statusNode.innerHTML = 'Disconnected';
    connectBtn.innerHTML = "Connect";
    setClassDisplay('con', 'none');
    console.log("SETTING TO NONE!");
    setClassDisplay('con-inline', 'none');
    setClassDisplay('discon', 'block');
    connected = false;
}

document.getElementById('connect').addEventListener('click', async () => {
    if (connected) {
        console.log('discon...');
        return disconnect();
    }
    try {
	    console.log("requesting device...");

        // filter to show only Intel devices
	    let dev = await navigator.usb.requestDevice({
            filters: [{
                'vendorId': 0x8086
            }]
        });
        await autoConnect(dev);
    }
    catch (e) {
	    console.log('no device selected');
    }
});
