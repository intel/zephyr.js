// Copyright (c) 2017, Intel Corporation.

var devicesNode = document.getElementById('devices');
var statusNode = document.getElementById('status');
var historyNode = document.getElementById('history');

device = null;

packetsRead = 0;
packetsWritten = 0;
bytesRead = 0;
bytesWritten = 0;

async function autoConnect(dev) {
    if (dev.manufacturerName == 'Acme' || dev.manufacturerName == 'Intel') {
	    devicesNode.innerHTML = dev.manufacturerName + ' ' +
	        dev.productName;
        try {
	        statusNode.innerHTML = "CONNECTED";
            await dev.open();
            if (!dev.configuration) {
                await dev.selectConfiguration(1);
            }
            await dev.claimInterface(2);
            device = dev;
            await ctrlTransfer();
            setTimeout(sendReceiveLoop, 250);
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

function updateHistory() {
    historyNode.innerHTML =
        packetsWritten + ' packets sent (' + bytesWritten + ' bytes)<br>\n' +
        packetsRead + ' packets received (' + bytesRead + ' bytes)';
}

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function connect(dev) {
    console.log('opening device...');
    try {
	    await device.open();
    }
    catch (e) {
        console.log('open failed');
        return;
    }
	statusNode.innerHTML = "Connected";

    // TODO: could split out
	if (device.configuration == null) {
        console.log('configuring device...');
	    device.selectConfiguration(1);
	}
}

async function claim() {
	console.log('claiming interface...');
    try {
	    await device.claimInterface(2)
    }
    catch (e) {
        console.log('claim failed');
        return;
    }        
}

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

async function sendReceiveLoop() {
    let maxlen = 32;
    let buf = new Uint8Array(maxlen);
    var count = 0;
    while (true) {
        count += 1;
        buf[0] = count % 256;
	    let result = await device.transferOut(2, buf);
        packetsWritten += 1;
        bytesWritten += result.bytesWritten;
        updateHistory();

        // NOTE: if this is not big enough to handle a message, call blocks
        //   forever, which seems like a Chrome bug
	    result = await device.transferIn(3, maxlen);
        packetsRead += 1;
        bytesRead += result.data.byteLength;
        updateHistory();

        await sleep(100)
    }
}

document.getElementById('disconnect').addEventListener('click', async () => {
    let devices = await navigator.usb.getDevices();
    devices.forEach(device => {
	    device.close();
    });
    devicesNode.innerHTML = '';
    statusNode.innerHTML = 'Disconnected';
});

document.getElementById('connect').addEventListener('click', async () => {
    try {
        console.log('requesting device...');
	    device = await navigator.usb.requestDevice({filters: []});
	    devicesNode.innerHTML += 'Added: ' + device.manufacturerName + ' ' +
	        device.productName + '<br>\n';
	    connect(device);
    }
    catch (e) {
	    console.log('no device selected');
    }
});

document.getElementById('claim').addEventListener('click', claim);
document.getElementById('ctrlTransfer').addEventListener('click', ctrlTransfer);
document.getElementById('sendReceive').addEventListener('click',
                                                        sendReceiveLoop);

document.getElementById('auto').addEventListener('click', async () => {
    try {
	    console.log("requesting device...");
	    let dev = await navigator.usb.requestDevice({filters: []});
        await autoConnect(dev);
    }
    catch (e) {
	    console.log('no device selected');
    }

    // seems to start failing around 150
    setTimeout(sendReceiveLoop, 250);
});
