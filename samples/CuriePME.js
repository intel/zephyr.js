// Copyright (c) 2017, Intel Corporation.

// Test code to use the Curie Pattern Matching Engine within the Intel Curie Compute
// on the Arduino 101/tinyTile, which uses the CuriePME API to train and classify data
console.log("Curie Pattern Matching Engine test...");

// import pme module
var CuriePME = require("pme");

var training1 = [ 10, 10, 10, 10 ];
var training2 = [ 20, 20, 20, 20 ];

var classify1 = [ 15, 15, 15, 15 ];
var classify2 = [ 14, 14, 14, 14 ];
var classify3 = [ 16, 16, 16, 16 ];
var classify4 = [ 25, 25, 25, 25 ];
var classify5 = [ 30, 30, 30, 30 ];

CuriePME.begin();

var mode = CuriePME.getClassifierMode();
if (mode === CuriePME.RBF_MODE) {
    console.log("Classification mode: RBF_MODE");
} else if (mode === CuriePME.RBF_MODE) {
    console.log("Classification mode: KNN_MODE");
}

mode = CuriePME.getDistanceMode();
if (mode === CuriePME.L1_DISTANCE) {
    console.log("Distance mode: L1_DISTANCE");
} else if (mode === CuriePME.LSUP_DISTANCE) {
    console.log("Distance mode: LSUP_DISTANCE");
}

CuriePME.learn(training1, 100);
console.log("Learned [ 10, 10, 10, 10 ] with category 100");
CuriePME.learn(training2, 200);
console.log("Learned [ 20, 20, 20, 20 ] with category 200");
console.log("Global context: " + CuriePME.getGlobalContext());
console.log("Neuron context: " + CuriePME.getNeuronContext());
console.log("Neuron count: " + CuriePME.getCommitedCount());

var category = CuriePME.classify(classify1);
category = 0;
if (category === CuriePME.NO_MATCH) {
    category = "no match";
}
console.log("Classified [ 15, 15, 15, 15 ] as category " + category);

category = CuriePME.classify(classify2);
if (category === CuriePME.NO_MATCH) {
    category = "no match";
}
console.log("Classified [ 14, 14, 14, 14 ] as category " + category);

category = CuriePME.classify(classify3);
if (category === CuriePME.NO_MATCH) {
    category = "no match";
}
console.log("Classified [ 16, 16, 16, 16 ] as category " + category);

category = CuriePME.classify(classify4);
if (category === CuriePME.NO_MATCH) {
    category = "no match";
}
console.log("Classified [ 25, 25, 25, 25 ] as category " + category);

category = CuriePME.classify(classify5);
if (category === CuriePME.NO_MATCH) {
    category = "no match";
}
console.log("Classified [ 30, 30, 30, 30 ] as category " + category);

for (var i = 1; i <= CuriePME.getCommitedCount(); i++) {
    var neuron = CuriePME.readNeuron(i);
    console.log("Neuron id=" + i +
                " category=" + neuron.category +
                " context=" + neuron.context +
                " influence=" + neuron.influence +
                " minInfluence=" + neuron.minInfluence);
}
