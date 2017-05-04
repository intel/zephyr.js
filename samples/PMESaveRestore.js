// Copyright (c) 2017, Intel Corporation.

// Test code to use the Curie Pattern Matching Engine within the Intel Curie Compute
// on the Arduino 101/tinyTile, which uses the PME API to train and
// then save the data as json to file system and restore it back
console.log("Curie Pattern Matching Engine save and restore test...");

// import pme and fs module
var pme = require("pme");
var fs = require("fs");

var file = "neurons.txt";

var training1 = [ 10, 10, 10, 10 ];
var training2 = [ 20, 20, 20, 20 ];
var training3 = [ 30, 30, 30, 30 ];
var training4 = [ 40, 40, 40, 40 ];
var training5 = [ 50, 50, 50, 50 ];

var classify1 = [ 15, 15, 15, 15 ];
var classify2 = [ 14, 14, 14, 14 ];
var classify3 = [ 16, 16, 16, 16 ];
var classify4 = [ 25, 25, 25, 25 ];
var classify5 = [ 30, 30, 30, 30 ];

function pme_stats() {
    console.log("");
    console.log("PME statistics");
    console.log("==============:");
    var mode = pme.getClassifierMode();
    if (mode === pme.RBF_MODE) {
        console.log("Classification mode: RBF_MODE");
    } else if (mode === pme.RBF_MODE) {
        console.log("Classification mode: KNN_MODE");
    }

    mode = pme.getDistanceMode();
    if (mode === pme.L1_DISTANCE) {
        console.log("Distance mode: L1_DISTANCE");
    } else if (mode === pme.LSUP_DISTANCE) {
        console.log("Distance mode: LSUP_DISTANCE");
    }

    for (var i = 1; i <= pme.getCommittedCount(); i++) {
        var neuron = pme.readNeuron(i);
        console.log("Neuron id=" + i +
                    " category=" + neuron.category +
                    " context=" + neuron.context +
                    " AIF=" + neuron.AIF +
                    " minIF=" + neuron.minIF);
    }
    console.log("");
}

pme.begin();

// NOTE: Currently there's limit on how many neurons we can save/restore
// since there are up to 128 neurons, where each
// neuron will create up to 128 objects to store the vector array,
// switching the array to a Buffer would probably be less memory constrained
// but the Buffer object currently can't be serialized to JSON string when
// JSON.stringify() is called.
// the current 16K memory in JerryScript can't handle it JSON string when
// it grows too big, and we will run out of memory when allocating the string
pme.learn(training1, 100);
pme.learn(training2, 200);
pme.learn(training3, 300);
pme.learn(training4, 400);
pme.learn(training5, 500);
console.log("Finished training");
pme_stats();

var neurons = pme.saveNeurons();
var jsonOut = JSON.stringify(neurons);

fs.writeFileSync(file, jsonOut);
console.log("Saved neurons to file " + file + " with " + jsonOut.length + " bytes...");

pme.forget();
console.log("Cleared engine...");
pme_stats();

var stats = fs.statSync(file);
var buf = fs.readFileSync(file);
neurons = JSON.parse(buf.toString("ascii"));
console.log("Reading from file " + file + " with " + stats.size + " bytes...");
console.log("Restoring " + neurons.length + " neurons...");
pme.restoreNeurons(neurons);
console.log("Restore complete...");
pme_stats();

// test that classification still works after restore
var category = pme.classify(classify1);
if (category === pme.NO_MATCH) {
    category = "no match";
}
console.log("Classified [ 15, 15, 15, 15 ] as category " + category);

category = pme.classify(classify2);
if (category === pme.NO_MATCH) {
    category = "no match";
}
console.log("Classified [ 14, 14, 14, 14 ] as category " + category);

category = pme.classify(classify3);
if (category === pme.NO_MATCH) {
    category = "no match";
}
console.log("Classified [ 16, 16, 16, 16 ] as category " + category);

category = pme.classify(classify4);
if (category === pme.NO_MATCH) {
    category = "no match";
}
console.log("Classified [ 25, 25, 25, 25 ] as category " + category);

category = pme.classify(classify5);
if (category === pme.NO_MATCH) {
    category = "no match";
}
console.log("Classified [ 30, 30, 30, 30 ] as category " + category);
