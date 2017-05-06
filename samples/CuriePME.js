// Copyright (c) 2017, Intel Corporation.

// Test code to use the Curie Pattern Matching Engine within the Intel Curie Compute
// on the Arduino 101/tinyTile, which uses the PME API to train and classify data
console.log("Curie Pattern Matching Engine test...");

// import pme module
var pme = require("pme");

var training1 = [ 10, 10, 10, 10 ];
var training2 = [ 20, 20, 20, 20 ];

var classify1 = [ 15, 15, 15, 15 ];
var classify2 = [ 14, 14, 14, 14 ];
var classify3 = [ 16, 16, 16, 16 ];
var classify4 = [ 25, 25, 25, 25 ];
var classify5 = [ 30, 30, 30, 30 ];

function pme_stats() {
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
                    " influence=" + neuron.influence +
                    " minInfluence=" + neuron.minInfluence);
    }
}

pme.begin();
pme.learn(training1, 100);
console.log("Learned [ 10, 10, 10, 10 ] with category 100");
pme.learn(training2, 200);
console.log("Learned [ 20, 20, 20, 20 ] with category 200");
console.log("Global context: " + pme.getGlobalContext());
console.log("Neuron context: " + pme.getNeuronContext());
console.log("Neuron count: " + pme.getCommittedCount());

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

pme_stats();

console.log("Reconfigure with KNN_MODE, LSUP_DISTANCE,");
pme.configure(pme.getGlobalContext(), pme.KNN_MODE, pme.LSUP_DISTANCE, 5, 10);

pme_stats();
